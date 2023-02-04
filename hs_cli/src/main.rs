use hs_dbg::Debugger;
use hs_gba::{Gba, Message};

use lazy_static::lazy_static;

lazy_static! {
    static ref GBA: Gba = Gba::new();
}

fn main() {
    let mut debugger = Debugger::from(&GBA);
    let (msg_channel, _) = GBA.channels();

    ctrlc::set_handler(|| {
        let (msg_channel, _) = GBA.channels();
        msg_channel.send(Message::Pause);
    }).expect("Error setting Ctrl-C handler");

    let gba_thread = std::thread::spawn(|| {
        GBA.run();
    });

    debugger.repl();

    msg_channel.send(Message::Exit);
    gba_thread.join().expect("Failed to join the GBA thread");
}
