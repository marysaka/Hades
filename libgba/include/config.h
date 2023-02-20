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
#include "memory.h"

struct gba_config {
    // The game ROM and its size
    uint8_t const *rom;
    size_t rom_size;

    // The BIOS and its size
    uint8_t const *bios;
    size_t bios_size;

    // True if the BIOS should be skipped
    bool skip_bios;

    // Set to the frontend's audio frequency.
    // Can be 0 if the frontend has no audio.
    uint32_t audio_frequency;

    // True if RTC is enabled, false otherwise.
    bool rtc;

    // The kind of storage type to use.
    enum backup_storage_types backup_storage_type;
};
