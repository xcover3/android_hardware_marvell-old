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

#include "ARGB8888ToYUV.h"

/*
// Color Conversion:
//    RGB -> YUV
//    RGB -> YUV420
//    RGB -> YUV422
//
//    Y' =  0.299*R' + 0.587*G' + 0.114*B'
//    U  = -0.147*R' - 0.289*G' + 0.436*B' = 0.492*(B' - Y' )
//    V  =  0.615*R' - 0.515*G' - 0.100*B' = 0.877*(R' - Y' )
*/
extern GPU_CSC_FORMULA g_Formula;
extern Ipp32s RGB_YUV[];
extern Ipp16s RGB_YUV_Q10[];
extern Ipp16s RGB_YUV_GC[];

/*
// Saturate table for depth 8u
// saturated value range is [-376..751]
*/
static Ipp8u saturTable[] = {
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

#define saturTableStartOffset 376
Ipp8u* SAT_DK = saturTable + saturTableStartOffset;

/** Clip function ensures values with range of 0 and 255 */
//#define CLIP(x)  ((x) < 0 ? 0 : ((x) > 255 ? 255 : (x)))

/*
//  Function = ARGB8888_YUV420_8u_C3P3R
//  Lib = PX
//  Lib = S1
//  Lib = S2
*/
void gpu_csc_ARGBToI420(const Ipp8u* pSrc, int srcStep,
                        Ipp8u* pDst[3], int dstStep[3],
                        int width, int height)
{
    int dstStepY  = dstStep[0];
    int dstStepU  = dstStep[1];
    int dstStepV  = dstStep[2];

    Ipp8u* pDstY0 = pDst[0];
    Ipp8u* pDstU  = pDst[1];
    Ipp8u* pDstV  = pDst[2];
    Ipp8u* pDstY1 = pDstY0+dstStepY;

    Ipp8u* pSrc0  = (Ipp8u*)pSrc;
    Ipp8u* pSrc1  = pSrc0+srcStep;

    int w, h;
    srcStep  *= 2;
    dstStepY *= 2;

    /* if we use fixed point formula */
    if(g_Formula & GPU_CSC_FORMULA_BT601)
    {
        for(h = height & ~1;  h > 0;  h -= 2)
        {
            Ipp8u* src0  = pSrc0;
            Ipp8u* src1  = pSrc1;
            Ipp8u* dsty0 = pDstY0;
            Ipp8u* dsty1 = pDstY1;
            Ipp8u* dstU  = pDstU;
            Ipp8u* dstV  = pDstV;
            for(w = width & ~1;  w > 0;  w -= 2)
            {
                Ipp32s R, B;
                Ipp32s y, ysum;
                Ipp32s k0 = RGB_YUV[0];
                Ipp32s k1 = RGB_YUV[1];
                Ipp32s k2 = RGB_YUV[2];

                y  = k0 * (Ipp32s)src0[2];
                y += k1 * (Ipp32s)src0[1];
                y += k2 * (Ipp32s)src0[0];
                dsty0[0] = (Ipp8u)(y >> 16);
                ysum  = y;

                y  = k0 * (Ipp32s)src0[6];
                y += k1 * (Ipp32s)src0[5];
                y += k2 * (Ipp32s)src0[4];
                dsty0[1] = (Ipp8u)(y >> 16);
                ysum += y;

                y  = k0 * (Ipp32s)src1[2];
                y += k1 * (Ipp32s)src1[1];
                y += k2 * (Ipp32s)src1[0];
                dsty1[0] = (Ipp8u)(y >> 16);
                ysum += y;

                y  = k0 * (Ipp32s)src1[6];
                y += k1 * (Ipp32s)src1[5];
                y += k2 * (Ipp32s)src1[4];
                dsty1[1] = (Ipp8u)(y >> 16);
                ysum += y;

                R  = (Ipp32s)src0[2];
                B  = (Ipp32s)src0[0];
                R += (Ipp32s)src0[6];
                B += (Ipp32s)src0[4];
                R += (Ipp32s)src1[2];
                B += (Ipp32s)src1[0];
                R += (Ipp32s)src1[6];
                B += (Ipp32s)src1[4];

                k0 = RGB_YUV[3];
                k1 = RGB_YUV[4];

                *dstU++ = (Ipp8u)       (((k0 * ((((B << 16) - ysum) + 0x20000) >> 16)) >> 18) + 128);
                *dstV++ = (Ipp8u)SAT_DK[(((k1 * ((((R << 16) - ysum) + 0x20000) >> 16)) >> 18) + 128)];
                dsty0  += 2;
                dsty1  += 2;
                src0   += 8;
                src1   += 8;
            }
            if(width & 1)
            {
                Ipp32s r0 = (Ipp32s)src0[2];
                Ipp32s r1 = (Ipp32s)src1[2];
                Ipp32s b0 = (Ipp32s)src0[0];
                Ipp32s b1 = (Ipp32s)src1[0];
                Ipp32s y0 = RGB_YUV[0] * r0 + RGB_YUV[1] * (Ipp32s)src0[1] + RGB_YUV[2] * b0;
                Ipp32s y1 = RGB_YUV[0] * r1 + RGB_YUV[1] * (Ipp32s)src1[1] + RGB_YUV[2] * b1;
                dsty0[0]  = (Ipp8u)(y0 >> 16);
                dsty1[0]  = (Ipp8u)(y1 >> 16);
                y0 += y1;
                r0 += r1;
                b0 += b1;
                dstU[0]   = (Ipp8u)       (((RGB_YUV[3] * ((((b0 << 16) - y0)) >> 16)) >> 17) + 128);
                dstV[0]   = (Ipp8u)SAT_DK[(((RGB_YUV[4] * ((((r0 << 16) - y0)) >> 16)) >> 17) + 128)];
            }
            pSrc0  += srcStep;
            pSrc1  += srcStep;
            pDstY0 += dstStepY;
            pDstY1 += dstStepY;
            pDstU  += dstStepU;
            pDstV  += dstStepV;
        }
        if(height & 1)
        {
            Ipp32s y0, y1;
            Ipp32s r0, r1;
            Ipp32s b0, b1;
            Ipp8u* src0  = pSrc0;
            Ipp8u* dsty0 = pDstY0;
            Ipp8u* dstU  = pDstU;
            Ipp8u* dstV  = pDstV;
            for(w = width & ~1;  w > 0;  w -= 2)
            {
                r0 = (Ipp32s)src0[2];
                r1 = (Ipp32s)src0[6];
                b0 = (Ipp32s)src0[0];
                b1 = (Ipp32s)src0[4];
                y0 = RGB_YUV[0] * r0 + RGB_YUV[1] * (Ipp32s)src0[1] + RGB_YUV[2] * b0;
                y1 = RGB_YUV[0] * r1 + RGB_YUV[1] * (Ipp32s)src0[1] + RGB_YUV[2] * b1;
                dsty0[0] = (Ipp8u)(y0 >> 16);
                dsty0[1] = (Ipp8u)(y1 >> 16);
                y0 += y1;
                r0 += r1;
                b0 += b1;
                dstU[0] = (Ipp8u)       (((RGB_YUV[3] * ((((b0 << 16) - y0)) >> 16)) >> 17) + 128);
                dstV[0] = (Ipp8u)SAT_DK[(((RGB_YUV[4] * ((((r0 << 16) - y0)) >> 16)) >> 17) + 128)];
                dsty0 += 2;
                dstU  += 1;
                dstV  += 1;
                src0  += 8;
            }
            if(width & 1)
            {
                r0 = (Ipp32s)src0[2];
                b0 = (Ipp32s)src0[0];
                y0 = RGB_YUV[0] * r0 + RGB_YUV[1] * (Ipp32s)src0[1] + RGB_YUV[2] * b0;
                dsty0[0] = (Ipp8u)(y0 >> 16);
                dstU[0]  = (Ipp8u)       (((RGB_YUV[3] * ((((b0 << 16) - y0)) >> 16)) >> 16) + 128);
                dstV[0]  = (Ipp8u)SAT_DK[(((RGB_YUV[4] * ((((r0 << 16) - y0)) >> 16)) >> 16) + 128)];
            }
        }
    }
    else /* if we use integer approximated formula */
    {
        for(h = height & ~1;  h > 0;  h -= 2)
        {
            Ipp8u* src0  = pSrc0;
            Ipp8u* src1  = pSrc1;
            Ipp8u* dsty0 = pDstY0;
            Ipp8u* dsty1 = pDstY1;
            Ipp8u* dstU  = pDstU;
            Ipp8u* dstV  = pDstV;
            for(w = width & ~1;  w > 0;  w -= 2)
            {
                Ipp32s R, G, B;
                Ipp32s y;

                y  =  128;
                y +=  RGB_YUV_GC[0] * (Ipp32s)src0[2];
                y +=  RGB_YUV_GC[1] * (Ipp32s)src0[1];
                y +=  RGB_YUV_GC[2] * (Ipp32s)src0[0];
                dsty0[0] = (Ipp8u)((y >> 8) + RGB_YUV_GC[3]);

                y  =  128;
                y +=  RGB_YUV_GC[0] * (Ipp32s)src0[6];
                y +=  RGB_YUV_GC[1] * (Ipp32s)src0[5];
                y +=  RGB_YUV_GC[2] * (Ipp32s)src0[4];
                dsty0[1] = (Ipp8u)((y >> 8) + RGB_YUV_GC[3]);

                y  =  128;
                y +=  RGB_YUV_GC[0] * (Ipp32s)src1[2];
                y +=  RGB_YUV_GC[1] * (Ipp32s)src1[1];
                y +=  RGB_YUV_GC[2] * (Ipp32s)src1[0];
                dsty1[0] = (Ipp8u)((y >> 8) + RGB_YUV_GC[3]);

                y  =  128;
                y +=  RGB_YUV_GC[0] * (Ipp32s)src1[6];
                y +=  RGB_YUV_GC[1] * (Ipp32s)src1[5];
                y +=  RGB_YUV_GC[2] * (Ipp32s)src1[4];
                dsty1[1] = (Ipp8u)((y >> 8) + RGB_YUV_GC[3]);

                R  = (Ipp32s)src0[2];
                G  = (Ipp32s)src0[1];
                B  = (Ipp32s)src0[0];
                R += (Ipp32s)src0[6];
                G += (Ipp32s)src0[5];
                B += (Ipp32s)src0[4];
                R += (Ipp32s)src1[2];
                G += (Ipp32s)src1[1];
                B += (Ipp32s)src1[0];
                R += (Ipp32s)src1[6];
                G += (Ipp32s)src1[5];
                B += (Ipp32s)src1[4];

                *dstU++ = (Ipp8u)(((RGB_YUV_GC[4] * R + RGB_YUV_GC[5] * G + RGB_YUV_GC[6]  * B + 512) >> 10) + RGB_YUV_GC[7]);
                *dstV++ = (Ipp8u)(((RGB_YUV_GC[8] * R + RGB_YUV_GC[9] * G + RGB_YUV_GC[10] * B + 512) >> 10) + RGB_YUV_GC[11]);
                dsty0  += 2;
                dsty1  += 2;
                src0   += 8;
                src1   += 8;
            }
            if(width & 1)
            {
                Ipp32s r0 = (Ipp32s)src0[2];
                Ipp32s r1 = (Ipp32s)src1[2];
                Ipp32s g0 = (Ipp32s)src0[1];
                Ipp32s g1 = (Ipp32s)src1[1];
                Ipp32s b0 = (Ipp32s)src0[0];
                Ipp32s b1 = (Ipp32s)src1[0];
                Ipp32s y0 = RGB_YUV_GC[0] * r0 + RGB_YUV_GC[1] * g0 + RGB_YUV_GC[2] * b0 + 128;
                Ipp32s y1 = RGB_YUV_GC[0] * r1 + RGB_YUV_GC[1] * g1 + RGB_YUV_GC[2] * b1 + 128;
                dsty0[0]  = (Ipp8u)((y0 >> 8) + RGB_YUV_GC[3]);
                dsty1[0]  = (Ipp8u)((y1 >> 8) + RGB_YUV_GC[3]);
                r0 += r1;
                g0 += g1;
                b0 += b1;
                dstU[0]   = (Ipp8u)(((RGB_YUV_GC[4] * r0 + RGB_YUV_GC[5] * g0 + RGB_YUV_GC[6]  * b0 + 256) >> 9) + RGB_YUV_GC[7]);
                dstV[0]   = (Ipp8u)(((RGB_YUV_GC[8] * r0 + RGB_YUV_GC[9] * g0 + RGB_YUV_GC[10] * b0 + 256) >> 9) + RGB_YUV_GC[11]);
            }
            pSrc0  += srcStep;
            pSrc1  += srcStep;
            pDstY0 += dstStepY;
            pDstY1 += dstStepY;
            pDstU  += dstStepU;
            pDstV  += dstStepV;
        }
        if(height & 1)
        {
            Ipp32s y0, y1;
            Ipp32s r0, r1;
            Ipp32s g0, g1;
            Ipp32s b0, b1;
            Ipp8u* src0  = pSrc0;
            Ipp8u* dsty0 = pDstY0;
            Ipp8u* dstU  = pDstU;
            Ipp8u* dstV  = pDstV;
            for(w = width & ~1;  w > 0;  w -= 2)
            {
                r0 = (Ipp32s)src0[2];
                r1 = (Ipp32s)src0[6];
                g0 = (Ipp32s)src0[1];
                g1 = (Ipp32s)src0[5];
                b0 = (Ipp32s)src0[0];
                b1 = (Ipp32s)src0[4];
                y0 = RGB_YUV_GC[0] * r0 + RGB_YUV_GC[1] * g0 + RGB_YUV_GC[2] * b0 + 128;
                y1 = RGB_YUV_GC[0] * r1 + RGB_YUV_GC[1] * g1 + RGB_YUV_GC[2] * b1 + 128;
                dsty0[0] = (Ipp8u)((y0 >> 8) + RGB_YUV_GC[3]);
                dsty0[1] = (Ipp8u)((y1 >> 8) + RGB_YUV_GC[3]);
                r0 += r1;
                g0 += g1;
                b0 += b1;
                dstU[0] = (Ipp8u)(((RGB_YUV_GC[4] * r0 + RGB_YUV_GC[5] * g0 + RGB_YUV_GC[6]  * b0 + 256) >> 9) + RGB_YUV_GC[7]);
                dstV[0] = (Ipp8u)(((RGB_YUV_GC[8] * r0 + RGB_YUV_GC[9] * g0 + RGB_YUV_GC[10] * b0 + 256) >> 9) + RGB_YUV_GC[11]);
                dsty0 += 2;
                dstU  += 1;
                dstV  += 1;
                src0  += 8;
            }
            if(width & 1)
            {
                r0 = (Ipp32s)src0[2];
                g0 = (Ipp32s)src0[1];
                b0 = (Ipp32s)src0[0];
                y0 = RGB_YUV_GC[0] * r0 + RGB_YUV_GC[1] * g0 + RGB_YUV_GC[2] * b0 + 128;
                dsty0[0] = (Ipp8u)((y0 >> 8) + RGB_YUV_GC[3]);
                dstU[0]  = (Ipp8u)(((RGB_YUV_GC[4] * r0 + RGB_YUV_GC[5] * g0 + RGB_YUV_GC[6]  * b0 + 128) >> 8) + RGB_YUV_GC[7]);
                dstV[0]  = (Ipp8u)(((RGB_YUV_GC[8] * r0 + RGB_YUV_GC[9] * g0 + RGB_YUV_GC[10] * b0 + 128) >> 8) + RGB_YUV_GC[11]);
            }
        }
    }
}

static void lineRGBToYUV422_Fixed(const Ipp8u* src, Ipp8u* dst, int width)
{
    int    w;
    Ipp32s r, b, y0, y1;
    int width2 = width & ~1;
    int last   = width &  1;

    for(w = 0;  w < width2;  w += 2)
    {
        y0  = RGB_YUV[0] * (Ipp32s)src[2] + RGB_YUV[1] * (Ipp32s)src[1] + RGB_YUV[2] * (Ipp32s)src[0];
        r   = (Ipp32s)src[6];
        b   = (Ipp32s)src[4];
        y1  = RGB_YUV[0] * r + RGB_YUV[1] * (Ipp32s)src[5] + RGB_YUV[2] * b;
        dst[1] = (Ipp8u)(y0 >> 16);
        dst[3] = (Ipp8u)(y1 >> 16);

        y0 += y1;
        r  += (Ipp32s)src[2];
        b  += (Ipp32s)src[0];
        dst[0] = (Ipp8u)       (((RGB_YUV[3] * ((((b << 16) - y0) + 0xffff) >> 16)) >> 17) + 128);
        dst[2] = (Ipp8u)SAT_DK[(((RGB_YUV[4] * ((((r << 16) - y0) + 0xffff) >> 16)) >> 17) + 128)];
        dst += 4;
        src += 8;
    }

    if(last)
    {
        y0 = RGB_YUV[0] * (Ipp32s)src[2] + RGB_YUV[1] * (Ipp32s)src[1] + RGB_YUV[2] * (Ipp32s)src[0];
        dst[0] = (Ipp8u)((RGB_YUV[3] * (((((Ipp32s)src[0] << 16) - y0) + 0x7fff) >> 16) >> 16) + 128);
        dst[1] = (Ipp8u)(y0 >> 16);
    }
}

static void lineRGBToYUV422_Int(const Ipp8u* src, Ipp8u* dst, int width)
{
    int    w;
    Ipp32s r, g, b, y0, y1;
    int width2 = width & ~1;
    int last   = width &  1;

    for(w = 0;  w < width2;  w += 2)
    {
        y0 = RGB_YUV_GC[0] * (Ipp32s)src[2] + RGB_YUV_GC[1] * (Ipp32s)src[1] + RGB_YUV_GC[2] * (Ipp32s)src[0] + 128;
        r  = (Ipp32s)src[6];
        g  = (Ipp32s)src[5];
        b  = (Ipp32s)src[4];
        y1 = RGB_YUV_GC[0] * r + RGB_YUV_GC[1] * g + RGB_YUV_GC[2] * b + 128;
        dst[1] = (Ipp8u)((y0 >> 8) + RGB_YUV_GC[3]);
        dst[3] = (Ipp8u)((y1 >> 8) + RGB_YUV_GC[3]);
        r += (Ipp32s)src[2];
        g += (Ipp32s)src[1];
        b += (Ipp32s)src[0];
        dst[0] = (Ipp8u)(((RGB_YUV_GC[4] * r + RGB_YUV_GC[5] * g + RGB_YUV_GC[6]  * b + 256) >> 9) + RGB_YUV_GC[7]);
        dst[2] = (Ipp8u)(((RGB_YUV_GC[8] * r + RGB_YUV_GC[9] * g + RGB_YUV_GC[10] * b + 256) >> 9) + RGB_YUV_GC[11]);
        dst += 4;
        src += 8;
    }

    if(last)
    {
        r  = (Ipp32s)src[2];
        g  = (Ipp32s)src[1];
        b  = (Ipp32s)src[0];
        y0 = RGB_YUV_GC[0] * r + RGB_YUV_GC[1] * g + RGB_YUV_GC[2] * b + 128;
        dst[0] = (Ipp8u)(((RGB_YUV_GC[4] * r + RGB_YUV_GC[5] * g + RGB_YUV_GC[6]  * b + 128) >> 8) + RGB_YUV_GC[7]);
        dst[1] = (Ipp8u)((y0 >> 8) + RGB_YUV_GC[3]);
    }
}

void gpu_csc_ARGBToUYVY(const Ipp8u* pSrc, int srcStep,
                        Ipp8u* pDst, int dstStep,
                        int width, int height)
{
    int h;
    /* if we use fixed point formula */
    if(g_Formula & GPU_CSC_FORMULA_BT601)
    {
        for(h = height;  h > 0;  --h)
        {
            lineRGBToYUV422_Fixed(pSrc, pDst, width);
            pSrc += srcStep;
            pDst += dstStep;
        }
    }
    else /* if we use integer approximated formula */
    {
        for(h = height;  h > 0;  --h)
        {
            lineRGBToYUV422_Int(pSrc, pDst, width);
            pSrc += srcStep;
            pDst += dstStep;
        }
    }
}

void gpu_csc_ARGBToNV21(const Ipp8u* pSrc, int srcStep,
                        Ipp8u* pDst[2], int dstStep[2],
                        int width, int height)
{
    int dstStepY  = dstStep[0];
    int dstStepVU = dstStep[1];


    Ipp8u* pDstY0 = pDst[0];
    Ipp8u* pDstVU = pDst[1];

    Ipp8u* pDstY1 = pDstY0 + dstStepY;

    Ipp8u* pSrc0  = (Ipp8u*)pSrc;
    Ipp8u* pSrc1  = pSrc0 + srcStep;

    int w, h;
    srcStep  *= 2;
    dstStepY *= 2;
    /* if we use fixed point formula */
    if(g_Formula & GPU_CSC_FORMULA_BT601)
    {
        for(h = height & ~1;  h > 0;  h -= 2)
        {
            Ipp8u* src0  = pSrc0;
            Ipp8u* src1  = pSrc1;
            Ipp8u* dsty0 = pDstY0;
            Ipp8u* dsty1 = pDstY1;
            Ipp8u* dstVU = pDstVU;
            for(w = width & ~1;  w > 0;  w -= 2)
            {
                Ipp32s R, B;
                Ipp32s y, ysum;
                Ipp32s k0 = RGB_YUV[0];
                Ipp32s k1 = RGB_YUV[1];
                Ipp32s k2 = RGB_YUV[2];

                y  = k0 * (Ipp32s)src0[2];
                y += k1 * (Ipp32s)src0[1];
                y += k2 * (Ipp32s)src0[0];
                dsty0[0] = (Ipp8u)(y >> 16);
                ysum = y;

                y  = k0 * (Ipp32s)src0[6];
                y += k1 * (Ipp32s)src0[5];
                y += k2 * (Ipp32s)src0[4];
                dsty0[1] = (Ipp8u)(y >> 16);
                ysum += y;

                y  = k0 * (Ipp32s)src1[2];
                y += k1 * (Ipp32s)src1[1];
                y += k2 * (Ipp32s)src1[0];
                dsty1[0] = (Ipp8u)(y >> 16);
                ysum += y;

                y  = k0 * (Ipp32s)src1[6];
                y += k1 * (Ipp32s)src1[5];
                y += k2 * (Ipp32s)src1[4];
                dsty1[1] = (Ipp8u)(y >> 16);
                ysum += y;

                R  = (Ipp32s)src0[2];
                B  = (Ipp32s)src0[0];
                R += (Ipp32s)src0[6];
                B += (Ipp32s)src0[4];
                R += (Ipp32s)src1[2];
                B += (Ipp32s)src1[0];
                R += (Ipp32s)src1[6];
                B += (Ipp32s)src1[4];

                k0 = RGB_YUV[3];
                k1 = RGB_YUV[4];

                *dstVU++ = (Ipp8u)SAT_DK[(((k1 * ((((R << 16) - ysum) + 0x20000) >> 16)) >> 18) + 128)];
                *dstVU++ = (Ipp8u)       (((k0 * ((((B << 16) - ysum) + 0x20000) >> 16)) >> 18) + 128);
                dsty0 += 2;
                dsty1 += 2;
                src0  += 8;
                src1  += 8;
            }
            if(width & 1)
            {
                Ipp32s r0 = (Ipp32s)src0[2];
                Ipp32s r1 = (Ipp32s)src1[2];
                Ipp32s b0 = (Ipp32s)src0[0];
                Ipp32s b1 = (Ipp32s)src1[0];
                Ipp32s y0 = RGB_YUV[0] * r0 + RGB_YUV[1] * (Ipp32s)src0[1] + RGB_YUV[2] * b0;
                Ipp32s y1 = RGB_YUV[0] * r1 + RGB_YUV[1] * (Ipp32s)src1[1] + RGB_YUV[2] * b1;
                dsty0[0]  = (Ipp8u)(y0 >> 16);
                dsty1[0]  = (Ipp8u)(y1 >> 16);
                y0 += y1;
                r0 += r1;
                b0 += b1;
                dstVU[0]  = (Ipp8u)SAT_DK[(((RGB_YUV[4] * ((((r0 << 16) - y0)) >> 16)) >> 17) + 128)];
                dstVU[1]  = (Ipp8u)       (((RGB_YUV[3] * ((((b0 << 16) - y0)) >> 16)) >> 17) + 128);
            }
            pSrc0  += srcStep;
            pSrc1  += srcStep;
            pDstY0 += dstStepY;
            pDstY1 += dstStepY;
            pDstVU += dstStepVU;
        }
        if(height & 1)
        {
            Ipp32s y0, y1;
            Ipp32s r0, r1;
            Ipp32s b0, b1;
            Ipp8u* src0  = pSrc0;
            Ipp8u* dsty0 = pDstY0;
            Ipp8u* dstVU = pDstVU;
            for(w = width & ~1;  w > 0;  w -= 2)
            {
                r0 = (Ipp32s)src0[2];
                r1 = (Ipp32s)src0[6];
                b0 = (Ipp32s)src0[0];
                b1 = (Ipp32s)src0[4];
                y0 = RGB_YUV[0] * r0 + RGB_YUV[1] * (Ipp32s)src0[1] + RGB_YUV[2] * b0;
                y1 = RGB_YUV[0] * r1 + RGB_YUV[1] * (Ipp32s)src0[5] + RGB_YUV[2] * b1;
                dsty0[0] = (Ipp8u)(y0 >> 16);
                dsty0[1] = (Ipp8u)(y1 >> 16);
                y0 += y1;
                r0 += r1;
                b0 += b1;
                dstVU[0] = (Ipp8u)SAT_DK[(((RGB_YUV[4] * ((((r0 << 16) - y0)) >> 16)) >> 17) + 128)];
                dstVU[1] = (Ipp8u)       (((RGB_YUV[3] * ((((b0 << 16) - y0)) >> 16)) >> 17) + 128);
                dsty0 += 2;
                dstVU += 2;
                src0  += 8;
            }
            if(width & 1)
            {
                r0 = (Ipp32s)src0[2];
                b0 = (Ipp32s)src0[0];
                y0 = RGB_YUV[0] * r0 + RGB_YUV[1] * (Ipp32s)src0[1] + RGB_YUV[2] * b0;
                dsty0[0] = (Ipp8u)(y0 >> 16);
                dstVU[0] = (Ipp8u)SAT_DK[(((RGB_YUV[4] * ((((r0 << 16) - y0)) >> 16)) >> 16) + 128)];
                dstVU[1] = (Ipp8u)       (((RGB_YUV[3] * ((((b0 << 16) - y0)) >> 16)) >> 16) + 128);
            }
        }
    }
    else /* if we use integer approximated formula */
    {
        for(h = height & ~1;  h > 0;  h -= 2)
        {
            Ipp8u* src0  = pSrc0;
            Ipp8u* src1  = pSrc1;
            Ipp8u* dsty0 = pDstY0;
            Ipp8u* dsty1 = pDstY1;
            Ipp8u* dstVU = pDstVU;
            for(w = width & ~1;  w > 0;  w -= 2)
            {
                Ipp32s R, G, B;
                Ipp32s y;
                Ipp32s k0 = RGB_YUV_GC[0];
                Ipp32s k1 = RGB_YUV_GC[1];
                Ipp32s k2 = RGB_YUV_GC[2];

                y  =  128;
                y += k0 * (Ipp32s)src0[2];
                y += k1 * (Ipp32s)src0[1];
                y += k2 * (Ipp32s)src0[0];
                dsty0[0] = (Ipp8u)((y >> 8) + RGB_YUV_GC[3]);

                y  =  128;
                y += k0 * (Ipp32s)src0[6];
                y += k1 * (Ipp32s)src0[5];
                y += k2 * (Ipp32s)src0[4];
                dsty0[1] = (Ipp8u)((y >> 8) + RGB_YUV_GC[3]);

                y  =  128;
                y += k0 * (Ipp32s)src1[2];
                y += k1 * (Ipp32s)src1[1];
                y += k2 * (Ipp32s)src1[0];
                dsty1[0] = (Ipp8u)((y >> 8) + RGB_YUV_GC[3]);

                y  =  128;
                y += k0 * (Ipp32s)src1[6];
                y += k1 * (Ipp32s)src1[5];
                y += k2 * (Ipp32s)src1[4];
                dsty1[1] = (Ipp8u)((y >> 8) + RGB_YUV_GC[3]);

                R  = (Ipp32s)src0[2];
                G  = (Ipp32s)src0[1];
                B  = (Ipp32s)src0[0];
                R += (Ipp32s)src0[6];
                G += (Ipp32s)src0[5];
                B += (Ipp32s)src0[4];
                R += (Ipp32s)src1[2];
                G += (Ipp32s)src1[1];
                B += (Ipp32s)src1[0];
                R += (Ipp32s)src1[6];
                G += (Ipp32s)src1[5];
                B += (Ipp32s)src1[4];

                *dstVU++ = (Ipp8u)(((RGB_YUV_GC[8] * R + RGB_YUV_GC[9] * G + RGB_YUV_GC[10] * B + 512) >> 10) + RGB_YUV_GC[11]);
                *dstVU++ = (Ipp8u)(((RGB_YUV_GC[4] * R + RGB_YUV_GC[5] * G + RGB_YUV_GC[6]  * B + 512) >> 10) + RGB_YUV_GC[7]);
                dsty0 += 2;
                dsty1 += 2;
                src0  += 8;
                src1  += 8;
            }
            if(width & 1)
            {
                Ipp32s r0 = (Ipp32s)src0[2];
                Ipp32s r1 = (Ipp32s)src1[2];
                Ipp32s g0 = (Ipp32s)src0[1];
                Ipp32s g1 = (Ipp32s)src1[1];
                Ipp32s b0 = (Ipp32s)src0[0];
                Ipp32s b1 = (Ipp32s)src1[0];
                Ipp32s y0 = RGB_YUV_GC[0] * r0 + RGB_YUV_GC[1] * g0 + RGB_YUV_GC[2] * b0 + 128;
                Ipp32s y1 = RGB_YUV_GC[0] * r1 + RGB_YUV_GC[1] * g1 + RGB_YUV_GC[2] * b1 + 128;
                dsty0[0]  = (Ipp8u)((y0 >> 8) + RGB_YUV_GC[3]);
                dsty1[0]  = (Ipp8u)((y1 >> 8) + RGB_YUV_GC[3]);
                r0 += r1;
                g0 += g1;
                b0 += b1;
                dstVU[0]  = (Ipp8u)(((RGB_YUV_GC[8] * r0 + RGB_YUV_GC[9] * g0 + RGB_YUV_GC[10] * b0 + 256) >> 9) + RGB_YUV_GC[11]);
                dstVU[1]  = (Ipp8u)(((RGB_YUV_GC[4] * r0 + RGB_YUV_GC[5] * g0 + RGB_YUV_GC[6]  * b0 + 256) >> 9) + RGB_YUV_GC[7]);
            }
            pSrc0  += srcStep;
            pSrc1  += srcStep;
            pDstY0 += dstStepY;
            pDstY1 += dstStepY;
            pDstVU += dstStepVU;
        }
        if(height & 1)
        {
            Ipp32s y0, y1;
            Ipp32s r0, r1;
            Ipp32s g0, g1;
            Ipp32s b0, b1;
            Ipp8u* src0  = pSrc0;
            Ipp8u* dsty0 = pDstY0;
            Ipp8u* dstVU = pDstVU;
            for(w = width & ~1;  w > 0;  w -= 2)
            {
                r0 = (Ipp32s)src0[2];
                r1 = (Ipp32s)src0[6];
                g0 = (Ipp32s)src0[1];
                g1 = (Ipp32s)src0[5];
                b0 = (Ipp32s)src0[0];
                b1 = (Ipp32s)src0[4];
                y0 = RGB_YUV_GC[0] * r0 + RGB_YUV_GC[1] * g0 + RGB_YUV_GC[2] * b0 + 128;
                y1 = RGB_YUV_GC[0] * r1 + RGB_YUV_GC[1] * g1 + RGB_YUV_GC[2] * b1 + 128;
                dsty0[0] = (Ipp8u)((y0 >> 8) + RGB_YUV_GC[3]);
                dsty0[1] = (Ipp8u)((y1 >> 8) + RGB_YUV_GC[3]);
                r0 += r1;
                g0 += g1;
                b0 += b1;
                dstVU[0] = (Ipp8u)(((RGB_YUV_GC[8] * r0 + RGB_YUV_GC[9] * g0 + RGB_YUV_GC[10] * b0 + 256) >> 9) + RGB_YUV_GC[11]);
                dstVU[1] = (Ipp8u)(((RGB_YUV_GC[4] * r0 + RGB_YUV_GC[5] * g0 + RGB_YUV_GC[6]  * b0 + 256) >> 9) + RGB_YUV_GC[7]);
                dsty0 += 2;
                dstVU += 2;
                src0  += 8;
            }
            if(width & 1)
            {
                r0 = (Ipp32s)src0[2];
                g0 = (Ipp32s)src0[1];
                b0 = (Ipp32s)src0[0];
                y0 = RGB_YUV_GC[0] * r0 + RGB_YUV_GC[1] * g0 + RGB_YUV_GC[2] * b0 + 128;
                dsty0[0] = (Ipp8u)((y0 >> 8) + RGB_YUV_GC[3]);
                dstVU[0] = (Ipp8u)(((RGB_YUV_GC[8] * r0 + RGB_YUV_GC[9] * g0 + RGB_YUV_GC[10] * b0 + 128) >> 8) + RGB_YUV_GC[11]);
                dstVU[1] = (Ipp8u)(((RGB_YUV_GC[4] * r0 + RGB_YUV_GC[5] * g0 + RGB_YUV_GC[6]  * b0 + 128) >> 8) + RGB_YUV_GC[7]);
            }
        }
    }
}

void gpu_csc_ARGBToNV12(const Ipp8u* pSrc, int srcStep,
                        Ipp8u* pDst[2], int dstStep[2],
                        int width, int height)
{
    int dstStepY  = dstStep[0];
    int dstStepVU = dstStep[1];


    Ipp8u* pDstY0 = pDst[0];
    Ipp8u* pDstVU = pDst[1];

    Ipp8u* pDstY1 = pDstY0 + dstStepY;

    Ipp8u* pSrc0  = (Ipp8u*)pSrc;
    Ipp8u* pSrc1  = pSrc0 + srcStep;

    int w, h;
    srcStep  *= 2;
    dstStepY *= 2;
    /* if we use fixed point formula */
    if(g_Formula & GPU_CSC_FORMULA_BT601)
    {
        for(h = height & ~1;  h > 0;  h -= 2)
        {
            Ipp8u* src0  = pSrc0;
            Ipp8u* src1  = pSrc1;
            Ipp8u* dsty0 = pDstY0;
            Ipp8u* dsty1 = pDstY1;
            Ipp8u* dstVU = pDstVU;
            for(w = width & ~1;  w > 0;  w -= 2)
            {
                Ipp32s R, B;
                Ipp32s y, ysum;
                Ipp32s k0 = RGB_YUV[0];
                Ipp32s k1 = RGB_YUV[1];
                Ipp32s k2 = RGB_YUV[2];

                y  = k0 * (Ipp32s)src0[2];
                y += k1 * (Ipp32s)src0[1];
                y += k2 * (Ipp32s)src0[0];
                dsty0[0] = (Ipp8u)(y >> 16);
                ysum = y;

                y  = k0 * (Ipp32s)src0[6];
                y += k1 * (Ipp32s)src0[5];
                y += k2 * (Ipp32s)src0[4];
                dsty0[1] = (Ipp8u)(y >> 16);
                ysum += y;

                y  = k0 * (Ipp32s)src1[2];
                y += k1 * (Ipp32s)src1[1];
                y += k2 * (Ipp32s)src1[0];
                dsty1[0] = (Ipp8u)(y >> 16);
                ysum += y;

                y  = k0 * (Ipp32s)src1[6];
                y += k1 * (Ipp32s)src1[5];
                y += k2 * (Ipp32s)src1[4];
                dsty1[1] = (Ipp8u)(y >> 16);
                ysum += y;

                R  = (Ipp32s)src0[2];
                B  = (Ipp32s)src0[0];
                R += (Ipp32s)src0[6];
                B += (Ipp32s)src0[4];
                R += (Ipp32s)src1[2];
                B += (Ipp32s)src1[0];
                R += (Ipp32s)src1[6];
                B += (Ipp32s)src1[4];

                k0 = RGB_YUV[3];
                k1 = RGB_YUV[4];

                *dstVU++ = (Ipp8u)       (((k0 * ((((B << 16) - ysum) + 0x20000) >> 16)) >> 18) + 128);
                *dstVU++ = (Ipp8u)SAT_DK[(((k1 * ((((R << 16) - ysum) + 0x20000) >> 16)) >> 18) + 128)];
                dsty0 += 2;
                dsty1 += 2;
                src0  += 8;
                src1  += 8;
            }
            if(width & 1)
            {
                Ipp32s r0 = (Ipp32s)src0[2];
                Ipp32s r1 = (Ipp32s)src1[2];
                Ipp32s b0 = (Ipp32s)src0[0];
                Ipp32s b1 = (Ipp32s)src1[0];
                Ipp32s y0 = RGB_YUV[0] * r0 + RGB_YUV[1] * (Ipp32s)src0[1] + RGB_YUV[2] * b0;
                Ipp32s y1 = RGB_YUV[0] * r1 + RGB_YUV[1] * (Ipp32s)src1[1] + RGB_YUV[2] * b1;
                dsty0[0]  = (Ipp8u)(y0 >> 16);
                dsty1[0]  = (Ipp8u)(y1 >> 16);
                y0 += y1;
                r0 += r1;
                b0 += b1;
                dstVU[0]  = (Ipp8u)       (((RGB_YUV[3] * ((((b0 << 16) - y0)) >> 16)) >> 17) + 128);
                dstVU[1]  = (Ipp8u)SAT_DK[(((RGB_YUV[4] * ((((r0 << 16) - y0)) >> 16)) >> 17) + 128)];
            }
            pSrc0  += srcStep;
            pSrc1  += srcStep;
            pDstY0 += dstStepY;
            pDstY1 += dstStepY;
            pDstVU += dstStepVU;
        }
        if(height & 1)
        {
            Ipp32s y0, y1;
            Ipp32s r0, r1;
            Ipp32s b0, b1;
            Ipp8u* src0  = pSrc0;
            Ipp8u* dsty0 = pDstY0;
            Ipp8u* dstVU = pDstVU;
            for(w = width & ~1;  w > 0;  w -= 2)
            {
                r0 = (Ipp32s)src0[2];
                r1 = (Ipp32s)src0[6];
                b0 = (Ipp32s)src0[0];
                b1 = (Ipp32s)src0[4];
                y0 = RGB_YUV[0] * r0 + RGB_YUV[1] * (Ipp32s)src0[1] + RGB_YUV[2] * b0;
                y1 = RGB_YUV[0] * r1 + RGB_YUV[1] * (Ipp32s)src0[5] + RGB_YUV[2] * b1;
                dsty0[0] = (Ipp8u)(y0 >> 16);
                dsty0[1] = (Ipp8u)(y1 >> 16);
                y0 += y1;
                r0 += r1;
                b0 += b1;
                dstVU[0] = (Ipp8u)       (((RGB_YUV[3] * ((((b0 << 16) - y0)) >> 16)) >> 17) + 128);
                dstVU[1] = (Ipp8u)SAT_DK[(((RGB_YUV[4] * ((((r0 << 16) - y0)) >> 16)) >> 17) + 128)];
                dsty0 += 2;
                dstVU += 2;
                src0  += 8;
            }
            if(width & 1)
            {
                r0 = (Ipp32s)src0[2];
                b0 = (Ipp32s)src0[0];
                y0 = RGB_YUV[0] * r0 + RGB_YUV[1] * (Ipp32s)src0[1] + RGB_YUV[2] * b0;
                dsty0[0] = (Ipp8u)(y0 >> 16);
                dstVU[0] = (Ipp8u)       (((RGB_YUV[3] * ((((b0 << 16) - y0)) >> 16)) >> 16) + 128);
                dstVU[1] = (Ipp8u)SAT_DK[(((RGB_YUV[4] * ((((r0 << 16) - y0)) >> 16)) >> 16) + 128)];
            }
        }
    }
    else /* if we use integer approximated formula */
    {
        for(h = height & ~1;  h > 0;  h -= 2)
        {
            Ipp8u* src0  = pSrc0;
            Ipp8u* src1  = pSrc1;
            Ipp8u* dsty0 = pDstY0;
            Ipp8u* dsty1 = pDstY1;
            Ipp8u* dstVU = pDstVU;
            for(w = width & ~1;  w > 0;  w -= 2)
            {
                Ipp32s R, G, B;
                Ipp32s y;
                Ipp32s k0 = RGB_YUV_GC[0];
                Ipp32s k1 = RGB_YUV_GC[1];
                Ipp32s k2 = RGB_YUV_GC[2];

                y  =  128;
                y += k0 * (Ipp32s)src0[2];
                y += k1 * (Ipp32s)src0[1];
                y += k2 * (Ipp32s)src0[0];
                dsty0[0] = (Ipp8u)((y >> 8) + RGB_YUV_GC[3]);

                y  =  128;
                y += k0 * (Ipp32s)src0[6];
                y += k1 * (Ipp32s)src0[5];
                y += k2 * (Ipp32s)src0[4];
                dsty0[1] = (Ipp8u)((y >> 8) + RGB_YUV_GC[3]);

                y  =  128;
                y += k0 * (Ipp32s)src1[2];
                y += k1 * (Ipp32s)src1[1];
                y += k2 * (Ipp32s)src1[0];
                dsty1[0] = (Ipp8u)((y >> 8) + RGB_YUV_GC[3]);

                y  =  128;
                y += k0 * (Ipp32s)src1[6];
                y += k1 * (Ipp32s)src1[5];
                y += k2 * (Ipp32s)src1[4];
                dsty1[1] = (Ipp8u)((y >> 8) + RGB_YUV_GC[3]);

                R  = (Ipp32s)src0[2];
                G  = (Ipp32s)src0[1];
                B  = (Ipp32s)src0[0];
                R += (Ipp32s)src0[6];
                G += (Ipp32s)src0[5];
                B += (Ipp32s)src0[4];
                R += (Ipp32s)src1[2];
                G += (Ipp32s)src1[1];
                B += (Ipp32s)src1[0];
                R += (Ipp32s)src1[6];
                G += (Ipp32s)src1[5];
                B += (Ipp32s)src1[4];

                *dstVU++ = (Ipp8u)(((RGB_YUV_GC[4] * R + RGB_YUV_GC[5] * G + RGB_YUV_GC[6]  * B + 512) >> 10) + RGB_YUV_GC[7]);
                *dstVU++ = (Ipp8u)(((RGB_YUV_GC[8] * R + RGB_YUV_GC[9] * G + RGB_YUV_GC[10] * B + 512) >> 10) + RGB_YUV_GC[11]);
                dsty0 += 2;
                dsty1 += 2;
                src0  += 8;
                src1  += 8;
            }
            if(width & 1)
            {
                Ipp32s r0 = (Ipp32s)src0[2];
                Ipp32s r1 = (Ipp32s)src1[2];
                Ipp32s g0 = (Ipp32s)src0[1];
                Ipp32s g1 = (Ipp32s)src1[1];
                Ipp32s b0 = (Ipp32s)src0[0];
                Ipp32s b1 = (Ipp32s)src1[0];
                Ipp32s y0 = RGB_YUV_GC[0] * r0 + RGB_YUV_GC[1] * g0 + RGB_YUV_GC[2] * b0 + 128;
                Ipp32s y1 = RGB_YUV_GC[0] * r1 + RGB_YUV_GC[1] * g1 + RGB_YUV_GC[2] * b1 + 128;
                dsty0[0]  = (Ipp8u)((y0 >> 8) + RGB_YUV_GC[3]);
                dsty1[0]  = (Ipp8u)((y1 >> 8) + RGB_YUV_GC[3]);
                r0 += r1;
                g0 += g1;
                b0 += b1;
                dstVU[0]  = (Ipp8u)(((RGB_YUV_GC[4] * r0 + RGB_YUV_GC[5] * g0 + RGB_YUV_GC[6]  * b0 + 256) >> 9) + RGB_YUV_GC[7]);
                dstVU[1]  = (Ipp8u)(((RGB_YUV_GC[8] * r0 + RGB_YUV_GC[9] * g0 + RGB_YUV_GC[10] * b0 + 256) >> 9) + RGB_YUV_GC[11]);
            }
            pSrc0  += srcStep;
            pSrc1  += srcStep;
            pDstY0 += dstStepY;
            pDstY1 += dstStepY;
            pDstVU += dstStepVU;
        }
        if(height & 1)
        {
            Ipp32s y0, y1;
            Ipp32s r0, r1;
            Ipp32s g0, g1;
            Ipp32s b0, b1;
            Ipp8u* src0  = pSrc0;
            Ipp8u* dsty0 = pDstY0;
            Ipp8u* dstVU = pDstVU;
            for(w = width & ~1;  w > 0;  w -= 2)
            {
                r0 = (Ipp32s)src0[2];
                r1 = (Ipp32s)src0[6];
                g0 = (Ipp32s)src0[1];
                g1 = (Ipp32s)src0[5];
                b0 = (Ipp32s)src0[0];
                b1 = (Ipp32s)src0[4];
                y0 = RGB_YUV_GC[0] * r0 + RGB_YUV_GC[1] * g0 + RGB_YUV_GC[2] * b0 + 128;
                y1 = RGB_YUV_GC[0] * r1 + RGB_YUV_GC[1] * g1 + RGB_YUV_GC[2] * b1 + 128;
                dsty0[0] = (Ipp8u)((y0 >> 8) + RGB_YUV_GC[3]);
                dsty0[1] = (Ipp8u)((y1 >> 8) + RGB_YUV_GC[3]);
                r0 += r1;
                g0 += g1;
                b0 += b1;
                dstVU[0] = (Ipp8u)(((RGB_YUV_GC[4] * r0 + RGB_YUV_GC[5] * g0 + RGB_YUV_GC[6]  * b0 + 256) >> 9) + RGB_YUV_GC[7]);
                dstVU[1] = (Ipp8u)(((RGB_YUV_GC[8] * r0 + RGB_YUV_GC[9] * g0 + RGB_YUV_GC[10] * b0 + 256) >> 9) + RGB_YUV_GC[11]);
                dsty0 += 2;
                dstVU += 2;
                src0  += 8;
            }
            if(width & 1)
            {
                r0 = (Ipp32s)src0[2];
                g0 = (Ipp32s)src0[1];
                b0 = (Ipp32s)src0[0];
                y0 = RGB_YUV_GC[0] * r0 + RGB_YUV_GC[1] * g0 + RGB_YUV_GC[2] * b0 + 128;
                dsty0[0] = (Ipp8u)((y0 >> 8) + RGB_YUV_GC[3]);
                dstVU[0] = (Ipp8u)(((RGB_YUV_GC[4] * r0 + RGB_YUV_GC[5] * g0 + RGB_YUV_GC[6]  * b0 + 128) >> 8) + RGB_YUV_GC[7]);
                dstVU[1] = (Ipp8u)(((RGB_YUV_GC[8] * r0 + RGB_YUV_GC[9] * g0 + RGB_YUV_GC[10] * b0 + 128) >> 8) + RGB_YUV_GC[11]);
            }
        }
    }
}

void gpu_csc_RGBToNV12(const Ipp8u* pSrc, int srcStep,
                       Ipp8u* pDst[2], int dstStep[2],
                       int width, int height)
{
    int dstStepY  = dstStep[0];
    int dstStepUV = dstStep[1];

    Ipp8u* pDstY0 = pDst[0];
    Ipp8u* pDstUV = pDst[1];

    Ipp8u* pDstY1 = pDstY0+dstStepY;

    srcStep /= 2;

    Ipp16u* pSrc0 = (Ipp16u*)pSrc;
    Ipp16u* pSrc1 = pSrc0+srcStep;

    int w, h;
    srcStep *= 2;
    dstStepY *= 2;

    for (h = height & ~1;  h > 0;  h -= 2)
    {
        Ipp16u* src0 = pSrc0;
        Ipp16u* src1 = pSrc1;
        Ipp8u* dsty0 = pDstY0;
        Ipp8u* dsty1 = pDstY1;
        Ipp8u* dstUV = pDstUV;
        for (w = width & ~1;  w > 0;  w -= 2)
        {
            Ipp32s R, B;
            Ipp32s y, ysum;
            Ipp32s k0 = RGB_YUV[0];
            Ipp32s k1 = RGB_YUV[1];
            Ipp32s k2 = RGB_YUV[2];
            Ipp8u  red0, green0, blue0, red1, green1, blue1;

            red0 = (Ipp8u)((src0[0]) >> 11);
            green0 = (Ipp8u)(((src0[0]) >> 5) & 0x3f);
            blue0 = (Ipp8u)((src0[0]) & 0x1f);

            red0 = (red0 << 3)|(red0 >> 2);
            green0 = (green0 << 2)|(green0 >> 4);
            blue0 = (blue0 << 3)|(blue0 >> 2);

            R  = (Ipp32s)red0;
            B  = (Ipp32s)blue0;

            y  = k0 * (Ipp32s)red0;
            y += k1 * (Ipp32s)green0;
            y += k2 * (Ipp32s)blue0;
            dsty0[0] = (Ipp8u)(y >> 16);
            ysum = y;

            red0   = (Ipp8u)((src0[1]) >> 11);
            green0 = (Ipp8u)(((src0[1]) >> 5) & 0x3f);
            blue0  = (Ipp8u)((src0[1]) & 0x1f);
            red0   = (red0   << 3) | (red0   >> 2);
            green0 = (green0 << 2) | (green0 >> 4);
            blue0  = (blue0  << 3) | (blue0  >> 2);

            R  += (Ipp32s)red0;
            B  += (Ipp32s)blue0;

            y  = k0 * (Ipp32s)red0;
            y += k1 * (Ipp32s)green0;
            y += k2 * (Ipp32s)blue0;
            dsty0[1] = (Ipp8u)(y >> 16);
            ysum += y;


            red1   = (Ipp8u)(src1[0] >> 11);
            green1 = (Ipp8u)((src1[0] >> 5) & 0x3f);
            blue1  = (Ipp8u)(src1[0] & 0x1f);
            red1   = (red1   << 3) | (red1   >> 2);
            green1 = (green1 << 2) | (green1 >> 4);
            blue1  = (blue1  << 3) | (blue1  >> 2);
            R  += (Ipp32s)red1;
            B  += (Ipp32s)blue1;

            y  = k0 * (Ipp32s)red1;
            y += k1 * (Ipp32s)green1;
            y += k2 * (Ipp32s)blue1;
            dsty1[0] = (Ipp8u)(y >> 16);
            ysum += y;

            red1   = (Ipp8u)(src1[1] >> 11);
            green1 = (Ipp8u)((src1[1] >> 5) & 0x3f);
            blue1  = (Ipp8u)(src1[1] & 0x1f);
            red1   = (red1   << 3) | (red1   >> 2);
            green1 = (green1 << 2) | (green1 >> 4);
            blue1  = (blue1  << 3) | (blue1  >> 2);

            R  += (Ipp32s)red1;
            B  += (Ipp32s)blue1;


            y  = k0*(Ipp32s)red1;
            y += k1*(Ipp32s)green1;
            y += k2*(Ipp32s)blue1;
            dsty1[1] = (Ipp8u)(y >> 16);
            ysum += y;

            k0 = RGB_YUV[3];
            k1 = RGB_YUV[4];

            *dstUV++ = (Ipp8u)       (((k0 * ((((B << 16) - ysum) + 0x20000) >> 16)) >> 18) + 128);
            *dstUV++ = (Ipp8u)SAT_DK[(((k1 * ((((R << 16) - ysum) + 0x20000) >> 16)) >> 18) + 128)];
            dsty0 += 2;
            dsty1 += 2;
            src0  += 2;
            src1  += 2;
        }
        if (width & 1)
        {
            Ipp8u  red0, green0, blue0, red1, green1, blue1;
            Ipp32s r0, r1, b0, b1, y0, y1;
            red0   = (Ipp8u)((src0[0]) >> 11);
            green0 = (Ipp8u)(((src0[0]) >> 5) & 0x3f);
            blue0  = (Ipp8u)((src0[0]) & 0x1f);
            red0   = (red0   << 3) | (red0   >> 2);
            green0 = (green0 << 2) | (green0 >> 4);
            blue0  = (blue0  << 3) | (blue0  >> 2);
            red1   = (Ipp8u)(src1[0] >> 11);
            green1 = (Ipp8u)((src1[0] >> 5) & 0x3f);
            blue1  = (Ipp8u)(src1[0] & 0x1f);
            red1   = (red1   << 3) | (red1   >> 2);
            green1 = (green1 << 2) | (green1 >> 4);
            blue1  = (blue1  << 3) | (blue1  >> 2);

            r0 = (Ipp32s)red0;
            r1 = (Ipp32s)red1;
            b0 = (Ipp32s)blue0;
            b1 = (Ipp32s)blue1;
            y0 = ((RGB_YUV[0] * r0 + RGB_YUV[1] * (Ipp32s)green0 + RGB_YUV[2] * b0));
            y1 = ((RGB_YUV[0] * r1 + RGB_YUV[1] * (Ipp32s)green1 + RGB_YUV[2] * b1));
            dsty0[0] = (Ipp8u)(y0 >> 16);
            dsty1[0] = (Ipp8u)(y1 >> 16);
            y0 += y1;
            r0 += r1;
            b0 += b1;
            dstUV[0] = (Ipp8u)       (((RGB_YUV[3] * ((((b0 << 16) - y0)) >> 16)) >> 17) + 128);
            dstUV[1] = (Ipp8u)SAT_DK[(((RGB_YUV[4] * ((((r0 << 16) - y0)) >> 16)) >> 17) + 128)];
        }
        pSrc0 += srcStep;
        pSrc1 += srcStep;
        pDstY0 += dstStepY;
        pDstY1 += dstStepY;
        pDstUV += dstStepUV;
    }

    if (height & 1)
    {
        Ipp32s  y0, y1;
        Ipp32s  r0, r1;
        Ipp32s  b0, b1;
        Ipp16u* src0  = pSrc0;
        Ipp8u*  dsty0 = pDstY0;
        Ipp8u*  dstUV = pDstUV;
        for (w = width & ~1;  w > 0;  w -= 2)
        {
            Ipp8u  red0, green0, blue0, red1, green1, blue1;
            red0   = (Ipp8u)((src0[0]) >> 11);
            green0 = (Ipp8u)(((src0[0]) >> 5) & 0x3f);
            blue0  = (Ipp8u)((src0[0]) & 0x1f);
            red0   = (red0   << 3) | (red0   >> 2);
            green0 = (green0 << 2) | (green0 >> 4);
            blue0  = (blue0  << 3) | (blue0  >> 2);

            red1   = (Ipp8u)((src0[1]) >> 11);
            green1 = (Ipp8u)(((src0[1]) >> 5) & 0x3f);
            blue1  = (Ipp8u)((src0[1]) & 0x1f);
            red1   = (red1   << 3) | (red1   >> 2);
            green1 = (green1 << 2) | (green1 >> 4);
            blue1  = (blue1  << 3) | (blue1  >> 2);

            r0 = (Ipp32s)red0;
            r1 = (Ipp32s)red1;
            b0 = (Ipp32s)blue0;
            b1 = (Ipp32s)blue1;
            y0 = ((RGB_YUV[0] * r0 + RGB_YUV[1] * (Ipp32s)green0 + RGB_YUV[2] * b0));
            y1 = ((RGB_YUV[0] * r1 + RGB_YUV[1] * (Ipp32s)green1 + RGB_YUV[2] * b1));
            dsty0[0] = (Ipp8u)(y0 >> 16);
            dsty0[1] = (Ipp8u)(y1 >> 16);
            y0 += y1;
            r0 += r1;
            b0 += b1;
            dstUV[0] = (Ipp8u)       (((RGB_YUV[3] * ((((b0 << 16) - y0)) >> 16)) >> 17) + 128);
            dstUV[1] = (Ipp8u)SAT_DK[(((RGB_YUV[4] * ((((r0 << 16) - y0)) >> 16)) >> 17) + 128)];
            dsty0 += 2;
            dstUV += 2;
            src0  += 2;
        }
        if (width & 1)
        {
            Ipp8u red0   = (Ipp8u)((src0[0]) >> 11);
            Ipp8u green0 = (Ipp8u)(((src0[0]) >> 5) & 0x3f);
            Ipp8u blue0  = (Ipp8u)((src0[0]) & 0x1f);
            red0   = (red0   << 3) | (red0   >> 2);
            green0 = (green0 << 2) | (green0 >> 4);
            blue0  = (blue0  << 3) | (blue0  >> 2);
            r0 = (Ipp32s)red0;
            b0 = (Ipp32s)blue0;
            y0 = ((RGB_YUV[0] * r0 + RGB_YUV[1] * (Ipp32s)green0 + RGB_YUV[2] * b0));
            dsty0[0] = (Ipp8u)(y0 >> 16);
            dstUV[0] = (Ipp8u)       (((RGB_YUV[3] * ((((b0 << 16) - y0)) >> 16)) >> 16) + 128);
            dstUV[1] = (Ipp8u)SAT_DK[(((RGB_YUV[4] * ((((r0 << 16) - y0)) >> 16)) >> 16) + 128)];
        }
    }
}

