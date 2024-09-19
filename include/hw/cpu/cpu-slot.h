/*
 * CPU slot abstraction header
 *
 * Copyright (C) 2024 Intel Corporation.
 *
 * Author: Zhao Liu <zhao1.liu@intel.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * later.  See the COPYING file in the top-level directory.
 */

#ifndef CPU_SLOT_H
#define CPU_SLOT_H

#include "hw/cpu/cpu-topology.h"
#include "hw/qdev-core.h"
#include "hw/sysbus.h"
#include "qapi/qapi-types-machine-common.h"
#include "qom/object.h"

/**
 * CPUTopoStatEntry:
 * @total_instances: Total number of topological instances at the same level
 *                   that are currently inserted in CPU slot
 * @max_instances: Maximum number of topological instances at the same level
 *                 under the parent topological container
 * @max_limit: Maximum limitation of topological instances at the same level
 *             under the parent topological container
 */
typedef struct CPUTopoStatEntry {
    int total_instances;
    int max_instances;
    int max_limit;
} CPUTopoStatEntry;

/**
 * CPUTopoStat:
 * @entries: Detail count information for valid topology levels under
 *           CPU slot
 * @curr_levels: Current CPU topology levels inserted in CPU slot
 */
typedef struct CPUTopoStat {
    /* TODO: Exclude invalid and default levels. */
    CPUTopoStatEntry entries[CPU_TOPOLOGY_LEVEL__MAX];
    DECLARE_BITMAP(curr_levels, CPU_TOPOLOGY_LEVEL__MAX);
} CPUTopoStat;

#define TYPE_CPU_SLOT "cpu-slot"
OBJECT_DECLARE_SIMPLE_TYPE(CPUSlot, CPU_SLOT)

/**
 * CPUSlot:
 * @cores: Queue consisting of all the cores in the topology tree
 *     where the cpu-slot is the root. cpu-slot can maintain similar
 *     queues for other topology levels to facilitate traversal
 *     when necessary.
 * @stat: Topological statistics for topology tree.
 * @bus: CPU bus to add the children topology device.
 * @supported_levels: Supported topology levels for topology tree.
 * @custom_topo_enabled: Whether user to create custom topology tree.
 * @listener: Hooks to listen realize() and unrealize() of topology
 *            device.
 */
struct CPUSlot {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    CPUBusState bus;
    CPUTopoStat stat;
    DECLARE_BITMAP(supported_levels, CPU_TOPOLOGY_LEVEL__MAX);
    bool custom_topo_enabled;

    DeviceListener listener;
};

#define TOPO_FOREACH_END             1
#define TOPO_FOREACH_CONTINUE        0
#define TOPO_FOREACH_ERR             -1

void machine_plug_cpu_slot(MachineState *ms);
bool machine_create_topo_tree(MachineState *ms, Error **errp);
int get_max_topo_by_level(const MachineState *ms, CpuTopologyLevel level);
bool machine_parse_custom_topo_config(MachineState *ms,
                                      const SMPConfiguration *config,
                                      Error **errp);

#endif /* CPU_SLOT_H */
