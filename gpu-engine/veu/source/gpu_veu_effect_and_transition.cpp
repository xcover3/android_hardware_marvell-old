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
*  \file gpu_veu_effect_and_transition.cpp
*  \brief
*      Implementation of VEU effect and transition
******************************************************************************/

#include "gcu.h"
#include "gpu_veu.h"
#include "gpu_veu_internal.h"

#include "M4VSS3GPP_API.h"
#include "M4xVSS_API.h"
#include "M4xVSS_Internal.h"

VEUbool _veuGetYUVSurface(VEUContext pContext,
                          VEUSurface pSrcSurface,
                          VEUSurface* ppYSurface,
                          VEUSurface* ppUSurface,
                          VEUSurface* ppVSurface)
{
    VEUbool                 ret = VEU_FALSE;
    VEUSurface              pYSurface = VEU_NULL;
    VEUSurface              pUSurface = VEU_NULL;
    VEUSurface              pVSurface = VEU_NULL;
    GCU_SURFACE_DATA        infoData;
    GCU_SURFACE_LOCK_DATA   lockData;
    GCU_ALLOC_INFO          allocInfos[3];
    VEUbool                 bLockOk = VEU_FALSE;

    VEU_ASSERT(pContext);
    VEU_ASSERT(pSrcSurface);
    VEU_ASSERT(ppYSurface);
    VEU_ASSERT(ppUSurface);
    VEU_ASSERT(ppVSurface);

    /* check whether format is supported */
    memset(&infoData, 0, sizeof(infoData));
    gcuQuerySurfaceInfo(pContext, pSrcSurface, &infoData);
    if( (infoData.format != (VEUuint)VEU_FORMAT_I420) &&
        (infoData.format != (VEUuint)VEU_FORMAT_YV12) &&
        (infoData.arraySize != 3) )
    {
        _veuDebugf("%s(%d) : the format(%d) is not supported", __FUNCTION__, __LINE__, infoData.format);
        return ret;
    }

    memset(&lockData, 0, sizeof(lockData));
    lockData.pSurface           = pSrcSurface;
    lockData.flag.bits.preserve = 1;
    lockData.arraySize          = 3;
    lockData.pAllocInfos        = allocInfos;
    bLockOk = gcuLockSurface(pContext, &lockData);
    if(bLockOk)
    {
        pYSurface = _gcuCreatePreAllocBuffer(pContext,
                                             allocInfos[0].width / 4,
                                             allocInfos[0].height,
                                             GCU_FORMAT_ARGB8888,
                                             VEU_TRUE,
                                             allocInfos[0].virtualAddr,
                                             VEU_TRUE,
                                             allocInfos[0].physicalAddr);

        pUSurface = _gcuCreatePreAllocBuffer(pContext,
                                             allocInfos[1].width / 4,
                                             allocInfos[1].height,
                                             GCU_FORMAT_ARGB8888,
                                             VEU_TRUE,
                                             allocInfos[1].virtualAddr,
                                             VEU_TRUE,
                                             allocInfos[1].physicalAddr);

        pVSurface = _gcuCreatePreAllocBuffer(pContext,
                                             allocInfos[2].width / 4,
                                             allocInfos[2].height,
                                             GCU_FORMAT_ARGB8888,
                                             VEU_TRUE,
                                             allocInfos[2].virtualAddr,
                                             VEU_TRUE,
                                             allocInfos[2].physicalAddr);

        if(pYSurface && pUSurface && pVSurface)
        {
            *ppYSurface = pYSurface;
            *ppUSurface = pUSurface;
            *ppVSurface = pVSurface;
            ret = VEU_TRUE;
        }
        else
        {
            if(pYSurface)
            {
                _gcuDestroyBuffer(pContext, pYSurface);
                pYSurface = VEU_NULL;
            }

            if(pUSurface)
            {
                _gcuDestroyBuffer(pContext, pUSurface);
                pUSurface = VEU_NULL;
            }

            if(pVSurface)
            {
                _gcuDestroyBuffer(pContext, pVSurface);
                pVSurface = VEU_NULL;
            }
        }

        gcuUnlockSurface(pContext, pSrcSurface);
    }

    return ret;
}

