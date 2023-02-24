use std::sync::mpsc::Sender;
use std::sync::{Arc, Mutex};

use crate::config::GbaConfig;
use crate::libgba_sys;

// TODO FIXME
// There's an unsoundness here with GbaConfig that can be dropped before the
// message is processed by the Gba thread.

#[derive(Debug, Copy, Clone, Eq, PartialEq, Ord, PartialOrd, Hash)]
#[repr(u8)]
pub enum Key {
    A,
    B,
    L,
    R,
    Up,
    Down,
    Left,
    Right,
    Start,
    Select,
}

impl Key {
    pub(crate) fn raw(&self) -> libgba_sys::keys {
        match self {
            Key::A => libgba_sys::keys::KEY_A,
            Key::B => libgba_sys::keys::KEY_B,
            Key::L => libgba_sys::keys::KEY_L,
            Key::R => libgba_sys::keys::KEY_R,
            Key::Up => libgba_sys::keys::KEY_UP,
            Key::Down => libgba_sys::keys::KEY_DOWN,
            Key::Left => libgba_sys::keys::KEY_LEFT,
            Key::Right => libgba_sys::keys::KEY_RIGHT,
            Key::Start => libgba_sys::keys::KEY_START,
            Key::Select => libgba_sys::keys::KEY_SELECT,
        }
    }
}

#[derive(Debug)]
pub enum Message {
    Exit,
    Reset(Arc<GbaConfig>),
    Run,
    Pause,
    Key { key: Key, pressed: bool },
}

#[derive(Debug)]
pub struct MessageChannel {
    sender: Arc<Mutex<Sender<Message>>>,
}

impl MessageChannel {
    pub(crate) fn from(sender: Arc<Mutex<Sender<Message>>>) -> Self {
        Self { sender: sender }
    }

    pub fn send(&self, message: Message) {
        let sender = self.sender.lock().unwrap();

        sender.send(message).unwrap();
    }
}
