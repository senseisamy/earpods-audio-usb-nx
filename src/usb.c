#include "usb.h"
#include "utils.h"
#include <strings.h>

Result earpods_init(UsbAudioIf_t* device) {
    Result rc;

    bzero(device, sizeof(*device));

    // Create a device filter which only filters in the earpods;
    device->filter.Flags = UsbHsInterfaceFilterFlags_idVendor | UsbHsInterfaceFilterFlags_idProduct;
    device->filter.idVendor = 0x05ac; // apple
    device->filter.idProduct = 0x110b; // usb c earpods

    // Create an event which is signaled when the earpods are connected
    rc = usbHsCreateInterfaceAvailableEvent(&device->event, true, 0, &device->filter);
    if (R_FAILED(rc))
        R_LOG("Failed to create interface available event", rc);
    return rc;
}

bool device_found(UsbAudioIf_t* device) {
    return R_SUCCEEDED(eventWait(&device->event, 0));
}

Result query_and_acquire_interface(UsbAudioIf_t* device) {
    UsbHsInterface interfaces[8];
    s32 total_entries;
    Result rc;

    bzero(interfaces, sizeof(interfaces));
    rc = usbHsQueryAvailableInterfaces(&device->filter, interfaces, sizeof(interfaces), &total_entries);
    if (R_FAILED(rc)) {
        R_LOG("Failed to query available interfaces", rc);
        return rc;
    }

    // iterate over every interface and try to find interface 1
    for(int i = 0; i < total_entries; i++) {
        if (interfaces[i].inf.interface_desc.bInterfaceNumber == 1) {
            device->interface = interfaces[i];
            break;
        }
    }
    if (device->interface.inf.interface_desc.bInterfaceNumber != 1) {
        LOG("Failed to find iterface 1");
        rc = 1;
        return rc;
    }

    LOG("Found Interface 1 ! Acquiring..");

    // Acquire the usb interface
    rc = usbHsAcquireUsbIf(&device->if_session, &device->interface);
    if (R_FAILED(rc)) {
        R_LOG("Failed to acquire the audio streaming interface", rc);
        return rc;
    }
    LOG("Successfully acquired the audio streaming interface.");

    // Switch to alternate setting 2 (16-bit audio profile with 192-byte max packet size)
    rc = usbHsIfSetInterface(&device->if_session, &device->interface.inf, 2);
    if (R_FAILED(rc)) {
        R_LOG("Failed to set Alternate Setting 2", rc);
        usbHsIfClose(&device->if_session);
        return rc;
    }
    LOG("Successfully switched to Alternate Setting 2.");

    return rc;
}

Result open_endpoint(UsbAudioIf_t* device) {
    Result rc;
    // struct usb_endpoint_descriptor ep_desc = {
    //     .bLength = sizeof(ep_desc),
    //     .bDescriptorType = USB_DT_ENDPOINT,
    //     .bEndpointAddress = 2, // EP2 : OUT
    //     .bmAttributes = 0x0D, // 13 in decimal (Isochronous | Synchronous)  
    //     .wMaxPacketSize = 192, // 192 bytes (16-bit audio profile)
    //     .bInterval = 1, // Poll every 1ms
    // };
    // we can actually just copy the endpoint descriptor sent by the earpods
    struct usb_endpoint_descriptor ep_desc = device->interface.inf.output_endpoint_descs[0];
    printf("endpoint descritor:\n");
    printf(" bLength = %d\n", ep_desc.bLength);
    printf(" bDescriptorType = %d\n", ep_desc.bDescriptorType);
    printf(" bEndpointAddress = %d\n", ep_desc.bEndpointAddress);
    printf(" bmAttributes = %d\n", ep_desc.bmAttributes);
    printf(" wMaxPacketSize = %d\n", ep_desc.wMaxPacketSize);
    printf(" bInterval = %d\n", ep_desc.bInterval);

    // Open the usb endpoint
    rc = usbHsIfOpenUsbEp(&device->if_session, &device->ep_session, 10, 1920, &ep_desc);
    if (R_FAILED(rc)) {
        R_LOG("Failed to open the usb endpoint", rc);
        usbHsIfClose(&device->if_session);
        return rc;
    }
    LOG("Usb endpoint opened!");
    return rc;
}