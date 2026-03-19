#include <stdlib.h>
#include <switch.h>
#include <math.h>
#include "audio.h"

// from https://github.com/switchbrew/switch-examples/blob/master/audio/playtone/source/main.c
void fill_audio_buffer(void* audio_buffer, size_t offset, size_t size, int frequency) {
    if (audio_buffer == NULL)
        return;
    
    u32* dest = (u32*) audio_buffer;
    for (int i = 0; i < size; i++) {
        // This is a simple sine wave, with a frequency of `frequency` Hz, and an amplitude 30% of maximum.
        s16 sample = 0.3 * 0x7FFF * sin(frequency * (2 * M_PI) * (offset + i) / SAMPLERATE);

        // Stereo samples are interleaved: left and right channels.
        dest[i] = (sample << 16) | (sample & 0xffff);
    }
}