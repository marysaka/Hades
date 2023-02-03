/******************************************************************************\
**
**  This file is part of the Hades GBA Emulator, and is made available under
**  the terms of the GNU General Public License version 2.
**
**  Copyright (C) 2021-2023 - The Hades Authors
**
\******************************************************************************/

#pragma once

#include <pthread.h>

struct event_header {
    int32_t kind;
    size_t size;
};

enum message_kind {
    MESSAGE_EXIT,
    MESSAGE_RESET,
    MESSAGE_RUN,
    MESSAGE_PAUSE,
};

struct message {
    struct event_header header;
};

struct message_reset {
    struct event_header header;
    struct gba_config *config;
};

enum notification_kind {
    NOTIFICATION_RUN,
    NOTIFICATION_PAUSE,
};

struct notification {
    struct event_header header;
};

struct channel {
    struct event_header *events;
    size_t length;          // The number of event in `events`
    size_t size;            // The sum of the size of all the events in `events`
    size_t allocated_size;  // The size of the allocation of `events`

    pthread_mutex_t lock;
    pthread_cond_t ready;
};

struct channels {
    struct channel messages;        // Sent by the frontned to the emulator
    struct channel notifications;   // Sent by the emulator to the frontend
};

/* channel.c */
void channel_lock(struct channel *channel);
void channel_release(struct channel *channel);
void channel_push(struct channel *channel, struct event_header const *event);
void channel_wait(struct channel *channel);
struct event_header const *channel_next(struct channel const *channel, struct event_header const *event);
void channel_clear(struct channel *channel);
