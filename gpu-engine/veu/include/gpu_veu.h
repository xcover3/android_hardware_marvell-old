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
*  \file gpu_veu.h
*  \brief
*      Decalration of VEU internal interface
******************************************************************************/

#ifndef __GPU_VEU_H__
#define __GPU_VEU_H__

/*check if the compiler is of C++*/
#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int            VEUbool;
typedef unsigned int            VEUenum;
typedef int                     VEUint;
typedef unsigned int            VEUuint;
typedef float                   VEUfloat;
typedef void                    VEUvoid;
typedef unsigned char           VEUuchar;
typedef unsigned long           VEUulong;
typedef unsigned short          VEUushort;

typedef void*                   VEUContext;
typedef void*                   VEUSurface;
typedef void*                   VEUFence;

typedef void*                   VEUVirtualAddr;
typedef unsigned int            VEUPhysicalAddr;

#define VEU_FALSE               0
#define VEU_TRUE                1

#ifdef __cplusplus
#define VEU_NULL                0
#else
#define VEU_NULL                ((void *) 0)
#endif

#define VEU_MAX_UINT32          0xffffffff

typedef enum _VEU_FORMAT{
    /* YUV series */
    VEU_FORMAT_UYVY         =   200,
    VEU_FORMAT_YUY2         =   201,
    VEU_FORMAT_YV12         =   202,
    VEU_FORMAT_NV12         =   203,
    VEU_FORMAT_I420         =   204,
    VEU_FORMAT_NV16         =   205,
    VEU_FORMAT_NV21         =   206,
    VEU_FORMAT_NV61         =   207,

    VEU_FORMAT_FORCE_UINT   =   VEU_MAX_UINT32
}VEU_FORMAT;

/*!
*********************************************************************
*   \brief
*       Data types for initialization API
*********************************************************************
*/
typedef struct _VEU_INIT_DATA{
    VEUbool     debug;          /* [IN   ]  enable debug output or not */
    VEUuint     reserved[7];
}VEU_INIT_DATA;

/*!
*********************************************************************
*   \brief
*       VEU interface definition
*********************************************************************
*/

/*
 *  VEU Initialization API
 */
VEUbool veuInitialize(VEU_INIT_DATA* pData);
VEUvoid veuTerminate();

/*
 * VEU Surface API
 */

/* Create surface in video memory */
VEUSurface veuCreateSurface(VEUuint             width,
                            VEUuint             height,
                            VEU_FORMAT          format);

/*
 * Create pre-allocated surface, the pre-allocated buffer address passed by virtualAddr and physicalAddr
 *
 * Currently we only support below combinations :
 *  1) bPreAllocVirtual and bPreAllocPhysical are both VEU_TRUE.
 *  2) bPreAllocVirtual is VEU_TRUE but bPreAllocPhysical is VEU_FALSE.
 *
 * Currently We don't support below combinations
 *  1) bPreAllocVirtua is VEU_FALSE but bPreAllocPhysical is VEU_TRUE
 *
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
                                    VEUPhysicalAddr     physicalAddr3);

/*
 * Update pre-allocated buffer address.
 * When updating pre-alloc buffer , the bPreAllocVirtual and bPreAllocPhysical flags should be same as the
 * values you create this buffer. Else, this function will directly return and set error.
 */
VEUbool veuUpdatePreAllocSurface(VEUSurface          pSurface,
                                 VEUbool             bPreAllocVirtual,
                                 VEUbool             bPreAllocPhysical,
                                 VEUVirtualAddr      virtualAddr1,
                                 VEUVirtualAddr      virtualAddr2,
                                 VEUVirtualAddr      virtualAddr3,
                                 VEUPhysicalAddr     physicalAddr1,
                                 VEUPhysicalAddr     physicalAddr2,
                                 VEUPhysicalAddr     physicalAddr3);

/* Destroy buffer, whether pre-allocated or none-preallocated */
VEUvoid veuDestroySurface(VEUSurface pSurface);

/*
 *  VEU Effect and Transition API
 */
VEUbool veuEffectProc(VEUvoid*       pFunctionContext,
                      VEUSurface     pSrc,
                      VEUSurface     pDst,
                      VEUvoid*       pProgress,
                      VEUulong       uiEffectKind);

VEUbool veuTransitionProc(VEUvoid*       userData,
                          VEUSurface     pSrc1,
                          VEUSurface     pSrc2,
                          VEUSurface     pDst,
                          VEUvoid*       pProgress,
                          VEUulong       uiTransitionKind);

VEUbool veuEffectColorProc(VEUvoid*       pFunctionContext,
                           VEUSurface     pSrcSurface,
                           VEUSurface     pDstSurface,
                           VEUvoid*       pProgress,
                           VEUulong       uiEffectKind);

VEUbool veuEffectZoomProc(VEUvoid*       pFunctionContext,
                          VEUSurface     pSrcSurface,
                          VEUSurface     pDstSurface,
                          VEUvoid*       pProgress,
                          VEUulong       uiEffectKind);

VEUbool veuSlideTransitionProc(VEUvoid*       userData,
                               VEUSurface     pSrcSurface1,
                               VEUSurface     pSrcSurface2,
                               VEUSurface     pDstSurface,
                               VEUvoid*       pProgress,
                               VEUulong       uiTransitionKind);

VEUbool veuFadeBlackProc(VEUvoid*       userData,
                         VEUSurface     pSrcSurface1,
                         VEUSurface     pSrcSurface2,
                         VEUSurface     pDstSurface,
                         VEUvoid*       pProgress,
                         VEUulong       uiTransitionKind);
/*
 * Misc API
 */

/*
 * Save surface content to bmp or yuv file, file type is decided by surface format.
 * Saved file name will be prefix_index.bmp(or .yuv), index is a global counter for the
 * times your called this function.
 */
VEUbool _veuSaveSurface(VEUSurface pSurface, const char* prefix);

/* Create surface from raw yuv file, user needs to provide format, width and height */
VEUSurface _veuLoadSurface(const char*   filename,
                           VEU_FORMAT    format,
                           VEUuint       width,
                           VEUuint       height);

/* Get current context */
VEUContext _veuGetContext();

/*check if the compiler is of C++*/
#ifdef __cplusplus
}
#endif

#endif /* __GPU_VEU_H__ */
