/*
 * MDPP series machine interface
 *
 * Copyright (c) 2017 MDPP, Inc.
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

#ifndef HW_MDPP_H
#define HW_MDPP_H

#include "hw/boards.h"
#include "hw/cpu/cluster.h"
#include "hw/riscv/riscv_hart.h"
#include "hw/riscv/mdpp_cpu.h"
#include "hw/gpio/mdpp_gpio.h"
#include "hw/gpio/mdpp_can.h"
#include "hw/misc/mdpp_prci.h"

#define TYPE_RISCV_U_SOC "riscv.mdpp.u.soc"
#define RISCV_U_SOC(obj) OBJECT_CHECK(MDPPSoCState, (obj), TYPE_RISCV_U_SOC)

#define MDPP_DEFAULT_CAN0_UDP_PORT 15000
#define MDPP_DEFAULT_CAN1_UDP_PORT 15001
#define MDPP_DEFAULT_LVDS0_TCP_PORT 16000
#define MDPP_DEFAULT_LVDS1_TCP_PORT 16001
#define MDPP_DEFAULT_NVMEM0_FILE "/tmp/nvmem0.img"
#define MDPP_DEFAULT_NVMEM1_FILE "/tmp/nvmem1.img"
    
typedef struct MDPPSoCState {
    /*< private >*/
    DeviceState parent_obj;

    /*< public >*/
    CPUClusterState e_cluster;
    CPUClusterState u_cluster;
    RISCVHartArrayState e_cpus;
    RISCVHartArrayState u_cpus;
    DeviceState *plic;
    MDPP_CanState can[2];
    MDPP_GPIOState gpio[2];
    MDPP_NvMemState nvmem[2];
    MDPP_LvdsState lvds[2];
    MDPP_UartState uart[6];
    MDPP_OBTState obt;

    uint32_t serial;
    char *cpu_type;
    /* Configuración de red */
    int can_udp_port[2];  // Puertos UDP para CAN0 y CAN1
    int lvds_tcp_port[2]; // Puertos TCP para LVDS0 y LVDS1

    /* Configuración de almacenamiento */
    char *nvmem_file[2]; // Archivos de almacenamiento para NVMEM0 y NVMEM1
} MDPPSoCState;

#define TYPE_RISCV_U_MACHINE MACHINE_TYPE_NAME("mdpp")
#define RISCV_U_MACHINE(obj) \
    OBJECT_CHECK(MDPPState, (obj), TYPE_RISCV_U_MACHINE)

typedef struct MDPPState {
    /*< private >*/
    MachineState parent_obj;

    /*< public >*/
    MDPPSoCState soc;
    int fdt_size;

    bool start_in_flash;
    uint32_t msel;
    uint32_t serial;
} MDPPState;

enum {
    MDPP_DEV_DEBUG,
    MDPP_DEV_MROM,
    MDPP_DEV_CLINT,
    MDPP_DEV_PLIC,
    MDPP_DEV_UART0,
    MDPP_DEV_UART1,
    MDPP_DEV_UART2,
    MDPP_DEV_UART3,
    MDPP_DEV_UART4,
    MDPP_DEV_UART5,
    MDPP_DEV_UART_COUNT,
    MDPP_DEV_GPIO0,
    MDPP_DEV_GPIO1,
    MDPP_DEV_GPIO_COUNT,
    MDPP_DEV_CAN0,
    MDPP_DEV_CAN1,
    MDPP_DEV_CAN_COUNT,
    MDPP_DEV_OBT,
    MDPP_DEV_NVMEM0,
    MDPP_DEV_NVMEM1,
    MDPP_DEV_NVMEM_COUNT,
    MDPP_DEV_SRF,
    MDPP_DEV_LVDS0,
    MDPP_DEV_LVDS1,
    MDPP_DEV_LVDS_COUNT,
    MDPP_DEV_DRAM
};

enum {
    MDPP_UART0_IRQ  = 1,
    MDPP_UART1_IRQ  = 8,
    MDPP_UART2_IRQ  = 9,
    MDPP_UART3_IRQ  = 10,
    MDPP_UART4_IRQ  = 11,
    MDPP_UART5_IRQ  = 12,
    MDPP_GPIO0_IRQ  = 21,
    MDPP_GPIO1_IRQ  = 22,
    MDPP_CAN0_IRQ   = 23,
    MDPP_CAN1_IRQ   = 24,
    MDPP_OBT_IRQ    = 25,
    MDPP_NVMEM0_IRQ = 17,
    MDPP_NVMEM1_IRQ = 18,
    MDPP_SRF_IRQ    = 26,
    MDPP_LVDS0_IRQ  = 19,
    MDPP_LVDS1_IRQ  = 20
};

enum {
    MDPP_HFCLK_FREQ = 33333333,
    MDPP_RTCCLK_FREQ = 1000000
};

enum {
    MSEL_MEMMAP_QSPI0_FLASH = 1,
};

#define MDPP_MANAGEMENT_CPU_COUNT   1
#define MDPP_COMPUTE_CPU_COUNT      4

#define MDPP_PLIC_NUM_SOURCES 54
#define MDPP_PLIC_NUM_PRIORITIES 7
#define MDPP_PLIC_PRIORITY_BASE 0x00
#define MDPP_PLIC_PENDING_BASE 0x1000
#define MDPP_PLIC_ENABLE_BASE 0x2000
#define MDPP_PLIC_ENABLE_STRIDE 0x80
#define MDPP_PLIC_CONTEXT_BASE 0x200000
#define MDPP_PLIC_CONTEXT_STRIDE 0x1000

#endif
