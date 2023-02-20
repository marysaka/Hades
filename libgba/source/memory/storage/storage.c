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

size_t const backup_storage_sizes[] = {
    [BACKUP_NONE] = 0,
    [BACKUP_EEPROM_4K] = EEPROM_4K_SIZE,
    [BACKUP_EEPROM_64K] = EEPROM_64K_SIZE,
    [BACKUP_SRAM] = SRAM_SIZE,
    [BACKUP_FLASH64] = FLASH64_SIZE,
    [BACKUP_FLASH128] = FLASH128_SIZE,
};

void
mem_backup_storage_init(
    struct gba *gba
) {
    free(gba->memory.backup_storage_data);

    if (   gba->memory.backup_storage_type == BACKUP_EEPROM_4K
        || gba->memory.backup_storage_type == BACKUP_EEPROM_64K
    ) {

        /*
        ** Those are masks applied to the address of any ROM data transfers
        ** to know if it's targetting the ROM or the EEPROM.
        ** They depend on the ROM's size.
        **
        ** A data transfer is going to the EEPROM iff (addr & eeprom.mask) == eeprom.range.
        */

        if (gba->memory.rom_size > 16 * 1024 * 1024) {
            gba->memory.eeprom.mask = 0x01FFFF00;
            gba->memory.eeprom.range = 0x01FFFF00;
        } else {
            gba->memory.eeprom.mask = 0xFF000000;
            gba->memory.eeprom.range = 0x0d000000;
        }

        if (gba->memory.backup_storage_type == BACKUP_EEPROM_4K) {
            gba->memory.eeprom.address_mask = EEPROM_4K_ADDR_MASK;
            gba->memory.eeprom.address_len = EEPROM_4K_ADDR_LEN;
        } else { // EEPROM_64K
            gba->memory.eeprom.address_mask = EEPROM_64K_ADDR_MASK;
            gba->memory.eeprom.address_len = EEPROM_64K_ADDR_LEN;
        }
    }

    if (gba->memory.backup_storage_type > BACKUP_NONE) {
        gba->memory.backup_storage_data = calloc(1, backup_storage_sizes[gba->memory.backup_storage_type]);
        hs_assert(gba->memory.backup_storage_data);
    } else {
        gba->memory.backup_storage_data = NULL;
    }
}

uint8_t
mem_backup_storage_read8(
    struct gba const *gba,
    uint32_t addr
) {
    switch (gba->memory.backup_storage_type) {
        case BACKUP_FLASH64:
        case BACKUP_FLASH128:
            return (mem_flash_read8(gba, addr));
            break;
        case BACKUP_SRAM:
            return (gba->memory.backup_storage_data[addr & SRAM_MASK]);
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
    switch (gba->memory.backup_storage_type) {
        case BACKUP_FLASH64:
        case BACKUP_FLASH128:
            mem_flash_write8(gba, addr, val);
            break;
        case BACKUP_SRAM:
            gba->memory.backup_storage_data[addr & SRAM_MASK] = val;
            gba->memory.backup_storage_dirty = true;
            break;
        default:
            break;
    }
}
