#[allow(non_upper_case_globals)]
#[allow(non_camel_case_types)]
#[allow(non_snake_case)]
#[allow(dead_code)]
mod bindings;

mod gba;
mod message;
mod notification;

pub use gba::Gba;
pub use message::{Message, MessageChannel};
pub use notification::{Notification, NotificationChannel};
