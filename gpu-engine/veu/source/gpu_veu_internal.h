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
*  \file gpu_veu_internal.h
*  \brief
*      Decalration of VEU internal interface
******************************************************************************/

#ifndef __GPU_VEU_INTERNAL_H__
#define __GPU_VEU_INTERNAL_H__


/*check if the compiler is of C++*/
#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include "gpu_veu.h"

#if defined(DEBUG) || defined(_DEBUG)
#define VEU_DEBUG 1
#else
#define VEU_DEBUG 0
#endif

#if VEU_DEBUG
#define VEU_ASSERT(exp) if(!(exp))                                                                                          \
                        {                                                                                                   \
                            _veuDebugf("VEU_ASSERT(%s) failed on %s, %d\n", #exp, __FUNCTION__, __LINE__);   \
                            abort();                                                                                        \
                        }

#else
#define VEU_ASSERT(exp)
#endif

#define VEU_CHECK_ALIGNE(n, align) (((n) & ((align) - 1)) == 0)


/** Check for value is EVEN */
#ifndef VEU_IS_EVEN
#define VEU_IS_EVEN(a)    (!((a) & 0x01))
#endif

#define IS_ALIGNED(n, align) (((n) & ((align) - 1)) == 0)

/**
 *****************************************************************************
 *                    Macros used for color transform RGB565 to YUV
 *****************************************************************************
*/
#define _veuY16(r, g, b) _veuCLIP((((80593 * r) + (77855 * g) + (30728 * b)) >> 15))
#define _veuU16(r, g, b) _veuCLIP(128 + (( -(45483 * r) - (43936 * g) + (134771 * b)) >> 15))
#define _veuV16(r, g, b) _veuCLIP(128 + (((134771 * r) - (55532 * g) - (21917 * b)) >> 15))

/** Clip function ensures values with range of 0 and 255 */
#define _veuCLIP(x)  ((x) < 0 ? 0 : ((x) > 255 ? 255 : (x)))

/* Flush or Finish commands */
VEUvoid _veuFlush(VEUContext pContext);
VEUvoid _veuFinish(VEUContext pContext);

VEUSurface _veuGetGradient();

/* print log */
void _veuDebugf(const char format[], ...);
/*check if the compiler is of C++*/
#ifdef __cplusplus
}
#endif

#endif /* __GPU_VEU_INTERNAL_H__ */
