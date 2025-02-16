/*
 * MDPP System-on-Chip CAN register definition
 *
 *
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

 #ifndef MDPP_CAN_H
 #define MDPP_CAN_H
 
 #include "hw/sysbus.h"
 #include "qom/object.h"
 
 #define TYPE_MDPP_CAN "mdpp_soc.can"
 
 typedef struct MDPP_CANState {
    SysBusDevice parent_obj;
    MemoryRegion iomem;

    uint32_t control;
    uint32_t command;
    uint32_t status;
    uint32_t interrupt;
    uint32_t acceptance_code;
    uint32_t acceptance_mask;
    uint32_t bus_timing_0;
    uint32_t bus_timing_1;
    uint32_t tx_id1;
    uint32_t tx_id2_rtr_dlc;
    uint32_t tx_data[8];
    uint32_t rx_id1;
    uint32_t rx_id2_rtr_dlc;
    uint32_t rx_data[8];
    uint32_t clock_divider;
} MDPP_CANState;

 DECLARE_INSTANCE_CHECKER(MDPP_CANState, MDPP_CAN, TYPE_MDPP_CAN)

 
 #define MDPP_CAN_PINS 4
 
 #define MDPP_CAN_SIZE 0x100
 
#define CAN_CONTROL             0x00    //    RW
#define CAN_COMMAND             0x04    //    WO
#define CAN_STATUS              0x08    //    RO
#define CAN_INTERRUPT           0x0C    //    RO
#define CAN_ACCEPTANCE_CODE     0x10    //    RstMode
#define CAN_ACCEPTANCE_MASK     0x14    //    RstMode
#define CAN_BUS_TIMING_0        0x18    //    RstMode
#define CAN_BUS_TIMING_1        0x1C    //    RstMode
#define CAN_RESERVED_0          0x20    //    0
#define CAN_RESERVED_1          0x24    //    0
#define CAN_TX_ID1              0x28    //    RW
#define CAN_TX_ID2_RTR_DLC      0x2C    //    RW
#define CAN_TX_DATA_BYTE_1      0x30    //    RW
#define CAN_TX_DATA_BYTE_2      0x34    //    RW
#define CAN_TX_DATA_BYTE_3      0x38    //    RW
#define CAN_TX_DATA_BYTE_4      0x3C    //    RW
#define CAN_TX_DATA_BYTE_5      0x40    //    RW
#define CAN_TX_DATA_BYTE_6      0x44    //    RW
#define CAN_TX_DATA_BYTE_7      0x48    //    RW
#define CAN_TX_DATA_BYTE_8      0x4C    //    RW
#define CAN_RX_ID1              0x50    //    RO
#define CAN_RX_ID2_RTR_DLC      0x54    //    RO
#define CAN_RX_DATA_BYTE_1      0x58    //    RO
#define CAN_RX_DATA_BYTE_2      0x5C    //    RO
#define CAN_RX_DATA_BYTE_3      0x60    //    RO
#define CAN_RX_DATA_BYTE_4      0x64    //    RO
#define CAN_RX_DATA_BYTE_5      0x68    //    RO
#define CAN_RX_DATA_BYTE_6      0x6C    //    RO
#define CAN_RX_DATA_BYTE_7      0x70    //    RO
#define CAN_RX_DATA_BYTE_8      0x74    //    RO
#define CAN_RESERVED_2          0x78    //    0
#define CAN_CLOCK_DIVIDER       0x7C    //    RW
 
 
 #endif /* MDPP_CAN_H */
 