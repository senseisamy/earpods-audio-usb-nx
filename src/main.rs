#![no_std]
#![no_main]

#[macro_use]
extern crate nx;

extern crate alloc;

use nx::diag::abort;
use nx::diag::log;
use nx::ipc::client::IClientObject;
use nx::ipc::sf;
use nx::ipc::sf::sm::IUserInterfaceClient;
use nx::ipc::sf::Session;
use nx::result::*;
use nx::service::sm::ServiceName;
use nx::service::sm::UserInterface;
use nx::svc;
use nx::util;

use core::panic;

// Using 128KB custom heap
const CUSTOM_HEAP_LEN: usize = 0x20000;
static mut CUSTOM_HEAP: [u8; CUSTOM_HEAP_LEN] = [0; CUSTOM_HEAP_LEN];

#[no_mangle]
pub fn initialize_heap(_hbl_heap: util::PointerAndSize) -> util::PointerAndSize {
    unsafe { util::PointerAndSize::new(CUSTOM_HEAP.as_mut_ptr(), CUSTOM_HEAP.len()) }
}

#[no_mangle]
pub fn main() -> Result<()> {
    diag_log!(log::lm::LmLogger { log::LogSeverity::Trace, false } => "Earpods driver starting...");

    let sm_handle = unsafe { svc::connect_to_named_port(c"sm:") }?;
    let session = Session::from_handle(sm_handle);
    let user_interface = UserInterface::new(session);
    user_interface.register_client(sf::ProcessId::new())?;
    let usb_hs_handle = user_interface.get_service_handle(ServiceName::new("usb:hs"));

    diag_log!(log::lm::LmLogger { log::LogSeverity::Trace, false } => "Successfully acquired the raw usb:hs session!");

    loop {
        // Sleep 10ms (aka 10'000'000 ns)
        svc::sleep_thread(10_000_000)?;
    }
}

#[panic_handler]
fn panic_handler(info: &panic::PanicInfo) -> ! {
    util::simple_panic_handler::<log::lm::LmLogger>(info, abort::AbortLevel::SvcBreak())
}
