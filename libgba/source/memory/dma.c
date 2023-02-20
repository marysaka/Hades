/******************************************************************************\
**
**  This file is part of the Hades GBA Emulator, and is made available under
**  the terms of the GNU General Public License version 2.
**
**  Copyright (C) 2021-2023 - The Hades Authors
**
\******************************************************************************/

#include "hades.h"
#include "gba.h"
#include "scheduler.h"

static uint32_t src_mask[4]   = {0x07FFFFFF, 0x0FFFFFFF, 0x0FFFFFFF, 0x0FFFFFFF};
static uint32_t dst_mask[4]   = {0x07FFFFFF, 0x07FFFFFF, 0x07FFFFFF, 0x0FFFFFFF};
static uint32_t count_mask[4] = {0x3FFF,     0x3FFF,     0x3FFF,     0xFFFF};

void
mem_io_dma_ctl_write8(
    struct gba *gba,
    struct dma_channel *channel,
    uint8_t val
) {
    bool old;
    bool new;

    channel = &gba->io.dma[channel->index];

    old = channel->control.enable;
    channel->control.bytes[1] = val;
    channel->control.gamepak_drq &= (channel->index == 3);
    new = channel->control.enable;

    if (!old) {
        // 0 -> 1, the DMA is enabled
        if (new) {
            channel->is_fifo = (channel->index >= 1 && channel->index <= 2 && channel->control.timing == DMA_TIMING_SPECIAL);
            channel->is_video = (channel->index == 3 && channel->control.timing == DMA_TIMING_SPECIAL);

            // Find the amount of transfers the DMA will do
            if (channel->is_fifo) {
                channel->internal_count = 4;
            } else {
                channel->internal_count = channel->count.raw;
                channel->internal_count &= count_mask[channel->index];

                // A count of 0 is treated as max length.
                if (channel->internal_count == 0) {
                    channel->internal_count = count_mask[channel->index] + 1;
                }
            }

            channel->internal_src = channel->src.raw & (channel->control.unit_size ? ~3 : ~1);
            channel->internal_src &= src_mask[channel->index];
            channel->internal_dst = channel->dst.raw & (channel->control.unit_size ? ~3 : ~1);
            channel->internal_dst &= dst_mask[channel->index];

            if (channel->control.timing == DMA_TIMING_NOW) {
                mem_schedule_dma_transfers_for(gba, channel->index, DMA_TIMING_NOW);
            }
        }
    } else {
        // 1 -> 0, the DMA is canceled
        if (!new) {
            if (channel->enable_event_handle != INVALID_EVENT_HANDLE) {
                sched_cancel_event(gba, channel->enable_event_handle);
                channel->enable_event_handle = INVALID_EVENT_HANDLE;
            }

            gba->core.pending_dma &= ~(1 << channel->index);
            if (gba->core.is_dma_running) {
                gba->core.reenter_dma_transfer_loop = true;
            }
        }
    }
}

