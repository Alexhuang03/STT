#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <portaudio.h>
#include "vosk-api/src/vosk_api.h"

#define SAMPLE_RATE 16000
#define FRAMES_PER_BUFFER 3200 // 0.2 seconds of audio

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
    if (recognizer) vosk_recognizer_free(recognizer);
    if (model) vosk_model_free(model);
    printf("\nCleaned up resources.\n");
}

void sigint_handler(int sig) {
    stop_requested = 1;
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
    inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(&stream, &inputParameters, NULL, SAMPLE_RATE, FRAMES_PER_BUFFER, paClipOff, NULL, NULL);
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

    printf("Listening... Press Ctrl+C to stop.\n");

    int16_t buffer[FRAMES_PER_BUFFER];
    
    while (!stop_requested) {
        err = Pa_ReadStream(stream, buffer, FRAMES_PER_BUFFER);
        if (err != paNoError && err != paInputOverflowed) {
             fprintf(stderr, "PortAudio ReadStream error: %s\n", Pa_GetErrorText(err));
             break;
        }

        if (vosk_recognizer_accept_waveform_s(recognizer, buffer, FRAMES_PER_BUFFER)) {
            const char *result = vosk_recognizer_result(recognizer);
            printf("%s\n", result);
        } else {
            const char *partial = vosk_recognizer_partial_result(recognizer);
            printf("%s\r", partial); 
            fflush(stdout);
        }
    }

    // Print final result
    const char *final_result = vosk_recognizer_final_result(recognizer);
    printf("%s\n", final_result);

    cleanup();
    return 0;
}
