mod config;
mod gba;
mod libgba_sys;
mod message;
mod notification;
mod shared;

pub const GBA_SCREEN_WIDTH: u32 = 240;
pub const GBA_SCREEN_HEIGHT: u32 = 160;

pub const GBA_HEADER_LEN: usize = 0xC0;
pub const GBA_HEADER_GAME_CODE_OFFSET: usize = 0xAC;
pub const GBA_HEADER_GAME_CODE_LEN: usize = 3;

pub use config::{BackupStorageType, GbaConfig};
pub use gba::Gba;
pub use message::{Key, Message, MessageChannel};
pub use notification::{Notification, NotificationChannel};
pub use shared::GbaSharedData;
