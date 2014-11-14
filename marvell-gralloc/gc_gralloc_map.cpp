/****************************************************************************
**
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
*****************************************************************************/

#include <limits.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <linux/version.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

#include <hardware/hardware.h>
#include <hardware/gralloc.h>
#include "gc_gralloc_priv.h"
#include "gc_gralloc_gr.h"
#include <gc_hal_user.h>
#include <gc_hal_base.h>

#if HAVE_ANDROID_OS

#if (MRVL_VIDEO_MEMORY_USE_TYPE == gcdMEM_TYPE_ION)
#include <linux/ion.h>
#include <linux/pxa_ion.h>
#else
#include <linux/android_pmem.h>
#endif
#endif

#if gcdDEBUG
/* Must be included after Vivante headers. */
#include <utils/Timers.h>
#endif

/* We need thsi for now because pmem can't mmap at an offset. */
#define PMEM_HACK       1

/* desktop Linux needs a little help with gettid() */
#if defined(ARCH_X86) && !defined(HAVE_ANDROID_OS)
#define __KERNEL__
# include <linux/unistd.h>
pid_t gettid() { return syscall(__NR_gettid);}
#undef __KERNEL__
#endif

#if (MRVL_VIDEO_MEMORY_USE_TYPE == gcdMEM_TYPE_ION)

static int ion_flush(gc_private_handle_t *hnd);
static int ion_mmap(gc_private_handle_t *hnd)
{
    struct ion_fd_data req;
    void *mappedAddr;
    size_t size = 0;
    int ret = 0;

    mappedAddr = NULL;
    memset(&req, 0, sizeof(struct ion_fd_data));
    req.fd = hnd->master;
    ret = ioctl(hnd->fd, ION_IOC_IMPORT, &req);
    if (ret < 0)
        goto out;
    size = hnd->size;
#if PMEM_HACK
    size += hnd->offset;
#endif
    mappedAddr = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, hnd->master, 0);
    if(mappedAddr == MAP_FAILED)
    {
        ALOGE("Could not mmap %s", strerror(errno));
        return -errno;
    }

    hnd->base = intptr_t(mappedAddr) + hnd->offset;
    return 0;
out:
    ALOGE("ION MAP failed (%s), fd=%d, shared fd:%d, size:%d",
         strerror(errno), hnd->fd, hnd->master, size);
    return ret;
}

#else
static int pmem_mmap(gc_private_handle_t *hnd)
{
    struct pmem_region sub;
    size_t size = roundUpToPageSize(hnd->size);
    int ret;
    void *mappedAddr;
    mappedAddr = NULL;
    hnd->fd = open("/dev/pmem", O_RDWR, 0);
    if (hnd->fd < 0)
        return -EINVAL;
    ret = ioctl(hnd->fd, PMEM_CONNECT, hnd->master);
    if (ret < 0)
        goto out;
    sub.offset = 0;
    sub.len = size;
    ret = ioctl(hnd->fd, PMEM_MAP, &sub);
    if (ret < 0)
        goto out;
    size = hnd->size;
#if PMEM_HACK
    size += hnd->offset;
#endif
    mappedAddr = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, hnd->fd, 0);

    if(mappedAddr == MAP_FAILED)
    {
        ALOGE("Could not mmap %s", strerror(errno));
        return -errno;
    }

    hnd->base = intptr_t(mappedAddr) + hnd->offset;
    return 0;
out:
    ALOGE("PMEM_MAP failed (%s), fd=%d, master=%d, sub.offset=%lu, sub.len=%lu",
         strerror(errno), hnd->fd, hnd->master, sub.offset, sub.len);
    return ret;
}
#endif

