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

#include <string.h>
#include "ARGB8888ToYUV.h"

/*
    Default formula is GC approximated BT601:
    Y  =  (( 66 * R + 129 * G +  25 * B + 128) >> 8) +  16;
    U  =  ((-38 * R -  74 * G + 112 * B + 128) >> 8) + 128;
    V  =  ((112 * R -  94 * G -  18 * B + 128) >> 8) + 128;
*/
GPU_CSC_FORMULA g_Formula = GPU_CSC_FORMULA_BT601_GC;

Ipp32s RGB_YUV[] = {
    (Ipp32s)( FSPQ16(0.299f) ),
    (Ipp32s)( FSPQ16(0.587f) ),
    (Ipp32s)( FSPQ16(0.114f) ),
    (Ipp32s)( FSPQ16(0.564f) ),
    (Ipp32s)( FSPQ16(0.713f) )
};

Ipp16s RGB_YUV_Q10[] = {
    (Ipp16s)( FSPQ10(0.299f) ),
    (Ipp16s)( FSPQ10(0.587f) ),
    (Ipp16s)( FSPQ10(0.114f) ),
    (Ipp16s)( FSPQ10(0.564f) ),
    (Ipp16s)( FSPQ10(0.713f) )
};

Ipp16s RGB_YUV_GC[] = {
    66,
    129,
    25,
    16,
    -38,
    -74,
    112,
    128,
    112,
    -94,
    -18,
    128
};

/*
    BT601 formula:
    Y' =  0.299*R' + 0.587*G' + 0.114*B'
    U  = -0.169*R' - 0.331*G' + 0.500*B' = 0.564*(B' - Y')
    V  =  0.500*R' - 0.419*G' - 0.081*B' = 0.713*(R' - Y')

    BT601 integer formula:
    Y  =  (( 66 * R + 129 * G +  25 * B + 128) >> 8) +  16;
    U  =  ((-38 * R -  74 * G + 112 * B + 128) >> 8) + 128;
    V  =  ((112 * R -  94 * G -  18 * B + 128) >> 8) + 128;
*/
static const Ipp32s RGB_YUV_BT601[] = {
    (Ipp32s)( FSPQ16(0.299f) ),
    (Ipp32s)( FSPQ16(0.587f) ),
    (Ipp32s)( FSPQ16(0.114f) ),
    (Ipp32s)( FSPQ16(0.564f) ),
    (Ipp32s)( FSPQ16(0.713f) )
};

static const Ipp16s RGB_YUV_Q10_BT601[] = {
    (Ipp16s)( FSPQ10(0.299f) ),
    (Ipp16s)( FSPQ10(0.587f) ),
    (Ipp16s)( FSPQ10(0.114f) ),
    (Ipp16s)( FSPQ10(0.564f) ),
    (Ipp16s)( FSPQ10(0.713f) )
};

static const Ipp16s RGB_YUV_BT601_GC[] = {
    66,
    129,
    25,
    16,
    -38,
    -74,
    112,
    128,
    112,
    -94,
    -18,
    128
};

/*
    BT709 formula:
    Y' =  0.2126*R' + 0.7152*G' + 0.0722*B'
    U  = -0.1146*R' - 0.3854*G' + 0.5000*B' = 0.5389*(B' - Y')
    V  =  0.5000*R' - 0.4542*G' - 0.0458*B' = 0.6350*(R' - Y')

    BT709 integer formula:
    Y  =  (( 47 * R + 157 * G +  16 * B + 128) >> 8) +  16;
    U  =  ((-26 * R -  86 * G + 112 * B + 128) >> 8) + 128;
    V  =  ((112 * R - 102 * G -  10 * B + 128) >> 8) + 128;
*/
static const Ipp32s RGB_YUV_BT709[] = {
    (Ipp32s)( FSPQ16(0.2126f) ),
    (Ipp32s)( FSPQ16(0.7152f) ),
    (Ipp32s)( FSPQ16(0.0722f) ),
    (Ipp32s)( FSPQ16(0.5389f) ),
    (Ipp32s)( FSPQ16(0.6350f) )
};

static const Ipp16s RGB_YUV_Q10_BT709[] = {
    (Ipp16s)( FSPQ10(0.2126f) ),
    (Ipp16s)( FSPQ10(0.7152f) ),
    (Ipp16s)( FSPQ10(0.0722f) ),
    (Ipp16s)( FSPQ10(0.5389f) ),
    (Ipp16s)( FSPQ10(0.6350f) )
};

static const Ipp16s RGB_YUV_BT709_GC[] = {
    47,
    157,
    16,
    16,
    -26,
    -86,
    112,
    128,
    112,
    -102,
    -10,
    128
};

/*
    Traditional formula:
    Y' =  0.299*R' + 0.587*G' + 0.114*B'
    U  = -0.147*R' - 0.289*G' + 0.436*B' = 0.492*(B' - Y' )
    V  =  0.615*R' - 0.515*G' - 0.100*B' = 0.877*(R' - Y' )
*/
static const Ipp32s RGB_YUV_TRADITIONAL[] = {
    (Ipp32s)( FSPQ16(0.299f) ),
    (Ipp32s)( FSPQ16(0.587f) ),
    (Ipp32s)( FSPQ16(0.114f) ),
    (Ipp32s)( FSPQ16(0.492f) ),
    (Ipp32s)( FSPQ16(0.877f) )
};

static const Ipp16s RGB_YUV_Q10_TRADITIONAL[] = {
    (Ipp16s)( FSPQ10(0.299f) ),
    (Ipp16s)( FSPQ10(0.587f) ),
    (Ipp16s)( FSPQ10(0.114f) ),
    (Ipp16s)( FSPQ10(0.492f) ),
    (Ipp16s)( FSPQ10(0.877f) )
};

void gpu_csc_ChooseFormula(GPU_CSC_FORMULA formula)
{
    if(formula != g_Formula)
    {
        switch(formula)
        {
            case GPU_CSC_FORMULA_BT601_GC:
                memcpy(RGB_YUV_GC, RGB_YUV_BT601_GC, sizeof(RGB_YUV_GC));

                g_Formula       = formula;
                break;

            case GPU_CSC_FORMULA_BT709_GC:
                memcpy(RGB_YUV_GC, RGB_YUV_BT709_GC, sizeof(RGB_YUV_GC));

                g_Formula       = formula;
                break;

            case GPU_CSC_FORMULA_BT601:
                memcpy(RGB_YUV, RGB_YUV_BT601, sizeof(RGB_YUV));
                memcpy(RGB_YUV_Q10, RGB_YUV_Q10_BT601, sizeof(RGB_YUV_Q10));

                g_Formula       = formula;
                break;

            case GPU_CSC_FORMULA_BT709:
                memcpy(RGB_YUV, RGB_YUV_BT709, sizeof(RGB_YUV));
                memcpy(RGB_YUV_Q10, RGB_YUV_Q10_BT709, sizeof(RGB_YUV_Q10));

                g_Formula       = formula;
                break;

            case GPU_CSC_FORMULA_TRADITIONAL:
                memcpy(RGB_YUV, RGB_YUV_TRADITIONAL, sizeof(RGB_YUV));
                memcpy(RGB_YUV_Q10, RGB_YUV_Q10_TRADITIONAL, sizeof(RGB_YUV_Q10));

                g_Formula       = formula;
                break;

            default:
                break;
        }
    }
}

