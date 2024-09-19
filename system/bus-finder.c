/*
 * Bus finder interface
 *
 * Copyright (C) 2024 Intel Corporation.
 *
 * Author: Zhao Liu <zhao1.liu@intel.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * later.  See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"

#include "hw/qdev-core.h"
#include "monitor/bus-finder.h"
#include "qom/object.h"

bool is_bus_finder_type(DeviceClass *dc)
{
    return !!object_class_dynamic_cast(OBJECT_CLASS(dc), TYPE_BUS_FINDER);
}

BusState *bus_finder_select_bus(DeviceState *dev)
{
    BusFinder *bf = BUS_FINDER(dev);
    BusFinderClass *bfc = BUS_FINDER_GET_CLASS(bf);

    if (bfc->find_bus) {
        return bfc->find_bus(dev);
    }

    return NULL;
}

static const TypeInfo bus_finder_interface_info = {
    .name          = TYPE_BUS_FINDER,
    .parent        = TYPE_INTERFACE,
    .class_size = sizeof(BusFinderClass),
};

static void bus_finder_register_types(void)
{
    type_register_static(&bus_finder_interface_info);
}

type_init(bus_finder_register_types)
