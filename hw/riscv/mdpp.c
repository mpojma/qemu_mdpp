/*
 * QEMU RISC-V Board Compatible with MDPP Freedom U SDK
 *
 * Copyright (c) 2016-2017 Sagar Karandikar, sagark@eecs.berkeley.edu
 * Copyright (c) 2017 MDPP, Inc.
 * Copyright (c) 2019 Bin Meng <bmeng.cn@gmail.com>
 *
 * Provides a board compatible with the MDPP Freedom U SDK:
 *
 * 0) UART
 * 1) CLINT (Core Level Interruptor)
 * 2) PLIC (Platform Level Interrupt Controller)
 * 3) PRCI (Power, Reset, Clock, Interrupt)
 * 4) GPIO (General Purpose Input/Output Controller)
 * 7) DMA (Direct Memory Access Controller)
 *
 * This board currently generates devicetree dynamically that indicates at least
 * two harts and up to five harts.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License, * version 2 or later, as published by the Free Software Foundation.
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
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "qapi/visitor.h"
#include "hw/boards.h"
#include "hw/irq.h"
#include "hw/loader.h"
#include "hw/sysbus.h"
#include "hw/cpu/cluster.h"
#include "hw/misc/unimp.h"
#include "hw/ssi/ssi.h"
#include "target/riscv/cpu.h"
#include "hw/riscv/riscv_hart.h"
#include "hw/riscv/mdpp.h"
#include "hw/riscv/boot.h"
#include "hw/char/mdpp_uart.h"
#include "hw/intc/riscv_aclint.h"
#include "hw/intc/mdpp_plic.h"
#include "chardev/char.h"
#include "system/device_tree.h"
#include "system/runstate.h"
#include "system/system.h"

#include <libfdt.h>

/* CLINT timebase frequency */
#define CLINT_TIMEBASE_FREQ 1000000

