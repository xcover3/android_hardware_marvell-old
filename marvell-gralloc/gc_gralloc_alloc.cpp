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
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <cutils/ashmem.h>
#include <cutils/log.h>
#include <cutils/atomic.h>

#include <hardware/hardware.h>
#include <hardware/gralloc.h>

#include <binder/IPCThreadState.h>

#include "gc_gralloc_priv.h"
#include "gc_gralloc_gr.h"

#include <gc_hal_user.h>
#include <gc_hal_base.h>

#define PLATFORM_SDK_VERSION 22

#if HAVE_ANDROID_OS
#if (MRVL_VIDEO_MEMORY_USE_TYPE == gcdMEM_TYPE_ION)
#include <linux/ion.h>
#include <linux/pxa_ion.h>
#else
#include <linux/android_pmem.h>
#endif
#endif

#if MRVL_SUPPORT_DISPLAY_MODEL
#include <dms_if.h>
extern android::sp<android::IDisplayModel> displayModel;
#endif

using namespace android;

#define _ALIGN( n, align_dst ) ( (n + (align_dst-1)) & ~(align_dst-1) )

/*******************************************************************************
**
**  _ConvertAndroid2HALFormat
**
**  Convert android pixel format to Vivante HAL pixel format.
**
**  INPUT:
**
**      int Format
**          Specified android pixel format.
**
**  OUTPUT:
**
**      gceSURF_FORMAT HalFormat
**          Pointer to hold hal pixel format.
*/
static gceSTATUS
_ConvertAndroid2HALFormat(
    int Format,
    gceSURF_FORMAT * HalFormat
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    switch (Format)
    {
    case HAL_PIXEL_FORMAT_RGB_565:
        *HalFormat = gcvSURF_R5G6B5;
        break;

    case HAL_PIXEL_FORMAT_RGBA_8888:
        *HalFormat = gcvSURF_A8R8G8B8;
        break;

    case HAL_PIXEL_FORMAT_RGBX_8888:
        *HalFormat = gcvSURF_X8R8G8B8;
        break;

#if PLATFORM_SDK_VERSION < 19
    case HAL_PIXEL_FORMAT_RGBA_4444:
        *HalFormat = gcvSURF_R4G4B4A4;
        break;

    case HAL_PIXEL_FORMAT_RGBA_5551:
        *HalFormat = gcvSURF_R5G5B5A1;
        break;
#endif

    case HAL_PIXEL_FORMAT_BGRA_8888:
        *HalFormat = gcvSURF_A8B8G8R8;
        break;

    case HAL_PIXEL_FORMAT_YCbCr_420_SP_MRVL:
        /* YUV 420 semi planner: NV12 */
        *HalFormat = gcvSURF_NV12;
        break;

    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        /* YVU 420 semi planner: NV21 */
        *HalFormat = gcvSURF_NV21;
        break;

    case HAL_PIXEL_FORMAT_YCbCr_420_P:
        /* YUV 420 planner: I420 */
        *HalFormat = gcvSURF_I420;
        break;

    case HAL_PIXEL_FORMAT_YV12:
        /* YVU 420 planner: YV12 */
        *HalFormat = gcvSURF_YV12;
        break;

    case HAL_PIXEL_FORMAT_YCbCr_422_I:
        /* YUV 422 package: YUYV, YUY2 */
        *HalFormat = gcvSURF_YUY2;
        break;

    case HAL_PIXEL_FORMAT_CbYCrY_422_I:
        /* UYVY 422 package: UYVY */
        *HalFormat = gcvSURF_UYVY;
        break;


    default:
        *HalFormat = gcvSURF_UNKNOWN;
        status = gcvSTATUS_INVALID_ARGUMENT;

        ALOGE("Unknown format %d", Format);
        break;
    }

    return status;
}

static int _ConvertFormatToSurfaceInfo(
    int Format,
    int Width,
    int Height,
    size_t *Xstride,
    size_t *Ystride,
    size_t *Size
    )
{
    size_t xstride = 0;
    size_t ystride = 0;
    size_t size = 0;

    switch(Format)
    {
    case HAL_PIXEL_FORMAT_YCbCr_420_SP_MRVL:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        xstride = _ALIGN(Width, 64 );
        ystride = _ALIGN(Height, 64 );
        size    = xstride * ystride * 3 / 2;
        break;

    case HAL_PIXEL_FORMAT_YCbCr_422_SP:
        xstride = _ALIGN(Width, 64);
        ystride = _ALIGN(Height, 64);
        size    = xstride * ystride * 2;
        break;

    case HAL_PIXEL_FORMAT_YCbCr_420_P:
        xstride = _ALIGN(Width, 64 );
        ystride = _ALIGN(Height, 64 );
        size    = xstride * ystride * 3 / 2;
        break;

    case HAL_PIXEL_FORMAT_YV12:
        xstride = _ALIGN(Width, 64 );
        ystride = _ALIGN(Height, 64 );
        size    = xstride * ystride * 3 / 2;
        break;

    case HAL_PIXEL_FORMAT_YCbCr_422_I:
    case HAL_PIXEL_FORMAT_CbYCrY_422_I:
        xstride = _ALIGN(Width, 16);
        ystride = _ALIGN(Height, 32);
        size    = xstride * ystride * 2;
        break;

    /* RGBA format lists*/
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_BGRA_8888:
        xstride = _ALIGN(Width, 16);
        ystride = _ALIGN(Height, 4);
        size = xstride * ystride * 4;
        break;

    case HAL_PIXEL_FORMAT_RGB_888:
        xstride = _ALIGN(Width, 16);
        ystride = _ALIGN(Height, 4);
        size = xstride * ystride * 3;
        break;

    case HAL_PIXEL_FORMAT_RGB_565:
#if PLATFORM_SDK_VERSION < 19
    case HAL_PIXEL_FORMAT_RGBA_5551:
    case HAL_PIXEL_FORMAT_RGBA_4444:
#endif
        xstride = _ALIGN(Width, 16);
        ystride = _ALIGN(Height, 4);
        size = xstride * ystride * 2;
        break;

    default:
        return -EINVAL;
    }

    *Xstride = xstride;
    *Ystride = ystride;
    *Size = size;

    return 0;
}

