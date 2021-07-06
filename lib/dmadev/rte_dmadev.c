/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 HiSilicon Limited.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <rte_log.h>
#include <rte_debug.h>
#include <rte_dev.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_errno.h>
#include <rte_string_fns.h>

#include "rte_dmadev.h"
#include "rte_dmadev_pmd.h"

struct rte_dmadev rte_dmadevices[RTE_DMADEV_MAX_DEVS];

uint16_t
rte_dmadev_count(void)
{
	uint16_t count = 0;
	uint16_t i;

	for (i = 0; i < RTE_DMADEV_MAX_DEVS; i++) {
		if (rte_dmadevices[i].attached)
			count++;
	}

	return count;
}

int
rte_dmadev_get_dev_id(const char *name)
{
	uint16_t i;

	if (name == NULL)
		return -EINVAL;

	for (i = 0; i < RTE_DMADEV_MAX_DEVS; i++)
		if ((strcmp(rte_dmadevices[i].name, name) == 0) &&
		    (rte_dmadevices[i].attached == RTE_DMADEV_ATTACHED))
			return i;

	return -ENODEV;
}

int
rte_dmadev_socket_id(uint16_t dev_id)
{
	struct rte_dmadev *dev;

	RTE_DMADEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_dmadevices[dev_id];

	return dev->socket_id;
}

int
rte_dmadev_info_get(uint16_t dev_id, struct rte_dmadev_info *dev_info)
{
	struct rte_dmadev *dev;
	int diag;

	RTE_DMADEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	RTE_FUNC_PTR_OR_ERR_RET(dev_info, -EINVAL);

	dev = &rte_dmadevices[dev_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_info_get, -ENOTSUP);

	memset(dev_info, 0, sizeof(struct rte_dmadev_info));
	diag = (*dev->dev_ops->dev_info_get)(dev, dev_info);
	if (diag != 0)
		return diag;

	dev_info->device = dev->device;
	dev_info->driver_name = dev->driver_name;
	dev_info->socket_id = dev->socket_id;

	return 0;
}

int
rte_dmadev_configure(uint16_t dev_id, const struct rte_dmadev_conf *dev_conf)
{
	struct rte_dmadev *dev;
	int diag;

	RTE_DMADEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	RTE_FUNC_PTR_OR_ERR_RET(dev_conf, -EINVAL);

	dev = &rte_dmadevices[dev_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_configure, -ENOTSUP);

	if (dev->started) {
		RTE_DMADEV_LOG(ERR,
		   "device %u must be stopped to allow configuration", dev_id);
		return -EBUSY;
	}

	diag = (*dev->dev_ops->dev_configure)(dev, dev_conf);
	if (diag != 0)
		RTE_DMADEV_LOG(ERR, "device %u dev_configure failed, ret = %d",
			       dev_id, diag);
	else
		dev->attached = 1;

	return diag;
}

int
rte_dmadev_start(uint16_t dev_id)
{
	struct rte_dmadev *dev;
	int diag;

	RTE_DMADEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);

	dev = &rte_dmadevices[dev_id];
	if (dev->started != 0) {
		RTE_DMADEV_LOG(ERR, "device %u already started", dev_id);
		return 0;
	}

	if (dev->dev_ops->dev_start == NULL)
		goto mark_started;

	diag = (*dev->dev_ops->dev_start)(dev);
	if (diag != 0)
		return diag;

mark_started:
	dev->started = 1;
	return 0;
}

int
rte_dmadev_stop(uint16_t dev_id)
{
	struct rte_dmadev *dev;
	int diag;

	RTE_DMADEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);

	dev = &rte_dmadevices[dev_id];

	if (dev->started == 0) {
		RTE_DMADEV_LOG(ERR, "device %u already stopped", dev_id);
		return 0;
	}

	if (dev->dev_ops->dev_stop == NULL)
		goto mark_stopped;

	diag = (*dev->dev_ops->dev_stop)(dev);
	if (diag != 0)
		return diag;

mark_stopped:
	dev->started = 0;
	return 0;
}