/*
** Run a single DMA transfer.
*/
static
void
dma_run_channel(
    struct gba *gba,
    struct dma_channel *channel
) {
    enum access_types access;
    int32_t src_step;
    int32_t dst_step;
    int32_t unit_size;

    unit_size = channel->control.unit_size ? sizeof(uint32_t) : sizeof(uint16_t); // In  bytes

    if (channel->is_fifo) {
        dst_step = 0;
    } else {
        switch (channel->control.dst_ctl) {
            case 0b00:      dst_step = unit_size; break;
            case 0b01:      dst_step = -unit_size; break;
            case 0b10:      dst_step = 0; break;
            case 0b11:      dst_step = unit_size; break;
        }
    }

    switch (channel->control.src_ctl) {
        case 0b00:      src_step = unit_size; break;
        case 0b01:      src_step = -unit_size; break;
        case 0b10:      src_step = 0; break;
        case 0b11:      src_step = 0; break;
    }

    logln(
        HS_DMA,
        "DMA transfer from 0x%08x%c to 0x%08x%c (len=%#08x, unit_size=%u, channel %zu)",
        channel->internal_src,
        src_step > 0 ? '+' : '-',
        channel->internal_dst,
        dst_step > 0 ? '+' : '-',
        channel->internal_count,
        unit_size,
        channel->index
    );

    access = NON_SEQUENTIAL;
    if (unit_size == 4) {
        while (channel->internal_count > 0 && !gba->core.reenter_dma_transfer_loop) {
            if (likely(channel->internal_src >= EWRAM_START)) {
                channel->bus = mem_read32(gba, channel->internal_src, access);
            } else {
                core_idle(gba);
            }
            mem_write32(gba, channel->internal_dst, channel->bus, access);
            channel->internal_src += src_step;
            channel->internal_dst += dst_step;
            channel->internal_count -= 1;
            access = SEQUENTIAL;
        }
    } else { // unit_size == 2
        while (channel->internal_count > 0 && !gba->core.reenter_dma_transfer_loop) {
            if (likely(channel->internal_src >= EWRAM_START)) {

                /*
                ** Not sure what's the expected behaviour regarding the DMA's open bus behaviour/latch management,
                ** this is more or less random.
                */
                channel->bus <<= 16;
                channel->bus |= mem_read16(gba, channel->internal_src, access);
            } else {
                core_idle(gba);
            }
            mem_write16(gba, channel->internal_dst, channel->bus, access);
            channel->internal_src += src_step;
            channel->internal_dst += dst_step;
            channel->internal_count -= 1;
            access = SEQUENTIAL;
        }
    }

    if (gba->core.reenter_dma_transfer_loop) {
        return ;
    }

    gba->core.pending_dma &= ~(1 << channel->index);
    gba->io.int_flag.raw |= (channel->control.irq_end << (IRQ_DMA0 + channel->index));

    if (channel->control.repeat) {
        if (channel->is_fifo) {
            channel->internal_count = 4;
        } else if (channel->is_video) {
            if (gba->io.vcount.raw < GBA_SCREEN_HEIGHT + 1) {
                channel->internal_count = channel->count.raw;
                channel->internal_count &= count_mask[channel->index];

                if (channel->control.dst_ctl == 0b11) {
                    channel->internal_dst = channel->dst.raw & (channel->control.unit_size ? ~3 : ~1);
                    channel->internal_dst &= dst_mask[channel->index];
                }
            } else {
                channel->control.enable = false;
            }
        } else {
            channel->internal_count = channel->count.raw;
            channel->internal_count &= count_mask[channel->index];

            if (channel->control.dst_ctl == 0b11) {
                channel->internal_dst = channel->dst.raw & (channel->control.unit_size ? ~3 : ~1);
                channel->internal_dst &= dst_mask[channel->index];
            }
        }
    } else {
        channel->control.enable = false;
    }
}

/*
** Go through all DMA channels and process all the ones waiting for the given timing.
*/
void
mem_dma_do_all_pending_transfers(
    struct gba *gba
) {
    size_t i;

    if (!gba->core.pending_dma) {
        return ;
    }

    gba->core.is_dma_running = true;
    core_idle(gba);

    while (gba->core.pending_dma) {
        gba->core.reenter_dma_transfer_loop = false;

        for (i = 0; i < 4; ++i) {
            struct dma_channel *channel;

            channel = &gba->io.dma[i];

            // Skip channels that aren't enabled or that shouldn't happen at the given timing
            if (!(gba->core.pending_dma & (1 << i))) {
                continue;
            }

            gba->core.current_dma = channel;
            dma_run_channel(gba, channel);
            gba->core.current_dma = NULL;
            break;
        }
    }

    core_idle(gba);
    gba->core.is_dma_running = false;
}

static
void
mem_dma_add_to_pending(
    struct gba *gba,
    struct event_args args
) {
    uint32_t channel_idx;

    channel_idx = args.a1.u32;
    gba->io.dma[channel_idx].enable_event_handle = INVALID_EVENT_HANDLE;
    gba->core.pending_dma |= (1 << channel_idx);
    if (gba->core.is_dma_running) {
        gba->core.reenter_dma_transfer_loop = true;
    }
}

void
mem_schedule_dma_transfers_for(
    struct gba *gba,
    uint32_t channel_idx,
    enum dma_timings timing
) {
    struct dma_channel *channel;

    channel = &gba->io.dma[channel_idx];
    if (channel->control.enable && channel->control.timing == timing) {
        channel->enable_event_handle = sched_add_event(
            gba,
            NEW_FIX_EVENT_ARGS(
                gba->scheduler.cycles + 2,
                mem_dma_add_to_pending,
                EVENT_ARG(u32, channel_idx)
            )
        );
    }
}

void
mem_schedule_dma_transfers(
    struct gba *gba,
    enum dma_timings timing
) {
    uint32_t i;

    for (i = 0; i < 4; ++i) {
        mem_schedule_dma_transfers_for(gba, i, timing);
    }
}

bool
mem_dma_is_fifo(
    struct gba const *gba,
    uint32_t dma_channel_idx,
    uint32_t fifo_idx
) {
    struct dma_channel const *dma;

    dma = &gba->io.dma[dma_channel_idx];
    return (
           dma->control.enable
        && dma->control.timing == DMA_TIMING_SPECIAL
        && dma->dst.raw == (fifo_idx == FIFO_A ? IO_REG_FIFO_A_L : IO_REG_FIFO_B_L)
    );
}