VEUvoid _veuReleaseYUVSurface(VEUContext pContext,
                              VEUSurface pSrcSurface,
                              VEUSurface pYSurface,
                              VEUSurface pUSurface,
                              VEUSurface pVSurface)
{
    VEU_ASSERT(pContext);
    VEU_ASSERT(pSrcSurface);
    VEU_ASSERT(pYSurface);
    VEU_ASSERT(pUSurface);
    VEU_ASSERT(pVSurface);

    _gcuDestroyBuffer(pContext, pYSurface);
    _gcuDestroyBuffer(pContext, pUSurface);
    _gcuDestroyBuffer(pContext, pVSurface);
}

VEUvoid _veuFill(VEUContext pContext, VEUSurface pSurface, VEUuint color, GCU_RECT *pRect)
{
    GCU_FILL_DATA fillData;
    VEUuint       fillColor = 0;

    VEU_ASSERT(pContext);
    VEU_ASSERT(pSurface);

    fillColor = color & 0x000000ff;
    fillColor = (color << 24) | (color << 16) | (color << 8) | color;

    memset(&fillData, 0, sizeof(fillData));
    fillData.pSurface    = pSurface;
    fillData.bSolidColor = GCU_TRUE;
    fillData.color       = fillColor;
    fillData.pRect       = pRect;
    gcuFill(pContext, &fillData);
}

VEUvoid _veuBlit(VEUContext pContext, VEUSurface pDstSurface, VEUSurface pSrcSurface, GCU_RECT *pDstRect, GCU_RECT *pSrcRect)
{
    GCU_BLT_DATA blitData;

    VEU_ASSERT(pContext);
    VEU_ASSERT(pDstSurface);
    VEU_ASSERT(pSrcSurface);

    memset(&blitData, 0, sizeof(blitData));
    blitData.pSrcSurface = pSrcSurface;
    blitData.pDstSurface = pDstSurface;
    blitData.pSrcRect    = pSrcRect;
    blitData.pDstRect    = pDstRect;
    gcuBlit(pContext, &blitData);
}

VEUvoid _veuResizeBilinear(VEUContext pContext, VEUSurface pDstSurface, VEUSurface pSrcSurface, GCU_RECT *pRect)
{
    GCU_BLT_DATA blitData;

    VEU_ASSERT(pContext);
    VEU_ASSERT(pDstSurface);
    VEU_ASSERT(pSrcSurface);

    memset(&blitData, 0, sizeof(blitData));
    blitData.pSrcSurface = pSrcSurface;
    blitData.pDstSurface = pDstSurface;
    blitData.pSrcRect    = pRect;
    blitData.pDstRect    = VEU_NULL;
    gcuSet(pContext, GCU_QUALITY, GCU_QUALITY_HIGH);
    gcuBlit(pContext, &blitData);
    _veuFinish(pContext);
    gcuSet(pContext, GCU_QUALITY, GCU_QUALITY_NORMAL);
}

