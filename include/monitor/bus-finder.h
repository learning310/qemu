/*
 * Bus finder interface header
 *
 * Copyright (C) 2024 Intel Corporation.
 *
 * Author: Zhao Liu <zhao1.liu@intel.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * later.  See the COPYING file in the top-level directory.
 */

#ifndef BUS_FINDER_H
#define BUS_FINDER_H

#include "hw/qdev-core.h"
#include "qom/object.h"

#define TYPE_BUS_FINDER "bus-finder"

typedef struct BusFinderClass BusFinderClass;
DECLARE_CLASS_CHECKERS(BusFinderClass, BUS_FINDER, TYPE_BUS_FINDER)
#define BUS_FINDER(obj) INTERFACE_CHECK(BusFinder, (obj), TYPE_BUS_FINDER)

typedef struct BusFinder BusFinder;

/**
 * BusFinderClass:
 * @find_bus: Method to find bus.
 */
struct BusFinderClass {
    /* <private> */
    InterfaceClass parent_class;

    /* <public> */
    BusState *(*find_bus)(DeviceState *dev);
};

bool is_bus_finder_type(DeviceClass *dc);
BusState *bus_finder_select_bus(DeviceState *dev);

#endif /* BUS_FINDER_H */
