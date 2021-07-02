/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 HiSilicon Limited.
 */

#ifndef _RTE_DMADEV_H_
#define _RTE_DMADEV_H_

/**
 * @file rte_dmadev.h
 *
 * RTE DMA (Direct Memory Access) device APIs.
 *
 * The generic DMA device diagram:
 *
 *            ------------     ------------
 *            | HW-queue |     | HW-queue |
 *            ------------     ------------
 *                   \            /
 *                    \          /
 *                     \        /
 *                  ----------------
 *                  |dma-controller|
 *                  ----------------
 *
 *   The DMA could have multiple HW-queues, each HW-queue could have multiple
 *   capabilities, e.g. whether to support fill operation, supported DMA
 *   transfter direction and etc.
 *
 * The DMA framework is built on the following abstraction model:
 *
 *     ------------    ------------
 *     |virt-queue|    |virt-queue|
 *     ------------    ------------
 *            \           /
 *             \         /
 *              \       /
 *            ------------     ------------
 *            | HW-queue |     | HW-queue |
 *            ------------     ------------
 *                   \            /
 *                    \          /
 *                     \        /
 *                     ----------
 *                     | dmadev |
 *                     ----------
 *
 *   a) The DMA operation request must be submitted to the virt queue, virt
 *      queues must be created based on HW queues, the DMA device could have
 *      multiple HW queues.
 *   b) The virt queues on the same HW-queue could represent different contexts,
 *      e.g. user could create virt-queue-0 on HW-queue-0 for mem-to-mem
 *      transfer scenario, and create virt-queue-1 on the same HW-queue for
 *      mem-to-dev transfer scenario.
 *   NOTE: user could also create multiple virt queues for mem-to-mem transfer
 *         scenario as long as the corresponding driver supports.
 *
 * The control plane APIs include configure/queue_setup/queue_release/start/
 * stop/reset/close, in order to start device work, the call sequence must be
 * as follows:
 *     - rte_dmadev_configure()
 *     - rte_dmadev_queue_setup()
 *     - rte_dmadev_start()
 *
 * The dataplane APIs include two parts:
 *   a) The first part is the submission of operation requests:
 *        - rte_dmadev_copy()
 *        - rte_dmadev_copy_sg() - scatter-gather form of copy
 *        - rte_dmadev_fill()
 *        - rte_dmadev_fill_sg() - scatter-gather form of fill
 *        - rte_dmadev_fence()   - add a fence force ordering between operations
 *        - rte_dmadev_perform() - issue doorbell to hardware
 *      These APIs could work with different virt queues which have different
 *      contexts.
 *      The first four APIs are used to submit the operation request to the virt
 *      queue, if the submission is successful, a cookie (as type
 *      'dma_cookie_t') is returned, otherwise a negative number is returned.
 *   b) The second part is to obtain the result of requests:
 *        - rte_dmadev_completed()
 *            - return the number of operation requests completed successfully.
 *        - rte_dmadev_completed_fails()
 *            - return the number of operation requests failed to complete.
 *
 * The misc APIs include info_get/queue_info_get/stats/xstats/selftest, provide
 * information query and self-test capabilities.
 *
 * About the dataplane APIs MT-safe, there are two dimensions:
 *   a) For one virt queue, the submit/completion API could be MT-safe,
 *      e.g. one thread do submit operation, another thread do completion
 *      operation.
 *      If driver support it, then declare RTE_DMA_DEV_CAPA_MT_VQ.
 *      If driver don't support it, it's up to the application to guarantee
 *      MT-safe.
 *   b) For multiple virt queues on the same HW queue, e.g. one thread do
 *      operation on virt-queue-0, another thread do operation on virt-queue-1.
 *      If driver support it, then declare RTE_DMA_DEV_CAPA_MT_MVQ.
 *      If driver don't support it, it's up to the application to guarantee
 *      MT-safe.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_errno.h>
#include <rte_compat.h>

/**
 * dma_cookie_t - an opaque DMA cookie
 *
 * If dma_cookie_t is >=0 it's a DMA operation request cookie, <0 it's a error
 * code.
 * When using cookies, comply with the following rules:
 * a) Cookies for each virtual queue are independent.
 * b) For a virt queue, the cookie are monotonically incremented, when it reach
 *    the INT_MAX, it wraps back to zero.
 * c) The initial cookie of a virt queue is zero, after the device is stopped or
 *    reset, the virt queue's cookie needs to be reset to zero.
 * Example:
 *    step-1: start one dmadev
 *    step-2: enqueue a copy operation, the cookie return is 0
 *    step-3: enqueue a copy operation again, the cookie return is 1
 *    ...
 *    step-101: stop the dmadev
 *    step-102: start the dmadev
 *    step-103: enqueue a copy operation, the cookie return is 0
 *    ...
 */