int
rte_dmadev_close(uint16_t dev_id)
{
	struct rte_dmadev *dev;

	RTE_DMADEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);

	dev = &rte_dmadevices[dev_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_close, -ENOTSUP);

	/* Device must be stopped before it can be closed */
	if (dev->started == 1) {
		RTE_DMADEV_LOG(ERR, "device %u must be stopped before closing",
			       dev_id);
		return -EBUSY;
	}

	return (*dev->dev_ops->dev_close)(dev);
}

int
rte_dmadev_reset(uint16_t dev_id)
{
	struct rte_dmadev *dev;

	RTE_DMADEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);

	dev = &rte_dmadevices[dev_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_reset, -ENOTSUP);

	/* Reset is not dependent on state of the device */
	return (*dev->dev_ops->dev_reset)(dev);
}

int
rte_dmadev_queue_setup(uint16_t dev_id,
		       const struct rte_dmadev_queue_conf *conf)
{
	struct rte_dmadev *dev;

	RTE_DMADEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	RTE_FUNC_PTR_OR_ERR_RET(conf, -EINVAL);

	dev = &rte_dmadevices[dev_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->queue_setup, -ENOTSUP);

	return (*dev->dev_ops->queue_setup)(dev, conf);
}

int
rte_dmadev_queue_release(uint16_t dev_id, uint16_t vq_id)
{
	struct rte_dmadev *dev;

	RTE_DMADEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);

	dev = &rte_dmadevices[dev_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->queue_release, -ENOTSUP);

	return (*dev->dev_ops->queue_release)(dev, vq_id);
}

int
rte_dmadev_queue_info_get(uint16_t dev_id, uint16_t vq_id,
			  struct rte_dmadev_queue_info *info)
{
	struct rte_dmadev *dev;

	RTE_DMADEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	RTE_FUNC_PTR_OR_ERR_RET(info, -EINVAL);

	dev = &rte_dmadevices[dev_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->queue_info_get, -ENOTSUP);

	memset(info, 0, sizeof(struct rte_dmadev_queue_info));
	return (*dev->dev_ops->queue_info_get)(dev, vq_id, info);
}

int
rte_dmadev_stats_get(uint16_t dev_id, int vq_id,
		     struct rte_dmadev_stats *stats)
{
	struct rte_dmadev *dev;

	RTE_DMADEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	RTE_FUNC_PTR_OR_ERR_RET(stats, -EINVAL);

	dev = &rte_dmadevices[dev_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->stats_get, -ENOTSUP);

	return (*dev->dev_ops->stats_get)(dev, vq_id, stats);
}

int
rte_dmadev_stats_reset(uint16_t dev_id, int vq_id)
{
	struct rte_dmadev *dev;

	RTE_DMADEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);

	dev = &rte_dmadevices[dev_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->stats_reset, -ENOTSUP);

	return (*dev->dev_ops->stats_reset)(dev, vq_id);
}

static int
xstats_get_count(uint16_t dev_id)
{
	struct rte_dmadev *dev = &rte_dmadevices[dev_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->xstats_get_names, -ENOTSUP);

	return (*dev->dev_ops->xstats_get_names)(dev, NULL, 0);
}

int
rte_dmadev_xstats_names_get(uint16_t dev_id,
			    struct rte_dmadev_xstats_name *xstats_names,
			    uint32_t size)
{
	struct rte_dmadev *dev;
	int cnt_expected_entries;

	RTE_DMADEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);

	cnt_expected_entries = xstats_get_count(dev_id);

	if (xstats_names == NULL || cnt_expected_entries < 0 ||
	    (int)size < cnt_expected_entries || size == 0)
		return cnt_expected_entries;

	dev = &rte_dmadevices[dev_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->xstats_get_names, -ENOTSUP);
	return (*dev->dev_ops->xstats_get_names)(dev, xstats_names, size);
}

