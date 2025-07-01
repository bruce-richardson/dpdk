/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>
#include <sys/eventhandler.h>

#include <machine/bus.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_phys.h>

struct contigmem_buffer {
	void           *addr;
	int             refcnt;
	struct mtx      mtx;
	int             socket_id;  /* NUMA domain/socket this buffer belongs to */
};

struct contigmem_vm_handle {
	int             buffer_index;
};

static int              contigmem_load(void);
static int              contigmem_unload(void);
static int              contigmem_physaddr(SYSCTL_HANDLER_ARGS);

static d_mmap_single_t  contigmem_mmap_single;
static d_open_t         contigmem_open;
static d_close_t        contigmem_close;

static int              contigmem_num_buffers = RTE_CONTIGMEM_DEFAULT_NUM_BUFS;
static int64_t          contigmem_buffer_size = RTE_CONTIGMEM_DEFAULT_BUF_SIZE;
static bool             contigmem_coredump_enable;
static char             contigmem_socket_mem[256] = "";
static int64_t          contigmem_socket_sizes[MAXMEMDOM];
static int              contigmem_socket_count = 0;
static bool             contigmem_use_socket_mem = false;

static eventhandler_tag contigmem_eh_tag;
static struct contigmem_buffer contigmem_buffers[RTE_CONTIGMEM_MAX_NUM_BUFS];
static struct cdev     *contigmem_cdev = NULL;
static int              contigmem_refcnt;

TUNABLE_INT("hw.contigmem.num_buffers", &contigmem_num_buffers);
TUNABLE_QUAD("hw.contigmem.buffer_size", &contigmem_buffer_size);
TUNABLE_BOOL("hw.contigmem.coredump_enable", &contigmem_coredump_enable);
TUNABLE_STR("hw.contigmem.socket_mem", contigmem_socket_mem, sizeof(contigmem_socket_mem));

static SYSCTL_NODE(_hw, OID_AUTO, contigmem, CTLFLAG_RD, 0, "contigmem");

SYSCTL_INT(_hw_contigmem, OID_AUTO, num_buffers, CTLFLAG_RD,
	&contigmem_num_buffers, 0, "Number of contigmem buffers allocated");
SYSCTL_QUAD(_hw_contigmem, OID_AUTO, buffer_size, CTLFLAG_RD,
	&contigmem_buffer_size, 0, "Size of each contiguous buffer");
SYSCTL_INT(_hw_contigmem, OID_AUTO, num_references, CTLFLAG_RD,
	&contigmem_refcnt, 0, "Number of references to contigmem");
SYSCTL_BOOL(_hw_contigmem, OID_AUTO, coredump_enable, CTLFLAG_RD,
	&contigmem_coredump_enable, 0, "Include mapped buffers in core dump");
SYSCTL_STRING(_hw_contigmem, OID_AUTO, socket_mem, CTLFLAG_RD,
	contigmem_socket_mem, sizeof(contigmem_socket_mem),
	"Per-socket memory allocation specification");

static SYSCTL_NODE(_hw_contigmem, OID_AUTO, physaddr, CTLFLAG_RD, 0,
	"physaddr");

MALLOC_DEFINE(M_CONTIGMEM, "contigmem", "contigmem(4) allocations");

static int contigmem_modevent(module_t mod, int type, void *arg)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		error = contigmem_load();
		break;
	case MOD_UNLOAD:
		error = contigmem_unload();
		break;
	default:
		break;
	}

	return error;
}

moduledata_t contigmem_mod = {
	"contigmem",
	(modeventhand_t)contigmem_modevent,
	0
};

DECLARE_MODULE(contigmem, contigmem_mod, SI_SUB_DRIVERS, SI_ORDER_ANY);
MODULE_VERSION(contigmem, 1);

static struct cdevsw contigmem_ops = {
	.d_name         = "contigmem",
	.d_version      = D_VERSION,
	.d_flags        = D_TRACKCLOSE,
	.d_mmap_single  = contigmem_mmap_single,
	.d_open         = contigmem_open,
	.d_close        = contigmem_close,
};

