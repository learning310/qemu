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

#ifndef HW_CPU_MODULE_H
#define HW_CPU_MODULE_H

#include "hw/cpu/cpu-topology.h"
#include "hw/qdev-core.h"

#define TYPE_CPU_MODULE "cpu-module"

OBJECT_DECLARE_SIMPLE_TYPE(CPUModule, CPU_MODULE)

struct CPUModule {
    /*< private >*/
    CPUTopoState obj;

    /*< public >*/
};

#endif /* HW_CPU_MODULE_H */
