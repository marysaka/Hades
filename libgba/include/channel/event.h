/******************************************************************************\
**
**  This file is part of the Hades GBA Emulator, and is made available under
**  the terms of the GNU General Public License version 2.
**
**  Copyright (C) 2021-2023 - The Hades Authors
**
\******************************************************************************/

#pragma once

#include "hades.h"
#include "gba.h"

/*
** Messages
*/

enum message_kind {
    MESSAGE_EXIT,
    MESSAGE_RESET,
    MESSAGE_RUN,
    MESSAGE_PAUSE,
    MESSAGE_KEY,
};

struct message {
    struct event_header header;
};

struct message_reset {
    struct event_header header;
    struct gba_config config;
};

enum keys {
    KEY_A,
    KEY_B,
    KEY_L,
    KEY_R,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_START,
    KEY_SELECT,
};

struct message_key {
    struct event_header header;
    enum keys key;
    bool pressed;
};

/*
** Notifications
*/

enum notification_kind {
    NOTIFICATION_RUN,
    NOTIFICATION_PAUSE,
    NOTIFICATION_RESET,
};

struct notification {
    struct event_header header;
};
