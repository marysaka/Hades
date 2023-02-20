mod libgba_sys;
mod gba;
mod message;
mod notification;
mod config;
mod shared;

pub const GBA_SCREEN_WIDTH: u32 = 240;
pub const GBA_SCREEN_HEIGHT: u32 = 160;

pub use gba::Gba;
pub use message::{Message, MessageChannel};
pub use notification::{Notification, NotificationChannel};
pub use config::{GbaConfig, BackupStorageType};
pub use shared::GbaSharedData;
