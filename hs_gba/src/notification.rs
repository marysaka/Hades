use std::marker::PhantomData;

use crate::gba::Gba;
use crate::libgba_sys;

#[derive(Copy, Clone, Debug)]
pub enum Notification {
    Run,
    Pause,
    Reset,
}

impl Notification {
    unsafe fn from_raw(raw: *const libgba_sys::notification) -> Self {
        match (*raw).header.kind {
            x if x == libgba_sys::notification_kind::NOTIFICATION_RUN as i32 => Notification::Run,
            x if x == libgba_sys::notification_kind::NOTIFICATION_PAUSE as i32 => {
                Notification::Pause
            }
            x if x == libgba_sys::notification_kind::NOTIFICATION_RESET as i32 => {
                Notification::Reset
            }
            kind => panic!(
                "Invalid notification kind {} received from the GBA thread",
                kind
            ),
        }
    }

    fn to_raw(self) -> i32 {
        match self {
            Notification::Run => libgba_sys::notification_kind::NOTIFICATION_RUN as i32,
            Notification::Pause => libgba_sys::notification_kind::NOTIFICATION_PAUSE as i32,
            Notification::Reset => libgba_sys::notification_kind::NOTIFICATION_RESET as i32,
        }
    }
}

#[derive(Debug, Eq, PartialEq, Ord, PartialOrd)]
pub struct NotificationChannel<'a> {
    inner: *mut libgba_sys::gba,
    phantom: PhantomData<&'a Gba>,
}

impl<'a> NotificationChannel<'a> {
    pub(crate) fn from(gba: &Gba) -> Self {
        Self {
            inner: gba.inner,
            phantom: PhantomData,
        }
    }

    pub fn send(&mut self, notif: Notification) {
        unsafe {
            let msg_channel = &mut (*self.inner).channels.notifications;

            let raw_notif = libgba_sys::notification {
                header: libgba_sys::event_header {
                    kind: notif.to_raw(),
                    size: core::mem::size_of::<libgba_sys::notification>(),
                },
            };

            libgba_sys::channel_lock(msg_channel);
            libgba_sys::channel_push(msg_channel, &raw_notif.header);
            libgba_sys::channel_release(msg_channel);
        }
    }

    pub fn pop(&self) -> Vec<Notification> {
        let mut ret = Vec::new();

        unsafe {
            let msg_channel = &mut (*self.inner).channels.notifications;
            libgba_sys::channel_lock(msg_channel);

            let mut event = libgba_sys::channel_next(msg_channel, std::ptr::null());
            while event != std::ptr::null() {
                ret.push(Notification::from_raw(
                    event as *const libgba_sys::notification,
                ));
                event = libgba_sys::channel_next(msg_channel, event);
            }

            libgba_sys::channel_clear(msg_channel);
            libgba_sys::channel_release(msg_channel);
        }

        ret
    }

    pub fn wait(&self) {
        unsafe {
            libgba_sys::channel_lock(&mut (*self.inner).channels.notifications);
            libgba_sys::channel_wait(&mut (*self.inner).channels.notifications);
            libgba_sys::channel_release(&mut (*self.inner).channels.notifications);
        }
    }
}
