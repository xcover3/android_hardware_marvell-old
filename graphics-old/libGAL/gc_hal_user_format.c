/****************************************************************************
*
*    Copyright (c) 2005 - 2015 by Vivante Corp.  All rights reserved.
*
*    The material in this file is confidential and contains trade secrets
*    of Vivante Corporation. This is proprietary information owned by
*    Vivante Corporation. No part of this work may be disclosed,
*    reproduced, copied, transmitted, or used in any way for any purpose,
*    without the express written permission of Vivante Corporation.
*
*****************************************************************************/


/**
**  @file
**  Architecture independent format conversion.
**
*/

#include "gc_hal_user_precomp.h"
#if gcdENABLE_3D


#define gcdGET_FIELD(data, start, bits) (((data) >> (start)) & (~(~0 << bits)))
#define gcdUNORM_MAX(n)                 ((gctFLOAT)(((gctUINT64)1 << (n)) - 1))

#define gcdUNORM_TO_FLOAT(ub, n)        (gctFLOAT)((ub) / gcdUNORM_MAX(n))
#define gcdFLOAT_TO_UNORM(x, n)         (gctUINT32)(gcmCLAMP((x), 0.0f, 1.0f) * gcdUNORM_MAX(n) + 0.5f)

#define gcdSNORM_TO_FLOAT(b, n)         (gctFLOAT)(gcmMAX(((b) / gcdUNORM_MAX((n) - 1)), -1.0f))
#define gcdOFFSET_O_DOT_5(x)            (((x) < 0) ? ((x) - 0.5f) : ((x) + 0.5))
#define gcdFLOAT_TO_SNORM(x, n)         (gctINT32)(gcdOFFSET_O_DOT_5(gcmCLAMP((x), -1.0f, 1.0f) * gcdUNORM_MAX((n) - 1)))

#define gcdINT_MIN(n)                   (-(gctINT)((gctINT64)1 << ((n) - 1)))
#define gcdINT_MAX(n)                   ((gctINT)(((gctINT64)1 << ((n) - 1)) - 1))
#define gcdUINT_MAX(n)                  ((gctUINT)(((gctUINT64)1 << (n)) - 1))



