use std::marker::PhantomData;

use crate::config::GbaConfig;
use crate::gba::Gba;
use crate::libgba_sys;

pub enum Message {
    Exit,
    Reset(GbaConfig),
    Run,
    Pause,
}

impl Message {
    fn raw_no_extension(&self, kind: i32) -> libgba_sys::message {
        libgba_sys::message {
            header: libgba_sys::event_header {
                kind,
                size: std::mem::size_of::<libgba_sys::message>(),
            },
        }
    }

    unsafe fn send(self, channel: &MessageChannelLocked) {
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
            message.send(self);
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
