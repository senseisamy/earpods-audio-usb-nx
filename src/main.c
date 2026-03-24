#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <switch.h>
#include "usb.h"
#include "audio.h"
#include "utils.h"

int main(int argc, char** argv) {
    Result rc = 0;
    UsbAudioIf_t earpods;
    void* audio_buffer = NULL;

    consoleInit(NULL);

    // Configure our supported input layout: a single player with standard controller styles
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    LOG("YAYY app is starting!");

    // Initialize the switch usb:hs service
    rc = usbHsInitialize();
    if (R_FAILED(rc)) {
        R_LOG("Failed to initialize the usb:hs service", rc);
        goto exit;
    }

    // Initialize the earpods and reate an event which is signaled when they are connected
    rc = earpods_init(&earpods);
    if (R_FAILED(rc))
        goto exit;

    LOG("Ready!! searching for earpods...");
    
    // Main loop
    while (appletMainLoop()) {
        consoleUpdate(NULL);

        // Scan the gamepad. This should be done once for each frame
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);
        if (kDown & HidNpadButton_Plus) // break if (+) is pressed
            break;

        if (!device_found(&earpods))
            continue;

        // earpods found, we can now query the available interfaces and acquire the one we want
        rc = query_and_acquire_interface(&earpods);
        if (R_FAILED(rc))
            goto exit;

        // open endpoint
        rc = open_endpoint(&earpods);
        if (R_FAILED(rc))
            goto exit;
        
        // audio test
        {
            // Allocate page-aligned memory for 1 second of audio
            u32 data_size = (SAMPLERATE * CHANNELCOUNT * BYTESPERSAMPLE);
            u32 buffer_size = ALIGN_UP(data_size, 0x1000); // align size to 0x1000
            audio_buffer = aligned_alloc(0x1000, buffer_size);
            if (!audio_buffer) {
                rc = MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
                R_LOG("Memory allocation failed", rc);
                goto abort_audio;
            }

            bzero(audio_buffer, buffer_size);

            rc = usbHsEpCreateSmmuSpace(&earpods.ep_session, audio_buffer, buffer_size);
            if (R_FAILED(rc)) {
                R_LOG("Failed to create Smmu Space", rc);
                goto abort_audio;
            }
            printf("SMMU memory mapped successfully!\n");

            // fill audio buffer with 440hz sine wave
            fill_audio_buffer(audio_buffer, 0, data_size, 440);

            printf("Playing beep... :p\n");

            rc = audio_loop(&earpods, audio_buffer);
            if (R_FAILED(rc))
                goto abort_audio;
    
            printf("Beep finished!\n");
        }


        abort_audio:
            free(audio_buffer);
            usbHsEpClose(&earpods.ep_session);
            usbHsIfClose(&earpods.if_session);
    }

    exit:
        printf("exiting program!");
        consoleUpdate(NULL);
        svcSleepThread(5000000000ull); // 5s
        usbHsDestroyInterfaceAvailableEvent(&earpods.event, 0);
        usbHsExit();    
        consoleExit(NULL);
        return 0;
}
