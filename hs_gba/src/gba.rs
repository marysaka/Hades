use std::marker::PhantomData;

use crate::message::MessageChannel;
use crate::notification::NotificationChannel;
use crate::{libgba_sys, GbaSharedData};

#[derive(Debug, Eq, PartialEq, Ord, PartialOrd)]
pub struct Gba {
    pub(crate) inner: *mut libgba_sys::gba,
}

impl Gba {
    pub fn new() -> Self {
        Self {
            inner: unsafe { libgba_sys::gba_create() },
        }
    }

    pub fn channels<'a>(&self) -> (MessageChannel<'a>, NotificationChannel) {
        (MessageChannel::from(self), NotificationChannel::from(self))
    }

    pub fn shared_data<'a>(&self) -> GbaSharedData<'a> {
        GbaSharedData::from(self)
    }

    pub fn runner<'a>(&self) -> GbaRunner<'a> {
        GbaRunner::from(self.inner)
    }

    pub fn request_pause(&self) {
        unsafe {
            libgba_sys::gba_request_pause(self.inner);
        }
    }
}

impl Drop for Gba {
    fn drop(&mut self) {
        unsafe {
            println!("DROP");
            libgba_sys::gba_delete(self.inner);
        }
    }
}

unsafe impl Send for Gba {}
unsafe impl Sync for Gba {}

pub struct GbaRunner<'a> {
    inner: *mut libgba_sys::gba,
    phantom: PhantomData<&'a Gba>,
}

impl<'a> GbaRunner<'a> {
    pub(crate) fn from(raw: *mut libgba_sys::gba) -> Self {
        Self {
            inner: raw,
            phantom: PhantomData,
        }
    }
    pub fn run(&mut self) {
        unsafe {
            libgba_sys::gba_run(self.inner);
        }
    }
}

unsafe impl<'a> Send for GbaRunner<'a> {}
unsafe impl<'a> Sync for GbaRunner<'a> {}