VEUvoid _veumodifyLumaWithScale(VEUContext pContext, VEUSurface pDstSurface, VEUSurface pSrcSurface, VEUuint luma)
{
    GCU_BLEND_DATA blendData;

    VEU_ASSERT(pContext);
    VEU_ASSERT(pDstSurface);
    VEU_ASSERT(pSrcSurface);

    VEUbool    bSrcOk = VEU_FALSE;
    VEUbool    bDstOk = VEU_FALSE;

    VEUSurface pSrcYSurface = VEU_NULL;
    VEUSurface pSrcUSurface = VEU_NULL;
    VEUSurface pSrcVSurface = VEU_NULL;
    VEUSurface pDstYSurface = VEU_NULL;
    VEUSurface pDstUSurface = VEU_NULL;
    VEUSurface pDstVSurface = VEU_NULL;

    bSrcOk = _veuGetYUVSurface(pContext, pSrcSurface, &pSrcYSurface, &pSrcUSurface, &pSrcVSurface);
    bDstOk = _veuGetYUVSurface(pContext, pDstSurface, &pDstYSurface, &pDstUSurface, &pDstVSurface);

    if(bSrcOk && bDstOk)
    {
        /* apply luma factor */
        memset(&blendData, 0, sizeof(blendData));
        blendData.pSrcSurface    = pSrcYSurface;
        blendData.pDstSurface    = pDstYSurface;
        blendData.pSrcRect       = VEU_NULL;
        blendData.pDstRect       = VEU_NULL;
        blendData.srcGlobalAlpha = luma;
        blendData.dstGlobalAlpha = 255;
        blendData.blendMode      = GCU_BLEND_SRC;
        gcuBlend(pContext, &blendData);

        /* copy or filter chroma */
        if(luma>=64)
        {
            _veuBlit(pContext, pDstUSurface, pSrcUSurface, VEU_NULL, VEU_NULL);
            _veuBlit(pContext, pDstVSurface, pSrcVSurface, VEU_NULL, VEU_NULL);
        }
        else
        {
            VEUuchar pix  = (VEUuchar)((VEUfloat)(128/255.)*((255-luma)/255.)*255. + 0.5);
            VEUuint color = (VEUuint)((pix << 24) | (pix << 16) | (pix << 8) | pix);
            _veuFill(pContext, pDstUSurface, color, VEU_NULL);
            _veuFill(pContext, pDstVSurface, color, VEU_NULL);

            memset(&blendData, 0, sizeof(blendData));
            blendData.pSrcSurface    = pSrcUSurface;
            blendData.pDstSurface    = pDstUSurface;
            blendData.pSrcRect       = VEU_NULL;
            blendData.pDstRect       = VEU_NULL;
            blendData.srcGlobalAlpha = luma;
            blendData.dstGlobalAlpha = 255;
            blendData.blendMode      = GCU_BLEND_PLUS;
            gcuBlend(pContext, &blendData);

            memset(&blendData, 0, sizeof(blendData));
            blendData.pSrcSurface    = pSrcVSurface;
            blendData.pDstSurface    = pDstVSurface;
            blendData.pSrcRect       = VEU_NULL;
            blendData.pDstRect       = VEU_NULL;
            blendData.srcGlobalAlpha = luma;
            blendData.dstGlobalAlpha = 255;
            blendData.blendMode      = GCU_BLEND_PLUS;
            gcuBlend(pContext, &blendData);
        }
        _veuFinish(pContext);
    }
    if(bSrcOk)
    {
        _veuReleaseYUVSurface(pContext, pSrcSurface, pSrcYSurface, pSrcUSurface, pSrcVSurface);
    }

    if(bDstOk)
    {
        _veuReleaseYUVSurface(pContext, pDstSurface, pDstYSurface, pDstUSurface, pDstVSurface);
    }

}