static const MemMapEntry mdpp_memmap[] = {  [MDPP_DEV_DEBUG]  = {       0x0,      0x100}, 
                                            [MDPP_DEV_MROM]   = {    0x1000,     0xf000}, 
                                            [MDPP_DEV_CLINT]  = { 0xe000000,    0x10000}, 
                                            [MDPP_DEV_PLIC]   = {0xf8000000,  0x4000000}, 
                                            [MDPP_DEV_UART0]  = {0xfc001100,      0x100},
                                            [MDPP_DEV_UART1]  = {0xfc001200,      0x100},
                                            [MDPP_DEV_UART2]  = {0xfc001300,      0x100},
                                            [MDPP_DEV_UART3]  = {0xfc001400,      0x100},
                                            [MDPP_DEV_UART4]  = {0xfc001500,      0x100},
                                            [MDPP_DEV_UART5]  = {0xfc001000,      0x100}, 
                                            [MDPP_DEV_GPIO0]  = {0x80500100,       0x10}, 
                                            [MDPP_DEV_GPIO1]  = {0x80500110,       0x10}, 
                                            [MDPP_DEV_CAN0]   = {0x80500120,       0x20}, 
                                            [MDPP_DEV_CAN1]   = {0x80500140,       0x20}, 
                                            [MDPP_DEV_OBT]    = {0x80500160,       0x1b}, 
                                            [MDPP_DEV_NVMEM0] = {0x80601a00,      0x100}, 
                                            [MDPP_DEV_NVMEM1] = {0x80601b00,      0x100}, 
                                            [MDPP_DEV_SRF]    = {0x80601c00,      0x100}, 
                                            [MDPP_DEV_LVDS0]  = {0x80602000,     0x1000}, 
                                            [MDPP_DEV_LVDS1]  = {0x80603000,     0x1000}, 
                                            [MDPP_DEV_DRAM]   = {       0x0,  0x8000000}, // 128 MB de RAM
};
static void add_fdt(int element, uint8_t countElem, char* elemBaseName, uint32_t plic_phandle, int irqVector[]){
    char compatible [20];
    for (int i = element,j=0; j < countElem; ++i,++j) {
        sprintf(compatible,"mdpp,%s%d",elemBaseName,i);
        char *nodename = g_strdup_printf("/soc/%s@%lx", elemBaseName,(long)memmap[i].base);
        qemu_fdt_add_subnode(fdt, nodename);
        qemu_fdt_setprop_string(fdt, nodename, "compatible", compatible);
        qemu_fdt_setprop_cells(fdt, nodename, "reg", 0x0, memmap[i].base, 0x0, memmap[i].size);
        qemu_fdt_setprop_cell(fdt, nodename, "interrupt-parent", plic_phandle);
        qemu_fdt_setprop_cell(fdt, nodename, "interrupts", irqVector[j]);
        g_free(nodename);
    }
}    
static void create_fdt(MDPPState *s, const MemMapEntry *memmap, bool is_32_bit){
    MachineState *ms = MACHINE(s);
    uint64_t mem_size = ms->ram_size;
    void *fdt;
    int cpu;
    uint32_t *cells;
    char *nodename;
    uint32_t plic_phandle, prci_phandle, gpio_phandle, phandle = 1;
    uint32_t hfclk_phandle, rtcclk_phandle, phy_phandle;
    
    char compatible[14], alias[8];
    static const char *const ethclk_names[2] = {"pclk", "hclk"};
    static const char *const clint_compat[2] = {"mdpp,clint0", "riscv,clint0"};
    static const char *const plic_compat[2] = {"mdpp,plic-1.0.0", "riscv,plic0"};

    fdt = ms->fdt = create_device_tree(&s->fdt_size);
    if (!fdt){
        error_report("create_device_tree() failed");
        exit(1);
    }

    qemu_fdt_setprop_string(fdt, "/", "model", "MDPP HiFive Unleashed A00");
    qemu_fdt_setprop_string(fdt, "/", "compatible", "mdpp,hifive-unleashed-a00");
    qemu_fdt_setprop_cell(fdt, "/", "#size-cells", 0x2);
    qemu_fdt_setprop_cell(fdt, "/", "#address-cells", 0x2);

    qemu_fdt_add_subnode(fdt, "/soc");
    qemu_fdt_setprop(fdt, "/soc", "ranges", NULL, 0);
    qemu_fdt_setprop_string(fdt, "/soc", "compatible", "simple-bus");
    qemu_fdt_setprop_cell(fdt, "/soc", "#size-cells", 0x2);
    qemu_fdt_setprop_cell(fdt, "/soc", "#address-cells", 0x2);

    hfclk_phandle = phandle++;
    nodename = g_strdup_printf("/hfclk");
    qemu_fdt_add_subnode(fdt, nodename);
    qemu_fdt_setprop_cell(fdt, nodename, "phandle", hfclk_phandle);
    qemu_fdt_setprop_string(fdt, nodename, "clock-output-names", "hfclk");
    qemu_fdt_setprop_cell(fdt, nodename, "clock-frequency", MDPP_HFCLK_FREQ);
    qemu_fdt_setprop_string(fdt, nodename, "compatible", "fixed-clock");
    qemu_fdt_setprop_cell(fdt, nodename, "#clock-cells", 0x0);
    g_free(nodename);

    rtcclk_phandle = phandle++;
    nodename = g_strdup_printf("/rtcclk");
    qemu_fdt_add_subnode(fdt, nodename);
    qemu_fdt_setprop_cell(fdt, nodename, "phandle", rtcclk_phandle);
    qemu_fdt_setprop_string(fdt, nodename, "clock-output-names", "rtcclk");
    qemu_fdt_setprop_cell(fdt, nodename, "clock-frequency", MDPP_RTCCLK_FREQ);
    qemu_fdt_setprop_string(fdt, nodename, "compatible", "fixed-clock");
    qemu_fdt_setprop_cell(fdt, nodename, "#clock-cells", 0x0);
    g_free(nodename);

    nodename = g_strdup_printf("/memory@%lx", (long)memmap[MDPP_DEV_DRAM].base);
    qemu_fdt_add_subnode(fdt, nodename);
    qemu_fdt_setprop_cells(fdt, nodename, "reg", memmap[MDPP_DEV_DRAM].base >> 32, memmap[MDPP_DEV_DRAM].base, mem_size >> 32, mem_size);
    qemu_fdt_setprop_string(fdt, nodename, "device_type", "memory");
    g_free(nodename);

    qemu_fdt_add_subnode(fdt, "/cpus");
    qemu_fdt_setprop_cell(fdt, "/cpus", "timebase-frequency", CLINT_TIMEBASE_FREQ);
    qemu_fdt_setprop_cell(fdt, "/cpus", "#size-cells", 0x0);
    qemu_fdt_setprop_cell(fdt, "/cpus", "#address-cells", 0x1);

    for (cpu = ms->smp.cpus - 1; cpu >= 0; cpu--){
        int cpu_phandle = phandle++;
        nodename = g_strdup_printf("/cpus/cpu@%d", cpu);
        char *intc = g_strdup_printf("/cpus/cpu@%d/interrupt-controller", cpu);
        qemu_fdt_add_subnode(fdt, nodename);
        /* cpu 0 is the management hart that does not have mmu */
        if (cpu != 0){
            if (is_32_bit){
                qemu_fdt_setprop_string(fdt, nodename, "mmu-type", "riscv,sv32");
            }
            else
            {
                qemu_fdt_setprop_string(fdt, nodename, "mmu-type", "riscv,sv48");
            }
            riscv_isa_write_fdt(&s->soc.u_cpus.harts[cpu - 1], fdt, nodename);
        }
        else
        {
            riscv_isa_write_fdt(&s->soc.e_cpus.harts[0], fdt, nodename);
        }
        qemu_fdt_setprop_string(fdt, nodename, "compatible", "riscv");
        qemu_fdt_setprop_string(fdt, nodename, "status", "okay");
        qemu_fdt_setprop_cell(fdt, nodename, "reg", cpu);
        qemu_fdt_setprop_string(fdt, nodename, "device_type", "cpu");
        qemu_fdt_add_subnode(fdt, intc);
        qemu_fdt_setprop_cell(fdt, intc, "phandle", cpu_phandle);
        qemu_fdt_setprop_string(fdt, intc, "compatible", "riscv,cpu-intc");
        qemu_fdt_setprop(fdt, intc, "interrupt-controller", NULL, 0);
        qemu_fdt_setprop_cell(fdt, intc, "#interrupt-cells", 1);
        g_free(intc);
        g_free(nodename);
    }

    cells = g_new0(uint32_t, ms->smp.cpus * 4);
    for (cpu = 0; cpu < ms->smp.cpus; cpu++){
        nodename =
            g_strdup_printf("/cpus/cpu@%d/interrupt-controller", cpu);
        uint32_t intc_phandle = qemu_fdt_get_phandle(fdt, nodename);
        cells[cpu * 4 + 0] = cpu_to_be32(intc_phandle);
        cells[cpu * 4 + 1] = cpu_to_be32(IRQ_M_SOFT);
        cells[cpu * 4 + 2] = cpu_to_be32(intc_phandle);
        cells[cpu * 4 + 3] = cpu_to_be32(IRQ_M_TIMER);
        g_free(nodename);
    }
    nodename = g_strdup_printf("/soc/clint@%lx", (long)memmap[MDPP_DEV_CLINT].base);
    qemu_fdt_add_subnode(fdt, nodename);
    qemu_fdt_setprop_string_array(fdt, nodename, "compatible", (char **)&clint_compat, ARRAY_SIZE(clint_compat));
    qemu_fdt_setprop_cells(fdt, nodename, "reg", 0x0, memmap[MDPP_DEV_CLINT].base, 0x0, memmap[MDPP_DEV_CLINT].size);
    qemu_fdt_setprop(fdt, nodename, "interrupts-extended", cells, ms->smp.cpus * sizeof(uint32_t) * 4);
    g_free(cells);
    g_free(nodename);

    prci_phandle = phandle++;
    nodename = g_strdup_printf("/soc/clock-controller@%lx", (long)memmap[MDPP_DEV_PRCI].base);
    qemu_fdt_add_subnode(fdt, nodename);
    qemu_fdt_setprop_cell(fdt, nodename, "phandle", prci_phandle);
    qemu_fdt_setprop_cell(fdt, nodename, "#clock-cells", 0x1);
    qemu_fdt_setprop_cells(fdt, nodename, "clocks", hfclk_phandle, rtcclk_phandle);
    qemu_fdt_setprop_cells(fdt, nodename, "reg", 0x0, memmap[MDPP_DEV_PRCI].base, 0x0, memmap[MDPP_DEV_PRCI].size);
    qemu_fdt_setprop_string(fdt, nodename, "compatible", "mdpp,fu540-c000-prci");
    g_free(nodename);

    plic_phandle = phandle++;
    cells = g_new0(uint32_t, ms->smp.cpus * 4 - 2);
    for (cpu = 0; cpu < ms->smp.cpus; cpu++){
        nodename =
            g_strdup_printf("/cpus/cpu@%d/interrupt-controller", cpu);
        uint32_t intc_phandle = qemu_fdt_get_phandle(fdt, nodename);
        /* cpu 0 is the management hart that does not have S-mode */
        if (cpu == 0){
            cells[0] = cpu_to_be32(intc_phandle);
            cells[1] = cpu_to_be32(IRQ_M_EXT);
        }
        else
        {
            cells[cpu * 4 - 2] = cpu_to_be32(intc_phandle);
            cells[cpu * 4 - 1] = cpu_to_be32(IRQ_M_EXT);
            cells[cpu * 4 + 0] = cpu_to_be32(intc_phandle);
            cells[cpu * 4 + 1] = cpu_to_be32(IRQ_S_EXT);
        }
        g_free(nodename);
    }
    nodename = g_strdup_printf("/soc/interrupt-controller@%lx", (long)memmap[MDPP_DEV_PLIC].base);
    qemu_fdt_add_subnode(fdt, nodename);
    qemu_fdt_setprop_cell(fdt, nodename, "#interrupt-cells", 1);
    qemu_fdt_setprop_string_array(fdt, nodename, "compatible", (char **)&plic_compat, ARRAY_SIZE(plic_compat));
    qemu_fdt_setprop(fdt, nodename, "interrupt-controller", NULL, 0);
    qemu_fdt_setprop(fdt, nodename, "interrupts-extended", cells, (ms->smp.cpus * 4 - 2) * sizeof(uint32_t));
    qemu_fdt_setprop_cells(fdt, nodename, "reg", 0x0, memmap[MDPP_DEV_PLIC].base, 0x0, memmap[MDPP_DEV_PLIC].size);
    qemu_fdt_setprop_cell(fdt, nodename, "riscv,ndev", MDPP_PLIC_NUM_SOURCES - 1);
    qemu_fdt_setprop_cell(fdt, nodename, "phandle", plic_phandle);
    plic_phandle = qemu_fdt_get_phandle(fdt, nodename);
    g_free(cells);
    g_free(nodename);

    gpio_phandle = phandle++;
    int irq_gpio[]={MDPP_GPIO0_IRQ, MDPP_GPIO1_IRQ};
    for (int i = MDPP_DEV_GPIO0, j=0; i < MDPP_DEV_GPIO_COUNT; ++i,++j){
        sprintf(compatible, "mdpp,gpio%d", j);
        nodename = g_strdup_printf("/soc/gpio@%lx", (long)memmap[i].base);
        qemu_fdt_add_subnode(fdt, nodename);
        qemu_fdt_setprop_cell(fdt, nodename, "phandle", gpio_phandle);
        qemu_fdt_setprop_cells(fdt, nodename, "clocks", prci_phandle, PRCI_CLK_TLCLK);
        qemu_fdt_setprop_cell(fdt, nodename, "#interrupt-cells", 2);
        qemu_fdt_setprop(fdt, nodename, "interrupt-controller", NULL, 0);
        qemu_fdt_setprop_cell(fdt, nodename, "#gpio-cells", 2);
        qemu_fdt_setprop(fdt, nodename, "gpio-controller", NULL, 0);
        qemu_fdt_setprop_cells(fdt, nodename, "reg", 0x0, memmap[i].base, 0x0, memmap[i].size);
        qemu_fdt_setprop_cells(fdt, nodename, "interrupts", irq_gpio[j]);
        qemu_fdt_setprop_cell(fdt, nodename, "interrupt-parent", plic_phandle);
        qemu_fdt_setprop_string(fdt, nodename, "compatible", compatible);
        g_free(nodename);
    }
    nodename = g_strdup_printf("/gpio-restart");
    qemu_fdt_add_subnode(fdt, nodename);
    qemu_fdt_setprop_cells(fdt, nodename, "gpios", gpio_phandle, 10, 1);
    qemu_fdt_setprop_string(fdt, nodename, "compatible", "gpio-restart");
    g_free(nodename);

    nodename = g_strdup_printf("/soc/cache-controller@%lx", (long)memmap[MDPP_DEV_L2CC].base);
    qemu_fdt_add_subnode(fdt, nodename);
    qemu_fdt_setprop_cells(fdt, nodename, "reg", 0x0, memmap[MDPP_DEV_L2CC].base, 0x0, memmap[MDPP_DEV_L2CC].size);
    qemu_fdt_setprop_cells(fdt, nodename, "interrupts", MDPP_L2CC_IRQ0, MDPP_L2CC_IRQ1, MDPP_L2CC_IRQ2);
    qemu_fdt_setprop_cell(fdt, nodename, "interrupt-parent", plic_phandle);
    qemu_fdt_setprop(fdt, nodename, "cache-unified", NULL, 0);
    qemu_fdt_setprop_cell(fdt, nodename, "cache-size", 2097152);
    qemu_fdt_setprop_cell(fdt, nodename, "cache-sets", 1024);
    qemu_fdt_setprop_cell(fdt, nodename, "cache-level", 2);
    qemu_fdt_setprop_cell(fdt, nodename, "cache-block-size", 64);
    qemu_fdt_setprop_string(fdt, nodename, "compatible", "mdpp,fu540-c000-ccache");
    g_free(nodename);

    int irq_uart[]={MDPP_UART0_IRQ, MDPP_UART1_IRQ, MDPP_UART2_IRQ, MDPP_UART3_IRQ, MDPP_UART4_IRQ, MDPP_UART5_IRQ};
    for (int i = MDPP_DEV_UART0, j = 0; i < MDPP_DEV_UART_COUNT; ++i,++j){
        sprintf(compatible, "mdpp,apbuart%d", j);
        sprintf(alias, "serial%d", j);

        nodename = g_strdup_printf("/soc/serial@%lx", (long)memmap[i].base);
        qemu_fdt_add_subnode(fdt, nodename);
        qemu_fdt_setprop_string(fdt, nodename, "compatible", compatible);
        qemu_fdt_setprop_cells(fdt, nodename, "reg", 0x0, memmap[i].base, 0x0, memmap[i].size);
        qemu_fdt_setprop_cells(fdt, nodename, "clocks", prci_phandle, PRCI_CLK_TLCLK);
        qemu_fdt_setprop_cell(fdt, nodename, "interrupt-parent", plic_phandle);
        qemu_fdt_setprop_cell(fdt, nodename, "interrupts", irq_uart[j]);
        qemu_fdt_setprop_string(fdt, "/aliases", alias, nodename);
        g_free(nodename);
    }
    nodename = g_strdup_printf("/soc/serial@%lx", (long)memmap[MDPP_DEV_UART0].base);
    qemu_fdt_add_subnode(fdt, "/chosen");
    qemu_fdt_setprop_string(fdt, "/chosen", "stdout-path", nodename);
    g_free(nodename);

    add_fdt(MDPP_DEV_CAN0, MDPP_DEV_CAN_COUNT-MDPP_DEV_CAN0, "can", plic_phandle, {MDPP_CAN0_IRQ, MDPP_CAN1_IRQ});
    add_fdt(MDPP_DEV_NVMEM0, MDPP_DEV_NVMEM_COUNT-MDPP_DEV_NVMEM0, "nvmem", plic_phandle, {MDPP_NVMEM0_IRQ, MDPP_NVMEM1_IRQ});
    add_fdt(MDPP_DEV_LVDS0, MDPP_DEV_LVDS_COUNT-MDPP_DEV_LVDS0, "lvds", plic_phandle, {MDPP_LVDS0_IRQ, MDPP_LVDS1_IRQ});
    add_fdt(MDPP_DEV_OBT, 1, "obt", plic_phandle, {MDPP_OBT_IRQ});
    add_fdt(MDPP_DEV_SRF, 1, "srf", plic_phandle, {MDPP_SRF_IRQ});


}

