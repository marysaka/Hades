use crate::{Gba, libgba_sys};

use std::marker::PhantomData;

pub struct GbaSharedData<'a> {
    inner: *mut libgba_sys::gba,
    phantom: PhantomData<&'a Gba>,
}

impl<'a> GbaSharedData<'a> {
    pub(crate) fn from(gba: &Gba) -> Self {
        Self {
            inner: gba.inner,
            phantom: PhantomData,
        }
    }

    /// Lock the shared data and return a copy of the framebuffer
    pub fn copy_framebuffer_into(&mut self, raw: &mut [u8]) {
        unsafe {
            libgba_sys::gba_shared_framebuffer_lock(self.inner);
            let fb = &(*self.inner).shared_data.framebuffer;
            let len = fb.len();
            let slice_u8 = std::slice::from_raw_parts(fb.as_ptr() as *const u8, len * 4);
            raw.copy_from_slice(slice_u8);
            libgba_sys::gba_shared_framebuffer_release(self.inner);
        }
    }
}