int
rte_dmadev_xstats_get(uint16_t dev_id, const uint32_t ids[],
		      uint64_t values[], uint32_t n)
{
	struct rte_dmadev *dev;

	RTE_DMADEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);
	RTE_FUNC_PTR_OR_ERR_RET(ids, -EINVAL);
	RTE_FUNC_PTR_OR_ERR_RET(values, -EINVAL);

	dev = &rte_dmadevices[dev_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->xstats_get, -ENOTSUP);

	return (*dev->dev_ops->xstats_get)(dev, ids, values, n);
}

int
rte_dmadev_xstats_reset(uint16_t dev_id, const uint32_t ids[], uint32_t nb_ids)
{
	struct rte_dmadev *dev;

	RTE_DMADEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);

	dev = &rte_dmadevices[dev_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->xstats_reset, -ENOTSUP);

	return (*dev->dev_ops->xstats_reset)(dev, ids, nb_ids);
}

int
rte_dmadev_dump(uint16_t dev_id, FILE *f)
{
	struct rte_dmadev *dev = &rte_dmadevices[dev_id];

	RTE_DMADEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);

	fprintf(f, "DMA Dev %u, '%s' [%s]\n", dev->dev_id, dev->name,
			dev->started ? "started" : "stopped");
	fprintf(f, "  Driver: %s\n", dev->driver_name);
	fprintf(f, "  Socket Id: %d\n", dev->socket_id);

	if (dev->dev_ops->dump != NULL)
		return (*dev->dev_ops->dump)(dev, f);
	return 0;
}

int
rte_dmadev_selftest(uint16_t dev_id)
{
	struct rte_dmadev *dev;

	RTE_DMADEV_VALID_DEVID_OR_ERR_RET(dev_id, -EINVAL);

	dev = &rte_dmadevices[dev_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_selftest, -ENOTSUP);

	return (*dev->dev_ops->dev_selftest)(dev_id);
}

static inline uint16_t
rte_dmadev_find_free_device_index(void)
{
	uint16_t i;

	for (i = 0; i < RTE_DMADEV_MAX_DEVS; i++) {
		if (rte_dmadevices[i].attached == RTE_DMADEV_DETACHED)
			return i;
	}

	return RTE_DMADEV_MAX_DEVS;
}

__rte_experimental
struct rte_dmadev *
rte_dmadev_pmd_allocate(const char *name, size_t dev_priv_size, int socket_id)
{
	struct rte_dmadev *dev;
	uint16_t dev_id;

	if (rte_dmadev_get_dev_id(name) >= 0) {
		RTE_DMADEV_LOG(ERR,
			"device with name %s already allocated!", name);
		return NULL;
	}

	dev_id = rte_dmadev_find_free_device_index();
	if (dev_id == RTE_DMADEV_MAX_DEVS) {
		RTE_DMADEV_LOG(ERR, "reached maximum number of DMA devices");
		return NULL;
	}

	dev = &rte_dmadevices[dev_id];

	if (dev_priv_size > 0) {
		dev->dev_private = rte_zmalloc_socket("dmadev private",
				     dev_priv_size,
				     RTE_CACHE_LINE_SIZE,
				     socket_id);
		if (dev->dev_private == NULL) {
			RTE_DMADEV_LOG(ERR,
				"unable to allocate memory for dmadev");
			return NULL;
		}
	}

	dev->dev_id = dev_id;
	dev->socket_id = socket_id;
	dev->started = 0;
	strlcpy(dev->name, name, RTE_DMADEV_NAME_MAX_LEN);

	dev->attached = RTE_DMADEV_ATTACHED;

	return dev;
}

__rte_experimental
int
rte_dmadev_pmd_release(struct rte_dmadev *dev)
{
	int ret;

	if (dev == NULL)
		return -EINVAL;

	ret = rte_dmadev_close(dev->dev_id);
	if (ret != 0)
		return ret;

	if (dev->dev_private != NULL)
		rte_free(dev->dev_private);

	memset(dev, 0, sizeof(struct rte_dmadev));
	dev->attached = RTE_DMADEV_DETACHED;

	return 0;
}

RTE_LOG_REGISTER(libdmadev_logtype, lib.dmadev, INFO);