static void mdpp_machine_reset(void *opaque, int n, int level){
    /* gpio pin active low triggers reset */
    if (!level){
        qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
    }
}

static void mdpp_machine_init(MachineState *machine){
    const MemMapEntry *memmap = mdpp_memmap;
    MDPPState *s = RISCV_U_MACHINE(machine);
    MemoryRegion *system_memory = get_system_memory();
    MemoryRegion *flash0 = g_new(MemoryRegion, 1);
    hwaddr start_addr = memmap[MDPP_DEV_DRAM].base;
    target_ulong firmware_end_addr, kernel_start_addr;
    const char *firmware_name;
    uint32_t start_addr_hi32 = 0x00000000;
    uint32_t fdt_load_addr_hi32 = 0x00000000;
    int i;
    uint64_t fdt_load_addr;
    uint64_t kernel_entry;
    DriveInfo *dinfo;
    BlockBackend *blk;
    DeviceState *flash_dev, *sd_dev, *card_dev;
    qemu_irq flash_cs, sd_cs;
    RISCVBootInfo boot_info;

    /* Initialize SoC */
    object_initialize_child(OBJECT(machine), "soc", &s->soc, TYPE_RISCV_U_SOC);
    object_property_set_uint(OBJECT(&s->soc), "serial", s->serial, &error_abort);
    object_property_set_str(OBJECT(&s->soc), "cpu-type", machine->cpu_type, &error_abort);
    qdev_realize(DEVICE(&s->soc), NULL, &error_fatal);

    /* register RAM */
    memory_region_add_subregion(system_memory, memmap[MDPP_DEV_DRAM].base, machine->ram);

    /* register gpio-restart */
    qdev_connect_gpio_out(DEVICE(&(s->soc.gpio)), 10, qemu_allocate_irq(mdpp_machine_reset, NULL, 0));

    /* load/create device tree */
    if (machine->dtb){
        machine->fdt = load_device_tree(machine->dtb, &s->fdt_size);
        if (!machine->fdt){
            error_report("load_device_tree() failed");
            exit(1);
        }
    }
    else
    {
        create_fdt(s, memmap, riscv_is_32bit(&s->soc.u_cpus));
    }

    start_addr = memmap[MDPP_DEV_DRAM].base;
    
    firmware_name = riscv_default_firmware_name(&s->soc.u_cpus);
    firmware_end_addr = riscv_find_and_load_firmware(machine, firmware_name, &start_addr, NULL);

    riscv_boot_info_init(&boot_info, &s->soc.u_cpus);
    if (machine->kernel_filename){
        kernel_start_addr = riscv_calc_kernel_start_addr(&boot_info, firmware_end_addr);
        riscv_load_kernel(machine, &boot_info, kernel_start_addr, true, NULL);
        kernel_entry = boot_info.image_low_addr;
    }
    else
    {
        /*
         * If dynamic firmware is used, it doesn't know where is the next mode
         * if kernel argument is not set.
         */
        kernel_entry = 0;
    }

    fdt_load_addr = riscv_compute_fdt_addr(memmap[MDPP_DEV_DRAM].base, memmap[MDPP_DEV_DRAM].size, machine, &boot_info);
    riscv_load_fdt(fdt_load_addr, machine->fdt);

    if (!riscv_is_32bit(&s->soc.u_cpus)){
        start_addr_hi32 = (uint64_t)start_addr >> 32;
        fdt_load_addr_hi32 = fdt_load_addr >> 32;
    }

    /* reset vector */
    uint32_t reset_vec[12] = {
        s->msel,    /* MSEL pin state */
        0x00000297, /* 1:  auipc  t0, %pcrel_hi(fw_dyn) */
        0x02c28613, /*     addi   a2, t0, %pcrel_lo(1b) */
        0xf1402573, /*     csrr   a0, mhartid  */
        0, 0, 0x00028067, /*     jr     t0 */
        start_addr, /* start: .dword */
        start_addr_hi32, fdt_load_addr, /* fdt_laddr: .dword */
        fdt_load_addr_hi32, 0x00000000, /* fw_dyn: */
    };
    if (riscv_is_32bit(&s->soc.u_cpus)){
        reset_vec[4] = 0x0202a583; /*     lw     a1, 32(t0) */
        reset_vec[5] = 0x0182a283; /*     lw     t0, 24(t0) */
    }
    else
    {
        reset_vec[4] = 0x0202b583; /*     ld     a1, 32(t0) */
        reset_vec[5] = 0x0182b283; /*     ld     t0, 24(t0) */
    }

    /* copy in the reset vector in little_endian byte order */
    for (i = 0; i < ARRAY_SIZE(reset_vec); i++){
        reset_vec[i] = cpu_to_le32(reset_vec[i]);
    }
    rom_add_blob_fixed_as("mrom.reset", reset_vec, sizeof(reset_vec), memmap[MDPP_DEV_MROM].base, &address_space_memory);

    riscv_rom_copy_firmware_info(machine, &s->soc.u_cpus, memmap[MDPP_DEV_MROM].base, memmap[MDPP_DEV_MROM].size, sizeof(reset_vec), kernel_entry);

}

