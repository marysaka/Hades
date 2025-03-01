/******************************************************************************\
**
**  This file is part of the Hades GBA Emulator, and is made available under
**  the terms of the GNU General Public License version 2.
**
**  Copyright (C) 2021-2023 - The Hades Authors
**
\******************************************************************************/

#include <string.h>
#include <math.h>
#include "gba/gba.h"
#include "gba/ppu.h"

static void ppu_merge_layer(struct gba const *gba, struct scanline *scanline, struct rich_color *layer);

/*
** Initialize the content of the given `scanline` to a default, sane and working value.
*/
static
void
ppu_initialize_scanline(
    struct gba const *gba,
    struct scanline *scanline
) {
    struct rich_color backdrop;
    uint32_t x;

    memset(scanline, 0x00, sizeof(*scanline));

    backdrop.visible = true;
    backdrop.idx = 5;
    backdrop.raw = (gba->io.dispcnt.blank ? 0x7fff : mem_palram_read16(gba, PALRAM_START));

    for (x = 0; x < GBA_SCREEN_WIDTH; ++x) {
        scanline->result[x] = backdrop;
    }

    /*
    ** The only layer that `ppu_merge_layer` will never merge is the backdrop layer so we force
    ** it here instead (if that's useful).
    */

    if (gba->io.bldcnt.mode == BLEND_LIGHT || gba->io.bldcnt.mode == BLEND_DARK) {
        scanline->top_idx = 5;
        memcpy(scanline->bg, scanline->result, sizeof(scanline->bg));
        memcpy(scanline->bot, scanline->result, sizeof(scanline->bot));
        ppu_merge_layer(gba, scanline, scanline->bg);
        scanline->top_idx = 0;
    }
}

/*
** Merge the current layer with any previous ones (using alpha blending) as stated in REG_BLDCNT.
*/
static
void
ppu_merge_layer(
    struct gba const *gba,
    struct scanline *scanline,
    struct rich_color *layer
) {
    uint32_t eva;
    uint32_t evb;
    uint32_t evy;
    struct io const *io;
    uint32_t x;

    io = &gba->io;
    eva = min(16, io->bldalpha.top_coef);
    evb = min(16, io->bldalpha.bot_coef);
    evy = min(16, io->bldy.coef);

    for (x = 0; x < GBA_SCREEN_WIDTH; ++x) {
        bool bot_enabled;
        struct rich_color topc;
        struct rich_color botc;
        uint32_t mode;

        topc = layer[x];
        botc = scanline->bot[x];

        /* Skip transparent pixels */
        if (!topc.visible) {
            continue;
        }

        mode = gba->io.bldcnt.mode;
        bot_enabled = bitfield_get(io->bldcnt.raw, botc.idx + 8);

        /* Apply windowing, if any */
        if (scanline->top_idx <= 4 && (io->dispcnt.win0 || io->dispcnt.win1 || io->dispcnt.winobj)) {
            uint8_t win_opts;

            win_opts = ppu_find_top_window(gba, scanline, x);

            /* Hide pixels that belong to a layer that this window doesn't show. */
            if (!bitfield_get(win_opts, scanline->top_idx)) {
                continue;
            }

            /* Windows can disable blending */
            if (!bitfield_get(win_opts, 5)) {
                mode = BLEND_OFF;
            }
        }

        /* Sprite can force blending no matter what BLDCNT says */
        if (topc.force_blend && bot_enabled) {
            mode = BLEND_ALPHA;
        }

        scanline->bot[x] = layer[x];

        switch (mode) {
            case BLEND_OFF: {
                scanline->result[x] = topc;
                break;
            };
            case BLEND_ALPHA: {
                bool top_enabled;

                /*
                ** If both the top and bot layers are enabled, blend the colors.
                ** Otherwise, the top layer takes priority.
                */

                top_enabled = bitfield_get(io->bldcnt.raw, scanline->top_idx) || topc.force_blend;
                if (top_enabled && bot_enabled && botc.visible) {
                    scanline->result[x].red = min(31, ((uint32_t)topc.red * eva + (uint32_t)botc.red * evb) >> 4);
                    scanline->result[x].green = min(31, ((uint32_t)topc.green * eva + (uint32_t)botc.green * evb) >> 4);
                    scanline->result[x].blue = min(31, ((uint32_t)topc.blue * eva + (uint32_t)botc.blue * evb) >> 4);
                    scanline->result[x].visible = true;
                    scanline->result[x].idx = scanline->top_idx;
                } else {
                    scanline->result[x] = topc;
                }
                break;
            };
            case BLEND_LIGHT: {
                if (bitfield_get(io->bldcnt.raw, scanline->top_idx)) {
                    scanline->result[x].red = topc.red + (((31 - topc.red) * evy) >> 4);
                    scanline->result[x].green = topc.green + (((31 - topc.green) * evy) >> 4);
                    scanline->result[x].blue = topc.blue + (((31 - topc.blue) * evy) >> 4);
                    scanline->result[x].idx = topc.idx;
                    scanline->result[x].visible = true;
                } else {
                    scanline->result[x] = topc;
                }
                break;
            };
            case BLEND_DARK: {
                if (bitfield_get(io->bldcnt.raw, scanline->top_idx)) {
                    scanline->result[x].red = topc.red - ((topc.red * evy) >> 4);
                    scanline->result[x].green = topc.green - ((topc.green * evy) >> 4);
                    scanline->result[x].blue = topc.blue - ((topc.blue * evy) >> 4);
                    scanline->result[x].idx = topc.idx;
                    scanline->result[x].visible = true;
                } else {
                    scanline->result[x] = topc;
                }
                break;
            };
        }
    }
}