/*
 * Parse size suffix (k, M, G, T) and return size in bytes.
 */
static int64_t
contigmem_parse_size_suffix(const char *str)
{
	char *endptr;
	unsigned long size;

	size = strtoul(str, &endptr, 10);
	if (endptr == str)  /* No digits found */
		return -1;

	switch (*endptr) {
	case 'T': size *= 1024; /* FALLTHROUGH */
	case 'G': size *= 1024; /* FALLTHROUGH */
	case 'M': size *= 1024; /* FALLTHROUGH */
	case 'k': size *= 1024; break;
	case '\0':
	case ',':
		/* No suffix, size is in bytes */
		break;
	default:
		return -1;
	}

	return (int64_t)size;
}

/*
 * Calculate the highest power of two that divides into all given sizes.
 * Minimum value is 16M since all sizes must be multiples of 16M.
 * Zero values are skipped.
 */
static int64_t
contigmem_highest_power_of_two_divisor(int64_t *sizes, int count)
{
	int result_trailing_zeros = 64;  /* Start with maximum possible */

	/* Find the minimum trailing zeros across all non-zero sizes */
	for (int i = 0; i < count; i++) {
		/* Skip zero values */
		if (sizes[i] == 0)
			continue;

		/* Use ffsl() to find the position of the lowest set bit */
		int trailing_zeros = ffsl(sizes[i]) - 1;  /* ffsl returns 1-based index */

		/* Keep track of the minimum trailing zeros across all sizes */
		if (trailing_zeros < result_trailing_zeros)
			result_trailing_zeros = trailing_zeros;
	}

	/* Convert trailing zeros to actual power of two value */
	return 1LL << result_trailing_zeros;
}

/*
 * Parse socket_mem string and configure per-socket memory allocation.
 * Format: "size1,size2,size3,..." where each size can have suffix k,M,G,T
 * Each size must be multiple of 16M.
 * Buffer size will be the highest power of two that divides all sizes (min 16M).
 */
static int
contigmem_parse_socket_mem(const char *socket_mem_str)
{
	char *str_copy, *token, *remaining;
	int socket_id = 0;
	int64_t total_buffers = 0;
	int64_t gcd_size = 0;
	int i;

	if (strlen(socket_mem_str) == 0)
		return 0;  /* No socket_mem specified */

	str_copy = malloc(strlen(socket_mem_str) + 1, M_CONTIGMEM, M_WAITOK);
	strcpy(str_copy, socket_mem_str);
	remaining = str_copy;

	/* First pass: parse and validate sizes */
	while ((token = strsep(&remaining, ",")) != NULL && socket_id < MAXMEMDOM) {
		int64_t size = contigmem_parse_size_suffix(token);

		if (size < 0) {
			printf("Invalid socket memory size: %s\n", token);
			free(str_copy, M_CONTIGMEM);
			return EINVAL;
		}

		/* Check that non-zero size is multiple of 16M */
		if (size > 0 && size % (16 * 1024 * 1024) != 0) {
			printf("Socket memory size %s must be multiple of 16M\n", token);
			free(str_copy, M_CONTIGMEM);
			return EINVAL;
		}

		contigmem_socket_sizes[socket_id] = size;
		socket_id++;
	}

	contigmem_socket_count = socket_id;
	free(str_copy, M_CONTIGMEM);

	if (contigmem_socket_count == 0)
		return 0;

	/* Calculate highest power of two that divides all socket sizes (zeros are skipped) */
	gcd_size = contigmem_highest_power_of_two_divisor(contigmem_socket_sizes, contigmem_socket_count);

	/* Calculate total number of buffers needed (only for non-zero sockets) */
	for (i = 0; i < contigmem_socket_count; i++) {
		if (contigmem_socket_sizes[i] > 0) {
			total_buffers += contigmem_socket_sizes[i] / gcd_size;
		}
	}

	if (total_buffers == 0) {
		printf("At least one socket must have non-zero memory size\n");
		return EINVAL;
	}

	if (total_buffers > RTE_CONTIGMEM_MAX_NUM_BUFS) {
		printf("Total buffers (%ld) exceeds maximum (%d)\n",
			total_buffers, RTE_CONTIGMEM_MAX_NUM_BUFS);
		return EINVAL;
	}

	/* Update global configuration */
	contigmem_buffer_size = gcd_size;
	contigmem_num_buffers = (int)total_buffers;
	contigmem_use_socket_mem = true;

	printf("Socket memory configuration: buffer_size=%ld, num_buffers=%d\n",
		contigmem_buffer_size, contigmem_num_buffers);

	return 0;
}

