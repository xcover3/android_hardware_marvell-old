/***********************************************************************************
 *
 *    Copyright (c) 2011 - 2013 by Marvell International Ltd. and its affiliates.
 *    All rights reserved.
 *
 *    This software file (the "File") is owned and distributed by Marvell
 *    International Ltd. and/or its affiliates ("Marvell") under Marvell Commercial
 *    License.
 *
 *    If you received this File from Marvell and you have entered into a commercial
 *    license agreement (a "Commercial License") with Marvell, the File is licensed
 *    to you under the terms of the applicable Commercial License.
 *
 ***********************************************************************************/

/*!
*******************************************************************************
*  \file gpu_veu.cpp
*  \brief
*      Implementation of VEU initialization and surface interfaces
******************************************************************************/

#include "gcu.h"
#include "gpu_veu.h"
#include "gpu_veu_internal.h"

static VEUContext g_pContext = VEU_NULL;
static VEUSurface g_pSurface = VEU_NULL;

/*Init a palette from 0-255 and 255-0, used for color effect gradient*/
VEUbool veuInitGradient()
{
    VEUContext pContext = _veuGetContext();
    VEUVirtualAddr      VirtAddr = 0;
    VEUPhysicalAddr     PhysAddr = 0;
    unsigned char*      color;
    VEUuint             i,j,k;
    if(pContext)
    {
        g_pSurface = _gcuCreateBuffer(pContext,
                                      16,
                                      256,
                                      GCU_FORMAT_ARGB8888,
                                      &VirtAddr,
                                      &PhysAddr);
        if(!g_pSurface)
        {
            _gcuDestroyBuffer(pContext, g_pSurface);
            g_pSurface = VEU_NULL;
            return VEU_FALSE;
        }
        color = (unsigned char *)VirtAddr;
        for(i = 0; i < 256; i++)
        {
            for(j = 0;j <8; j++)
            {
                k = 16*4*i + 4*j;
                color[k]   = i;
                color[k+1] = i;
                color[k+2] = i;
                color[k+3] = i;
            }
            for(j = 8;j <16; j++)
            {
                k = 16*4*i + 4*j;
                color[k]   = 255 - i;
                color[k+1] = 255 - i;
                color[k+2] = 255 - i;
                color[k+3] = 255 - i;
            }
        }
        _gcuFlushCache(pContext, VirtAddr, 16*256*4, GCU_CLEAN);
        return VEU_TRUE;
    }
    return VEU_FALSE;
}

/*!
*********************************************************************
*   \brief
*       VEU interface definition
*********************************************************************
*/
VEUbool veuInitialize(VEU_INIT_DATA* pData)
{
    VEUbool ret = VEU_FALSE;

    if(!g_pContext)
    {
        GCU_INIT_DATA       initData;
        GCU_CONTEXT_DATA    createData;

        VEU_ASSERT(pData);

        memset(&initData, 0, sizeof(initData));
        initData.version = GCU_VERSION_3_0;
        initData.debug   = pData->debug;
        ret = gcuInitialize(&initData);
        if(ret)
        {
            if(strcmp(gcuGetString(GCU_VERSION), "GCU 3.0 ver3000") < 0)
            {
                _veuDebugf("%s(%d) : GCU version not suitable (<3.0)\n", __func__, __LINE__);
                return VEU_FALSE;
            }

            memset(&createData, 0, sizeof(createData));
            createData.bRelaxMode = VEU_TRUE;
            g_pContext = gcuCreateContext(&createData);
            if(g_pContext)
            {
                ret = veuInitGradient();
                if(!ret)
                {
                    gcuTerminate();
                }
            }
            else
            {
                gcuTerminate();
                ret = VEU_FALSE;
            }
        }
    }
    else
    {
        ret = VEU_TRUE;
    }

    return ret;
}

VEUvoid veuTerminate()
{
    if(g_pContext)
    {
        if(g_pSurface)
        {
            _gcuDestroyBuffer(g_pContext, g_pSurface);
            g_pSurface = VEU_NULL;
        }
        gcuDestroyContext(g_pContext);
        gcuTerminate();
        g_pContext = VEU_NULL;
    }
}

/*
 * VEU Surface API
 */