VEUbool veuEffectColorProc(VEUvoid*     pFunctionContext,
                           VEUSurface     pSrcSurface,
                           VEUSurface     pDstSurface,
                           VEUvoid*       pProgress,
                           VEUulong       uiEffectKind)
{
    M4xVSS_ColorStruct* ColorContext = (M4xVSS_ColorStruct*)pFunctionContext;
    VEUbool     ret      = VEU_FALSE;
    VEUContext  pContext = _veuGetContext();
    if(pContext)
    {
        VEUbool    bSrcOk = VEU_FALSE;
        VEUbool    bDstOk = VEU_FALSE;

        VEUSurface pSrcYSurface = VEU_NULL;
        VEUSurface pSrcUSurface = VEU_NULL;
        VEUSurface pSrcVSurface = VEU_NULL;
        VEUSurface pDstYSurface = VEU_NULL;
        VEUSurface pDstUSurface = VEU_NULL;
        VEUSurface pDstVSurface = VEU_NULL;

        VEU_ASSERT(pSrcSurface);
        VEU_ASSERT(pDstSurface);

        bSrcOk = _veuGetYUVSurface(pContext, pSrcSurface, &pSrcYSurface, &pSrcUSurface, &pSrcVSurface);
        bDstOk = _veuGetYUVSurface(pContext, pDstSurface, &pDstYSurface, &pDstUSurface, &pDstVSurface);

        if(bSrcOk && bDstOk)
        {
            switch(ColorContext->colorEffectType)
            {
                case M4xVSS_kVideoEffectType_BlackAndWhite:
                    {
                        _veuBlit(pContext, pDstYSurface, pSrcYSurface, VEU_NULL, VEU_NULL);
                        _veuFill(pContext, pDstUSurface, 128, VEU_NULL);
                        _veuFill(pContext, pDstVSurface, 128, VEU_NULL);
                        _veuFinish(pContext);
                        ret = VEU_TRUE;
                    }
                    break;

                case M4xVSS_kVideoEffectType_Pink:
                    {
                        _veuBlit(pContext, pDstYSurface, pSrcYSurface, VEU_NULL, VEU_NULL);
                        _veuFill(pContext, pDstUSurface, 255, VEU_NULL);
                        _veuFill(pContext, pDstVSurface, 255, VEU_NULL);
                        _veuFinish(pContext);
                        ret = VEU_TRUE;
                    }
                    break;

                case M4xVSS_kVideoEffectType_Green:
                    {
                        _veuBlit(pContext, pDstYSurface, pSrcYSurface, VEU_NULL, VEU_NULL);
                        _veuFill(pContext, pDstUSurface, 0, VEU_NULL);
                        _veuFill(pContext, pDstVSurface, 0, VEU_NULL);
                        _veuFinish(pContext);
                        ret = VEU_TRUE;
                    }
                    break;

                case M4xVSS_kVideoEffectType_Sepia:
                    {
                        _veuBlit(pContext, pDstYSurface, pSrcYSurface, VEU_NULL, VEU_NULL);
                        _veuFill(pContext, pDstUSurface, 117, VEU_NULL);
                        _veuFill(pContext, pDstVSurface, 139, VEU_NULL);
                        _veuFinish(pContext);
                        ret = VEU_TRUE;
                    }
                    break;

                case M4xVSS_kVideoEffectType_Gradient:
                    {
                        VEUushort r   = 0;
                        VEUushort g   = 0;
                        VEUushort b   = 0;
                        VEUushort u   = 0;
                        VEUushort v   = 0;
                        GCU_RECT rectU;
                        GCU_RECT rectV;

                        /*first get the r, g, b*/
                        b = (ColorContext->rgb16ColorData &  0x001f);
                        g = (ColorContext->rgb16ColorData &  0x07e0)>>5;
                        r = (ColorContext->rgb16ColorData &  0xf800)>>11;
                        u = _veuU16(r, g, b);
                        v = _veuV16(r, g, b);
                        if(u>128)
                        {
                            rectU.left      = 8;
                            rectU.right     = 16;
                            rectU.top       = 255-u;
                            rectU.bottom    = 128;
                        }
                        else
                        {
                            rectU.left      = 0;
                            rectU.right     = 8;
                            rectU.top       = u;
                            rectU.bottom    = 128;
                        }
                        if(v>128)
                        {
                            rectV.left      = 8;
                            rectV.right     = 16;
                            rectV.top       = 255-v;
                            rectV.bottom    = 128;
                        }
                        else
                        {
                            rectV.left      = 0;
                            rectV.right     = 8;
                            rectV.top       = v;
                            rectV.bottom    = 128;
                        }

                        VEUSurface pGradient = _veuGetGradient();

                        _veuBlit(pContext, pDstUSurface, pGradient, VEU_NULL, &rectU);
                        _veuBlit(pContext, pDstVSurface, pGradient, VEU_NULL, &rectV);

                        /* copy Y surface */
                        _veuBlit(pContext, pDstYSurface, pSrcYSurface, VEU_NULL, VEU_NULL);

                        _veuFinish(pContext);
                        ret = VEU_TRUE;
                    }
                    break;

                case M4xVSS_kVideoEffectType_ColorRGB16:
                    {
                        VEUushort r   = 0;
                        VEUushort g   = 0;
                        VEUushort b   = 0;
                        VEUushort u   = 0;
                        VEUushort v   = 0;

                        /*first get the r, g, b*/
                        b = (ColorContext->rgb16ColorData &  0x001f);
                        g = (ColorContext->rgb16ColorData &  0x07e0)>>5;
                        r = (ColorContext->rgb16ColorData &  0xf800)>>11;

                        /* copy Y surface */
                        _veuBlit(pContext, pDstYSurface, pSrcYSurface, VEU_NULL, VEU_NULL);

                        /*convert to u*/
                        u = _veuU16(r, g, b);
                        _veuFill(pContext, pDstUSurface, u, VEU_NULL);

                        /*convert to v*/
                        v = _veuV16(r, g, b);
                        _veuFill(pContext, pDstVSurface, v, VEU_NULL);

                        _veuFinish(pContext);
                        ret = VEU_TRUE;
                    }
                    break;

                case M4xVSS_kVideoEffectType_Negative:
                    {
                        _veuBlit(pContext, pDstUSurface, pSrcUSurface, VEU_NULL, VEU_NULL);
                        _veuBlit(pContext, pDstVSurface, pSrcVSurface, VEU_NULL, VEU_NULL);

                        /* Luma NAGATIVE:  *dst = 255 - *src */
                        GCU_ROP_DATA ropData;
                        memset(&ropData, 0, sizeof(ropData));
                        ropData.pSrcSurface = pSrcYSurface;
                        ropData.pDstSurface = pDstYSurface;
                        ropData.rop         = 0x33;
                        gcuRop(pContext, &ropData);

                        _veuFinish(pContext);
                        ret = VEU_TRUE;
                    }
                    break;

                default:
                    _veuDebugf("%s(%d) : unknown effect(%d)", __FUNCTION__, __LINE__, ColorContext->colorEffectType);
                    break;
            }
        }

        if(bSrcOk)
        {
            _veuReleaseYUVSurface(pContext, pSrcSurface, pSrcYSurface, pSrcUSurface, pSrcVSurface);
        }

        if(bDstOk)
        {
            _veuReleaseYUVSurface(pContext, pDstSurface, pDstYSurface, pDstUSurface, pDstVSurface);
        }
    }

    return ret;
}