typedef int32_t dma_cookie_t;

/**
 * dma_scatterlist - can hold scatter DMA operation request
 */
struct dma_scatterlist {
	void *src;
	void *dst;
	uint32_t length;
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Get the total number of DMA devices that have been successfully
 * initialised.
 *
 * @return
 *   The total number of usable DMA devices.
 */
__rte_experimental
uint16_t
rte_dmadev_count(void);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Get the device identifier for the named DMA device.
 *
 * @param name
 *   DMA device name to select the DMA device identifier.
 *
 * @return
 *   Returns DMA device identifier on success.
 *   - <0: Failure to find named DMA device.
 */
__rte_experimental
int
rte_dmadev_get_dev_id(const char *name);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Return the NUMA socket to which a device is connected.
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @return
 *   The NUMA socket id to which the device is connected or
 *   a default of zero if the socket could not be determined.
 *   - -EINVAL: dev_id value is out of range.
 */
__rte_experimental
int
rte_dmadev_socket_id(uint16_t dev_id);

/**
 * The capabilities of a DMA device
 */
#define RTE_DMA_DEV_CAPA_M2M	(1ull << 0) /**< Support mem-to-mem transfer */
#define RTE_DMA_DEV_CAPA_M2D	(1ull << 1) /**< Support mem-to-dev transfer */
#define RTE_DMA_DEV_CAPA_D2M	(1ull << 2) /**< Support dev-to-mem transfer */
#define RTE_DMA_DEV_CAPA_D2D	(1ull << 3) /**< Support dev-to-dev transfer */
#define RTE_DMA_DEV_CAPA_COPY	(1ull << 4) /**< Support copy ops */
#define RTE_DMA_DEV_CAPA_FILL	(1ull << 5) /**< Support fill ops */
#define RTE_DMA_DEV_CAPA_SG	(1ull << 6) /**< Support scatter-gather ops */
#define RTE_DMA_DEV_CAPA_FENCE	(1ull << 7) /**< Support fence ops */
#define RTE_DMA_DEV_CAPA_IOVA	(1ull << 8) /**< Support IOVA as DMA address */
#define RTE_DMA_DEV_CAPA_VA	(1ull << 9) /**< Support VA as DMA address */
#define RTE_DMA_DEV_CAPA_MT_VQ	(1ull << 10) /**< Support MT-safe of one virt queue */
#define RTE_DMA_DEV_CAPA_MT_MVQ	(1ull << 11) /**< Support MT-safe of multiple virt queues */

/**
 * A structure used to retrieve the contextual information of
 * an DMA device
 */
struct rte_dmadev_info {
	/**
	 * Fields filled by framewok
	 */
	struct rte_device *device; /**< Generic Device information */
	const char *driver_name; /**< Device driver name */
	int socket_id; /**< Socket ID where memory is allocated */

	/**
	 * Specification fields filled by driver
	 */
	uint64_t dev_capa; /**< Device capabilities (RTE_DMA_DEV_CAPA_) */
	uint16_t max_hw_queues; /**< Maximum number of HW queues. */
	uint16_t max_vqs_per_hw_queue;
	/**< Maximum number of virt queues to allocate per HW queue */
	uint16_t max_desc;
	/**< Maximum allowed number of virt queue descriptors */
	uint16_t min_desc;
	/**< Minimum allowed number of virt queue descriptors */

