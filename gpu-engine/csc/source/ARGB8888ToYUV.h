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

#ifndef __ARGB8888TOYUV_H__
#define __ARGB8888TOYUV_H__
typedef unsigned char   Ipp8u;
typedef unsigned short  Ipp16u;
typedef signed int      Ipp32s;
typedef signed short    Ipp16s;
typedef unsigned int    Ipp32u;

/*
// Useful Definitions & Macros
*/
#define Q16          (16)     /* use 16-bit scaling */
#define Q15          (15)     /* use 15-bit scaling */
#define Q14          (14)     /* use 14-bit scaling */
#define Q10          (10)     /* use 10-bit scaling */

#define FSP_Q(x,QBITS) ((x)*(1<<(QBITS))+0.5f)
#define FSPQ14(x) FSP_Q((x),Q14)
#define FSPQ16(x) FSP_Q((x),Q16)
#define FSPQ15(x) FSP_Q((x),Q15)
#define FSPQ10(x) FSP_Q((x),Q10)

#include "gpu_csc.h"

#endif /* __ARGB8888TOYUV_H__ */
