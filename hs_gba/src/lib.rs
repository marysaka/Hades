mod config;
mod gba;
mod libgba_sys;
mod message;
mod notification;
mod shared;

pub const GBA_SCREEN_WIDTH: u32 = 240;
pub const GBA_SCREEN_HEIGHT: u32 = 160;

pub use config::{BackupStorageType, GbaConfig};
pub use gba::Gba;
pub use message::{Key, Message, MessageChannel};
pub use notification::{Notification, NotificationChannel};
pub use shared::GbaSharedData;
