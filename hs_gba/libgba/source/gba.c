/******************************************************************************\
**
**  This file is part of the Hades GBA Emulator, and is made available under
**  the terms of the GNU General Public License version 2.
**
**  Copyright (C) 2021-2023 - The Hades Authors
**
\******************************************************************************/

#include <string.h>
#include "utils.h"
#include "channel.h"
#include "gba.h"

/*
** Initialize libgba.
**
** This needs te be called only once, before any other libgba function.
*/
void
gba_init(void)
{
    /* Initialize the ARM and Thumb decoder */
    //core_arm_decode_insns();
    //core_thumb_decode_insns();
}

/*
** Create a new GBA emulator.
*/
struct gba *
gba_create(void)
{
    struct gba *gba;

    gba = malloc(sizeof(struct gba));
    hs_assert(gba);

    memset(gba, 0, sizeof(*gba));

    logln(HS_INFO, "Create");

    // Channels
    {
        pthread_mutex_init(&gba->channels.messages.lock, NULL);
        pthread_cond_init(&gba->channels.messages.ready, NULL);
        pthread_mutex_init(&gba->channels.notifications.lock, NULL);
        pthread_cond_init(&gba->channels.notifications.ready, NULL);
    }

    return (gba);
}

static
void
gba_process_message(
    struct gba *gba,
    struct message const *message
) {
    switch (message->header.kind) {
        case MESSAGE_EXIT: {
            printf("EXIT\n");
            gba->exit = true;
            break;
        };
        case MESSAGE_RUN: {
            printf("RUN\n");
            if (gba->state != GBA_STATE_RUN) {
                struct notification notif;

                // Notify the UI that we are now running.
                notif.header.kind = NOTIFICATION_RUN;
                notif.header.size = sizeof(notif);
                channel_lock(&gba->channels.notifications);
                channel_push(&gba->channels.notifications, &notif);
                channel_release(&gba->channels.notifications);
            }
            gba->state = GBA_STATE_RUN;
            break;
        };
        case MESSAGE_PAUSE: {
            printf("PAUSE\n");
            if (gba->state != GBA_STATE_PAUSE) {
                struct notification notif;

                // Notify the UI that we are now paused.
                notif.header.kind = NOTIFICATION_PAUSE;
                notif.header.size = sizeof(notif);
                channel_lock(&gba->channels.notifications);
                channel_push(&gba->channels.notifications, &notif);
                channel_release(&gba->channels.notifications);
            }
            gba->state = GBA_STATE_PAUSE;
            break;
        };
    }
}

/*
** Run the given GBA emulator.
** This will process all the message sent to the gba until an exit message is sent.
*/
void
gba_run(
    struct gba *gba
) {
    struct channel *messages;

    messages = &gba->channels.messages;

    while (!gba->exit) {
        // Consume all messages
        {
            struct message *msg;
            channel_lock(messages);

            msg = channel_next(messages, NULL);
            while (msg) {
                gba_process_message(gba, msg);
                msg = channel_next(messages, msg);
            }

            channel_clear(messages);
            channel_release(messages);
        }
    }
}

/*
** Delete the given GBA and all its resources.
*/
void
gba_delete(
    struct gba *gba
) {
    logln(HS_INFO, "Delete");
    free(gba);
}