void _ReadPixelFrom_A8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT8 ub = *(gctUINT8*)inAddr[0];

    outPixel->pf.r =
    outPixel->pf.g =
    outPixel->pf.b = 0.0f;
    outPixel->pf.a = gcdUNORM_TO_FLOAT(ub, 8);
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_A16(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16 us = *(gctUINT16*)inAddr[0];

    outPixel->pf.r =
    outPixel->pf.g =
    outPixel->pf.b = 0.0f;
    outPixel->pf.a = gcdUNORM_TO_FLOAT(us, 16);
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_A16F(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16 sf = *(gctUINT16*)inAddr[0];
    gctUINT32 f32 = gcoMATH_Float16ToFloat(sf);

    outPixel->pf.r =
    outPixel->pf.g =
    outPixel->pf.b = 0.0f;
    outPixel->pf.a = *(gctFLOAT *)&f32;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}


void _ReadPixelFrom_L8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT8 ub = *(gctUINT8*)inAddr[0];

    outPixel->pf.r =
    outPixel->pf.g =
    outPixel->pf.b = gcdUNORM_TO_FLOAT(ub, 8);
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_L16(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16 us = *(gctUINT16*)inAddr[0];

    outPixel->pf.r =
    outPixel->pf.g =
    outPixel->pf.b = gcdUNORM_TO_FLOAT(us, 16);
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_L16F(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16 sf = *(gctUINT16*)inAddr[0];
    gctUINT32 f32 = gcoMATH_Float16ToFloat(sf);

    outPixel->pf.r =
    outPixel->pf.g =
    outPixel->pf.b = *(gctFLOAT *)&f32;
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_A8L8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT8* pUB = (gctUINT8*)inAddr[0];

    outPixel->pf.r =
    outPixel->pf.g =
    outPixel->pf.b = gcdUNORM_TO_FLOAT(pUB[0], 8);
    outPixel->pf.a = gcdUNORM_TO_FLOAT(pUB[1], 8);
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_A16L16(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16* pUS = (gctUINT16*)inAddr[0];

    outPixel->pf.r =
    outPixel->pf.g =
    outPixel->pf.b = gcdUNORM_TO_FLOAT(pUS[0], 16);
    outPixel->pf.a = gcdUNORM_TO_FLOAT(pUS[1], 16);
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_A16L16F(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16* pSF = (gctUINT16*)inAddr[0];
    gctUINT32 f32[2];

    f32[0] = gcoMATH_Float16ToFloat(pSF[0]);
    f32[1] = gcoMATH_Float16ToFloat(pSF[1]);
    outPixel->pf.r =
    outPixel->pf.g =
    outPixel->pf.b = *(gctFLOAT *)&f32[0];
    outPixel->pf.a = *(gctFLOAT *)&f32[1];
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_R8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT8 ub = *(gctUINT8*)inAddr[0];

    outPixel->pf.r = gcdUNORM_TO_FLOAT(ub, 8);
    outPixel->pf.g = 0.0f;
    outPixel->pf.b = 0.0f;
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_R8_1_X8R8G8B8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT8 *pUB = (gctUINT8*)inAddr[0];

    outPixel->pf.r = gcdUNORM_TO_FLOAT(pUB[2], 8);
    outPixel->pf.g = 0.0f;
    outPixel->pf.b = 0.0f;
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_R16(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16 us = *(gctUINT16*)inAddr[0];

    outPixel->pf.r = gcdUNORM_TO_FLOAT(us, 16);
    outPixel->pf.g = 0.0f;
    outPixel->pf.b = 0.0f;
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_R16F(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16 sf = *(gctUINT16*)inAddr[0];
    gctUINT32 f32 = gcoMATH_Float16ToFloat(sf);

    outPixel->pf.r = *(gctFLOAT*)&f32;
    outPixel->pf.g = 0.0f;
    outPixel->pf.b = 0.0f;
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}


void _ReadPixelFrom_R32F(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctFLOAT f = *(gctFLOAT*)inAddr[0];

    outPixel->pf.r = f;
    outPixel->pf.g = 0.0f;
    outPixel->pf.b = 0.0f;
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}


void _ReadPixelFrom_G8R8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT8* pUB = (gctUINT8*)inAddr[0];

    outPixel->pf.r = gcdUNORM_TO_FLOAT(pUB[0], 8);
    outPixel->pf.g = gcdUNORM_TO_FLOAT(pUB[1], 8);
    outPixel->pf.b = 0.0f;
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_G8R8_1_X8R8G8B8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT8 *pUB = (gctUINT8*)inAddr[0];

    outPixel->pf.r = gcdUNORM_TO_FLOAT(pUB[2], 8);
    outPixel->pf.g = gcdUNORM_TO_FLOAT(pUB[1], 8);
    outPixel->pf.b = 0.0f;
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_G16R16(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16* pUS = (gctUINT16*)inAddr[0];

    outPixel->pf.r = gcdUNORM_TO_FLOAT(pUS[0], 16);
    outPixel->pf.g = gcdUNORM_TO_FLOAT(pUS[1], 16);
    outPixel->pf.b = 0.0f;
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_G16R16F(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16* pSF = (gctUINT16*)inAddr[0];
    gctUINT32 f32[2];

    f32[0] = gcoMATH_Float16ToFloat(pSF[0]);
    f32[1] = gcoMATH_Float16ToFloat(pSF[1]);
    outPixel->pf.r = *(gctFLOAT *)&f32[0];
    outPixel->pf.g = *(gctFLOAT *)&f32[1];
    outPixel->pf.b = 0.0f;
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}


void _ReadPixelFrom_G32R32F(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctFLOAT* pF = (gctFLOAT*)inAddr[0];

    outPixel->pf.r = pF[0];
    outPixel->pf.g = pF[1];
    outPixel->pf.b = 0.0f;
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_G32R32F_2_A8R8G8B8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctFLOAT* pF0 = (gctFLOAT*)inAddr[0];
    gctFLOAT* pF1 = (gctFLOAT*)inAddr[1];

    outPixel->pf.r = pF0[0];
    outPixel->pf.g = pF1[0];
    outPixel->pf.b = 0.0f;
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}


void _ReadPixelFrom_A4R4G4B4(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16 us = *(gctUINT16*)inAddr[0];

    outPixel->pf.r = gcdUNORM_TO_FLOAT(gcdGET_FIELD(us,  8, 4), 4);
    outPixel->pf.g = gcdUNORM_TO_FLOAT(gcdGET_FIELD(us,  4, 4), 4);
    outPixel->pf.b = gcdUNORM_TO_FLOAT(gcdGET_FIELD(us,  0, 4), 4);
    outPixel->pf.a = gcdUNORM_TO_FLOAT(gcdGET_FIELD(us, 12, 4), 4);
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}


void _ReadPixelFrom_A1R5G5B5(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16 us = *(gctUINT16*)inAddr[0];

    outPixel->pf.r = gcdUNORM_TO_FLOAT(gcdGET_FIELD(us, 10, 5), 5);
    outPixel->pf.g = gcdUNORM_TO_FLOAT(gcdGET_FIELD(us,  5, 5), 5);
    outPixel->pf.b = gcdUNORM_TO_FLOAT(gcdGET_FIELD(us,  0, 5), 5);
    outPixel->pf.a = gcdUNORM_TO_FLOAT(gcdGET_FIELD(us, 15, 1), 1);
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}



void _ReadPixelFrom_R5G6B5(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16 us = *(gctUINT16*)inAddr[0];

    outPixel->pf.r = gcdUNORM_TO_FLOAT(gcdGET_FIELD(us, 11, 5), 5);
    outPixel->pf.g = gcdUNORM_TO_FLOAT(gcdGET_FIELD(us,  5, 6), 6);
    outPixel->pf.b = gcdUNORM_TO_FLOAT(gcdGET_FIELD(us,  0, 5), 5);
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_B8G8R8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT8 *pUB = (gctUINT8*)inAddr[0];

    outPixel->pf.r = gcdUNORM_TO_FLOAT(pUB[0], 8);
    outPixel->pf.g = gcdUNORM_TO_FLOAT(pUB[1], 8);
    outPixel->pf.b = gcdUNORM_TO_FLOAT(pUB[2], 8);
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_B16G16R16(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16 *pUS = (gctUINT16*)inAddr[0];

    outPixel->pf.r = gcdUNORM_TO_FLOAT(pUS[0], 16);
    outPixel->pf.g = gcdUNORM_TO_FLOAT(pUS[1], 16);
    outPixel->pf.b = gcdUNORM_TO_FLOAT(pUS[2], 16);
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_X4R4G4B4(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16 us = *(gctUINT16*)inAddr[0];

    outPixel->pf.r = gcdUNORM_TO_FLOAT(gcdGET_FIELD(us, 8, 4), 4);
    outPixel->pf.g = gcdUNORM_TO_FLOAT(gcdGET_FIELD(us, 4, 4), 4);
    outPixel->pf.b = gcdUNORM_TO_FLOAT(gcdGET_FIELD(us, 0, 4), 4);
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_R4G4B4A4(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16 us = *(gctUINT16*)inAddr[0];

    outPixel->pf.r = gcdUNORM_TO_FLOAT(gcdGET_FIELD(us, 12, 4), 4);
    outPixel->pf.g = gcdUNORM_TO_FLOAT(gcdGET_FIELD(us,  8, 4), 4);
    outPixel->pf.b = gcdUNORM_TO_FLOAT(gcdGET_FIELD(us,  4, 4), 4);
    outPixel->pf.a = gcdUNORM_TO_FLOAT(gcdGET_FIELD(us,  0, 4), 4);
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_X1R5G5B5(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16 us = *(gctUINT16*)inAddr[0];

    outPixel->pf.r = gcdUNORM_TO_FLOAT(gcdGET_FIELD(us, 10, 5), 5);
    outPixel->pf.g = gcdUNORM_TO_FLOAT(gcdGET_FIELD(us,  5, 5), 5);
    outPixel->pf.b = gcdUNORM_TO_FLOAT(gcdGET_FIELD(us,  0, 5), 5);
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_R5G5B5A1(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16 us = *(gctUINT16*)inAddr[0];

    outPixel->pf.r = gcdUNORM_TO_FLOAT(gcdGET_FIELD(us, 11, 5), 5);
    outPixel->pf.g = gcdUNORM_TO_FLOAT(gcdGET_FIELD(us,  6, 5), 5);
    outPixel->pf.b = gcdUNORM_TO_FLOAT(gcdGET_FIELD(us,  1, 5), 5);
    outPixel->pf.a = gcdUNORM_TO_FLOAT(gcdGET_FIELD(us,  0, 1), 1);
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_A8B8G8R8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT8 *pUB = (gctUINT8*)inAddr[0];

    outPixel->pf.r = gcdUNORM_TO_FLOAT(pUB[0], 8);
    outPixel->pf.g = gcdUNORM_TO_FLOAT(pUB[1], 8);
    outPixel->pf.b = gcdUNORM_TO_FLOAT(pUB[2], 8);
    outPixel->pf.a = gcdUNORM_TO_FLOAT(pUB[3], 8);
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}


void _ReadPixelFrom_A8R8G8B8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT8 *pUB = (gctUINT8*)inAddr[0];

    outPixel->pf.r = gcdUNORM_TO_FLOAT(pUB[2], 8);
    outPixel->pf.g = gcdUNORM_TO_FLOAT(pUB[1], 8);
    outPixel->pf.b = gcdUNORM_TO_FLOAT(pUB[0], 8);
    outPixel->pf.a = gcdUNORM_TO_FLOAT(pUB[3], 8);
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_X8R8G8B8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT8 *pUB = (gctUINT8*)inAddr[0];

    outPixel->pf.r = gcdUNORM_TO_FLOAT(pUB[2], 8);
    outPixel->pf.g = gcdUNORM_TO_FLOAT(pUB[1], 8);
    outPixel->pf.b = gcdUNORM_TO_FLOAT(pUB[0], 8);
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_X2B10G10R10(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT32 ui = *(gctUINT32*)inAddr[0];

    outPixel->pf.r = gcdUNORM_TO_FLOAT(gcdGET_FIELD(ui,  0, 10), 10);
    outPixel->pf.g = gcdUNORM_TO_FLOAT(gcdGET_FIELD(ui, 10, 10), 10);
    outPixel->pf.b = gcdUNORM_TO_FLOAT(gcdGET_FIELD(ui, 20, 10), 10);
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_A2B10G10R10(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT32 ui = *(gctUINT32*)inAddr[0];

    outPixel->pf.r = gcdUNORM_TO_FLOAT(gcdGET_FIELD(ui,  0, 10), 10);
    outPixel->pf.g = gcdUNORM_TO_FLOAT(gcdGET_FIELD(ui, 10, 10), 10);
    outPixel->pf.b = gcdUNORM_TO_FLOAT(gcdGET_FIELD(ui, 20, 10), 10);
    outPixel->pf.a = gcdUNORM_TO_FLOAT(gcdGET_FIELD(ui, 30,  2),  2);
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_A8B12G12R12_2_A8R8G8B8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT32 ui0 = *(gctUINT32*)inAddr[0];
    gctUINT32 ui1 = *(gctUINT32*)inAddr[1];

    outPixel->pf.b = gcdUNORM_TO_FLOAT((gcdGET_FIELD(ui0,  0,  8) << 4) + gcdGET_FIELD(ui1,  0,  8), 12);
    outPixel->pf.g = gcdUNORM_TO_FLOAT((gcdGET_FIELD(ui0,  8,  8) << 4) + gcdGET_FIELD(ui1,  8,  8), 12);
    outPixel->pf.r = gcdUNORM_TO_FLOAT((gcdGET_FIELD(ui0,  16, 8) << 4) + gcdGET_FIELD(ui1,  16, 8), 12);
    outPixel->pf.a = gcdUNORM_TO_FLOAT( gcdGET_FIELD(ui0,  24, 8),  8);
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_A16B16G16R16(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16 *pUS = (gctUINT16*)inAddr[0];

    outPixel->pf.r = gcdUNORM_TO_FLOAT(pUS[0], 16);
    outPixel->pf.g = gcdUNORM_TO_FLOAT(pUS[1], 16);
    outPixel->pf.b = gcdUNORM_TO_FLOAT(pUS[2], 16);
    outPixel->pf.a = gcdUNORM_TO_FLOAT(pUS[3], 16);
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_X16B16G16R16F(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16 *pSF = (gctUINT16*)inAddr[0];
    gctUINT32 f32[3];

    f32[0] = gcoMATH_Float16ToFloat(pSF[0]);
    f32[1] = gcoMATH_Float16ToFloat(pSF[1]);
    f32[2] = gcoMATH_Float16ToFloat(pSF[2]);

    outPixel->pf.r = *(gctFLOAT *)&f32[0];
    outPixel->pf.g = *(gctFLOAT *)&f32[1];
    outPixel->pf.b = *(gctFLOAT *)&f32[2];
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_A16B16G16R16F(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16 *pSF = (gctUINT16*)inAddr[0];
    gctUINT32 f32[4];

    f32[0] = gcoMATH_Float16ToFloat(pSF[0]);
    f32[1] = gcoMATH_Float16ToFloat(pSF[1]);
    f32[2] = gcoMATH_Float16ToFloat(pSF[2]);
    f32[3] = gcoMATH_Float16ToFloat(pSF[3]);

    outPixel->pf.r = *(gctFLOAT *)&f32[0];
    outPixel->pf.g = *(gctFLOAT *)&f32[1];
    outPixel->pf.b = *(gctFLOAT *)&f32[2];
    outPixel->pf.a = *(gctFLOAT *)&f32[3];
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_A16B16G16R16F_2_A8R8G8B8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16 *pSF0 = (gctUINT16*)inAddr[0];
    gctUINT16 *pSF1 = (gctUINT16*)inAddr[1];
    gctUINT32 f32[4];

    f32[0] = gcoMATH_Float16ToFloat(pSF0[0]);
    f32[1] = gcoMATH_Float16ToFloat(pSF0[1]);
    f32[2] = gcoMATH_Float16ToFloat(pSF1[0]);
    f32[3] = gcoMATH_Float16ToFloat(pSF1[1]);

    outPixel->pf.r = *(gctFLOAT *)&f32[0];
    outPixel->pf.g = *(gctFLOAT *)&f32[1];
    outPixel->pf.b = *(gctFLOAT *)&f32[2];
    outPixel->pf.a = *(gctFLOAT *)&f32[3];
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_D16(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16 us = *(gctUINT16*)inAddr[0];

    outPixel->pf.r =
    outPixel->pf.g =
    outPixel->pf.b = 0.0f;
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = gcdUNORM_TO_FLOAT(us, 16);
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_D24X8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT32 ui = *(gctUINT32*)inAddr[0];

    outPixel->pf.r =
    outPixel->pf.g =
    outPixel->pf.b = 0.0f;
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = gcdUNORM_TO_FLOAT(gcdGET_FIELD(ui,  8, 24), 24);
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_D24S8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT32 ui = *(gctUINT32*)inAddr[0];

    outPixel->pf.r =
    outPixel->pf.g =
    outPixel->pf.b = 0.0f;
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = gcdUNORM_TO_FLOAT(gcdGET_FIELD(ui,  8, 24), 24);
    outPixel->pf.s = gcdUNORM_TO_FLOAT(gcdGET_FIELD(ui,  0,  8),  8);
}

void _ReadPixelFrom_S8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT8 ui = *(gctUINT8*)inAddr[0];

    outPixel->pui.r = 0;
    outPixel->pui.g = 0;
    outPixel->pui.b = 0;
    outPixel->pui.a = 1;
    outPixel->pui.d = 0;
    outPixel->pui.s = ui;

}

void _ReadPixelFrom_D32(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT32 ui = *(gctUINT32*)inAddr[0];

    outPixel->pf.r =
    outPixel->pf.g =
    outPixel->pf.b = 0.0f;
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = gcdUNORM_TO_FLOAT(ui, 32);
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_D32F(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctFLOAT f = *(gctFLOAT*)inAddr[0];

    outPixel->pf.r =
    outPixel->pf.g =
    outPixel->pf.b = 0.0f;
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_S8D32F(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctFLOAT f = *(gctFLOAT*)inAddr[0];
    gctUINT32 ui = *((gctINT32 *)inAddr[0] + 1);

    outPixel->pf.r =
    outPixel->pf.g =
    outPixel->pf.b = 0.0f;
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = f;
    outPixel->pf.s = gcdUNORM_TO_FLOAT(ui, 8);
}


/* SNORM format. */
void _ReadPixelFrom_R8_SNORM(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctINT8 b = *(gctINT8*)inAddr[0];

    outPixel->pf.r = gcdSNORM_TO_FLOAT(b, 8);
    outPixel->pf.g = 0.0f;
    outPixel->pf.b = 0.0f;
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_G8R8_SNORM(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctINT8* pB = (gctINT8*)inAddr[0];

    outPixel->pf.r = gcdSNORM_TO_FLOAT(pB[0], 8);
    outPixel->pf.g = gcdSNORM_TO_FLOAT(pB[1], 8);
    outPixel->pf.b = 0.0f;
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_B8G8R8_SNORM(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctINT8 *pB = (gctINT8*)inAddr[0];

    outPixel->pf.r = gcdSNORM_TO_FLOAT(pB[0], 8);
    outPixel->pf.g = gcdSNORM_TO_FLOAT(pB[1], 8);
    outPixel->pf.b = gcdSNORM_TO_FLOAT(pB[2], 8);
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_A8B8G8R8_SNORM(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctINT8 *pB = (gctINT8*)inAddr[0];

    outPixel->pf.r = gcdSNORM_TO_FLOAT(pB[0], 8);
    outPixel->pf.g = gcdSNORM_TO_FLOAT(pB[1], 8);
    outPixel->pf.b = gcdSNORM_TO_FLOAT(pB[2], 8);
    outPixel->pf.a = gcdSNORM_TO_FLOAT(pB[3], 8);
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_X8B8G8R8_SNORM(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctINT8 *pB = (gctINT8*)inAddr[0];

    outPixel->pf.r = gcdSNORM_TO_FLOAT(pB[0], 8);
    outPixel->pf.g = gcdSNORM_TO_FLOAT(pB[1], 8);
    outPixel->pf.b = gcdSNORM_TO_FLOAT(pB[2], 8);
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

/* Integer format. */
void _ReadPixelFrom_R8_I(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctINT8 i = *(gctINT8*)inAddr[0];

    outPixel->pi.r = i;
    outPixel->pi.g = 0;
    outPixel->pi.b = 0;
    outPixel->pi.a = 1;
    outPixel->pi.d = 1;
    outPixel->pi.s = 0;
}

void _ReadPixelFrom_R8_UI(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT8 ui = *(gctUINT8*)inAddr[0];

    outPixel->pui.r = ui;
    outPixel->pui.g = 0;
    outPixel->pui.b = 0;
    outPixel->pui.a = 1;
    outPixel->pui.d = 1;
    outPixel->pui.s = 0;
}

void _ReadPixelFrom_R16_I(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctINT16 i = *(gctINT16*)inAddr[0];

    outPixel->pi.r = i;
    outPixel->pi.g = 0;
    outPixel->pi.b = 0;
    outPixel->pi.a = 1;
    outPixel->pi.d = 1;
    outPixel->pi.s = 0;
}

void _ReadPixelFrom_R16_UI(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16 i = *(gctUINT16*)inAddr[0];

    outPixel->pui.r = i;
    outPixel->pui.g = 0;
    outPixel->pui.b = 0;
    outPixel->pui.a = 1;
    outPixel->pui.d = 1;
    outPixel->pui.s = 0;
}

void _ReadPixelFrom_R32_I(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctINT32 i = *(gctINT32*)inAddr[0];

    outPixel->pi.r = i;
    outPixel->pi.g = 0;
    outPixel->pi.b = 0;
    outPixel->pi.a = 1;
    outPixel->pi.d = 1;
    outPixel->pi.s = 0;
}

void _ReadPixelFrom_R32_UI(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT32 i = *(gctUINT32*)inAddr[0];

    outPixel->pui.r = i;
    outPixel->pui.g = 0;
    outPixel->pui.b = 0;
    outPixel->pui.a = 1;
    outPixel->pui.d = 1;
    outPixel->pui.s = 0;
}

void _ReadPixelFrom_G8R8_I(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctINT8* pI = (gctINT8*)inAddr[0];

    outPixel->pi.r = pI[0];
    outPixel->pi.g = pI[1];
    outPixel->pi.b = 0;
    outPixel->pi.a = 1;
    outPixel->pi.d = 1;
    outPixel->pi.s = 0;
}

void _ReadPixelFrom_G8R8_UI(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT8* pI = (gctUINT8*)inAddr[0];

    outPixel->pui.r = pI[0];
    outPixel->pui.g = pI[1];
    outPixel->pui.b = 0;
    outPixel->pui.a = 1;
    outPixel->pui.d = 1;
    outPixel->pui.s = 0;
}

void _ReadPixelFrom_G16R16_I(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctINT16* pI = (gctINT16*)inAddr[0];

    outPixel->pi.r = pI[0];
    outPixel->pi.g = pI[1];
    outPixel->pi.b = 0;
    outPixel->pi.a = 1;
    outPixel->pi.d = 1;
    outPixel->pi.s = 0;
}

void _ReadPixelFrom_G16R16_UI(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16* pI = (gctUINT16*)inAddr[0];

    outPixel->pui.r = pI[0];
    outPixel->pui.g = pI[1];
    outPixel->pui.b = 0;
    outPixel->pui.a = 1;
    outPixel->pui.d = 1;
    outPixel->pui.s = 0;
}

void _ReadPixelFrom_G32R32_I(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctINT32* pI = (gctINT32*)inAddr[0];

    outPixel->pi.r = pI[0];
    outPixel->pi.g = pI[1];
    outPixel->pi.b = 0;
    outPixel->pi.a = 1;
    outPixel->pi.d = 1;
    outPixel->pi.s = 0;
}

void _ReadPixelFrom_G32R32_I_2_A8R8G8B8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctINT32* pI0 = (gctINT32*)inAddr[0];
    gctINT32* pI1 = (gctINT32*)inAddr[1];


    outPixel->pi.r = pI0[0];
    outPixel->pi.g = pI1[0];
    outPixel->pi.b = 0;
    outPixel->pi.a = 1;
    outPixel->pi.d = 1;
    outPixel->pi.s = 0;
}


void _ReadPixelFrom_G32R32_UI(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT32* pI = (gctUINT32*)inAddr[0];

    outPixel->pui.r = pI[0];
    outPixel->pui.g = pI[1];
    outPixel->pui.b = 0;
    outPixel->pui.a = 1;
    outPixel->pui.d = 1;
    outPixel->pui.s = 0;
}

void _ReadPixelFrom_G32R32_UI_2_A8R8G8B8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT32* pI0 = (gctUINT32*)inAddr[0];
    gctUINT32* pI1 = (gctUINT32*)inAddr[1];

    outPixel->pui.r = pI0[0];
    outPixel->pui.g = pI1[0];
    outPixel->pui.b = 0;
    outPixel->pui.a = 1;
    outPixel->pui.d = 1;
    outPixel->pui.s = 0;
}




void _ReadPixelFrom_B8G8R8_I(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctINT8* pI = (gctINT8*)inAddr[0];

    outPixel->pi.r = pI[0];
    outPixel->pi.g = pI[1];
    outPixel->pi.b = pI[2];
    outPixel->pi.a = 1;
    outPixel->pi.d = 1;
    outPixel->pi.s = 0;
}

void _ReadPixelFrom_B8G8R8_UI(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT8* pI = (gctUINT8*)inAddr[0];

    outPixel->pui.r = pI[0];
    outPixel->pui.g = pI[1];
    outPixel->pui.b = pI[2];
    outPixel->pui.a = 1;
    outPixel->pui.d = 1;
    outPixel->pui.s = 0;
}

void _ReadPixelFrom_B16G16R16_I(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctINT16* pI = (gctINT16*)inAddr[0];

    outPixel->pi.r = pI[0];
    outPixel->pi.g = pI[1];
    outPixel->pi.b = pI[2];
    outPixel->pi.a = 1;
    outPixel->pi.d = 1;
    outPixel->pi.s = 0;
}

void _ReadPixelFrom_B16G16R16I_2_A8R8G8B8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctINT16* pI0 = (gctINT16*)inAddr[0];
    gctINT16* pI1 = (gctINT16*)inAddr[1];

    outPixel->pi.r = pI0[0];
    outPixel->pi.g = pI0[1];
    outPixel->pi.b = pI1[0];
    outPixel->pi.a = 1;
    outPixel->pi.d = 1;
    outPixel->pi.s = 0;
}





void _ReadPixelFrom_B16G16R16_UI(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16* pI = (gctUINT16*)inAddr[0];

    outPixel->pui.r = pI[0];
    outPixel->pui.g = pI[1];
    outPixel->pui.b = pI[2];
    outPixel->pui.a = 1;
    outPixel->pui.d = 1;
    outPixel->pui.s = 0;
}


void _ReadPixelFrom_B16G16R16UI_2_A8R8G8B8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16* pI0 = (gctUINT16*)inAddr[0];
    gctUINT16* pI1 = (gctUINT16*)inAddr[1];

    outPixel->pui.r = pI0[0];
    outPixel->pui.g = pI0[1];
    outPixel->pui.b = pI1[0];
    outPixel->pui.a = 1;
    outPixel->pui.d = 1;
    outPixel->pui.s = 0;
}


void _ReadPixelFrom_B16G16R16F(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16* pI = (gctUINT16*)inAddr[0];
    gctUINT32 f32[3];

    f32[0] = gcoMATH_Float16ToFloat(pI[0]);
    f32[1] = gcoMATH_Float16ToFloat(pI[1]);
    f32[2] = gcoMATH_Float16ToFloat(pI[2]);

    outPixel->pf.r = *(gctFLOAT *)&f32[0];
    outPixel->pf.g = *(gctFLOAT *)&f32[1];
    outPixel->pf.b = *(gctFLOAT *)&f32[2];
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_B16G16R16F_2_A8R8G8B8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16* pI0 = (gctUINT16*)inAddr[0];
    gctUINT16* pI1 = (gctUINT16*)inAddr[1];

    gctUINT32 f32[3];

    f32[0] = gcoMATH_Float16ToFloat(pI0[0]);
    f32[1] = gcoMATH_Float16ToFloat(pI0[1]);
    f32[2] = gcoMATH_Float16ToFloat(pI1[0]);

    outPixel->pf.r = *(gctFLOAT *)&f32[0];
    outPixel->pf.g = *(gctFLOAT *)&f32[1];
    outPixel->pf.b = *(gctFLOAT *)&f32[2];
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_B32G32R32_I(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctINT32* pI = (gctINT32*)inAddr[0];

    outPixel->pi.r = pI[0];
    outPixel->pi.g = pI[1];
    outPixel->pi.b = pI[2];
    outPixel->pi.a = 1;
    outPixel->pi.d = 1;
    outPixel->pi.s = 0;
}

void _ReadPixelFrom_B32G32R32_UI(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT32* pI = (gctUINT32*)inAddr[0];

    outPixel->pui.r = pI[0];
    outPixel->pui.g = pI[1];
    outPixel->pui.b = pI[2];
    outPixel->pui.a = 1;
    outPixel->pui.d = 1;
    outPixel->pui.s = 0;
}

void _ReadPixelFrom_B32G32R32_I_3_A8R8G8B8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctINT32* pI0 = (gctINT32*)inAddr[0];
    gctINT32* pI1 = (gctINT32*)inAddr[1];
    gctINT32* pI2 = (gctINT32*)inAddr[2];

    outPixel->pi.r = pI0[0];
    outPixel->pi.g = pI1[0];
    outPixel->pi.b = pI2[0];
    outPixel->pi.a = 1;
    outPixel->pi.d = 1;
    outPixel->pi.s = 0;
}

void _ReadPixelFrom_B32G32R32_UI_3_A8R8G8B8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT32* pI0 = (gctUINT32*)inAddr[0];
    gctUINT32* pI1 = (gctUINT32*)inAddr[1];
    gctUINT32* pI2 = (gctUINT32*)inAddr[2];

    outPixel->pui.r = pI0[0];
    outPixel->pui.g = pI1[0];
    outPixel->pui.b = pI2[0];
    outPixel->pui.a = 1;
    outPixel->pui.d = 1;
    outPixel->pui.s = 0;
}


void _ReadPixelFrom_B32G32R32F(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctFLOAT* pf = (gctFLOAT*)inAddr[0];

    outPixel->pf.r = pf[0];
    outPixel->pf.g = pf[1];
    outPixel->pf.b = pf[2];
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}


void _ReadPixelFrom_B32G32R32F_3_A8R8G8B8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctFLOAT* pf0 = (gctFLOAT*)inAddr[0];
    gctFLOAT* pf1 = (gctFLOAT*)inAddr[1];
    gctFLOAT* pf2 = (gctFLOAT*)inAddr[2];

    outPixel->pf.r = pf0[0];
    outPixel->pf.g = pf1[0];
    outPixel->pf.b = pf2[0];
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}


void _ReadPixelFrom_A8B8G8R8_I(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctINT8* pI = (gctINT8*)inAddr[0];

    outPixel->pi.r = pI[0];
    outPixel->pi.g = pI[1];
    outPixel->pi.b = pI[2];
    outPixel->pi.a = pI[3];
    outPixel->pi.d = 1;
    outPixel->pi.s = 0;
}

void _ReadPixelFrom_A8B8G8R8_UI(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT8* pI = (gctUINT8*)inAddr[0];

    outPixel->pui.r = pI[0];
    outPixel->pui.g = pI[1];
    outPixel->pui.b = pI[2];
    outPixel->pui.a = pI[3];
    outPixel->pui.d = 1;
    outPixel->pui.s = 0;
}

void _ReadPixelFrom_A16B16G16R16_I(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctINT16* pI = (gctINT16*)inAddr[0];

    outPixel->pi.r = pI[0];
    outPixel->pi.g = pI[1];
    outPixel->pi.b = pI[2];
    outPixel->pi.a = pI[3];
    outPixel->pi.d = 1;
    outPixel->pi.s = 0;
}

void _ReadPixelFrom_A16B16G16R16_I_2_A8R8G8B8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctINT16* pI0 = (gctINT16*)inAddr[0];
    gctINT16* pI1 = (gctINT16*)inAddr[1];

    outPixel->pi.r = pI0[0];
    outPixel->pi.g = pI0[1];
    outPixel->pi.b = pI1[0];
    outPixel->pi.a = pI1[1];
    outPixel->pi.d = 1;
    outPixel->pi.s = 0;
}


void _ReadPixelFrom_A16B16G16R16_UI(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16* pI = (gctUINT16*)inAddr[0];

    outPixel->pui.r = pI[0];
    outPixel->pui.g = pI[1];
    outPixel->pui.b = pI[2];
    outPixel->pui.a = pI[3];
    outPixel->pui.d = 1;
    outPixel->pui.s = 0;
}

void _ReadPixelFrom_A16B16G16R16_UI_2_A8R8G8B8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16* pI0 = (gctUINT16*)inAddr[0];
    gctUINT16* pI1 = (gctUINT16*)inAddr[1];

    outPixel->pui.r = pI0[0];
    outPixel->pui.g = pI0[1];
    outPixel->pui.b = pI1[0];
    outPixel->pui.a = pI1[1];
    outPixel->pui.d = 1;
    outPixel->pui.s = 0;
}

void _ReadPixelFrom_A32B32G32R32_I(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctINT32* pI = (gctINT32*)inAddr[0];

    outPixel->pi.r = pI[0];
    outPixel->pi.g = pI[1];
    outPixel->pi.b = pI[2];
    outPixel->pi.a = pI[3];
    outPixel->pi.d = 1;
    outPixel->pi.s = 0;
}

void _ReadPixelFrom_A32B32G32R32_I_2_G32R32I(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctINT32* pI0 = (gctINT32*)inAddr[0];
    gctINT32* pI1 = (gctINT32*)inAddr[1];

    outPixel->pi.r = pI0[0];
    outPixel->pi.g = pI0[1];
    outPixel->pi.b = pI1[0];
    outPixel->pi.a = pI1[1];
    outPixel->pi.d = 1;
    outPixel->pi.s = 0;
}


void _ReadPixelFrom_A32B32G32R32_I_4_A8R8G8B8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctINT32* pI0 = (gctINT32*)inAddr[0];
    gctINT32* pI1 = (gctINT32*)inAddr[1];
    gctINT32* pI2 = (gctINT32*)inAddr[2];
    gctINT32* pI3 = (gctINT32*)inAddr[3];


    outPixel->pi.r = pI0[0];
    outPixel->pi.g = pI1[0];
    outPixel->pi.b = pI2[0];
    outPixel->pi.a = pI3[0];
    outPixel->pi.d = 1;
    outPixel->pi.s = 0;
}





void _ReadPixelFrom_A32B32G32R32_UI(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT32* pI = (gctUINT32*)inAddr[0];

    outPixel->pui.r = pI[0];
    outPixel->pui.g = pI[1];
    outPixel->pui.b = pI[2];
    outPixel->pui.a = pI[3];
    outPixel->pui.d = 1;
    outPixel->pui.s = 0;
}

void _ReadPixelFrom_A32B32G32R32_UI_2_G32R32UI(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT32* pI0 = (gctUINT32*)inAddr[0];
    gctUINT32* pI1 = (gctUINT32*)inAddr[1];

    outPixel->pui.r = pI0[0];
    outPixel->pui.g = pI0[1];
    outPixel->pui.b = pI1[0];
    outPixel->pui.a = pI1[1];
    outPixel->pui.d = 1;
    outPixel->pui.s = 0;
}

void _ReadPixelFrom_A32B32G32R32_UI_4_A8R8G8B8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT32* pI0 = (gctUINT32*)inAddr[0];
    gctUINT32* pI1 = (gctUINT32*)inAddr[1];
    gctUINT32* pI2 = (gctUINT32*)inAddr[2];
    gctUINT32* pI3 = (gctUINT32*)inAddr[3];


    outPixel->pui.r = pI0[0];
    outPixel->pui.g = pI1[0];
    outPixel->pui.b = pI2[0];
    outPixel->pui.a = pI3[0];
    outPixel->pui.d = 1;
    outPixel->pui.s = 0;
}




void _ReadPixelFrom_A32B32G32R32F(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctFLOAT* pf = (gctFLOAT*)inAddr[0];

    outPixel->pf.r = pf[0];
    outPixel->pf.g = pf[1];
    outPixel->pf.b = pf[2];
    outPixel->pf.a = pf[3];
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_A32B32G32R32F_2_G32R32F(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctFLOAT* pf0 = (gctFLOAT*)inAddr[0];
    gctFLOAT* pf1 = (gctFLOAT*)inAddr[1];

    outPixel->pf.r = pf0[0];
    outPixel->pf.g = pf0[1];
    outPixel->pf.b = pf1[0];
    outPixel->pf.a = pf1[1];
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_X32B32G32R32F_2_G32R32F(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctFLOAT* pf0 = (gctFLOAT*)inAddr[0];
    gctFLOAT* pf1 = (gctFLOAT*)inAddr[1];

    outPixel->pf.r = pf0[0];
    outPixel->pf.g = pf0[1];
    outPixel->pf.b = pf1[0];
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_A2B10G10R10_UI(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT32 ui = *(gctUINT32*)inAddr[0];

    outPixel->pui.r = gcdGET_FIELD(ui,  0, 10);
    outPixel->pui.g = gcdGET_FIELD(ui, 10, 10);
    outPixel->pui.b = gcdGET_FIELD(ui, 20, 10);
    outPixel->pui.a = gcdGET_FIELD(ui, 30,  2);
    outPixel->pui.d = 1;
    outPixel->pui.s = 0;
}

void _ReadPixelFrom_E5B9G9R9(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    const gctUINT32 mBits = 9;      /* mantissa bits */
    const gctUINT32 eBias = 15;     /* max allowed bias exponent */
    gctUINT32 ui = *(gctUINT32*)inAddr[0];
    gctFLOAT  scale = gcoMATH_Power(2.0f, (gctFLOAT)gcdGET_FIELD(ui, 27, 5) - eBias - mBits);

    outPixel->pf.r = (gctFLOAT)gcdGET_FIELD(ui, 0,  9) * scale;
    outPixel->pf.g = (gctFLOAT)gcdGET_FIELD(ui, 9,  9) * scale;
    outPixel->pf.b = (gctFLOAT)gcdGET_FIELD(ui, 18, 9) * scale;
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_B10G11R11(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctUINT16 r, g, b;
    gctUINT32 or, og, ob;
    gctUINT32 input = *(gctUINT32*) inAddr[0];
    r = input & 0x7ff;
    g = (input >> 11) & 0x7ff;
    b = (input >> 22) & 0x3ff;

    or = gcoMATH_Float11ToFloat(r);
    og = gcoMATH_Float11ToFloat(g);
    ob = gcoMATH_Float10ToFloat(b);

    outPixel->pf.r = *(gctFLOAT*)&or;
    outPixel->pf.g = *(gctFLOAT*)&og;
    outPixel->pf.b = *(gctFLOAT*)&ob;
    outPixel->pf.a = 1.0f;
    outPixel->pf.d = 1.0f;
    outPixel->pf.s = 0.0f;
}

void _ReadPixelFrom_S8D32F_1_G32R32F(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctFLOAT* pF = (gctFLOAT*)inAddr[0];
    outPixel->pf.d = pF[0];
    outPixel->pf.s = pF[1];
}

void _ReadPixelFrom_S8D32F_2_A8R8G8B8(gctPOINTER inAddr[4], gcsPIXEL* outPixel)
{
    gctFLOAT* pF0 = (gctFLOAT*)inAddr[0];
    gctFLOAT* pF1 = (gctFLOAT*)inAddr[1];

    outPixel->pf.d = pF0[0];
    outPixel->pf.s = pF1[0];
}

_PFNreadPixel gcoSURF_GetReadPixelFunc(gcoSURF surf)
{
    switch (surf->info.format)
    {
    case gcvSURF_A8:
        return _ReadPixelFrom_A8;
    case gcvSURF_A16:
        return _ReadPixelFrom_A16;
    case gcvSURF_A16F:
        return _ReadPixelFrom_A16F;
/*
    case gcvSURF_A32F:
        return _ReadPixelFrom_;
*/

    case gcvSURF_L8:
        return _ReadPixelFrom_L8;
    case gcvSURF_L16:
        return _ReadPixelFrom_L16;
/*
    case gcvSURF_L32:
        return _ReadPixelFrom_L32;
*/
    case gcvSURF_L16F:
        return _ReadPixelFrom_L16F;
/*
    case gcvSURF_L32F:
        return _ReadPixelFrom;
*/

    case gcvSURF_A8L8:
        return _ReadPixelFrom_A8L8;
    case gcvSURF_A16L16:
        return _ReadPixelFrom_A16L16;
    case gcvSURF_A16L16F:
        return _ReadPixelFrom_A16L16F;
/*
    case gcvSURF_A32L32F:
        return _ReadPixelFrom_;
*/

    case gcvSURF_R8:
        return _ReadPixelFrom_R8;
    case gcvSURF_R8_1_X8R8G8B8:
        return _ReadPixelFrom_R8_1_X8R8G8B8;
    case gcvSURF_R8_SNORM:
        return _ReadPixelFrom_R8_SNORM;
    case gcvSURF_R16:
        return _ReadPixelFrom_R16;

    case gcvSURF_R16F:
    case gcvSURF_R16F_1_A4R4G4B4:
        return _ReadPixelFrom_R16F;

    case gcvSURF_R32F:
    case gcvSURF_R32F_1_A8R8G8B8:
        return _ReadPixelFrom_R32F;

    case gcvSURF_G8R8:
        return _ReadPixelFrom_G8R8;
    case gcvSURF_G8R8_1_X8R8G8B8:
        return _ReadPixelFrom_G8R8_1_X8R8G8B8;
    case gcvSURF_G8R8_SNORM:
        return _ReadPixelFrom_G8R8_SNORM;
    case gcvSURF_G16R16:
        return _ReadPixelFrom_G16R16;

    case gcvSURF_G16R16F:
    case gcvSURF_G16R16F_1_A8R8G8B8:
        return _ReadPixelFrom_G16R16F;

    case gcvSURF_G32R32F:
        return _ReadPixelFrom_G32R32F;

    case gcvSURF_G32R32F_2_A8R8G8B8:
        return _ReadPixelFrom_G32R32F_2_A8R8G8B8;

    case gcvSURF_A4R4G4B4:
        return _ReadPixelFrom_A4R4G4B4;

    case gcvSURF_A1R5G5B5:
        return _ReadPixelFrom_A1R5G5B5;

    case gcvSURF_R5G6B5:
        return _ReadPixelFrom_R5G6B5;
    case gcvSURF_B8G8R8:
        return _ReadPixelFrom_B8G8R8;
    case gcvSURF_B8G8R8_SNORM:
        return _ReadPixelFrom_B8G8R8_SNORM;
    case gcvSURF_B16G16R16:
        return _ReadPixelFrom_B16G16R16;
    case gcvSURF_X4R4G4B4:
        return _ReadPixelFrom_X4R4G4B4;
    case gcvSURF_R4G4B4A4:
        return _ReadPixelFrom_R4G4B4A4;
    case gcvSURF_X1R5G5B5:
        return _ReadPixelFrom_X1R5G5B5;
    case gcvSURF_R5G5B5A1:
        return _ReadPixelFrom_R5G5B5A1;
    case gcvSURF_A8B8G8R8:
        return _ReadPixelFrom_A8B8G8R8;
    case gcvSURF_A8B8G8R8_SNORM:
        return _ReadPixelFrom_A8B8G8R8_SNORM;
    case gcvSURF_A8R8G8B8:
        return _ReadPixelFrom_A8R8G8B8;
    case gcvSURF_X8R8G8B8:
        return _ReadPixelFrom_X8R8G8B8;
    case gcvSURF_X8B8G8R8_SNORM:
        return _ReadPixelFrom_X8B8G8R8_SNORM;
    case gcvSURF_X2B10G10R10:
        return _ReadPixelFrom_X2B10G10R10;
    case gcvSURF_A2B10G10R10:
        return _ReadPixelFrom_A2B10G10R10;
    case gcvSURF_A16B16G16R16:
        return _ReadPixelFrom_A16B16G16R16;
    case gcvSURF_X16B16G16R16F:
        return _ReadPixelFrom_X16B16G16R16F;
    case gcvSURF_A16B16G16R16F:
        return _ReadPixelFrom_A16B16G16R16F;
    case gcvSURF_B16G16R16F:
        return _ReadPixelFrom_B16G16R16F;
    case gcvSURF_X16B16G16R16F_2_A8R8G8B8:
    case gcvSURF_B16G16R16F_2_A8R8G8B8:
        return _ReadPixelFrom_B16G16R16F_2_A8R8G8B8;
    case gcvSURF_A16B16G16R16F_2_A8R8G8B8:
        return _ReadPixelFrom_A16B16G16R16F_2_A8R8G8B8;

    case gcvSURF_B32G32R32F:
        return _ReadPixelFrom_B32G32R32F;
    case gcvSURF_B32G32R32F_3_A8R8G8B8:
        return _ReadPixelFrom_B32G32R32F_3_A8R8G8B8;

    case gcvSURF_A32B32G32R32F:
        return _ReadPixelFrom_A32B32G32R32F;

    case gcvSURF_A32B32G32R32F_2_G32R32F:
        return _ReadPixelFrom_A32B32G32R32F_2_G32R32F;
    case gcvSURF_X32B32G32R32F_2_G32R32F:
        return _ReadPixelFrom_X32B32G32R32F_2_G32R32F;

    case gcvSURF_D16:
        return _ReadPixelFrom_D16;
    case gcvSURF_D24X8:
        return _ReadPixelFrom_D24X8;
    case gcvSURF_D24S8:
        return _ReadPixelFrom_D24S8;
    case gcvSURF_D32:
        return _ReadPixelFrom_D32;
    case gcvSURF_D32F:
        return _ReadPixelFrom_D32F;

    case gcvSURF_S8D32F:
        return _ReadPixelFrom_S8D32F;

    case gcvSURF_S8:
        return _ReadPixelFrom_S8;

    case gcvSURF_S8D32F_1_G32R32F:
        return _ReadPixelFrom_S8D32F_1_G32R32F;
    case gcvSURF_S8D32F_2_A8R8G8B8:
        return _ReadPixelFrom_S8D32F_2_A8R8G8B8;
    case gcvSURF_D24S8_1_A8R8G8B8:
        return _ReadPixelFrom_D24S8;

    case gcvSURF_A8B12G12R12_2_A8R8G8B8:
        return _ReadPixelFrom_A8B12G12R12_2_A8R8G8B8;

    /* Integer format. */
    case gcvSURF_R8I:
    case gcvSURF_R8I_1_A4R4G4B4:
        return _ReadPixelFrom_R8_I;

    case gcvSURF_R8UI:
    case gcvSURF_R8UI_1_A4R4G4B4:
        return _ReadPixelFrom_R8_UI;

    case gcvSURF_R16I:
    case gcvSURF_R16I_1_A4R4G4B4:
        return _ReadPixelFrom_R16_I;

    case gcvSURF_R16UI:
    case gcvSURF_R16UI_1_A4R4G4B4:
        return _ReadPixelFrom_R16_UI;

    case gcvSURF_R32I:
    case gcvSURF_R32I_1_A8R8G8B8:
        return _ReadPixelFrom_R32_I;

    case gcvSURF_R32UI:
    case gcvSURF_R32UI_1_A8R8G8B8:
        return _ReadPixelFrom_R32_UI;

    case gcvSURF_G8R8I:
    case gcvSURF_G8R8I_1_A4R4G4B4:
        return _ReadPixelFrom_G8R8_I;

    case gcvSURF_G8R8UI:
    case gcvSURF_G8R8UI_1_A4R4G4B4:
        return _ReadPixelFrom_G8R8_UI;

    case gcvSURF_G16R16I:
    case gcvSURF_G16R16I_1_A8R8G8B8:
        return _ReadPixelFrom_G16R16_I;

    case gcvSURF_G16R16UI:
    case gcvSURF_G16R16UI_1_A8R8G8B8:
        return _ReadPixelFrom_G16R16_UI;

    case gcvSURF_G32R32I:
        return _ReadPixelFrom_G32R32_I;

    case gcvSURF_G32R32I_2_A8R8G8B8:
        return _ReadPixelFrom_G32R32_I_2_A8R8G8B8;

    case gcvSURF_G32R32UI:
        return _ReadPixelFrom_G32R32_UI;

    case gcvSURF_G32R32UI_2_A8R8G8B8:
        return _ReadPixelFrom_G32R32_UI_2_A8R8G8B8;

    case gcvSURF_B8G8R8I:
    case gcvSURF_B8G8R8I_1_A8R8G8B8:
        return _ReadPixelFrom_B8G8R8_I;

    case gcvSURF_B8G8R8UI:
    case gcvSURF_B8G8R8UI_1_A8R8G8B8:
        return _ReadPixelFrom_B8G8R8_UI;

    case gcvSURF_B16G16R16I:
        return _ReadPixelFrom_B16G16R16_I;

    case gcvSURF_B16G16R16I_2_A8R8G8B8:
        return _ReadPixelFrom_B16G16R16I_2_A8R8G8B8;

    case gcvSURF_B16G16R16UI:
        return _ReadPixelFrom_B16G16R16_UI;

    case gcvSURF_B16G16R16UI_2_A8R8G8B8:
        return _ReadPixelFrom_B16G16R16UI_2_A8R8G8B8;

    case gcvSURF_B32G32R32I:
        return _ReadPixelFrom_B32G32R32_I;

    case gcvSURF_B32G32R32I_3_A8R8G8B8:
        return _ReadPixelFrom_B32G32R32_I_3_A8R8G8B8;

    case gcvSURF_B32G32R32UI:
        return _ReadPixelFrom_B32G32R32_UI;

    case gcvSURF_B32G32R32UI_3_A8R8G8B8:
        return _ReadPixelFrom_B32G32R32_UI_3_A8R8G8B8;

    case gcvSURF_A8B8G8R8I:
    case gcvSURF_A8B8G8R8I_1_A8R8G8B8:
        return _ReadPixelFrom_A8B8G8R8_I;

    case gcvSURF_A8B8G8R8UI:
    case gcvSURF_A8B8G8R8UI_1_A8R8G8B8:
        return _ReadPixelFrom_A8B8G8R8_UI;

    case gcvSURF_A16B16G16R16I:
        return _ReadPixelFrom_A16B16G16R16_I;
    case gcvSURF_A16B16G16R16I_2_A8R8G8B8:
        return _ReadPixelFrom_A16B16G16R16_I_2_A8R8G8B8;

    case gcvSURF_A16B16G16R16UI:
        return _ReadPixelFrom_A16B16G16R16_UI;
    case gcvSURF_A16B16G16R16UI_2_A8R8G8B8:
        return _ReadPixelFrom_A16B16G16R16_UI_2_A8R8G8B8;

    case gcvSURF_A32B32G32R32I:
        return _ReadPixelFrom_A32B32G32R32_I;
    case gcvSURF_A32B32G32R32I_2_G32R32I:
        return _ReadPixelFrom_A32B32G32R32_I_2_G32R32I;
    case gcvSURF_A32B32G32R32I_4_A8R8G8B8:
        return _ReadPixelFrom_A32B32G32R32_I_4_A8R8G8B8;

    case gcvSURF_A32B32G32R32UI:
        return _ReadPixelFrom_A32B32G32R32_UI;
    case gcvSURF_A32B32G32R32UI_2_G32R32UI:
        return _ReadPixelFrom_A32B32G32R32_UI_2_G32R32UI;
    case gcvSURF_A32B32G32R32UI_4_A8R8G8B8:
        return _ReadPixelFrom_A32B32G32R32_UI_4_A8R8G8B8;

    case gcvSURF_A2B10G10R10UI:
    case gcvSURF_A2B10G10R10UI_1_A8R8G8B8:
        return _ReadPixelFrom_A2B10G10R10_UI;

    case gcvSURF_SBGR8:
        return _ReadPixelFrom_B8G8R8;

    case gcvSURF_A8_SBGR8:
        return _ReadPixelFrom_A8B8G8R8;

    case gcvSURF_E5B9G9R9:
        return _ReadPixelFrom_E5B9G9R9;

    case gcvSURF_B10G11R11F:
    case gcvSURF_B10G11R11F_1_A8R8G8B8:
        return _ReadPixelFrom_B10G11R11;

    default:
        gcmASSERT(0);
    }

    return gcvNULL;
}


void _WritePixelTo_A8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    *(gctUINT8*)outAddr[0] = (gctUINT8)gcdFLOAT_TO_UNORM(inPixel->pf.a, 8);
}

void _WritePixelTo_L8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    *(gctUINT8*)outAddr[0] = (gctUINT8)gcdFLOAT_TO_UNORM(inPixel->pf.r, 8);
}

void _WritePixelTo_R8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    *(gctUINT8*)outAddr[0] = (gctUINT8)gcdFLOAT_TO_UNORM(inPixel->pf.r, 8);
}

void _WritePixelTo_R8_1_X8R8G8B8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT8 *pUB = (gctUINT8*)outAddr[0];

    pUB[0] = (gctUINT8)gcdFLOAT_TO_UNORM(0.0f, 8);
    pUB[1] = (gctUINT8)gcdFLOAT_TO_UNORM(0.0f, 8);
    pUB[2] = (gctUINT8)gcdFLOAT_TO_UNORM(inPixel->pf.r, 8);
    pUB[3] = (gctUINT8)gcdFLOAT_TO_UNORM(1.0f, 8);
}

void _WritePixelTo_A8L8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT8 *pUB = (gctUINT8*)outAddr[0];

    pUB[0] = (gctUINT8)gcdFLOAT_TO_UNORM(inPixel->pf.r, 8);
    pUB[1] = (gctUINT8)gcdFLOAT_TO_UNORM(inPixel->pf.a, 8);
}

void _WritePixelTo_G8R8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT8 *pUB = (gctUINT8*)outAddr[0];

    pUB[0] = (gctUINT8)gcdFLOAT_TO_UNORM(inPixel->pf.r, 8);
    pUB[1] = (gctUINT8)gcdFLOAT_TO_UNORM(inPixel->pf.g, 8);
}

void _WritePixelTo_G8R8_1_X8R8G8B8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT8 *pUB = (gctUINT8*)outAddr[0];

    pUB[0] = (gctUINT8)gcdFLOAT_TO_UNORM(0.0, 8);
    pUB[1] = (gctUINT8)gcdFLOAT_TO_UNORM(inPixel->pf.g, 8);
    pUB[2] = (gctUINT8)gcdFLOAT_TO_UNORM(inPixel->pf.r, 8);
    pUB[3] = (gctUINT8)gcdFLOAT_TO_UNORM(1.0f, 8);
}

void _WritePixelTo_R5G6B5(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    *(gctUINT16*)outAddr[0] = (gctUINT16)(gcdFLOAT_TO_UNORM(inPixel->pf.r, 5) << 11 |
                                          gcdFLOAT_TO_UNORM(inPixel->pf.g, 6) << 5  |
                                          gcdFLOAT_TO_UNORM(inPixel->pf.b, 5));
}

void _WritePixelTo_X4R4G4B4(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    *(gctUINT16*)outAddr[0] = (gctUINT16)(gcdFLOAT_TO_UNORM(inPixel->pf.r, 4) << 8 |
                                          gcdFLOAT_TO_UNORM(inPixel->pf.g, 4) << 4 |
                                          gcdFLOAT_TO_UNORM(inPixel->pf.b, 4));
}

void _WritePixelTo_A4R4G4B4(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    *(gctUINT16*)outAddr[0] = (gctUINT16)(gcdFLOAT_TO_UNORM(inPixel->pf.a, 4) << 12 |
                                          gcdFLOAT_TO_UNORM(inPixel->pf.r, 4) << 8  |
                                          gcdFLOAT_TO_UNORM(inPixel->pf.g, 4) << 4  |
                                          gcdFLOAT_TO_UNORM(inPixel->pf.b, 4));
}

void _WritePixelTo_R4G4B4A4(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    *(gctUINT16*)outAddr[0] = (gctUINT16)(gcdFLOAT_TO_UNORM(inPixel->pf.r, 4) << 12 |
                                          gcdFLOAT_TO_UNORM(inPixel->pf.g, 4) << 8  |
                                          gcdFLOAT_TO_UNORM(inPixel->pf.b, 4) << 4  |
                                          gcdFLOAT_TO_UNORM(inPixel->pf.a, 4));
}

void _WritePixelTo_X1R5G5B5(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    *(gctUINT16*)outAddr[0] = (gctUINT16)(gcdFLOAT_TO_UNORM(inPixel->pf.r, 5) << 10 |
                                          gcdFLOAT_TO_UNORM(inPixel->pf.g, 5) << 5  |
                                          gcdFLOAT_TO_UNORM(inPixel->pf.b, 5));
}

void _WritePixelTo_A1R5G5B5(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    *(gctUINT16*)outAddr[0] = (gctUINT16)(gcdFLOAT_TO_UNORM(inPixel->pf.a, 1) << 15 |
                                          gcdFLOAT_TO_UNORM(inPixel->pf.r, 5) << 10 |
                                          gcdFLOAT_TO_UNORM(inPixel->pf.g, 5) << 5  |
                                          gcdFLOAT_TO_UNORM(inPixel->pf.b, 5));
}

void _WritePixelTo_X8R8G8B8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT8 *pUB = (gctUINT8*)outAddr[0];

    pUB[0] = (gctUINT8)gcdFLOAT_TO_UNORM(inPixel->pf.b, 8);
    pUB[1] = (gctUINT8)gcdFLOAT_TO_UNORM(inPixel->pf.g, 8);
    pUB[2] = (gctUINT8)gcdFLOAT_TO_UNORM(inPixel->pf.r, 8);
    pUB[3] = 0xFF;
}

void _WritePixelTo_A8B8G8R8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT8 *pUB = (gctUINT8*)outAddr[0];

    pUB[0] = (gctUINT8)gcdFLOAT_TO_UNORM(inPixel->pf.r, 8);
    pUB[1] = (gctUINT8)gcdFLOAT_TO_UNORM(inPixel->pf.g, 8);
    pUB[2] = (gctUINT8)gcdFLOAT_TO_UNORM(inPixel->pf.b, 8);
    pUB[3] = (gctUINT8)gcdFLOAT_TO_UNORM(inPixel->pf.a, 8);
}

void _WritePixelTo_X8B8G8R8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT8 *pUB = (gctUINT8*)outAddr[0];

    pUB[0] = (gctUINT8)gcdFLOAT_TO_UNORM(inPixel->pf.r, 8);
    pUB[1] = (gctUINT8)gcdFLOAT_TO_UNORM(inPixel->pf.g, 8);
    pUB[2] = (gctUINT8)gcdFLOAT_TO_UNORM(inPixel->pf.b, 8);
    pUB[3] = (gctUINT8)gcdFLOAT_TO_UNORM(1.0, 8);
}


void _WritePixelTo_B8G8R8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT8 *pUB = (gctUINT8*)outAddr[0];

    pUB[0] = (gctUINT8)gcdFLOAT_TO_UNORM(inPixel->pf.r, 8);
    pUB[1] = (gctUINT8)gcdFLOAT_TO_UNORM(inPixel->pf.g, 8);
    pUB[2] = (gctUINT8)gcdFLOAT_TO_UNORM(inPixel->pf.b, 8);
}


void _WritePixelTo_A8R8G8B8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT8 *pUB = (gctUINT8*)outAddr[0];

    pUB[0] = (gctUINT8)gcdFLOAT_TO_UNORM(inPixel->pf.b, 8);
    pUB[1] = (gctUINT8)gcdFLOAT_TO_UNORM(inPixel->pf.g, 8);
    pUB[2] = (gctUINT8)gcdFLOAT_TO_UNORM(inPixel->pf.r, 8);
    pUB[3] = (gctUINT8)gcdFLOAT_TO_UNORM(inPixel->pf.a, 8);
}

void _WritePixelTo_X2B10G10R10(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    *(gctUINT32*)outAddr[0] = (gctUINT32)(gcdFLOAT_TO_UNORM(inPixel->pf.b, 10) << 20 |
                                          gcdFLOAT_TO_UNORM(inPixel->pf.g, 10) << 10 |
                                          gcdFLOAT_TO_UNORM(inPixel->pf.r, 10));
}

void _WritePixelTo_A2B10G10R10(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    *(gctUINT32*)outAddr[0] = (gctUINT32)(gcdFLOAT_TO_UNORM(inPixel->pf.a,  2) << 30 |
                                          gcdFLOAT_TO_UNORM(inPixel->pf.b, 10) << 20 |
                                          gcdFLOAT_TO_UNORM(inPixel->pf.g, 10) << 10 |
                                          gcdFLOAT_TO_UNORM(inPixel->pf.r, 10));
}

void _WritePixelTo_A8B12G12R12_2_A8R8G8B8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT8 *pUB0 = (gctUINT8*)outAddr[0];
    gctUINT8 *pUB1 = (gctUINT8*)outAddr[1];

    pUB0[0] = (gctUINT8) ((gcdFLOAT_TO_UNORM(inPixel->pf.r, 12) & 0xF00) >> 4);
    pUB0[1] = (gctUINT8) ((gcdFLOAT_TO_UNORM(inPixel->pf.g, 12) & 0xF00) >> 4);
    pUB0[2] = (gctUINT8) ((gcdFLOAT_TO_UNORM(inPixel->pf.b, 12) & 0xF00) >> 4);
    pUB0[3] = (gctUINT8) gcdFLOAT_TO_UNORM(inPixel->pf.a, 8);

    pUB1[0] = (gctUINT8) (gcdFLOAT_TO_UNORM(inPixel->pf.b, 12) & 0xFF);
    pUB1[1] = (gctUINT8) (gcdFLOAT_TO_UNORM(inPixel->pf.g, 12) & 0xFF);
    pUB1[2] = (gctUINT8) (gcdFLOAT_TO_UNORM(inPixel->pf.r, 12) & 0xFF);
    pUB1[3] = (gctUINT8) gcdFLOAT_TO_UNORM(inPixel->pf.a, 8);
}

void _WritePixelTo_D16(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    *(gctUINT16*)outAddr[0] = (gctUINT16)gcdFLOAT_TO_UNORM(inPixel->pf.d, 16);
}

void _WritePixelTo_D24X8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT32 depth = gcdFLOAT_TO_UNORM(inPixel->pf.d, 24);

    /* Float cannot hold 24bit precision, and the nearest rounding sometimes
    ** would cause the value larger than maximum, especially on ARM platform.
    */
    if (depth > (gctUINT32)gcdUNORM_MAX(24))
    {
        depth = (gctUINT32)gcdUNORM_MAX(24);
    }

    *(gctUINT32*)outAddr[0] = (gctUINT32)(depth << 8);
}

void _WritePixelTo_D24S8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT32 depth, stencil;

    if (flags & gcvBLIT_FLAG_SKIP_DEPTH_WRITE)
    {
        depth = (*(gctUINT32*)outAddr[0]) >> 8;
    }
    else
    {
        depth = gcdFLOAT_TO_UNORM(inPixel->pf.d, 24);

        /* Float cannot hold 24bit precision, and the nearest rounding sometimes
        ** would cause the value larger than maximum, especially on ARM platform.
        */
        if (depth > (gctUINT32)gcdUNORM_MAX(24))
        {
            depth = (gctUINT32)gcdUNORM_MAX(24);
        }
    }

    if (flags & gcvBLIT_FLAG_SKIP_STENCIL_WRITE)
    {
        stencil = (*(gctUINT32*)outAddr[0]) & 0xFF;
    }
    else
    {
        stencil = gcdFLOAT_TO_UNORM(inPixel->pf.s, 8);
    }

    *(gctUINT32*)outAddr[0] = (gctUINT32)(depth << 8 | stencil);
}

void _WritePixelTo_D32(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT64 depth = (gctUINT64)(inPixel->pf.d * gcdUNORM_MAX(32) + 0.5f);

    /* Float cannot hold 32bit precision, and the nearest rounding sometimes
    ** would cause the value larger than maximum, especially on ARM platform.
    */
    if (depth > (gctUINT64)gcdUNORM_MAX(32))
    {
        depth = (gctUINT64)gcdUNORM_MAX(32);
    }

    *(gctUINT32*)outAddr[0] = (gctUINT32)gcdFLOAT_TO_UNORM(inPixel->pf.d, 32);
}

void _WritePixelTo_D32F(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    *(gctFLOAT*)outAddr[0] = gcmCLAMP(inPixel->pf.d, 0.0f, 1.0f);
}

void _WritePixelTo_S8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    *(gctUINT8*)outAddr[0] = (gctUINT8)gcmMIN(inPixel->pui.s, gcdUINT_MAX(8));
}


/* SNORM format. */
void _WritePixelTo_R8_SNORM(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    *(gctINT8*)outAddr[0] = (gctINT8)gcdFLOAT_TO_SNORM(inPixel->pf.r, 8);
}

void _WritePixelTo_G8R8_SNORM(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctINT8 *pB = (gctINT8*)outAddr[0];

    pB[0] = (gctINT8)gcdFLOAT_TO_SNORM(inPixel->pf.r, 8);
    pB[1] = (gctINT8)gcdFLOAT_TO_SNORM(inPixel->pf.g, 8);
}

void _WritePixelTo_B8G8R8_SNORM(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctINT8 *pB = (gctINT8*)outAddr[0];

    pB[0] = (gctINT8)gcdFLOAT_TO_SNORM(inPixel->pf.r, 8);
    pB[1] = (gctINT8)gcdFLOAT_TO_SNORM(inPixel->pf.g, 8);
    pB[2] = (gctINT8)gcdFLOAT_TO_SNORM(inPixel->pf.b, 8);
}

void _WritePixelTo_A8B8G8R8_SNORM(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctINT8 *pB = (gctINT8*)outAddr[0];

    pB[0] = (gctINT8)gcdFLOAT_TO_SNORM(inPixel->pf.r, 8);
    pB[1] = (gctINT8)gcdFLOAT_TO_SNORM(inPixel->pf.g, 8);
    pB[2] = (gctINT8)gcdFLOAT_TO_SNORM(inPixel->pf.b, 8);
    pB[3] = (gctINT8)gcdFLOAT_TO_SNORM(inPixel->pf.a, 8);
}

void _WritePixelTo_X8B8G8R8_SNORM(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctINT8 *pB = (gctINT8*)outAddr[0];

    pB[0] = (gctINT8)gcdFLOAT_TO_SNORM(inPixel->pf.r, 8);
    pB[1] = (gctINT8)gcdFLOAT_TO_SNORM(inPixel->pf.g, 8);
    pB[2] = (gctINT8)gcdFLOAT_TO_SNORM(inPixel->pf.b, 8);
    pB[3] = (gctINT8)gcdFLOAT_TO_SNORM(1.0f, 8);
}


/* Integer format. */
void _WritePixelTo_R8_I(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    *(gctINT8*)outAddr[0] = (gctINT8)gcmCLAMP(inPixel->pi.r, gcdINT_MIN(8), gcdINT_MAX(8));
}

void _WritePixelTo_R8_UI(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    *(gctUINT8*)outAddr[0] = (gctUINT8)gcmMIN(inPixel->pui.r, gcdUINT_MAX(8));
}

void _WritePixelTo_R16_I(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    *(gctINT16*)outAddr[0] = (gctINT16)gcmCLAMP(inPixel->pi.r, gcdINT_MIN(16), gcdINT_MAX(16));
}

void _WritePixelTo_R16_UI(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    *(gctUINT16*)outAddr[0] = (gctUINT16)gcmMIN(inPixel->pui.r, gcdUINT_MAX(16));
}

void _WritePixelTo_R16F(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    *(gctUINT16*)outAddr[0] = gcoMATH_FloatToFloat16(*(gctUINT32*)&inPixel->pf.r);
}

void _WritePixelTo_R32_I(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    *(gctINT32*)outAddr[0] = (gctINT32)(inPixel->pi.r);
}

void _WritePixelTo_R32_UI(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    *(gctUINT32*)outAddr[0] = (gctUINT32)(inPixel->pui.r);
}

void _WritePixelTo_R32F(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    *(gctFLOAT*)outAddr[0] = (gctFLOAT)(inPixel->pf.r);
}

void _WritePixelTo_G8R8_I(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctINT8* pI = (gctINT8*)outAddr[0];

    pI[0] = (gctINT8)gcmCLAMP(inPixel->pi.r, gcdINT_MIN(8), gcdINT_MAX(8));
    pI[1] = (gctINT8)gcmCLAMP(inPixel->pi.g, gcdINT_MIN(8), gcdINT_MAX(8));
}

void _WritePixelTo_G8R8_UI(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT8* pI = (gctUINT8*)outAddr[0];

    pI[0] = (gctUINT8)gcmMIN(inPixel->pui.r, gcdUINT_MAX(8));
    pI[1] = (gctUINT8)gcmMIN(inPixel->pui.g, gcdUINT_MAX(8));
}

void _WritePixelTo_G16R16_I(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctINT16* pI = (gctINT16*)outAddr[0];

    pI[0] = (gctINT16)gcmCLAMP(inPixel->pi.r, gcdINT_MIN(16), gcdINT_MAX(16));
    pI[1] = (gctINT16)gcmCLAMP(inPixel->pi.g, gcdINT_MIN(16), gcdINT_MAX(16));
}

void _WritePixelTo_G16R16_UI(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT16* pI = (gctUINT16*)outAddr[0];

    pI[0] = (gctUINT16)gcmMIN(inPixel->pui.r, gcdUINT_MAX(16));
    pI[1] = (gctUINT16)gcmMIN(inPixel->pui.g, gcdUINT_MAX(16));
}

void _WritePixelTo_G16R16F(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT16* pI = (gctUINT16*)outAddr[0];

    pI[0] = (gctUINT16)gcoMATH_FloatToFloat16(*(gctUINT32*)&inPixel->pf.r);
    pI[1] = (gctUINT16)gcoMATH_FloatToFloat16(*(gctUINT32*)&inPixel->pf.g);
}

void _WritePixelTo_G32R32_I(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctINT32* pI = (gctINT32*)outAddr[0];

    pI[0] = (gctINT32)(inPixel->pi.r);
    pI[1] = (gctINT32)(inPixel->pi.g);
}

void _WritePixelTo_G32R32_I_2_A8R8G8B8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctINT32* pI0 = (gctINT32*)outAddr[0];
    gctINT32* pI1 = (gctINT32*)outAddr[1];

    pI0[0] = (gctINT32)(inPixel->pi.r);
    pI1[0] = (gctINT32)(inPixel->pi.g);
}