/*******************************************************************************
**
**  gc_gralloc_map
**
**  Map android native buffer to userspace.
**  This is not for client. So only the linear surface is exported since only
**  linear surface can be used for software access.
**  Reference table in gralloc_alloc_buffer(gc_gralloc_alloc.cpp)
**
**  INPUT:
**
**      gralloc_module_t const * Module
**          Specified gralloc module.
**
**      buffer_handle_t Handle
**          Specified buffer handle.
**
**  OUTPUT:
**
**      void ** Vaddr
**          Point to save virtual address pointer.
*/
int
gc_gralloc_map(
    gralloc_module_t const * Module,
    buffer_handle_t Handle,
    void ** Vaddr
    )
{
    gc_private_handle_t * hnd = (gc_private_handle_t *) Handle;

    /* Map if non-framebuffer and not mapped. */
    if (!(hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER))
    {
        if(hnd->flags & private_handle_t::PRIV_FLAGS_USES_PMEM)
        {
            int err;
#if (MRVL_VIDEO_MEMORY_USE_TYPE == gcdMEM_TYPE_ION)
            err = ion_mmap(hnd);
#else
            err = pmem_mmap(hnd);
#endif
            if (err < 0)
                goto OnError;
        }
        else
        {
            gctUINT32 Address[3];
            gctPOINTER Memory[3];
            gceSTATUS status;

            gcoSURF surface = hnd->surface ? (gcoSURF) hnd->surface
                            : (gcoSURF) hnd->resolveSurface;

            /* Lock surface for memory. */
            gcmONERROR(
                gcoSURF_Lock(surface,
                             Address,
                             Memory));

            hnd->physAddr = (int) Address[0];
            hnd->base = intptr_t(Memory[0]);

            ALOGV("mapped buffer "
                 "hnd=%p, "
                 "surface=%p, "
                 "physical=%p, "
                 "logical=%p",
                 (void *) hnd,
                 surface,
                 (void *) hnd->physAddr, Memory[0]);
        }
    }

    /* Obtain virtual address. */
    *Vaddr = (void *) hnd->base;

    return 0;

OnError:
    ALOGE("Failed to map hnd=%p",
         (void *) hnd);

    return -EINVAL;
}

#if (MRVL_VIDEO_MEMORY_USE_TYPE == gcdMEM_TYPE_ION)
static int ion_unmap(gc_private_handle_t *hnd)
{
    void *base;
    size_t size;

    if (hnd->fd < 0)
        return -EINVAL;
    base = (void *)hnd->base;
    size = hnd->size;
#if PMEM_HACK
    base = (void*)(intptr_t(base) - hnd->offset);
    size += hnd->offset;
#endif
    if(munmap(base, size) < 0)
    {
        ALOGE("Could not unmap %s", strerror(errno));
    }

    hnd->base = 0;
    return 0;
}
#else
static int pmem_unmap(gc_private_handle_t *hnd)
{
    struct pmem_region sub;
    size_t size = roundUpToPageSize(hnd->size);
    void *base;
    int ret;

    if (hnd->fd < 0)
        return -EINVAL;
    sub.offset = hnd->offset;
    sub.len = size;
    ret = ioctl(hnd->fd, PMEM_UNMAP, &sub);
    if (ret < 0) {
        goto out;
    }
    base = (void *)hnd->base;
    size = hnd->size;
#if PMEM_HACK
    base = (void*)(intptr_t(base) - hnd->offset);
    size += hnd->offset;
#endif
    if(munmap(base, size) < 0)
    {
        ALOGE("Could not unmap %s", strerror(errno));
    }

    hnd->base = 0;
    if(hnd->fd > 0)
    {
        close(hnd->fd);
        hnd->fd = -1;
    }
    return 0;
out:
    ALOGE("PMEM_UNMAP failed (%s), fd=%d, sub.offset=%lu, sub.len=%lu",
         strerror(errno), hnd->fd, sub.offset, sub.len);
    close(hnd->fd);
    return ret;
}
#endif
/*******************************************************************************
**
**  gc_gralloc_unmap
**
**  Unmap mapped android native buffer.
**
**  INPUT:
**
**      gralloc_module_t const * Module
**          Specified gralloc module.
**
**      buffer_handle_t Handle
**          Specified buffer handle.
**
**  OUTPUT:
**      Nothing.
**
*/
int
gc_gralloc_unmap(
    gralloc_module_t const * Module,
    buffer_handle_t Handle
    )
{
    gc_private_handle_t * hnd = (gc_private_handle_t *) Handle;

    /* Only unmap non-framebuffer and mapped buffer. */
    if (!(hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER))
    {
        if(hnd->flags & private_handle_t::PRIV_FLAGS_USES_PMEM)
        {
            int ret;
#if (MRVL_VIDEO_MEMORY_USE_TYPE == gcdMEM_TYPE_ION)
            ret = ion_unmap(hnd);
#else
            ret = pmem_unmap(hnd);
#endif
            if (ret < 0)
                goto OnError;
        }
        else
        {
            gceSTATUS status;

            gcoSURF surface = hnd->surface ? (gcoSURF) hnd->surface
                            : (gcoSURF) hnd->resolveSurface;

            /* Unlock surface. */
            gcmONERROR(
                gcoSURF_Unlock(surface,
                               gcvNULL));

            hnd->physAddr = 0;
            hnd->base = 0;
        }

        ALOGV("unmapped buffer hnd=%p", (void *) hnd);
    }

    return 0;

OnError:
    ALOGV("Failed to unmap hnd=%p", (void *) hnd);

    return -EINVAL;
}

