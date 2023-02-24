use std::marker::PhantomData;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};

use crate::message::MessageChannel;
use crate::notification::NotificationChannel;
use crate::{libgba_sys, GbaSharedData, Message, Notification};
use std::sync::mpsc::{self, Receiver, Sender};

#[derive(Debug)]
pub struct Gba {
    pub(crate) inner: *mut libgba_sys::gba,
    request_pause: AtomicBool,
    message_sender: Arc<Mutex<Sender<Message>>>,
    message_receiver: Receiver<Message>,
}

impl Gba {
    pub fn new() -> Self {
        let (message_sender, message_receiver): (Sender<Message>, Receiver<Message>) =
            mpsc::channel();

        Self {
            inner: unsafe { libgba_sys::gba_create() },
            request_pause: AtomicBool::new(false),
            message_sender: Arc::new(Mutex::new(message_sender)),
            message_receiver,
        }
    }

    pub fn channels<'a>(&self) -> (MessageChannel, NotificationChannel) {
        (
            MessageChannel::from(self.message_sender.clone()),
            NotificationChannel::from(self),
        )
    }

    pub fn shared_data<'a>(&self) -> GbaSharedData<'a> {
        GbaSharedData::from(self)
    }

    pub fn runner<'a>(&'a self) -> GbaRunner<'a> {
        GbaRunner::from(&self)
    }

    pub fn request_pause(&self) {
        self.request_pause.store(true, Ordering::SeqCst);
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
    raw: &'a Gba,
    phantom: PhantomData<&'a Gba>,
}

impl<'a> GbaRunner<'a> {
    pub(crate) fn from(raw: &'a Gba) -> Self {
        Self {
            raw,
            phantom: PhantomData,
        }
    }

    pub fn is_running(&self) -> bool {
        unsafe { !(*self.raw.inner).exit }
    }

    pub fn process_event(&mut self, notif_channel: &mut NotificationChannel, msg: Message) {
        match msg {
            Message::Exit => unsafe {
                (*self.raw.inner).exit = true;
            },
            Message::Reset(config) => {
                let raw_config = config.raw();

                unsafe {
                    libgba_sys::gba_reset(self.raw.inner, &raw_config);
                }
            }
            Message::Run => {
                unsafe {
                    (*self.raw.inner).state = libgba_sys::gba_states::GBA_STATE_RUN;
                }
                notif_channel.send(Notification::Run);
            }
            Message::Pause => {
                unsafe {
                    (*self.raw.inner).state = libgba_sys::gba_states::GBA_STATE_PAUSE;
                }
                notif_channel.send(Notification::Pause);
            }
            Message::Key { key, pressed } => unsafe {
                libgba_sys::gba_process_key_press(self.raw.inner, key.raw() as u32, pressed);
            },
        }
    }

    pub fn run(&mut self) {
        let (_, mut notif_channel) = self.raw.channels();

        while self.is_running() {
            while let Ok(msg) = self.raw.message_receiver.try_recv() {
                self.process_event(&mut notif_channel, msg);
            }

            let state = unsafe { (*self.raw.inner).state };

            if state == libgba_sys::gba_states::GBA_STATE_PAUSE {
                println!("SLEEPING");
                // TODO: wait channel without consuming (condition variable maybe?)
                if let Ok(msg) = self.raw.message_receiver.recv() {
                    self.process_event(&mut notif_channel, msg);
                }
            }

            if !self.is_running() {
                break;
            }

            if let Ok(pause) = self.raw.request_pause.compare_exchange(
                true,
                false,
                Ordering::SeqCst,
                Ordering::Acquire,
            ) {
                if pause {
                    self.process_event(&mut notif_channel, Message::Pause);
                }
            }

            let state = unsafe { (*self.raw.inner).state };

            match state {
                libgba_sys::gba_states::GBA_STATE_PAUSE => {
                    break;
                }
                libgba_sys::gba_states::GBA_STATE_RUN => unsafe {
                    libgba_sys::sched_run_for(self.raw.inner, u64::from(crate::CYCLES_PER_FRAME));
                },
            }
        }
    }
}

unsafe impl<'a> Send for GbaRunner<'a> {}
unsafe impl<'a> Sync for GbaRunner<'a> {}