void _WritePixelTo_G32R32_UI(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT32* pI = (gctUINT32*)outAddr[0];

    pI[0] = (gctUINT32)(inPixel->pui.r);
    pI[1] = (gctUINT32)(inPixel->pui.g);
}

void _WritePixelTo_G32R32_UI_2_A8R8G8B8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT32* pI0 = (gctUINT32*)outAddr[0];
    gctUINT32* pI1 = (gctUINT32*)outAddr[1];

    pI0[0] = (gctUINT32)(inPixel->pui.r);
    pI1[0] = (gctUINT32)(inPixel->pui.g);
}

void _WritePixelTo_G32R32F(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctFLOAT* pI = (gctFLOAT*)outAddr[0];

    pI[0] = (gctFLOAT)(inPixel->pf.r);
    pI[1] = (gctFLOAT)(inPixel->pf.g);
}

void _WritePixelTo_G32R32F_2_A8R8G8B8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctFLOAT* pI0 = (gctFLOAT*)outAddr[0];
    gctFLOAT* pI1 = (gctFLOAT*)outAddr[1];

    pI0[0] = (gctFLOAT)(inPixel->pf.r);
    pI1[0] = (gctFLOAT)(inPixel->pf.g);
}

void _WritePixelTo_B8G8R8_I(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctINT8* pI = (gctINT8*)outAddr[0];

    pI[0] = (gctINT8)gcmCLAMP(inPixel->pi.r, gcdINT_MIN(8), gcdINT_MAX(8));
    pI[1] = (gctINT8)gcmCLAMP(inPixel->pi.g, gcdINT_MIN(8), gcdINT_MAX(8));
    pI[2] = (gctINT8)gcmCLAMP(inPixel->pi.b, gcdINT_MIN(8), gcdINT_MAX(8));
}

