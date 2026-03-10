#![no_std]
#![no_main]

extern crate nx;
extern crate alloc;

use core::num::NonZeroU16;
use core::panic;
use alloc::sync::Arc;
use nx::diag::abort;
use nx::diag::log;
use nx::gpu;
use nx::ipc::client::IClientObject;
use nx::ipc::sf;
use nx::ipc::sf::sm::IUserInterfaceClient;
use nx::ipc::sf::CopyHandle;
use nx::ipc::sf::OutMapAliasBuffer;
use nx::ipc::sf::Session;
use nx::result::*;
use nx::service::sm::ServiceName;
use nx::service::sm::UserInterface;
use nx::service::usb::hs::ClientRootSessionService;
use nx::service::usb::hs::IClientRootSessionClient;
use nx::service::usb::hs::InterfaceQueryOutput;
use nx::svc;
use nx::sync::RwLock;
use nx::util;
use alloc::format;

nx::rrt0_define_module_name!("earpods-audio-usb-nx");

#[no_mangle]
pub fn initialize_heap(hbl_heap: util::PointerAndSize) -> util::PointerAndSize {
    if hbl_heap.is_valid() {
        hbl_heap
    } else {
        let heap_size: usize = 0x10000000;
        let heap_address = svc::set_heap_size(heap_size).unwrap();
        util::PointerAndSize::new(heap_address, heap_size)
    }
}

#[no_mangle]
pub fn main() -> Result<()> {
    let mut console = {
        let gpu_ctx = match gpu::Context::new(
            gpu::NvDrvServiceKind::Applet,
            gpu::ViServiceKind::System,
            0x40000,
        ) {
            Ok(ctx) => ctx,
            Err(e) => panic!("{}", e)
        };
        match nx::console::scrollback::ScrollbackConsole::new(
            Arc::new(RwLock::new(gpu_ctx)),
            300,
            NonZeroU16::new(90).unwrap(),
            true,
            None,
            2
        ) {
            Ok(console) => console,
            Err(e) => panic!("{}", e)
        }
    };

    console.write("Earpods driver starting...\n");

    /*
    // gets the service manager
    let sm_handle = unsafe { svc::connect_to_named_port(c"sm:") }?;
    let sm_session = Session::from_handle(sm_handle);

    // register our sysmodule to the service manager
    let user_interface = UserInterface::new(sm_session);
    user_interface.register_client(sf::ProcessId::new())?;

    // gets the usb:hs service
    let usb_hs_handle = user_interface.get_service_handle(ServiceName::new("usb:hs"))?;
    let usb_hs_session = Session::from_handle(usb_hs_handle.handle);
    let usb_interface = ClientRootSessionService::new(usb_hs_session);
    let current_process_handle = CopyHandle::from(svc::CURRENT_PROCESS_PSEUDO_HANDLE);
    usb_interface.bind_client_process(current_process_handle)?;

    //diag_log!(log::lm::LmLogger { log::LogSeverity::Trace, false } => "Successfully acquired the raw usb:hs session!");
    console.write("Successfully acquired the raw usb:hs session!");

    let query_usb_device_filter = sf::usb::hs::DeviceFilter {
        flags: sf::usb::hs::DeviceFilterFlags::from(0),
        ..Default::default()
    };
    let mut interfaces: [InterfaceQueryOutput; 8] = unsafe { core::mem::zeroed() };

    loop {
        let total_found = usb_interface.query_available_interfaces(
            query_usb_device_filter,
            OutMapAliasBuffer::from_mut_array(&mut interfaces),
        )?;

        //diag_log!(log::lm::LmLogger { log::LogSeverity::Trace, false } => "{} device(s) found!", total_found);
        console.write(format!("{} device(s) found!", total_found));

        // sleep 1s
        svc::sleep_thread(1_000_000_000)?;
    }*/

    loop {
        console.write("a");
        svc::sleep_thread(1_000_000_000)?;
    }
}

#[panic_handler]
fn panic_handler(info: &panic::PanicInfo) -> ! {
    util::simple_panic_handler::<log::lm::LmLogger>(info, abort::AbortLevel::SvcBreak())
}
