/*
 * Copyright (C) 2010-2012 Guan Xuetao
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Contributions from 2012-04-01 on are considered under GPL version 2,
 * or (at your option) any later version.
 */

#include "cpu.h"
#include "exec/gdbstub.h"
#include "helper.h"
#include "qemu/host-utils.h"
#ifndef CONFIG_USER_ONLY
#include "ui/console.h"
#endif

#undef DEBUG_UC32

#ifdef DEBUG_UC32
#define DPRINTF(fmt, ...) printf("%s: " fmt , __func__, ## __VA_ARGS__)
#else
#define DPRINTF(fmt, ...) do {} while (0)
#endif

CPUUniCore32State *uc32_cpu_init(const char *cpu_model)
{
    UniCore32CPU *cpu;
    CPUUniCore32State *env;
    ObjectClass *oc;
    static int inited = 1;

    oc = cpu_class_by_name(TYPE_UNICORE32_CPU, cpu_model);
    if (oc == NULL) {
        return NULL;
    }
    cpu = UNICORE32_CPU(object_new(object_class_get_name(oc)));
    env = &cpu->env;

    if (inited) {
        inited = 0;
        uc32_translate_init();
    }

    qemu_init_vcpu(env);
    return env;
}

uint32_t HELPER(clo)(uint32_t x)
{
    return clo32(x);
}

uint32_t HELPER(clz)(uint32_t x)
{
    return clz32(x);
}

#ifndef CONFIG_USER_ONLY
void helper_cp0_set(CPUUniCore32State *env, uint32_t val, uint32_t creg,
        uint32_t cop)
{
    /*
     * movc pp.nn, rn, #imm9
     *      rn: UCOP_REG_D
     *      nn: UCOP_REG_N
     *          1: sys control reg.
     *          2: page table base reg.
     *          3: data fault status reg.
     *          4: insn fault status reg.
     *          5: cache op. reg.
     *          6: tlb op. reg.
     *      imm9: split UCOP_IMM10 with bit5 is 0
     */
    switch (creg) {
    case 1:
        if (cop != 0) {
            goto unrecognized;
        }
        env->cp0.c1_sys = val;
        break;
    case 2:
        if (cop != 0) {
            goto unrecognized;
        }
        env->cp0.c2_base = val;
        break;
    case 3:
        if (cop != 0) {
            goto unrecognized;
        }
        env->cp0.c3_faultstatus = val;
        break;
    case 4:
        if (cop != 0) {
            goto unrecognized;
        }
        env->cp0.c4_faultaddr = val;
        break;
    case 5:
        switch (cop) {
        case 28:
            DPRINTF("Invalidate Entire I&D cache\n");
            return;
        case 20:
            DPRINTF("Invalidate Entire Icache\n");
            return;
        case 12:
            DPRINTF("Invalidate Entire Dcache\n");
            return;
        case 10:
            DPRINTF("Clean Entire Dcache\n");
            return;
        case 14:
            DPRINTF("Flush Entire Dcache\n");
            return;
        case 13:
            DPRINTF("Invalidate Dcache line\n");
            return;
        case 11:
            DPRINTF("Clean Dcache line\n");
            return;
        case 15:
            DPRINTF("Flush Dcache line\n");
            return;
        }
        break;
    case 6:
        if ((cop <= 6) && (cop >= 2)) {
            /* invalid all tlb */
            tlb_flush(env, 1);
            return;
        }
        break;
    default:
        goto unrecognized;
    }
    return;
unrecognized:
    DPRINTF("Wrong register (%d) or wrong operation (%d) in cp0_set!\n",
            creg, cop);
}

uint32_t helper_cp0_get(CPUUniCore32State *env, uint32_t creg, uint32_t cop)
{
    /*
     * movc rd, pp.nn, #imm9
     *      rd: UCOP_REG_D
     *      nn: UCOP_REG_N
     *          0: cpuid and cachetype
     *          1: sys control reg.
     *          2: page table base reg.
     *          3: data fault status reg.
     *          4: insn fault status reg.
     *      imm9: split UCOP_IMM10 with bit5 is 0
     */
    switch (creg) {
    case 0:
        switch (cop) {
        case 0:
            return env->cp0.c0_cpuid;
        case 1:
            return env->cp0.c0_cachetype;
        }
        break;
    case 1:
        if (cop == 0) {
            return env->cp0.c1_sys;
        }
        break;
    case 2:
        if (cop == 0) {
            return env->cp0.c2_base;
        }
        break;
    case 3:
        if (cop == 0) {
            return env->cp0.c3_faultstatus;
        }
        break;
    case 4:
        if (cop == 0) {
            return env->cp0.c4_faultaddr;
        }
        break;
    }
    DPRINTF("Wrong register (%d) or wrong operation (%d) in cp0_set!\n",
            creg, cop);
    return 0;
}

#ifdef CONFIG_CURSES
/*
 * FIXME:
 *     1. curses windows will be blank when switching back
 *     2. backspace is not handled yet
 */
static void putc_on_screen(unsigned char ch)
{
    static WINDOW *localwin;
    static int init;

    if (!init) {
        /* Assume 80 * 30 screen to minimize the implementation */
        localwin = newwin(30, 80, 0, 0);
        scrollok(localwin, TRUE);
        init = TRUE;
    }

    if (isprint(ch)) {
        wprintw(localwin, "%c", ch);
    } else {
        switch (ch) {
        case '\n':
            wprintw(localwin, "%c", ch);
            break;
        case '\r':
            /* If '\r' is put before '\n', the curses window will destroy the
             * last print line. And meanwhile, '\n' implifies '\r' inside. */
            break;
        default: /* Not handled, so just print it hex code */
            wprintw(localwin, "-- 0x%x --", ch);
        }
    }

    wrefresh(localwin);
}
#else
#define putc_on_screen(c)               do { } while (0)
#endif

void helper_cp1_putc(target_ulong x)
{
    putc_on_screen((unsigned char)x);   /* Output to screen */
    DPRINTF("%c", x);                   /* Output to stdout */
}
#endif

#ifdef CONFIG_USER_ONLY
void switch_mode(CPUUniCore32State *env, int mode)
{
    if (mode != ASR_MODE_USER) {
        cpu_abort(env, "Tried to switch out of user mode\n");
    }
}

void do_interrupt(CPUUniCore32State *env)
{
    cpu_abort(env, "NO interrupt in user mode\n");
}

int uc32_cpu_handle_mmu_fault(CPUUniCore32State *env, target_ulong address,
                              int access_type, int mmu_idx)
{
    cpu_abort(env, "NO mmu fault in user mode\n");
    return 1;
}
#endif
