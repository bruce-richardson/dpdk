/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 HiSilicon Limited.
 */

#ifndef _RTE_DMADEV_PMD_H_
#define _RTE_DMADEV_PMD_H_

/** @file
 * RTE DMA PMD APIs
 *
 * @note
 * Driver facing APIs for a DMA device. These are not to be called directly by
 * any application.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

#include <rte_dev.h>
#include <rte_log.h>
#include <rte_common.h>

#include "rte_dmadev.h"

extern int libdmadev_logtype;

#define RTE_DMADEV_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, libdmadev_logtype, "%s(): " fmt "\n", \
		__func__, ##args)

/* Macros to check for valid device */
#define RTE_DMADEV_VALID_DEVID_OR_ERR_RET(dev_id, retval) do { \
	if (!rte_dmadev_pmd_is_valid_dev((dev_id))) { \
		RTE_DMADEV_LOG(ERR, "Invalid dev_id=%d", dev_id); \
		return retval; \
	} \
} while (0)

#define RTE_DMADEV_VALID_DEVID_OR_RET(dev_id) do { \
	if (!rte_dmadev_pmd_is_valid_dev((dev_id))) { \
		RTE_DMADEV_LOG(ERR, "Invalid dev_id=%d", dev_id); \
		return; \
	} \
} while (0)

#define RTE_DMADEV_DETACHED  0
#define RTE_DMADEV_ATTACHED  1

/**
 * Validate if the DMA device index is a valid attached DMA device.
 *
 * @param dev_id
 *   DMA device index.
 *
 * @return
 *   - If the device index is valid (1) or not (0).
 */
static inline unsigned
rte_dmadev_pmd_is_valid_dev(uint16_t dev_id)
{
	struct rte_dmadev *dev;

	if (dev_id >= RTE_DMADEV_MAX_DEVS)
		return 0;

	dev = &rte_dmadevices[dev_id];
	if (dev->attached != RTE_DMADEV_ATTACHED)
		return 0;
	else
		return 1;
}

/**
 * Definitions of control-plane functions exported by a driver through the
 * generic structure of type *rte_dmadev_ops* supplied in the *rte_dmadev*
 * structure associated with a device.
 */

typedef int (*dmadev_info_get_t)(struct rte_dmadev *dev,
				 struct rte_dmadev_info *dev_info);
/**< @internal Function used to get device information of a device. */

typedef int (*dmadev_configure_t)(struct rte_dmadev *dev,
				  const struct rte_dmadev_conf *dev_conf);
/**< @internal Function used to configure a device. */

typedef int (*dmadev_start_t)(struct rte_dmadev *dev);
/**< @internal Function used to start a configured device. */

typedef int (*dmadev_stop_t)(struct rte_dmadev *dev);
/**< @internal Function used to stop a configured device. */

typedef int (*dmadev_close_t)(struct rte_dmadev *dev);
/**< @internal Function used to close a configured device. */

typedef int (*dmadev_reset_t)(struct rte_dmadev *dev);
/**< @internal Function used to reset a configured device. */

typedef int (*dmadev_queue_setup_t)(struct rte_dmadev *dev,
				    const struct rte_dmadev_queue_conf *conf);
/**< @internal Function used to allocate and set up a virt queue. */

typedef int (*dmadev_queue_release_t)(struct rte_dmadev *dev, uint16_t vq_id);
/**< @internal Function used to release a virt queue. */

typedef int (*dmadev_queue_info_t)(struct rte_dmadev *dev, uint16_t vq_id,
				   struct rte_dmadev_queue_info *info);
/**< @internal Function used to retrieve information of a virt queue. */

typedef int (*dmadev_stats_get_t)(struct rte_dmadev *dev, int vq_id,
				  struct rte_dmadev_stats *stats);
/**< @internal Function used to retrieve basic statistics. */

typedef int (*dmadev_stats_reset_t)(struct rte_dmadev *dev, int vq_id);
/**< @internal Function used to reset basic statistics. */

typedef int (*dmadev_xstats_get_names_t)(const struct rte_dmadev *dev,
		struct rte_dmadev_xstats_name *xstats_names,
		uint32_t size);
/**< @internal Function used to get names of extended stats. */

typedef int (*dmadev_xstats_get_t)(const struct rte_dmadev *dev,
		const uint32_t ids[], uint64_t values[], uint32_t n);
/**< @internal Function used to retrieve extended stats. */

typedef int (*dmadev_xstats_reset_t)(struct rte_dmadev *dev,
				     const uint32_t ids[], uint32_t nb_ids);
/**< @internal Function used to reset extended stats. */

typedef int (*dmadev_selftest_t)(uint16_t dev_id);
/**< @internal Function used to start dmadev selftest. */

/** DMA device operations function pointer table */
struct rte_dmadev_ops {
	/**< Get device info. */
	dmadev_info_get_t dev_info_get;
	/**< Configure device. */
	dmadev_configure_t dev_configure;
	/**< Start device. */
	dmadev_start_t dev_start;
	/**< Stop device. */
	dmadev_stop_t dev_stop;
	/**< Close device. */
	dmadev_close_t dev_close;
	/**< Reset device. */
	dmadev_reset_t dev_reset;

	/**< Allocate and set up a virt queue. */
	dmadev_queue_setup_t queue_setup;
	/**< Release a virt queue. */
	dmadev_queue_release_t queue_release;
	/**< Retrieve information of a virt queue */
	dmadev_queue_info_t queue_info_get;

	/**< Get basic statistics. */
	dmadev_stats_get_t stats_get;
	/**< Reset basic statistics. */
	dmadev_stats_reset_t stats_reset;
	/**< Get names of extended stats. */
	dmadev_xstats_get_names_t xstats_get_names;
	/**< Get extended statistics. */
	dmadev_xstats_get_t xstats_get;
	/**< Reset extended statistics values. */
	dmadev_xstats_reset_t xstats_reset;

	/**< Device selftest function */
	dmadev_selftest_t dev_selftest;
};

/**
 * Allocates a new dmadev slot for an DMA device and returns the pointer
 * to that slot for the driver to use.
 *
 * @param name
 *   Unique identifier name for each device
 * @param dev_private_size
 *   Size of private data memory allocated within rte_dmadev object.
 *   Set to 0 to disable internal memory allocation and allow for
 *   self-allocation.
 * @param socket_id
 *   Socket to allocate resources on.
 *
 * @return
 *   - NULL: Failure to allocate
 *   - Other: The rte_dmadev structure pointer for the new device
 */
struct rte_dmadev *
rte_dmadev_pmd_allocate(const char *name, size_t dev_private_size,
			int socket_id);

/**
 * Release the specified dmadev device.
 *
 * @param dev
 *   The *dmadev* pointer is the address of the *rte_dmadev* structure.
 *
 * @return
 *   - 0 on success, negative on error
 */
int
rte_dmadev_pmd_release(struct rte_dmadev *dev);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_DMADEV_PMD_H_ */
