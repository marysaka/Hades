/******************************************************************************\
**
**  This file is part of the Hades GBA Emulator, and is made available under
**  the terms of the GNU General Public License version 2.
**
**  Copyright (C) 2021-2023 - The Hades Authors
**
\******************************************************************************/

/*
** References:
**   - https://dillonbeliveau.com/2020/06/05/GBA-FLASH.html
**   - https://densinh.github.io/DenSinH/emulation/2021/02/01/gba-eeprom.html
*/

#define _GNU_SOURCE
#include <string.h>
#include <errno.h>
#include "gba.h"
#include "db.h"

uint8_t
mem_backup_storage_read8(
    struct gba const *gba,
    uint32_t addr
) {
    switch (gba->memory.backup_storage.type) {
        case BACKUP_FLASH64:
        case BACKUP_FLASH128:
            return (mem_flash_read8(gba, addr));
            break;
        case BACKUP_SRAM:
            return (gba->memory.backup_storage.data[addr & SRAM_MASK]);
            break;
        default:
            return (0);
    }
}

void
mem_backup_storage_write8(
    struct gba *gba,
    uint32_t addr,
    uint8_t val
) {
    switch (gba->memory.backup_storage.type) {
        case BACKUP_FLASH64:
        case BACKUP_FLASH128:
            mem_flash_write8(gba, addr, val);
            break;
        case BACKUP_SRAM:
            gba->memory.backup_storage.data[addr & SRAM_MASK] = val;
            gba->memory.backup_storage.dirty = true;
            break;
        default:
            break;
    }
}