static bool mdpp_machine_get_start_in_flash(Object *obj, Error **errp){
    MDPPState *s = RISCV_U_MACHINE(obj);

    return s->start_in_flash;
}

static void mdpp_machine_set_start_in_flash(Object *obj, bool value, Error **errp){
    MDPPState *s = RISCV_U_MACHINE(obj);

    s->start_in_flash = value;
}

static void mdpp_machine_instance_init(Object *obj){
    MDPPState *s = RISCV_U_MACHINE(obj);

    s->start_in_flash = false;
    s->msel = 0;
    /* Valores por defecto */
    s->soc.can_udp_port[0] = MDPP_DEFAULT_CAN0_UDP_PORT;
    s->soc.can_udp_port[1] = MDPP_DEFAULT_CAN1_UDP_PORT;
    s->soc.lvds_tcp_port[0] = MDPP_DEFAULT_LVDS0_TCP_PORT;
    s->soc.lvds_tcp_port[1] = MDPP_DEFAULT_LVDS1_TCP_PORT;
    s->soc.nvmem_file[0] = g_strdup(MDPP_DEFAULT_NVMEM0_FILE);
    s->soc.nvmem_file[1] = g_strdup(MDPP_DEFAULT_NVMEM1_FILE);
    object_property_add_uint32_ptr(obj, "msel", &s->msel, OBJ_PROP_FLAG_READWRITE);
    object_property_set_description(obj, "msel", "Mode Select (MSEL[3:0]) pin state");

}