/* Create surface in video memory, newly created buffer address returned in pVirtAddr and pPhysicalAddr  */
VEUSurface veuCreateSurface(VEUuint             width,
                            VEUuint             height,
                            VEU_FORMAT          format)
{
    VEUSurface pSurface = VEU_NULL;
    VEUContext pContext = _veuGetContext();
    VEUVirtualAddr      VirtAddr = 0;
    VEUPhysicalAddr     PhysAddr = 0;

    if(pContext)
    {
        //is YUV format
        if((format >= 200) && (format <= 207))
        {
            if(!IS_ALIGNED(width, 16) || !IS_ALIGNED(height, 4))
            {
                _veuDebugf("%s(%d) : width not align to 16 or height not align to 4.\n", __func__, __LINE__);
                return VEU_NULL;
            }
        }

        pSurface = _gcuCreateBuffer(pContext,
                                    width,
                                    height,
                                    (GCU_FORMAT)format,
                                    &VirtAddr,
                                    &PhysAddr);
    }
    return pSurface;
}


/*
 * Create pre-allocated surface, the pre-allocated buffer address passed by virtualAddr and physicalAddr
 *
 * Currently we only support below combinations :
 *  1) bPreAllocVirtual and bPreAllocPhysical are both VEU_TRUE.
 *  2) bPreAllocVirtual is VEU_TRUE but bPreAllocPhysical is VEU_FALSE.
 *
 * Currently We don't support below combinations
 *  1) bPreAllocVirtua is VEU_FALSE but bPreAllocPhysical is VEU_TRUE
 */
VEUSurface veuCreatePreAllocSurface(VEUuint             width,
                                    VEUuint             height,
                                    VEU_FORMAT          format,
                                    VEUbool             bPreAllocVirtual,
                                    VEUbool             bPreAllocPhysical,
                                    VEUVirtualAddr      virtualAddr1,
                                    VEUVirtualAddr      virtualAddr2,
                                    VEUVirtualAddr      virtualAddr3,
                                    VEUPhysicalAddr     physicalAddr1,
                                    VEUPhysicalAddr     physicalAddr2,
                                    VEUPhysicalAddr     physicalAddr3)