/*
** Render the current scanline and write the result in `gba->framebuffer`.
*/
static
void
ppu_render_scanline(
    struct gba *gba,
    struct scanline *scanline
) {
    struct io const *io;
    int32_t prio;
    uint32_t y;

    io = &gba->io;
    y = gba->io.vcount.raw;

    switch (io->dispcnt.bg_mode) {
        case 0: {
            for (prio = 3; prio >= 0; --prio) {
                int32_t bg_idx;

                for (bg_idx = 3; bg_idx >= 0; --bg_idx) {
                    if (bitfield_get((uint8_t)io->dispcnt.bg, bg_idx) && io->bgcnt[bg_idx].priority == prio) {
                        ppu_render_background_text(gba, scanline, y, bg_idx);
                        ppu_merge_layer(gba, scanline, scanline->bg);
                    }
                }
                scanline->top_idx = 4;
                ppu_merge_layer(gba, scanline, scanline->oam[prio]);
            }
            break;
        };
        case 1: {
            for (prio = 3; prio >= 0; --prio) {
                int32_t bg_idx;

                for (bg_idx = 2; bg_idx >= 0; --bg_idx) {
                    if (bitfield_get((uint8_t)io->dispcnt.bg, bg_idx) && io->bgcnt[bg_idx].priority == prio) {
                        if (bg_idx == 2) {
                            memset(scanline->bg, 0x00, sizeof(scanline->bg));
                            ppu_render_background_affine(gba, scanline, y, bg_idx);
                        } else {
                            ppu_render_background_text(gba, scanline, y, bg_idx);
                        }
                        ppu_merge_layer(gba, scanline, scanline->bg);
                    }
                }
                scanline->top_idx = 4;
                ppu_merge_layer(gba, scanline, scanline->oam[prio]);
            }
            break;
        };
        case 2: {
            for (prio = 3; prio >= 0; --prio) {
                int32_t bg_idx;

                for (bg_idx = 3; bg_idx >= 2; --bg_idx) {
                    if (bitfield_get((uint8_t)io->dispcnt.bg, bg_idx) && io->bgcnt[bg_idx].priority == prio) {
                        memset(scanline->bg, 0x00, sizeof(scanline->bg));
                        ppu_render_background_affine(gba, scanline, y, bg_idx);
                        ppu_merge_layer(gba, scanline, scanline->bg);
                    }
                }
                scanline->top_idx = 4;
                ppu_merge_layer(gba, scanline, scanline->oam[prio]);
            }
            break;
        };
        case 3: {
            for (prio = 3; prio >= 0; --prio) {
                if (bitfield_get((uint8_t)io->dispcnt.bg, 2) && io->bgcnt[2].priority == prio) {
                    memset(scanline->bg, 0x00, sizeof(scanline->bg));
                    ppu_render_background_bitmap(gba, scanline, false);
                    ppu_merge_layer(gba, scanline, scanline->bg);
                }
                scanline->top_idx = 4;
                ppu_merge_layer(gba, scanline, scanline->oam[prio]);
            }
            break;
        };
        case 4: {
            for (prio = 3; prio >= 0; --prio) {
                if (bitfield_get((uint8_t)io->dispcnt.bg, 2) && io->bgcnt[2].priority == prio) {
                    memset(scanline->bg, 0x00, sizeof(scanline->bg));
                    ppu_render_background_bitmap(gba, scanline, true);
                    ppu_merge_layer(gba, scanline, scanline->bg);
                }
                scanline->top_idx = 4;
                ppu_merge_layer(gba, scanline, scanline->oam[prio]);
            }
            break;
        };
        case 5: {
            for (prio = 3; prio >= 0; --prio) {
                if (bitfield_get((uint8_t)io->dispcnt.bg, 2) && io->bgcnt[2].priority == prio && y < 128) {
                    memset(scanline->bg, 0x00, sizeof(scanline->bg));
                    ppu_render_background_bitmap_small(gba, scanline);
                    ppu_merge_layer(gba, scanline, scanline->bg);
                }
                scanline->top_idx = 4;
                ppu_merge_layer(gba, scanline, scanline->oam[prio]);
            }
            break;
        };
    }
}

