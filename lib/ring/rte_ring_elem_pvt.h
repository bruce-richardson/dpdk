/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2017,2018 HXT-semitech Corporation.
 * Copyright (c) 2007-2009 Kip Macy kmacy@freebsd.org
 * All rights reserved.
 * Derived from FreeBSD's bufring.h
 * Used as BSD-3 Licensed with permission from Kip Macy.
 */

#ifndef _RTE_RING_ELEM_PVT_H_
#define _RTE_RING_ELEM_PVT_H_

#if defined(RTE_TOOLCHAIN_GCC) && (GCC_VERSION >= 120000)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#pragma GCC diagnostic ignored "-Wstringop-overread"
#endif

static __rte_always_inline void
__rte_ring_enqueue_elems_32(void *ring_table, const void *obj_table,
	uint32_t size, uint32_t idx, uint32_t n)
{
	unsigned int i;

	uint32_t *ring = (uint32_t *)ring_table;
	const uint32_t *obj = (const uint32_t *)obj_table;

	if (likely(idx + n <= size)) {
		for (i = 0; i < (n & ~0x7); i += 8, idx += 8) {
			ring[idx] = obj[i];
			ring[idx + 1] = obj[i + 1];
			ring[idx + 2] = obj[i + 2];
			ring[idx + 3] = obj[i + 3];
			ring[idx + 4] = obj[i + 4];
			ring[idx + 5] = obj[i + 5];
			ring[idx + 6] = obj[i + 6];
			ring[idx + 7] = obj[i + 7];
		}
		switch (n & 0x7) {
		case 7:
			ring[idx++] = obj[i++]; /* fallthrough */
		case 6:
			ring[idx++] = obj[i++]; /* fallthrough */
		case 5:
			ring[idx++] = obj[i++]; /* fallthrough */
		case 4:
			ring[idx++] = obj[i++]; /* fallthrough */
		case 3:
			ring[idx++] = obj[i++]; /* fallthrough */
		case 2:
			ring[idx++] = obj[i++]; /* fallthrough */
		case 1:
			ring[idx++] = obj[i++]; /* fallthrough */
		}
	} else {
		for (i = 0; idx < size; i++, idx++)
			ring[idx] = obj[i];
		/* Start at the beginning */
		for (idx = 0; i < n; i++, idx++)
			ring[idx] = obj[i];
	}
}

static __rte_always_inline void
__rte_ring_enqueue_elems_64(void *ring_table, const void *obj_table,
	uint32_t size, uint32_t idx, uint32_t n)
{
	unsigned int i;

	uint64_t *ring = (uint64_t *)ring_table;
	const unaligned_uint64_t *obj = (const unaligned_uint64_t *)obj_table;

	if (likely(idx + n <= size)) {
		for (i = 0; i < (n & ~0x3); i += 4, idx += 4) {
			ring[idx] = obj[i];
			ring[idx + 1] = obj[i + 1];
			ring[idx + 2] = obj[i + 2];
			ring[idx + 3] = obj[i + 3];
		}
		switch (n & 0x3) {
		case 3:
			ring[idx++] = obj[i++]; /* fallthrough */
		case 2:
			ring[idx++] = obj[i++]; /* fallthrough */
		case 1:
			ring[idx++] = obj[i++];
		}
	} else {
		for (i = 0; idx < size; i++, idx++)
			ring[idx] = obj[i];
		/* Start at the beginning */
		for (idx = 0; i < n; i++, idx++)
			ring[idx] = obj[i];
	}
}

static __rte_always_inline void
__rte_ring_enqueue_elems_128(void *ring_table, const void *obj_table,
	uint32_t size, uint32_t idx, uint32_t n)
{
	unsigned int i;

	rte_int128_t *ring = (rte_int128_t *)ring_table;
	const rte_int128_t *obj = (const rte_int128_t *)obj_table;

	if (likely(idx + n <= size)) {
		for (i = 0; i < (n & ~0x1); i += 2, idx += 2)
			memcpy((void *)(ring + idx),
				(const void *)(obj + i), 32);
		switch (n & 0x1) {
		case 1:
			memcpy((void *)(ring + idx),
				(const void *)(obj + i), 16);
		}
	} else {
		for (i = 0; idx < size; i++, idx++)
			memcpy((void *)(ring + idx),
				(const void *)(obj + i), 16);
		/* Start at the beginning */
		for (idx = 0; i < n; i++, idx++)
			memcpy((void *)(ring + idx),
				(const void *)(obj + i), 16);
	}
}

