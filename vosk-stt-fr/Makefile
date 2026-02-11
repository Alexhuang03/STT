CC = gcc
CFLAGS = -I./vosk-api/src -Wall
LDFLAGS = -L./vosk-api/src -lvosk -lportaudio -Wl,-rpath=./vosk-api/src

all: stt_app

stt_app: main.o
	$(CC) -o $@ $^ $(LDFLAGS)

main.o: main.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f *.o stt_app

run: stt_app
	./stt_app
