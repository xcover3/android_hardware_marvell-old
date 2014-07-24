/***********************************************************************************
 *
 *    Copyright (c) 2012 - 2013 by Marvell International Ltd. and its affiliates.
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

#ifndef __GCUCSC_H__
#define __GCUCSC_H__

#include "gcu.h"

typedef enum _GPU_CSC_FORMULA{          /* different formula to choose */
    GPU_CSC_FORMULA_BT601_GC    =   0x0,    /* Y'CbCr formula defined in the ITU-R BT.601 standard, integer approximated in GC, default value */
    GPU_CSC_FORMULA_BT709_GC    =   0x1,    /* Y'CbCr formula defined in the ITU-R BT.709 standard, integer approximated in GC */
    GPU_CSC_FORMULA_BT601       =   0x10,   /* Y'CbCr formula defined in the ITU-R BT.601 standard, fixed-point implementation */
    GPU_CSC_FORMULA_BT709       =   0x11,   /* Y'CbCr formula defined in the ITU-R BT.709 standard, fixed-point implementation */
    GPU_CSC_FORMULA_TRADITIONAL =   0x12,   /* traditional YUV formula used in analog equipments, fixed-point implementation */

    GPU_CSC_FORMULA_FORCE_UINT  =   0xFFFFFFFF
}GPU_CSC_FORMULA;

/*check if the compiler is of C++*/
#ifdef __cplusplus
extern "C" {
#endif

GCUbool _gcuConvertARGB2YUV(GCUContext pContext, GCUSurface pARGBSurface, GCUSurface pYUVSurface);

void gpu_csc_ARGBToI420(const unsigned char* pSrc, GCUint srcStride,
                        unsigned char* pDst[3], GCUint dstStride[3],
                        GCUint width, GCUint height);

void gpu_csc_ARGBToUYVY(const unsigned char* pSrc, GCUint srcStride,
                        unsigned char* pDst, GCUint dstStride,
                        GCUint width, GCUint height);

void gpu_csc_ARGBToNV21(const unsigned char* pSrc, GCUint srcStride,
                        unsigned char* pDst[2], GCUint dstStride[2],
                        GCUint width, GCUint height);

void gpu_csc_ARGBToNV12(const unsigned char* pSrc, GCUint srcStride,
                        unsigned char* pDst[2], GCUint dstStride[2],
                        GCUint width, GCUint height);

void gpu_csc_RGBToNV12(const unsigned char* pSrc, GCUint srcStride,
                       unsigned char* pDst[2], GCUint dstStride[2],
                       GCUint width, GCUint height);

void gpu_csc_ChooseFormula(GPU_CSC_FORMULA formula);

/*check if the compiler is of C++*/
#ifdef __cplusplus
}
#endif

#endif /* __GCUCSC_H__ */