/* the actual enqueue of elements on the ring.
 * Placed here since identical code needed in both
 * single and multi producer enqueue functions.
 */
static __rte_always_inline void
__rte_ring_do_enqueue_elems(void *ring_table, const void *obj_table,
	uint32_t size, uint32_t idx, uint32_t esize, uint32_t num)
{
	/* 8B and 16B copies implemented individually to retain
	 * the current performance.
	 */
	if (esize == 8)
		__rte_ring_enqueue_elems_64(ring_table, obj_table, size,
				idx, num);
	else if (esize == 16)
		__rte_ring_enqueue_elems_128(ring_table, obj_table, size,
				idx, num);
	else {
		uint32_t scale, nr_idx, nr_num, nr_size;

		/* Normalize to uint32_t */
		scale = esize / sizeof(uint32_t);
		nr_num = num * scale;
		nr_idx = idx * scale;
		nr_size = size * scale;
		__rte_ring_enqueue_elems_32(ring_table, obj_table, nr_size,
				nr_idx, nr_num);
	}
}

static __rte_always_inline void
__rte_ring_enqueue_elems(struct rte_ring *r, uint32_t prod_head,
		const void *obj_table, uint32_t esize, uint32_t num)
{
	__rte_ring_do_enqueue_elems(&r[1], obj_table, r->size,
			prod_head & r->mask, esize, num);
}

static __rte_always_inline void
__rte_ring_dequeue_elems_32(void *obj_table, const void *ring_table,
	uint32_t size, uint32_t idx, uint32_t n)
{
	unsigned int i;
	uint32_t *obj = (uint32_t *)obj_table;
	const uint32_t *ring = (const uint32_t *)ring_table;

	if (likely(idx + n <= size)) {
		for (i = 0; i < (n & ~0x7); i += 8, idx += 8) {
			obj[i] = ring[idx];
			obj[i + 1] = ring[idx + 1];
			obj[i + 2] = ring[idx + 2];
			obj[i + 3] = ring[idx + 3];
			obj[i + 4] = ring[idx + 4];
			obj[i + 5] = ring[idx + 5];
			obj[i + 6] = ring[idx + 6];
			obj[i + 7] = ring[idx + 7];
		}
		switch (n & 0x7) {
		case 7:
			obj[i++] = ring[idx++]; /* fallthrough */
		case 6:
			obj[i++] = ring[idx++]; /* fallthrough */
		case 5:
			obj[i++] = ring[idx++]; /* fallthrough */
		case 4:
			obj[i++] = ring[idx++]; /* fallthrough */
		case 3:
			obj[i++] = ring[idx++]; /* fallthrough */
		case 2:
			obj[i++] = ring[idx++]; /* fallthrough */
		case 1:
			obj[i++] = ring[idx++]; /* fallthrough */
		}
	} else {
		for (i = 0; idx < size; i++, idx++)
			obj[i] = ring[idx];
		/* Start at the beginning */
		for (idx = 0; i < n; i++, idx++)
			obj[i] = ring[idx];
	}
}

static __rte_always_inline void
__rte_ring_dequeue_elems_64(void *obj_table, const void *ring_table,
	uint32_t size, uint32_t idx, uint32_t n)
{
	unsigned int i;
	unaligned_uint64_t *obj = (unaligned_uint64_t *)obj_table;
	const uint64_t *ring = (const uint64_t *)ring_table;

	if (likely(idx + n <= size)) {
		for (i = 0; i < (n & ~0x3); i += 4, idx += 4) {
			obj[i] = ring[idx];
			obj[i + 1] = ring[idx + 1];
			obj[i + 2] = ring[idx + 2];
			obj[i + 3] = ring[idx + 3];
		}
		switch (n & 0x3) {
		case 3:
			obj[i++] = ring[idx++]; /* fallthrough */
		case 2:
			obj[i++] = ring[idx++]; /* fallthrough */
		case 1:
			obj[i++] = ring[idx++]; /* fallthrough */
		}
	} else {
		for (i = 0; idx < size; i++, idx++)
			obj[i] = ring[idx];
		/* Start at the beginning */
		for (idx = 0; i < n; i++, idx++)
			obj[i] = ring[idx];
	}
}

