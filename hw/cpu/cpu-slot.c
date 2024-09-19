/*
 * CPU slot abstraction - manage CPU topology
 *
 * Copyright (C) 2024 Intel Corporation.
 *
 * Author: Zhao Liu <zhao1.liu@intel.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * later.  See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"

#include "hw/boards.h"
#include "hw/cpu/cpu-slot.h"
#include "hw/cpu/cpu-topology.h"
#include "hw/qdev-core.h"
#include "hw/qdev-properties.h"
#include "hw/sysbus.h"
#include "qapi/error.h"

static void cpu_slot_add_topo_info(CPUSlot *slot, CPUTopoState *topo)
{
    CpuTopologyLevel level = GET_CPU_TOPO_LEVEL(topo);
    CPUTopoStatEntry *entry;
    int instances_num;

    entry = &slot->stat.entries[level];
    entry->total_instances++;

    instances_num = cpu_topo_get_instances_num(topo);
    if (instances_num > entry->max_instances) {
        entry->max_instances = instances_num;
    }

    set_bit(level, slot->stat.curr_levels);

    return;
}

static void cpu_slot_device_realize(DeviceListener *listener,
                                    DeviceState *dev)
{
    CPUSlot *slot = container_of(listener, CPUSlot, listener);
    CPUTopoState *topo;

    if (!object_dynamic_cast(OBJECT(dev), TYPE_CPU_TOPO)) {
        return;
    }

    topo = CPU_TOPO(dev);
    cpu_slot_add_topo_info(slot, topo);
}

static void cpu_slot_del_topo_info(CPUSlot *slot, CPUTopoState *topo)
{
    CpuTopologyLevel level = GET_CPU_TOPO_LEVEL(topo);
    CPUTopoStatEntry *entry;

    entry = &slot->stat.entries[level];
    entry->total_instances--;

    return;
}

static void cpu_slot_device_unrealize(DeviceListener *listener,
                                      DeviceState *dev)
{
    CPUSlot *slot = container_of(listener, CPUSlot, listener);
    CPUTopoState *topo;

    if (!object_dynamic_cast(OBJECT(dev), TYPE_CPU_TOPO)) {
        return;
    }

    topo = CPU_TOPO(dev);
    cpu_slot_del_topo_info(slot, topo);
}

DeviceListener cpu_slot_device_listener = {
    .realize = cpu_slot_device_realize,
    .unrealize = cpu_slot_device_unrealize,
};

static bool slot_bus_check_topology(CPUBusState *cbus,
                                    CPUTopoState *topo,
                                    Error **errp)
{
    CPUSlot *slot = CPU_SLOT(BUS(cbus)->parent);
    CpuTopologyLevel level = GET_CPU_TOPO_LEVEL(topo);

    if (!test_bit(level, slot->supported_levels)) {
        error_setg(errp, "cpu topo: level %s is not supported",
                   CpuTopologyLevel_str(level));
        return false;
    }
    return true;
}

static void cpu_slot_realize(DeviceState *dev, Error **errp)
{
    CPUSlot *slot = CPU_SLOT(dev);

    slot->listener = cpu_slot_device_listener;
    device_listener_register(&slot->listener);

    qbus_init(&slot->bus, sizeof(CPUBusState),
              TYPE_CPU_BUS, dev, "cpu-slot");
    slot->bus.check_topology = slot_bus_check_topology;
}

static void cpu_slot_unrealize(DeviceState *dev)
{
    CPUSlot *slot = CPU_SLOT(dev);

    device_listener_unregister(&slot->listener);
}

static void cpu_slot_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    set_bit(DEVICE_CATEGORY_BRIDGE, dc->categories);
    dc->realize = cpu_slot_realize;
    dc->unrealize = cpu_slot_unrealize;
}

static const TypeInfo cpu_slot_type_info = {
    .name = TYPE_CPU_SLOT,
    .parent = TYPE_SYS_BUS_DEVICE,
    .class_init = cpu_slot_class_init,
    .instance_size = sizeof(CPUSlot),
};

static void cpu_slot_register_types(void)
{
    type_register_static(&cpu_slot_type_info);
}

type_init(cpu_slot_register_types)

void machine_plug_cpu_slot(MachineState *ms)
{
    MachineClass *mc = MACHINE_GET_CLASS(ms);
    CPUSlot *slot;

    slot = CPU_SLOT(qdev_new(TYPE_CPU_SLOT));
    set_bit(CPU_TOPOLOGY_LEVEL_THREAD, slot->supported_levels);
    set_bit(CPU_TOPOLOGY_LEVEL_CORE, slot->supported_levels);
    set_bit(CPU_TOPOLOGY_LEVEL_SOCKET, slot->supported_levels);

    /*
     * Now just consider the levels that x86 supports.
     * TODO: Supports other levels.
     */
    if (mc->smp_props.modules_supported) {
        set_bit(CPU_TOPOLOGY_LEVEL_MODULE, slot->supported_levels);
    }

    if (mc->smp_props.dies_supported) {
        set_bit(CPU_TOPOLOGY_LEVEL_DIE, slot->supported_levels);
    }

    ms->topo = slot;
    object_property_add_child(container_get(OBJECT(ms), "/peripheral"),
                              "cpu-slot", OBJECT(ms->topo));
    DEVICE(ms->topo)->id = g_strdup_printf("%s", "cpu-slot");

    sysbus_realize(SYS_BUS_DEVICE(slot), &error_abort);

    if (mc->get_hotplug_handler) {
        qbus_set_hotplug_handler(BUS(&slot->bus), OBJECT(ms));
    }
}
