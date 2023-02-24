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
#include "gba.h"
#include "channel/channel.h"
#include "channel/event.h"
#include "core/arm.h"
#include "core/thumb.h"

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

    // Initialize the ARM and Thumb decoder
    {
        core_arm_decode_insns();
        core_thumb_decode_insns();
    }

    // Channels
    {
        pthread_mutex_init(&gba->channels.notifications.lock, NULL);
        pthread_cond_init(&gba->channels.notifications.ready, NULL);
    }

    // Shared Data
    {
        pthread_mutex_init(&gba->shared_data.framebuffer_mutex, NULL);
        pthread_mutex_init(&gba->shared_data.audio_rbuffer_mutex, NULL);
    }

    return (gba);
}

void
gba_reset(
    struct gba *gba,
    struct gba_config const *config
) {
    // Free pointers from any previous run
    {
        free(gba->scheduler.events);
        free(gba->memory.backup_storage.data);
    }

    // Scheduler
    {
        struct scheduler *scheduler;

        scheduler = &gba->scheduler;
        memset(scheduler, 0, sizeof(*scheduler));

        scheduler->events_size = 64;
        scheduler->events = calloc(scheduler->events_size, sizeof(struct scheduler_event));
        hs_assert(scheduler->events);
    }

    // Memory
    {
        struct memory *memory;

        memory = &gba->memory;
        memset(memory, 0, sizeof(*memory));

        // Copy the BIOS and ROM to memory
        memcpy(gba->memory.bios, config->bios, min(config->bios_size, BIOS_SIZE));
        memcpy(gba->memory.rom, config->rom, min(config->rom_size, CART_SIZE));
        gba->memory.rom_size = config->rom_size;
    }

    // IO
    {
        struct io *io;

        io = &gba->io;
        memset(io, 0, sizeof(*io));

        io->keyinput.raw = 0x3FF; // Every button set to "released"
        io->soundbias.bias = 0x200;
        io->bg_pa[0].raw = 0x100;
        io->bg_pd[0].raw = 0x100;
        io->bg_pa[1].raw = 0x100;
        io->bg_pd[1].raw = 0x100;
        io->timers[0].handler = INVALID_EVENT_HANDLE;
        io->timers[1].handler = INVALID_EVENT_HANDLE;
        io->timers[2].handler = INVALID_EVENT_HANDLE;
        io->timers[3].handler = INVALID_EVENT_HANDLE;
        io->dma[0].enable_event_handle = INVALID_EVENT_HANDLE;
        io->dma[1].enable_event_handle = INVALID_EVENT_HANDLE;
        io->dma[2].enable_event_handle = INVALID_EVENT_HANDLE;
        io->dma[3].enable_event_handle = INVALID_EVENT_HANDLE;
        io->dma[0].index = 0;
        io->dma[1].index = 1;
        io->dma[2].index = 2;
        io->dma[3].index = 3;
    }

    // APU
    {
        struct apu *apu;

        apu = &gba->apu;
        memset(apu, 0, sizeof(*apu));

        // Wave Channel
        gba->apu.wave.step_handler = INVALID_EVENT_HANDLE;
        gba->apu.wave.counter_handler = INVALID_EVENT_HANDLE;

        sched_add_event(
            gba,
            NEW_REPEAT_EVENT(
                0,
                CYCLES_PER_SECOND / 256,
                apu_sequencer
            )
        );

        if (gba->apu.resample_frequency) {
            sched_add_event(
                gba,
                NEW_REPEAT_EVENT(
                    0,
                    gba->apu.resample_frequency,
                    apu_resample
                )
            );
        }
    }

    // PPU
    {
        struct ppu *ppu;

        ppu = &gba->ppu;
        memset(ppu, 0, sizeof(*ppu));

        // HDraw
        sched_add_event(
            gba,
            NEW_REPEAT_EVENT(
                CYCLES_PER_PIXEL * GBA_SCREEN_REAL_WIDTH,       // Timing of first trigger
                CYCLES_PER_PIXEL * GBA_SCREEN_REAL_WIDTH,       // Period
                ppu_hdraw
            )
        );

        // HBlank
        sched_add_event(
            gba,
            NEW_REPEAT_EVENT(
                CYCLES_PER_PIXEL * GBA_SCREEN_WIDTH + 46,       // Timing of first trigger
                CYCLES_PER_PIXEL * GBA_SCREEN_REAL_WIDTH,       // Period
                ppu_hblank
            )
        );
    }

    // GPIO
    {
        struct gpio *gpio;

        gpio = &gba->gpio;
        memset(gpio, 0, sizeof(*gpio));

        if (config->rtc) {
            gpio->rtc.enabled = true;
            gpio->rtc.state = RTC_COMMAND;
            gpio->rtc.data_len = 8;
            printf("RTC ENABLED\n");
        }
    }

    // Backup storage
    {
        gba->memory.backup_storage.type = config->backup_storage_type;
        switch (gba->memory.backup_storage.type) {
            case BACKUP_EEPROM_4K: {
                gba->memory.backup_storage.eeprom.mask = (gba->memory.rom_size > 16 * 1024 * 1024) ? 0x01FFFF00 : 0xFF000000;
                gba->memory.backup_storage.eeprom.range = (gba->memory.rom_size > 16 * 1024 * 1024) ? 0x01FFFF00 : 0x0d000000;
                gba->memory.backup_storage.eeprom.address_mask = EEPROM_4K_ADDR_MASK;
                gba->memory.backup_storage.eeprom.address_len = EEPROM_4K_ADDR_LEN;
                gba->memory.backup_storage.size = EEPROM_4K_SIZE;
                break;
            };
            case BACKUP_EEPROM_64K: {
                gba->memory.backup_storage.eeprom.mask = (gba->memory.rom_size > 16 * 1024 * 1024) ? 0x01FFFF00 : 0xFF000000;
                gba->memory.backup_storage.eeprom.range = (gba->memory.rom_size > 16 * 1024 * 1024) ? 0x01FFFF00 : 0x0d000000;
                gba->memory.backup_storage.eeprom.address_mask = EEPROM_64K_ADDR_MASK;
                gba->memory.backup_storage.eeprom.address_len = EEPROM_64K_ADDR_LEN;
                gba->memory.backup_storage.size = EEPROM_64K_SIZE;
                break;
            };
            case BACKUP_SRAM: gba->memory.backup_storage.size = SRAM_SIZE; break;
            case BACKUP_FLASH64: gba->memory.backup_storage.size = FLASH64_SIZE; break;
            case BACKUP_FLASH128:gba->memory.backup_storage.size = FLASH128_SIZE; break;
            case BACKUP_NONE: gba->memory.backup_storage.size = 0; break;
        }

        if (gba->memory.backup_storage.size) {
            gba->memory.backup_storage.data = calloc(1, gba->memory.backup_storage.size);
            hs_assert(gba->memory.backup_storage.data);

            if (config->backup && config->backup_size) {
                printf("BACKUP PROVIDED\n");
                memcpy(gba->memory.backup_storage.data, config->backup, min(gba->memory.backup_storage.size, config->backup_size));
            }
        }

    }

    // Core
    {
        struct core *core;

        core = &gba->core;

        memset(core, 0, sizeof(*core));

        core->r13_irq = 0x03007FA0;
        core->r13_svc = 0x03007FE0;
        core->sp = 0x03007F00;
        core->cpsr.mode = MODE_SYS;
        core->prefetch_access_type = NON_SEQUENTIAL;
        mem_update_waitstates(gba);

        if (config->skip_bios) {
            gba->core.pc = 0x08000000;
            gba->io.postflg = 1;
            core_reload_pipeline(gba);
        } else {
            core_interrupt(gba, VEC_RESET, MODE_SVC);
        }
    }

    // Send a reset notification back to the frontend
    {
        struct notification notif;

        notif.header.kind = NOTIFICATION_RESET;
        notif.header.size = sizeof(notif);
        channel_lock(&gba->channels.notifications);
        channel_push(&gba->channels.notifications, &notif.header);
        channel_release(&gba->channels.notifications);
    }
}