VEUbool veuEffectZoomProc(VEUvoid*       pFunctionContext,
                          VEUSurface     pSrcSurface,
                          VEUSurface     pDstSurface,
                          VEUvoid*       pProgress,
                          VEUulong       uiEffectKind)
{
    VEUbool     ret      = VEU_FALSE;
    VEUContext  pContext = _veuGetContext();
    M4VSS3GPP_ExternalProgress* pProcessData = (M4VSS3GPP_ExternalProgress*) pProgress;
    if(pContext)
    {
        VEU_ASSERT(pSrcSurface);
        VEU_ASSERT(pDstSurface);

        GCU_RECT         rect;
        GCU_SURFACE_DATA infoData;
        VEUuint          width    = 0;
        VEUuint          height   = 0;
        VEUuint          ratio    = 0;
        /*  * 1.189207 between ratio */
        /* zoom between x1 and x16 */
        VEUuint ratiotab[17] = {1024,1218,1448,1722,2048,2435,2896,3444,4096,4871,5793,\
                                    6889,8192,9742,11585,13777,16384};

        if(M4xVSS_kVideoEffectType_ZoomOut == (VEUulong)pFunctionContext)
        {
            //ratio = 16 - (15 * pProgress->uiProgress)/1000;
            ratio = 16 - pProcessData->uiProgress / 66 ;
        }
        else if(M4xVSS_kVideoEffectType_ZoomIn == (VEUulong)pFunctionContext)
        {
            //ratio = 1 + (15 * pProgress->uiProgress)/1000;
            ratio = 1 + pProcessData->uiProgress / 66 ;
        }

        memset(&infoData, 0, sizeof(infoData));
        gcuQuerySurfaceInfo(pContext, pSrcSurface, &infoData);
        width       = (infoData.width << 10) / ratiotab[ratio];
        height      = (infoData.height << 10) / ratiotab[ratio];

        width       = width & (~1);
        height      = height & (~1);

        rect.left   = ((infoData.width >> 1) - (width >> 1)) & (~1);
        rect.top    = ((infoData.height >> 1) - (height >> 1)) & (~1);
        rect.right  = rect.left + width;
        rect.bottom = rect.top + height;

        _veuResizeBilinear(pContext, pDstSurface, pSrcSurface, &rect);

        ret = VEU_TRUE;

    }
    return ret;
}

