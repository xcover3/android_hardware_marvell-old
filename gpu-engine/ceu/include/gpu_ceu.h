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


#ifndef _GPU_CEU_H_
#define _GPU_CEU_H_

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************\
******************************* GPU CEU Context Flag******************************
\******************************************************************************/

#define GPU_CEU_FLAG_DEBUG          0x00000001

/******************************************************************************\
******************************* GPU CEU Data Type********************************
\******************************************************************************/

typedef void* gpu_context_t;
typedef void* gpu_surface_t;
typedef void* gpu_phy_addr_t;
typedef void* gpu_vir_addr_t;

#define GPU_PHY_NULL  (gpu_phy_addr_t)(~0)
#define GPU_VIR_NULL  (gpu_vir_addr_t)(0)

typedef enum _GPU_FORMAT{
    /* RGB series */
    GPU_FORMAT_ARGB8888     =   0,
    GPU_FORMAT_XRGB8888     =   1,

    GPU_FORMAT_ARGB1555     =   2,
    GPU_FORMAT_XRGB1555     =   3,
    GPU_FORMAT_RGB565       =   4,

    GPU_FORMAT_RGBA8888     =   20,   /* TODO */
    GPU_FORMAT_RGBX8888     =   21,   /* TODO */

    /* BGR series */
    GPU_FORMAT_ABGR8888     =   50,
    GPU_FORMAT_XBGR8888     =   51,

    GPU_FORMAT_BGRA8888     =   70,   /* TODO */
    GPU_FORMAT_BGRX8888     =   71,   /* TODO */

    /* Luminance series */
    GPU_FORMAT_L8           =   100,
    GPU_FORMAT_A8           =   101,

    /* YUV series */
    GPU_FORMAT_UYVY         =   200,
    GPU_FORMAT_YUY2         =   201,
    GPU_FORMAT_YV12         =   202,
    GPU_FORMAT_NV12         =   203,
    GPU_FORMAT_I420         =   204,
    GPU_FORMAT_NV16         =   205,
    GPU_FORMAT_NV21         =   206,
    GPU_FORMAT_NV61         =   207,

    GPU_FORMAT_UNKNOW       =   500
}GPU_FORMAT;

typedef GPU_FORMAT gpu_format_t;

typedef enum _GPU_EFFECT{
    GPU_EFFECT_NULL         =   0,
    GPU_EFFECT_OLDMOVIE     =   1,
    GPU_EFFECT_TOONSHADING  =   2,
    GPU_EFFECT_HATCHING     =   3,

    GPU_RGB2UYVY            =   4,
    GPU_RGB2YUYV            =   5,

    GPU_EFFECT_PENCILSKETCH =   6,
    GPU_EFFECT_GLOW         =   7,
    GPU_EFFECT_TWIST        =   8,
    GPU_EFFECT_FRAME        =   9,
    GPU_EFFECT_SUNSHINE     =   10,

    GPU_EFFECT_COUNT
}gpu_effect_t;

typedef enum _GPU_ROTATION{     /*Clock-wise rotation.*/
    GPU_ROTATION_0          =   0,
    GPU_ROTATION_90         =   1,
    GPU_ROTATION_180        =   2,
    GPU_ROTATION_270        =   3,

    GPU_ROTATION_FLIP_H     =   4,
    GPU_ROTATION_FLIP_V     =   5
}GPU_ROTATION;

typedef struct _GPU_RECT{
    int         left;           /* left boundary is included in the rect. */
    int         top;            /* top boundary is included in the rect. */
    int         right;          /* right boundary is not included in the rect, width = right - left. */
    int         bottom;         /* bottom boundary is not included in the rect, height = bottom - top. */
}GPU_RECT;

/******************************************************************************\
******************************** Process Parameter*******************************
\******************************************************************************/

