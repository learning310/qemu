/*
 * CPU socket abstract device
 *
 * Copyright (C) 2024 Intel Corporation.
 *
 * Author: Zhao Liu <zhao1.liu@intel.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * later.  See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "hw/cpu/socket.h"

static void cpu_socket_class_init(ObjectClass *oc, void *data)
{
    CPUTopoClass *tc = CPU_TOPO_CLASS(oc);

    tc->level = CPU_TOPOLOGY_LEVEL_SOCKET;
}

static const TypeInfo cpu_socket_type_info = {
    .name = TYPE_CPU_SOCKET,
    .parent = TYPE_CPU_TOPO,
    .class_init = cpu_socket_class_init,
    .instance_size = sizeof(CPUSocket),
};

static void cpu_socket_register_types(void)
{
    type_register_static(&cpu_socket_type_info);
}

type_init(cpu_socket_register_types)
