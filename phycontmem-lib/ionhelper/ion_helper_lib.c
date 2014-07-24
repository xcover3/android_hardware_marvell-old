#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/ion.h>
#include <linux/pxa_ion.h>

#include "ion_helper_lib.h"
#include "../phycontmem/phycontmem.h"

#undef ANDROID
#ifdef ANDROID
#define LOG_TAG "ion_helper"
#include <cutils/log.h>
#else
#define ALOGV(...)
#define ALOGD(...)
#define ALOGI(...)
#define ALOGW(...)
#define ALOGE(...)
#endif

struct ion_args *ion_malloc(int size, int attrs)
{
	struct ion_allocation_data ion_data;
	struct ion_fd_data fd_data = {0};
	struct ion_pxa_region ion_region = {0};
	struct ion_custom_data data = {0};
	struct ion_handle_data req = {0};
	struct ion_args *ion_args;
	int ret;

	ALOGI("%s() calling, sz %d, attrs %d\n", __FUNCTION__, size, attrs);

	ion_args = (struct ion_args *)calloc(1, sizeof(struct ion_args));
	if (!ion_args) {
		ALOGE("malloc failure %s, %d\n", __func__, __LINE__);
		return NULL;
	}
	ion_args->fd = open("/dev/ion", O_RDWR);
	if (ion_args->fd < 0) {
		ALOGE("open /dev/ion failure, ret:%d", ion_args->fd);
		goto out;
	}

	memset(&ion_data, 0, sizeof(struct ion_allocation_data));
	/* align to PAGE_SIZE */
	ion_data.len = (size + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);
	ion_data.align = PAGE_SIZE;
    /* ion_data.heap_id_mask = ION_HEAP_TYPE_DMA_MASK; */
    ion_data.heap_id_mask = ION_HEAP_CARVEOUT_MASK;
	/*
	 * attrs:
	 *	(0, nonCacheable & bufferable),
	 *	(nonzero, Cacheable & bufferable)
	 */
	if (attrs) {
		ion_data.flags = ION_FLAG_CACHED;
		ion_data.flags |= ION_FLAG_CACHED_NEEDS_SYNC;
	}
	ret = ioctl(ion_args->fd, ION_IOC_ALLOC, &ion_data);
	if (ret < 0) {
		ALOGE("failed to allocate memory, ret:%d", ret);
		goto out_alloc;
	}

	ion_args->size = ion_data.len;
	ion_args->handle = ion_data.handle;
	fd_data.handle = ion_data.handle;
	ret = ioctl(ion_args->fd, ION_IOC_SHARE, &fd_data);
	if (ret < 0) {
		ALOGE("failed to share memory, ret:%d", ret);
		goto out_share;
	}

	ion_region.handle = fd_data.handle;
	data.cmd = ION_PXA_PHYS;
	data.arg = (unsigned long)&ion_region;
	ret = ioctl(ion_args->fd, ION_IOC_CUSTOM, &data);
	if (ret < 0) {
		ALOGE("failed to get physical address, ret:%d", ret);
		goto out_share;
	}
	ion_args->buf_fd = fd_data.fd;
	ion_args->pa = (void*)(ion_region.addr);

	ion_args->va = mmap(NULL, ion_args->size, PROT_READ
				| PROT_WRITE, MAP_SHARED, fd_data.fd, 0);
	if (ion_args->va == MAP_FAILED) {
		ALOGE("failed to maping");
		goto out_share;
	}
#if 0
	/*
	 * make sure virtual address is loaded into page table
	 * workaround if ION_FLAG_PXA_SYNC doesn't exist
	 */
	memset(ion_args->va, 0, ion_args->size);
#endif

	ALOGI("%s() ok, sz %d, va 0x%08x, pa 0x%08x, buf_fd %d, ion_args 0x%08x\n", __FUNCTION__, size, (unsigned int)ion_args->va, (unsigned int)ion_args->pa, ion_args->buf_fd, (unsigned int)ion_args);

	return ion_args;
out_share:
	memset(&req, 0, sizeof(struct ion_handle_data));
	req.handle = ion_args->handle;
	ret = ioctl(ion_args->fd, ION_IOC_FREE, &req);
	if (ret < 0) {
		ALOGE("Failed to free ION buffer, ret:%d", ret);
		return NULL;
	}
out_alloc:
	close(ion_args->fd);
out:
	free(ion_args);
	return NULL;
}

int ion_free(struct ion_args *ion_args)
{
	struct ion_handle_data req = {0};
	int ret;

	ALOGI("%s() calling, ion_args 0x%08x\n", __FUNCTION__, (unsigned int)ion_args);

	if (!ion_args)
		return -1;
	if (ion_args->fd < 0)
		return -2;
	munmap(ion_args->va, ion_args->size);
	req.handle = ion_args->handle;
	ret = ioctl(ion_args->fd, ION_IOC_FREE, &req);
	if (ret < 0) {
		ALOGE("Failed to free ION buffer, ret:%d", ret);
		ALOGE("fd:%d, handle:0x%x", ion_args->fd,
			(unsigned int)ion_args->handle);
		return -3;
	}
	/* close buf_fd to release dmabuf */
	close(ion_args->buf_fd);
	close(ion_args->fd);
	ALOGI("%s() ok before calling free for ion_args 0x%08x\n", __FUNCTION__, (unsigned int)ion_args);
	free(ion_args);

	return 0;
}

void ion_flush_cache(struct ion_args *ion_args, int offset, int size, int dir)
{
	struct ion_pxa_cache_region region;
	struct ion_custom_data data;

	memset(&region, 0, sizeof(struct ion_pxa_cache_region));
	memset(&data, 0, sizeof(struct ion_custom_data));
	region.handle = ion_args->handle;
	region.offset = offset;
	region.len = size;
	data.cmd = ION_PXA_SYNC;
	data.arg = (unsigned long)&region;
	ioctl(ion_args->fd, ION_IOC_CUSTOM, &data);
}
