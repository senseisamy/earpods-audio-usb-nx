#ifndef USB_H
# define USB_H
# include <switch.h>

typedef struct UsbAudioIf_s {
    UsbHsClientIfSession if_session;
    UsbHsClientEpSession ep_session;
    UsbHsInterface interface;
    UsbHsInterfaceFilter filter;
    Event event;
} UsbAudioIf_t;

Result earpods_init(UsbAudioIf_t* device);
bool device_found(UsbAudioIf_t* device);
Result query_and_acquire_interface(UsbAudioIf_t* device);
Result open_endpoint(UsbAudioIf_t* device);

#endif