	/**
	 * Status fields filled by driver
	 */
	uint16_t nb_hw_queues; /**< Number of HW queues configured */
	uint16_t nb_vqs; /**< Number of virt queues configured */
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Retrieve the contextual information of a DMA device.
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @param[out] dev_info
 *   A pointer to a structure of type *rte_dmadev_info* to be filled with the
 *   contextual information of the device.
 * @return
 *   - =0: Success, driver updates the contextual information of the DMA device
 *   - <0: Error code returned by the driver info get function.
 *
 */
__rte_experimental
int
rte_dmadev_info_get(uint16_t dev_id, struct rte_dmadev_info *dev_info);

/**
 * dma_address_type
 */
enum dma_address_type {
	DMA_ADDRESS_TYPE_IOVA, /**< Use IOVA as dma address */
	DMA_ADDRESS_TYPE_VA, /**< Use VA as dma address */
};

/**
 * A structure used to configure a DMA device.
 */
struct rte_dmadev_conf {
	enum dma_address_type addr_type; /**< Address type to used */
	uint16_t nb_hw_queues; /**< Number of HW-queues enable to use */
	uint16_t max_vqs; /**< Maximum number of virt queues to use */
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Configure a DMA device.
 *
 * This function must be invoked first before any other function in the
 * API. This function can also be re-invoked when a device is in the
 * stopped state.
 *
 * The caller may use rte_dmadev_info_get() to get the capability of each
 * resources available for this DMA device.
 *
 * @param dev_id
 *   The identifier of the device to configure.
 * @param dev_conf
 *   The DMA device configuration structure encapsulated into rte_dmadev_conf
 *   object.
 *
 * @return
 *   - =0: Success, device configured.
 *   - <0: Error code returned by the driver configuration function.
 */
__rte_experimental
int
rte_dmadev_configure(uint16_t dev_id, const struct rte_dmadev_conf *dev_conf);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Start a DMA device.
 *
 * The device start step is the last one and consists of setting the DMA
 * to start accepting jobs.
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @return
 *   - =0: Success, device started.
 *   - <0: Error code returned by the driver start function.
 */
__rte_experimental
int
rte_dmadev_start(uint16_t dev_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Stop a DMA device.
 *
 * The device can be restarted with a call to rte_dmadev_start()
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @return
 *   - =0: Success, device stopped.
 *   - <0: Error code returned by the driver stop function.
 */
__rte_experimental
int
rte_dmadev_stop(uint16_t dev_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Close a DMA device.
 *
 * The device cannot be restarted after this call.
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @return
 *  - =0: Successfully closing device
 *  - <0: Failure to close device
 */
__rte_experimental
int
rte_dmadev_close(uint16_t dev_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Reset a DMA device.
 *
 * This is different from cycle of rte_dmadev_start->rte_dmadev_stop in the
 * sense similar to hard or soft reset.
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @return
 *   - =0: Successful reset device.
 *   - <0: Failure to reset device.
 *   - (-ENOTSUP): If the device doesn't support this function.
 */
__rte_experimental
int
rte_dmadev_reset(uint16_t dev_id);

/**
 * dma_transfer_direction
 */
enum dma_transfer_direction {
	DMA_MEM_TO_MEM,
	DMA_MEM_TO_DEV,
	DMA_DEV_TO_MEM,
	DMA_DEV_TO_DEV,
};

/**
 * A structure used to configure a DMA virt queue.
 */
struct rte_dmadev_queue_conf {
	enum dma_transfer_direction direction;
	/**< Associated transfer direction */
	uint16_t hw_queue_id; /**< The HW queue on which to create virt queue */
	uint16_t nb_desc; /**< Number of descriptor for this virt queue */
	uint64_t dev_flags; /**< Device specific flags */
	void *dev_ctx; /**< Device specific context */
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Allocate and set up a virt queue.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param conf
 *   The queue configuration structure encapsulated into rte_dmadev_queue_conf
 *   object.
 *
 * @return
 *   - >=0: Allocate virt queue success, it is virt queue id.
 *   - <0: Error code returned by the driver queue setup function.
 */
__rte_experimental
int
rte_dmadev_queue_setup(uint16_t dev_id,
		       const struct rte_dmadev_queue_conf *conf);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Release a virt queue.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vq_id
 *   The identifier of virt queue which return by queue setup.
 *
 * @return
 *   - =0: Successful release the virt queue.
 *   - <0: Error code returned by the driver queue release function.
 */
__rte_experimental
int
rte_dmadev_queue_release(uint16_t dev_id, uint16_t vq_id);

/**
 * A structure used to retrieve information of a DMA virt queue.
 */
struct rte_dmadev_queue_info {
	enum dma_transfer_direction direction;
	/**< Associated transfer direction */
	uint16_t hw_queue_id; /**< The HW queue on which to create virt queue */
	uint16_t nb_desc; /**< Number of descriptor for this virt queue */
	uint64_t dev_flags; /**< Device specific flags */
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Retrieve information of a DMA virt queue.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vq_id
 *   The identifier of virt queue which return by queue setup.
 * @param[out] info
 *   The queue info structure encapsulated into rte_dmadev_queue_info object.
 *
 * @return
 *   - =0: Successful retrieve information.
 *   - <0: Error code returned by the driver queue release function.
 */
__rte_experimental
int
rte_dmadev_queue_info_get(uint16_t dev_id, uint16_t vq_id,
			  struct rte_dmadev_queue_info *info);

#include "rte_dmadev_core.h"

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Enqueue a copy operation onto the DMA virt queue.
 *
 * This queues up a copy operation to be performed by hardware, but does not
 * trigger hardware to begin that operation.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vq_id
 *   The identifier of virt queue.
 * @param src
 *   The address of the source buffer.
 * @param dst
 *   The address of the destination buffer.
 * @param length
 *   The length of the data to be copied.
 * @param flags
 *   An opaque flags for this operation.
 *
 * @return
 *   dma_cookie_t: please refer to the corresponding definition.
 *
 * NOTE: The caller must ensure that the input parameter is valid and the
 *       corresponding device supports the operation.
 */
__rte_experimental
static inline dma_cookie_t
rte_dmadev_copy(uint16_t dev_id, uint16_t vq_id, void *src, void *dst,
		uint32_t length, uint64_t flags)
{
	struct rte_dmadev *dev = &rte_dmadevices[dev_id];
	return (*dev->copy)(dev, vq_id, src, dst, length, flags);
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Enqueue a scatter list copy operation onto the DMA virt queue.
 *
 * This queues up a scatter list copy operation to be performed by hardware,
 * but does not trigger hardware to begin that operation.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vq_id
 *   The identifier of virt queue.
 * @param sg
 *   The pointer of scatterlist.
 * @param sg_len
 *   The number of scatterlist elements.
 * @param flags
 *   An opaque flags for this operation.
 *
 * @return
 *   dma_cookie_t: please refer to the corresponding definition.
 *
 * NOTE: The caller must ensure that the input parameter is valid and the
 *       corresponding device supports the operation.
 */
__rte_experimental
static inline dma_cookie_t
rte_dmadev_copy_sg(uint16_t dev_id, uint16_t vq_id,
		   const struct dma_scatterlist *sg,
		   uint32_t sg_len, uint64_t flags)
{
	struct rte_dmadev *dev = &rte_dmadevices[dev_id];
	return (*dev->copy_sg)(dev, vq_id, sg, sg_len, flags);
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Enqueue a fill operation onto the DMA virt queue
 *
 * This queues up a fill operation to be performed by hardware, but does not
 * trigger hardware to begin that operation.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vq_id
 *   The identifier of virt queue.
 * @param pattern
 *   The pattern to populate the destination buffer with.
 * @param dst
 *   The address of the destination buffer.
 * @param length
 *   The length of the destination buffer.
 * @param flags
 *   An opaque flags for this operation.
 *
 * @return
 *   dma_cookie_t: please refer to the corresponding definition.
 *
 * NOTE: The caller must ensure that the input parameter is valid and the
 *       corresponding device supports the operation.
 */
__rte_experimental
static inline dma_cookie_t
rte_dmadev_fill(uint16_t dev_id, uint16_t vq_id, uint64_t pattern,
		void *dst, uint32_t length, uint64_t flags)
{
	struct rte_dmadev *dev = &rte_dmadevices[dev_id];
	return (*dev->fill)(dev, vq_id, pattern, dst, length, flags);
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Enqueue a scatter list fill operation onto the DMA virt queue
 *
 * This queues up a scatter list fill operation to be performed by hardware,
 * but does not trigger hardware to begin that operation.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vq_id
 *   The identifier of virt queue.
 * @param pattern
 *   The pattern to populate the destination buffer with.
 * @param sg
 *   The pointer of scatterlist.
 * @param sg_len
 *   The number of scatterlist elements.
 * @param flags
 *   An opaque flags for this operation.
 *
 * @return
 *   dma_cookie_t: please refer to the corresponding definition.
 *
 * NOTE: The caller must ensure that the input parameter is valid and the
 *       corresponding device supports the operation.
 */
__rte_experimental
static inline dma_cookie_t
rte_dmadev_fill_sg(uint16_t dev_id, uint16_t vq_id, uint64_t pattern,
		   const struct dma_scatterlist *sg, uint32_t sg_len,
		   uint64_t flags)
{
	struct rte_dmadev *dev = &rte_dmadevices[dev_id];
	return (*dev->fill_sg)(dev, vq_id, pattern, sg, sg_len, flags);
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Add a fence to force ordering between operations
 *
 * This adds a fence to a sequence of operations to enforce ordering, such that
 * all operations enqueued before the fence must be completed before operations
 * after the fence.
 * NOTE: Since this fence may be added as a flag to the last operation enqueued,
 * this API may not function correctly when called immediately after an
 * "rte_dmadev_perform" call i.e. before any new operations are enqueued.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vq_id
 *   The identifier of virt queue.
 *
 * @return
 *   - =0: Successful add fence.
 *   - <0: Failure to add fence.
 *
 * NOTE: The caller must ensure that the input parameter is valid and the
 *       corresponding device supports the operation.
 */
__rte_experimental
static inline int
rte_dmadev_fence(uint16_t dev_id, uint16_t vq_id)
{
	struct rte_dmadev *dev = &rte_dmadevices[dev_id];
	return (*dev->fence)(dev, vq_id);
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Trigger hardware to begin performing enqueued operations
 *
 * This API is used to write the "doorbell" to the hardware to trigger it
 * to begin the operations previously enqueued by rte_dmadev_copy/fill()
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vq_id
 *   The identifier of virt queue.
 *
 * @return
 *   - =0: Successful trigger hardware.
 *   - <0: Failure to trigger hardware.
 *
 * NOTE: The caller must ensure that the input parameter is valid and the
 *       corresponding device supports the operation.
 */
__rte_experimental
static inline int
rte_dmadev_perform(uint16_t dev_id, uint16_t vq_id)
{
	struct rte_dmadev *dev = &rte_dmadevices[dev_id];
	return (*dev->perform)(dev, vq_id);
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Returns the number of operations that have been successful completed.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vq_id
 *   The identifier of virt queue.
 * @param nb_cpls
 *   The maximum number of completed operations that can be processed.
 * @param[out] cookie
 *   The last completed operation's cookie.
 * @param[out] has_error
 *   Indicates if there are transfer error.
 *
 * @return
 *   The number of operations that successful completed.
 *
 * NOTE: The caller must ensure that the input parameter is valid and the
 *       corresponding device supports the operation.
 */
__rte_experimental
static inline uint16_t
rte_dmadev_completed(uint16_t dev_id, uint16_t vq_id, const uint16_t nb_cpls,
		     dma_cookie_t *cookie, bool *has_error)
{
	struct rte_dmadev *dev = &rte_dmadevices[dev_id];
	has_error = false;
	return (*dev->completed)(dev, vq_id, nb_cpls, cookie, has_error);
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Returns the number of operations that failed to complete.
 * NOTE: This API was used when rte_dmadev_completed has_error was set.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vq_id
 *   The identifier of virt queue.
 * @param nb_status
 *   Indicates the size of status array.
 * @param[out] status
 *   The error code of operations that failed to complete.
 * @param[out] cookie
 *   The last failed completed operation's cookie.
 *
 * @return
 *   The number of operations that failed to complete.
 *
 * NOTE: The caller must ensure that the input parameter is valid and the
 *       corresponding device supports the operation.
 */
__rte_experimental
static inline uint16_t
rte_dmadev_completed_fails(uint16_t dev_id, uint16_t vq_id,
			   const uint16_t nb_status, uint32_t *status,
			   dma_cookie_t *cookie)
{
	struct rte_dmadev *dev = &rte_dmadevices[dev_id];
	return (*dev->completed_fails)(dev, vq_id, nb_status, status, cookie);
}

struct rte_dmadev_stats {
	uint64_t enqueue_fail_count;
	/**< Conut of all operations which failed enqueued */
	uint64_t enqueued_count;
	/**< Count of all operations which successful enqueued */
	uint64_t completed_fail_count;
	/**< Count of all operations which failed to complete */
	uint64_t completed_count;
	/**< Count of all operations which successful complete */
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Retrieve basic statistics of a or all DMA virt queue(s).
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vq_id
 *   The identifier of virt queue, -1 means all virt queues.
 * @param[out] stats
 *   The basic statistics structure encapsulated into rte_dmadev_stats
 *   object.
 *
 * @return
 *   - =0: Successful retrieve stats.
 *   - <0: Failure to retrieve stats.
 */
__rte_experimental
int
rte_dmadev_stats_get(uint16_t dev_id, int vq_id,
		     struct rte_dmadev_stats *stats);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Reset basic statistics of a or all DMA virt queue(s).
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vq_id
 *   The identifier of virt queue, -1 means all virt queues.
 *
 * @return
 *   - =0: Successful retrieve stats.
 *   - <0: Failure to retrieve stats.
 */
__rte_experimental
int
rte_dmadev_stats_reset(uint16_t dev_id, int vq_id);

/** Maximum name length for extended statistics counters */
#define RTE_DMA_DEV_XSTATS_NAME_SIZE 64

/**
 * A name-key lookup element for extended statistics.
 *
 * This structure is used to map between names and ID numbers
 * for extended ethdev statistics.
 */
struct rte_dmadev_xstats_name {
	char name[RTE_DMA_DEV_XSTATS_NAME_SIZE];
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Retrieve names of extended statistics of a DMA device.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param[out] xstats_names
 *   Block of memory to insert names into. Must be at least size in capacity.
 *   If set to NULL, function returns required capacity.
 * @param size
 *   Capacity of xstats_names (number of names).
 * @return
 *   - positive value lower or equal to size: success. The return value
 *     is the number of entries filled in the stats table.
 *   - positive value higher than size: error, the given statistics table
 *     is too small. The return value corresponds to the size that should
 *     be given to succeed. The entries in the table are not valid and
 *     shall not be used by the caller.
 *   - negative value on error.
 */
__rte_experimental
int
rte_dmadev_xstats_names_get(uint16_t dev_id,
			    struct rte_dmadev_xstats_name *xstats_names,
			    uint32_t size);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Retrieve extended statistics of a DMA device.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param ids
 *   The id numbers of the stats to get. The ids can be got from the stat
 *   position in the stat list from rte_dmadev_get_xstats_names().
 * @param[out] values
 *   The values for each stats request by ID.
 * @param n
 *   The number of stats requested.
 *
 * @return
 *   - positive value: number of stat entries filled into the values array.
 *   - negative value on error.
 */
__rte_experimental
int
rte_dmadev_xstats_get(uint16_t dev_id, const uint32_t ids[],
		      uint64_t values[], uint32_t n);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Reset the values of the xstats of the selected component in the device.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param ids
 *   Selects specific statistics to be reset. When NULL, all statistics
 *   will be reset. If non-NULL, must point to array of at least
 *   *nb_ids* size.
 * @param nb_ids
 *   The number of ids available from the *ids* array. Ignored when ids is NULL.
 *
 * @return
 *   - zero: successfully reset the statistics to zero.
 *   - negative value on error.
 */
__rte_experimental
int
rte_dmadev_xstats_reset(uint16_t dev_id, const uint32_t ids[], uint32_t nb_ids);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Trigger the dmadev self test.
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @return
 *   - 0: Selftest successful.
 *   - -ENOTSUP if the device doesn't support selftest
 *   - other values < 0 on failure.
 */
__rte_experimental
int
rte_dmadev_selftest(uint16_t dev_id);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_DMADEV_H_ */
