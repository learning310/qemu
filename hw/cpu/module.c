/*
 * CPU module abstract device
 *
 * Copyright (C) 2024 Intel Corporation.
 *
 * Author: Zhao Liu <zhao1.liu@intel.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * later.  See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "hw/cpu/module.h"

static void cpu_module_class_init(ObjectClass *oc, void *data)
{
    CPUTopoClass *tc = CPU_TOPO_CLASS(oc);

    tc->level = CPU_TOPOLOGY_LEVEL_MODULE;
}

static const TypeInfo cpu_module_type_info = {
    .name = TYPE_CPU_MODULE,
    .parent = TYPE_CPU_TOPO,
    .class_init = cpu_module_class_init,
    .instance_size = sizeof(CPUModule),
};

static void cpu_module_register_types(void)
{
    type_register_static(&cpu_module_type_info);
}

type_init(cpu_module_register_types)
