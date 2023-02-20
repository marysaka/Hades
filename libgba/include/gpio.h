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

enum gpio_regs {
    GPIO_REG_START          = 0x80000C4,

    GPIO_REG_DATA           = 0x80000C4,
    GPIO_REG_DIRECTION      = 0x80000C6,
    GPIO_REG_CTRL           = 0x80000C8,

    GPIO_REG_END            = 0x80000C8,
};

enum rtc_states {
    RTC_COMMAND = 0,
    RTC_REG_RECV,
    RTC_REG_SEND,
};

enum rtc_registers {
    RTC_REG_RESET       = 0,
    RTC_REG_CONTROL     = 1,
    RTC_REG_DATE_TIME   = 2,
    RTC_REG_TIME        = 3,
    RTC_REG_IRQ         = 4,
};

struct rtc {
    bool enabled;

    enum rtc_states state;

    uint64_t data;
    uint8_t data_count;
    uint8_t data_len;

    bool sck;
    bool sio;
    bool cs;

    enum rtc_registers active_register;

    union {
        struct {
            uint8_t : 3;
            uint8_t irq: 1;
            uint8_t : 2;
            uint8_t mode_24h: 1;
            uint8_t poweroff: 1;
        } __packed;
        uint8_t raw;
    } control;
};

struct gpio {
    bool readable;

    struct rtc rtc;
};

struct gba;

/* source/gpio/gpio.c */
void gpio_init(struct gba *);
uint8_t gpio_read_u8(struct gba *gba, uint32_t);
void gpio_write_u8(struct gba *gba, uint32_t, uint8_t);

/* source/gpio/rtc.c */
void gpio_rtc_init(struct gba *);
uint8_t gpio_rtc_read(struct gba *gba);
void gpio_rtc_write(struct gba *gba, uint8_t);
