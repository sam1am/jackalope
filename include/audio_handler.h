#ifndef AUDIO_HANDLER_H
#define AUDIO_HANDLER_H

#include <Arduino.h>

void init_audio();
void create_wav_header(byte *header, int data_size, int bitsPerSample);

#endif // AUDIO_HANDLER_H

