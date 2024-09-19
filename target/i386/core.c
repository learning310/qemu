/*
 * x86 CPU core
 *
 * Copyright (C) 2024 Intel Corporation.
 *
 * Author: Zhao Liu <zhao1.liu@intel.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * later.  See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "core.h"

static void x86_common_core_class_init(ObjectClass *oc, void *data)
{
    X86CPUCoreClass *cc = X86_CPU_CORE_CLASS(oc);

    cc->core_type = COMMON_CORE;
}

static void x86_intel_atom_class_init(ObjectClass *oc, void *data)
{
    X86CPUCoreClass *cc = X86_CPU_CORE_CLASS(oc);

    cc->core_type = INTEL_ATOM;
}

static void x86_intel_core_class_init(ObjectClass *oc, void *data)
{
    X86CPUCoreClass *cc = X86_CPU_CORE_CLASS(oc);

    cc->core_type = INTEL_CORE;
}

static const TypeInfo x86_cpu_core_infos[] = {
    {
        .name = TYPE_X86_CPU_CORE,
        .parent = TYPE_CPU_CORE,
        .class_size = sizeof(X86CPUCoreClass),
        .class_init = x86_common_core_class_init,
        .instance_size = sizeof(X86CPUCore),
    },
    {
        .parent = TYPE_X86_CPU_CORE,
        .name = X86_CPU_CORE_TYPE_NAME("intel-atom"),
        .class_init = x86_intel_atom_class_init,
    },
    {
        .parent = TYPE_X86_CPU_CORE,
        .name = X86_CPU_CORE_TYPE_NAME("intel-core"),
        .class_init = x86_intel_core_class_init,
    },
};

DEFINE_TYPES(x86_cpu_core_infos)
