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

#ifndef _GPU_CEU_TYPES_H_
#define _GPU_CEU_TYPES_H_

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GL_EXT_read_format_bgra
#define GL_BGRA_EXT                                             0x80E1
#define GL_UNSIGNED_SHORT_4_4_4_4_REV_EXT                       0x8365
#define GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT                       0x8366
#endif

#ifndef GL_MRVL_upload_Tex_Resolve
#define GL_MRVL_upload_Tex_Resolve    1
#define GL_MRVL_UPLOAD_TEXTURE_RESOLVE    0x9A01
#endif

#ifndef GL_MRVL_upload_tex_physical
#define GL_MRVL_upload_tex_physical   1
#define GL_MRVL_UPLOAD_TEXTURE_PHYSICAL   0x9A00
#endif

/* GL_MRVL_texture_video */
#ifndef GL_MRVL_texture_video
#define GL_MRVL_texture_video 1
#define GL_UYVY_MRVL                                            0x9100
#define GL_I420_MRVL                                            0x9101
#define GL_YV12_MRVL                                            0x9102
#define GL_YUY2_MRVL                                            0x9103
#define GL_NV12_MRVL                                            0x9014

#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL void GL_APIENTRY glTexImageVideo(GLenum target,GLsizei width,GLsizei height,GLenum Format,const void * pixels);
GL_APICALL void GL_APIENTRY glSetReadBuffer(GLsizei width,GLsizei height,GLenum format,GLenum type,void * pixels);
GL_APICALL void GL_APIENTRY glReadBuffer(GLint x,GLint y,GLsizei width,GLsizei height);
#endif
typedef void (GL_APIENTRYP PFNGLTEXIMAGEVIDEOPROC) ( GLenum target,GLsizei width,GLsizei height,GLenum Format,const void * pixels);
typedef void (GL_APIENTRYP PFNGLSETREADBUFFERPROC) (GLsizei width,GLsizei height,GLenum format,GLenum type,void * pixels);
typedef void (GL_APIENTRYP PFNGLREADBUFFERPROC) (GLint x,GLint y,GLsizei width,GLsizei height);
#endif

/******************************************************************************\
********************************** Common Types ********************************
\******************************************************************************/
typedef void                    CEUvoid;
typedef unsigned int            CEUbool;
typedef char                    CEUchar;
typedef unsigned char           CEUbyte;
typedef int                     CEUint;
typedef unsigned int            CEUuint;
typedef float                   CEUfloat;

/*CEU Aliases*/
#define CEU_FALSE   0
#define CEU_TRUE    1
#define CEU_NULL    0

#define CEU_PI      3.14159265358979323846

typedef struct tagCEU_CONTEXT{
    /*egl context*/
    EGLNativeDisplayType        display;
    EGLSurface                  pbufferSurface;
    EGLContext                  renderContext;

    /*FBO*/
    GLuint                      frameBuffer;
    GLuint                      renderBuffer;
    GLuint                      renderBufferWidth;
    GLuint                      renderBufferHeight;

    /*effect type*/
    gpu_effect_t                effectType;
    void*                       pEffectUserParam;  /* User input effect parameters. */

    /*Effect data struct and Parameters*/
    void*                       effectParam[GPU_EFFECT_COUNT];

    /* Texture to store the image to be processed */
    GLuint                      imageTexture;
    GLuint                      srcWidth;
    GLuint                      srcHeight;
    GLuint                      srcStride;
    GPU_RECT                    srcRect;

    /*Output image infomation.*/
    gpu_format_t                dstSurfFormat;
    CEUuint                     dstWidth;
    CEUuint                     dstHeight;
    GPU_ROTATION                dstRotation;

    /*vertex position and texture coodinates for each block*/
    GLfloat                     *blockVertexPos;
    GLfloat                     *blockTexCoords;
    GLfloat                     *blockNormalizedTexCoords;

    /*The number that texture image be devided.*/
    GLuint                      nTotalBlocks;
    GLuint                      nHorizontalBlocks;
    GLuint                      nVerticalBlocks;

    /* Read buffer for image output. */
    void                        *readBuffer;
    GLuint                      readBufferWidth;
    GLuint                      readBufferHeight;

    /* The thread ID in which create current CEU context. */
#ifdef ANDROID
    pid_t                       nCreatorThreadID;
#else
    CEUint                      nCreatorThreadID;
#endif

    /* User Specified Flag. */
    CEUuint                     usrFlag;

    /*Function pointer*/
    PFNGLSETREADBUFFERPROC      glSetReadBuffer;
    PFNGLREADBUFFERPROC         glReadBuffer;
    PFNGLTEXIMAGEVIDEOPROC      glTexImageVideo;
}CEUContext;

typedef struct _GPU_SURFACE{
    unsigned int            width;
    unsigned int            height;
    unsigned int            stride;

    gpu_format_t            format;

    gpu_vir_addr_t          pVertAddr;
    gpu_phy_addr_t          pPhyAddr;

} GPUSurface;

/******************************************************************************\
********************************** Status Codes * ********************************
\******************************************************************************/
typedef enum _ceuSTATUS
{
    ceuSTATUS_OK                    = 0,
    ceuSTATUS_SUCCESS               = 1,

    ceuSTATUS_FAILED                = -1,
    ceuSTATUS_INVALID_ARGUMENT      = -2,
}ceuSTATUS;

typedef ceuSTATUS (*pfn_gpu_effect_init)(CEUContext* pContext);
typedef ceuSTATUS (*pfn_gpu_effect_deinit)(CEUContext* pContext);
typedef ceuSTATUS (*pfn_gpu_effect_render)(CEUContext* pContext);

typedef struct _gpu_effect_func{
    pfn_gpu_effect_init     init;
    pfn_gpu_effect_deinit   deinit;
    pfn_gpu_effect_render   render;
}gpu_effect_func_t;


#ifdef __cplusplus
}
#endif

#endif

