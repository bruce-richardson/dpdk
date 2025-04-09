/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Intel Corporation
 */

#include <rte_lcore.h>

unsigned int
dpdk_lcore_id()
{
	return rte_lcore_id();
}