{
    VEUSurface pSurface = VEU_NULL;
    VEUContext pContext = _veuGetContext();
    if(pContext)
    {
        //is YUV format
        if((format >= 200) && (format <= 207))
        {
            if(!IS_ALIGNED(width, 16) || !IS_ALIGNED(height, 4))
            {
                _veuDebugf("%s(%d) : width not align to 16 or height not align to 4.\n", __func__, __LINE__);
                return GCU_NULL;
            }
            if(bPreAllocVirtual && bPreAllocPhysical)
            {
                /* physical address alignedment check */
                if(((VEUuint)physicalAddr1 | (VEUuint)physicalAddr2 | (VEUuint)physicalAddr3) & 63)
                {
                    _veuDebugf("%s(%d) : physicalAddr not aligned to 64 .\n", __func__, __LINE__);
                    return VEU_NULL;
                }
            }
            else if(bPreAllocVirtual && !bPreAllocPhysical)
            {
                /* virtual address alignedment check */
                if(((VEUuint)virtualAddr1 | (VEUuint)virtualAddr2 | (VEUuint)virtualAddr3) & 63)
                {
                    _veuDebugf("%s(%d) : virtualAddr not aligned to 64 .\n", __func__, __LINE__);
                    return VEU_NULL;
                }
            }
        }
        GCU_SURFACE_DATA    surfaceData;
        GCU_ALLOC_INFO      preAllocInfos[3];
        VEUuint             stride;
        memset(preAllocInfos, 0, sizeof(GCU_ALLOC_INFO)*3);
        memset(&surfaceData, 0, sizeof(surfaceData));
        switch(format)
        {
            case GCU_FORMAT_UYVY:
            case GCU_FORMAT_YUY2:
                stride = 2*width;
                preAllocInfos[0].width                  = width;
                preAllocInfos[0].height                 = height;
                preAllocInfos[0].stride                 = stride;
                preAllocInfos[0].virtualAddr            = (GCUVirtualAddr)virtualAddr1;
                preAllocInfos[0].physicalAddr           = (GCUPhysicalAddr)physicalAddr1;
                preAllocInfos[0].bMappedPhysical        = VEU_FALSE;

                surfaceData.location                    = GCU_SURFACE_LOCATION_VIDEO;
                surfaceData.dimention                   = GCU_SURFACE_2D;
                surfaceData.flag.value                  = 0;
                surfaceData.flag.bits.preAllocVirtual   = bPreAllocVirtual;
                surfaceData.flag.bits.preAllocPhysical  = bPreAllocPhysical;
                surfaceData.format                      = (GCU_FORMAT)format;
                surfaceData.width                       = width;
                surfaceData.height                      = height;
                surfaceData.arraySize                   = 1;
                surfaceData.pPreAllocInfos              = preAllocInfos;
                break;

            case GCU_FORMAT_I420:
            case GCU_FORMAT_YV12:
                stride = width;
                /* Y plane */
                preAllocInfos[0].width                  = width;
                preAllocInfos[0].height                 = height;
                preAllocInfos[0].stride                 = stride;
                preAllocInfos[0].virtualAddr            = (GCUVirtualAddr)virtualAddr1;
                preAllocInfos[0].physicalAddr           = (GCUPhysicalAddr)physicalAddr1;
                preAllocInfos[0].bMappedPhysical        = VEU_FALSE;

                /* U Plane */
                preAllocInfos[1].width                  = width  / 2;
                preAllocInfos[1].height                 = height / 2;
                preAllocInfos[1].stride                 = stride / 2;
                preAllocInfos[1].virtualAddr            = (GCUVirtualAddr)virtualAddr2;
                preAllocInfos[1].physicalAddr           = (GCUPhysicalAddr)physicalAddr2;
                preAllocInfos[1].bMappedPhysical        = VEU_FALSE;

                /* V Plane */
                preAllocInfos[2].width                  = width  / 2;
                preAllocInfos[2].height                 = height / 2;
                preAllocInfos[2].stride                 = stride / 2;
                preAllocInfos[2].virtualAddr            = (GCUVirtualAddr)virtualAddr3;
                preAllocInfos[2].physicalAddr           = (GCUPhysicalAddr)physicalAddr3;
                preAllocInfos[2].bMappedPhysical        = VEU_FALSE;

                surfaceData.location                    = GCU_SURFACE_LOCATION_VIDEO;
                surfaceData.dimention                   = GCU_SURFACE_2D;
                surfaceData.flag.value                  = 0;
                surfaceData.flag.bits.preAllocVirtual   = bPreAllocVirtual;
                surfaceData.flag.bits.preAllocPhysical  = bPreAllocPhysical;
                surfaceData.format                      = (GCU_FORMAT)format;
                surfaceData.width                       = width;
                surfaceData.height                      = height;
                surfaceData.arraySize                   = 3;
                surfaceData.pPreAllocInfos              = preAllocInfos;
                break;

            case GCU_FORMAT_NV12:
            case GCU_FORMAT_NV21:
                stride = width;
                /* Y plane */
                preAllocInfos[0].width                  = width;
                preAllocInfos[0].height                 = height;
                preAllocInfos[0].stride                 = stride;
                preAllocInfos[0].virtualAddr            = (GCUVirtualAddr)virtualAddr1;
                preAllocInfos[0].physicalAddr           = (GCUPhysicalAddr)physicalAddr1;
                preAllocInfos[0].bMappedPhysical        = VEU_FALSE;

                /* uv plane*/
                preAllocInfos[1].width                  = width  / 2;
                preAllocInfos[1].height                 = height / 2;
                preAllocInfos[1].stride                 = stride;
                preAllocInfos[1].virtualAddr            = (GCUVirtualAddr)virtualAddr2;
                preAllocInfos[1].physicalAddr           = (GCUPhysicalAddr)physicalAddr2;
                preAllocInfos[1].bMappedPhysical        = VEU_FALSE;

                surfaceData.location                    = GCU_SURFACE_LOCATION_VIDEO;
                surfaceData.dimention                   = GCU_SURFACE_2D;
                surfaceData.flag.value                  = 0;
                surfaceData.flag.bits.preAllocVirtual   = bPreAllocVirtual;
                surfaceData.flag.bits.preAllocPhysical  = bPreAllocPhysical;
                surfaceData.format                      = (GCU_FORMAT)format;
                surfaceData.width                       = width;
                surfaceData.height                      = height;
                surfaceData.arraySize                   = 2;
                surfaceData.pPreAllocInfos              = preAllocInfos;
                break;

            case GCU_FORMAT_NV16:
            case GCU_FORMAT_NV61:
                stride = width;
                /* Y plane */
                preAllocInfos[0].width                  = width;
                preAllocInfos[0].height                 = height;
                preAllocInfos[0].stride                 = stride;
                preAllocInfos[0].virtualAddr            = (GCUVirtualAddr)virtualAddr1;
                preAllocInfos[0].physicalAddr           = (GCUPhysicalAddr)physicalAddr1;
                preAllocInfos[0].bMappedPhysical        = VEU_FALSE;

                /* uv plane*/
                preAllocInfos[1].width                  = width  / 2;
                preAllocInfos[1].height                 = height;
                preAllocInfos[1].stride                 = stride;
                preAllocInfos[1].virtualAddr            = (GCUVirtualAddr)virtualAddr2;
                preAllocInfos[1].physicalAddr           = (GCUPhysicalAddr)physicalAddr2;
                preAllocInfos[1].bMappedPhysical        = VEU_FALSE;

                surfaceData.location                    = GCU_SURFACE_LOCATION_VIDEO;
                surfaceData.dimention                   = GCU_SURFACE_2D;
                surfaceData.flag.value                  = 0;
                surfaceData.flag.bits.preAllocVirtual   = bPreAllocVirtual;
                surfaceData.flag.bits.preAllocPhysical  = bPreAllocPhysical;
                surfaceData.format                      = (GCU_FORMAT)format;
                surfaceData.width                       = width;
                surfaceData.height                      = height;
                surfaceData.arraySize                   = 2;
                surfaceData.pPreAllocInfos              = preAllocInfos;
                break;

            default:
                _veuDebugf("%s(%d) : unsupported format: %d.\n", __func__, __LINE__, (VEUuint)format);
                return VEU_NULL;
        }
        pSurface = gcuCreateSurface(pContext, &surfaceData);
    }
    return pSurface;
}