VEUbool veuSlideTransitionProc(VEUvoid*       userData,
                               VEUSurface     pSrcSurface1,
                               VEUSurface     pSrcSurface2,
                               VEUSurface     pDstSurface,
                               VEUvoid*       pProgress,
                               VEUulong       uiTransitionKind)
{
    M4xVSS_internal_SlideTransitionSettings* settings = (M4xVSS_internal_SlideTransitionSettings*) userData;
    M4VSS3GPP_ExternalProgress* pProcessData    = (M4VSS3GPP_ExternalProgress*) pProgress;
    VEUbool     ret      = VEU_FALSE;
    VEUContext  pContext = _veuGetContext();
    if(pContext)
    {
        VEU_ASSERT(pSrcSurface1);
        VEU_ASSERT(pSrcSurface2);
        VEU_ASSERT(pDstSurface);

        VEUuint          shift;
        GCU_RECT         srcRect1;
        GCU_RECT         srcRect2;
        GCU_RECT         dstRect1;
        GCU_RECT         dstRect2;
        GCU_SURFACE_DATA infoData;
        VEUuint          width    = 0;
        VEUuint          height   = 0;
        VEUSurface       pSrc1Surfacetemp;
        VEUSurface       pSrc2Surfacetemp;

        memset(&infoData, 0, sizeof(infoData));
        gcuQuerySurfaceInfo(pContext, pDstSurface, &infoData);
        width    = infoData.width;
        height   = infoData.height;
        if ((M4xVSS_SlideTransition_RightOutLeftIn == settings->direction)
            || (M4xVSS_SlideTransition_LeftOutRightIn == settings->direction) )
        {
            /* horizontal slide */
            shift = (width * pProcessData->uiProgress)/1000;
            if (M4xVSS_SlideTransition_RightOutLeftIn == settings->direction)
            {
                /* Put the previous clip frame right, the next clip frame left, and reverse shiftUV
                (since it's a shift from the left frame) so that we start out on the right
                (i.e. not left) frame, it
                being from the previous clip. */
                pSrc1Surfacetemp = pSrcSurface2;
                pSrc2Surfacetemp = pSrcSurface1;
                shift = width - shift;
            }
            else
            {
                pSrc1Surfacetemp = pSrcSurface1;
                pSrc2Surfacetemp = pSrcSurface2;
            }
            if (0 == shift)    /* output left frame */
            {
                _veuBlit(pContext, pDstSurface, pSrc1Surfacetemp, VEU_NULL, VEU_NULL);

            }

            else if (width == shift) /* output right frame */
            {
                _veuBlit(pContext, pDstSurface, pSrc2Surfacetemp, VEU_NULL, VEU_NULL);

            }
            else
            {
                dstRect1.left         = 0;
                dstRect1.top          = 0;
                dstRect1.right        = width - shift;
                dstRect1.bottom       = height;

                dstRect2.left         = width - shift;
                dstRect2.top          = 0;
                dstRect2.right        = width;
                dstRect2.bottom       = height;

                srcRect1.left         = shift;
                srcRect1.top          = 0;
                srcRect1.right        = width;
                srcRect1.bottom       = height;

                srcRect2.left         = 0;
                srcRect2.top          = 0;
                srcRect2.right        = shift;
                srcRect2.bottom       = height;

                /* copy Y surface */
                _veuBlit(pContext, pDstSurface, pSrc1Surfacetemp, &dstRect1, &srcRect1);
                _veuBlit(pContext, pDstSurface, pSrc2Surfacetemp, &dstRect2, &srcRect2);

            }
        }
        else
        {
            /* vertical slide */
            shift = (height * pProcessData->uiProgress)/1000;
            if (M4xVSS_SlideTransition_BottomOutTopIn == settings->direction)
            {
                pSrc1Surfacetemp = pSrcSurface2;
                pSrc2Surfacetemp = pSrcSurface1;
                shift = height - shift;
            }
            else
            {
                pSrc1Surfacetemp = pSrcSurface1;
                pSrc2Surfacetemp = pSrcSurface2;
            }
            if (0 == shift)    /* output left frame */
            {
                _veuBlit(pContext, pDstSurface, pSrc1Surfacetemp, VEU_NULL, VEU_NULL);

            }

            else if (height == shift) /* output right frame */
            {
                _veuBlit(pContext, pDstSurface, pSrc2Surfacetemp, VEU_NULL, VEU_NULL);

            }
            else
            {
                dstRect1.left         = 0;
                dstRect1.top          = 0;
                dstRect1.right        = width;
                dstRect1.bottom       = height - shift;

                dstRect2.left         = 0;
                dstRect2.top          = height - shift;
                dstRect2.right        = width;
                dstRect2.bottom       = height;

                srcRect1.left         = 0;
                srcRect1.top          = shift;
                srcRect1.right        = width;
                srcRect1.bottom       = height;

                srcRect2.left         = 0;
                srcRect2.top          = 0;
                srcRect2.right        = width;
                srcRect2.bottom       = shift;

                /* copy Y surface */
                _veuBlit(pContext, pDstSurface, pSrc1Surfacetemp, &dstRect1, &srcRect1);
                _veuBlit(pContext, pDstSurface, pSrc2Surfacetemp, &dstRect2, &srcRect2);
            }
        }
        _veuFinish(pContext);
        ret = VEU_TRUE;
    }
    return ret;
}

