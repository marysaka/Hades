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
#include "dbg/dbg.h"

struct io_register g_io_registers[64];
size_t g_io_registers_len;

static
struct io_register *
debugger_io_new_register(
    uint32_t addr,
    size_t size,
    char const *name
) {
    struct io_register *reg;

    hs_assert(g_io_registers_len < array_length(g_io_registers));

    reg = &g_io_registers[g_io_registers_len];
    ++g_io_registers_len;

    memset(reg, 0, sizeof(*reg));
    reg->address = addr;
    reg->size = size;
    reg->name = name;
    return (reg);
}

static
struct io_register *
debugger_io_new_register16(
    uint32_t addr,
    char const *name,
    uint16_t *ptr
) {
    struct io_register *reg;

    reg = debugger_io_new_register(addr, 2, name);
    reg->ptr16 = ptr;
    return (reg);
}

static
void
debugger_io_new_bitfield(
    struct io_register *reg,
    size_t start,
    size_t end,
    char const *label,
    char const *hint
) {
    struct io_bitfield *bitfield;

    hs_assert(reg->bitfield_len < array_length(reg->bitfield));

    bitfield = &reg->bitfield[reg->bitfield_len];
    ++reg->bitfield_len;

    bitfield->start = start;
    bitfield->end = end;
    bitfield->label = label;
    bitfield->hint = hint;
}