/*******************************************************************************
**
**  gc_gralloc_register_buffer
**
**  Register buffer to current process (in client).
**
**  INPUT:
**
**      gralloc_module_t const * Module
**          Specified gralloc module.
**
**      buffer_handle_t Handle
**          Specified buffer handle.
**
**  OUTPUT:
**      Nothing.
**
*/
int
gc_gralloc_register_buffer(
    gralloc_module_t const * Module,
    buffer_handle_t Handle
    )
{
    int err = 0;
    gc_private_handle_t* hnd = (gc_private_handle_t *) Handle;

    gcoSURF surface         = gcvNULL;
    gcoSURF resolveSurface  = gcvNULL;
    gctSIGNAL signal        = gcvNULL;
    gceSTATUS status        = gcvSTATUS_OK;
    void * vaddr            = gcvNULL;

    if (gc_private_handle_t::validate(Handle) < 0)
    {
        return -EINVAL;
    }

#ifdef BUILD_GOOGLETV
    /* No need to do more things for these formats. */
    if (hnd->format == HAL_PIXEL_FORMAT_GTV_VIDEO_HOLE ||
        hnd->format == HAL_PIXEL_FORMAT_GTV_OPAQUE_BLACK)
    {
        return 0;
    }
#endif

    gcmONERROR(
        gcoOS_ModuleConstructor());

    /* Also must set client hardware type t 3D. */
    gcmONERROR(
        gcoHAL_SetHardwareType(gcvNULL, gcvHARDWARE_3D));

    /* if this handle was created in this process, then we keep it as is. */
    if (hnd->pid != getpid())
    {
        /* Register linear surface if exists. */
        if (hnd->surface != 0)
        {
            if(hnd->flags & private_handle_t::PRIV_FLAGS_USES_PMEM)
            {
                /* 2D App, use pmem. */
                gcmONERROR(
                    gcoSURF_Construct(gcvNULL,
                                      (gctUINT) hnd->width,
                                      (gctUINT) hnd->height,
                                      1,
                                      gcvSURF_BITMAP,
                                      (gceSURF_FORMAT)hnd->surfFormat,
                                      (gcePOOL) hnd->pool,
                                      (gcoSURF *)&surface));

                err = gc_gralloc_map(Module, Handle, &vaddr);

                if(err == 0)
                {
                    gcmONERROR(
                        gcoSURF_MapUserSurface(surface,
                                               0,
                                               (void*)hnd->base,
                                               ~0));

                    hnd->surface = (int)surface;
                }
            }
            else
            {
                /* 3D App: texture surface use linear. */
                gcmONERROR(
                    gcoSURF_Construct(gcvNULL,
                                      (gctUINT) hnd->width,
                                      (gctUINT) hnd->height,
                                      1,
                                      gcvSURF_BITMAP_NO_VIDMEM,
                                      (gceSURF_FORMAT) hnd->surfFormat,
                                      (gcePOOL) hnd->pool,
                                      &surface));

                /* Save surface info. */
                surface->info.node.u.normal.node = (gcuVIDMEM_NODE_PTR) hnd->vidNode;
                surface->info.node.pool          = (gcePOOL) hnd->pool;
                surface->info.node.size          = hnd->adjustedSize;

                /* Lock once as it's done in gcoSURF_Construct with vidmem
                by ioctrl to kernel mode to get physical and virtual address. */
                gcmONERROR(
                    gcoSURF_Lock(surface,
                                 gcvNULL,
                                 gcvNULL));

                hnd->surface = (int)surface;
            }

            ALOGV("registered buffer hnd=%p, "
                 "vidNode=%p, "
                 "surface=%p",
                 (void *) hnd,
                 (void *) hnd->vidNode,
                 (void *) hnd->surface);
        }

#if !gcdGPU_LINEAR_BUFFER_ENABLED
        /* 3D App: texture surface use tile. */
        /* Register tile surface if exists. */
        if (hnd->resolveSurface != 0)
        {
            gceSURF_FORMAT resolveFormat = gcvSURF_UNKNOWN;

            gcmONERROR(
                gcoTEXTURE_GetClosestFormat(gcvNULL,
                                            (gceSURF_FORMAT) hnd->surfFormat,
                                            &resolveFormat));

            /* This video node was created in another process.
             * Import it into this process.. */
            gcmONERROR(
                gcoSURF_Construct(gcvNULL,
                                  (gctUINT) hnd->width,
                                  (gctUINT) hnd->height,
                                  1,
                                  gcvSURF_TEXTURE_NO_VIDMEM,
                                  resolveFormat,
                                  gcvPOOL_DEFAULT,
                                  &resolveSurface));

            /* Save surface info. */
            resolveSurface->info.node.u.normal.node = (gcuVIDMEM_NODE_PTR) hnd->resolveVidNode;
            resolveSurface->info.node.pool          = (gcePOOL) hnd->resolvePool;
            resolveSurface->info.node.size          = hnd->resolveAdjustedSize;

            gcmONERROR(
                gcoSURF_Lock(resolveSurface,
                             gcvNULL,
                             gcvNULL));

            hnd->resolveSurface = (int) resolveSurface;

            ALOGV("registered resolve buffer hnd=%p, "
                 "resolveVidNode=%p, "
                 "resolveSurface=%p",
                 (void *) hnd,
                 (void *) hnd->resolveVidNode,
                 (void *) hnd->resolveSurface);

            /* HW surfaces are bottom-top. */
            gcmONERROR(
                gcoSURF_SetOrientation((gcoSURF)hnd->resolveSurface,
                                       gcvORIENTATION_BOTTOM_TOP));
        }
#endif

        /* Map signal if exists. */
        if (hnd->hwDoneSignal != 0)
        {
            /* Map signal to be used in CPU/GPU synchronization. */
            gcmONERROR(
                gcoOS_MapSignal((gctSIGNAL) hnd->hwDoneSignal,
                                &signal));

            hnd->hwDoneSignal = (int) signal;

            ALOGV("Created hwDoneSignal=%p for hnd=%p", signal, hnd);
        }

        /* This *IS* the client. Make sure server has the correct information. */
        gcmASSERT(hnd->clientPID == getpid());

        if(!(hnd->flags & private_handle_t::PRIV_FLAGS_USES_PMEM))
        {
            err = gc_gralloc_map(Module, Handle, &vaddr);
        }

        hnd->pid = getpid();
    }

    return err;

OnError:
    /* Error roll back. */
    if (surface != gcvNULL)
    {
        gcmVERIFY_OK(
            gcoSURF_Destroy(surface));

        hnd->surface = 0;
    }

    if (resolveSurface != gcvNULL)
    {
        gcmVERIFY_OK(
            gcoSURF_Destroy(resolveSurface));

        hnd->resolveSurface = 0;
    }

    if (signal)
    {
        gcmVERIFY_OK(
            gcoOS_DestroySignal(gcvNULL, (gctSIGNAL)hnd->hwDoneSignal));

        hnd->hwDoneSignal = 0;
    }

    ALOGE("Failed to register buffer hnd=%p", (void *) hnd);

    return -EINVAL;
}

