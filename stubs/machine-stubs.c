/*
 * Machine stubs
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

bool machine_parse_custom_topo_config(MachineState *ms,
                                      const SMPConfiguration *config,
                                      Error **errp)
{
    return true;
}
