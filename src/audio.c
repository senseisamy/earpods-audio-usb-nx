#include <stdlib.h>
#include <switch.h>
#include <math.h>
#include <string.h>
#include "audio.h"
#include "utils.h"

#define PACKET_SIZE 192

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

Result audio_loop(UsbAudioIf_t* earpods, void* audio_buffer) {
    Result rc = 0;
    void* tx_buffer;

    tx_buffer = aligned_alloc(0x1000, ALIGN_UP(PACKET_SIZE, 0x1000));
    if (!tx_buffer) {
        rc = MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
        R_LOG("Memory allocation failed", rc);
        return rc;
    }
    memset(tx_buffer, 0, ALIGN_UP(PACKET_SIZE, 0x1000));

    // pre fill the queue
    for (int ms = 0; ms < 10; ++ms) {
        u32 xferId = 0;
        void* packet_ptr = (void*)((uintptr_t)audio_buffer + (ms * PACKET_SIZE));
        memcpy(tx_buffer, packet_ptr, PACKET_SIZE);
        rc = usbHsEpPostBufferAsync(&earpods->ep_session, tx_buffer, PACKET_SIZE, (u64)ms, &xferId);
        if (R_FAILED(rc)) {
            free(tx_buffer);
            printf("Kernel rejected Async Post at ms=%d! Error: 0x%x\n", ms, rc);
            return rc;
        }
    }

    // waits for a packet to finish and queue a new one
    for (int ms = 10; ms < 1000; ++ms) {
        Event* event = usbHsEpGetXferEvent(&earpods->ep_session);
        eventWait(event, UINT64_MAX);
        eventClear(event);

        UsbHsXferReport report;
        u32 report_count = 0;
        usbHsEpGetXferReport(&earpods->ep_session, &report, 1, &report_count);

        if (R_FAILED(report.res)) {
            free(tx_buffer);
            printf("Hardware rejected packet at ms=%ld! Error: 0x%x\n", report.id, report.res);
            return report.res;
        }

        u32 xferId = 0;
        void* packet_ptr = (void*)((uintptr_t)audio_buffer + (ms * PACKET_SIZE));
        memcpy(tx_buffer, packet_ptr, PACKET_SIZE);
        rc = usbHsEpPostBufferAsync(&earpods->ep_session, tx_buffer, PACKET_SIZE, (u64)ms, &xferId);
        
        if (R_FAILED(rc)) {
            free(tx_buffer);
            printf("Kernel rejected Async Post at ms=%d! Error: 0x%x\n", ms, rc);
            return rc;
        }
    }
    
    return rc;
}