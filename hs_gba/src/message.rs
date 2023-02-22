use std::marker::PhantomData;

use crate::config::GbaConfig;
use crate::gba::Gba;
use crate::libgba_sys;

// TODO FIXME
// There's an unsoundness here with GbaConfig that can be dropped before the
// message is processed by the Gba thread.

#[derive(Debug, Copy, Clone, Eq, PartialEq, Ord, PartialOrd, Hash)]
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

pub enum Message<'a> {
    Exit,
    Reset(&'a GbaConfig),
    Run,
    Pause,
    Key { key: Key, pressed: bool },
}

impl<'a> Message<'a> {
    fn raw_no_extension(&self, kind: i32) -> libgba_sys::message {
        libgba_sys::message {
            header: libgba_sys::event_header {
                kind,
                size: std::mem::size_of::<libgba_sys::message>(),
            },
        }
    }

    unsafe fn send_over(self, channel: &MessageChannelLocked) {
        let raw_channel = &mut (*channel.inner).channels.messages;

        match self {
            Message::Exit => {
                libgba_sys::channel_push(
                    raw_channel,
                    &self
                        .raw_no_extension(libgba_sys::message_kind::MESSAGE_EXIT as i32)
                        .header,
                );
            }
            Message::Run => {
                libgba_sys::channel_push(
                    raw_channel,
                    &self
                        .raw_no_extension(libgba_sys::message_kind::MESSAGE_RUN as i32)
                        .header,
                );
            }
            Message::Pause => {
                libgba_sys::channel_push(
                    raw_channel,
                    &self
                        .raw_no_extension(libgba_sys::message_kind::MESSAGE_PAUSE as i32)
                        .header,
                );
            }
            Message::Reset(config) => {
                let msg = libgba_sys::message_reset {
                    header: libgba_sys::event_header {
                        kind: libgba_sys::message_kind::MESSAGE_RESET as i32,
                        size: std::mem::size_of::<libgba_sys::message_reset>(),
                    },
                    config: config.raw(),
                };
                libgba_sys::channel_push(raw_channel, &msg.header);
            }
            Message::Key { key, pressed } => {
                let msg = libgba_sys::message_key {
                    header: libgba_sys::event_header {
                        kind: libgba_sys::message_kind::MESSAGE_KEY as i32,
                        size: std::mem::size_of::<libgba_sys::message_key>(),
                    },
                    key: key.raw(),
                    pressed: pressed,
                };
                libgba_sys::channel_push(raw_channel, &msg.header);
            }
        }
    }
}

#[derive(Debug, Eq, PartialEq, Ord, PartialOrd)]
pub struct MessageChannel<'a> {
    inner: *mut libgba_sys::gba,
    phantom: PhantomData<&'a Gba>,
}

impl<'a> MessageChannel<'a> {
    pub(crate) fn from(gba: &Gba) -> Self {
        Self {
            inner: gba.inner,
            phantom: PhantomData,
        }
    }

    pub fn lock(&self) -> MessageChannelLocked<'a> {
        MessageChannelLocked::from(self)
    }

    pub fn lock_and_send(&self, message: Message) {
        let locked_channel = self.lock();
        locked_channel.send(message);
    }
}

pub struct MessageChannelLocked<'a> {
    inner: *mut libgba_sys::gba,
    phantom: PhantomData<&'a mut Gba>,
}

impl<'a> MessageChannelLocked<'a> {
    pub(crate) fn from(channel: &MessageChannel) -> Self {
        unsafe {
            libgba_sys::channel_lock(&mut (*channel.inner).channels.messages);
        }

        Self {
            inner: channel.inner,
            phantom: PhantomData,
        }
    }

    pub fn send(&self, message: Message) {
        unsafe {
            message.send_over(self);
        }
    }
}

impl<'a> Drop for MessageChannelLocked<'a> {
    fn drop(&mut self) {
        unsafe {
            libgba_sys::channel_release(&mut (*self.inner).channels.messages);
        }
    }
}
