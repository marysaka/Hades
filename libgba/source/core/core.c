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
**   * ARM7TDMI-S Data Sheet
**      https://www.dwedit.org/files/ARM7TDMI.pdf
**   * GBATEK
**      https://problemkaputt.de/gbatek.htm
*/

#include <string.h>
#include "hades.h"
#include "memory.h"
#include "core.h"
#include "core/arm.h"
#include "core/thumb.h"
#include "gba.h"

/*
** Fetch, decode and execute the next instruction.
*/
void
core_step(
    struct gba *gba
) {
    struct core *core;

    core = &gba->core;

    /*
    ** Ensure all the conditions to trigger any IRQ are met, and if they are, fire that interrupt.
    **
    ** To trigger any interrupt, we need:
    **   * The CPSR.I flag set to 0
    **   * The bit 0 of the IME IO register set to 1
    **   * That interrupt enabled in both REG_IE and REG_IF
    */
    if (gba->io.int_enabled.raw & gba->io.int_flag.raw) {
        switch (gba->core.state) {
            case CORE_RUN: {
                if (
                       !gba->core.cpsr.irq_disable
                    && (gba->io.ime.raw & 0b1)
                ) {
                    logln(HS_IRQ, "Received new IRQ: 0x%04x.", gba->io.int_enabled.raw & gba->io.int_flag.raw);
                    core_interrupt(gba, VEC_IRQ, MODE_IRQ);
                }
                break;
            };
            case CORE_HALT: {
                gba->core.state = CORE_RUN;
                break;
            };
            case CORE_STOP: {
                if (gba->io.int_flag.keypad) {
                    gba->core.state = CORE_RUN;
                }
                break;
            };
        }
    }

    if (likely(core->state == CORE_RUN)) {
        if (core->cpsr.thumb) {
            uint16_t op;

            op = core->prefetch[0];
            core->prefetch[0] = core->prefetch[1];
            core->prefetch[1] = mem_read16(gba, core->pc, core->prefetch_access_type);

            if (unlikely(thumb_lut[op >> 8] == NULL)) {
                panic(HS_CORE, "Unknown Thumb op-code 0x%04x (pc=0x%08x).", op, core->pc);
            }

            thumb_lut[op >> 8](gba, op);
        } else {
            size_t idx;
            uint32_t op;

            op = core->prefetch[0];
            core->prefetch[0] = core->prefetch[1];
            core->prefetch[1] = mem_read32(gba, core->pc, core->prefetch_access_type);

            /*
            ** Test if the conditions required to execute the instruction are met
            ** Ignore instructions where the conditions aren't met.
            */

            idx = (bitfield_get_range(core->cpsr.raw, 28, 32) << 4) | (bitfield_get_range(op, 28, 32));
            if (unlikely(!cond_lut[idx])) {
                core->pc += 4;
                core->prefetch_access_type = SEQUENTIAL;
                goto end;
            }

            idx = ((op >> 16) & 0xFF0) | ((op >> 4) & 0x00F);

            if (unlikely(arm_lut[idx] == NULL)) {
                panic(HS_CORE, "Unknown ARM op-code 0x%08x (pc=0x%08x).", op, core->pc);
            }

            arm_lut[idx](gba, op);
        }
    } else if (core->state == CORE_HALT) {
        core_idle(gba);
    }

end:
#ifdef WITH_DEBUGGER
    debugger_eval_breakpoints(gba);
#else
    (void)0;
#endif
}

void
core_idle(
    struct gba *gba
) {
    core_idle_for(gba, 1);
}