void _WritePixelTo_B8G8R8_UI(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT8* pI = (gctUINT8*)outAddr[0];

    pI[0] = (gctUINT8)gcmMIN(inPixel->pui.r, gcdUINT_MAX(8));
    pI[1] = (gctUINT8)gcmMIN(inPixel->pui.g, gcdUINT_MAX(8));
    pI[2] = (gctUINT8)gcmMIN(inPixel->pui.b, gcdUINT_MAX(8));
}

void _WritePixelTo_B16G16R16_I(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctINT16* pI = (gctINT16*)outAddr[0];

    pI[0] = (gctINT16)gcmCLAMP(inPixel->pi.r, gcdINT_MIN(16), gcdINT_MAX(16));
    pI[1] = (gctINT16)gcmCLAMP(inPixel->pi.g, gcdINT_MIN(16), gcdINT_MAX(16));
    pI[2] = (gctINT16)gcmCLAMP(inPixel->pi.b, gcdINT_MIN(16), gcdINT_MAX(16));
}

void _WritePixelTo_B16G16R16_I_2_A8R8G8B8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctINT16* pI0 = (gctINT16*)outAddr[0];
    gctINT16* pI1 = (gctINT16*)outAddr[1];

    pI0[0] = (gctINT16)gcmCLAMP(inPixel->pi.r, gcdINT_MIN(16), gcdINT_MAX(16));
    pI0[1] = (gctINT16)gcmCLAMP(inPixel->pi.g, gcdINT_MIN(16), gcdINT_MAX(16));
    pI1[0] = (gctINT16)gcmCLAMP(inPixel->pi.b, gcdINT_MIN(16), gcdINT_MAX(16));
}

