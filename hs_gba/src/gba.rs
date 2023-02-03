use crate::bindings;
use crate::notification::NotificationChannel;
use crate::message::MessageChannel;

#[derive(Debug, Eq, PartialEq, Ord, PartialOrd)]
pub struct Gba {
    inner: *mut bindings::gba,
    message_channel: MessageChannel,
    notification_channel: NotificationChannel,
}

impl Gba {
    pub fn new() -> Self {
        let inner = unsafe {
            bindings::gba_create()
        };

        Self {
            inner,
            message_channel: MessageChannel::from(inner),
            notification_channel: NotificationChannel::from(inner),
        }
    }

    pub fn channels(&self) -> (&MessageChannel, &NotificationChannel) {
        (&self.message_channel, &self.notification_channel)
    }

    pub fn run(&self) {
        unsafe {
            bindings::gba_run(self.inner);
        }
    }
}

impl Drop for Gba {
    fn drop(&mut self) {
        unsafe {
            bindings::gba_delete(self.inner);
        }
    }
}

unsafe impl Send for Gba {}
unsafe impl Sync for Gba {}
