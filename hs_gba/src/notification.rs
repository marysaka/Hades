use crate::bindings;

pub enum Notification {
    Run,
    Pause,
}

impl Notification {
    unsafe fn from_raw(raw: *const bindings::notification) -> Self {
        match (*raw).header.kind {
            x if x == bindings::notification_kind::NOTIFICATION_RUN as i32 => Notification::Run,
            x if x  == bindings::notification_kind::NOTIFICATION_PAUSE as i32 => Notification::Pause,
            kind => panic!("Invalid notification kind {} received from the GBA thread", kind),
        }
    }
}

#[derive(Debug, Eq, PartialEq, Ord, PartialOrd)]
pub struct NotificationChannel {
    inner: *mut bindings::gba,
}

impl NotificationChannel {
    pub(crate) fn from(ptr: *mut bindings::gba) -> Self {
        Self {
            inner: ptr,
        }
    }

    pub fn pop(&self) -> Vec<Notification> {
        let mut ret = Vec::new();

        unsafe {
            let msg_channel = &mut (*self.inner).channels.notifications;
            bindings::channel_lock(msg_channel);

            let mut event = bindings::channel_next(msg_channel, std::ptr::null());
            while event != std::ptr::null() {
                ret.push(Notification::from_raw(event as *const bindings::notification));
                event = bindings::channel_next(msg_channel, event);
            }

            bindings::channel_release(msg_channel);
        }

        ret
    }

    pub fn wait(&self) {
        unsafe {
            bindings::channel_lock(&mut (*self.inner).channels.notifications);
            bindings::channel_wait(&mut (*self.inner).channels.notifications);
            bindings::channel_release(&mut (*self.inner).channels.notifications);
        }
    }
}

unsafe impl Send for NotificationChannel {}
unsafe impl Sync for NotificationChannel {}
