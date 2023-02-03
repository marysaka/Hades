use hs_gba::{Gba, Message, MessageChannel, NotificationChannel};

mod commands;

pub struct Debugger<'a> {
    message_channel: &'a MessageChannel,
    notification_channel: &'a NotificationChannel,
}

impl<'a> Debugger<'a> {
    pub fn from(gba: &'a Gba) -> Self {
        let channels = gba.channels();
        Self {
            message_channel: channels.0,
            notification_channel: channels.1,
        }
    }

    pub fn execute(&mut self, cmd: &str) {
        let args = cmd.split_whitespace().collect::<Vec<_>>();

        if args.len() != 1 {
            println!("Invalid command.");
            return
        }

        match args[0] {
            "run" => {
                self.message_channel.send(Message::Run)
            },
            "pause" => {
                self.message_channel.send(Message::Pause)
            },
            _ => println!("Invalid command."),
        }
    }
}