VEUbool veuFadeBlackProc(VEUvoid*       userData,
                         VEUSurface     pSrcSurface1,
                         VEUSurface     pSrcSurface2,
                         VEUSurface     pDstSurface,
                         VEUvoid*       pProgress,
                         VEUulong       uiTransitionKind)
{
    VEUbool     ret      = VEU_FALSE;
    VEUContext  pContext = _veuGetContext();
    M4VSS3GPP_ExternalProgress* pProcessData = (M4VSS3GPP_ExternalProgress*) pProgress;
    if(pContext)
    {
        VEU_ASSERT(pSrcSurface1);
        VEU_ASSERT(pSrcSurface2);
        VEU_ASSERT(pDstSurface);

        VEUuint tmp;
        if((pProcessData->uiProgress) < 500)
        {
            /**
             * Compute where we are in the effect (scale is 0->255) */
            tmp = (VEUuint)((1.0 - ((VEUfloat)(pProcessData->uiProgress*2)/1000)) * 255 );

            /**
             * Apply the darkening effect */
            _veumodifyLumaWithScale(pContext, pDstSurface, pSrcSurface1, tmp);
            ret = VEU_TRUE;

        }
        else
        {
            /**
             * Compute where we are in the effect (scale is 0->255). */
            tmp = (VEUuint)( (((VEUfloat)(((pProcessData->uiProgress-500)*2))/1000)) * 255 );

            /**
             * Apply the darkening effect */
            _veumodifyLumaWithScale(pContext, pDstSurface, pSrcSurface2, tmp);
            ret = VEU_TRUE;
        }
    }
    return ret;
}

VEUbool veuEffectProc(VEUvoid*       pFunctionContext,
                      VEUSurface     pSrc,
                      VEUSurface     pDst,
                      VEUvoid*       pProgress,
                      VEUulong       uiEffectKind)
{
    VEUbool     ret      = VEU_FALSE;
    switch(uiEffectKind + M4VSS3GPP_kVideoTransitionType_External)
    {
        case M4xVSS_kVideoEffectType_BlackAndWhite :
        case M4xVSS_kVideoEffectType_Pink :
        case M4xVSS_kVideoEffectType_Green :
        case M4xVSS_kVideoEffectType_Sepia :
        case M4xVSS_kVideoEffectType_Negative :
        case M4xVSS_kVideoEffectType_ColorRGB16 :
        case M4xVSS_kVideoEffectType_Gradient :
            ret = veuEffectColorProc(pFunctionContext, pSrc, pDst, pProgress, uiEffectKind);
            return ret;
       case M4xVSS_kVideoEffectType_ZoomIn:
       case M4xVSS_kVideoEffectType_ZoomOut:
            ret = veuEffectZoomProc(pFunctionContext, pSrc, pDst, pProgress, uiEffectKind);
            return ret;
        default:
            _veuDebugf("%s(%d) : unsupported EffectKind: %d.\n", __func__, __LINE__, (VEUuint)uiEffectKind);
            return VEU_FALSE;
    }
}

VEUbool veuTransitionProc(VEUvoid*       userData,
                          VEUSurface     pSrc1,
                          VEUSurface     pSrc2,
                          VEUSurface     pDst,
                          VEUvoid*       pProgress,
                          VEUulong       uiTransitionKind)
{
    VEUbool     ret      = VEU_FALSE;
    switch(uiTransitionKind + M4VSS3GPP_kVideoTransitionType_External)
    {
        case M4xVSS_kVideoTransitionType_SlideTransition :
            ret = veuSlideTransitionProc(userData, pSrc1, pSrc2, pDst, pProgress, uiTransitionKind);
            return ret;
       case M4xVSS_kVideoTransitionType_FadeBlack :
            ret = veuFadeBlackProc(userData, pSrc1, pSrc2, pDst, pProgress, uiTransitionKind);
            return ret;
        default:
            _veuDebugf("%s(%d) : unsupported TransitionKind: %d.\n", __func__, __LINE__, (VEUuint)uiTransitionKind);
            return VEU_FALSE;
    }

}