VEUbool veuUpdatePreAllocSurface(VEUSurface          pSurface,
                                 VEUbool             bPreAllocVirtual,
                                 VEUbool             bPreAllocPhysical,
                                 VEUVirtualAddr      virtualAddr1,
                                 VEUVirtualAddr      virtualAddr2,
                                 VEUVirtualAddr      virtualAddr3,
                                 VEUPhysicalAddr     physicalAddr1,
                                 VEUPhysicalAddr     physicalAddr2,
                                 VEUPhysicalAddr     physicalAddr3)
{
    VEUContext              pContext = _veuGetContext();
    GCU_SURFACE_DATA        infoData;
    memset(&infoData, 0, sizeof(infoData));
    gcuQuerySurfaceInfo(pContext, (GCUSurface)pSurface, &infoData);
    VEU_FORMAT              format = (VEU_FORMAT)infoData.format;

    if(pContext)
    {
        if(pSurface)
        {
            //is YUV format
            if((format >= 200) && (format <= 207))
            {
                if(bPreAllocVirtual && bPreAllocPhysical)
                {
                    /* physical address alignedment check */
                    if(((VEUuint)physicalAddr1 | (VEUuint)physicalAddr2 | (VEUuint)physicalAddr3) & 63)
                    {
                        _veuDebugf("%s(%d) : physicalAddr not aligned to 64 .\n", __func__, __LINE__);
                        return VEU_FALSE;
                    }
                }
                else if(bPreAllocVirtual && !bPreAllocPhysical)
                {
                    /* virtual address alignedment check */
                    if(((VEUuint)virtualAddr1 | (VEUuint)virtualAddr2 | (VEUuint)virtualAddr3) & 63)
                    {
                        _veuDebugf("%s(%d) : virtualAddr not aligned to 64 .\n", __func__, __LINE__);
                        return VEU_FALSE;
                    }
                }
            }
            GCU_SURFACE_UPDATE_DATA  surfaceUpdate;
            surfaceUpdate.pSurface   = pSurface;
            surfaceUpdate.flag.value = 0;
            surfaceUpdate.arraySize  = infoData.arraySize;

            surfaceUpdate.flag.bits.preAllocVirtual  = bPreAllocVirtual;
            surfaceUpdate.flag.bits.preAllocPhysical = bPreAllocPhysical;

            GCU_ALLOC_INFO preAllocInfos[3];
            memset(preAllocInfos, 0, sizeof(GCU_ALLOC_INFO)*3);

            switch(format)
            {
                case GCU_FORMAT_UYVY:
                case GCU_FORMAT_YUY2:
                    preAllocInfos[0].virtualAddr            = (GCUVirtualAddr)virtualAddr1;
                    preAllocInfos[0].physicalAddr           = (GCUPhysicalAddr)physicalAddr1;
                    preAllocInfos[0].bMappedPhysical        = VEU_FALSE;

                    break;

                case GCU_FORMAT_I420:
                case GCU_FORMAT_YV12:
                    /* Y plane */
                    preAllocInfos[0].virtualAddr            = (GCUVirtualAddr)virtualAddr1;
                    preAllocInfos[0].physicalAddr           = (GCUPhysicalAddr)physicalAddr1;
                    preAllocInfos[0].bMappedPhysical        = VEU_FALSE;

                    /* U Plane */
                    preAllocInfos[1].virtualAddr            = (GCUVirtualAddr)virtualAddr2;
                    preAllocInfos[1].physicalAddr           = (GCUPhysicalAddr)physicalAddr2;
                    preAllocInfos[1].bMappedPhysical        = VEU_FALSE;

                    /* V Plane */
                    preAllocInfos[2].virtualAddr            = (GCUVirtualAddr)virtualAddr3;
                    preAllocInfos[2].physicalAddr           = (GCUPhysicalAddr)physicalAddr3;
                    preAllocInfos[2].bMappedPhysical        = VEU_FALSE;

                    break;

                case GCU_FORMAT_NV12:
                case GCU_FORMAT_NV21:
                    /* Y plane */
                    preAllocInfos[0].virtualAddr            = (GCUVirtualAddr)virtualAddr1;
                    preAllocInfos[0].physicalAddr           = (GCUPhysicalAddr)physicalAddr1;
                    preAllocInfos[0].bMappedPhysical        = VEU_FALSE;

                    /* uv plane*/
                    preAllocInfos[1].virtualAddr            = (GCUVirtualAddr)virtualAddr2;
                    preAllocInfos[1].physicalAddr           = (GCUPhysicalAddr)physicalAddr2;
                    preAllocInfos[1].bMappedPhysical        = VEU_FALSE;

                    break;

                case GCU_FORMAT_NV16:
                case GCU_FORMAT_NV61:
                    /* Y plane */
                    preAllocInfos[0].virtualAddr            = (GCUVirtualAddr)virtualAddr1;
                    preAllocInfos[0].physicalAddr           = (GCUPhysicalAddr)physicalAddr1;
                    preAllocInfos[0].bMappedPhysical        = VEU_FALSE;

                    /* uv plane*/
                    preAllocInfos[1].virtualAddr            = (GCUVirtualAddr)virtualAddr2;
                    preAllocInfos[1].physicalAddr           = (GCUPhysicalAddr)physicalAddr2;
                    preAllocInfos[1].bMappedPhysical        = VEU_FALSE;

                    break;

                default:
                    _veuDebugf("%s(%d) : unsupported format: %d.\n", __func__, __LINE__, (VEUuint)format);
                    return VEU_FALSE;
            }
            if(!bPreAllocVirtual)
            {
                preAllocInfos[0].virtualAddr  = preAllocInfos[1].virtualAddr  = preAllocInfos[2].virtualAddr  = VEU_NULL;
            }
            if(!bPreAllocPhysical)
            {
                preAllocInfos[0].physicalAddr = preAllocInfos[1].physicalAddr = preAllocInfos[2].physicalAddr = 0;
            }

            surfaceUpdate.pAllocInfos = preAllocInfos;

            return gcuUpdateSurface(pContext, &surfaceUpdate);
        }
    }
    return VEU_FALSE;
}