/*******************************************************************************
**
**  _MapBuffer
**
**  Map android native buffer (call gc_gralloc_map).
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
_MapBuffer(
    gralloc_module_t const * Module,
    private_handle_t * Handle)
{
    void * vaddr;
    int retCode = 0;

    retCode = gc_gralloc_map(Module, Handle, &vaddr);

    if (retCode < 0)
    {
        return retCode;
    }
    /* Cleanup noise except for YUV*/
    gc_private_handle_t* hnd = (gc_private_handle_t*) Handle;

    if((hnd->surfFormat < gcvSURF_YUY2)||(hnd->surfFormat >= gcvSURF_D16))
    {
        /* Clear memory. */
        gcoOS_MemFill(vaddr,
                      0,
                      (hnd->surface != 0)
                      ? hnd->adjustedSize
                      : hnd->resolveAdjustedSize);

        /*flush cache in case there is garbage generated from gcoOS_MemFill*/
        if((hnd->flags & private_handle_t::PRIV_FLAGS_USES_PMEM))
        {
            gc_gralloc_flush(Handle);
        }
    }
    return retCode;
}

/*******************************************************************************
**
**  _TerminateBuffer
**
**  Destroy android native buffer and free resource (in surfaceflinger).
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
static int
_TerminateBuffer(
    gralloc_module_t const * Module,
    private_handle_t * Handle
    )
{
    gceSTATUS status;

    gc_private_handle_t* hnd = (gc_private_handle_t*) Handle;

    if (hnd->base)
    {
        /* this buffer was mapped, unmap it now. */
        gc_gralloc_unmap(Module, Handle);
    }

    /* Memory is merely scheduled to be freed.
       It will be freed after a h/w event. */
    if (hnd->surface != 0)
    {
        gcmVERIFY_OK(
            gcoSURF_Destroy((gcoSURF) hnd->surface));

        ALOGV("Freed buffer handle=%p, "
             "surface=%p",
             (void *) hnd,
             (void *) hnd->surface);

        hnd->surface = 0;
    }

    /* Destroy tile surface. */
    if (hnd->resolveSurface != 0)
    {
        gcmVERIFY_OK(
            gcoSURF_Destroy((gcoSURF) hnd->resolveSurface));

        ALOGV("Freed buffer handle=%p, "
             "resolveSurface=%p",
             (void *) hnd,
             (void *) hnd->resolveSurface);

        hnd->resolveSurface = 0;
    }

    /* Destroy signal if exists. */
    if (hnd->hwDoneSignal != 0)
    {
        gcmVERIFY_OK(
            gcoOS_DestroySignal(gcvNULL, (gctSIGNAL) hnd->hwDoneSignal));

        hnd->hwDoneSignal = 0;
    }

    hnd->clientPID = 0;

    /* Commit command buffer. */
    gcmONERROR(
        gcoHAL_Commit(gcvNULL, gcvFALSE));


    return 0;

OnError:
    ALOGE("Failed to free buffer, status=%d", status);

    return -EINVAL;
}

