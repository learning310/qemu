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

#ifndef HW_CPU_SOCKET_H
#define HW_CPU_SOCKET_H

#include "hw/cpu/cpu-topology.h"
#include "hw/qdev-core.h"

#define TYPE_CPU_SOCKET "cpu-socket"

OBJECT_DECLARE_SIMPLE_TYPE(CPUSocket, CPU_SOCKET)

struct CPUSocket {
    /*< private >*/
    CPUTopoState parent_obj;

    /*< public >*/
};

#endif /* HW_CPU_SOCKET_H */
