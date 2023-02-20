//mod framebuffer;
mod gui;

use crate::gui::Gui;

use std::fmt::Debug;
use std::fs;

use hs_dbg::Debugger;
use hs_gba::{Gba, Message, Notification, GbaConfig, GBA_SCREEN_WIDTH, GBA_SCREEN_HEIGHT, BackupStorageType};

use clap::Parser;
use lazy_static::lazy_static;
use pixels::{Pixels, SurfaceTexture};
use winit::dpi::LogicalSize;
use winit::event::{Event, VirtualKeyCode};
use winit::event_loop::{ControlFlow, EventLoop};
use winit::window::WindowBuilder;
use winit_input_helper::WinitInputHelper;

lazy_static! {
    static ref GBA: Gba = Gba::new();
}

const DEFAULT_WINDOW_WIDTH: u32 = GBA_SCREEN_WIDTH * 3;
const DEFAULT_WINDOW_HEIGHT: u32 = GBA_SCREEN_HEIGHT * 3;

/// A Nintendo GameBoy Advance Emulator.
#[derive(Parser, Debug)]
#[command(name = "hades", author, version, about, long_about = None)]
struct Args {
    /// Path to the ROM to emulate
    #[arg()]
    rom: std::path::PathBuf,

    /// Path to the BIOS binary blob
    #[arg(short, long, default_value = "bios.bin")]
    bios: std::path::PathBuf,
}

fn main() {
    let args = Args::parse();
    let mut config = GbaConfig::new();
    let (msg_channel, notif_channel) = GBA.channels();

    config.with_rtc(true);
    config.with_backup_storage_type(BackupStorageType::Flash128);

    // Read the ROM
    match fs::read(&args.rom) {
        Ok(rom) => config.with_rom(rom),
        Err(e) => {
            eprintln!("Failed to read ROM: {}: {}", args.rom.display(), e);
            return
        }
    }

    // Read the BIOS
    match fs::read(&args.bios) {
        Ok(bios) => config.with_bios(bios),
        Err(e) => {
            eprintln!("Failed to read BIOS: {}: {}", args.bios.display(), e);
            return
        }
    }

    // Create the Window, EventLoop and WinitInputHelper
    let event_loop = EventLoop::new();
    let mut input = WinitInputHelper::new();
    let window = WindowBuilder::new()
        .with_title("Hades")
        .with_inner_size(LogicalSize::new(DEFAULT_WINDOW_WIDTH as f64, DEFAULT_WINDOW_HEIGHT as f64))
        .with_min_inner_size(LogicalSize::new(DEFAULT_WINDOW_WIDTH as f64, DEFAULT_WINDOW_HEIGHT as f64))
        .build(&event_loop)
        .unwrap();

    // Create the framebuffer used to render the window
    let mut pixels = Pixels::new(
        GBA_SCREEN_WIDTH,
        GBA_SCREEN_HEIGHT,
        SurfaceTexture::new(
            window.inner_size().width,
            window.inner_size().height,
            &window
        )
    ).expect("Failed to create the window's framebuffer");

    let mut gui = Gui::new(&window, &pixels);

    // Start the GBA
    std::thread::spawn(move || {
        GBA.runner().run();
    });

    // Reset the GBA with the given configuration
    msg_channel.lock_and_send(Message::Reset(config));

    // Wait for the reset notification
    'notif_loop: loop {
        notif_channel.wait();
        for notif in notif_channel.pop() {
            if let Notification::Reset{..} = notif {
                break 'notif_loop;
            }
        }
    }

    // Start the debugger thread
    let dbg_thread = std::thread::spawn(|| {
        let mut debugger = Debugger::from(&GBA);
        let (msg_channel, _) = GBA.channels();

        // Set the Ctrl-C handler
        ctrlc::set_handler(|| {
            GBA.request_pause();
        }).expect("Error setting Ctrl-C handler");

        // Start the GBA
        msg_channel.lock_and_send(Message::Run);
        debugger.wait_for_gba();

        debugger.repl();
    });

    // Run the event loop.
    event_loop.run(move |event, _, control_flow| {

        // Handle draw request
        if let Event::RedrawRequested(_) = event {
            // Draw the game's content
            GBA.shared_data().copy_framebuffer_into(pixels.get_frame_mut());

            // Prepare the Gui
            gui.prepare(&window).expect("Gui::prepare() failed");

            // Render everything together
            pixels.render_with(|encoder, render_target, context| {
                context.scaling_renderer.render(encoder, render_target);
                gui.render(&window, encoder, render_target, context)?;
                Ok(())
            }).expect("Pixels::render() failed");
        }

        // Transfer events to the Gui
        gui.handle_event(&window, &event);

        if input.update(&event) {

            // Handle all close events
            if input.key_pressed(VirtualKeyCode::Escape) || input.quit() || dbg_thread.is_finished() {
                *control_flow = ControlFlow::Exit;

                // We create a useless Reedline instance to ensure the terminal is reset
                let _ignore = reedline::Reedline::create();
                return;
            }

            // Resize the surface texture if the window was resized
            if let Some(size) = input.window_resized() {
                if size.width > 0 && size.height > 0 {
                    pixels.resize_surface(size.width, size.height).expect("Pixels::resize_surface() failed");
                }
            }

            window.request_redraw();
        }
    });
}