/*
** Compose the content of the framebuffer based on the content of `scanline->result` and/or the backdrop color.
*/
static
void
ppu_draw_scanline(
    struct gba *gba,
    struct scanline const *scanline
) {
    uint32_t x;
    uint32_t y;

    y = gba->io.vcount.raw;
    for (x = 0; x < GBA_SCREEN_WIDTH; ++x) {
        struct rich_color c;

        c = scanline->result[x];
        gba->framebuffer[GBA_SCREEN_WIDTH * y + x] = 0xFF000000
            | (((uint32_t)c.red   << 3 ) | (((uint32_t)c.red   >> 2) & 0b111)) << 0
            | (((uint32_t)c.green << 3 ) | (((uint32_t)c.green >> 2) & 0b111)) << 8
            | (((uint32_t)c.blue  << 3 ) | (((uint32_t)c.blue  >> 2) & 0b111)) << 16
        ;
    }
}

/*
** Compose the content of the framebuffer based on the content of `scanline->result` and/or the backdrop color.
** This algorithm applies a color correction.
**
** NOTE: lcd_gamma is 4.0, out_gamma is 2.0.
** Reference:
**   - https://near.sh/articles/video/color-emulation
*/
static
void
ppu_draw_scanline_color_correction(
    struct gba *gba,
    struct scanline const *scanline
) {
    uint32_t x;
    uint32_t y;

    y = gba->io.vcount.raw;
    for (x = 0; x < GBA_SCREEN_WIDTH; ++x) {
        struct rich_color c;
        float r;
        float g;
        float b;

        c = scanline->result[x];

        r = c.red * c.red * c.red * c.red           / (31.0 * 31.0 * 31.0 * 31.0);  // <=> pow(c.red   / 31.0, lcd_gamma);
        g = c.green * c.green * c.green * c.green   / (31.0 * 31.0 * 31.0 * 31.0);  // <=> pow(c.green / 31.0, lcd_gamma);
        b = c.blue * c.blue * c.blue * c.blue       / (31.0 * 31.0 * 31.0 * 31.0);  // <=> pow(c.blue  / 31.0, lcd_gamma);

        gba->framebuffer[GBA_SCREEN_WIDTH * y + x] = 0xFF000000
            | (uint32_t)(sqrt(            0.196 * g + 1.000 * r) * 213.0) << 0      // <=> pow(r, 1.0 / out_gamma);
            | (uint32_t)(sqrt(0.118 * b + 0.902 * g + 0.039 * r) * 240.0) << 8      // <=> pow(g, 1.0 / out_gamma);
            | (uint32_t)(sqrt(0.863 * b + 0.039 * g + 0.196 * r) * 232.0) << 16     // <=> pow(b, 1.0 / out_gamma);
        ;
    }
}