/*******************************************************************************
**
**  gc_gralloc_alloc_framebuffer_locked
**
**  Map framebuffer memory to android native buffer.
**
**  INPUT:
**
**      alloc_device_t * Dev
**          alloc device handle.
**
**      int Width
**          Specified target buffer width.
**
**      int Hieght
**          Specifed target buffer height.
**
**      int Format
**          Specified target buffer format.
**
**      int Usage
**          Specified target buffer usage.
**
**  OUTPUT:
**
**      buffer_handle_t * Handle
**          Pointer to hold the mapped buffer handle.
**
**      int * Stride
**          Pointer to hold buffer stride.
*/
static int
gc_gralloc_alloc_framebuffer_locked(
    alloc_device_t * Dev,
    int Width,
    int Height,
    int Format,
    int Usage,
    buffer_handle_t * Handle,
    int * Stride
    )
{
    private_module_t * m =
        reinterpret_cast<private_module_t*>(Dev->common.module);

    /* Map framebuffer to Vivante hal surface(fb wrapper). */
    gceSTATUS      status         = gcvSTATUS_OK;
    gcoSURF        fbWrapper      = gcvNULL;
    gceSURF_FORMAT format;
    gctUINT32      baseAddress;
    gctUINT32      mmuVersion;

    /* Allocate the framebuffer. */
    if (m->framebuffer == NULL)
    {
        /* Initialize the framebuffer, the framebuffer is mapped once
        ** and forever. */
        int err = mapFrameBufferLocked(m);
        if (err < 0)
        {
            return err;
        }
    }

    const uint32_t bufferMask = m->bufferMask;
    const uint32_t numBuffers = m->numBuffers;
    //size_t   bufferSize = m->finfo.line_length * m->info.yres;
    size_t   bufferSize = m->finfo.line_length * (m->info.yres_virtual/numBuffers);
#if MRVL_SUPPORT_DISPLAY_MODEL
    if (displayModel != NULL) {
        buffer_handle_t* pHandle = Handle;
        if (pHandle != NULL && *pHandle != NULL && private_handle_t::validate(*pHandle) >= 0 ) {
            ALOGI("This buffer handle is valid already");

            private_handle_t* hnd = (private_handle_t*) (*pHandle);
            if (hnd->offset != 0) {
                int width, height;
                displayModel->CRTC_GetVirtualScreenResolution(&width, &height);
                int bufferSize = m->finfo.line_length * height;
                hnd->base = bufferSize + intptr_t(m->framebuffer->base);
                hnd->offset = bufferSize;

                ALOGI("index 1, hnd %p, base %X, offset %X", hnd, hnd->base, hnd->offset);
            } else {
                hnd->base = intptr_t(m->framebuffer->base);
                hnd->offset = 0;
                ALOGI("index 0, hnd %p, base %X, offset %X", hnd, hnd->base, hnd->offset);
            }
            return 0;
        } else {
            ALOGI("framebuffer is allocated for the first time");
            int width, height;
            displayModel->CRTC_GetVirtualScreenResolution(&width, &height);
            bufferSize = m->finfo.line_length * height;
            ALOGI("New screen resolution is %dx%d", width, height);
        }
    } else {
        bufferSize = m->finfo.line_length * m->info.yres;
    }
#else
    //bufferSize = m->finfo.line_length * m->info.yres;
    bufferSize = (m->finfo.line_length * (m->info.yres_virtual/numBuffers));
#endif

    if (bufferMask >= ((1LU << numBuffers) - 1))
    {
        /* We ran out of buffers. */
        return -ENOMEM;
    }

    /* Create handles for it. */
    intptr_t vaddr = intptr_t(m->framebuffer->base);
    intptr_t paddr = intptr_t(m->framebuffer->physAddr);

    gc_private_handle_t * handle =
        new gc_private_handle_t(dup(m->framebuffer->fd),
                             bufferSize,
                             private_handle_t::PRIV_FLAGS_FRAMEBUFFER);
    handle->master = open("/dev/null", O_RDONLY);
    handle->allocUsage = Usage;
    /* Find a free slot. */
    for (uint32_t i = 0; i < numBuffers; i++)
    {
        if ((bufferMask & (1LU << i)) == 0)
        {
#ifndef FRONT_BUFFER
            m->bufferMask |= (1LU << i);
#endif
            break;
        }
        vaddr += bufferSize;
        paddr += bufferSize;
    }

    handle->master = -1;
    /* Save memory address. */
    handle->base   = vaddr;
    handle->offset = vaddr - intptr_t(m->framebuffer->base);
    handle->physAddr   = paddr;

    ALOGV("Allocated framebuffer handle=%p, vaddr=%p, paddr=%p",
         (void *) handle, (void *) vaddr, (void *) paddr);

    *Handle = handle;
    *Stride = m->finfo.line_length / (m->info.bits_per_pixel / 8);

    /* Map framebuffer to Vivante hal surface(fb wrapper). */
    gctUINT32      phys;

    gcmONERROR(gcoHAL_SetHardwareType(gcvNULL, gcvHARDWARE_3D));

    /* Get base address. */
    gcmONERROR(
        gcoOS_GetBaseAddress(gcvNULL, &baseAddress));

    /* Get mmu version. */
    gcmONERROR(
        gcoOS_GetMmuVersion(gcvNULL, &mmuVersion));

    /* Construct fb wrapper. */
    gcmONERROR(
        gcoSURF_ConstructWrapper(gcvNULL,
                                 &fbWrapper));

    gcmONERROR(
        _ConvertAndroid2HALFormat(Format,
                                  &format));
    /* Set the underlying frame buffer surface. */
    if  ( (mmuVersion != 0 )||
        (!((handle->physAddr - baseAddress) & 0x80000000)
        && !((handle->physAddr + m->finfo.line_length * (m->info.yres_virtual/m->numBuffers) - 1 - baseAddress) & 0x80000000)))
        //&& !((handle->physAddr + m->finfo.line_length * Height - 1 - baseAddress) & 0x80000000)))
    {
        phys = (gctUINT32) handle->physAddr - baseAddress;
    }
    else
    {
        phys = ~0U;
    }

    gcmONERROR(
        gcoSURF_SetBuffer(fbWrapper,
                          gcvSURF_BITMAP,
                          format,
                          m->finfo.line_length,
                          (gctPOINTER) handle->base,
                          phys));

    /* Set the window. */
    gcmONERROR(
        gcoSURF_SetWindow(fbWrapper,
                          0, 0,
                          Width, Height));

    gctPOINTER memory[3];
    gctUINT32  address[3];

    gcmONERROR(gcoSURF_Lock(fbWrapper,
                            address, memory));

    handle->physAddr   = address[0];
    gcmONERROR(gcoSURF_Unlock(fbWrapper,
                              memory));

    /* Save surface to buffer handle. */
    handle->surface         = (int) fbWrapper;
    handle->resolveSurface  = 0;
    handle->surfFormat      = (int) format;
    handle->format          = (int) Format;

    return 0;

OnError:
    *Handle = NULL;

    return -ENOMEM;
}