/*******************************************************************************
**
**  gc_gralloc_unregister_buffer
**
**  Unregister buffer in current process (in client).
**
**  INPUT:
**
**      gralloc_module_t const * Module
**          Specified gralloc module.
**
**      buffer_handle_t Handle
**          Specified buffer handle.
**
**  OUTPUT:
**      Nothing.
**
*/
int
gc_gralloc_unregister_buffer(
    gralloc_module_t const * Module,
    buffer_handle_t Handle
    )
{
    gceSTATUS status;

    if (gc_private_handle_t::validate(Handle) < 0)
    {
        return -EINVAL;
    }

    gc_private_handle_t* hnd = (gc_private_handle_t*)Handle;

#ifdef BUILD_GOOGLETV
    /* No need to do more things for these formats. */
    if (hnd->format == HAL_PIXEL_FORMAT_GTV_VIDEO_HOLE ||
        hnd->format == HAL_PIXEL_FORMAT_GTV_OPAQUE_BLACK)
    {
        return 0;
    }
#endif

    if (hnd->base != 0)
    {
        gc_gralloc_unmap(Module, Handle);
    }

    /* Unregister linear surface if exists. */
    if (hnd->surface != 0)
    {
        /* Non-3D rendering...
         * Append the gcvSURF_NO_VIDMEM flag to indicate we are only destroying
         * the wrapper here, actual vid mem node is destroyed in the process
         * that created it. */
        gcoSURF surface = (gcoSURF) hnd->surface;

        gcmONERROR(
            gcoSURF_Destroy(surface));

        ALOGV("unregistered buffer hnd=%p, "
             "surface=%p", (void*)hnd, (void*)hnd->surface);
    }

    /* Unregister tile surface if exists. */
    if (hnd->resolveSurface != 0)
    {
        /* Only destroying the wrapper here, actual vid mem node is
         * destroyed in the process that created it. */
        gcoSURF surface = (gcoSURF) hnd->resolveSurface;

        gcmONERROR(
            gcoSURF_Destroy(surface));

        ALOGV("unregistered buffer hnd=%p, "
             "resolveSurface=%p", (void*)hnd, (void*)hnd->resolveSurface);
    }

    /* Destroy signal if exist. */
    if (hnd->hwDoneSignal)
    {
        gcmONERROR(
            gcoOS_UnmapSignal((gctSIGNAL)hnd->hwDoneSignal));
        hnd->hwDoneSignal = 0;
    }

    hnd->clientPID = 0;

    /* Commit the command buffer. */
    gcmONERROR(
        gcoHAL_Commit(gcvNULL, gcvFALSE));

    return 0;

OnError:
    ALOGE("Failed to unregister buffer, hnd=%p", (void *) hnd);

    return -EINVAL;
}

