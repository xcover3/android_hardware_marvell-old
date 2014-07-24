/***********************************************************************************
 *
 *    Copyright (c) 2009 - 2013 by Marvell International Ltd. and its affiliates.
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
 *
 ***********************************************************************************/

/*!
*******************************************************************************
*  \file veu.h
*  \brief
*      Decalration of VEU internal interface
******************************************************************************/

#ifndef __VEU_H__
#define __VEU_H__


/*check if the compiler is of C++*/
#ifdef __cplusplus
extern "C" {
#endif

#include "gcu.h"


/*!
*********************************************************************
*   \brief
*       Data types for initialization API
*********************************************************************
*/
typedef struct _VEU_INIT_DATA{
    GCUbool     debug;          /* [IN   ]  enable debug output or not */
    GCUuint     reserved[7];
}VEU_INIT_DATA;



/*!
*********************************************************************
*   \brief
*       Data types for effect API
*********************************************************************
*/

typedef enum _VEU_EFFECT{
    VEU_kVideoEffectType_BlackAndWhite = 257,                                       /* 257 */
    VEU_kVideoEffectType_Pink,                                                      /* 258 */
    VEU_kVideoEffectType_Green,                                                     /* 259 */
    VEU_kVideoEffectType_Sepia,                                                     /* 260 */
    VEU_kVideoEffectType_Negative,                                                  /* 261 */
    VEU_kVideoEffectType_Framing,                                                   /* 262 */
    VEU_kVideoEffectType_Text, /* Text overlay */                                   /* 263 */
    VEU_kVideoEffectType_ZoomIn,                                                    /* 264 */
    VEU_kVideoEffectType_ZoomOut,                                                   /* 265 */
    VEU_kVideoEffectType_Fifties,                                                   /* 266 */
    VEU_kVideoEffectType_ColorRGB16,                                                /* 267 */
    VEU_kVideoEffectType_Gradient                                                   /* 268 */
}VEU_EFFECT;

/**
 ******************************************************************************
 * struct    VEU_ColorRGB16
 * @brief    It is used internally for RGB16 color effect
 ******************************************************************************
*/
typedef struct _VEU_ColorStruct{
    VEU_EFFECT        colorEffectType;              /*Color type of effect*/
    unsigned short    rgb16ColorData;               /*RGB16 color only for the RGB16 color effect*/
} VEU_ColorStruct;


/*!
*********************************************************************
*   \brief
*       Data types for apply Curtain API
*********************************************************************
*/
typedef struct _VEU_CurtainParam
{
    unsigned short  nb_black_lines;
    unsigned char   top_is_black;
} VEU_CurtainParam;



/**
 ******************************************************************************
 * struct   VEU_ExternalProgress
 * @brief   This structure contains information provided to the external Effect
 *          and Transition functions
 * @note    The uiProgress value should be enough for most cases
 ******************************************************************************
 */
typedef struct
{
    /**< Progress of the Effect or the Transition, from 0 to 1000 (one thousand) */
    unsigned long     uiProgress;
    /**< Index of the current clip (first clip in case of a Transition), from 0 to N */
    //M4OSA_UInt8     uiCurrentClip;
    /**< Current time, in milliseconds, in the current clip time-line */
    unsigned long     uiClipTime;
    /**< Current time, in milliseconds, in the output clip time-line */
    unsigned long     uiOutputTime;
    signed char       bIsLast;

} VEU_ExternalProgress;


/**
 ******************************************************************************
 * enum        VEU_SlideTransition_Direction
 * @brief    Defines directions for the slide transition
 ******************************************************************************
*/
typedef enum {
    VEU_SlideTransition_RightOutLeftIn,
    VEU_SlideTransition_LeftOutRightIn,
    VEU_SlideTransition_TopOutBottomIn,
    VEU_SlideTransition_BottomOutTopIn
} VEU_SlideTransition_Direction;

/**
 ******************************************************************************
 * struct    VEU_SlideTransitionSettings
 * @brief    This structure defines the slide transition settings
 ******************************************************************************
*/

typedef struct
{
    VEU_SlideTransition_Direction direction; /* direction of the slide */
} VEU_SlideTransitionSettings;


/**
 ******************************************************************************
 * struct    VEU_fiftiesStruct
 * @brief    It is used for fifties effect
 ******************************************************************************
*/
typedef struct
{
    unsigned long fiftiesEffectDuration;     /**< Duration of the same effect in a video */
    signed long   previousClipTime;          /**< Previous clip time, used by framing filter
                                                for SAVING */
    unsigned long shiftRandomValue;          /**< Vertical shift of the image */
    unsigned long stripeRandomValue;         /**< Horizontal position of the stripe */

} VEU_FiftiesStruct;






typedef struct _VEU_EFFECT_DATA{
    VEU_EFFECT      effect;
    GCUSurface      pSrcSurface;
    GCUSurface      pDstSurface;
    GCUuint         color;
    GCUuint         reserved[4];
}VEU_EFFECT_DATA;

/*!
*********************************************************************
*   \brief
*       Data types for apply Curtain API
*********************************************************************
*/
typedef struct _VEU_CURTAIN_DATA{
    GCUSurface      pSrcSurface;
    GCUSurface      pDstSurface;
    GCUuint         nb_black_lines;
    unsigned char   top_is_black;
    GCUuint         reserved[4];
}VEU_CURTAIN_DATA;

/*!
*********************************************************************
*   \brief
*       Data types for Slide Transition API
*********************************************************************
*/
typedef struct _VEU_SLIDE_DATA{
    GCUSurface                     pSrcSurface1;
    GCUSurface                     pSrcSurface2;
    GCUSurface                     pDstSurface;
    GCUuint                        uiProgress;
    VEU_SlideTransition_Direction  direction;
    GCUuint                        reserved[4];
}VEU_SLIDE_DATA;


/*!
*********************************************************************
*   \brief
*       Data types for Effect zoom API
*********************************************************************
*/
typedef struct _VEU_ZOOM_DATA{
    GCUSurface            pSrcSurface;
    GCUSurface            pDstSurface;
    GCUuint               pFunctionContext;
    GCUuint               uiProgress;    /**< Progress of the effect, from 0 to 1000 (one thousand) */
    GCUuint               reserved[4];
}VEU_ZOOM_DATA;


/*!
*********************************************************************
*   \brief
*       Data types for Effect Fifties API
*********************************************************************
*/
typedef struct _VEU_FIFTIES_DATA{
    GCUSurface              pSrcSurface;
    GCUSurface              pDstSurface;
    VEU_FiftiesStruct*      p_FiftiesData;
    VEU_ExternalProgress*   pProgress;
    GCUuint                 reserved[4];
}VEU_FIFTIES_DATA;

/*!
*********************************************************************
*   \brief
*       VEU interface definition
*********************************************************************
*/

/*
 *  VEU Initialization API
 */
GCUbool veuInitialize(VEU_INIT_DATA* pData);
GCUvoid veuTerminate();


/*
 * VEU Surface API
 */

/* Create surface in video memory, newly created buffer address returned in pVirtAddr and pPhysicalAddr  */
GCUSurface veuCreateSurface(GCUuint             width,
                            GCUuint             height,
                            GCU_FORMAT          format,
                            GCUVirtualAddr*     pVirtAddr,
                            GCUPhysicalAddr*    pPhysicalAddr);

/*
 * Create pre-allocated surface, the pre-allocated buffer address passed by virtualAddr and physicalAddr
 *
 * Currently we only support below combinations :
 *  1) bPreAllocVirtual and bPreAllocPhysical are both GCU_TRUE.
 *  2) bPreAllocVirtual is GCU_TRUE but bPreAllocPhysical is GCU_FALSE.
 *
 * Currently We don't support below combinations
 *  1) bPreAllocVirtua is GCU_FALSE but bPreAllocPhysical is GCU_TRUE
 */
GCUSurface veuCreatePreAllocSurface(GCUuint             width,
                                    GCUuint             height,
                                    GCU_FORMAT          format,
                                    GCUbool             bPreAllocVirtual,
                                    GCUVirtualAddr      virtualAddr,
                                    GCUbool             bPreAllocPhysical,
                                    GCUPhysicalAddr     physicalAddr);

/*
 * Update pre-allocated buffer address.
 * When updating pre-alloc buffer , the bPreAllocVirtual and bPreAllocPhysical flags should be same as the
 * values you create this buffer. Else, this function will directly return and set error.
 */
GCUbool veuUpdatePreAllocBuffer(GCUSurface          pSurface,
                                    GCUbool             bPreAllocVirtual,
                                    GCUVirtualAddr      virtualAddr,
                                    GCUbool             bPreAllocPhysical,
                                    GCUPhysicalAddr     physicalAddr);

/* Destroy buffer, whether pre-allocated or none-preallocated */
GCUvoid veuDestroySurface(GCUSurface pSurface);


/*
 *  VEU Effect API
 */
GCUbool veuEffectColorProc(VEU_EFFECT_DATA* pData);

GCUbool veuEffectCurtainProc(VEU_CURTAIN_DATA* pData);

GCUbool veuEffectSlideTransitionProc(VEU_SLIDE_DATA* pData);

GCUbool veuEffectFiftiesProc(VEU_FIFTIES_DATA* pData);

GCUbool veuEffectZoomProc(VEU_ZOOM_DATA* pData);


/*
 *  VEU Effect API with original interface
 */
GCUbool _veuEffectColorProc(GCUvoid*     pFunctionContext,
                             GCUSurface     pSrcSurface,
                             GCUSurface     pDstSurface,
                             GCUvoid*       pProgress,
                             unsigned long  uiEffectKind);


GCUbool _veuEffectCurtainProc(GCUSurface     pSrcSurface,
                             GCUSurface         pDstSurface,
                             VEU_CurtainParam*  curtain_factor,
                             GCUvoid*           user_data);

GCUbool _veuEffectSlideTransitionProc(GCUvoid*   userData,
                            GCUSurface            pSrcSurface1,
                            GCUSurface            pSrcSurface2,
                            GCUSurface            pDstSurface,
                            VEU_ExternalProgress* pProgress,
                            unsigned long         uiTransitionKind);

GCUbool _veuEffectFiftiesProc(GCUvoid*       pUserData,
                            GCUSurface            pSrcSurface,
                            GCUSurface            pDstSurface,
                            VEU_ExternalProgress* pProgress,
                            unsigned long         uiEffectKind);

GCUbool _veuEffectZoomProc(GCUvoid*       pFunctionContext,
                            GCUSurface            pSrcSurface,
                            GCUSurface            pDstSurface,
                            VEU_ExternalProgress* pProgress,
                            unsigned long         uiEffectKind);


/*
 * Save surface content to bmp or yuv file, file type is decided by surface format.
 * Saved file name will be prefix_index.bmp(or .yuv), index is a global counter for the
 * times your called this function.
 */
GCUbool _veuDumpSurface(GCUSurface pSurface, const char* prefix);

/* Create surface from raw yuv file, user needs to provide format, width and height */
GCUSurface _veuLoadYUVSurfaceFromFile(const char*   filename,
                                                     GCU_FORMAT    format,
                                                     GCUuint       width,
                                                     GCUuint       height);

/* Create surface from bmp file, surface format is ARGB8888 */
GCUSurface _veuLoadRGBSurfaceFromFile(const char*   filename);

/* Color conversion implemented by neon */
GCUbool Convert8888ToYUV_NEON(GCUSurface pSurfaceARGB, GCUSurface pDstSurface);

/* Color conversion implemented by wmmx */
GCUbool Convert8888ToYUV(GCUSurface pSurfaceARGB, GCUSurface pDstSurface);

/* Color conversion implemented by c */
GCUbool Convert8888ToYUV_C(GCUSurface pSurfaceARGB, GCUSurface pDstSurface);

/*check if the compiler is of C++*/
#ifdef __cplusplus
}
#endif

#endif /* __VEU_H__ */