/*
 * Helper function to setup a buffer and register its sysctl.
 */
static void
contigmem_setup_buffer(int buffer_idx, void *addr, int socket_id)
{
	char index_string[8], description[32];

	printf("%2u: virt=%p phys=%p socket=%d\n", buffer_idx, addr,
		(void *)pmap_kextract((vm_offset_t)addr), socket_id);

	mtx_init(&contigmem_buffers[buffer_idx].mtx, "contigmem", NULL, MTX_DEF);
	contigmem_buffers[buffer_idx].addr = addr;
	contigmem_buffers[buffer_idx].refcnt = 0;
	contigmem_buffers[buffer_idx].socket_id = socket_id;

	snprintf(index_string, sizeof(index_string), "%d", buffer_idx);
	snprintf(description, sizeof(description),
			"phys addr for buffer %d", buffer_idx);
	SYSCTL_ADD_PROC(NULL,
			&SYSCTL_NODE_CHILDREN(_hw_contigmem, physaddr), OID_AUTO,
			index_string, CTLTYPE_U64 | CTLFLAG_RD,
			(void *)(uintptr_t)buffer_idx, 0, contigmem_physaddr, "LU",
			description);
}

/*
 * Allocate buffers per socket according to socket_mem configuration.
 */
static int
contigmem_allocate_socket_buffers(void)
{
	void *socket_addr;
	int socket_id, buffers_per_socket, buffer_idx = 0;
	int i;

	for (socket_id = 0; socket_id < contigmem_socket_count; socket_id++) {
		/* Skip sockets with zero allocation */
		if (contigmem_socket_sizes[socket_id] == 0) {
			printf("Socket %d: skipping (zero allocation)\n", socket_id);
			continue;
		}

		buffers_per_socket = (int)(contigmem_socket_sizes[socket_id] / contigmem_buffer_size);

		/* First, try to allocate entire socket memory as one contiguous block */
		socket_addr = contigmalloc_domainset(contigmem_socket_sizes[socket_id], M_CONTIGMEM,
			DOMAINSET_FIXED(socket_id), M_ZERO,
			0, BUS_SPACE_MAXADDR, contigmem_buffer_size, 0);

		if (socket_addr != NULL) {
			/* Success: single large allocation */
			printf("Socket %d: allocated %ld bytes at virt=%p phys=%p (single block)\n", socket_id,
				contigmem_socket_sizes[socket_id], socket_addr,
				(void *)pmap_kextract((vm_offset_t)socket_addr));

			/* Divide the socket allocation into individual buffers */
			for (i = 0; i < buffers_per_socket; i++) {
				void *addr = (char *)socket_addr + (i * contigmem_buffer_size);
				contigmem_setup_buffer(buffer_idx++, addr, socket_id);
			}
		} else {
			/* Fallback: allocate buffers individually */
			printf("Socket %d: large allocation failed, falling back to individual buffers\n", socket_id);

			for (i = 0; i < buffers_per_socket; i++) {
				void *addr = contigmalloc_domainset(contigmem_buffer_size, M_CONTIGMEM,
					DOMAINSET_FIXED(socket_id), M_ZERO,
					0, BUS_SPACE_MAXADDR, contigmem_buffer_size, 0);
				if (addr == NULL) {
					printf("contigmalloc failed for buffer %d on socket %d\n",
						buffer_idx, socket_id);
					return ENOMEM;
				}

				contigmem_setup_buffer(buffer_idx++, addr, socket_id);
			}
		}
	}

	return 0;
}

/*
 * Allocate buffers using legacy interleaved allocation method.
 */
