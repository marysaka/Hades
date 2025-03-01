/******************************************************************************\
**
**  This file is part of the Hades GBA Emulator, and is made available under
**  the terms of the GNU General Public License version 2.
**
**  Copyright (C) 2021-2023 - The Hades Authors
**
\******************************************************************************/

#include "hades.h"
#include "gba/gba.h"

void
core_arm_swi(
    struct gba *gba,
    uint32_t op
) {
    core_interrupt(gba, VEC_SVC, MODE_SVC);
}