typedef struct _GPU_PROC_PARAM{
    /*Region of Interest.*/
    GPU_RECT*           pSrcRect;  /* Src sub rectangle to process, NULL means full surface. */

    /*Rotation.*/
    GPU_ROTATION        rotation;

    /* Parameters for effect. If the effect need no parameter,
    **set this variable to NULL. If the effect need parameter,
    **and this variable is NULL, then CEU will use default parameters.
    */
    void*               pEffectParam;

    unsigned int        reserved[5];
}GPU_PROC_PARAM;

/******************************************************************************\
******************************* Effect Control Parameter****************************
\******************************************************************************/

typedef struct _CEU_PARAM_PENCILSKETCH{
    float               contrast;   /* In the range of [-100.0, 100.0], default is 50.0. */
    float               brightness; /* In the range of [-100.0, 100.0], default is -50.0f. */
    int                 pencilSize; /* In the range of [0, 5], default is 0. */

    unsigned int        reserved[5];
}CEU_PARAM_PENCILSKETCH;

typedef struct _CEU_PARAM_GLOW{
    float               contrast;   /* In the range of [-100.0, 100.0], default is 10.0. */
    float               brightness; /* In the range of [-100.0, 100.0], default is 20.0. */

    unsigned int        reserved[6];
}CEU_PARAM_GLOW;

typedef struct _CEU_PARAM_TWIST{
    float               twist;      /* In the range of [-10.0, 10.0], default is 3.0. */
    float               radius;     /* In the range of [0.0, 1.0], default is 0.9. */

    unsigned int        reserved[6];
}CEU_PARAM_TWIST;

typedef struct _CEU_PARAM_FRAME{
    int                 frameNumber;    /* Interger between [1, 3], default is 1. */

    unsigned int        reserved[7];
}CEU_PARAM_FRAME;

typedef struct _CEU_PARAM_SUNSHINE{
    float               sunPositionX;   /* In the range of [0.0, 1.0], default is 0.1, 0.0 is left-most.*/
    float               sunPositionY;   /* In the range of [0.0, 1.0], default is 0.1, 0.0 is top-most.*/
    float               sunRadius;      /* In the range of [0.0, 1.0], default is 0.1. */

    unsigned int        reserved[5];
}CEU_PARAM_SUNSHINE;

/******************************************************************************\
********************************** GPU CEU API **********************************
\******************************************************************************/

/******************************************************************************
** Create and initialize GPU-CEU context.
**
** return value:
** If sucess, return the address of created GPU-CEU context, otherwise, return NULL.
*/
gpu_context_t gpu_ceu_init(
    unsigned int flag
    );

/******************************************************************************
** Destroy the supplied context, and release coresponding resource.
**
** If this function success, the supplied context can not be used any more.
**
** Return value:
** If success, return 0, otherwise, return -1.
*/
int gpu_ceu_deinit(
    gpu_context_t context
    );

/******************************************************************************
** Create GPU-CEU surface.
**
** Return value:
** If success, return the address of created surface, otherwise, return NULL;
*/
gpu_surface_t gpu_ceu_create_surface(
    gpu_context_t context,
    gpu_format_t format,
    unsigned int width,
    unsigned int height,
    unsigned int stride,
    gpu_vir_addr_t vir_addr,
    gpu_phy_addr_t phy_addr
    );

/******************************************************************************
** Destroy the supplied surface, and release coresponding resource.
**
** If this function success, the supplied surface can not be used any more.
**
** Return value:
** If success, return 0, otherwise, return -1.
*/
int gpu_ceu_destroy_surface(
    gpu_context_t context,
    gpu_surface_t surface
    );

/******************************************************************************
** Do the actually process.
**
** Return value:
** If success, return 0, otherwise, return -1.
*/
int gpu_ceu_process(
    gpu_context_t context,
    gpu_surface_t src,
    gpu_surface_t dst,
    gpu_effect_t effect,
    void* proc_param
    );

#ifdef __cplusplus
}
#endif

#endif

