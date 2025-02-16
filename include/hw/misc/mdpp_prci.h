/*
 * QEMU MDPP PRCI (Power, Reset, Clock, Interrupt) interface
 *
 * Copyright (c) 2019 Bin Meng <bmeng.cn@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HW_MDPP_PRCI_H
#define HW_MDPP_PRCI_H

#include "hw/sysbus.h"

#define MDPP_PRCI_HFXOSCCFG     0x00
#define MDPP_PRCI_COREPLLCFG0   0x04
#define MDPP_PRCI_DDRPLLCFG0    0x0C
#define MDPP_PRCI_DDRPLLCFG1    0x10
#define MDPP_PRCI_GEMGXLPLLCFG0 0x1C
#define MDPP_PRCI_GEMGXLPLLCFG1 0x20
#define MDPP_PRCI_CORECLKSEL    0x24
#define MDPP_PRCI_DEVICESRESET  0x28
#define MDPP_PRCI_CLKMUXSTATUS  0x2C

/*
 * Current FU540-C000 manual says ready bit is at bit 29, but
 * freedom-u540-c000-bootloader codes (ux00prci.h) says it is at bit 31.
 * We have to trust the actual code that works.
 *
 */

#define MDPP_PRCI_HFXOSCCFG_EN  (1 << 30)
#define MDPP_PRCI_HFXOSCCFG_RDY (1 << 31)

/* xxxPLLCFG0 register bits */
#define MDPP_PRCI_PLLCFG0_DIVR  (1 << 0)
#define MDPP_PRCI_PLLCFG0_DIVF  (31 << 6)
#define MDPP_PRCI_PLLCFG0_DIVQ  (3 << 15)
#define MDPP_PRCI_PLLCFG0_FSE   (1 << 25)
#define MDPP_PRCI_PLLCFG0_LOCK  (1 << 31)

/* xxxPLLCFG1 register bits */
#define MDPP_PRCI_PLLCFG1_CKE   (1 << 24)

/* coreclksel register bits */
#define MDPP_PRCI_CORECLKSEL_HFCLK  (1 << 0)


#define MDPP_PRCI_REG_SIZE  0x1000

#define TYPE_MDPP_PRCI      "riscv.mdpp.prci"

typedef struct {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion mmio;
    uint32_t hfxosccfg;
    uint32_t corepllcfg0;
    uint32_t ddrpllcfg0;
    uint32_t ddrpllcfg1;
    uint32_t gemgxlpllcfg0;
    uint32_t gemgxlpllcfg1;
    uint32_t coreclksel;
    uint32_t devicesreset;
    uint32_t clkmuxstatus;
}MDPPPRCIState;

DECLARE_INSTANCE_CHECKER(MDPPPRCIState, MDPP_PRCI, TYPE_MDPP_PRCI)

/*
 * Clock indexes for use by Device Tree data and the PRCI driver.
 *
 * These values are from mdpp-fu540-prci.h in the Linux kernel.
 */
#define PRCI_CLK_COREPLL        0
#define PRCI_CLK_DDRPLL         1
#define PRCI_CLK_GEMGXLPLL      2
#define PRCI_CLK_TLCLK          3

#endif /* HW_MDPP_PRCI_H */
