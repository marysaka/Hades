use crate::bindings;

pub enum Message {
    Exit,
    Run,
    Pause,
}

impl Message {
    pub fn raw(self) -> bindings::message {
        match self {
            Message::Exit => {
                bindings::message {
                    header: bindings::event_header {
                        kind: bindings::message_kind::MESSAGE_EXIT as i32,
                        size: std::mem::size_of::<bindings::message>(),
                    }
                }
            },
            Message::Run => {
                bindings::message {
                    header: bindings::event_header {
                        kind: bindings::message_kind::MESSAGE_RUN as i32,
                        size: std::mem::size_of::<bindings::message>(),
                    }
                }
            },
            Message::Pause => {
                bindings::message {
                    header: bindings::event_header {
                        kind: bindings::message_kind::MESSAGE_PAUSE as i32,
                        size: std::mem::size_of::<bindings::message>(),
                    }
                }
            }
        }
    }
}

#[derive(Debug, Eq, PartialEq, Ord, PartialOrd)]
pub struct MessageChannel {
    inner: *mut bindings::gba,
}

impl MessageChannel {
    pub(crate) fn from(ptr: *mut bindings::gba) -> Self {
        Self {
            inner: ptr,
        }
    }

    pub fn send(&self, message: Message) {
        let raw_message = message.raw();
        let raw_event = &raw_message as *const bindings::message as *const bindings::event_header;

        unsafe {
            bindings::channel_lock(&mut (*self.inner).channels.messages);
            bindings::channel_push(&mut (*self.inner).channels.messages, raw_event);
            bindings::channel_release(&mut (*self.inner).channels.messages);
        }
    }
}

unsafe impl Send for MessageChannel {}
unsafe impl Sync for MessageChannel {}