/*
** Called when the PPU enters HDraw, this function updates some IO registers
** to reflect the progress of the PPU and eventually triggers an IRQ.
*/
static
void
ppu_hdraw(
    struct gba *gba,
    struct event_args args __unused
) {
    struct io *io;

    io = &gba->io;

    /* Increment VCOUNT */
    ++io->vcount.raw;

    if (io->vcount.raw >= GBA_SCREEN_REAL_HEIGHT) {
        io->vcount.raw = 0;
        ++gba->framecounter;
    } else if (io->vcount.raw == GBA_SCREEN_HEIGHT) {
        /*
        ** Now that the frame is finished, we can copy the current framebuffer to
        ** the one the frontend uses.
        **
        ** Doing it now will avoid tearing.
        */
        pthread_mutex_lock(&gba->framebuffer_frontend_mutex);
        memcpy(gba->framebuffer_frontend, gba->framebuffer, sizeof(gba->framebuffer));
        pthread_mutex_unlock(&gba->framebuffer_frontend_mutex);
    }

    io->dispstat.vcount_eq = (io->vcount.raw == io->dispstat.vcount_val);
    io->dispstat.vblank = (io->vcount.raw >= GBA_SCREEN_HEIGHT && io->vcount.raw < GBA_SCREEN_REAL_HEIGHT - 1);
    io->dispstat.hblank = false;

    /* Trigger the VBlank IRQ & DMA transfer */
    if (io->vcount.raw == GBA_SCREEN_HEIGHT) {
        if (io->dispstat.vblank_irq) {
            gba->io.int_flag.vblank = true;
        }
        mem_schedule_dma_transfers(gba, DMA_TIMING_VBLANK);
        gba->ppu.reload_internal_affine_regs = true;
    }

    // This is set either on VBlank (see above) or when the affine registers are written to.
    if (gba->ppu.reload_internal_affine_regs) {
        ppu_reload_affine_internal_registers(gba, 0);
        ppu_reload_affine_internal_registers(gba, 1);
        gba->ppu.reload_internal_affine_regs = false;
    }

    /* Trigger the VCOUNT IRQ */
    if (io->dispstat.vcount_eq && io->dispstat.vcount_irq) {
        gba->io.int_flag.vcounter = true;
    }
}

/*
** Called when the PPU enters HBlank, this function updates some IO registers
** to reflect the progress of the PPU and eventually triggers an IRQ.
*/
static
void
ppu_hblank(
    struct gba *gba,
    struct event_args args __unused
) {
    struct io *io;

    io = &gba->io;

    if (io->vcount.raw < GBA_SCREEN_HEIGHT) {
        struct scanline scanline;

        ppu_initialize_scanline(gba, &scanline);

        if (!gba->io.dispcnt.blank) {
            ppu_window_build_masks(gba, io->vcount.raw);
            ppu_prerender_oam(gba, &scanline, io->vcount.raw);
            ppu_render_scanline(gba, &scanline);
        }

        if (gba->color_correction) {
            ppu_draw_scanline_color_correction(gba, &scanline);
        } else {
            ppu_draw_scanline(gba, &scanline);
        }

        ppu_step_affine_internal_registers(gba);
    }

    io->dispstat.hblank = true;

    /*
    ** Trigger the HBLANK IRQ & DMA transfer
    */

    if (io->dispstat.hblank_irq) {
        gba->io.int_flag.hblank = true;
    }

    if (io->vcount.raw < GBA_SCREEN_HEIGHT) {
        mem_schedule_dma_transfers(gba, DMA_TIMING_HBLANK);
    }

    if (io->vcount.raw >= 2 && io->vcount.raw < GBA_SCREEN_HEIGHT + 2) {
        mem_schedule_dma_transfers_for(gba, 3, DMA_TIMING_SPECIAL);  // Video DMA
    }
}

/*
** Initialize the PPU.
*/
void
ppu_init(
    struct gba *gba
) {
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

/*
** Called when the CPU enters stop-mode to render the screen black.
*/
void
ppu_render_black_screen(
    struct gba *gba
) {
    pthread_mutex_lock(&gba->framebuffer_frontend_mutex);
    memset(gba->framebuffer_frontend, 0x00, sizeof(gba->framebuffer));
    pthread_mutex_unlock(&gba->framebuffer_frontend_mutex);
}
