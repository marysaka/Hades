/******************************************************************************\
**
**  This file is part of the Hades GBA Emulator, and is made available under
**  the terms of the GNU General Public License version 2.
**
**  Copyright (C) 2021-2023 - The Hades Authors
**
\******************************************************************************/

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "channel.h"

enum gba_states {
    GBA_STATE_PAUSE = 0,
    GBA_STATE_RUN,
};

struct gba {
    bool exit;

    // The current state of the GBA
    enum gba_states state;

    // The initial configuration given when resetting the GBA.
    struct gba_config *config;

    // The channel used to communicate with the frontend
    struct channels channels;
};

struct gba_config {
    void *rom;
    size_t rom_size;
};

void gba_init(void);
struct gba *gba_create(void);
void gba_run(struct gba *gba);
void gba_delete(struct gba *gba);
