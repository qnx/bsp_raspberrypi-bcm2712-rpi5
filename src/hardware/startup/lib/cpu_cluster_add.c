/*
 * Copyright 2022, BlackBerry Limited.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You
 * may not reproduce, modify or distribute this software except in
 * compliance with the License. You may obtain a copy of the License
 * at: http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 *
 * This file may contain contributions from others, either as
 * contributors under the License or as licensors under other terms.
 * Please review this entire file for other proprietary rights or license
 * notices, as well as the QNX Development Suite License Guide at
 * http://licensing.qnx.com/license-guide/ for other information.
 * $
 */

#include <startup.h>

/**
 * Registers a processor cluster for the kernel's scheduler.
 * The cluster is specified in terms of a bit mask, with bit N set if and only
 * if processor N belongs to the cluster.
 * While any arbitrary subset of processors can belong to a cluster, it is
 * expected that scheduling clusters correspond to the hardware topology of the
 * system (e.g., to take advantage of cache locality or of processors with
 * different power characteristics).
 * @param   name    A symbolic name for that cluster
 * @param   cpumask A bit mask of processors participating in the cluster
 * @return  1 if successful, 0 otherwise
 */
int
cpu_cluster_add(const char * const name, uint64_t const cpumask)
{
    // Users may not create names in the system cluster name space
    if (*name == SYSTEM_CLUSTER_NAME_PREFIX) return 0;

    if (cpumask == 0) return 0;
    // The total number of CPUs in the system is not know at this point

    // Add the name to the string table
    unsigned const name_off = add_string(name);

    struct cluster_entry * const cluster_array = lsp.cluster.p;
    size_t const num_clusters = lsp.cluster.size / sizeof (*cluster_array);

    size_t i;
    for (i = 0; i < num_clusters; ++i) {
        if (cluster_array[i].name == name_off) break;
    }

    if (i == num_clusters) {
        grow_syspage_section(&lsp.cluster, sizeof(*cluster_array));
    }

    cluster_array[i].cpumask = cpumask;
    cluster_array[i].name = (_Uint16t)name_off;
    return 1;
}
