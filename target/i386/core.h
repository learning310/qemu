/*
 * x86 CPU core header
 *
 * Copyright (C) 2024 Intel Corporation.
 *
 * Author: Zhao Liu <zhao1.liu@intel.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * later.  See the COPYING file in the top-level directory.
 */

#include "hw/cpu/core.h"
#include "hw/cpu/cpu-topology.h"
#include "qom/object.h"

#ifndef I386_CORE_H
#define I386_CORE_H

#ifdef TARGET_X86_64
#define TYPE_X86_PREFIX "x86-"
#else
#define TYPE_X86_PREFIX "i386-"
#endif

#define TYPE_X86_CPU_CORE TYPE_X86_PREFIX "core"

OBJECT_DECLARE_TYPE(X86CPUCore, X86CPUCoreClass, X86_CPU_CORE)

typedef enum {
    COMMON_CORE = 0,
    INTEL_ATOM,
    INTEL_CORE,
} X86CoreType;

struct X86CPUCoreClass {
    /*< private >*/
    CPUTopoClass parent_class;

    /*< public >*/
    DeviceRealize parent_realize;
    X86CoreType core_type;
};

struct X86CPUCore {
    /*< private >*/
    CPUCore parent_obj;

    /*< public >*/
};

#define X86_CPU_CORE_TYPE_NAME(core_type_str) (TYPE_X86_PREFIX core_type_str)

#endif /* I386_CORE_H */
