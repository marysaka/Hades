//mod framebuffer;
mod gui;

use crate::gui::Gui;

use std::collections::HashMap;
use std::fmt::Debug;
use std::fs;
use std::sync::Arc;

use hs_db::game_database_lookup;
use hs_dbg::Debugger;
use hs_gba::{
    Gba, GbaConfig, Key, Message, Notification, GBA_HEADER_GAME_CODE_LEN,
    GBA_HEADER_GAME_CODE_OFFSET, GBA_HEADER_LEN, GBA_SCREEN_HEIGHT, GBA_SCREEN_WIDTH,
};

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
    let mut title = "";
    let (msg_channel, notif_channel) = GBA.channels();

    // Read the ROM
    match fs::read(&args.rom) {
        Ok(rom) => {
            if rom.len() < GBA_HEADER_LEN {
                eprintln!("Invalid ROM: {}: bad header length", args.rom.display());
                return;
            }

            config.with_rom(rom)
        }
        Err(e) => {
            eprintln!("Failed to read ROM: {}: {}", args.rom.display(), e);
            return;
        }
    }

    // Read the BIOS
    match fs::read(&args.bios) {
        Ok(bios) => config.with_bios(bios),
        Err(e) => {
            eprintln!("Failed to read BIOS: {}: {}", args.bios.display(), e);
            return;
        }
    }

    let code = std::str::from_utf8(
        &config.rom()
            [GBA_HEADER_GAME_CODE_OFFSET..GBA_HEADER_GAME_CODE_OFFSET + GBA_HEADER_GAME_CODE_LEN],
    )
    .unwrap_or_default();

    println!("Welcome to Hades v{}", env!("CARGO_PKG_VERSION"));
    println!("=========================");

    // Look for the game in the database
    if let Some(entry) = game_database_lookup(code) {
        println!(
            "Game code \"{}\" identified as \"{}\"",
            entry.code(),
            entry.title()
        );

        title = entry.title();

        if let Some(backup_storage_type) = entry.backup_storage_type() {
            config.with_backup_storage_type(backup_storage_type);
        }

        config.with_rtc(entry.has_rtc());
    } else {
        println!("No game with code \"{code}\" could be found in the Hades game database.");
        println!("Configuration will be automatically detected.");
        config.detect_backup_storage();
    }

    // Read the backup storage
    if config.backup_storage_type().is_some() {
        match fs::read(&args.rom.with_extension("sav")) {
            Ok(backup) => config.with_backup_storage_data(backup),
            Err(e) => {
                eprintln!(
                    "Failed to read ROM's Backup storage: {}: {}",
                    args.rom.with_extension("sav").display(),
                    e
                );
            }
        }
    }

    // Print the configuration
    println!("Configuration:");
    println!(
        "    Backup storage: {}",
        config
            .backup_storage_type()
            .and_then(|b| Some(b.to_string()))
            .unwrap_or_else(|| "none".to_string())
    );
    println!("    Rtc: {}", config.rtc());

    // Create the Window, EventLoop and WinitInputHelper
    let event_loop = EventLoop::new();
    let mut input = WinitInputHelper::new();
    let window = WindowBuilder::new()
        .with_title(format!("Hades - {}", title))
        .with_inner_size(LogicalSize::new(
            DEFAULT_WINDOW_WIDTH as f64,
            DEFAULT_WINDOW_HEIGHT as f64,
        ))
        .with_min_inner_size(LogicalSize::new(
            DEFAULT_WINDOW_WIDTH as f64,
            DEFAULT_WINDOW_HEIGHT as f64,
        ))
        .build(&event_loop)
        .unwrap();

    // Create the framebuffer used to render the window
    let mut pixels = Pixels::new(
        GBA_SCREEN_WIDTH,
        GBA_SCREEN_HEIGHT,
        SurfaceTexture::new(
            window.inner_size().width,
            window.inner_size().height,
            &window,
        ),
    )
    .expect("Failed to create the window's framebuffer");

    let mut gui = Gui::new(&window, &pixels);

    // Start the GBA
    std::thread::spawn(move || {
        GBA.runner().run();
    });

    let config = Arc::new(config);

    // Reset the GBA with the given configuration
    msg_channel.send(Message::Reset(config.clone()));

    // Wait for the reset notification
    'notif_loop: loop {
        notif_channel.wait();
        for notif in notif_channel.pop() {
            println!("{:?}", notif);
            if let Notification::Reset { .. } = notif {
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
        })
        .expect("Error setting Ctrl-C handler");

        // Start the GBA
        msg_channel.send(Message::Run);
        debugger.wait_for_gba();

        debugger.repl();
    });

    // Quick default keybinds
    let mut keybinds = HashMap::new();
    keybinds.insert(Key::A, VirtualKeyCode::P);
    keybinds.insert(Key::B, VirtualKeyCode::L);
    keybinds.insert(Key::L, VirtualKeyCode::E);
    keybinds.insert(Key::R, VirtualKeyCode::O);
    keybinds.insert(Key::Up, VirtualKeyCode::W);
    keybinds.insert(Key::Down, VirtualKeyCode::S);
    keybinds.insert(Key::Left, VirtualKeyCode::A);
    keybinds.insert(Key::Right, VirtualKeyCode::D);
    keybinds.insert(Key::Select, VirtualKeyCode::Back);
    keybinds.insert(Key::Start, VirtualKeyCode::Return);

    // Run the event loop.
    event_loop.run(move |event, _, control_flow| {
        // Handle draw request
        if let Event::RedrawRequested(_) = event {
            // Draw the game's content
            GBA.shared_data()
                .copy_framebuffer_into(pixels.get_frame_mut());

            // Prepare the Gui
            gui.prepare(&window).expect("Gui::prepare() failed");

            // Render everything together
            pixels
                .render_with(|encoder, render_target, context| {
                    context.scaling_renderer.render(encoder, render_target);
                    gui.render(&window, encoder, render_target, context)?;
                    Ok(())
                })
                .expect("Pixels::render() failed");
        }

        // Transfer events to the Gui
        gui.handle_event(&window, &event);

        if input.update(&event) {
            // Handle GBA keybinds
            for (bind, key) in keybinds.iter() {
                if input.key_pressed(*key) || input.key_released(*key) {
                    msg_channel.send(Message::Key {
                        key: *bind,
                        pressed: input.key_pressed(*key),
                    });
                }
            }

            // Handle all close events
            if input.quit() || dbg_thread.is_finished() {
                *control_flow = ControlFlow::Exit;

                // We create a useless Reedline instance to ensure the terminal is reset
                let _ignore = reedline::Reedline::create();
                return;
            }

            // Resize the surface texture if the window was resized
            if let Some(size) = input.window_resized() {
                if size.width > 0 && size.height > 0 {
                    pixels
                        .resize_surface(size.width, size.height)
                        .expect("Pixels::resize_surface() failed");
                }
            }

            window.request_redraw();
        }
    });
}