static void mdpp_machine_class_init(ObjectClass *oc, void *data){
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "RISC-V Board compatible with MDPP";
    mc->init = mdpp_machine_init;
    mc->max_cpus = MDPP_MANAGEMENT_CPU_COUNT + MDPP_COMPUTE_CPU_COUNT;
    mc->min_cpus = MDPP_MANAGEMENT_CPU_COUNT + 1;
    mc->default_cpu_type = MDPP_CPU;
    mc->default_cpus = mc->min_cpus;
    mc->default_ram_id = "riscv.mdpp.u.ram";

    object_class_property_add_int32(oc, "can0-udp-port", offsetof(MDPPSoCState, can_udp_port[0]), OBJ_PROP_FLAG_CONFIG);
    object_class_property_add_int32(oc, "can1-udp-port", offsetof(MDPPSoCState, can_udp_port[1]), OBJ_PROP_FLAG_CONFIG);
    object_class_property_add_int32(oc, "lvds0-tcp-port", offsetof(MDPPSoCState, lvds_tcp_port[0]), OBJ_PROP_FLAG_CONFIG);
    object_class_property_add_int32(oc, "lvds1-tcp-port", offsetof(MDPPSoCState, lvds_tcp_port[1]), OBJ_PROP_FLAG_CONFIG);
    object_class_property_add_str(oc, "nvmem0-file", offsetof(MDPPSoCState, nvmem_file[0]), OBJ_PROP_FLAG_CONFIG);
    object_class_property_add_str(oc, "nvmem1-file", offsetof(MDPPSoCState, nvmem_file[1]), OBJ_PROP_FLAG_CONFIG);

    object_class_property_add_bool(oc, "start-in-flash", mdpp_machine_get_start_in_flash, mdpp_machine_set_start_in_flash);
    object_class_property_set_description(oc, "start-in-flash", "Set on to tell QEMU's ROM to jump to " "flash. Otherwise QEMU will jump to DRAM ");
}

