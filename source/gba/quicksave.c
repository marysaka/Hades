/******************************************************************************\
**
**  This file is part of the Hades GBA Emulator, and is made available under
**  the terms of the GNU General Public License version 2.
**
**  Copyright (C) 2021-2023 - The Hades Authors
**
\******************************************************************************/

#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "gba/gba.h"
#include "gba/scheduler.h"
#include "compat.h"

/*
** Save the current state of the emulator in the file pointed by `path`.
*/
void
quicksave(
    struct gba const *gba,
    char const *path
) {
    FILE *file;
    size_t i;

    file = hs_fopen(path, "wb");
    if (!file) {
        goto err;
    }

    if (
           fwrite(&gba->core, sizeof(gba->core), 1, file) != 1
        || fwrite(gba->memory.ewram, sizeof(gba->memory.ewram), 1, file) != 1
        || fwrite(gba->memory.iwram, sizeof(gba->memory.iwram), 1, file) != 1
        || fwrite(gba->memory.palram, sizeof(gba->memory.palram), 1, file) != 1
        || fwrite(gba->memory.vram, sizeof(gba->memory.vram), 1, file) != 1
        || fwrite(gba->memory.oam, sizeof(gba->memory.oam), 1, file) != 1
        || fwrite(&gba->memory.pbuffer, sizeof(gba->memory.pbuffer), 1, file) != 1
        || fwrite(&gba->memory.flash, sizeof(gba->memory.flash), 1, file) != 1
        || fwrite(&gba->memory.eeprom, sizeof(gba->memory.eeprom), 1, file) != 1
        || fwrite(&gba->memory.bios_bus, sizeof(gba->memory.bios_bus), 1, file) != 1
        || fwrite(&gba->memory.gamepak_bus_in_use, sizeof(gba->memory.gamepak_bus_in_use), 1, file) != 1
        || fwrite(&gba->io, sizeof(gba->io), 1, file) != 1
        || fwrite(&gba->ppu, sizeof(gba->ppu), 1, file) != 1
        || fwrite(&gba->gpio, sizeof(gba->gpio), 1, file) != 1
        || fwrite(&gba->apu.fifos, sizeof(gba->apu.fifos), 1, file) != 1
        || fwrite(&gba->apu.wave, sizeof(gba->apu.wave), 1, file) != 1
        || fwrite(&gba->apu.latch, sizeof(gba->apu.latch), 1, file) != 1
        || fwrite(&gba->scheduler.next_event, sizeof(uint64_t), 1, file) != 1
    ) {
        goto err;
    }

    // Serialize the scheduler's event list
    for (i = 0; i < gba->scheduler.events_size; ++i) {
        struct scheduler_event *event;

        event = gba->scheduler.events + i;
        if (
               fwrite(&event->active, sizeof(bool), 1, file) != 1
            || fwrite(&event->repeat, sizeof(bool), 1, file) != 1
            || fwrite(&event->at, sizeof(uint64_t), 1, file) != 1
            || fwrite(&event->period, sizeof(uint64_t), 1, file) != 1
            || fwrite(&event->args, sizeof(struct event_args), 1, file) != 1
        ) {
            goto err;
        }
    }

    fflush(file);

    logln(
        HS_INFO,
        "State saved to %s%s%s",
        g_light_magenta,
        path,
        g_reset
    );

    goto finally;

err:
    logln(
        HS_INFO,
        "%sError: failed to save state to %s: %s%s",
        g_light_red,
        path,
        strerror(errno),
        g_reset
    );

finally:

    fclose(file);
}

/*
** Load a new state for the emulator from the content of the file pointed by `path`.
*/
void
quickload(
    struct gba *gba,
    char const *path
) {
    FILE *file;
    size_t i;

    file = hs_fopen(path, "rb");
    if (!file) {
        goto err;
    }

    if (
           fread(&gba->core, sizeof(gba->core), 1, file) != 1
        || fread(gba->memory.ewram, sizeof(gba->memory.ewram), 1, file) != 1
        || fread(gba->memory.iwram, sizeof(gba->memory.iwram), 1, file) != 1
        || fread(gba->memory.palram, sizeof(gba->memory.palram), 1, file) != 1
        || fread(gba->memory.vram, sizeof(gba->memory.vram), 1, file) != 1
        || fread(gba->memory.oam, sizeof(gba->memory.oam), 1, file) != 1
        || fread(&gba->memory.pbuffer, sizeof(gba->memory.pbuffer), 1, file) != 1
        || fread(&gba->memory.flash, sizeof(gba->memory.flash), 1, file) != 1
        || fread(&gba->memory.eeprom, sizeof(gba->memory.eeprom), 1, file) != 1
        || fread(&gba->memory.bios_bus, sizeof(gba->memory.bios_bus), 1, file) != 1
        || fread(&gba->memory.gamepak_bus_in_use, sizeof(gba->memory.gamepak_bus_in_use), 1, file) != 1
        || fread(&gba->io, sizeof(gba->io), 1, file) != 1
        || fread(&gba->ppu, sizeof(gba->ppu), 1, file) != 1
        || fread(&gba->gpio, sizeof(gba->gpio), 1, file) != 1
        || fread(&gba->apu.fifos, sizeof(gba->apu.fifos), 1, file) != 1
        || fread(&gba->apu.wave, sizeof(gba->apu.wave), 1, file) != 1
        || fread(&gba->apu.latch, sizeof(gba->apu.latch), 1, file) != 1
        || fread(&gba->scheduler.next_event, sizeof(uint64_t), 1, file) != 1
    ) {
        goto err;
    }

    // Serialize the scheduler's event list
    for (i = 0; i < gba->scheduler.events_size; ++i) {
        struct scheduler_event *event;

        event = gba->scheduler.events + i;
        if (
               fread(&event->active, sizeof(bool), 1, file) != 1
            || fread(&event->repeat, sizeof(bool), 1, file) != 1
            || fread(&event->at, sizeof(uint64_t), 1, file) != 1
            || fread(&event->period, sizeof(uint64_t), 1, file) != 1
            || fread(&event->args, sizeof(struct event_args), 1, file) != 1
        ) {
            goto err;
        }
    }

    logln(
        HS_INFO,
        "State loaded from %s%s%s",
        g_light_magenta,
        path,
        g_reset
    );

    goto finally;

err:
    logln(
        HS_INFO,
        "%sError: failed to load state from %s: %s%s",
        g_light_red,
        path,
        strerror(errno),
        g_reset
    );

finally:

    if (file) {
        fclose(file);
    }
}
