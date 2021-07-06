/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 Intel Corporation
 */

#include <unistd.h>

#include <rte_mbuf.h>
#include <rte_dmadev.h>
#include "test.h"

#define COPY_LEN 1024

static struct rte_mempool *pool;
static uint16_t id_count = 0;

#define PRINT_ERR(...) print_err(__func__, __LINE__, __VA_ARGS__)

static inline int
__rte_format_printf(3, 4)
print_err(const char *func, int lineno, const char *format, ...)
{
	va_list ap;
	int ret;

	ret = fprintf(stderr, "In %s:%d - ", func, lineno);
	va_start(ap, format);
	ret += vfprintf(stderr, format, ap);
	va_end(ap);

	return ret;
}

static int
do_multi_copies(int dev_id, int split_batches, int split_completions)
{
	struct rte_mbuf *srcs[32], *dsts[32];
	unsigned int i, j;
	bool dma_err = false;

	for (i = 0; i < RTE_DIM(srcs); i++) {
		char *src_data;

		if (split_batches && i == RTE_DIM(srcs) / 2)
			rte_dmadev_perform(dev_id, 0);

		srcs[i] = rte_pktmbuf_alloc(pool);
		dsts[i] = rte_pktmbuf_alloc(pool);
		src_data = rte_pktmbuf_mtod(srcs[i], char *);
		if (srcs[i] == NULL || dsts[i] == NULL) {
			PRINT_ERR("Error allocating buffers\n");
			return -1;
		}

		for (j = 0; j < COPY_LEN; j++)
			src_data[j] = rand() & 0xFF;

		if (rte_dmadev_copy(dev_id, 0, srcs[i]->buf_iova + srcs[i]->data_off,
				dsts[i]->buf_iova + dsts[i]->data_off, COPY_LEN, 0) != id_count++) {
			PRINT_ERR("Error with rte_dmadev_copy for buffer %u\n", i);
			return -1;
		}
	}
	rte_dmadev_perform(dev_id, 0);
	usleep(100);

	if (split_completions) {
		/* gather completions in two halves */
		uint16_t half_len = RTE_DIM(srcs) / 2;
		int ret = rte_dmadev_completed(dev_id, 0, half_len, NULL, &dma_err);
		if (ret != half_len || dma_err) {
			PRINT_ERR("Error with rte_dmadev_completed - first half. ret = %d, expected ret = %u, dma_err = %d\n",
					ret, half_len, dma_err);
			rte_dmadev_dump(dev_id, stdout);
			return -1;
		}
		ret = rte_dmadev_completed(dev_id, 0, half_len, NULL, &dma_err);
		if (ret != half_len || dma_err) {
			PRINT_ERR("Error with rte_dmadev_completed - second half. ret = %d, expected ret = %u, dma_err = %d\n",
					ret, half_len, dma_err);
			rte_dmadev_dump(dev_id, stdout);
			return -1;
		}
	} else {
		/* gather all completions in one go */
		if ((j = rte_dmadev_completed(dev_id, 0, RTE_DIM(srcs), NULL,
				&dma_err)) != RTE_DIM(srcs) || dma_err) {
			PRINT_ERR("Error with rte_dmadev_completed, %u [expected: %zu], dma_err = %d\n",
					j, RTE_DIM(srcs), dma_err);
			rte_dmadev_dump(dev_id, stdout);
			return -1;
		}
	}

	/* check for empty */
	if (rte_dmadev_completed(dev_id, 0, RTE_DIM(srcs), NULL, &dma_err) != 0 || dma_err) {
		PRINT_ERR("Error with rte_dmadev_completed - ops unexpectedly returned\n");
		rte_dmadev_dump(dev_id, stdout);
		return -1;
	}

	for (i = 0; i < RTE_DIM(srcs); i++) {
		char *src_data, *dst_data;

		src_data = rte_pktmbuf_mtod(srcs[i], char *);
		dst_data = rte_pktmbuf_mtod(dsts[i], char *);
		for (j = 0; j < COPY_LEN; j++)
			if (src_data[j] != dst_data[j]) {
				PRINT_ERR("Error with copy of packet %u, byte %u\n", i, j);
				return -1;
			}
		rte_pktmbuf_free(srcs[i]);
		rte_pktmbuf_free(dsts[i]);
	}
	return 0;
}

