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
#include "channel/channel.h"
#include "core.h"
#include "scheduler.h"
#include "memory.h"
#include "ppu.h"
#include "apu.h"
#include "io.h"
#include "gpio.h"

enum gba_states {
    GBA_STATE_PAUSE = 0,
    GBA_STATE_RUN,
};

struct shared_data {
    // The emulator's screen, as built by the PPU each frame.
    uint32_t framebuffer[GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT];
    pthread_mutex_t framebuffer_mutex;

    // The frame counter, used for FPS calculations.
    atomic_uint frame_counter;

    // Audio ring buffer.
    pthread_mutex_t audio_rbuffer_mutex;
    struct apu_rbuffer audio_rbuffer;
};

struct gba {
    bool exit;
    atomic_bool request_pause;

    // The current state of the GBA
    enum gba_states state;

    // The channel used to communicate with the frontend
    struct channels channels;

    // Shared data with the frontend, mainly the framebuffer and audio channels.
    struct shared_data shared_data;

    // The different components of the GBA
    struct core core;
    struct scheduler scheduler;
    struct memory memory;
    struct ppu ppu;
    struct apu apu;
    struct io io;
    struct gpio gpio;
};

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

/* source/gba.c */
struct gba *gba_create(void);
void gba_run(struct gba *gba);
void gba_request_pause(struct gba *gba);
void gba_delete(struct gba *gba);
void gba_shared_framebuffer_lock(struct gba *gba);
void gba_shared_framebuffer_release(struct gba *gba);
void gba_shared_audio_rbuffer_lock(struct gba *gba);
void gba_shared_audio_rbuffer_release(struct gba *gba);