void _WritePixelTo_B16G16R16_UI(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT16* pI = (gctUINT16*)outAddr[0];

    pI[0] = (gctUINT16)gcmMIN(inPixel->pui.r, gcdUINT_MAX(16));
    pI[1] = (gctUINT16)gcmMIN(inPixel->pui.g, gcdUINT_MAX(16));
    pI[2] = (gctUINT16)gcmMIN(inPixel->pui.b, gcdUINT_MAX(16));
}

void _WritePixelTo_B16G16R16_UI_2_A8R8G8B8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT16* pI0 = (gctUINT16*)outAddr[0];
    gctUINT16* pI1 = (gctUINT16*)outAddr[1];

    pI0[0] = (gctUINT16)gcmMIN(inPixel->pui.r, gcdUINT_MAX(16));
    pI0[1] = (gctUINT16)gcmMIN(inPixel->pui.g, gcdUINT_MAX(16));
    pI1[0] = (gctUINT16)gcmMIN(inPixel->pui.b, gcdUINT_MAX(16));
}

void _WritePixelTo_B32G32R32_I(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctINT32* pI = (gctINT32*)outAddr[0];

    pI[0] = (gctINT32)(inPixel->pi.r);
    pI[1] = (gctINT32)(inPixel->pi.g);
    pI[2] = (gctINT32)(inPixel->pi.b);
}