/*******************************************************************************
**
**  gc_gralloc_alloc_framebuffer
**
**  Map framebuffer memory to android native buffer.
**
**  INPUT:
**
**      alloc_device_t * Dev
**          alloc device handle.
**
**      int Width
**          Specified target buffer width.
**
**      int Hieght
**          Specified target buffer height.
**
**      int Format
**          Specified target buffer format.
**
**      int Usage
**          Specified target buffer usage.
**
**  OUTPUT:
**
**      buffer_handle_t * Handle
**          Pointer to hold the mapped buffer handle.
**
**      int * Stride
**          Pointer to hold buffer stride.
*/
static int
gc_gralloc_alloc_framebuffer(
    alloc_device_t * Dev,
    int Width,
    int Height,
    int Format,
    int Usage,
    buffer_handle_t * Handle,
    int * Stride
    )
{

    private_module_t * m =
        reinterpret_cast<private_module_t*>(Dev->common.module);

    pthread_mutex_lock(&m->lock);

    int err = gc_gralloc_alloc_framebuffer_locked(
        Dev, Width, Height, Format, Usage, Handle, Stride);

    pthread_mutex_unlock(&m->lock);

    return err;
}

#if (MRVL_VIDEO_MEMORY_USE_TYPE == gcdMEM_TYPE_ION)

static int ion_alloc_buffer(int offset, int size, int *o_fd, int *physaddr)
{
    struct ion_allocation_data req_alloc;
    struct ion_fd_data req_fd;
    struct ion_handle *handle;
    struct ion_custom_data data;
    struct ion_pxa_region region;
    int fd, ret, shared_fd;

    fd = open("/dev/ion", O_RDWR, 0);
    if (fd < 0) {
        ALOGE("Failed to open /dev/ion");
        return fd;
    }
    memset(&req_alloc, 0, sizeof(struct ion_allocation_data));
    req_alloc.len = _ALIGN(size, PAGE_SIZE);
    req_alloc.align = PAGE_SIZE;
    // req_alloc.heap_id_mask = ION_HEAP_TYPE_DMA_MASK;
    req_alloc.heap_id_mask = ION_HEAP_CARVEOUT_MASK;
    req_alloc.flags = ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC;
    ret = ioctl(fd, ION_IOC_ALLOC, &req_alloc);
    if (ret < 0){
        ALOGE("Failed to ION_IOC_ALLOC. if ret is -ENODEV means ION size is not enough, \
                mask: %d size: %d ret: %d",
                req_alloc.heap_id_mask, req_alloc.len, ret);
        ret = -errno;
        goto out;
    }
    handle = req_alloc.handle;

    memset(&region, 0, sizeof(struct ion_pxa_region));
    memset(&data, 0, sizeof(struct ion_custom_data));
    region.handle = handle;
    data.cmd = ION_PXA_PHYS;
    data.arg = (unsigned long)&region;
    ret = ioctl(fd, ION_IOC_CUSTOM, &data);
    if (ret < 0) {
        ret = -errno;
        goto out;
    }
    *physaddr = region.addr;
    memset(&req_fd, 0, sizeof(struct ion_fd_data));
    req_fd.handle = handle;
    ret = ioctl(fd, ION_IOC_SHARE, &req_fd);
    if (ret < 0) {
        ret = -errno;
        goto out;
    }
    *o_fd = fd;
    return req_fd.fd;
out:
    close(fd);
    ALOGE("Failed to allocate memory from ION");
    return ret;
}
#else

static int pmem_alloc_buffer(int offset, int size, int *physaddr)
{
    struct pmem_region sub = {offset, size};
    int fd, ret;

    fd = open("/dev/pmem", O_RDWR, 0);
    if (fd < 0) {
        ALOGE("Failed to open /dev/pmem");
        return fd;
    }

    ret = ioctl(fd, PMEM_ALLOCATE, size);
    if (ret) {
        ret = -errno;
        goto out;
    }

    ret = ioctl(fd, PMEM_GET_PHYS, &sub);
    if (ret < 0) {
        ret = -errno;
        goto out;
    }
    *physaddr = sub.offset;
    ALOGV("allocate pmem size:%d", size);
    return fd;
out:
    close(fd);
    ALOGE("Failed to allocate memory from pmem");
    return ret;
}