/* Destroy buffer, whether pre-allocated or none-preallocated */
VEUvoid veuDestroySurface(VEUSurface pSurface)
{
    VEUContext pContext = _veuGetContext();
    if(pContext)
    {
        VEU_ASSERT(pSurface);
        _gcuDestroyBuffer(pContext, pSurface);
    }
}

/*
 *  Internal interface
 */
VEUContext _veuGetContext()
{
    return g_pContext;
}

VEUSurface _veuGetGradient()
{
    return g_pSurface;
}

VEUvoid _veuFlush(VEUContext pContext)
{
    gcuFlush(pContext);
}

VEUvoid _veuFinish(VEUContext pContext)
{
    gcuFinish(pContext);
}

VEUbool _veuSaveSurface(VEUSurface pSurface, const char* prefix)
{
    VEUContext pContext = _veuGetContext();
    if(pContext)
    {
        return _gcuDumpSurface(pContext, pSurface, prefix);
    }
    return VEU_FALSE;
}

VEUSurface _veuLoadSurface(const char*   filename,
                           VEU_FORMAT    format,
                           VEUuint       width,
                           VEUuint       height)
{
    VEUContext pContext = _veuGetContext();
    VEUSurface pSurface = VEU_NULL;
    if(pContext)
    {
        pSurface = _gcuLoadYUVSurfaceFromFile(pContext, filename, (GCU_FORMAT)format, width, height);
    }
    return pSurface;
}