/*******************************************************************************
**
**  gc_gralloc_lock
**
**  Lock android native buffer and get address.
**
**  INPUT:
**
**      gralloc_module_t const * Module
**          Specified gralloc module.
**
**      buffer_handle_t Handle
**          Specified buffer handle.
**
**      int Usage
**          Usage for lock.
**
**      int Left, Top, Width, Height
**          Lock area.
**
**  OUTPUT:
**
**      void ** Vaddr
**          Point to save virtual address pointer.
*/
int
gc_gralloc_lock(
    gralloc_module_t const* Module,
    buffer_handle_t Handle,
    int Usage,
    int Left,
    int Top,
    int Width,
    int Height,
    void ** Vaddr
    )
{
    if (gc_private_handle_t::validate(Handle) < 0)
        return -EINVAL;

    gc_private_handle_t* hnd = (gc_private_handle_t*)Handle;
    *Vaddr = (void *) hnd->base;

    ALOGV("To lock buffer hnd=%p, for usage=0x%08X, in (l=%d, t=%d, w=%d, h=%d)",
         (void *) hnd, Usage, Left, Top, Width, Height);

    if (!(hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER))
    {
        hnd->lockUsage = (int) Usage;

#if gcdCOPYBLT_OPTIMIZATION
        if (Usage & GRALLOC_USAGE_SW_WRITE_MASK)
        {
            /* Record the dirty area. */
            hnd->dirtyX      = Left;
            hnd->dirtyY      = Top;
            hnd->dirtyWidth  = Width;
            hnd->dirtyHeight = Height;
        }
#endif
        /* Is SW rendering to PMEM instead of framebuffer, we need to flush on unlock. Set cache flush flag. */
        if((hnd->flags & private_handle_t::PRIV_FLAGS_USES_PMEM)&&(!(Usage & GRALLOC_USAGE_HW_RENDER)))
        {
            hnd->flags |= private_handle_t::PRIV_FLAGS_NEEDS_FLUSH;
        }

        if (hnd->hwDoneSignal != 0)
        {
            ALOGV("Waiting for compositor hnd=%p, "
                 "for hwDoneSignal=0x%08X",
                 (void *) hnd,
                 hnd->hwDoneSignal);

            /* Wait for compositor (2D, 3D, CE) to source from the CPU buffer. */
            gcmVERIFY_OK(
                gcoOS_WaitSignal(gcvNULL,
                                 (gctSIGNAL) hnd->hwDoneSignal,
                                 gcvINFINITE));
        }
    }


    /* ALOGV("locked buffer hnd=%p", (void *) hnd); */
    return 0;
}