#endif
/*******************************************************************************
**
**  gc_gralloc_alloc_buffer
**
**  Allocate android native buffer.
**  Will allocate surface types as follows,
**
**  +------------------------------------------------------------------------+
**  | To Allocate Surface(s)        | Linear(surface) | Tile(resolveSurface) |
**  |------------------------------------------------------------------------|
**  |Enable              |  CPU app |      yes        |                      |
**  |LINEAR OUTPUT       |  3D app  |      yes        |                      |
**  |------------------------------------------------------------------------|
**  |Diable              |  CPU app |      yes        |                      |
**  |LINEAR OUTPUT       |  3D app  |                 |         yes          |
**  +------------------------------------------------------------------------+
**
**
**  INPUT:
**
**      alloc_device_t * Dev
**          alloc device handle.
**
**      int Width
**          Specified target buffer width.
**
**      int Hieght
**          Specified target buffer height.
**
**      int Format
**          Specified target buffer format.
**
**      int Usage
**          Specified target buffer usage.
**
**  OUTPUT:
**
**      buffer_handle_t * Handle
**          Pointer to hold the allocated buffer handle.
**
**      int * Stride
**          Pointer to hold buffer stride.
*/
static int
gc_gralloc_alloc_buffer(
    alloc_device_t * Dev,
    int Width,
    int Height,
    int Format,
    int Usage,
    buffer_handle_t * Handle,
    int * Stride
    )
{
    int err             = 0;
    int fd              = -1;
    int master          = -1;
    int offset          = 0;
    int flags           = 0;
    int width           = 0;
    int height          = 0;

    size_t size         = 0;
    size_t xstride      = 0;
    size_t ystride      = 0;
    int bpr             = 0;
    int physAddr        = 0;
    gctUINT pixelPipes;

    /* Buffer handle. */
    gc_private_handle_t * handle      = NULL;

    gctBOOL isAlloc                   = gcvFALSE;
    gctBOOL isPmemAlloc               = gcvFALSE;
    gctBOOL isGcAlloc                 = gcvFALSE;

    gceSTATUS status                  = gcvSTATUS_OK;
    gctUINT stride                    = 0;

    gceSURF_FORMAT format             = gcvSURF_UNKNOWN;
    gceSURF_FORMAT resolveFormat      = gcvSURF_UNKNOWN;
    (void) resolveFormat;

    /* Linear stuff. */
    gcoSURF surface                   = gcvNULL;
    gcuVIDMEM_NODE_PTR vidNode        = gcvNULL;
    gcePOOL pool                      = gcvPOOL_UNKNOWN;
    gctUINT adjustedSize              = 0U;

    /* Tile stuff. */
    gcoSURF resolveSurface            = gcvNULL;
    gcuVIDMEM_NODE_PTR resolveVidNode = gcvNULL;
    gcePOOL resolvePool               = gcvPOOL_UNKNOWN;
    gctUINT resolveAdjustedSize       = 0U;

    gctSIGNAL signal                  = gcvNULL;
    gctINT clientPID                  = 0;

    gctBOOL forSelf                   = gcvTRUE;

    /* Binder info. */
    IPCThreadState* ipc               = IPCThreadState::self();

    clientPID = ipc->getCallingPid();

    forSelf = (clientPID == getpid());

    if (forSelf)
    {
        ALOGI("Self allocation");
    }

    /* Must be set current hardwareType to 3D, due to surfaceflinger need tile
     * surface.
     */
    gcmONERROR(
        gcoHAL_SetHardwareType(gcvNULL, gcvHARDWARE_3D));

    gcmONERROR(
        gcoHAL_QueryPixelPipesInfo(&pixelPipes,gcvNULL,gcvNULL));

    /* Convert to hal pixel format. */
    gcmONERROR(
       _ConvertAndroid2HALFormat(Format,
                                 &format));

    /*check usage to determine alloc buffer from ion or gc driver.*/
#if 0
    if(Usage & GRALLOC_USAGE_OVLY_DISP)
    {
        /*alloc buffer from ion first, if failed, alloc buffer from gc driver.*/
        isPmemAlloc = gcvTRUE;
        isGcAlloc   = gcvTRUE;
    }
#endif
    if((Usage & GRALLOC_USAGE_HW_RENDER) && !(Usage & GRALLOC_USAGE_PRIVATE_3))
    {
        /*alloc buffer from gc driver.*/
        isPmemAlloc = gcvFALSE;
        isGcAlloc = gcvTRUE;
    }
    else
    {
        /*alloc buffer from ion, including GRALLOC_USAGE_PRIVATE_3*/
        isPmemAlloc = gcvTRUE;
        isGcAlloc   = gcvFALSE;
    }

    /*ion path, allocated continuous memroy.*/
    if(isPmemAlloc)
    {
        flags |= private_handle_t::PRIV_FLAGS_USES_PMEM;

        /* FIX ME: Workaround for PMEM path, due to user-allocated surface don't
         * do alignment when created, it may cause resolve onto texture
         * surface failed.
         */
        width = _ALIGN(Width, 16);

        /*the alignment of height is base on HW pixelpipe*/
        height = _ALIGN(Height, 4*pixelPipes);

        private_module_t* module =
            reinterpret_cast<private_module_t*>(Dev->common.module);

        err = _ConvertFormatToSurfaceInfo(Format,
                                           width,
                                           height,
                                           &xstride,
                                           &ystride,
                                           &size);
        if(err < 0)
        {
            ALOGE("not found compatible format!");
            return err;
        }

        size = roundUpToPageSize(size);

#if (MRVL_VIDEO_MEMORY_USE_TYPE == gcdMEM_TYPE_ION)
        master = ion_alloc_buffer(offset, size, &fd, &physAddr);
#else
        master = pmem_alloc_buffer(offset, size, &physAddr);
#endif

        if(master >= 0)
        {
            gcmONERROR(
                gcoSURF_Construct(gcvNULL,
                                  width,
                                  height,
                                  1,
                                  gcvSURF_BITMAP,
                                  format,
                                  gcvPOOL_USER,
                                  &surface));

            /* Now retrieve and store vid mem node attributes. */
            gcmONERROR(
               gcoSURF_QueryVidMemNode(surface,
                                       &vidNode,
                                       &pool,
                                       &adjustedSize));

            /* For CPU apps, we must synchronize lock requests from CPU with the composition.
             * Composition could happen in the following ways. i)2D, ii)3D, iii)CE, iv)copybit 2D.
             * (Note that, we are not considering copybit composition of 3D apps, which also uses
             * linear surfaces. Can be added later if needed.)

             * In all cases, the following mechanism must be used for proper synchronization
             * between CPU and GPU :

             * - App on gralloc::lock
             *      wait_signal(hwDoneSignal);

             * - Compositor on composition
             *      set_unsignalled(hwDoneSignal);
             *      issue composition;
             *      schedule_event(hwDoneSignal, clientPID);

             *  This is a manually reset signal, which is unsignalled by the compositor when
             *  buffer is in use, prohibiting app from obtaining write lock.
             */
            /* Manually reset signal, for CPU/GPU sync. */
            gcmONERROR(gcoOS_CreateSignal(gcvNULL,
                                          gcvTRUE,
                                          &signal));

            /* Initially signalled. */
            gcmONERROR(gcoOS_Signal(gcvNULL,
                                    signal,
                                    gcvTRUE));

            ALOGV("Created signal=%p for hnd=%p", signal, handle);

            /*buffer is allocated successfully.*/
            isAlloc = gcvTRUE;
        }

        /*if fail to alloc buffer from ion and gc path is not allowed, return falied.*/
        if(master < 0 && !isGcAlloc)
        {
            err = master;
            goto OnError;
        }

    }

    /*gc path, allocate memroy from gc driver(continuous or non-continous).*/
    if(!isAlloc && isGcAlloc)
    {
        /*reset width and height*/
        width = Width;
        height = Height;

        /*remove pmem flags if it is set*/
        flags &= ~private_handle_t::PRIV_FLAGS_USES_PMEM;

        /* For 3D app. */
        master = open("/dev/null",O_RDONLY,0);

        /* we're using two fds now */
        fd = open("/dev/null", O_RDONLY, 0);

        /*This flag is used to decide texture surafce use linear or tile. */
#if !gcdGPU_LINEAR_BUFFER_ENABLED
        /* 3D App use tile buffer, non-cached.(gcvSURF_TEXTURE) */

        /* Get the resolve format. */
        gcmONERROR(
            gcoTEXTURE_GetClosestFormat(gcvNULL,
                                        format,
                                        &resolveFormat));

        /* Construct the resolve target. */
        gcmONERROR(
            gcoSURF_Construct(gcvNULL,
                              width,
                              height,
                              1,
                              gcvSURF_TEXTURE,
                              resolveFormat,
                              gcvPOOL_DEFAULT,
                              &resolveSurface));

        /* 3D surfaces are bottom-top. */
        gcmONERROR(
            gcoSURF_SetOrientation(resolveSurface,
                                   gcvORIENTATION_BOTTOM_TOP));

        /* Now retrieve and store vid mem node attributes. */
        gcmONERROR(
            gcoSURF_QueryVidMemNode(resolveSurface,
                                   &resolveVidNode,
                                   &resolvePool,
                                   &resolveAdjustedSize));

        /* Android expects stride in pixels which is returned as alignedWidth. */
        gcmONERROR(
            gcoSURF_GetAlignedSize(resolveSurface,
                                   &stride,
                                   gcvNULL,
                                   gcvNULL));

        gcmTRACE(gcvLEVEL_INFO,
                 "resolve buffer resolveSurface=%p, "
                 "resolveNode=%p, "
                 "resolvevidnode=%p, "
                 "resolveBytes=%d, "
                 "resolvePool=%d, "
                 "resolveFormat=%d",
                 (void *) resolveSurface,
                 (void *) &resolveSurface->info.node,
                 (void *) resolveVidNode,
                 resolveAdjustedSize,
                 resolvePool,
                 resolveFormat);

#else
            /* 3D App use linear buffer, non-cached. (gcvSURF_BITMAP)*/

            /* For HARWARE_RENDER case, allocate non-cacheable Linear Surface from GC reserved. */
#if gcdANDROID_UNALIGNED_LINEAR_COMPOSITION_ADJUST
             gcmONERROR(
                gcoSURF_Construct(gcvNULL,
                                  width,
                                  height,
                                  1,
                                  gcvSURF_FLIP_BITMAP,
                                  format,
                                  gcvPOOL_DEFAULT,
                                  &surface));
#else
             gcmONERROR(
                gcoSURF_Construct(gcvNULL,
                                  width,
                                  height,
                                  1,
                                  gcvSURF_BITMAP,
                                  format,
                                  gcvPOOL_DEFAULT,
                                  &surface));
#endif
            /* Now retrieve and store vid mem node attributes. */
            gcmONERROR(
               gcoSURF_QueryVidMemNode(surface,
                                       &vidNode,
                                       &pool,
                                       &adjustedSize));

            /* Get stride. */
            gcmONERROR(
                gcoSURF_GetAlignedSize(surface,
                                       &stride,
                                       gcvNULL,
                                       gcvNULL));

#endif
            /*buffer is allocated successfully.*/
            isAlloc = gcvTRUE;

            /*buffer is allocated from gc driver but not ion.*/
            isPmemAlloc = gcvFALSE;
    }

    if(err == 0)
    {
        handle = new gc_private_handle_t(fd, size, flags);
        handle->master              = master;
        handle->width               = (int) width;
        handle->height              = (int) height;
        handle->format              = (int) Format;
        handle->surfFormat          = (int) format;

        /* Save tile resolveSurface to buffer handle. */
        handle->resolveSurface      = (int) resolveSurface;
        handle->resolveVidNode      = (int) resolveVidNode;
        handle->resolvePool         = (int) resolvePool;
        handle->resolveAdjustedSize = (int) resolveAdjustedSize;
        handle->adjustedSize        = (int) adjustedSize;

        handle->hwDoneSignal        = (int) signal;

        /* Record usage to recall later in hwc. */
        handle->lockUsage           = 0;
        handle->allocUsage          = Usage;
        handle->clientPID           = clientPID;

        /* Case module. */
        gralloc_module_t *module =
            reinterpret_cast<gralloc_module_t *>(Dev->common.module);

        if(isPmemAlloc)
        {
            /* Public part. */
            handle->offset          = offset;
            handle->physAddr        = physAddr;
            handle->usage           = Usage;
            handle->mem_xstride     = xstride;/* Aligned width. */
            handle->mem_ystride     = ystride;

            /* Private part. */
            handle->bpr             = bpr;
            handle->surface         = (int) surface;
            handle->pool            = gcvPOOL_USER;
            stride                  = xstride;

            /* Map pmem memory to user. */
            err = _MapBuffer(module, handle);
            if (err < 0)
            {
                ALOGE(" gc_gralloc_map memory error");
                return -errno;
            }
            gcmONERROR(
                gcoSURF_MapUserSurface(surface,
                                        0,
                                        (void*)handle->base,
                                        ~0));
        }
        else
        {
            /*3D App: Save linear surface to buffer handle, possibe this surface
            is NULL if gcdGPU_LINEAR_BUFFER_ENABLED disabled. */
            handle->surface             = (int) surface;
            handle->vidNode             = (int) vidNode;
            handle->pool                = (int) pool;

            /* Save correct xstride and ystride. */
            if(surface != gcvNULL)
            {
                gcmONERROR(
                    gcoSURF_GetAlignedSize(surface, &xstride, &ystride, gcvNULL));

                handle->mem_xstride     = xstride;
                handle->mem_ystride     = ystride;
            }

            /* Map video memory to user. */
            err = _MapBuffer(module, handle);
        }
    }

    if(err == 0)
    {
        *Handle = handle;
        *Stride = stride;
    }
    else
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    return err;

OnError:
    /* Destroy linear surface. */
    if (surface != gcvNULL)
    {
        gcoSURF_Destroy(surface);
        surface = gcvNULL;
    }

    /* Destroy tile resolveSurface. */
    if (resolveSurface != gcvNULL)
    {
        gcoSURF_Destroy(resolveSurface);
        resolveSurface = gcvNULL;
    }

    /* Destroy signal. */
    if (signal)
    {
        gcoOS_DestroySignal(gcvNULL, signal);
        signal = gcvNULL;
    }

    /* Error roll back. */
    if (handle != NULL)
    {
        delete handle;
        handle = gcvNULL;
    }

    ALOGE("failed to allocate, status=%d", status);

    return -EINVAL;
}

