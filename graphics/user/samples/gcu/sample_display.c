#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <linux/fb.h>
#include <sys/mman.h>

#include "gcu.h"


int main(int argc, char** argv)
{

    char const * const device_template[] = {
            "/dev/graphics/fb%u",
            "/dev/fb%u",
            0 };

    int             fd = -1;
    int             i=0;
    char            name[64];

    struct          fb_var_screeninfo vinfo;
    struct          fb_fix_screeninfo finfo;
    void*           vaddr = NULL;
    unsigned int    fbSize = 0;


    GCUContext         pContext    = GCU_NULL;
    GCUSurface         pSrcSurface = GCU_NULL;
    GCUSurface         pDstSurface = GCU_NULL;
    GCU_RECT           srcRect;
    GCU_RECT           dstRect;
    GCU_INIT_DATA      initData;
    GCU_CONTEXT_DATA   contextData;
    GCU_SURFACE_DATA   surfaceData;
    GCU_BLT_DATA       blitData;

    if (argc <= 1)
    {
        return 0;
    }

    /* Open framebuffer */
    while ((fd==-1) && device_template[i]) {
        snprintf(name, 64, device_template[i], 0);
        fd = open(name, O_RDWR, 0);
        i++;
    }

    if (fd < 0)
    {
        return -1;
    }

    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == -1)
    {
        return -2;
    }

    if (ioctl (fd, FBIOGET_VSCREENINFO, &vinfo) == -1)
    {
        return -3;
    }

    fbSize = vinfo.yres_virtual * finfo.line_length;
    vaddr = mmap(0, fbSize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (vaddr == MAP_FAILED)
    {
        return -4;
    }


    /* Init GCU */
    memset(&initData, 0, sizeof(initData));
    initData.debug = 1;
    gcuInitialize(&initData);

    memset(&contextData, 0, sizeof(contextData));
    pContext = gcuCreateContext(&contextData);
    if(pContext == NULL)
    {
        return -5;
    }

    /* Load picture and blit */
    pSrcSurface = _gcuLoadRGBSurfaceFromFile(pContext, argv[1]);
    /* pSrcSurface = _gcuLoadYUVSurfaceFromFile(pContext, argv[1]); */

    pDstSurface = _gcuCreatePreAllocBuffer(pContext,
                                           vinfo.xres, vinfo.yres_virtual,
                                          (vinfo.bits_per_pixel == 16) ? GCU_FORMAT_RGB565 : GCU_FORMAT_ARGB8888,
                                           GCU_TRUE, vaddr,
                                           GCU_TRUE, finfo.smem_start);


    if(pSrcSurface && pDstSurface)
    {

        for (i = 0; i < 10000; i++)
        {
            gcuQuerySurfaceInfo(pContext, pSrcSurface, &surfaceData);

            memset(&blitData, 0, sizeof(blitData));
            blitData.pSrcSurface = pSrcSurface;
            blitData.pDstSurface = pDstSurface;
            blitData.pSrcRect    = &srcRect;
            blitData.pDstRect    = &dstRect;

            /* First FB */
            srcRect.left    = 0;
            srcRect.top     = 0;
            srcRect.right   = surfaceData.width;
            srcRect.bottom  = surfaceData.height;

            if(srcRect.right > (GCUint)(vinfo.xres))
            {
                srcRect.right = (GCUint)(vinfo.xres);
            }

            if(srcRect.bottom > (GCUint)(vinfo.yres))
            {
                srcRect.bottom = (GCUint)(vinfo.yres);
            }

            dstRect = srcRect;

            gcuBlit(pContext, &blitData);
            gcuFinish(pContext);

            /* Next FB */
            dstRect.top    += vinfo.yres;
            dstRect.bottom += vinfo.yres;

            gcuBlit(pContext, &blitData);
            gcuFinish(pContext);


            if(vinfo.yres_virtual >= vinfo.yres * 3)
            {

                dstRect.top    += vinfo.yres;
                dstRect.bottom += vinfo.yres;

                gcuBlit(pContext, &blitData);
                gcuFinish(pContext);
            }
        }

    }

    /* Free resource and terminate GCU */
    if(pSrcSurface)
    {
        _gcuDestroyBuffer(pContext, pSrcSurface);
        pSrcSurface = NULL;
    }

    if(pDstSurface)
    {
        _gcuDestroyBuffer(pContext, pDstSurface);
        pDstSurface = NULL;
    }
    gcuDestroyContext(pContext);
    gcuTerminate();


    /* Close framebuffer */
    if(fd)
    {
       close(fd);
    }

    return 0;
}