void
core_idle_for(
    struct gba *gba,
    uint32_t cycles
) {
    /*
    ** As far as I understand, DMA can start as soon as the CPU is idling after their two cycles startup delay.
    */
    if (gba->core.pending_dma && !gba->core.is_dma_running) {
        mem_dma_do_all_pending_transfers(gba);
    }

    gba->scheduler.cycles += cycles;

    /*
    ** Disable prefetchng during DMA.
    **
    ** According to Fleroviux (https://github.com/fleroviux/) this
    ** leads to better accuracy but the reasons why aren't well known yet.
    */
    if (gba->memory.pbuffer.enabled && !gba->memory.gamepak_bus_in_use && !gba->core.is_dma_running) {
        mem_prefetch_buffer_step(gba, cycles);
    }

    if (unlikely(gba->scheduler.cycles >= gba->scheduler.next_event)) {
        sched_process_events(gba);
    }
}

/*
** Reload the cached op-code on the 3-stage pipeline.
** This must be called when the value of PC is changed.
**
** This function takes 1N and 1S cycles to complete, and set the
** next prefetch access type to SEQUENTIAL.
*/
void
core_reload_pipeline(
    struct gba *gba
) {
    struct core *core;

    core = &gba->core;
    if (core->cpsr.thumb) {
        core->pc &= 0xFFFFFFFE;
        core->prefetch[0] = mem_read16(gba, core->pc, NON_SEQUENTIAL);
        core->pc += 2;
        core->prefetch[1] = mem_read16(gba, core->pc, SEQUENTIAL);
        core->pc += 2;
    } else {
        core->pc &= 0xFFFFFFFC;
        core->prefetch[0] = mem_read32(gba, core->pc, NON_SEQUENTIAL);
        core->pc += 4;
        core->prefetch[1] = mem_read32(gba, core->pc, SEQUENTIAL);
        core->pc += 4;
    }
    core->prefetch_access_type = SEQUENTIAL;
}

/*
** Get the SPSR of the given mode.
*/
struct psr
core_spsr_get(
    struct core const *core,
    enum arm_modes mode
) {
    switch (mode) {
        case MODE_USR:
        case MODE_SYS:
            return (core->cpsr);
        case MODE_FIQ:
            return (core->spsr_fiq);
        case MODE_IRQ:
            return (core->spsr_irq);
        case MODE_SVC:
            return (core->spsr_svc);
        case MODE_ABT:
            return (core->spsr_abt);
        case MODE_UND:
            return (core->spsr_und);
        default:
            panic(HS_CORE, "core_spsr_get(): unsupported mode (%u)", mode);
            break;
    }
}

/*
** Set the SPSR of the given mode to the given value.
*/
void
core_spsr_set(
    struct core *core,
    enum arm_modes mode,
    struct psr psr
) {
    switch (mode) {
        case MODE_USR:
        case MODE_SYS:
            core->cpsr.raw = psr.raw;
            break;
        case MODE_FIQ:
            core->spsr_fiq.raw = psr.raw;
            break;
        case MODE_IRQ:
            core->spsr_irq.raw = psr.raw;
            break;
        case MODE_SVC:
            core->spsr_svc.raw = psr.raw;
            break;
        case MODE_ABT:
            core->spsr_abt.raw = psr.raw;
            break;
        case MODE_UND:
            core->spsr_und.raw = psr.raw;
            break;
        default:
            panic(HS_CORE, "core_spsr_set(): unsupported mode (%u)", mode);
            break;
    }
}

