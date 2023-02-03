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
    'source/channel.c',
    'source/gba.c',
    'source/log.c',
    include_directories: include_directories('include'),
    c_args: cflags,
    link_args: ldflags,
)