static int
test_enqueue_copies(int dev_id)
{
	unsigned int i;
	uint16_t id;

	/* test doing a single copy */
	do {
		struct rte_mbuf *src, *dst;
		char *src_data, *dst_data;

		src = rte_pktmbuf_alloc(pool);
		dst = rte_pktmbuf_alloc(pool);
		src_data = rte_pktmbuf_mtod(src, char *);
		dst_data = rte_pktmbuf_mtod(dst, char *);

		for (i = 0; i < COPY_LEN; i++)
			src_data[i] = rand() & 0xFF;

		id = rte_dmadev_copy(dev_id, 0, src->buf_iova + src->data_off,
				dst->buf_iova + dst->data_off, COPY_LEN, 0);
		if (id != id_count) {
			PRINT_ERR("Error with rte_dmadev_copy, got %u, expected %u\n",
					id, id_count);
			return -1;
		}
		if (rte_dmadev_perform(dev_id, 0) < 0) {
			PRINT_ERR("Error with rte_dmadev_perform\n");
			return -1;
		}
		/* give time for copy to finish, then check it was done */
		usleep(10);

		for (i = 0; i < COPY_LEN; i++) {
			if (dst_data[i] != src_data[i]) {
				PRINT_ERR("Data mismatch at char %u [Got %02x not %02x]\n", i,
						dst_data[i], src_data[i]);
				rte_dmadev_dump(dev_id, stderr);
				return -1;
			}
		}

		/* now check completion works */
		if (rte_dmadev_completed(dev_id, 0, 1, &id, NULL) != 1) {
			PRINT_ERR("Error with rte_dmadev_completed\n");
			return -1;
		}
		if (id != id_count) {
			PRINT_ERR("Error:incorrect job id received, %u [expected %u]\n", id, id_count);
			return -1;
		}

		rte_pktmbuf_free(src);
		rte_pktmbuf_free(dst);

		/* now check completion works */
		if (rte_dmadev_completed(dev_id, 0, 1, NULL, NULL) != 0) {
			PRINT_ERR("Error with rte_dmadev_completed in empty check\n");
			return -1;
		}
		id_count++;

	} while (0);

	/* test doing a multiple single copies */
	do {
		const uint16_t max_ops = 4;
		struct rte_mbuf *src, *dst;
		char *src_data, *dst_data;

		src = rte_pktmbuf_alloc(pool);
		dst = rte_pktmbuf_alloc(pool);
		src_data = rte_pktmbuf_mtod(src, char *);
		dst_data = rte_pktmbuf_mtod(dst, char *);

		for (i = 0; i < COPY_LEN; i++)
			src_data[i] = rand() & 0xFF;

		/* perform the same copy <max_ops> times */
		for (i = 0; i < max_ops; i++) {
			if (rte_dmadev_copy(dev_id, 0,
					src->buf_iova + src->data_off,
					dst->buf_iova + dst->data_off,
					COPY_LEN, 0) != id_count++) {
				PRINT_ERR("Error with rte_dmadev_copy\n");
				return -1;
			}
			rte_dmadev_perform(dev_id, 0);
		}
		usleep(10);

		if ((i = rte_dmadev_completed(dev_id, 0, max_ops * 2, &id, NULL)) != max_ops) {
			PRINT_ERR("Error with rte_dmadev_completed, got %u not %u\n", i, max_ops);
			return -1;
		}
		if (id != id_count - 1) {
			PRINT_ERR("Error, incorrect job id returned: got %u not %u\n", id, id_count - 1);
			return -1;
		}
		for (i = 0; i < COPY_LEN; i++) {
			if (dst_data[i] != src_data[i]) {
				PRINT_ERR("Data mismatch at char %u\n", i);
				return -1;
			}
		}
		rte_pktmbuf_free(src);
		rte_pktmbuf_free(dst);
	} while (0);

	/* test doing multiple copies */
	return do_multi_copies(dev_id, 0, 0) /* enqueue and complete one batch at a time */
			|| do_multi_copies(dev_id, 1, 0) /* enqueue 2 batches and then complete both */
			|| do_multi_copies(dev_id, 0, 1); /* enqueue 1 batch, then complete in two halves */
}

static int
test_dmadev_instance(uint16_t dev_id)
{
#define TEST_RINGSIZE 512
	struct rte_dmadev_info info;
	struct rte_dmadev_conf conf = { .nb_hw_queues = 1};
	struct rte_dmadev_queue_conf qconf = { .nb_desc = TEST_RINGSIZE };
	int i;

	rte_dmadev_info_get(dev_id, &info);
	if (info.max_hw_queues < 1) {
		PRINT_ERR("Error, no queues reported on device id %u\n", dev_id);
		return -1;
	}
	if (rte_dmadev_configure(dev_id, &conf) != 0) {
		PRINT_ERR("Error with rte_rawdev_configure()\n");
		return -1;
	}
	if (rte_dmadev_queue_setup(dev_id, &qconf) != 0) {
		PRINT_ERR("Error with queue configuration\n");
		return -1;
	}
	rte_dmadev_info_get(dev_id, &info);
	if (info.nb_hw_queues != 1) {
		PRINT_ERR("Error, no configured queues reported on device id %u\n", dev_id);
		return -1;
	}

	if (rte_dmadev_start(dev_id) != 0) {
		PRINT_ERR("Error with rte_rawdev_start()\n");
		return -1;
	}
	id_count = 0;

	/* create a mempool for running tests */
	pool = rte_pktmbuf_pool_create("TEST_DMADEV_POOL",
			TEST_RINGSIZE * 2, /* n == num elements */
			32,  /* cache size */
			0,   /* priv size */
			2048, /* data room size */
			info.socket_id);
	if (pool == NULL) {
		PRINT_ERR("Error with mempool creation\n");
		return -1;
	}

	/* run the test cases */
	printf("DMA Dev: %u, Running Copy Tests\n", dev_id);
	for (i = 0; i < 768; i++) {
		struct rte_dmadev_stats stats;

		if (test_enqueue_copies(dev_id) != 0) {
			printf("Error with iteration %d\n", i);
			rte_dmadev_dump(dev_id, stdout);
			goto err;
		}

		rte_dmadev_stats_get(dev_id, 0, &stats);
		printf("Ops enqueued: %"PRIu64"\t", stats.enqueued_count);
		printf("Ops completed: %"PRIu64"\r", stats.completed_count);
	}
	printf("\n");

	rte_mempool_free(pool);
	rte_dmadev_stop(dev_id);

	return 0;

err:
	rte_mempool_free(pool);
	rte_dmadev_stop(dev_id);
	return -1;
}

static int
test_dmadevs(void)
{
	int i;

	if (rte_dmadev_count() == 0)
		return TEST_SKIPPED;

	for (i = 0; i < RTE_DMADEV_MAX_DEVS; i++)
		if (rte_dmadevices[i].attached && test_dmadev_instance(i) < 0)
			return -1;
	return 0;
}

REGISTER_TEST_COMMAND(dmadev_autotest, test_dmadevs);
