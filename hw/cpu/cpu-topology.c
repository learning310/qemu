/*
 * General CPU topology device abstraction
 *
 * Copyright (C) 2024 Intel Corporation.
 *
 * Author: Zhao Liu <zhao1.liu@intel.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * later.  See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"

#include "hw/cpu/cpu-topology.h"
#include "hw/qdev-core.h"
#include "hw/qdev-properties.h"
#include "hw/sysbus.h"
#include "qapi/error.h"

/* Roll up until topology root to check. */
static bool cpu_parent_check_topology(DeviceState *parent,
                                      DeviceState *dev,
                                      Error **errp)
{
    BusClass *bc;

    if (!parent || !parent->parent_bus ||
        object_dynamic_cast(OBJECT(parent->parent_bus), TYPE_CPU_BUS)) {
        return true;
    }

    bc = BUS_GET_CLASS(parent->parent_bus);
    if (bc->check_address) {
        return bc->check_address(parent->parent_bus, dev, errp);
    }

    return true;
}

static bool cpu_bus_check_address(BusState *bus, DeviceState *dev,
                                  Error **errp)
{
    CPUBusState *cbus = CPU_BUS(bus);

    if (cbus->check_topology) {
        return cbus->check_topology(CPU_BUS(bus), CPU_TOPO(dev), errp);
    }

    return cpu_parent_check_topology(bus->parent, dev, errp);
}

static void cpu_bus_class_init(ObjectClass *oc, void *data)
{
    BusClass *bc = BUS_CLASS(oc);

    bc->check_address = cpu_bus_check_address;
}

static const TypeInfo cpu_bus_type_info = {
    .name = TYPE_CPU_BUS,
    .parent = TYPE_BUS,
    .class_init = cpu_bus_class_init,
    .instance_size = sizeof(CPUBusState),
};

static bool cpu_topo_set_parent(CPUTopoState *topo, Error **errp)
{
    DeviceState *dev = DEVICE(topo);
    BusState *bus = dev->parent_bus;
    CPUTopoState *parent_topo = NULL;
    Object *parent;

    if (!bus || !bus->parent) {
        return true;
    }

    if (topo->parent) {
        error_setg(errp, "cpu topo: %s already have the parent?",
                   object_get_typename(OBJECT(topo)));
        return false;
    }

    parent = OBJECT(bus->parent);
    if (object_dynamic_cast(parent, TYPE_CPU_TOPO)) {
        parent_topo = CPU_TOPO(parent);

        if (GET_CPU_TOPO_LEVEL(topo) >= GET_CPU_TOPO_LEVEL(parent_topo)) {
            error_setg(errp, "cpu topo: current level (%s) should be "
                       "lower than parent (%s) level",
                       object_get_typename(OBJECT(topo)),
                       object_get_typename(parent));
            return false;
        }
    }

    if (dev->id) {
        /*
         * Reparent topology device to make child<> match topological
         * relationship.
         */
        if (!qdev_set_parent(dev, bus, parent, NULL, errp)) {
            return false;
        }
    }

    topo->parent = parent_topo;
    return true;
}

static void cpu_topo_realize(DeviceState *dev, Error **errp)
{
    CPUTopoState *topo = CPU_TOPO(dev);
    CPUTopoClass *tc = CPU_TOPO_GET_CLASS(topo);
    HotplugHandler *hotplug_handler;

    if (tc->level == CPU_TOPOLOGY_LEVEL_INVALID) {
        error_setg(errp, "cpu topo: no level specified type: %s",
                   object_get_typename(OBJECT(dev)));
        return;
    }

    if (!cpu_topo_set_parent(topo, errp)) {
        return;
    }

    topo->bus = CPU_BUS(qbus_new(TYPE_CPU_BUS, dev, dev->id));
    hotplug_handler = qdev_get_bus_hotplug_handler(dev);
    if (hotplug_handler) {
        qbus_set_hotplug_handler(BUS(topo->bus), OBJECT(hotplug_handler));
    }
}

static void cpu_topo_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    CPUTopoClass *tc = CPU_TOPO_CLASS(oc);

    set_bit(DEVICE_CATEGORY_CPU, dc->categories);
    dc->realize = cpu_topo_realize;

    /*
     * If people doesn't want a topology tree, it's necessary to
     * derive a child class and override this as NULL.
     */
    dc->bus_type = TYPE_CPU_BUS;

    /*
     * The general topo device is not hotpluggable by default.
     * If any topo device needs hotplug support, this flag must be
     * overridden.
     */
    dc->hotpluggable = false;

    tc->level = CPU_TOPOLOGY_LEVEL_INVALID;
}

static const TypeInfo cpu_topo_type_info = {
    .name = TYPE_CPU_TOPO,
    .parent = TYPE_DEVICE,
    .abstract = true,
    .class_size = sizeof(CPUTopoClass),
    .class_init = cpu_topo_class_init,
    .instance_size = sizeof(CPUTopoState),
};

static void cpu_topo_register_types(void)
{
    type_register_static(&cpu_bus_type_info);
    type_register_static(&cpu_topo_type_info);
}

type_init(cpu_topo_register_types)

int cpu_topo_get_instances_num(CPUTopoState *topo)
{
    BusState *bus = DEVICE(topo)->parent_bus;

    return bus ? bus->num_children : 1;
}
