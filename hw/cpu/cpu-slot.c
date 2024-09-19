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
#include "hw/cpu/core.h"
#include "hw/cpu/cpu-slot.h"
#include "hw/cpu/cpu-topology.h"
#include "hw/cpu/die.h"
#include "hw/cpu/module.h"
#include "hw/cpu/socket.h"
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
    int max_children;

    if (!object_dynamic_cast(OBJECT(dev), TYPE_CPU_TOPO)) {
        return;
    }

    topo = CPU_TOPO(dev);
    cpu_slot_add_topo_info(slot, topo);

    if (dev->parent_bus) {
        max_children = slot->stat.entries[GET_CPU_TOPO_LEVEL(topo)].max_limit;
        if (dev->parent_bus->num_children == max_children) {
            qbus_mark_full(dev->parent_bus);
        }
    }
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

    if (dev->parent_bus) {
        qbus_mask_full(dev->parent_bus);
    }
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

    /* Initizlize max_limit to 1, as members of CpuTopology. */
    for (int i = 0; i < CPU_TOPOLOGY_LEVEL__MAX; i++) {
        slot->stat.entries[i].max_limit = 1;
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

static int get_smp_info_by_level(const CpuTopology *smp_info,
                                 CpuTopologyLevel child_level)
{
    switch (child_level) {
    case CPU_TOPOLOGY_LEVEL_THREAD:
        return smp_info->threads;
    case CPU_TOPOLOGY_LEVEL_CORE:
        return smp_info->cores;
    case CPU_TOPOLOGY_LEVEL_MODULE:
        return smp_info->modules;
    case CPU_TOPOLOGY_LEVEL_CLUSTER:
        return smp_info->clusters;
    case CPU_TOPOLOGY_LEVEL_DIE:
        return smp_info->dies;
    case CPU_TOPOLOGY_LEVEL_SOCKET:
        return smp_info->sockets;
    default:
        /* TODO: Add support for other levels. */
        g_assert_not_reached();
    }

    return 0;
}

static const char *get_topo_typename_by_level(CpuTopologyLevel level)
{
    switch (level) {
    case CPU_TOPOLOGY_LEVEL_CORE:
        return TYPE_CPU_CORE;
    case CPU_TOPOLOGY_LEVEL_MODULE:
        return TYPE_CPU_MODULE;
    case CPU_TOPOLOGY_LEVEL_DIE:
        return TYPE_CPU_DIE;
    case CPU_TOPOLOGY_LEVEL_SOCKET:
        return TYPE_CPU_SOCKET;
    default:
        /* TODO: Add support for other levels. */
        g_assert_not_reached();
    }

    return NULL;
}

typedef struct SMPBuildCbData {
    DECLARE_BITMAP(create_levels, CPU_TOPOLOGY_LEVEL__MAX);
    const CpuTopology *smp_info;
    CPUTopoStat *stat;
    Error **errp;
} SMPBuildCbData;

static int create_smp_topo_children(DeviceState *dev, void *opaque)
{
    Object *parent = OBJECT(dev);
    CpuTopologyLevel child_level;
    SMPBuildCbData *cb = opaque;
    CPUTopoState *topo = NULL;
    BusState *qbus;
    CPUBusState *cbus;
    Error **errp = cb->errp;
    int max_children;

    if (object_dynamic_cast(parent, TYPE_CPU_TOPO)) {
        topo = CPU_TOPO(parent);
        CpuTopologyLevel parent_level;

        parent_level = GET_CPU_TOPO_LEVEL(topo);
        child_level = find_last_bit(cb->create_levels, parent_level);

        if (child_level == parent_level) {
            return TOPO_FOREACH_CONTINUE;
        }

        cbus = topo->bus;
    } else if (object_dynamic_cast(parent, TYPE_CPU_SLOT)) {
        child_level = find_last_bit(cb->create_levels, CPU_TOPOLOGY_LEVEL__MAX);
        cbus = &CPU_SLOT(parent)->bus;
    } else {
        return TOPO_FOREACH_ERR;
    }

    qbus = BUS(cbus);
    max_children = get_smp_info_by_level(cb->smp_info, child_level);
    for (int i = 0; i < max_children; i++) {
        DeviceState *child;

        child = qdev_new(get_topo_typename_by_level(child_level));

        /*
         * Bus inserts child device at head (QTAILQ_INSERT_HEAD_RCU), This
         * could result in the device IDs in the created topology having a
         * zig-zag arrangement.
         *
         * TODO: Remove obstacles preventing the use of QTAILQ_INSERT_HEAD_RCU
         * for bus to insert kid device.
         */
        child->id = g_strdup_printf("%s[%d]",
            CpuTopologyLevel_str(child_level),
            cb->stat->entries[child_level].total_instances);

        if (!qdev_realize_and_unref(child, qbus, errp)) {
            return TOPO_FOREACH_ERR;
        }
    }

    return TOPO_FOREACH_CONTINUE;
}

bool machine_create_topo_tree(MachineState *ms, Error **errp)
{
    MachineClass *mc = MACHINE_GET_CLASS(ms);
    CPUSlot *slot = ms->topo;
    CpuTopologyLevel level;
    SMPBuildCbData cb;

    if (!slot) {
        error_setg(errp, "Invalid machine: "
                   "the cpu-slot of machine is not initialized.");
        return false;
    }

    /* User will customize topology tree. */
    if (slot->custom_topo_enabled) {
        return true;
    }

    /*
     * Don't support full topology tree.
     * Just use slot to collect topology device.
     */
    if (!mc->smp_props.arch_id_topo_level) {
        return true;
    }

    bitmap_copy(cb.create_levels, slot->supported_levels,
                CPU_TOPOLOGY_LEVEL__MAX);
    cb.smp_info = &ms->smp;
    cb.stat = &slot->stat;
    cb.errp = errp;

    /*
     * Topology objects at arch_id_topo_level and lower levels will be
     * created by MachineClass.possible_cpu_arch_ids().
     */
    FOR_EACH_SET_BIT(level, slot->supported_levels,
                     mc->smp_props.arch_id_topo_level + 1) {
        clear_bit(level, cb.create_levels);
    }

    if (qdev_walk_children(DEVICE(slot), create_smp_topo_children,
                           NULL, NULL, NULL, &cb) < 0) {
        return false;
    }

    return true;
}

int get_max_topo_by_level(const MachineState *ms, CpuTopologyLevel level)
{
    if (!ms->topo || !ms->topo->custom_topo_enabled) {
        return get_smp_info_by_level(&ms->smp, level);
    }
    return ms->topo->stat.entries[level].max_limit;
}

unsigned int machine_topo_get_cores_per_socket(const MachineState *ms)
{
    int cores = 1, i;

    for (i = CPU_TOPOLOGY_LEVEL_CORE; i < CPU_TOPOLOGY_LEVEL_SOCKET; i++) {
        cores *= get_max_topo_by_level(ms, i);
    }
    return cores;
}

unsigned int machine_topo_get_threads_per_socket(const MachineState *ms)
{
    return get_max_topo_by_level(ms, CPU_TOPOLOGY_LEVEL_THREAD) *
           machine_topo_get_cores_per_socket(ms);
}

bool machine_parse_custom_topo_config(MachineState *ms,
                                      const SMPConfiguration *config,
                                      Error **errp)
{
    MachineClass *mc = MACHINE_GET_CLASS(ms);
    CPUSlot *slot = ms->topo;
    bool is_valid;
    int maxcpus;

    if (!slot) {
        return true;
    }

    is_valid = config->has_maxsockets && config->maxsockets;
    if (mc->smp_props.custom_topo_supported) {
        slot->stat.entries[CPU_TOPOLOGY_LEVEL_SOCKET].max_limit =
            is_valid ? config->maxsockets : ms->smp.sockets;
    } else if (is_valid) {
        error_setg(errp, "maxsockets > 0 not supported "
                   "by this machine's CPU topology");
        return false;
    } else {
        slot->stat.entries[CPU_TOPOLOGY_LEVEL_SOCKET].max_limit =
            ms->smp.sockets;
    }

    is_valid = config->has_maxdies && config->maxdies;
    if (mc->smp_props.custom_topo_supported &&
        mc->smp_props.dies_supported) {
        slot->stat.entries[CPU_TOPOLOGY_LEVEL_DIE].max_limit =
            is_valid ? config->maxdies : ms->smp.dies;
    } else if (is_valid) {
        error_setg(errp, "maxdies > 0 not supported "
                   "by this machine's CPU topology");
        return false;
    } else {
        slot->stat.entries[CPU_TOPOLOGY_LEVEL_DIE].max_limit =
            ms->smp.dies;
    }

    is_valid = config->has_maxmodules && config->maxmodules;
    if (mc->smp_props.custom_topo_supported &&
        mc->smp_props.modules_supported) {
        slot->stat.entries[CPU_TOPOLOGY_LEVEL_MODULE].max_limit =
            is_valid ? config->maxmodules : ms->smp.modules;
    } else if (is_valid) {
        error_setg(errp, "maxmodules > 0 not supported "
                   "by this machine's CPU topology");
        return false;
    } else {
        slot->stat.entries[CPU_TOPOLOGY_LEVEL_MODULE].max_limit =
            ms->smp.modules;
    }

    is_valid = config->has_maxcores && config->maxcores;
    if (mc->smp_props.custom_topo_supported) {
        slot->stat.entries[CPU_TOPOLOGY_LEVEL_CORE].max_limit =
            is_valid ? config->maxcores : ms->smp.cores;
    } else if (is_valid) {
        error_setg(errp, "maxcores > 0 not supported "
                   "by this machine's CPU topology");
        return false;
    } else {
        slot->stat.entries[CPU_TOPOLOGY_LEVEL_CORE].max_limit =
            ms->smp.cores;
    }

    is_valid = config->has_maxthreads && config->maxthreads;
    if (mc->smp_props.custom_topo_supported) {
        slot->stat.entries[CPU_TOPOLOGY_LEVEL_THREAD].max_limit =
            is_valid ? config->maxthreads : ms->smp.threads;
    } else if (is_valid) {
        error_setg(errp, "maxthreads > 0 not supported "
                   "by this machine's CPU topology");
        return false;
    } else {
        slot->stat.entries[CPU_TOPOLOGY_LEVEL_THREAD].max_limit =
            ms->smp.threads;
    }

    maxcpus = 1;
    /* Initizlize max_limit to 1, as members of CpuTopology. */
    for (int i = 0; i < CPU_TOPOLOGY_LEVEL__MAX; i++) {
        maxcpus *= slot->stat.entries[i].max_limit;
    }

    if (!config->has_maxcpus) {
        ms->smp.max_cpus = maxcpus;
    } else {
        if (maxcpus != ms->smp.max_cpus) {
            error_setg(errp, "maxcpus (%d) should be equal to "
                       "the product of the remaining max parameters (%d)",
                       ms->smp.max_cpus, maxcpus);
            return false;
        }
    }

    return true;
}

bool machine_validate_topo_tree(MachineState *ms, Error **errp)
{
    int cpus;

    if (!ms->topo || !ms->topo->custom_topo_enabled) {
        return true;
    }

    cpus = ms->topo->stat.entries[CPU_TOPOLOGY_LEVEL_THREAD].total_instances;
    if (cpus < ms->smp.cpus) {
        error_setg(errp, "machine requires at least %d online CPUs, "
                   "but currently only %d CPUs",
                   ms->smp.cpus, cpus);
        return false;
    }

    /* TODO: Add checks for other levels to honor more -smp parameters. */
    return true;
}
