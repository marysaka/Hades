use hs_gba::{Gba, Message, MessageChannel, NotificationChannel, Notification};

use reedline::{DefaultPrompt, Reedline, Signal};

mod commands;

pub struct Debugger<'a> {
    message_channel: MessageChannel<'a>,
    notification_channel: NotificationChannel<'a>,
}

impl<'a> Debugger<'a> {
    pub fn from(gba: &'a Gba) -> Self {
        let channels = gba.channels();
        Self {
            message_channel: channels.0,
            notification_channel: channels.1,
        }
    }

    pub fn wait_for_gba(&mut self) {
        // Process notifications sent by the gba
        loop {
            self.notification_channel.wait();
            for notif in self.notification_channel.pop() {
                if let Notification::Pause = notif {
                    return
                }
            }
        }
    }

    fn execute(&mut self, cmd: &str) {
        let args = cmd.split_whitespace().collect::<Vec<_>>();

        if args.len() == 0 {
            return
        }

        match args[0] {
            "run" => {
                self.message_channel.lock_and_send(Message::Run);
                self.wait_for_gba();

            },
            _ => println!("Invalid command."),
        }
    }

    pub fn repl(&mut self) {
        let mut line_editor = Reedline::create();
        let prompt = DefaultPrompt::default();

        loop {
            let sig = line_editor.read_line(&prompt);
            match sig {
                Ok(Signal::Success(buffer)) => {
                    self.execute(&buffer);
                }
                Ok(Signal::CtrlD) => break,
                _ => (),
            }
        }
    }
}
