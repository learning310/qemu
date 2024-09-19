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

#ifndef CPU_TOPO_H
#define CPU_TOPO_H

#include "hw/qdev-core.h"
#include "qapi/qapi-types-machine-common.h"
#include "qom/object.h"

#define TYPE_CPU_BUS "cpu-bus"
OBJECT_DECLARE_SIMPLE_TYPE(CPUBusState, CPU_BUS)

/**
 * CPUBusState:
 * @check_topology: Method to check if @topo is supported by @cbus.
 */
struct CPUBusState {
    /*< private >*/
    BusState parent_obj;

    /*< public >*/
    bool (*check_topology)(CPUBusState *cbus, CPUTopoState *topo,
                           Error **errp);
};

#define TYPE_CPU_TOPO "cpu-topo"
OBJECT_DECLARE_TYPE(CPUTopoState, CPUTopoClass, CPU_TOPO)

/**
 * CPUTopoClass:
 * @level: Topology level for this CPUTopoClass.
 */
struct CPUTopoClass {
    /*< private >*/
    DeviceClass parent_class;

    /*< public >*/
    CpuTopologyLevel level;
};

/**
 * CPUTopoState:
 * @parent: Topology parent of this topology device.
 * @bus: The CPU bus to add the children device.
 */
struct CPUTopoState {
    /*< private >*/
    DeviceState parent_obj;

    /*< public >*/
    struct CPUTopoState *parent;
    CPUBusState *bus;
};

#define GET_CPU_TOPO_LEVEL(topo)    (CPU_TOPO_GET_CLASS(topo)->level)

int cpu_topo_get_instances_num(CPUTopoState *topo);

#endif /* CPU_TOPO_H */
