#ifndef AUDIO_H
# define AUDIO_H

#include <stddef.h>

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

#define SAMPLERATE 48000
#define CHANNELCOUNT 2
#define BYTESPERSAMPLE 2

void fill_audio_buffer(void* audio_buffer, size_t offset, size_t size, int frequency);

#endif