/*******************************************************************************
**
**  gc_gralloc_alloc
**
**  General buffer alloc.
**
**  INPUT:
**
**      alloc_device_t * Dev
**          alloc device handle.
**
**      int Width
**          Specified target buffer width.
**
**      int Hieght
**          Specified target buffer height.
**
**      int Format
**          Specified target buffer format.
**
**      int Usage
**          Specified target buffer usage.
**
**  OUTPUT:
**
**      buffer_handle_t * Handle
**          Pointer to hold the allocated buffer handle.
**
**      int * Stride
**          Pointer to hold buffer stride.
*/
int
gc_gralloc_alloc(
    alloc_device_t * Dev,
    int Width,
    int Height,
    int Format,
    int Usage,
    buffer_handle_t * Handle,
    int * Stride)
{
    int err;

    if (!Handle || !Stride)
    {
        return -EINVAL;
    }

#ifdef BUILD_GOOGLETV
    if ((HAL_PIXEL_FORMAT_GTV_VIDEO_HOLE == Format) ||
        (HAL_PIXEL_FORMAT_GTV_OPAQUE_BLACK == Format))
    {
        gc_private_handle_t *hnd = NULL;
        if (Usage & GRALLOC_USAGE_HW_FB) {
            hnd = new gc_private_handle_t(0, 0, NULL, private_handle_t::PRIV_FLAGS_FRAMEBUFFER);
        } else {
            hnd = new gc_private_handle_t(0, 0, NULL, 0);
        }

        hnd->width  = 1;
        hnd->height = 1;
        hnd->format = Format;

        *Handle = hnd;
        return 0;
    }
#endif
    if(gcmIS_SUCCESS(gcoOS_ModuleConstructor()))
    {
        err = 0;
    }
    else
    {
        ALOGE("failed to module construct!");
        return -EINVAL;
    }

    if (Usage & GRALLOC_USAGE_HW_FB)
    {
        /* Alloc framebuffer buffer. */
        err = gc_gralloc_alloc_framebuffer(
            Dev, Width, Height, Format, Usage, Handle, Stride);
    }
    else
    {
        /* Alloc non-framebuffer buffer. */
        err = gc_gralloc_alloc_buffer(
            Dev, Width, Height, Format, Usage, Handle, Stride);
    }

    return err;
}
#if (MRVL_VIDEO_MEMORY_USE_TYPE == gcdMEM_TYPE_ION)
static void ion_free(gc_private_handle_t const *hnd)
{
        struct ion_handle_data req;
        struct ion_fd_data req_fd;
        int ret;

        memset(&req_fd, 0, sizeof(struct ion_fd_data));
        req_fd.fd = hnd->master;
        ret = ioctl(hnd->fd, ION_IOC_IMPORT, &req_fd);
        if (ret < 0) {
            ALOGE("Failed to import ION buffer, ret:%d", ret);
            return;
        }
        memset(&req, 0, sizeof(struct ion_handle_data));
        req.handle = req_fd.handle;
        ret = ioctl(hnd->fd, ION_IOC_FREE, &req);
        if (ret < 0) {
            ALOGE("Failed to free ION buffer, ret:%d", ret);
            return;
        }
}
#endif
/*******************************************************************************
**
**  gc_gralloc_free
**
**  General buffer free.
**
**  INPUT:
**
**      alloc_device_t * Dev
**          alloc device handle.
**
**      buffer_handle_t Handle
**          Specified target buffer to free.
**
**  OUTPUT:
**
**      Nothing.
*/
int
gc_gralloc_free(
    alloc_device_t * Dev,
    buffer_handle_t Handle
    )
{
    if (gc_private_handle_t::validate(Handle) < 0)
    {
        return -EINVAL;
    }

    /* Cast private buffer handle. */
    gc_private_handle_t const * hnd =
        reinterpret_cast<gc_private_handle_t const *>(Handle);

#ifdef BUILD_GOOGLETV
    if ((HAL_PIXEL_FORMAT_GTV_VIDEO_HOLE == hnd->format) ||
        (HAL_PIXEL_FORMAT_GTV_OPAQUE_BLACK == hnd->format))
    {
        delete hnd;
        return 0;
    }
#endif

    if (hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER)
    {
#if MRVL_SUPPORT_DISPLAY_MODEL
        private_module_t* m = reinterpret_cast<private_module_t*>(
                Dev->common.module);
        int index;
        if (hnd->offset == 0) {
            index = 0;
        } else {
            index = 1;
        }
        m->bufferMask &= ~(1<<index);
#else
        /* Free framebuffer. */
        private_module_t * m =
            reinterpret_cast<private_module_t *>(Dev->common.module);

        const size_t bufferSize = m->finfo.line_length * m->info.yres;
        int index = (hnd->base - m->framebuffer->base) / bufferSize;

        /* Save unused buffer index. */
        m->bufferMask &= ~(1<<index);
#endif
    }
    else
    {
        /* Free non-framebuffer. */
        gralloc_module_t * module =
            reinterpret_cast<gralloc_module_t *>(Dev->common.module);

        _TerminateBuffer(module, const_cast<gc_private_handle_t *>(hnd));

        if(hnd->flags & private_handle_t::PRIV_FLAGS_USES_PMEM)
        {
#if (MRVL_VIDEO_MEMORY_USE_TYPE == gcdMEM_TYPE_ION)
            if (hnd->fd >= 0) {
                ion_free(hnd);
                close(hnd->master);
                close(hnd->fd);
            }
#else
            if (hnd->fd >= 0 && hnd->fd != hnd->master) {
                close(hnd->fd);
            }
#endif
        } else {
            close(hnd->fd);
            close(hnd->master);
        }
    }

    delete hnd;

    return 0;
}