static const TypeInfo mdpp_machine_typeinfo = {
    .name = MACHINE_TYPE_NAME("mdpp"), .parent = TYPE_MACHINE, .class_init = mdpp_machine_class_init, .instance_init = mdpp_machine_instance_init, .instance_size = sizeof(MDPPState), 
};

static void mdpp_machine_init_register_types(void){
    type_register_static(&mdpp_machine_typeinfo);
}

type_init(mdpp_machine_init_register_types)

static void mdpp_soc_instance_init(Object *obj){
    MDPPSoCState *s = RISCV_U_SOC(obj);

    object_initialize_child(obj, "e-cluster", &s->e_cluster, TYPE_CPU_CLUSTER);
    qdev_prop_set_uint32(DEVICE(&s->e_cluster), "cluster-id", 0);

    object_initialize_child(OBJECT(&s->e_cluster), "e-cpus", &s->e_cpus, TYPE_RISCV_HART_ARRAY);
    qdev_prop_set_uint32(DEVICE(&s->e_cpus), "num-harts", 1);
    qdev_prop_set_uint32(DEVICE(&s->e_cpus), "hartid-base", 0);
    qdev_prop_set_string(DEVICE(&s->e_cpus), "cpu-type", MDPP_E_CPU);
    qdev_prop_set_uint64(DEVICE(&s->e_cpus), "resetvec", 0x1004);

    object_initialize_child(obj, "u-cluster", &s->u_cluster, TYPE_CPU_CLUSTER);
    qdev_prop_set_uint32(DEVICE(&s->u_cluster), "cluster-id", 1);

    object_initialize_child(OBJECT(&s->u_cluster), "u-cpus", &s->u_cpus, TYPE_RISCV_HART_ARRAY);

    object_initialize_child(obj, "prci", &s->prci, TYPE_MDPP_PRCI);
    object_initialize_child(obj, "gpio", &s->gpio, TYPE_MDPP_GPIO);
}