/*
** The name and description provided in this function are from GBATEK.
**   - https://problemkaputt.de/gbatek.htm
*/
void
debugger_io_init(
    struct gba *gba
) {
    // Display
    {
        struct io_register *reg;

        reg = debugger_io_new_register16(IO_REG_DISPCNT, "LCD Control", NULL);
        debugger_io_new_bitfield(reg, 0,  2,  "BG Mode",                    "(0-5=Video Mode 0-5, 6-7=Prohibited)");
        debugger_io_new_bitfield(reg, 3,  3,  "CGB Mode",                   "(0=GBA, 1=CGB; can be set only by BIOS opcodes)");
        debugger_io_new_bitfield(reg, 4,  4,  "Display Frame Select",       "(0-1=Frame 0-1) (for BG Modes 4,5 only)");
        debugger_io_new_bitfield(reg, 5,  5,  "H-Blank Interval Free",      "(1=Allow access to OAM during H-Blank)");
        debugger_io_new_bitfield(reg, 6,  6,  "OBJ Character VRAM Mapping", "(0=Two dimensional, 1=One dimensional)");
        debugger_io_new_bitfield(reg, 7,  7,  "Forced Blank",               "(1=Allow FAST access to VRAM, Palette, OAM)");
        debugger_io_new_bitfield(reg, 8,  8,  "Screen Display BG0",         "(0=Off, 1=On)");
        debugger_io_new_bitfield(reg, 9,  9,  "Screen Display BG1",         "(0=Off, 1=On)");
        debugger_io_new_bitfield(reg, 10, 10, "Screen Display BG2",         "(0=Off, 1=On)");
        debugger_io_new_bitfield(reg, 11, 11, "Screen Display BG3",         "(0=Off, 1=On)");
        debugger_io_new_bitfield(reg, 12, 12, "Screen Display OBJ",         "(0=Off, 1=On)");
        debugger_io_new_bitfield(reg, 13, 13, "Window 0 Display Flag",      "(0=Off, 1=On)");
        debugger_io_new_bitfield(reg, 14, 14, "Window 1 Display Flag",      "(0=Off, 1=On)");
        debugger_io_new_bitfield(reg, 15, 15, "OBJ Window Display Flag",    "(0=Off, 1=On)");

        reg = debugger_io_new_register16(IO_REG_DISPSTAT, "General LCD Status", NULL);
        debugger_io_new_bitfield(reg, 0, 0,  "V-Blank flag",         "(1=VBlank) (set in line 160..226; not 227)");
        debugger_io_new_bitfield(reg, 1, 1,  "H-Blank flag",         "(1=HBlank) (toggled in all lines, 0..227)");
        debugger_io_new_bitfield(reg, 2, 2,  "V-Counter flag",       "(1=Match)  (set in selected line)");
        debugger_io_new_bitfield(reg, 3, 3,  "V-Blank IRQ Enable",   "(1=Enable)");
        debugger_io_new_bitfield(reg, 4, 4,  "H-Blank IRQ Enable",   "(1=Enable)");
        debugger_io_new_bitfield(reg, 5, 5,  "V-Counter IRQ Enable", "(1=Enable)");
        debugger_io_new_bitfield(reg, 6, 6,  "Reserved (0)",         NULL);
        debugger_io_new_bitfield(reg, 7, 7,  "Reserved (0)",         NULL);
        debugger_io_new_bitfield(reg, 8, 15, "V-Count Setting",      NULL);

        reg = debugger_io_new_register16(IO_REG_VCOUNT, "Vertical Counter", NULL);
        debugger_io_new_bitfield(reg, 0, 7,  "Current Scanline", NULL);
        debugger_io_new_bitfield(reg, 8, 15, "Reserved (0)",     NULL);

        {
            struct io_register *bg0cnt;
            struct io_register *bg1cnt;
            struct io_register *bg2cnt;
            struct io_register *bg3cnt;
            size_t i;

            bg0cnt = debugger_io_new_register16(IO_REG_BG0CNT, "BG0 Control", NULL);
            bg1cnt = debugger_io_new_register16(IO_REG_BG1CNT, "BG1 Control", NULL);
            bg2cnt = debugger_io_new_register16(IO_REG_BG2CNT, "BG2 Control", NULL);
            bg3cnt = debugger_io_new_register16(IO_REG_BG3CNT, "BG3 Control", NULL);

            struct io_register *bgxcnt[] = {
                bg0cnt,
                bg1cnt,
                bg2cnt,
                bg3cnt,
            };

            for (i = 0; i < array_length(bgxcnt); ++i) {
                reg = bgxcnt[i];

                debugger_io_new_bitfield(reg,  0, 1,  "BG Priority",               "(0-3, 0=Highest)");
                debugger_io_new_bitfield(reg,  2, 3,  "Character Base Block",      "(0-3, in units of 16 KBytes)");
                debugger_io_new_bitfield(reg,  4, 5,  "Reserved (0)",              NULL);
                debugger_io_new_bitfield(reg,  6, 6,  "Mosaic",                    "(0=Disable, 1=Enable)");
                debugger_io_new_bitfield(reg,  7, 7,  "Colors/Palettes",           "(0=16/16, 1=256/1)");
                debugger_io_new_bitfield(reg,  8, 12, "Screen Base Block",         "(0-31, in units of 2 KBytes)");

                if (reg->address <= IO_REG_BG1CNT) {
                    debugger_io_new_bitfield(reg, 13, 13, "Reserved (0)",          NULL);
                } else {
                    debugger_io_new_bitfield(reg, 13, 13, "Display Area Overflow", "(0=Transparent, 1=Wraparound)");
                }

                debugger_io_new_bitfield(reg, 14, 15, "Screen Size",               "(0-3)");
            }
        }

        {
            struct io_register *bg0hofs;
            struct io_register *bg0vofs;
            struct io_register *bg1hofs;
            struct io_register *bg1vofs;
            struct io_register *bg2hofs;
            struct io_register *bg2vofs;
            struct io_register *bg3hofs;
            struct io_register *bg3vofs;
            size_t i;

            bg0hofs = debugger_io_new_register16(IO_REG_BG0HOFS, "BG0 X-Offset", &gba->io.bg_hoffset[0].raw);
            bg0vofs = debugger_io_new_register16(IO_REG_BG0VOFS, "BG0 Y-Offset", &gba->io.bg_voffset[0].raw);
            bg1hofs = debugger_io_new_register16(IO_REG_BG1HOFS, "BG1 X-Offset", &gba->io.bg_hoffset[1].raw);
            bg1vofs = debugger_io_new_register16(IO_REG_BG1VOFS, "BG1 Y-Offset", &gba->io.bg_voffset[1].raw);
            bg2hofs = debugger_io_new_register16(IO_REG_BG2HOFS, "BG2 X-Offset", &gba->io.bg_hoffset[2].raw);
            bg2vofs = debugger_io_new_register16(IO_REG_BG2VOFS, "BG2 Y-Offset", &gba->io.bg_voffset[2].raw);
            bg3hofs = debugger_io_new_register16(IO_REG_BG3HOFS, "BG3 X-Offset", &gba->io.bg_hoffset[3].raw);
            bg3vofs = debugger_io_new_register16(IO_REG_BG3VOFS, "BG3 Y-Offset", &gba->io.bg_voffset[3].raw);

            struct io_register *bgxhvofs[] = {
                bg0hofs,
                bg0vofs,
                bg1hofs,
                bg1vofs,
                bg2hofs,
                bg2vofs,
                bg3hofs,
                bg3vofs,
            };

            for (i = 0; i < array_length(bgxhvofs); ++i) {
                reg = bgxhvofs[i];

                debugger_io_new_bitfield(reg,  0, 8,   "Offset",       NULL);
                debugger_io_new_bitfield(reg,  9, 15,  "Reserved (0)", NULL);
            }
        }
    }
}

struct io_register *
debugger_io_lookup_reg(
    uint32_t address
) {
    size_t i;

    for (i = 0; i < g_io_registers_len; ++i) {
        struct io_register *reg;

        reg = &g_io_registers[i];
        if (address == (reg->address & ~(reg->size - 1))) {
            return (reg);
        }
    }
    return (NULL);
}