void
gba_process_key_press(
    struct gba *gba,
    uint32_t key,
    bool pressed
) {
    switch (key) {
        case KEY_A:         gba->io.keyinput.a = !pressed; break;
        case KEY_B:         gba->io.keyinput.b = !pressed; break;
        case KEY_L:         gba->io.keyinput.l = !pressed; break;
        case KEY_R:         gba->io.keyinput.r = !pressed; break;
        case KEY_UP:        gba->io.keyinput.up = !pressed; break;
        case KEY_DOWN:      gba->io.keyinput.down = !pressed; break;
        case KEY_RIGHT:     gba->io.keyinput.right = !pressed; break;
        case KEY_LEFT:      gba->io.keyinput.left = !pressed; break;
        case KEY_START:     gba->io.keyinput.start = !pressed; break;
        case KEY_SELECT:    gba->io.keyinput.select = !pressed; break;
    };

    io_scan_keypad_irq(gba);
}

/*
** Delete the given GBA and all its resources.
*/
void
gba_delete(
    struct gba *gba
) {
    free(gba);
}

/*
** Lock the mutex protecting the framebuffer shared with the frontend.
*/
void
gba_shared_framebuffer_lock(
    struct gba *gba
) {
    pthread_mutex_lock(&gba->shared_data.framebuffer_mutex);
}

/*
** Release the mutex protecting the framebuffer shared with the frontend.
*/
void
gba_shared_framebuffer_release(
    struct gba *gba
) {
    pthread_mutex_unlock(&gba->shared_data.framebuffer_mutex);
}

/*
** Lock the mutex protecting the audio ring buffer shared with the frontend.
*/
void
gba_shared_audio_rbuffer_lock(
    struct gba *gba
) {
    pthread_mutex_lock(&gba->shared_data.audio_rbuffer_mutex);
}

/*
** Release the mutex protecting the audio ring buffer shared with the frontend.
*/
void
gba_shared_audio_rbuffer_release(
    struct gba *gba
) {
    pthread_mutex_unlock(&gba->shared_data.audio_rbuffer_mutex);
}
