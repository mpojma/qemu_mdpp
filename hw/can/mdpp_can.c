#include "qemu/osdep.h"
#include "hw/qdev-properties.h"
#include "hw/sysbus.h"
#include "qemu/module.h"
#include "qemu/log.h"


static uint64_t can_device_read(void *opaque, hwaddr addr, unsigned size){
    MDPP_CANState *s = opaque;
    uint32_t ret = 0;

    switch (addr) {
        case CAN_CONTROL: 
            ret = s->control;
            break;
        case CAN_STATUS: // STATUS
            ret = s->status;
            break;
        case CAN_INTERRUPT: // INTERRUPT
            ret = s->interrupt;
            break;
        case CAN_RX_ID1: // RX_ID1
            ret = s->rx_id1;
            break;
        case CAN_RX_ID2_RTR_DLC: // RX_ID2_RTR_DLC
            ret = s->rx_id2_rtr_dlc;
            break;
        case CAN_RX_DATA_BYTE_1 ... CAN_RX_DATA_BYTE_8: // RX_DATA_BYTE_1 to RX_DATA_BYTE_8
            ret = s->rx_data[(addr - CAN_RX_DATA_BYTE_1) >> 2];
            break;
        default:
            qemu_log_mask(LOG_GUEST_ERROR, "CAN device read: bad offset %x\n", (int)addr);
            break;
    }

    return ret;
}

static void can_device_write(void *opaque, hwaddr addr, uint64_t val, unsigned size){
    MDPP_CANState *s = opaque;

    switch (addr) {
        case CAN_CONTROL: // CONTROL
            s->control = val;
            break;
        case CAN_COMMAND: // COMMAND
            s->command = val;
            if (val & 0x01) { // TR - Transmit Request
                // Simulate transmission
                s->status |= 0x04; // TBS - Transmit Buffer Status
                s->interrupt |= 0x02; // TI - Transmit Interrupt
            }
            break;
        case CAN_ACCEPTANCE_CODE: // ACCEPTANCE_CODE
            s->acceptance_code = val;
            break;
        case CAN_ACCEPTANCE_MASK: // ACCEPTANCE_MASK
            s->acceptance_mask = val;
            break;
        case CAN_BUS_TIMING_0: // BUS_TIMING_0
            s->bus_timing_0 = val;
            break;
        case CAN_BUS_TIMING_1: // BUS_TIMING_1
            s->bus_timing_1 = val;
            break;
        case CAN_TX_ID1: // TX_ID1
            s->tx_id1 = val;
            break;
        case CAN_TX_ID2_RTR_DLC: // TX_ID2_RTR_DLC
            s->tx_id2_rtr_dlc = val;
            break;
        case CAN_TX_DATA_BYTE_1 ... CAN_TX_DATA_BYTE_8: // TX_DATA_BYTE_1 to TX_DATA_BYTE_8
            s->tx_data[(addr - CAN_TX_DATA_BYTE_1) >> 2] = val;
            break;
        case CAN_CLOCK_DIVIDER: // CLOCK_DIVIDER
            s->clock_divider = val;
            break;
        default:
            qemu_log_mask(LOG_GUEST_ERROR, "CAN device write: bad offset %x\n", (int)addr);
            break;
    }
}

static const MemoryRegionOps can_device_ops = {
    .read = can_device_read,
    .write = can_device_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void can_device_realize(DeviceState *dev, Error **errp){
    MDPP_CANState *s = MDPP_CAN(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &can_device_ops, s, "can", 0x80);
    sysbus_init_mmio(sbd, &s->iomem);
}

static void can_device_class_init(ObjectClass *klass, void *data){
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = can_device_realize;
}

static const TypeInfo can_device_info = {
    .name = TYPE_MDPP_CAN,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(MDPP_CANState),
    .class_init = can_device_class_init,
};

static void can_device_register_types(void){
    type_register_static(&can_device_info);
}

type_init(can_device_register_types)