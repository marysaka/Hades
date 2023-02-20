/******************************************************************************\
**
**  This file is part of the Hades GBA Emulator, and is made available under
**  the terms of the GNU General Public License version 2.
**
**  Copyright (C) 2021-2023 - The Hades Authors
**
\******************************************************************************/

#include <string.h>
#include "hades.h"
#include "channel.h"

/*
** Lock the channel.
*/
void
channel_lock(
    struct channel *channel
) {
    pthread_mutex_lock(&channel->lock);
}

/*
** Release the channel.
*/
void
channel_release(
    struct channel *channel
) {
    pthread_mutex_unlock(&channel->lock);
}

/*
** Push an event at the end of a channel.
**
** The channel must already be locked.
*/
void
channel_push(
    struct channel *channel,
    struct event_header const *event
) {
    size_t new_size;

    // Reallocate if `channel->events` is too small.
    new_size = channel->size + event->size;
    if (channel->allocated_size < new_size) {
        if (!channel->allocated_size) {
            channel->allocated_size += 1;
        }

        while (channel->allocated_size < new_size) {
            channel->allocated_size *= 2;
        }
        channel->events = realloc(channel->events, channel->allocated_size);
        hs_assert(channel->events);
    }

    // Copy the new event at the end of the buffer
    memcpy((uint8_t *)channel->events + channel->size, event, event->size);

    // Update the channel's length and size
    channel->length += 1;
    channel->size += event->size;

    pthread_cond_broadcast(&channel->ready);
}

/*
** Wait for an event to be available.
**
** The channel must already be locked.
*/
void
channel_wait(
    struct channel *channel
) {
    pthread_cond_wait(&channel->ready, &channel->lock);
}

/*
** Return a read-only view of the next event in the channel.
** One can iterate over the events contained in a channel by calling
** this function multiple time, using the previous return value as the
** new value for `old_event`.
**
** On the first call, `old_event` must be NULL.
** Return `NULL` if there is no more event in the channel.
**
** The return pointer is valid until `channel_release` or `channel_clear_events` is called.
**
** The channel must already be locked.
*/
struct event_header const *
channel_next(
    struct channel const *channel,
    struct event_header const *old_event
) {
    struct event_header *new_event;

    if (old_event) {
        new_event = (struct event_header *)((uint8_t *)old_event + old_event->size);
    } else {
        new_event = channel->events;
    }

    if (new_event < channel->events + channel->length) {
        return (new_event);
    }
    return (NULL);
}

/*
** Clear the channel of all of its events.
**
** The channel must already be locked.
*/
void
channel_clear(
    struct channel *channel
) {
    channel->length = 0;
    channel->size = 0;
}