/*
** Switch from the current mode to the given one.
**
** In practice, this function saves the content of the registers
** to the current mode's bank and replace their value with the
** ones from the new mode's bank. It also sets the CPSR's mode bits\
** to the given mode.
**
** No SPSRs are updated.
*/
void
core_switch_mode(
    struct core *core,
    enum arm_modes mode
) {
    if (mode != core->cpsr.mode) {

        logln(
            HS_CORE,
            "Switching from %s to %s mode.",
            arm_modes_name[core->cpsr.mode],
            arm_modes_name[mode]
        );

        /* Save current registers to their bank */
        switch (core->cpsr.mode) {
            case MODE_SYS:
            case MODE_USR:
                core->r8_sys = core->r8;
                core->r9_sys = core->r9;
                core->r10_sys = core->r10;
                core->r11_sys = core->fp;
                core->r12_sys = core->ip;
                core->r13_sys = core->sp;
                core->r14_sys = core->lr;
                break;
            case MODE_FIQ:
                core->r8_fiq = core->r8;
                core->r9_fiq = core->r9;
                core->r10_fiq = core->r10;
                core->r11_fiq = core->fp;
                core->r12_fiq = core->ip;
                core->r13_fiq = core->sp;
                core->r14_fiq = core->lr;
                break;
            case MODE_IRQ:
                core->r8_sys = core->r8;
                core->r9_sys = core->r9;
                core->r10_sys = core->r10;
                core->r11_sys = core->fp;
                core->r12_sys = core->ip;
                core->r13_irq = core->sp;
                core->r14_irq = core->lr;
                break;
            case MODE_SVC:
                core->r8_sys = core->r8;
                core->r9_sys = core->r9;
                core->r10_sys = core->r10;
                core->r11_sys = core->fp;
                core->r12_sys = core->ip;
                core->r13_svc = core->sp;
                core->r14_svc = core->lr;
                break;
            case MODE_ABT:
                core->r8_sys = core->r8;
                core->r9_sys = core->r9;
                core->r10_sys = core->r10;
                core->r11_sys = core->fp;
                core->r12_sys = core->ip;
                core->r13_abt = core->sp;
                core->r14_abt = core->lr;
                break;
            case MODE_UND:
                core->r8_sys = core->r8;
                core->r9_sys = core->r9;
                core->r10_sys = core->r10;
                core->r11_sys = core->fp;
                core->r12_sys = core->ip;
                core->r13_und = core->sp;
                core->r14_und = core->lr;
                break;
            default:
                panic(HS_CORE, "core_switch_mode(): unsupported mode (%u)", core->cpsr.mode);
                break;
        }

        core->cpsr.mode = mode;

        /* Restore the registers based on the bank's content */
        switch (mode) {
            case MODE_SYS:
            case MODE_USR:
                core->r8 = core->r8_sys;
                core->r9 = core->r9_sys;
                core->r10 = core->r10_sys;
                core->fp = core->r11_sys;
                core->ip = core->r12_sys;
                core->sp = core->r13_sys;
                core->lr = core->r14_sys;
                break;
            case MODE_FIQ:
                core->r8 = core->r8_fiq;
                core->r9 = core->r9_fiq;
                core->r10 = core->r10_fiq;
                core->fp = core->r11_fiq;
                core->ip = core->r12_fiq;
                core->sp = core->r13_fiq;
                core->lr = core->r14_fiq;
                break;
            case MODE_IRQ:
                core->r8 = core->r8_sys;
                core->r9 = core->r9_sys;
                core->r10 = core->r10_sys;
                core->fp = core->r11_sys;
                core->ip = core->r12_sys;
                core->sp = core->r13_irq;
                core->lr = core->r14_irq;
                break;
            case MODE_SVC:
                core->r8 = core->r8_sys;
                core->r9 = core->r9_sys;
                core->r10 = core->r10_sys;
                core->fp = core->r11_sys;
                core->ip = core->r12_sys;
                core->sp = core->r13_svc;
                core->lr = core->r14_svc;
                break;
            case MODE_ABT:
                core->r8 = core->r8_sys;
                core->r9 = core->r9_sys;
                core->r10 = core->r10_sys;
                core->fp = core->r11_sys;
                core->ip = core->r12_sys;
                core->sp = core->r13_abt;
                core->lr = core->r14_abt;
                break;
            case MODE_UND:
                core->r8 = core->r8_sys;
                core->r9 = core->r9_sys;
                core->r10 = core->r10_sys;
                core->fp = core->r11_sys;
                core->ip = core->r12_sys;
                core->sp = core->r13_und;
                core->lr = core->r14_und;
                break;
            default:
                panic(HS_CORE, "core_switch_mode(): unsupported mode (%u)", mode);
                break;
        }
    }
}