void _WritePixelTo_B32G32R32_I_3_A8R8G8B8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctINT32* pI0 = (gctINT32*)outAddr[0];
    gctINT32* pI1 = (gctINT32*)outAddr[1];
    gctINT32* pI2 = (gctINT32*)outAddr[2];

    pI0[0] = (gctINT32)(inPixel->pi.r);
    pI1[0] = (gctINT32)(inPixel->pi.g);
    pI2[0] = (gctINT32)(inPixel->pi.b);
}

void _WritePixelTo_B32G32R32_UI(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT32* pI = (gctUINT32*)outAddr[0];

    pI[0] = (gctUINT32)(inPixel->pui.r);
    pI[1] = (gctUINT32)(inPixel->pui.g);
    pI[2] = (gctUINT32)(inPixel->pui.b);
}

void _WritePixelTo_B32G32R32_UI_3_A8R8G8B8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT32* pI0 = (gctUINT32*)outAddr[0];
    gctUINT32* pI1 = (gctUINT32*)outAddr[1];
    gctUINT32* pI2 = (gctUINT32*)outAddr[2];

    pI0[0] = (gctUINT32)(inPixel->pui.r);
    pI1[0] = (gctUINT32)(inPixel->pui.g);
    pI2[0] = (gctUINT32)(inPixel->pui.b);
}

void _WritePixelTo_A8B8G8R8_I(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctINT8* pI = (gctINT8*)outAddr[0];

    pI[0] = (gctINT8)gcmCLAMP(inPixel->pi.r, gcdINT_MIN(8), gcdINT_MAX(8));
    pI[1] = (gctINT8)gcmCLAMP(inPixel->pi.g, gcdINT_MIN(8), gcdINT_MAX(8));
    pI[2] = (gctINT8)gcmCLAMP(inPixel->pi.b, gcdINT_MIN(8), gcdINT_MAX(8));
    pI[3] = (gctINT8)gcmCLAMP(inPixel->pi.a, gcdINT_MIN(8), gcdINT_MAX(8));
}

void _WritePixelTo_X8B8G8R8_I(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctINT8* pI = (gctINT8*)outAddr[0];

    pI[0] = (gctINT8)gcmCLAMP(inPixel->pi.r, gcdINT_MIN(8), gcdINT_MAX(8));
    pI[1] = (gctINT8)gcmCLAMP(inPixel->pi.g, gcdINT_MIN(8), gcdINT_MAX(8));
    pI[2] = (gctINT8)gcmCLAMP(inPixel->pi.b, gcdINT_MIN(8), gcdINT_MAX(8));
    pI[3] = (gctINT8)1;
}

void _WritePixelTo_A8B8G8R8_UI(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT8* pI = (gctUINT8*)outAddr[0];

    pI[0] = (gctUINT8)gcmMIN(inPixel->pui.r, gcdUINT_MAX(8));
    pI[1] = (gctUINT8)gcmMIN(inPixel->pui.g, gcdUINT_MAX(8));
    pI[2] = (gctUINT8)gcmMIN(inPixel->pui.b, gcdUINT_MAX(8));
    pI[3] = (gctUINT8)gcmMIN(inPixel->pui.a, gcdUINT_MAX(8));
}

void _WritePixelTo_X8B8G8R8_UI(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT8* pI = (gctUINT8*)outAddr[0];

    pI[0] = (gctUINT8)gcmMIN(inPixel->pui.r, gcdUINT_MAX(8));
    pI[1] = (gctUINT8)gcmMIN(inPixel->pui.g, gcdUINT_MAX(8));
    pI[2] = (gctUINT8)gcmMIN(inPixel->pui.b, gcdUINT_MAX(8));
    pI[3] = (gctUINT8)1;
}

void _WritePixelTo_A16B16G16R16_I(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctINT16* pI = (gctINT16*)outAddr[0];

    pI[0] = (gctINT16)gcmCLAMP(inPixel->pi.r, gcdINT_MIN(16), gcdINT_MAX(16));
    pI[1] = (gctINT16)gcmCLAMP(inPixel->pi.g, gcdINT_MIN(16), gcdINT_MAX(16));
    pI[2] = (gctINT16)gcmCLAMP(inPixel->pi.b, gcdINT_MIN(16), gcdINT_MAX(16));
    pI[3] = (gctINT16)gcmCLAMP(inPixel->pi.a, gcdINT_MIN(16), gcdINT_MAX(16));
}

void _WritePixelTo_A16B16G16R16_I_2_A8R8G8B8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctINT16* pI0 = (gctINT16*)outAddr[0];
    gctINT16* pI1 = (gctINT16*)outAddr[1];

    pI0[0] = (gctINT16)gcmCLAMP(inPixel->pi.r, gcdINT_MIN(16), gcdINT_MAX(16));
    pI0[1] = (gctINT16)gcmCLAMP(inPixel->pi.g, gcdINT_MIN(16), gcdINT_MAX(16));
    pI1[0] = (gctINT16)gcmCLAMP(inPixel->pi.b, gcdINT_MIN(16), gcdINT_MAX(16));
    pI1[1] = (gctINT16)gcmCLAMP(inPixel->pi.a, gcdINT_MIN(16), gcdINT_MAX(16));
}

void _WritePixelTo_X16B16G16R16_I(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctINT16* pI = (gctINT16*)outAddr[0];

    pI[0] = (gctINT16)gcmCLAMP(inPixel->pi.r, gcdINT_MIN(16), gcdINT_MAX(16));
    pI[1] = (gctINT16)gcmCLAMP(inPixel->pi.g, gcdINT_MIN(16), gcdINT_MAX(16));
    pI[2] = (gctINT16)gcmCLAMP(inPixel->pi.b, gcdINT_MIN(16), gcdINT_MAX(16));
    pI[3] = (gctINT16)1;
}


void _WritePixelTo_A16B16G16R16_UI(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT16* pI = (gctUINT16*)outAddr[0];

    pI[0] = (gctUINT16)gcmMIN(inPixel->pui.r, gcdUINT_MAX(16));
    pI[1] = (gctUINT16)gcmMIN(inPixel->pui.g, gcdUINT_MAX(16));
    pI[2] = (gctUINT16)gcmMIN(inPixel->pui.b, gcdUINT_MAX(16));
    pI[3] = (gctUINT16)gcmMIN(inPixel->pui.a, gcdUINT_MAX(16));
}

void _WritePixelTo_A16B16G16R16_UI_2_A8R8G8B8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT16* pI0 = (gctUINT16*)outAddr[0];
    gctUINT16* pI1 = (gctUINT16*)outAddr[1];

    pI0[0] = (gctUINT16)gcmMIN(inPixel->pui.r, gcdUINT_MAX(16));
    pI0[1] = (gctUINT16)gcmMIN(inPixel->pui.g, gcdUINT_MAX(16));
    pI1[0] = (gctUINT16)gcmMIN(inPixel->pui.b, gcdUINT_MAX(16));
    pI1[1] = (gctUINT16)gcmMIN(inPixel->pui.a, gcdUINT_MAX(16));
}

void _WritePixelTo_X16B16G16R16_UI(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT16* pI = (gctUINT16*)outAddr[0];

    pI[0] = (gctUINT16)gcmMIN(inPixel->pui.r, gcdUINT_MAX(16));
    pI[1] = (gctUINT16)gcmMIN(inPixel->pui.g, gcdUINT_MAX(16));
    pI[2] = (gctUINT16)gcmMIN(inPixel->pui.b, gcdUINT_MAX(16));
    pI[3] = (gctUINT16)1;
}

void _WritePixelTo_A16B16G16R16F(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT16* pI = (gctUINT16*)outAddr[0];

    pI[0] = (gctUINT16)gcoMATH_FloatToFloat16(*(gctUINT32*)&inPixel->pf.r);
    pI[1] = (gctUINT16)gcoMATH_FloatToFloat16(*(gctUINT32*)&inPixel->pf.g);
    pI[2] = (gctUINT16)gcoMATH_FloatToFloat16(*(gctUINT32*)&inPixel->pf.b);
    pI[3] = (gctUINT16)gcoMATH_FloatToFloat16(*(gctUINT32*)&inPixel->pf.a);
}

