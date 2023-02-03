use hs_dbg::Debugger;
use hs_gba::{Gba, Message, Notification};

use crossbeam::thread;
use reedline::{DefaultPrompt, Reedline, Signal};

fn main() {
    let gba = Gba::new();
    let (msg_channels, notification_channel) = gba.channels();
    let mut debugger = Debugger::from(&gba);

    thread::scope(|s| {
        s.spawn(|_| {
            gba.run();
        });

        let mut line_editor = Reedline::create();
        let prompt = DefaultPrompt::default();

        loop {
            let sig = line_editor.read_line(&prompt);
            match sig {
                Ok(Signal::Success(buffer)) => {
                    debugger.execute(&buffer);
                }
                Ok(Signal::CtrlC) => {
                    msg_channels.send(Message::Pause);
                },
                Ok(Signal::CtrlD) => break,
                _ => (),
            }

            let mut is_running = false;

            loop {
                // Process notifications sent by the gba
                for notif in notification_channel.pop() {
                    match notif {
                        Notification::Run => is_running = true,
                        Notification::Pause => is_running = false,
                    }
                }

                // Wait if the GBA is running, leave the loop if it is not.
                if is_running {
                    println!("WAITING");
                    notification_channel.wait()
                } else {
                    println!("PROMPT AGAIN");
                    break;
                }
            }
        }

        msg_channels.send(Message::Exit);
    }).unwrap();
}
