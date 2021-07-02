/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 HiSilicon Limited.
 */

#ifndef _RTE_DMADEV_CORE_H_
#define _RTE_DMADEV_CORE_H_

/**
 * @file
 *
 * RTE DMA Device internal header.
 *
 * This header contains internal data types. But they are still part of the
 * public API because they are used by inline public functions.
 */

struct rte_dmadev;

typedef dma_cookie_t (*dmadev_copy_t)(struct rte_dmadev *dev, uint16_t vq_id,
				      void *src, void *dst,
				      uint32_t length, uint64_t flags);
/**< @internal Function used to enqueue a copy operation. */

typedef dma_cookie_t (*dmadev_copy_sg_t)(struct rte_dmadev *dev, uint16_t vq_id,
					 const struct dma_scatterlist *sg,
					 uint32_t sg_len, uint64_t flags);
/**< @internal Function used to enqueue a scatter list copy operation. */

typedef dma_cookie_t (*dmadev_fill_t)(struct rte_dmadev *dev, uint16_t vq_id,
				      uint64_t pattern, void *dst,
				      uint32_t length, uint64_t flags);
/**< @internal Function used to enqueue a fill operation. */

typedef dma_cookie_t (*dmadev_fill_sg_t)(struct rte_dmadev *dev, uint16_t vq_id,
			uint64_t pattern, const struct dma_scatterlist *sg,
			uint32_t sg_len, uint64_t flags);
/**< @internal Function used to enqueue a scatter list fill operation. */

typedef int (*dmadev_fence_t)(struct rte_dmadev *dev, uint16_t vq_id);
/**< @internal Function used to add a fence ordering between operations. */

typedef int (*dmadev_perform_t)(struct rte_dmadev *dev, uint16_t vq_id);
/**< @internal Function used to trigger hardware to begin performing. */

typedef uint16_t (*dmadev_completed_t)(struct rte_dmadev *dev, uint16_t vq_id,
				       const uint16_t nb_cpls,
				       dma_cookie_t *cookie, bool *has_error);
/**< @internal Function used to return number of successful completed operations */

typedef uint16_t (*dmadev_completed_fails_t)(struct rte_dmadev *dev,
			uint16_t vq_id, const uint16_t nb_status,
			uint32_t *status, dma_cookie_t *cookie);
/**< @internal Function used to return number of failed completed operations */

#define RTE_DMADEV_NAME_MAX_LEN	64 /**< Max length of name of DMA PMD */

struct rte_dmadev_ops;

/**
 * The data structure associated with each DMA device.
 */
struct rte_dmadev {
	/**< Enqueue a copy operation onto the DMA device. */
	dmadev_copy_t copy;
	/**< Enqueue a scatter list copy operation onto the DMA device. */
	dmadev_copy_sg_t copy_sg;
	/**< Enqueue a fill operation onto the DMA device. */
	dmadev_fill_t fill;
	/**< Enqueue a scatter list fill operation onto the DMA device. */
	dmadev_fill_sg_t fill_sg;
	/**< Add a fence to force ordering between operations. */
	dmadev_fence_t fence;
	/**< Trigger hardware to begin performing enqueued operations. */
	dmadev_perform_t perform;
	/**< Returns the number of operations that successful completed. */
	dmadev_completed_t completed;
	/**< Returns the number of operations that failed to complete. */
	dmadev_completed_fails_t completed_fails;

	void *dev_private; /**< PMD-specific private data */
	const struct rte_dmadev_ops *dev_ops; /**< Functions exported by PMD */

	uint16_t dev_id; /**< Device ID for this instance */
	int socket_id; /**< Socket ID where memory is allocated */
	struct rte_device *device;
	/**< Device info. supplied during device initialization */
	const char *driver_name; /**< Driver info. supplied by probing */
	char name[RTE_DMADEV_NAME_MAX_LEN]; /**< Device name */

	RTE_STD_C11
	uint8_t attached : 1; /**< Flag indicating the device is attached */
	uint8_t started : 1; /**< Device state: STARTED(1)/STOPPED(0) */

} __rte_cache_aligned;

extern struct rte_dmadev rte_dmadevices[];

#endif /* _RTE_DMADEV_CORE_H_ */