void _WritePixelTo_A16B16G16R16F_2_A8R8G8B8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT16* pI0 = (gctUINT16*)outAddr[0];
    gctUINT16* pI1 = (gctUINT16*)outAddr[1];

    pI0[0] = (gctUINT16)gcoMATH_FloatToFloat16(*(gctUINT32*)&inPixel->pf.r);
    pI0[1] = (gctUINT16)gcoMATH_FloatToFloat16(*(gctUINT32*)&inPixel->pf.g);
    pI1[0] = (gctUINT16)gcoMATH_FloatToFloat16(*(gctUINT32*)&inPixel->pf.b);
    pI1[1] = (gctUINT16)gcoMATH_FloatToFloat16(*(gctUINT32*)&inPixel->pf.a);
}

void _WritePixelTo_B16G16R16F_2_A8R8G8B8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT16* pI0 = (gctUINT16*)outAddr[0];
    gctUINT16* pI1 = (gctUINT16*)outAddr[1];

    pI0[0] = (gctUINT16)gcoMATH_FloatToFloat16(*(gctUINT32*)&inPixel->pf.r);
    pI0[1] = (gctUINT16)gcoMATH_FloatToFloat16(*(gctUINT32*)&inPixel->pf.g);
    pI1[0] = (gctUINT16)gcoMATH_FloatToFloat16(*(gctUINT32*)&inPixel->pf.b);
    pI1[1] = (gctUINT16)1;
}

void _WritePixelTo_A16B16G16R16F_2_A8R8B8G8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT16* pI0 = (gctUINT16*)outAddr[0];
    gctUINT16* pI1 = (gctUINT16*)outAddr[1];

    pI0[0] = (gctUINT16)gcoMATH_FloatToFloat16(*(gctUINT32*)&inPixel->pf.r);
    pI0[1] = (gctUINT16)gcoMATH_FloatToFloat16(*(gctUINT32*)&inPixel->pf.g);
    pI1[0] = (gctUINT16)gcoMATH_FloatToFloat16(*(gctUINT32*)&inPixel->pf.b);
    pI1[1] = (gctUINT16)gcoMATH_FloatToFloat16(*(gctUINT32*)&inPixel->pf.a);
}

void _WritePixelTo_X16B16G16R16F(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT16* pI = (gctUINT16*)outAddr[0];
    gctFLOAT alpha = 1.0f;

    pI[0] = (gctUINT16)gcoMATH_FloatToFloat16(*(gctUINT32*)&inPixel->pf.r);
    pI[1] = (gctUINT16)gcoMATH_FloatToFloat16(*(gctUINT32*)&inPixel->pf.g);
    pI[2] = (gctUINT16)gcoMATH_FloatToFloat16(*(gctUINT32*)&inPixel->pf.b);
    pI[3] = (gctUINT16)gcoMATH_FloatToFloat16(*(gctUINT32*)&alpha);
}

void _WritePixelTo_A32B32G32R32_I(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctINT32* pI = (gctINT32*)outAddr[0];

    pI[0] = (gctINT32)(inPixel->pi.r);
    pI[1] = (gctINT32)(inPixel->pi.g);
    pI[2] = (gctINT32)(inPixel->pi.b);
    pI[3] = (gctINT32)(inPixel->pi.a);
}

void _WritePixelTo_X32B32G32R32_I(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctINT32* pI = (gctINT32*)outAddr[0];

    pI[0] = (gctINT32)(inPixel->pi.r);
    pI[1] = (gctINT32)(inPixel->pi.g);
    pI[2] = (gctINT32)(inPixel->pi.b);
    pI[3] = (gctINT32)(1);
}

void _WritePixelTo_A32B32G32R32_I_2_G32R32I(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctINT32* pI0 = (gctINT32*)outAddr[0];
    gctINT32* pI1 = (gctINT32*)outAddr[1];

    pI0[0] = (gctINT32)(inPixel->pi.r);
    pI0[1] = (gctINT32)(inPixel->pi.g);
    pI1[0] = (gctINT32)(inPixel->pi.b);
    pI1[1] = (gctINT32)(inPixel->pi.a);
}

void _WritePixelTo_A32B32G32R32_I_4_A8R8G8B8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctINT32* pI0 = (gctINT32*)outAddr[0];
    gctINT32* pI1 = (gctINT32*)outAddr[1];
    gctINT32* pI2 = (gctINT32*)outAddr[2];
    gctINT32* pI3 = (gctINT32*)outAddr[3];

    pI0[0] = (gctINT32)(inPixel->pi.r);
    pI1[0] = (gctINT32)(inPixel->pi.g);
    pI2[0] = (gctINT32)(inPixel->pi.b);
    pI3[0] = (gctINT32)(inPixel->pi.a);
}

void _WritePixelTo_X32B32G32R32_I_2_G32R32I(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctINT32* pI0 = (gctINT32*)outAddr[0];
    gctINT32* pI1 = (gctINT32*)outAddr[1];

    pI0[0] = (gctINT32)(inPixel->pi.r);
    pI0[1] = (gctINT32)(inPixel->pi.g);
    pI1[0] = (gctINT32)(inPixel->pi.b);
    pI1[1] = (gctINT32)(1);
}

void _WritePixelTo_A32B32G32R32_UI(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT32* pI = (gctUINT32*)outAddr[0];

    pI[0] = (gctUINT32)(inPixel->pui.r);
    pI[1] = (gctUINT32)(inPixel->pui.g);
    pI[2] = (gctUINT32)(inPixel->pui.b);
    pI[3] = (gctUINT32)(inPixel->pui.a);
}

void _WritePixelTo_X32B32G32R32_UI(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT32* pI = (gctUINT32*)outAddr[0];

    pI[0] = (gctUINT32)(inPixel->pui.r);
    pI[1] = (gctUINT32)(inPixel->pui.g);
    pI[2] = (gctUINT32)(inPixel->pui.b);
    pI[3] = (gctUINT32)(1);
}

void _WritePixelTo_A32B32G32R32_UI_2_G32R32UI(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT32* pI0 = (gctUINT32*)outAddr[0];
    gctUINT32* pI1 = (gctUINT32*)outAddr[1];

    pI0[0] = (gctUINT32)(inPixel->pui.r);
    pI0[1] = (gctUINT32)(inPixel->pui.g);
    pI1[0] = (gctUINT32)(inPixel->pui.b);
    pI1[1] = (gctUINT32)(inPixel->pui.a);
}

void _WritePixelTo_A32B32G32R32_UI_4_A8R8G8B8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT32* pI0 = (gctUINT32*)outAddr[0];
    gctUINT32* pI1 = (gctUINT32*)outAddr[1];
    gctUINT32* pI2 = (gctUINT32*)outAddr[2];
    gctUINT32* pI3 = (gctUINT32*)outAddr[3];

    pI0[0] = (gctUINT32)(inPixel->pui.r);
    pI1[0] = (gctUINT32)(inPixel->pui.g);
    pI2[0] = (gctUINT32)(inPixel->pui.b);
    pI3[0] = (gctUINT32)(inPixel->pui.a);
}

void _WritePixelTo_X32B32G32R32_UI_2_G32R32UI(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT32* pI0 = (gctUINT32*)outAddr[0];
    gctUINT32* pI1 = (gctUINT32*)outAddr[1];

    pI0[0] = (gctUINT32)(inPixel->pui.r);
    pI0[1] = (gctUINT32)(inPixel->pui.g);
    pI1[0] = (gctUINT32)(inPixel->pui.b);
    pI1[1] = (gctUINT32)(1);
}

void _WritePixelTo_A32B32G32R32F(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctFLOAT* pI = (gctFLOAT*)outAddr[0];

    pI[0] = (gctFLOAT)(inPixel->pf.r);
    pI[1] = (gctFLOAT)(inPixel->pf.g);
    pI[2] = (gctFLOAT)(inPixel->pf.b);
    pI[3] = (gctFLOAT)(inPixel->pf.a);
}

void _WritePixelTo_A32B32G32R32F_2_G32R32F(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctFLOAT* pI0 = (gctFLOAT*)outAddr[0];
    gctFLOAT* pI1 = (gctFLOAT*)outAddr[1];

    pI0[0] = (gctFLOAT)(inPixel->pf.r);
    pI0[1] = (gctFLOAT)(inPixel->pf.g);
    pI1[0] = (gctFLOAT)(inPixel->pf.b);
    pI1[1] = (gctFLOAT)(inPixel->pf.a);
}

void _WritePixelTo_A32B32G32R32F_4_A8R8G8B8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctFLOAT* pI0 = (gctFLOAT*)outAddr[0];
    gctFLOAT* pI1 = (gctFLOAT*)outAddr[1];
    gctFLOAT* pI2 = (gctFLOAT*)outAddr[2];
    gctFLOAT* pI3 = (gctFLOAT*)outAddr[3];

    pI0[0] = (gctFLOAT)(inPixel->pf.r);
    pI1[0] = (gctFLOAT)(inPixel->pf.g);
    pI2[0] = (gctFLOAT)(inPixel->pf.b);
    pI3[0] = (gctFLOAT)(inPixel->pf.a);
}

void _WritePixelTo_X32B32G32R32F_2_G32R32F(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctFLOAT* pI0 = (gctFLOAT*)outAddr[0];
    gctFLOAT* pI1 = (gctFLOAT*)outAddr[1];

    pI0[0] = (gctFLOAT)(inPixel->pf.r);
    pI0[1] = (gctFLOAT)(inPixel->pf.g);
    pI1[0] = (gctFLOAT)(inPixel->pf.b);
    pI1[1] = (gctFLOAT)(1.0f);
}

void _WritePixelTo_X32B32G32R32F_4_A8R8G8B8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctFLOAT* pI0 = (gctFLOAT*)outAddr[0];
    gctFLOAT* pI1 = (gctFLOAT*)outAddr[1];
    gctFLOAT* pI2 = (gctFLOAT*)outAddr[2];
    gctFLOAT* pI3 = (gctFLOAT*)outAddr[3];

    pI0[0] = (gctFLOAT)(inPixel->pf.r);
    pI1[0] = (gctFLOAT)(inPixel->pf.g);
    pI2[0] = (gctFLOAT)(inPixel->pf.b);
    pI3[0] = (gctFLOAT)(1.0f);
}

void _WritePixelTo_A2B10G10R10_UI(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT32 r = gcmMIN(inPixel->pui.r, gcdUINT_MAX(10));
    gctUINT32 g = gcmMIN(inPixel->pui.g, gcdUINT_MAX(10));
    gctUINT32 b = gcmMIN(inPixel->pui.b, gcdUINT_MAX(10));
    gctUINT32 a = gcmMIN(inPixel->pui.a, gcdUINT_MAX(2));

    *(gctUINT32*)outAddr[0] = (gctUINT32)((a << 30) | (b << 20) | (g << 10) | r);
}

void _WritePixelTo_B10G11R11F(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctUINT32* pI = (gctUINT32*)outAddr[0];
    gctUINT16 r,g,b;

    r = (gctUINT16)gcoMATH_FloatToFloat11(*(gctUINT32*)&inPixel->pf.r);
    g = (gctUINT16)gcoMATH_FloatToFloat11(*(gctUINT32*)&inPixel->pf.g);
    b = (gctUINT16)gcoMATH_FloatToFloat10(*(gctUINT32*)&inPixel->pf.b);

    pI[0] = (b << 22) | (g << 11) | r;
}

/* (gctINT32)gcoMATH_Log2() can achieve same func, but has precision issue.
** e.g. gcoMATH_Log2(32768) = 14.999999... and become 14 after floor.
** Actually it should be 15.
*/
gctINT32 _FloorLog2(gctFLOAT val)
{
    gctINT32 exp = 0;
    gctUINT64 base = 1;

    while (val >= (gctFLOAT)base)
    {
        base = (gctUINT64)1 << (++exp);
    }

    return exp - 1;
}

void _WritePixelTo_E5B9G9R9(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    const gctINT32 mBits = 9;
    const gctINT32 eBits = 5;
    const gctINT32 eBias = 15;
    const gctINT32 eMax  = (1 << eBits) - 1;
    const gctFLOAT sharedExpMax = ((1 << mBits) - 1) * (1 << (eMax - eBias)) / (gctFLOAT)(1 << mBits);

    gctFLOAT rc   = gcmCLAMP(inPixel->pf.r, 0.0f, sharedExpMax);
    gctFLOAT gc   = gcmCLAMP(inPixel->pf.g, 0.0f, sharedExpMax);
    gctFLOAT bc   = gcmCLAMP(inPixel->pf.b, 0.0f, sharedExpMax);
    gctFLOAT maxc = gcoMATH_MAX(rc, gcoMATH_MAX(gc, bc));

    gctINT32 expp  = gcoMATH_MAX(-eBias - 1, _FloorLog2(maxc)) + 1 + eBias;
    gctFLOAT scale = (gctFLOAT)gcoMATH_Power(2.0f, (gctFLOAT)(expp - eBias - mBits));
    gctINT32 maxs  = (gctINT32)(maxc / scale + 0.5f);

    gctUINT32 exps = (maxs == (1 << mBits)) ? (gctUINT32)(expp + 1) : (gctUINT32)expp;
    gctUINT32 rs = gcmMIN((gctUINT32)(rc / scale + 0.5f), gcdUINT_MAX(9));
    gctUINT32 gs = gcmMIN((gctUINT32)(gc / scale + 0.5f), gcdUINT_MAX(9));
    gctUINT32 bs = gcmMIN((gctUINT32)(bc / scale + 0.5f), gcdUINT_MAX(9));

    *(gctUINT32*)outAddr[0] = rs | (gs << 9) | (bs << 18) | (exps << 27);
}

void _WritePixelTo_S8D32F_1_G32R32F(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctFLOAT* pF = (gctFLOAT *)outAddr[0];
    pF[0] = gcmCLAMP(inPixel->pf.d, 0.0f, 1.0f);
    pF[1] = inPixel->pf.s;
}

void _WritePixelTo_S8D32F_2_A8R8G8B8(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags)
{
    gctFLOAT* pF0 = (gctFLOAT *)outAddr[0];
    gctFLOAT* pF1 = (gctFLOAT *)outAddr[1];
    pF0[0] = gcmCLAMP(inPixel->pf.d, 0.0f, 1.0f);
    pF1[0] = inPixel->pf.s;
}