#if (MRVL_VIDEO_MEMORY_USE_TYPE == gcdMEM_TYPE_ION)
static int ion_flush(gc_private_handle_t *hnd)
{
    struct ion_fd_data req;
    struct ion_pxa_cache_region region;
    struct ion_custom_data data;
    int flush_fd, ret;

    memset(&req, 0, sizeof(struct ion_fd_data));
    req.fd = hnd->master;
    ret = ioctl(hnd->fd, ION_IOC_IMPORT, &req);
    if (ret < 0)
        goto out;
    memset(&region, 0, sizeof(struct ion_pxa_cache_region));
    memset(&data, 0, sizeof(struct ion_custom_data));
    region.handle = req.handle;
    if (hnd->bpr > 0)
    {
        // Partial cache flush for normal gralloc buffer
        int offset = hnd->dirtyY * hnd->bpr;
        int len = hnd->dirtyHeight * hnd->bpr;
        int left = hnd->size - offset;

        len = (len >= 0) ? len : 0;
        len = (len <= left) ? len : left;

        region.offset   = hnd->offset + offset;
        region.len      = len;
    }
    else
    {
        // Full cache flush for bpr is 0 case, like framebuffer and yuv
        region.offset   = hnd->offset;
        region.len      = hnd->size;
    }
    region.dir = PXA_DMA_BIDIRECTIONAL;
    data.cmd = ION_PXA_SYNC;
    data.arg = (unsigned long)&region;
    ret = ioctl(hnd->fd, ION_IOC_CUSTOM, &data);
out:
    return ret;
}
#else
static int pmem_flush(gc_private_handle_t *hnd)
{
    struct pmem_region region;
    int flush_fd, ret;

    if (hnd->bpr > 0)
    {
        // Partial cache flush for normal gralloc buffer
        int offset = hnd->dirtyY * hnd->bpr;
        int len = hnd->dirtyHeight * hnd->bpr;
        int left = hnd->size - offset;

        len = (len >= 0) ? len : 0;
        len = (len <= left) ? len : left;

        region.offset   = hnd->offset + offset;
        region.len      = len;
    }
    else
    {
        // Full cache flush for bpr is 0 case, like framebuffer and yuv
        region.offset   = hnd->offset;
        region.len      = hnd->size;
    }


    if (hnd->fd >= 0)
    {
        flush_fd = (hnd->pid != getpid()) ? hnd->fd : hnd->master;
        ret = ioctl(flush_fd, PMEM_CACHE_FLUSH, &region);
        if (ret < 0) {
            ALOGE("cannot flush handle %p (offs=%x len=%x)\n", hnd,
                 hnd->offset, hnd->size);
            return -EINVAL;
        }
     }
     return 0;
}