static __rte_always_inline void
__rte_ring_dequeue_elems_128(void *obj_table, const void *ring_table,
	uint32_t size, uint32_t idx, uint32_t n)
{
	unsigned int i;
	rte_int128_t *obj = (rte_int128_t *)obj_table;
	const rte_int128_t *ring = (const rte_int128_t *)ring_table;

	if (likely(idx + n <= size)) {
		for (i = 0; i < (n & ~0x1); i += 2, idx += 2)
			memcpy((obj + i), (const void *)(ring + idx), 32);
		switch (n & 0x1) {
		case 1:
			memcpy((obj + i), (const void *)(ring + idx), 16);
		}
	} else {
		for (i = 0; idx < size; i++, idx++)
			memcpy((obj + i), (const void *)(ring + idx), 16);
		/* Start at the beginning */
		for (idx = 0; i < n; i++, idx++)
			memcpy((obj + i), (const void *)(ring + idx), 16);
	}
}

/* the actual dequeue of elements from the ring.
 * Placed here since identical code needed in both
 * single and multi producer enqueue functions.
 */
static __rte_always_inline void
__rte_ring_do_dequeue_elems(void *obj_table, const void *ring_table,
	uint32_t size, uint32_t idx, uint32_t esize, uint32_t num)
{
	/* 8B and 16B copies implemented individually to retain
	 * the current performance.
	 */
	if (esize == 8)
		__rte_ring_dequeue_elems_64(obj_table, ring_table, size,
				idx, num);
	else if (esize == 16)
		__rte_ring_dequeue_elems_128(obj_table, ring_table, size,
				idx, num);
	else {
		uint32_t scale, nr_idx, nr_num, nr_size;

		/* Normalize to uint32_t */
		scale = esize / sizeof(uint32_t);
		nr_num = num * scale;
		nr_idx = idx * scale;
		nr_size = size * scale;
		__rte_ring_dequeue_elems_32(obj_table, ring_table, nr_size,
				nr_idx, nr_num);
	}
}

static __rte_always_inline void
__rte_ring_dequeue_elems(struct rte_ring *r, uint32_t cons_head,
		void *obj_table, uint32_t esize, uint32_t num)
{
	__rte_ring_do_dequeue_elems(obj_table, &r[1], r->size,
			cons_head & r->mask, esize, num);
}

/* Between load and load. there might be cpu reorder in weak model
 * (powerpc/arm).
 * There are 2 choices for the users
 * 1.use rmb() memory barrier
 * 2.use one-direction load_acquire/store_release barrier
 * It depends on performance test results.
 */
#ifdef RTE_USE_C11_MEM_MODEL
#include "rte_ring_c11_pvt.h"
#else
#include "rte_ring_generic_pvt.h"
#endif

/**
 * @internal This function updates the producer head for enqueue
 *
 * @param r
 *   A pointer to the ring structure
 * @param is_sp
 *   Indicates whether multi-producer path is needed or not
 * @param n
 *   The number of elements we will want to enqueue, i.e. how far should the
 *   head be moved
 * @param behavior
 *   RTE_RING_QUEUE_FIXED:    Enqueue a fixed number of items from a ring
 *   RTE_RING_QUEUE_VARIABLE: Enqueue as many items as possible from ring
 * @param old_head
 *   Returns head value as it was before the move, i.e. where enqueue starts
 * @param new_head
 *   Returns the current/new head value i.e. where enqueue finishes
 * @param free_entries
 *   Returns the amount of free space in the ring BEFORE head was moved
 * @return
 *   Actual number of objects enqueued.
 *   If behavior == RTE_RING_QUEUE_FIXED, this will be 0 or n only.
 */
static __rte_always_inline unsigned int
__rte_ring_move_prod_head(struct rte_ring *r, unsigned int is_sp,
		unsigned int n, enum rte_ring_queue_behavior behavior,
		uint32_t *old_head, uint32_t *new_head,
		uint32_t *free_entries)
{
	return __rte_ring_headtail_move_head(&r->prod, &r->cons, r->capacity,
			is_sp, n, behavior, old_head, new_head, free_entries);
}

