#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <math.h>
#include <switch.h>

#define VENDOR_ID 0x05ac // apple
#define PRODUCT_ID 0x110b // usb c earpods

int main(int argc, char** argv) {
    Result rc = 0;
    s32 i;
    s32 total_entries = 0;
    Event inf_event;
    UsbHsClientIfSession inf_session;
    UsbHsInterfaceFilter filter;
    UsbHsInterface interfaces[8];

    consoleInit(NULL);

    // Configure our supported input layout: a single player with standard controller styles
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    // Initialize the default gamepad (which reads handheld mode inputs as well as the first connected controller)
    PadState pad;
    padInitializeDefault(&pad);

    printf("YAYY app is starting!\n");

    // 0 initialize the structs
    bzero(&inf_event, sizeof(inf_event));
    bzero(&inf_session, sizeof(inf_session));
    bzero(&filter, sizeof(filter));

    // Create a device filter which only filters in the earpods
    filter.Flags = UsbHsInterfaceFilterFlags_idVendor | UsbHsInterfaceFilterFlags_idProduct;
    filter.idVendor = VENDOR_ID;
    filter.idProduct = PRODUCT_ID;

    // Initialize the switch usb:hs service
    rc = usbHsInitialize();
    if (R_FAILED(rc)) {
        printf("usbHsInitialize() failed: 0x%x\n", rc);
        goto exit;
    }

    // Create an event which is signaled when the earpods are connected
    rc = usbHsCreateInterfaceAvailableEvent(&inf_event, true, 0, &filter);
    if (R_FAILED(rc)) {
        printf("usbHsCreateInterfaceAvailableEvent() failed: 0x%x\n", rc);
        goto exit;
    }

    printf("Ready!! entering main loop :p\n");
    
    // Main loop
    while (appletMainLoop()) {
        consoleUpdate(NULL);

        // Scan the gamepad. This should be done once for each frame
        padUpdate(&pad);

        u64 kDown = padGetButtonsDown(&pad);
        if (kDown & HidNpadButton_Plus) // break if (+) is pressed
            break;

        // This returns an error unless our event is signaled
        rc = eventWait(&inf_event, 0);
        if (R_FAILED(rc)) {
            continue;
        }

        bzero(interfaces, sizeof(interfaces));
        rc = usbHsQueryAvailableInterfaces(&filter, interfaces, sizeof(interfaces), &total_entries);
        if (R_FAILED(rc)) {
            printf("usbHsQueryAvailableInterfaces() failed: 0x%x\n", rc);
            continue;
        }

        consoleClear();

        UsbHsInterface* interface = NULL;
        // iterate over every interface and try to find interface 1
        for(i = 0; i < total_entries; i++) {
            if (interfaces[i].inf.interface_desc.bInterfaceNumber == 1) {
                interface = &interfaces[i];
                break;
            }
        }
        if (interface == NULL) {
            printf("Interface 1 not found\n");
            continue;
        }
            
        printf("Found Interface 1 !! Acquiring..\n");
        UsbHsClientIfSession audio_session;
        bzero(&audio_session, sizeof(audio_session));

        // Acquire the usb interface
        rc = usbHsAcquireUsbIf(&audio_session, &interfaces[i]);
        if (R_FAILED(rc)) {
            printf("Failed to acquire the audio streamin interface (0x%x\n)\n", rc);
            continue;
        }
        printf("Successfully acquired interface.\n");

        // Switch to alternate setting 2 (16-bit audio profile with 192-byte max packet size)
        rc = usbHsIfSetInterface(&audio_session, &interfaces[i].inf, 2);
        if (R_FAILED(rc)) {
            printf("Failed to set Alternate Setting 2 (0x%x)\n", rc);
            usbHsIfClose(&audio_session);
            continue;
        }
        printf("Successfully switched to Alternate Setting 2.\n");

        struct usb_endpoint_descriptor ep_desc;
        bzero(&ep_desc, sizeof(ep_desc));
        ep_desc.bLength = sizeof(ep_desc);
        ep_desc.bDescriptorType = USB_DT_ENDPOINT;
        ep_desc.bEndpointAddress = 2; // OUT
        ep_desc.bmAttributes = 0x0D; // Isochronous | Synchronous      
        ep_desc.wMaxPacketSize = 192;
        ep_desc.bInterval = 1; // Poll every 1ms 

        UsbHsClientEpSession ep_session;
        bzero(&ep_session, sizeof(ep_session));

        // Open the usb endpoint
        rc = usbHsIfOpenUsbEp(&audio_session, &ep_session, 10, 1920, &ep_desc);
        if (R_FAILED(rc)) {
            printf("usbHsIfOpenUsbEp() failed, 0x%x\n", rc);
            usbHsIfClose(&audio_session);
            continue;
        }
        printf("Usb endpoint opened!\n");
        
        // 1. Allocate page-aligned memory for 1 second of audio
        size_t buffer_size = 0x2F000; // 192,512 bytes
        void* audio_buffer = aligned_alloc(0x1000, buffer_size); // The switch wants 0x1000 aligned memory
        if (!audio_buffer) {
            printf("Memory allocation failed\n");
            usbHsEpClose(&ep_session);
            usbHsIfClose(&audio_session);
            continue;
        }

        // Fill it with absolute silence (zeros) for now
        bzero(audio_buffer, buffer_size);

        rc = usbHsEpCreateSmmuSpace(&ep_session, audio_buffer, buffer_size);
        if (R_FAILED(rc)) {
            printf("Failed to create Smmu Space (0x%x)\n", rc);
            free(audio_buffer);
            usbHsEpClose(&ep_session);
            usbHsIfClose(&audio_session);
            continue;
        }
        printf("SMMU memory mapped successfully!\n");

        //Generate a 440Hz Sine Wave (1 second long)
        s16* pcm_data = (s16*)audio_buffer;
        for (int s = 0; s < 48000; ++s) {
            // Calculate continuous time across the whole second
            double time = (double)s / 48000.0;
            
            // Generate the wave amplitude (10000 out of max 32767 volume)
            s16 sample = (s16)(10000.0 * sin(2.0 * M_PI * 440.0 * time));
            
            pcm_data[s * 2]     = sample; // Left Ear
            pcm_data[s * 2 + 1] = sample; // Right Ear
        }

        printf("Playing beep... :p\n");
        for (int ms = 0; ms < 1000; ++ms) {
            u32 transferred = 0;
            // Get the memory address for this specific millisecond
            void* packet_ptr = (void*)((uintptr_t)audio_buffer + (ms * 192));

            // Push 192 bytes (1ms of audio). 
            // This function will block until the hardware consumes the packet!
            rc = usbHsEpPostBuffer(&ep_session, packet_ptr, 192, &transferred);
            if (R_FAILED(rc)) {
                // Only print the first 5 errors to avoid flooding the console
                if (ms < 5) {
                    printf("Packet %d rejected! Error: 0x%x\n", ms, rc);
                }
            } else if (ms == 0) {
                // If the first one works, let us know!
                printf("Packet 0 accepted by hardware!\n");
            }
        }
        printf("Beep finished!\n");

        free(audio_buffer);
        usbHsEpClose(&ep_session);
        usbHsIfClose(&audio_session);
    }

    exit:
        printf("exiting program!");
        consoleUpdate(NULL);
        svcSleepThread(5000000000ull); // 5s
        usbHsDestroyInterfaceAvailableEvent(&inf_event, 0);
        usbHsExit();    
        consoleExit(NULL);
        return 0;
}