static int
contigmem_allocate_legacy_buffers(void)
{
	void *addr;
	int i;

	for (i = 0; i < contigmem_num_buffers; i++) {
		addr = contigmalloc_domainset(contigmem_buffer_size, M_CONTIGMEM,
			DOMAINSET_IL(), M_ZERO,
			0, BUS_SPACE_MAXADDR, contigmem_buffer_size, 0);
		if (addr == NULL) {
			printf("contigmalloc failed for buffer %d\n", i);
			return ENOMEM;
		}

		contigmem_setup_buffer(i, addr, -1);  /* -1 for unknown/interleaved socket */
	}

	return 0;
}

static int
contigmem_load(void)
{
	int error = 0;

	/* Parse socket_mem configuration if provided */
	error = contigmem_parse_socket_mem(contigmem_socket_mem);
	if (error != 0)
		goto error;

	if (contigmem_num_buffers > RTE_CONTIGMEM_MAX_NUM_BUFS) {
		printf("%d buffers requested is greater than %d allowed\n",
				contigmem_num_buffers, RTE_CONTIGMEM_MAX_NUM_BUFS);
		error = EINVAL;
		goto error;
	}

	if (contigmem_buffer_size < PAGE_SIZE ||
			(contigmem_buffer_size & (contigmem_buffer_size - 1)) != 0) {
		printf("buffer size 0x%lx is not greater than PAGE_SIZE and "
				"power of two\n", contigmem_buffer_size);
		error = EINVAL;
		goto error;
	}

	/* Allocate buffers using appropriate method */
	error = contigmem_use_socket_mem ?
			contigmem_allocate_socket_buffers() :
			contigmem_allocate_legacy_buffers();
	if (error != 0)
		goto error;

	contigmem_cdev = make_dev_credf(0, &contigmem_ops, 0, NULL, UID_ROOT,
			GID_WHEEL, 0600, "contigmem");

	return 0;

error:
	for (int i = 0; i < contigmem_num_buffers; i++) {
		if (contigmem_buffers[i].addr != NULL) {
			contigfree(contigmem_buffers[i].addr,
				contigmem_buffer_size, M_CONTIGMEM);
			contigmem_buffers[i].addr = NULL;
		}
		if (mtx_initialized(&contigmem_buffers[i].mtx))
			mtx_destroy(&contigmem_buffers[i].mtx);
	}

	return error;
}

static int
contigmem_unload(void)
{
	int i;

	if (contigmem_refcnt > 0)
		return EBUSY;

	if (contigmem_cdev != NULL)
		destroy_dev(contigmem_cdev);

	if (contigmem_eh_tag != NULL)
		EVENTHANDLER_DEREGISTER(process_exit, contigmem_eh_tag);

	for (i = 0; i < RTE_CONTIGMEM_MAX_NUM_BUFS; i++) {
		if (contigmem_buffers[i].addr != NULL)
			contigfree(contigmem_buffers[i].addr,
				contigmem_buffer_size, M_CONTIGMEM);
		if (mtx_initialized(&contigmem_buffers[i].mtx))
			mtx_destroy(&contigmem_buffers[i].mtx);
	}

	return 0;
}

static int
contigmem_physaddr(SYSCTL_HANDLER_ARGS)
{
	uint64_t	physaddr;
	int		index = (int)(uintptr_t)arg1;

	physaddr = (uint64_t)vtophys(contigmem_buffers[index].addr);
	return sysctl_handle_64(oidp, &physaddr, 0, req);
}

static int
contigmem_open(struct cdev *cdev, int fflags, int devtype,
		struct thread *td)
{

	atomic_add_int(&contigmem_refcnt, 1);

	return 0;
}

static int
contigmem_close(struct cdev *cdev, int fflags, int devtype,
		struct thread *td)
{

	atomic_subtract_int(&contigmem_refcnt, 1);

	return 0;
}

