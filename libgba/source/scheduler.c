/******************************************************************************\
**
**  This file is part of the Hades GBA Emulator, and is made available under
**  the terms of the GNU General Public License version 2.
**
**  Copyright (C) 2021-2023 - The Hades Authors
**
\******************************************************************************/

#include <string.h>
#include "gba.h"
#include "scheduler.h"
#include "memory.h"

void
sched_process_events(
    struct gba *gba
) {
    struct scheduler *scheduler;

    scheduler = &gba->scheduler;
    while (true) {
        struct scheduler_event *event;
        uint64_t next_event;
        uint64_t delay;
        size_t i;

        event = NULL;

        next_event = UINT64_MAX;

        // We want to fire all the events in the correct order, hence the complicated
        // loop.
        for (i = 0; i < scheduler->events_size; ++i) {

            // Keep only the event that are active and should occure now
            if (scheduler->events[i].active) {
                if (scheduler->events[i].at <= scheduler->cycles) {
                    if (!event || scheduler->events[i].at < event->at) {
                        event = scheduler->events + i;
                    }
                } else if (scheduler->events[i].at < next_event) {
                    next_event = scheduler->events[i].at;
                }
            }
        }

        scheduler->next_event = next_event;

        if (!event) {
            break;
        }

        // We 'rollback' the cycle counter for the duration of the callback
        delay = scheduler->cycles - event->at;
        scheduler->cycles -= delay;

        if (event->repeat) {
            event->at += event->period;

            if (event->at < scheduler->next_event) {
                scheduler->next_event = event->at;
            }

        } else {
            event->active = false;
        }

        event->callback(gba, event->args);
        scheduler->cycles += delay;
    }
}

event_handler_t
sched_add_event(
    struct gba *gba,
    struct scheduler_event event
) {
    struct scheduler *scheduler;
    size_t i;

    scheduler = &gba->scheduler;

    hs_assert(!event.repeat || event.period);

    // Try and reuse an inactive event
    for (i = 0; i < scheduler->events_size; ++i) {
        if (!scheduler->events[i].active) {
            scheduler->events[i] = event;
            scheduler->events[i].active = true;
            goto end;
        }
    }

    // If no event are available, reallocate `scheduler->events`.
    scheduler->events_size += 5;
    scheduler->events = realloc(scheduler->events, scheduler->events_size * sizeof(struct scheduler_event));
    hs_assert(scheduler->events);

    scheduler->events[i] = event;
    scheduler->events[i].active = true;

end:
    if (event.at < scheduler->next_event) {
        scheduler->next_event = event.at;
    }

    return (i);
}

void
sched_cancel_event(
    struct gba *gba,
    event_handler_t handler
) {
    struct scheduler *scheduler;

    scheduler = &gba->scheduler;

    if (scheduler->events[handler].active) {
        scheduler->events[handler].active = false;
    }

    // TODO: update `scheduler->next_event`? Is it worth it?
}

void
sched_run_for(
    struct gba *gba,
    uint64_t cycles
) {
    struct scheduler *scheduler;
    uint64_t target;

    scheduler = &gba->scheduler;
    target = scheduler->cycles + cycles;

#ifdef WITH_DEBUGGER
    while (scheduler->cycles < target && !atomic_read(&gba->request_pause)) {
#else
    while (scheduler->cycles < target) {
#endif
        uint64_t elapsed;
        uint64_t old_cycles;

        old_cycles = scheduler->cycles;
        core_step(gba);
        elapsed = scheduler->cycles - old_cycles;

        if (!elapsed) {
            if (gba->core.state != CORE_STOP) {
                logln(HS_WARNING, "No cycles elapsed during `scheduler_next()`.");
            }
            break;
        }
    }
}