#endif
/*******************************************************************************
**
**  gc_gralloc_unlock
**
**  Unlock android native buffer.
**  For 3D composition, it will resolve linear to tile for SW surfaces.
**
**  INPUT:
**
**      gralloc_module_t const * Module
**          Specified gralloc module.
**
**      buffer_handle_t Handle
**          Specified buffer handle.
**
**  OUTPUT:
**
**      Nothing.
**
*/
int
gc_gralloc_unlock(
    gralloc_module_t const * Module,
    buffer_handle_t Handle
)
{
    if (gc_private_handle_t::validate(Handle) < 0)
        return -EINVAL;

    gc_private_handle_t * hnd = (gc_private_handle_t *) Handle;

    if (!(hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER))
    {
        /* ALOGV("To unlock buffer hnd=%p", (void *) Handle); */
#if 0
        if (hnd->lockUsage & GRALLOC_USAGE_SW_WRITE_MASK)
        {
            /* Ignoring the error. */
            gcmVERIFY_OK(
                gcoSURF_CPUCacheOperation((gcoSURF) hnd->surface,
                                          gcvCACHE_CLEAN));

#if gcdCOPYBLT_OPTIMIZATION
            /* CopyBlt locks the surface only for write, whereas the app locks it for
             * read/write which will be submitted for composition. */
            gcsVIDMEM_NODE_SHARED_INFO dynamicInfo;

            /* Src and Dst have identical geometries, so we use the src origin and size. */
            dynamicInfo.SrcOrigin.x     = hnd->dirtyX;
            dynamicInfo.SrcOrigin.y     = hnd->dirtyY;
            dynamicInfo.RectSize.width  = hnd->dirtyWidth;
            dynamicInfo.RectSize.height = hnd->dirtyHeight;

            /* Ignore this error. */
            gcmVERIFY_OK(
                gcoHAL_SetSharedInfo(0,
                                     gcvNULL,
                                     0,
                                     ((gcoSURF)(hnd->surface))->info.node.u.normal.node,
                                     (gctUINT8_PTR)(&dynamicInfo),
                                     gcvVIDMEM_INFO_DIRTY_RECTANGLE));

            ALOGV("Passing dirty rect to kernel (l=%d, t=%d, w=%d, h=%d)",
                hnd->dirtyX, hnd->dirtyY, hnd->dirtyWidth, hnd->dirtyHeight);
#endif
        }
#endif
        if((hnd->flags & private_handle_t::PRIV_FLAGS_USES_PMEM) &&
            (hnd->flags & private_handle_t::PRIV_FLAGS_NEEDS_FLUSH))
        {
            gc_gralloc_flush(Handle);

            hnd->flags &= ~private_handle_t::PRIV_FLAGS_NEEDS_FLUSH;
        }

        hnd->lockUsage = 0;

        ALOGV("Unlocked buffer hnd=%p", (void *) Handle);
    }
    else
    {
        /* Do nothing for framebuffer buffer. */
        ALOGV("Unlocked framebuffer hnd=%p", (void *) Handle);
    }

    return 0;
}

/*******************************************************************************
**
**  gc_gralloc_flush
**
**  Flush cache to memory.
**
**  INPUT:
**
**      buffer_handle_t Handle
**          Specified buffer handle.
**
**  OUTPUT:
**
**      Nothing.
*/

int
gc_gralloc_flush(
    buffer_handle_t Handle
    )
{
    gc_private_handle_t * hnd = (gc_private_handle_t *) Handle;
#if (MRVL_VIDEO_MEMORY_USE_TYPE == gcdMEM_TYPE_ION)
    ion_flush(hnd);
#else
    pmem_flush(hnd);
#endif

    return 0;
}