/**
 * @internal This function updates the consumer head for dequeue
 *
 * @param r
 *   A pointer to the ring structure
 * @param is_sc
 *   Indicates whether multi-consumer path is needed or not
 * @param n
 *   The number of elements we will want to dequeue, i.e. how far should the
 *   head be moved
 * @param behavior
 *   RTE_RING_QUEUE_FIXED:    Dequeue a fixed number of items from a ring
 *   RTE_RING_QUEUE_VARIABLE: Dequeue as many items as possible from ring
 * @param old_head
 *   Returns head value as it was before the move, i.e. where dequeue starts
 * @param new_head
 *   Returns the current/new head value i.e. where dequeue finishes
 * @param entries
 *   Returns the number of entries in the ring BEFORE head was moved
 * @return
 *   - Actual number of objects dequeued.
 *     If behavior == RTE_RING_QUEUE_FIXED, this will be 0 or n only.
 */
static __rte_always_inline unsigned int
__rte_ring_move_cons_head(struct rte_ring *r, unsigned int is_sc,
		unsigned int n, enum rte_ring_queue_behavior behavior,
		uint32_t *old_head, uint32_t *new_head,
		uint32_t *entries)
{
	return __rte_ring_headtail_move_head(&r->cons, &r->prod, 0,
			is_sc, n, behavior, old_head, new_head, entries);
}

/**
 * @internal Enqueue several objects on the ring
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of objects.
 * @param esize
 *   The size of ring element, in bytes. It must be a multiple of 4.
 *   This must be the same value used while creating the ring. Otherwise
 *   the results are undefined.
 * @param n
 *   The number of objects to add in the ring from the obj_table.
 * @param behavior
 *   RTE_RING_QUEUE_FIXED:    Enqueue a fixed number of items from a ring
 *   RTE_RING_QUEUE_VARIABLE: Enqueue as many items as possible from ring
 * @param is_sp
 *   Indicates whether to use single producer or multi-producer head update
 * @param free_space
 *   returns the amount of space after the enqueue operation has finished
 * @return
 *   Actual number of objects enqueued.
 *   If behavior == RTE_RING_QUEUE_FIXED, this will be 0 or n only.
 */
static __rte_always_inline unsigned int
__rte_ring_do_enqueue_elem(struct rte_ring *r, const void *obj_table,
		unsigned int esize, unsigned int n,
		enum rte_ring_queue_behavior behavior, unsigned int is_sp,
		unsigned int *free_space)
{
	uint32_t prod_head, prod_next;
	uint32_t free_entries;

	n = __rte_ring_move_prod_head(r, is_sp, n, behavior,
			&prod_head, &prod_next, &free_entries);
	if (n == 0)
		goto end;

	__rte_ring_enqueue_elems(r, prod_head, obj_table, esize, n);

	__rte_ring_update_tail(&r->prod, prod_head, prod_next, is_sp, 1);
end:
	if (free_space != NULL)
		*free_space = free_entries - n;
	return n;
}

/**
 * @internal Dequeue several objects from the ring
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of objects.
 * @param esize
 *   The size of ring element, in bytes. It must be a multiple of 4.
 *   This must be the same value used while creating the ring. Otherwise
 *   the results are undefined.
 * @param n
 *   The number of objects to pull from the ring.
 * @param behavior
 *   RTE_RING_QUEUE_FIXED:    Dequeue a fixed number of items from a ring
 *   RTE_RING_QUEUE_VARIABLE: Dequeue as many items as possible from ring
 * @param is_sc
 *   Indicates whether to use single consumer or multi-consumer head update
 * @param available
 *   returns the number of remaining ring entries after the dequeue has finished
 * @return
 *   - Actual number of objects dequeued.
 *     If behavior == RTE_RING_QUEUE_FIXED, this will be 0 or n only.
 */
static __rte_always_inline unsigned int
__rte_ring_do_dequeue_elem(struct rte_ring *r, void *obj_table,
		unsigned int esize, unsigned int n,
		enum rte_ring_queue_behavior behavior, unsigned int is_sc,
		unsigned int *available)
{
	uint32_t cons_head, cons_next;
	uint32_t entries;

	n = __rte_ring_move_cons_head(r, (int)is_sc, n, behavior,
			&cons_head, &cons_next, &entries);
	if (n == 0)
		goto end;

	__rte_ring_dequeue_elems(r, cons_head, obj_table, esize, n);

	__rte_ring_update_tail(&r->cons, cons_head, cons_next, is_sc, 0);

end:
	if (available != NULL)
		*available = entries - n;
	return n;
}

#if defined(RTE_TOOLCHAIN_GCC) && (GCC_VERSION >= 120000)
#pragma GCC diagnostic pop
#endif

#endif /* _RTE_RING_ELEM_PVT_H_ */
