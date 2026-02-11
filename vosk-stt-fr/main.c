#include "vosk-api/src/vosk_api.h"
#include <portaudio.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAMPLE_RATE 16000
#define FRAMES_PER_BUFFER 3200 // 0.2 seconds of audio

// Include new headers
#include "command_parser.h"
#include "llm_client.h"

// ... (keep includes and defines) ...

// Global variables for cleanup
VoskModel *model = NULL;
VoskRecognizer *recognizer = NULL;
PaStream *stream = NULL;
int stop_requested = 0;

void cleanup() {
  if (stream) {
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
  }
  Pa_Terminate();
  if (recognizer)
    vosk_recognizer_free(recognizer);
  if (model)
    vosk_model_free(model);
  printf("\nCleaned up resources.\n");
}

void sigint_handler(int sig) { stop_requested = 1; }

// Simple JSON parser for "text" field, to avoid full json-c dependency here if
// possible, OR just use json-c since we link it anyway. Let's use json-c to be
// safe and consistent. We need to include json.h in main.c too if we use it.
// Actually, let's just do a quick string search for "text" : "..." to avoid
// complexity if simple. But since we use json-c in llm_client, let's not be
// afraid of it. However, adding #include <json-c/json.h> here might require
// updating Makefile CFLAGS more if it's in a weird place. Let's stick to the
// previous simple logic or a manual parse for the main loop to keep main.c
// changes minimal relying on the previous implementation's output format if
// possible, but VOSK returns a JSON string. VOSK returns: { "text" : "hello
// world" } We can use a helper to extract the text.

// ANSI Color Codes
#define COLOR_RESET "\033[0m"
#define COLOR_GREEN "\033[1;32m" // Bold Green
#define COLOR_CYAN "\033[0;36m"
#define COLOR_YELLOW "\033[1;33m"

void process_result(const char *json_result) {
  // Basic manual JSON parsing to extract "text" value
  // expected format: ... "text" : "actual text" ...
  const char *key = "\"text\"";
  const char *text_start = strstr(json_result, key);
  if (!text_start)
    return;

  // Move to colon
  text_start = strchr(text_start, ':');
  if (!text_start)
    return;

  // Move to first quote
  text_start = strchr(text_start, '\"');
  if (!text_start)
    return;
  text_start++; // Skip quote

  const char *text_end = strchr(text_start, '\"');
  if (!text_end)
    return;

  int len = text_end - text_start;
  if (len <= 0)
    return; // Empty text

  char *spoken_text = malloc(len + 1);
  strncpy(spoken_text, text_start, len);
  spoken_text[len] = '\0';

  if (strlen(spoken_text) == 0) {
    free(spoken_text);
    return;
  }

  // Print original transcription (neutral)
  printf("Transcription : %s\n", spoken_text);

  // 1. Parse Command
  CommandType cmd = parse_command(spoken_text);
  if (cmd != CMD_UNKNOWN) {
    const char *action = get_command_action(cmd);
    printf("%s>>> COMMANDE DÉTECTÉE : [%s]%s\n", COLOR_GREEN, action,
           COLOR_RESET);
  } else {
    // 2. LLM Fallback
    printf("%s>>> [RELAY LLM] Envoi à l'IA...%s\n", COLOR_YELLOW, COLOR_RESET);
    send_to_llm(spoken_text);
  }

  free(spoken_text);
}

int main() {
  // Setup signal handler
  signal(SIGINT, sigint_handler);

  // Initialize VOSK
  printf("Loading model...\n");
  model = vosk_model_new("vosk-model-small-fr-0.22");
  if (model == NULL) {
    fprintf(stderr, "Error: Could not load model\n");
    return -1;
  }

  recognizer = vosk_recognizer_new(model, SAMPLE_RATE);

  // Initialize PortAudio
  printf("Initializing PortAudio...\n");
  PaError err = Pa_Initialize();
  if (err != paNoError) {
    fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
    cleanup();
    return -1;
  }

  // Open Stream
  PaStreamParameters inputParameters;
  inputParameters.device = Pa_GetDefaultInputDevice();
  if (inputParameters.device == paNoDevice) {
    fprintf(stderr, "Error: No default input device.\n");
    cleanup();
    return -1;
  }
  inputParameters.channelCount = 1;
  inputParameters.sampleFormat = paInt16;
  inputParameters.suggestedLatency =
      Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
  inputParameters.hostApiSpecificStreamInfo = NULL;

  err = Pa_OpenStream(&stream, &inputParameters, NULL, SAMPLE_RATE,
                      FRAMES_PER_BUFFER, paClipOff, NULL, NULL);
  if (err != paNoError) {
    fprintf(stderr, "PortAudio OpenStream error: %s\n", Pa_GetErrorText(err));
    cleanup();
    return -1;
  }

  // Start Stream
  err = Pa_StartStream(stream);
  if (err != paNoError) {
    fprintf(stderr, "PortAudio StartStream error: %s\n", Pa_GetErrorText(err));
    cleanup();
    return -1;
  }

  printf("Listening... Speak a command or ask a question.\n");
  printf("Commands: droite 45, gauche 45, stop, position, scanne, autopilot\n");

  int16_t buffer[FRAMES_PER_BUFFER];

  while (!stop_requested) {
    err = Pa_ReadStream(stream, buffer, FRAMES_PER_BUFFER);
    if (err != paNoError && err != paInputOverflowed) {
      fprintf(stderr, "PortAudio ReadStream error: %s\n", Pa_GetErrorText(err));
      break;
    }

    if (vosk_recognizer_accept_waveform_s(recognizer, buffer,
                                          FRAMES_PER_BUFFER)) {
      const char *result = vosk_recognizer_result(recognizer);
      process_result(result);
    } else {
      const char *partial = vosk_recognizer_partial_result(recognizer);
      printf("%s\r", partial);
      fflush(stdout);
    }
  }

  // Print final result
  const char *final_result = vosk_recognizer_final_result(recognizer);
  process_result(final_result);

  cleanup();
  return 0;
}
