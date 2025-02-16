/*
 * QEMU MDPP PRCI (Power, Reset, Clock, Interrupt)
 *
 * Copyright (c) 2019 Bin Meng <bmeng.cn@gmail.com>
 *
 * Simple model of the PRCI to emulate register reads made by the SDK BSP
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

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "hw/misc/mdpp_prci.h"

static uint64_t mdpp_prci_read(void *opaque, hwaddr addr, unsigned int size){
    MDPPPRCIState *s = opaque;

    switch (addr) {
        case MDPP_PRCI_HFXOSCCFG:       return s->hfxosccfg;
        case MDPP_PRCI_COREPLLCFG0:     return s->corepllcfg0;
        case MDPP_PRCI_DDRPLLCFG0:      return s->ddrpllcfg0;
        case MDPP_PRCI_DDRPLLCFG1:      return s->ddrpllcfg1;
        case MDPP_PRCI_GEMGXLPLLCFG0:   return s->gemgxlpllcfg0;
        case MDPP_PRCI_GEMGXLPLLCFG1:   return s->gemgxlpllcfg1;
        case MDPP_PRCI_CORECLKSEL:      return s->coreclksel;
        case MDPP_PRCI_DEVICESRESET:    return s->devicesreset;
        case MDPP_PRCI_CLKMUXSTATUS:    return s->clkmuxstatus;
    }

    qemu_log_mask(LOG_GUEST_ERROR, "%s: read: addr=0x%" HWADDR_PRIx "\n", __func__, addr);
    return 0;
}

static void mdpp_prci_write(void *opaque, hwaddr addr, uint64_t val64, unsigned int size){
    MDPPPRCIState *s = opaque;
    uint32_t val32 = (uint32_t)val64;

    switch (addr) {
    case MDPP_PRCI_HFXOSCCFG:
        s->hfxosccfg = val32;
        /* OSC stays ready */
        s->hfxosccfg |= MDPP_PRCI_HFXOSCCFG_RDY;
        break;
    case MDPP_PRCI_COREPLLCFG0:
        s->corepllcfg0 = val32;
        /* internal feedback */
        s->corepllcfg0 |= MDPP_PRCI_PLLCFG0_FSE;
        /* PLL stays locked */
        s->corepllcfg0 |= MDPP_PRCI_PLLCFG0_LOCK;
        break;
    case MDPP_PRCI_DDRPLLCFG0:
        s->ddrpllcfg0 = val32;
        /* internal feedback */
        s->ddrpllcfg0 |= MDPP_PRCI_PLLCFG0_FSE;
        /* PLL stays locked */
        s->ddrpllcfg0 |= MDPP_PRCI_PLLCFG0_LOCK;
        break;
    case MDPP_PRCI_DDRPLLCFG1:
        s->ddrpllcfg1 = val32;
        break;
    case MDPP_PRCI_GEMGXLPLLCFG0:
        s->gemgxlpllcfg0 = val32;
        /* internal feedback */
        s->gemgxlpllcfg0 |= MDPP_PRCI_PLLCFG0_FSE;
        /* PLL stays locked */
        s->gemgxlpllcfg0 |= MDPP_PRCI_PLLCFG0_LOCK;
        break;
    case MDPP_PRCI_GEMGXLPLLCFG1:
        s->gemgxlpllcfg1 = val32;
        break;
    case MDPP_PRCI_CORECLKSEL:
        s->coreclksel = val32;
        break;
    case MDPP_PRCI_DEVICESRESET:
        s->devicesreset = val32;
        break;
    case MDPP_PRCI_CLKMUXSTATUS:
        s->clkmuxstatus = val32;
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: bad write: addr=0x%" HWADDR_PRIx " v=0x%x\n", __func__, addr, val32);
    }
}

static const MemoryRegionOps mdpp_prci_ops = {
    .read = mdpp_prci_read,
    .write = mdpp_prci_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4
    }
};

static void mdpp_prci_realize(DeviceState *dev, Error **errp){
    MDPPPRCIState *s = MDPP_PRCI(dev);

    memory_region_init_io(&s->mmio, OBJECT(dev), &mdpp_prci_ops, s,
                          TYPE_MDPP_PRCI, MDPP_PRCI_REG_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->mmio);
}

static void mdpp_prci_reset(DeviceState *dev){
    MDPPPRCIState *s = MDPP_PRCI(dev);

    /* Initialize register to power-on-reset values */
    s->hfxosccfg = MDPP_PRCI_HFXOSCCFG_RDY | MDPP_PRCI_HFXOSCCFG_EN;
    s->corepllcfg0 = MDPP_PRCI_PLLCFG0_DIVR | MDPP_PRCI_PLLCFG0_DIVF |
                     MDPP_PRCI_PLLCFG0_DIVQ | MDPP_PRCI_PLLCFG0_FSE |
                     MDPP_PRCI_PLLCFG0_LOCK;
    s->ddrpllcfg0 = MDPP_PRCI_PLLCFG0_DIVR | MDPP_PRCI_PLLCFG0_DIVF |
                    MDPP_PRCI_PLLCFG0_DIVQ | MDPP_PRCI_PLLCFG0_FSE |
                    MDPP_PRCI_PLLCFG0_LOCK;
    s->gemgxlpllcfg0 = MDPP_PRCI_PLLCFG0_DIVR | MDPP_PRCI_PLLCFG0_DIVF |
                       MDPP_PRCI_PLLCFG0_DIVQ | MDPP_PRCI_PLLCFG0_FSE |
                       MDPP_PRCI_PLLCFG0_LOCK;
    s->coreclksel = MDPP_PRCI_CORECLKSEL_HFCLK;
}

static void mdpp_prci_class_init(ObjectClass *klass, void *data){
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = mdpp_prci_realize;
    device_class_set_legacy_reset(dc, mdpp_prci_reset);
}

static const TypeInfo mdpp_prci_info = {
    .name          = TYPE_MDPP_PRCI,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(MDPPPRCIState),
    .class_init    = mdpp_prci_class_init,
};

static void mdpp_prci_register_types(void){
    type_register_static(&mdpp_prci_info);
}

type_init(mdpp_prci_register_types)