static int
contigmem_cdev_pager_ctor(void *handle, vm_ooffset_t size, vm_prot_t prot,
		vm_ooffset_t foff, struct ucred *cred, u_short *color)
{
	struct contigmem_vm_handle *vmh = handle;
	struct contigmem_buffer *buf;

	buf = &contigmem_buffers[vmh->buffer_index];

	atomic_add_int(&contigmem_refcnt, 1);

	mtx_lock(&buf->mtx);
	if (buf->refcnt == 0)
		memset(buf->addr, 0, contigmem_buffer_size);
	buf->refcnt++;
	mtx_unlock(&buf->mtx);

	return 0;
}

static void
contigmem_cdev_pager_dtor(void *handle)
{
	struct contigmem_vm_handle *vmh = handle;
	struct contigmem_buffer *buf;

	buf = &contigmem_buffers[vmh->buffer_index];

	mtx_lock(&buf->mtx);
	buf->refcnt--;
	mtx_unlock(&buf->mtx);

	free(vmh, M_CONTIGMEM);

	atomic_subtract_int(&contigmem_refcnt, 1);
}

static int
contigmem_cdev_pager_fault(vm_object_t object, vm_ooffset_t offset, int prot,
		vm_page_t *mres)
{
	vm_paddr_t paddr;
	vm_page_t m_paddr, page;
	vm_memattr_t memattr, memattr1;

	memattr = object->memattr;

	VM_OBJECT_WUNLOCK(object);

	paddr = offset;

	m_paddr = vm_phys_paddr_to_vm_page(paddr);
	if (m_paddr != NULL) {
		memattr1 = pmap_page_get_memattr(m_paddr);
		if (memattr1 != memattr)
			memattr = memattr1;
	}

	if (((*mres)->flags & PG_FICTITIOUS) != 0) {
		/*
		 * If the passed in result page is a fake page, update it with
		 * the new physical address.
		 */
		page = *mres;
		VM_OBJECT_WLOCK(object);
		vm_page_updatefake(page, paddr, memattr);
	} else {
		/*
		 * Replace the passed in reqpage page with our own fake page and
		 * free up the original page.
		 */
		page = vm_page_getfake(paddr, memattr);
		VM_OBJECT_WLOCK(object);
#if __FreeBSD__ >= 13
		vm_page_replace(page, object, (*mres)->pindex, *mres);
#else
		vm_page_t mret = vm_page_replace(page, object, (*mres)->pindex);
		KASSERT(mret == *mres,
		    ("invalid page replacement, old=%p, ret=%p", *mres, mret));
		vm_page_lock(mret);
		vm_page_free(mret);
		vm_page_unlock(mret);
#endif
		*mres = page;
	}

	page->valid = VM_PAGE_BITS_ALL;

	return VM_PAGER_OK;
}

static struct cdev_pager_ops contigmem_cdev_pager_ops = {
	.cdev_pg_ctor = contigmem_cdev_pager_ctor,
	.cdev_pg_dtor = contigmem_cdev_pager_dtor,
	.cdev_pg_fault = contigmem_cdev_pager_fault,
};

static int
contigmem_mmap_single(struct cdev *cdev, vm_ooffset_t *offset, vm_size_t size,
		struct vm_object **obj, int nprot)
{
	struct contigmem_vm_handle *vmh;
	uint64_t buffer_index;

	/*
	 * The buffer index is encoded in the offset.  Divide the offset by
	 *  PAGE_SIZE to get the index of the buffer requested by the user
	 *  app.
	 */
	buffer_index = *offset / PAGE_SIZE;
	if (buffer_index >= contigmem_num_buffers)
		return EINVAL;

	if (size > contigmem_buffer_size)
		return EINVAL;

	vmh = malloc(sizeof(*vmh), M_CONTIGMEM, M_NOWAIT | M_ZERO);
	if (vmh == NULL)
		return ENOMEM;
	vmh->buffer_index = buffer_index;

	*offset = (vm_ooffset_t)vtophys(contigmem_buffers[buffer_index].addr);
	*obj = cdev_pager_allocate(vmh, OBJT_DEVICE, &contigmem_cdev_pager_ops,
			size, nprot, *offset, curthread->td_ucred);

	/* Mappings backed by OBJ_FICTITIOUS are excluded from core dump. */
	if (contigmem_coredump_enable)
		(*obj)->flags &= ~OBJ_FICTITIOUS;

	return 0;
}