static void mdpp_soc_realize(DeviceState *dev, Error **errp){
    MachineState *ms = MACHINE(qdev_get_machine());
    MDPPSoCState *s = RISCV_U_SOC(dev);
    const MemMapEntry *memmap = mdpp_memmap;
    MemoryRegion *system_memory = get_system_memory();
    MemoryRegion *mask_rom = g_new(MemoryRegion, 1);
    char *plic_hart_config;
    int i, j;

    qdev_prop_set_uint32(DEVICE(&s->u_cpus), "num-harts", ms->smp.cpus - 1);
    qdev_prop_set_uint32(DEVICE(&s->u_cpus), "hartid-base", 1);
    qdev_prop_set_string(DEVICE(&s->u_cpus), "cpu-type", s->cpu_type);
    qdev_prop_set_uint64(DEVICE(&s->u_cpus), "resetvec", 0x1004);

    sysbus_realize(SYS_BUS_DEVICE(&s->e_cpus), &error_fatal);
    sysbus_realize(SYS_BUS_DEVICE(&s->u_cpus), &error_fatal);
    /*
     * The cluster must be realized after the RISC-V hart array container, * as the container's CPU object is only created on realize, and the
     * CPU must exist and have been parented into the cluster before the
     * cluster is realized.
     */
    qdev_realize(DEVICE(&s->e_cluster), NULL, &error_abort);
    qdev_realize(DEVICE(&s->u_cluster), NULL, &error_abort);

    /* boot rom */
    memory_region_init_rom(mask_rom, OBJECT(dev), "riscv.mdpp.u.mrom", memmap[MDPP_DEV_MROM].size, &error_fatal);
    memory_region_add_subregion(system_memory, memmap[MDPP_DEV_MROM].base, mask_rom);

    /* create PLIC hart topology configuration string */
    plic_hart_config = riscv_plic_hart_config_string(ms->smp.cpus);

    /* MMIO */
    s->plic = mdpp_plic_create(memmap[MDPP_DEV_PLIC].base, plic_hart_config, ms->smp.cpus, 0, MDPP_PLIC_NUM_SOURCES, MDPP_PLIC_NUM_PRIORITIES, MDPP_PLIC_PRIORITY_BASE, MDPP_PLIC_PENDING_BASE, MDPP_PLIC_ENABLE_BASE, MDPP_PLIC_ENABLE_STRIDE, MDPP_PLIC_CONTEXT_BASE, MDPP_PLIC_CONTEXT_STRIDE, memmap[MDPP_DEV_PLIC].size);
    g_free(plic_hart_config);
    int uartIrqs[]={MDPP_UART0_IRQ,MDPP_UART1_IRQ,MDPP_UART2_IRQ,MDPP_UART3_IRQ,MDPP_UART4_IRQ,MDPP_UART5_IRQ};
    for (i = MDPP_DEV_UART0, j=0; i < MDPP_DEV_UART_COUNT; ++i,++j){
        mdpp_uart_create(system_memory, memmap[i].base, serial_hd(j), qdev_get_gpio_in(DEVICE(s->plic), uartIrqs[j]));
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->uart[j]), 0, qdev_get_gpio_in(DEVICE(s->plic), uartIrqs[j]));
    }
    riscv_aclint_swi_create(memmap[MDPP_DEV_CLINT].base, 0, ms->smp.cpus, false);
    riscv_aclint_mtimer_create(memmap[MDPP_DEV_CLINT].base + RISCV_ACLINT_SWI_SIZE, RISCV_ACLINT_DEFAULT_MTIMER_SIZE, 0, ms->smp.cpus, RISCV_ACLINT_DEFAULT_MTIMECMP, RISCV_ACLINT_DEFAULT_MTIME, CLINT_TIMEBASE_FREQ, false);

    if (!sysbus_realize(SYS_BUS_DEVICE(&s->prci), errp)){
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->prci), 0, memmap[MDPP_DEV_PRCI].base);

    int gpioIrq[]={MDPP_GPIO0_IRQ, MDPP_GPIO1_IRQ};
    for (i = 0; i < (MDPP_DEV_GPIO_COUNT-MDPP_DEV_GPIO0); ++i){
        char gpioName[7];
        sprintf(gpioName,"ngpio%d",i);
        qdev_prop_set_uint32(DEVICE(&s->gpio[i]), gpioName, 16);
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->gpio[i]), errp)){
            return;
        }
        sysbus_mmio_map(SYS_BUS_DEVICE(&s->gpio[i]), 0, memmap[i+MDPP_DEV_GPIO0].base);
        /* Pass all GPIOs to the SOC layer so they are available to the board */
        qdev_pass_gpios(DEVICE(&s->gpio[i]), dev, NULL);
        /* Connect GPIO interrupts to the PLIC */
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->gpio[i]), i, qdev_get_gpio_in(DEVICE(s->plic), gpioIrq[i]));
    }

    int irq_can[]={MDPP_CAN0_IRQ, MDPP_CAN1_IRQ};
    for (i = 0; i < (MDPP_DEV_CAN_COUNT-MDPP_DEV_CAN0); ++i) {
        sysbus_realize_and_unref(SYS_BUS_DEVICE(s->can[i]), &error_fatal);
        sysbus_mmio_map(SYS_BUS_DEVICE(s->can), 0, memmap[i+MDPP_DEV_CAN0].base);
        sysbus_connect_irq(SYS_BUS_DEVICE(s->can), 0, qdev_get_gpio_in(DEVICE(s->plic), irq_can[i]));
    }
    
    for (int i = MDPP_DEV_NVMEM0, j = 0; i < MDPP_DEV_NVMEM_COUNT; ++i, ++j) {
        sysbus_mmio_map(SYS_BUS_DEVICE(&s->nvmem[j]), 0, memmap[i].base);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->nvmem[j]), 0, qdev_get_gpio_in(DEVICE(s->plic), MDPP_NVMEM0_IRQ + (j)));
    }
    for (int i = MDPP_DEV_LVDS0, j = 0; i < MDPP_DEV_LVDS_COUNT; ++i, ++j) {
        sysbus_mmio_map(SYS_BUS_DEVICE(&s->lvds[j]), 0, memmap[i].base);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->lvds[j]), 0, qdev_get_gpio_in(DEVICE(s->plic), MDPP_LVDS0_IRQ + (j)));
    }
    
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->obt), 0, memmap[MDPP_DEV_OBT].base);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->obt), 0, qdev_get_gpio_in(DEVICE(s->plic), MDPP_OBT_IRQ));
    
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->srf), 0, memmap[MDPP_DEV_SRF].base);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->srf), 0, qdev_get_gpio_in(DEVICE(s->plic), MDPP_SRF_IRQ));
    
  /*
    create_unimplemented_device("riscv.mdpp.u.dmc", memmap[MDPP_DEV_DMC].base, memmap[MDPP_DEV_DMC].size);
    create_unimplemented_device("riscv.mdpp.u.l2cc", memmap[MDPP_DEV_L2CC].base, memmap[MDPP_DEV_L2CC].size);
*/
    for (i = 0; i < (MDPP_DEV_CAN_COUNT-MDPP_DEV_CAN0); ++i) {
        if (s->can_udp_port[i] > 0) {
            char socket_path[50];
            snprintf(socket_path, sizeof(socket_path), "udp::%d", s->can_udp_port[i]);
            qdev_prop_set_string(DEVICE(&s->can[i]), "chardev", socket_path);
        }
    }
    for (i = 0; i < (MDPP_DEV_LVDS_COUNT - MDPP_DEV_LVDS0); ++i) {
        if (s->lvds_tcp_port[i] > 0) {
            char socket_path[50];
            snprintf(socket_path, sizeof(socket_path), "tcp::%d", s->lvds_tcp_port[i]);
            qdev_prop_set_string(DEVICE(&s->lvds[i]), "chardev", socket_path);
        }
    }
    for (i = 0; i < (MDPP_DEV_NVMEM_COUNT - MDPP_DEV_NVMEM0); ++i) {
        if (s->nvmem_file[i]) {
            qdev_prop_set_string(DEVICE(&s->nvmem[i]), "file", s->nvmem_file[i]);
        }
    }
    create_unimplemented_device("riscv.mdpp.u.can0", memmap[MDPP_DEV_CAN0].base, memmap[MDPP_DEV_CAN0].size);
    create_unimplemented_device("riscv.mdpp.u.can1", memmap[MDPP_DEV_CAN1].base, memmap[MDPP_DEV_CAN1].size);
    create_unimplemented_device("riscv.mdpp.u.obt",    memmap[MDPP_DEV_OBT].base, memmap[MDPP_DEV_OBT].size);
    create_unimplemented_device("riscv.mdpp.u.nvmem0", memmap[MDPP_DEV_NVMEM0].base, memmap[MDPP_DEV_NVMEM0].size);
    create_unimplemented_device("riscv.mdpp.u.nvmem1", memmap[MDPP_DEV_NVMEM1].base, memmap[MDPP_DEV_NVMEM1].size);
    create_unimplemented_device("riscv.mdpp.u.srf",   memmap[MDPP_DEV_SRF].base, memmap[MDPP_DEV_SRF].size);
    create_unimplemented_device("riscv.mdpp.u.lvds0", memmap[MDPP_DEV_LVDS0].base, memmap[MDPP_DEV_LVDS0].size);
    create_unimplemented_device("riscv.mdpp.u.lvds1", memmap[MDPP_DEV_LVDS1].base, memmap[MDPP_DEV_LVDS1].size);
}

static void mdpp_soc_class_init(ObjectClass *oc, void *data){
    DeviceClass *dc = DEVICE_CLASS(oc);

    device_class_set_props(dc, mdpp_soc_props);
    dc->realize = mdpp_soc_realize;
    /* Reason: Uses serial_hds in realize function, thus can't be used twice */
    dc->user_creatable = false;
}

static const TypeInfo mdpp_soc_type_info = {
    .name = TYPE_RISCV_U_SOC, .parent = TYPE_DEVICE, .instance_size = sizeof(MDPPSoCState), .instance_init = mdpp_soc_instance_init, .class_init = mdpp_soc_class_init, 
};

static void mdpp_soc_register_types(void){
    type_register_static(&mdpp_soc_type_info);
}

type_init(mdpp_soc_register_types)