_PFNwritePixel gcoSURF_GetWritePixelFunc(gcoSURF surf)
{
    switch (surf->info.format)
    {
    case gcvSURF_A8:
        return _WritePixelTo_A8;
    case gcvSURF_L8:
        return _WritePixelTo_L8;
    case gcvSURF_R8:
        return _WritePixelTo_R8;
    case gcvSURF_R8_1_X8R8G8B8:
        return _WritePixelTo_R8_1_X8R8G8B8;
    case gcvSURF_A8L8:
        return _WritePixelTo_A8L8;
    case gcvSURF_G8R8:
        return _WritePixelTo_G8R8;
    case gcvSURF_G8R8_1_X8R8G8B8:
        return _WritePixelTo_G8R8_1_X8R8G8B8;
    case gcvSURF_R5G6B5:
        return _WritePixelTo_R5G6B5;
    case gcvSURF_X4R4G4B4:
        return _WritePixelTo_X4R4G4B4;
    case gcvSURF_A4R4G4B4:
        return _WritePixelTo_A4R4G4B4;
    case gcvSURF_R4G4B4A4:
        return _WritePixelTo_R4G4B4A4;
    case gcvSURF_X1R5G5B5:
        return _WritePixelTo_X1R5G5B5;
    case gcvSURF_A1R5G5B5:
        return _WritePixelTo_A1R5G5B5;
    case gcvSURF_X8R8G8B8:
        return _WritePixelTo_X8R8G8B8;
    case gcvSURF_A8B8G8R8:
        return _WritePixelTo_A8B8G8R8;
    case gcvSURF_X8B8G8R8:
        return _WritePixelTo_X8B8G8R8;
    case gcvSURF_A8R8G8B8:
        return _WritePixelTo_A8R8G8B8;
    case gcvSURF_X2B10G10R10:
        return _WritePixelTo_X2B10G10R10;
    case gcvSURF_A2B10G10R10:
        return _WritePixelTo_A2B10G10R10;
    case gcvSURF_D16:
        return _WritePixelTo_D16;
    case gcvSURF_D24X8:
        return _WritePixelTo_D24X8;
    case gcvSURF_D24S8:
        return _WritePixelTo_D24S8;
    case gcvSURF_D32:
        return _WritePixelTo_D32;
    case gcvSURF_D32F:
        return _WritePixelTo_D32F;

    case gcvSURF_D24S8_1_A8R8G8B8:
        return _WritePixelTo_D24S8;

    case gcvSURF_S8:
        return _WritePixelTo_S8;

    /* SNORM format. */
    case gcvSURF_R8_SNORM:
        return _WritePixelTo_R8_SNORM;
    case gcvSURF_G8R8_SNORM:
        return _WritePixelTo_G8R8_SNORM;
    case gcvSURF_B8G8R8_SNORM:
        return _WritePixelTo_B8G8R8_SNORM;
    case gcvSURF_A8B8G8R8_SNORM:
        return _WritePixelTo_A8B8G8R8_SNORM;
    case gcvSURF_X8B8G8R8_SNORM:
        return _WritePixelTo_X8B8G8R8_SNORM;


    /* Integer format. */
    case gcvSURF_R8I:
    case gcvSURF_R8I_1_A4R4G4B4:
        return _WritePixelTo_R8_I;

    case gcvSURF_R8UI:
    case gcvSURF_R8UI_1_A4R4G4B4:
        return _WritePixelTo_R8_UI;

    case gcvSURF_R16I:
    case gcvSURF_R16I_1_A4R4G4B4:
        return _WritePixelTo_R16_I;

    case gcvSURF_R16UI:
    case gcvSURF_R16UI_1_A4R4G4B4:
        return _WritePixelTo_R16_UI;

    case gcvSURF_R32I:
    case gcvSURF_R32I_1_A8R8G8B8:
        return _WritePixelTo_R32_I;

    case gcvSURF_R32UI:
    case gcvSURF_R32UI_1_A8R8G8B8:
        return _WritePixelTo_R32_UI;

    case gcvSURF_G8R8I:
    case gcvSURF_G8R8I_1_A4R4G4B4:
        return _WritePixelTo_G8R8_I;

    case gcvSURF_G8R8UI:
    case gcvSURF_G8R8UI_1_A4R4G4B4:
        return _WritePixelTo_G8R8_UI;

    case gcvSURF_G16R16I:
    case gcvSURF_G16R16I_1_A8R8G8B8:
        return _WritePixelTo_G16R16_I;

    case gcvSURF_G16R16UI:
    case gcvSURF_G16R16UI_1_A8R8G8B8:
        return _WritePixelTo_G16R16_UI;

    case gcvSURF_G32R32I:
        return _WritePixelTo_G32R32_I;

    case gcvSURF_G32R32I_2_A8R8G8B8:
        return _WritePixelTo_G32R32_I_2_A8R8G8B8;

    case gcvSURF_G32R32UI:
        return _WritePixelTo_G32R32_UI;

    case gcvSURF_G32R32UI_2_A8R8G8B8:
        return _WritePixelTo_G32R32_UI_2_A8R8G8B8;

    case gcvSURF_B8G8R8I:
    case gcvSURF_B8G8R8I_1_A8R8G8B8:
        return _WritePixelTo_B8G8R8_I;

    case gcvSURF_B8G8R8UI:
    case gcvSURF_B8G8R8UI_1_A8R8G8B8:
        return _WritePixelTo_B8G8R8_UI;

    case gcvSURF_B16G16R16I:
        return _WritePixelTo_B16G16R16_I;

    case gcvSURF_B16G16R16I_2_A8R8G8B8:
        return _WritePixelTo_B16G16R16_I_2_A8R8G8B8;

    case gcvSURF_B16G16R16UI:
        return _WritePixelTo_B16G16R16_UI;

    case gcvSURF_B16G16R16UI_2_A8R8G8B8:
        return _WritePixelTo_B16G16R16_UI_2_A8R8G8B8;

    case gcvSURF_B32G32R32I:
        return _WritePixelTo_B32G32R32_I;

    case gcvSURF_B32G32R32I_3_A8R8G8B8:
        return _WritePixelTo_B32G32R32_I_3_A8R8G8B8;

    case gcvSURF_B32G32R32UI:
        return _WritePixelTo_B32G32R32_UI;

    case gcvSURF_B32G32R32UI_3_A8R8G8B8:
        return _WritePixelTo_B32G32R32_UI_3_A8R8G8B8;

    case gcvSURF_A8B8G8R8I:
    case gcvSURF_A8B8G8R8I_1_A8R8G8B8:
        return _WritePixelTo_A8B8G8R8_I;

    case gcvSURF_X8B8G8R8I:
        return _WritePixelTo_X8B8G8R8_I;

    case gcvSURF_A8B8G8R8UI:
    case gcvSURF_A8B8G8R8UI_1_A8R8G8B8:
        return _WritePixelTo_A8B8G8R8_UI;

    case gcvSURF_X8B8G8R8UI:
        return _WritePixelTo_X8B8G8R8_UI;

    case gcvSURF_A16B16G16R16I:
        return _WritePixelTo_A16B16G16R16_I;

    case gcvSURF_A16B16G16R16I_2_A8R8G8B8:
        return _WritePixelTo_A16B16G16R16_I_2_A8R8G8B8;

    case gcvSURF_X16B16G16R16I:
        return _WritePixelTo_X16B16G16R16_I;

    case gcvSURF_A16B16G16R16UI:
        return _WritePixelTo_A16B16G16R16_UI;

    case gcvSURF_A16B16G16R16UI_2_A8R8G8B8:
        return _WritePixelTo_A16B16G16R16_UI_2_A8R8G8B8;

    case gcvSURF_X16B16G16R16UI:
        return _WritePixelTo_X16B16G16R16_UI;

    case gcvSURF_A32B32G32R32I:
        return _WritePixelTo_A32B32G32R32_I;

    case gcvSURF_A32B32G32R32I_2_G32R32I:
        return _WritePixelTo_A32B32G32R32_I_2_G32R32I;

    case gcvSURF_A32B32G32R32I_4_A8R8G8B8:
        return _WritePixelTo_A32B32G32R32_I_4_A8R8G8B8;

    case gcvSURF_X32B32G32R32I:
        return _WritePixelTo_X32B32G32R32_I;
    case gcvSURF_X32B32G32R32I_2_G32R32I:
        return _WritePixelTo_X32B32G32R32_I_2_G32R32I;

    case gcvSURF_A32B32G32R32UI:
        return _WritePixelTo_A32B32G32R32_UI;
    case gcvSURF_A32B32G32R32UI_2_G32R32UI:
        return _WritePixelTo_A32B32G32R32_UI_2_G32R32UI;
    case gcvSURF_A32B32G32R32UI_4_A8R8G8B8:
        return _WritePixelTo_A32B32G32R32_UI_4_A8R8G8B8;

    case gcvSURF_X32B32G32R32UI:
        return _WritePixelTo_X32B32G32R32_UI;
    case gcvSURF_X32B32G32R32UI_2_G32R32UI:
        return _WritePixelTo_X32B32G32R32_UI_2_G32R32UI;

    case gcvSURF_A2B10G10R10UI:
    case gcvSURF_A2B10G10R10UI_1_A8R8G8B8:
        return _WritePixelTo_A2B10G10R10_UI;

    case gcvSURF_SBGR8:
        return _WritePixelTo_B8G8R8;
    case gcvSURF_A8_SBGR8:
        return _WritePixelTo_A8B8G8R8;

    case gcvSURF_X8_SBGR8:
        return _WritePixelTo_X8B8G8R8;

    /* Floating format */
    case gcvSURF_R16F:
    case gcvSURF_R16F_1_A4R4G4B4:
        return _WritePixelTo_R16F;

    case gcvSURF_G16R16F:
    case gcvSURF_G16R16F_1_A8R8G8B8:
        return _WritePixelTo_G16R16F;

    case gcvSURF_A16B16G16R16F:
        return _WritePixelTo_A16B16G16R16F;

    case gcvSURF_A16B16G16R16F_2_A8R8G8B8:
        return _WritePixelTo_A16B16G16R16F_2_A8R8G8B8;

    case gcvSURF_X16B16G16R16F_2_A8R8G8B8:
    case gcvSURF_B16G16R16F_2_A8R8G8B8:
        return _WritePixelTo_B16G16R16F_2_A8R8G8B8;

    case gcvSURF_X16B16G16R16F:
        return _WritePixelTo_X16B16G16R16F;

    case gcvSURF_R32F:
    case gcvSURF_R32F_1_A8R8G8B8:
        return _WritePixelTo_R32F;

    case gcvSURF_G32R32F:
        return _WritePixelTo_G32R32F;

    case gcvSURF_G32R32F_2_A8R8G8B8:
        return _WritePixelTo_G32R32F_2_A8R8G8B8;

    case gcvSURF_A32B32G32R32F:
        return _WritePixelTo_A32B32G32R32F;

    case gcvSURF_A32B32G32R32F_2_G32R32F:
        return _WritePixelTo_A32B32G32R32F_2_G32R32F;

    case gcvSURF_A32B32G32R32F_4_A8R8G8B8:
        return _WritePixelTo_A32B32G32R32F_4_A8R8G8B8;

    case gcvSURF_X32B32G32R32F_2_G32R32F:
        return _WritePixelTo_X32B32G32R32F_2_G32R32F;

    case gcvSURF_X32B32G32R32F_4_A8R8G8B8:
        return _WritePixelTo_X32B32G32R32F_4_A8R8G8B8;

    case gcvSURF_B10G11R11F:
    case gcvSURF_B10G11R11F_1_A8R8G8B8:
        return _WritePixelTo_B10G11R11F;

    case gcvSURF_E5B9G9R9:
        return _WritePixelTo_E5B9G9R9;

    case gcvSURF_S8D32F_1_G32R32F:
        return _WritePixelTo_S8D32F_1_G32R32F;
    case gcvSURF_S8D32F_2_A8R8G8B8:
        return _WritePixelTo_S8D32F_2_A8R8G8B8;

    case gcvSURF_A8B12G12R12_2_A8R8G8B8:
        return _WritePixelTo_A8B12G12R12_2_A8R8G8B8;

    default:
        gcmASSERT(0);
    }

    return gcvNULL;
}


/*
** Linear -> Non-linear conversion for input pixel
*/

gctFLOAT _LinearToNonLinearConv(gctFLOAT lFloat)
{
    gctFLOAT sFloat;
    if (lFloat <= 0.0f)
    {
        sFloat = 0.0f;
    }
    else if (lFloat < 0.0031308f)
    {
        sFloat = 12.92f * lFloat;
    }
    else if (lFloat < 1.0)
    {
        sFloat = 1.055f * gcoMATH_Power(lFloat, 0.41666f) -0.055f;
    }
    else
    {
        sFloat = 1.0f;
    }

    return sFloat;
}

void gcoSURF_PixelToNonLinear(gcsPIXEL* inPixel)
{
    gctFLOAT sR, sG, sB, sA;
    sR = _LinearToNonLinearConv(inPixel->pf.r);
    sG = _LinearToNonLinearConv(inPixel->pf.g);
    sB = _LinearToNonLinearConv(inPixel->pf.b);
    sA = inPixel->pf.a;

    inPixel->pf.r = gcmCLAMP(sR, 0.0f, 1.0f);
    inPixel->pf.g = gcmCLAMP(sG, 0.0f, 1.0f);
    inPixel->pf.b = gcmCLAMP(sB, 0.0f, 1.0f);
    inPixel->pf.a = gcmCLAMP(sA, 0.0f, 1.0f);
}

/*
** Non-linear to Linear conversion for input pixel
*/
void gcoSURF_PixelToLinear(gcsPIXEL* inPixel)
{
    gctFLOAT sR, sG, sB, sA;

    sR = gcmCLAMP(inPixel->pf.r, 0.0f, 1.0f);
    sG = gcmCLAMP(inPixel->pf.g, 0.0f, 1.0f);
    sB = gcmCLAMP(inPixel->pf.b, 0.0f, 1.0f);
    sA = gcmCLAMP(inPixel->pf.a, 0.0f, 1.0f);

    inPixel->pf.r = (sR <= 0.04045f) ? (sR / 12.92f) : gcoMATH_Power((sR + 0.055f) / 1.055f, 2.4f);
    inPixel->pf.g = (sG <= 0.04045f) ? (sG / 12.92f) : gcoMATH_Power((sG + 0.055f) / 1.055f, 2.4f);
    inPixel->pf.b = (sB <= 0.04045f) ? (sB / 12.92f) : gcoMATH_Power((sB + 0.055f) / 1.055f, 2.4f);
    inPixel->pf.a = sA;
}

#endif /* gcdENABLE_3D */
