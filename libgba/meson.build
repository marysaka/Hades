################################################################################
##
##  This file is part of the Hades GBA Emulator, and is made available under
##  the terms of the GNU General Public License version 2.
##
##  Copyright (C) 2021-2023 - The Hades Authors
##
################################################################################

project(
    'Hades',
    'c',
    version: '0.0.1',
    license: 'GPL-2.0-only',
    default_options: ['c_std=gnu17', 'buildtype=release'],
)

cflags = [
    '-fms-extensions',
    '-Wno-microsoft-anon-tag',
    '-Werror',
    # '-fsanitize=undefined',
]

ldflags = [
    # '-fsanitize=undefined',
]

if get_option('with_debugger')
    cflags += ['-DWITH_DEBUGGER']
    ldflags += ['-DWITH_DEBUGGER']
endif

static_library(
    'gba',
    'source/apu/apu.c',
    'source/apu/wave.c',
    'source/core/arm/alu.c',
    'source/core/arm/bdt.c',
    'source/core/arm/branch.c',
    'source/core/arm/sdt.c',
    'source/core/arm/core.c',
    'source/core/arm/mul.c',
    'source/core/arm/psr.c',
    'source/core/arm/swi.c',
    'source/core/arm/swp.c',
    'source/core/thumb/alu.c',
    'source/core/thumb/bdt.c',
    'source/core/thumb/branch.c',
    'source/core/thumb/core.c',
    'source/core/thumb/logical.c',
    'source/core/thumb/sdt.c',
    'source/core/thumb/swi.c',
    'source/core/core.c',
    'source/gpio/gpio.c',
    'source/gpio/rtc.c',
    'source/memory/storage/eeprom.c',
    'source/memory/storage/flash.c',
    'source/memory/storage/storage.c',
    'source/memory/dma.c',
    'source/memory/io.c',
    'source/memory/memory.c',
    'source/ppu/background/affine.c',
    'source/ppu/background/bitmap.c',
    'source/ppu/background/text.c',
    'source/ppu/oam.c',
    'source/ppu/ppu.c',
    'source/ppu/window.c',
    'source/channel.c',
    'source/gba.c',
    'source/log.c',
    'source/scheduler.c',
    'source/timer.c',
    include_directories: include_directories('include'),
    c_args: cflags,
    link_args: ldflags,
)