/*
** Interrupt the CPU, switching to the given interrupt vector/mode.
*/
void
core_interrupt(
    struct gba *gba,
    enum arm_vectors vector,
    enum arm_modes mode
) {
    struct core *core;
    struct psr cpsr;

    core = &gba->core;

    cpsr = core->cpsr;
    core_switch_mode(core, mode);
    core_spsr_set(core, mode, cpsr);

    if (vector == VEC_SVC || vector == VEC_UND) {
        core->lr = core->pc - (core->cpsr.thumb ? 2 : 4);
    } else if (vector != VEC_RESET) {
        core->lr = core->pc - (core->cpsr.thumb ? 0 : 4);
    }

    core->pc = vector;
    core->cpsr.irq_disable = true;
    core->cpsr.thumb = false;

    core_reload_pipeline(gba);
}

/*
** Compute the operand of an instruction that uses an encoded shift register.
** If `carry` is not NULL, this will set the value pointed by `carry` to the
** shifter carry output.
*/
uint32_t
core_compute_shift(
    struct core *core,
    uint32_t encoded_shift,
    uint32_t value,
    bool *carry
){
    uint32_t type;
    uint32_t bits;
    bool carry_out;

    /*
    ** The first bit tells us if the amount of bits to shift is either stored as
    ** an immediate value or within a register.
    */
    if (bitfield_get(encoded_shift, 0)) {   // Register
        uint32_t rs;

        rs = (encoded_shift >> 4) & 0xF;
        bits = core->registers[rs] & 0xFF;

        /*
        ** The spec requires a bit of error handling regarding register
        ** specified shift amount.
        */

        if (bits == 0) {
            return (value);
        }
    } else {                                // Immediate value
        bits = (encoded_shift >> 3) & 0x1F;
    }

    type = (encoded_shift >> 1) & 0b11;
    carry_out = false;

    /*
    ** There's four kind of shifts: logical left, logicial right, arithmetic
    ** right and rotate right.
    */
    switch (type) {
        // Logical left
        case 0:
            /*
            ** If LSL#0 then the carry bit is the old content of the CPSR C flag
            ** and the value is left untouched.
            ** LSL by 32 has result zero, carry out equal to bit 0 of Rm.
            ** LSL by more than 32 has result zero, carry out zero.
            */
            if (bits == 0) {
                carry_out = core->cpsr.carry;
            } else if (bits <= 32) {
                value <<= bits - 1;
                carry_out = (value >> 31) & 0b1;                    // Save the carry
                value <<= 1;
            } else {
                value = 0;
                carry_out = false;
            }
            break;
        // Logical right
        case 1:
            // LSR#0 is used to encode LSR#32
            if (bits <= 32) {
                if (bits == 0) {
                    bits = 32;
                }
                value >>= bits - 1;
                carry_out = value & 0b1;                        // Save the carry
                value >>= 1;
            } else {
                value = 0;
                carry_out = false;
            }
            break;
        // Arithmetic right
        case 2:
            // ASR#0 is used to encode ASR#32
            if (bits == 0 || bits > 32) {
                bits = 32;
            }
            value = (int32_t)value >> (bits - 1);
            carry_out = value & 0b1;                        // Save the carry
            value = (int32_t)value >> 1;
            break;
        // Rotate right
        case 3:

            /*
            ** ROR by n where n is greater than 32 will give the same result and carry out
            ** as ROR by n-32; therefore repeatedly subtract 32 from n until the amount is
            ** in the range 1 to 32 and see above
            */
            if (bits > 32) {
                bits = ((bits - 1) % 32) + 1;
            }

            // ROR#0 is used to encode RRX
            if (bits == 0) {
                carry_out = value & 0b1;
                value >>= 1;
                value |= core->cpsr.carry << 31;
            } else {
                carry_out = (value >> (bits - 1)) & 0b1;    // Save the carry
                value = ror32(value, bits & 0x1F);
            }
            break;
    }

    if (carry) {
        *carry = carry_out;
    }

    return (value);
}
