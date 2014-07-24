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

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "gpu_ceu.h"
#include "gpu_ceu_internal.h"

#define RENDER_BUFFER_SIZE      512
#define RENDER_BUFFER_WIDTH     RENDER_BUFFER_SIZE
#define RENDER_BUFFER_HEIGHT    RENDER_BUFFER_SIZE

extern gpu_effect_func_t  g_effects[GPU_EFFECT_COUNT];

/*Create context*/
gpu_context_t gpu_ceu_init(
    unsigned int flag
    )
{
    EGLBoolean returnValue;

    EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    EGLint pbufferConfig[] =    {
        EGL_RED_SIZE,           5,
        EGL_GREEN_SIZE,         6,
        EGL_BLUE_SIZE,          5,
        EGL_ALPHA_SIZE,         0,
        EGL_DEPTH_SIZE,         0,
        EGL_SURFACE_TYPE,       EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE,    EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    EGLint pbuffer_attrib[] =   {
        EGL_WIDTH,              64,
        EGL_HEIGHT,             64,
        EGL_NONE
    };

    EGLint majorVersion;
    EGLint minorVersion;
    EGLConfig egl_config_choosed;
    CEUContext* pGpuContext;
    GLenum status;

    pGpuContext = (CEUContext*)gpu_ceu_allocate(sizeof(CEUContext));
    if(NULL == pGpuContext)
    {
        goto OnError;
    }

    gpu_ceu_memset(pGpuContext, 0, sizeof(CEUContext));
    pGpuContext->renderBufferWidth = RENDER_BUFFER_WIDTH;
    pGpuContext->renderBufferHeight = RENDER_BUFFER_HEIGHT;
    pGpuContext->effectType = (gpu_effect_t)-1;

    pGpuContext->nCreatorThreadID   = _ceu_get_current_thread_id();
    pGpuContext->usrFlag            = flag;

    pGpuContext->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (pGpuContext->display == EGL_NO_DISPLAY)
    {
        goto OnError;
    }

    returnValue = eglInitialize(pGpuContext->display, &majorVersion, &minorVersion);
    if (returnValue != EGL_TRUE)
    {
        goto OnError;
    }

    if(ceuSTATUS_SUCCESS != _gpu_ceu_chooseConfigExact(pGpuContext->display, &egl_config_choosed,
                                pbufferConfig, 5, 6, 5, 0))
    {
        goto OnError;
    }

    /*Create Pbuffer surface*/
    pGpuContext->pbufferSurface = eglCreatePbufferSurface(pGpuContext->display,
                                                          egl_config_choosed,
                                                          pbuffer_attrib);
    if (pGpuContext->pbufferSurface == EGL_NO_SURFACE)
    {
        goto OnError;
    }

    /*Create EGL render Context*/
    pGpuContext->renderContext = eglCreateContext(pGpuContext->display,
                                                  egl_config_choosed,
                                                  EGL_NO_CONTEXT,
                                                  context_attribs);
    if (pGpuContext->renderContext == EGL_NO_CONTEXT)
    {
        goto OnError;
    }
    returnValue = eglMakeCurrent(pGpuContext->display,
                                 pGpuContext->pbufferSurface,
                                 pGpuContext->pbufferSurface,
                                 pGpuContext->renderContext);
    if (returnValue != EGL_TRUE)
    {
        goto OnError;
    }

    /*Create FBO*/
    /* generate and bind framebuffer */
    glGenFramebuffers(1, &(pGpuContext->frameBuffer));
    glBindFramebuffer(GL_FRAMEBUFFER, pGpuContext->frameBuffer);

    /* generate render buffer */
    glGenRenderbuffers(1, &(pGpuContext->renderBuffer));
    glBindRenderbuffer(GL_RENDERBUFFER, pGpuContext->renderBuffer);

    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8_OES,
                            pGpuContext->renderBufferWidth,
                            pGpuContext->renderBufferHeight);

    /* attach render buffer to frame buffer */
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                              GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER,
                              pGpuContext->renderBuffer);

    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(status != GL_FRAMEBUFFER_COMPLETE)
    {
        goto OnError ;
    }

    glViewport(0, 0, pGpuContext->renderBufferWidth, pGpuContext->renderBufferHeight);

    /*Generate texture ID*/
    glGenTextures(1, &pGpuContext->imageTexture);

    /*Init function pointer.*/
    pGpuContext->glSetReadBuffer = (PFNGLSETREADBUFFERPROC)eglGetProcAddress("glSetReadBuffer");
    if(!pGpuContext->glSetReadBuffer)
    {
        goto OnError;
    }
    pGpuContext->glReadBuffer = (PFNGLREADBUFFERPROC)eglGetProcAddress("glReadBuffer");
    if(!pGpuContext->glReadBuffer)
    {
        goto OnError;
    }
    pGpuContext->glTexImageVideo = (PFNGLTEXIMAGEVIDEOPROC)eglGetProcAddress("glTexImageVideo");
    if(!pGpuContext->glTexImageVideo)
    {
        goto OnError;
    }

    if(GPU_CEU_FLAG_DEBUG & pGpuContext->usrFlag)
    {
        GPU_CEU_LOG(("%s: Sucess!", __FUNCTION__));
    }

    /*Set The GL states.*/
    glDisable(GL_DEPTH_TEST);

    return pGpuContext;

OnError:

    GPU_CEU_ERROR_LOG(("GPU_CEU initialization failed!"));

    if(NULL != pGpuContext)
    {
        if(EGL_NO_DISPLAY != pGpuContext->display)
        {
            if(EGL_NO_CONTEXT != pGpuContext->renderContext)
            {
                glDeleteTextures(1, &pGpuContext->imageTexture);

                glDeleteRenderbuffers(1, &(pGpuContext->renderBuffer));
                glDeleteFramebuffers(1, &(pGpuContext->frameBuffer));

                eglMakeCurrent(pGpuContext->display, EGL_NO_SURFACE , EGL_NO_SURFACE , EGL_NO_CONTEXT);

                eglDestroyContext(pGpuContext->display, pGpuContext->renderContext);
            }

            if(EGL_NO_SURFACE != pGpuContext->pbufferSurface)
            {
                eglDestroySurface(pGpuContext->display, pGpuContext->pbufferSurface);
            }

            eglTerminate(pGpuContext->display);
        }

        gpu_ceu_free((void*)pGpuContext);
    }
    return NULL;
}

/*Destroy context*/
int gpu_ceu_deinit(gpu_context_t context)
{
    CEUContext* pContext = (CEUContext*)context;

    if(CEU_FALSE == _isCreatorThread(pContext))
    {
        GPU_CEU_ERROR_LOG(("ERROR: %s: CEU context must be used in the thead that create it.",
            __FUNCTION__));
        return -1;
    }

    /*Check whether current egl context equals to ceu render context,
    ** if not, set ceu render context as current egl context.
    */
    if(eglGetCurrentContext() != pContext->renderContext)
    {
        if(EGL_TRUE != eglMakeCurrent(
                            pContext->display,
                            pContext->pbufferSurface,
                            pContext->pbufferSurface,
                            pContext->renderContext))
        {
            GPU_CEU_ERROR_LOG(("%s: eglMakeCurrent Failed!", __FUNCTION__));
            return -1;
        }
    }

    glDeleteRenderbuffers(1, &(pContext->renderBuffer));
    glDeleteFramebuffers(1, &(pContext->frameBuffer));

    glDeleteTextures(1, &pContext->imageTexture);

    for(int i = 0; i < GPU_EFFECT_COUNT; i++)
    {
        g_effects[i].deinit(pContext);
    }

    if(pContext->blockVertexPos)
    {
        gpu_ceu_free((void*)pContext->blockVertexPos);
        pContext->blockVertexPos = NULL;
    }

    if(pContext->blockTexCoords)
    {
        gpu_ceu_free((void*)pContext->blockTexCoords);
        pContext->blockTexCoords = NULL;
    }

    if(pContext->blockNormalizedTexCoords)
    {
        gpu_ceu_free((void*)pContext->blockNormalizedTexCoords);
        pContext->blockNormalizedTexCoords = NULL;
    }

    eglMakeCurrent(pContext->display, EGL_NO_SURFACE , EGL_NO_SURFACE , EGL_NO_CONTEXT);
    eglDestroySurface(pContext->display, pContext->pbufferSurface);
    eglDestroyContext(pContext->display, pContext->renderContext);
    eglTerminate(pContext->display);


    if(GPU_CEU_FLAG_DEBUG & pContext->usrFlag)
    {
        GPU_CEU_LOG(("%s: Sucess!", __FUNCTION__));
    }


    gpu_ceu_free((void*)pContext);

    return 0;
}

/*Create Surface*/
gpu_surface_t gpu_ceu_create_surface(
    gpu_context_t context,
    gpu_format_t format,
    unsigned int width,
    unsigned int height,
    unsigned int stride,
    gpu_vir_addr_t vir_addr,
    gpu_phy_addr_t phy_addr
    )
{
    GPUSurface* pSurface;

    if(CEU_FALSE == _isCreatorThread((CEUContext*)context))
    {
        GPU_CEU_ERROR_LOG(("ERROR: %s: CEU context must be used in the thead that create it.",
            __FUNCTION__));
        return NULL;
    }

    if(
        (!_isAligned(vir_addr, GPU_ADDR_ALIGN_BYTES))
        || ((phy_addr != GPU_PHY_NULL) && (!_isAligned(phy_addr, GPU_ADDR_ALIGN_BYTES)))
    )
    {
        /*Address didn't satisfy the align requirement.*/
        GPU_CEU_ERROR_LOG(("%s: Address didn't satisfy the 64 byte align requirement.", __FUNCTION__));
        return NULL;
    }

    if((width & 15) || (height & 3))
    {
        GPU_CEU_ERROR_LOG(("%s: size alignment isn't satisfied!", __FUNCTION__));
        return NULL;
    }

    pSurface = (GPUSurface*)gpu_ceu_allocate(sizeof(GPUSurface));
    if(NULL == pSurface)
    {
        GPU_CEU_ERROR_LOG(("%s: Can not allocate memory!", __FUNCTION__));
        return NULL;
    }
    gpu_ceu_memset(pSurface, 0, sizeof(GPUSurface));

    pSurface->width = width;
    pSurface->height = height;
    pSurface->stride = stride;
    pSurface->format = format;

    pSurface->pVertAddr = vir_addr;
    pSurface->pPhyAddr = phy_addr;

    if(GPU_CEU_FLAG_DEBUG & ((CEUContext*)context)->usrFlag)
    {
        GPU_CEU_LOG(("%s: Sucess!", __FUNCTION__));
        GPU_CEU_LOG(("Surface info: width = %d, height = %d, stride = %d, format = %d, virtAddr = 0x%08x, phyAddr = 0x%08x",
            width, height, stride, format, vir_addr, phy_addr));
    }

    return pSurface;
}

/*Destroy Surface*/
int gpu_ceu_destroy_surface(
    gpu_context_t context,
    gpu_surface_t surface
    )
{
    if(CEU_FALSE == _isCreatorThread((CEUContext*)context))
    {
        GPU_CEU_ERROR_LOG(("ERROR: %s: CEU context must be used in the thead that create it.",
            __FUNCTION__));
        return -1;
    }

    gpu_ceu_free((void*)surface);

    return 0;
}

int gpu_ceu_process(
    gpu_context_t context,
    gpu_surface_t src,
    gpu_surface_t dst,
    gpu_effect_t effect,
    void* proc_param
    )
{
    int ret = 0;
    float stretchX = 1.0f;
    float stretchY = 1.0f;
    CEUContext* pContext = (CEUContext*)context;
    GPUSurface* srcSurface = (GPUSurface*)src;
    GPUSurface* dstSurface = (GPUSurface*)dst;
    GPU_PROC_PARAM procParam;

    if((effect < 0) || (effect >= GPU_EFFECT_COUNT))
    {
        GPU_CEU_ERROR_LOG(("%s: Invalid Effect!", __FUNCTION__));
        return -1;
    }

    if(NULL != proc_param)
    {
        if(NULL != ((GPU_PROC_PARAM*)proc_param)->pSrcRect)
        {
            GPU_RECT* pRect = ((GPU_PROC_PARAM*)proc_param)->pSrcRect;
            if((pRect->left < 0)
                || (pRect->top < 0)
                || ((unsigned int)(pRect->right) > srcSurface->width)
                || ((unsigned int)(pRect->bottom) > srcSurface->height)
                || (pRect->left >= pRect->right)
                || (pRect->top >= pRect->bottom))
            {
                GPU_CEU_ERROR_LOG(("Invalid source rect: (%d, %d, %d, %d)",
                    pRect->left, pRect->top, pRect->right, pRect->bottom));
                return -1;
            }
        }
    }

    /* Save the previous process infomation. */
    gpu_format_t prevDstFormat = pContext->dstSurfFormat;

    /*Identify whether the texture coordinates changed, if does, need
    **to re-upload the texture coordinates.
    */
    CEUbool bTexCoordChanged = CEU_FALSE;
    CEUbool bSrcSizeChanged = CEU_FALSE;
    CEUbool bSrcRectChanged = CEU_FALSE;
    CEUbool bNeedCheckEffectParam = CEU_FALSE;

    if(CEU_FALSE == _isCreatorThread(pContext))
    {
        GPU_CEU_ERROR_LOG(("ERROR: %s: CEU context must be used in the thead that create it.",
            __FUNCTION__));
        return -1;
    }

    if(GPU_CEU_FLAG_DEBUG & pContext->usrFlag)
    {
        GPU_CEU_LOG(("%s:", __FUNCTION__));
        GPU_CEU_LOG(("\tsrcSurface info: width = %d, height = %d, stride = %d, format = %d, virtAddr = 0x%08x, phyAddr = 0x%08x",
            srcSurface->width, srcSurface->height, srcSurface->stride, srcSurface->format,
            srcSurface->pVertAddr, srcSurface->pPhyAddr));
        GPU_CEU_LOG(("\tsrcSurface info: width = %d, height = %d, stride = %d, format = %d, virtAddr = 0x%08x, phyAddr = 0x%08x",
            dstSurface->width, dstSurface->height, dstSurface->stride, dstSurface->format,
            dstSurface->pVertAddr, dstSurface->pPhyAddr));
    }

    /*Check whether current egl context equals to ceu render context,
    ** if not, set ceu render context as current egl context.
    */
    if(eglGetCurrentContext() != pContext->renderContext)
    {
        if(EGL_TRUE != eglMakeCurrent(
                            pContext->display,
                            pContext->pbufferSurface,
                            pContext->pbufferSurface,
                            pContext->renderContext))
        {
            GPU_CEU_ERROR_LOG(("%s: eglMakeCurrent Failed!", __FUNCTION__));
            return -1;
        }

        /* attach render buffer to frame buffer */
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                              GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER,
                              pContext->renderBuffer);
    }

    if(NULL != proc_param)
    {
        gpu_ceu_memcpy(&procParam, proc_param, sizeof(GPU_PROC_PARAM));
    }
    else
    {
        /*User didn't specify effect param, use the following defaults.*/
        gpu_ceu_memset(&procParam, 0, sizeof(GPU_PROC_PARAM));
        procParam.rotation = GPU_ROTATION_0;
        procParam.pSrcRect = NULL;
        procParam.pEffectParam = NULL;
    }

    if( (NULL != procParam.pEffectParam)
        || (pContext->pEffectUserParam != procParam.pEffectParam))
    {
        pContext->pEffectUserParam  = procParam.pEffectParam;
        bNeedCheckEffectParam       = CEU_TRUE;
    }

    pContext->srcStride     =   srcSurface->stride;

    if(pContext->srcWidth != srcSurface->width
        || pContext->srcHeight != srcSurface->height)
    {
        pContext->srcWidth      =   srcSurface->width;
        pContext->srcHeight     =   srcSurface->height;
        bSrcSizeChanged         =   CEU_TRUE;
    }

    if(NULL == procParam.pSrcRect)
    {
        if(pContext->srcRect.left != 0
            || pContext->srcRect.top != 0
            || (unsigned int)(pContext->srcRect.right) != srcSurface->width
            || (unsigned int)(pContext->srcRect.bottom) != srcSurface->height)
        {
            pContext->srcRect.left = 0;
            pContext->srcRect.top = 0;
            pContext->srcRect.right = srcSurface->width;
            pContext->srcRect.bottom = srcSurface->height;
            bSrcRectChanged = CEU_TRUE;
        }
    }
    else if(pContext->srcRect.left != procParam.pSrcRect->left
            || pContext->srcRect.top != procParam.pSrcRect->top
            || pContext->srcRect.right != procParam.pSrcRect->right
            || pContext->srcRect.bottom != procParam.pSrcRect->bottom)
    {
        pContext->srcRect.left = procParam.pSrcRect->left;
        pContext->srcRect.top = procParam.pSrcRect->top;
        pContext->srcRect.right = procParam.pSrcRect->right;
        pContext->srcRect.bottom = procParam.pSrcRect->bottom;
        bSrcRectChanged = CEU_TRUE;
    }

    if(!pContext->blockTexCoords
        || bSrcSizeChanged == CEU_TRUE
        || bSrcRectChanged == CEU_TRUE
        || pContext->dstWidth != dstSurface->width
        || pContext->dstHeight != dstSurface->height
        || pContext->dstRotation != procParam.rotation
        || prevDstFormat != dstSurface->format)
    {
        pContext->dstWidth      =   dstSurface->width;
        pContext->dstHeight     =   dstSurface->height;
        pContext->dstSurfFormat =   dstSurface->format;
        pContext->dstRotation   =   procParam.rotation;
        bTexCoordChanged        =   CEU_TRUE;
        _gpu_ceu_init_block_tex_coords(pContext);
    }

    _gpu_ceu_get_stretch_factor(pContext->dstSurfFormat, &stretchX, &stretchY);

    pContext->readBuffer        =   dstSurface->pVertAddr;
    pContext->readBufferWidth   =   dstSurface->width / stretchX;
    pContext->readBufferHeight  =   dstSurface->height / stretchY;
    pContext->dstSurfFormat     =   dstSurface->format;

    if(ceuSTATUS_FAILED == _gpu_ceu_init_read_buffer(pContext))
    {
        GPU_CEU_ERROR_LOG(("%s: Set output buffer failed!", __FUNCTION__));
        return -1;
    }

    if((pContext->effectType != effect)
        || (CEU_TRUE == bTexCoordChanged)
        || (CEU_TRUE == bSrcSizeChanged)
        || (CEU_TRUE == bNeedCheckEffectParam))
    {
        pContext->effectType = effect;

        _gpu_ceu_set_effect(pContext, effect);
    }

    _gpu_ceu_render(pContext, srcSurface);

    return ret;
}

/**********************************************************
**
**  _gpu_ceu_set_effect
**
**  Set effect type and parameters.
**
**  INPUT:
**
**      CEUContext *pContext
**          GPU CEU context
**
**      gpu_effect_t effect
**          GPU CEU effect enum value.
**
**  OUTPUT:
**
**      Nothing.
*/

ceuSTATUS _gpu_ceu_set_effect(
    CEUContext *pContext,
    gpu_effect_t effect
    )
{
    ceuSTATUS ret = ceuSTATUS_SUCCESS;

    g_effects[effect].init(pContext);

    return ret;
}

/**********************************************************
**
**  _gpu_ceu_init_read_buffer
**
**  Set the dest buffer for pixels read function.
**
**  INPUT:
**
**      CEUContext *pContext
**          GPU CEU context
**
**  OUTPUT:
**
**      Nothing
*/
ceuSTATUS _gpu_ceu_init_read_buffer(
    CEUContext *pContext
    )
{
    ceuSTATUS ret = ceuSTATUS_SUCCESS;
    TIME_START();
    GLenum readBufferFormat;
    GLenum readBufferType;

    switch(pContext->dstSurfFormat)
    {
    case GPU_FORMAT_RGB565:
        readBufferFormat = GL_RGB;
        readBufferType = GL_UNSIGNED_SHORT_5_6_5;
        break;

    case GPU_FORMAT_ARGB8888:
    case GPU_FORMAT_XRGB8888:
        readBufferFormat = GL_BGRA_EXT;
        readBufferType = GL_UNSIGNED_BYTE;
        break;

    case GPU_FORMAT_ABGR8888:
    case GPU_FORMAT_XBGR8888:
    case GPU_FORMAT_UYVY:
    case GPU_FORMAT_YUY2:
        readBufferFormat = GL_RGBA;
        readBufferType = GL_UNSIGNED_BYTE;
        break;

    default:
        ret = ceuSTATUS_FAILED;
        GPU_CEU_ERROR_LOG(("Un-supported output format!"));
        goto OnError;
        break;
    }

    pContext->glSetReadBuffer((GLsizei)(pContext->readBufferWidth),
                                (GLsizei)(pContext->readBufferHeight),
                                readBufferFormat, readBufferType, pContext->readBuffer);

    if(glGetError() != GL_NO_ERROR)
    {
        ret = ceuSTATUS_FAILED;
        GPU_CEU_ERROR_LOG(("Set output buffer failed!"));
        goto OnError;
    }

    TIME_END(__FUNCTION__);

    return ret;

OnError:
    return ret;
}

/********************************************************
**
** _gpu_ceu_get_stretch_factor
**
**  stretch factor is the indicator of storage size for each format.
**
**  INPUT:
**
**      gpu_format_t format
**          gpu ceu format enum value.
**
**  OUTPUT:
**
**      CEUfloat *pStretchX
**          horizontal stretch factor.
**
**      CEUfloat *pStretchY
**          vertical stretch factor.
*/
ceuSTATUS _gpu_ceu_get_stretch_factor(
    gpu_format_t format,
    CEUfloat *pStretchX,
    CEUfloat *pStretchY)
{
    ceuSTATUS ret = ceuSTATUS_SUCCESS;

    switch(format)
    {
    case GPU_FORMAT_ARGB8888:
    case GPU_FORMAT_XRGB8888:
    case GPU_FORMAT_ABGR8888:
    case GPU_FORMAT_XBGR8888:
    case GPU_FORMAT_RGB565:
        *pStretchX = 1.0f;
        *pStretchY = 1.0f;
        break;

    case GPU_FORMAT_UYVY:
    case GPU_FORMAT_YUY2:
        *pStretchX = 2.0f;
        *pStretchY = 1.0f;
        break;

    default:
        ret = ceuSTATUS_FAILED;
        GPU_CEU_ERROR_LOG(("Un-supported output format!"));
        break;
    }

    return ret;
}

/********************************************************
** The texture coordinate for the whole input image is from (0, 0) to (1, 1),
** So the tex coordinate step from one block to the next is, 1.0 / (number of blocks)
** in each direction when process the whole image.
** When the tex coordinate of the right most block > 1.0, just let it be, because the scissor
** test in rendering will trim out the region that lay out of the image.
*/
ceuSTATUS _gpu_ceu_calculate_tex_coords(
    GLfloat* pTexCoord,
    CEUfloat fOutBlockX,
    CEUfloat fOutBlockY,
    GPU_ROTATION rotation,
    GLfloat left,
    GLfloat top,
    GLfloat right,
    GLfloat bottom
)
{
    ceuSTATUS ret = ceuSTATUS_SUCCESS;

    CEUfloat srcStepX, srcStepY;

    CEUfloat xCoordStart, yCoordStart;
    CEUfloat xCoordEnd, yCoordEnd;
    CEUbool swapXY;
    CEUfloat srcCoordWidth, srcCoordHeight;

    CEUuint nHorizontalBlocks = (GLuint)ceil((double)fOutBlockX);
    CEUuint nVerticalBlocks = (GLuint)ceil((double)fOutBlockY);

    switch(rotation)
    {
    case GPU_ROTATION_0:
        xCoordStart = left;
        xCoordEnd   = right;
        yCoordStart = bottom;
        yCoordEnd   = top;
        swapXY      = CEU_FALSE;
        break;

    case GPU_ROTATION_90:
        xCoordStart = right;
        xCoordEnd   = left;
        yCoordStart = bottom;
        yCoordEnd   = top;
        swapXY = CEU_TRUE;
        break;

    case GPU_ROTATION_180:
        xCoordStart = right;
        xCoordEnd   = left;
        yCoordStart = top;
        yCoordEnd   = bottom;
        swapXY = CEU_FALSE;
        break;

    case GPU_ROTATION_270:
        xCoordStart = left;
        xCoordEnd   = right;
        yCoordStart = top;
        yCoordEnd   = bottom;
        swapXY = CEU_TRUE;
        break;

    case GPU_ROTATION_FLIP_H:
        xCoordStart = right;
        xCoordEnd   = left;
        yCoordStart = bottom;
        yCoordEnd   = top;
        swapXY = CEU_FALSE;
        break;

    case GPU_ROTATION_FLIP_V:
        xCoordStart = left;
        xCoordEnd   = right;
        yCoordStart = top;
        yCoordEnd   = bottom;
        swapXY = CEU_FALSE;
        break;

    default:
        GPU_CEU_ERROR_LOG(("Unsupported Rotation Value!"));
        return ceuSTATUS_INVALID_ARGUMENT;
    }

    srcCoordWidth  = xCoordEnd - xCoordStart;
    srcCoordHeight = yCoordEnd - yCoordStart;

    if(CEU_FALSE == swapXY)
    {
        srcStepX = srcCoordWidth / fOutBlockX;
        srcStepY = srcCoordHeight / fOutBlockY;
    }
    else
    {
        srcStepX = srcCoordWidth / fOutBlockY;
        srcStepY = srcCoordHeight / fOutBlockX;
    }

    if(GPU_ROTATION_0 == rotation
        || GPU_ROTATION_180 == rotation
        || GPU_ROTATION_FLIP_H == rotation
        || GPU_ROTATION_FLIP_V == rotation)
    {
        for(GLuint j = 0; j < nVerticalBlocks; j++)
            for(GLuint i = 0; i < nHorizontalBlocks; i++)
            {
                *pTexCoord++ = xCoordStart + i * srcStepX;
                *pTexCoord++ = yCoordStart + j * srcStepY;

                *pTexCoord++ = xCoordStart + i * srcStepX;
                *pTexCoord++ = yCoordStart + (j+1) * srcStepY;

                *pTexCoord++ = xCoordStart + (i+1) * srcStepX;
                *pTexCoord++ = yCoordStart + (j+1) * srcStepY;

                *pTexCoord++ = xCoordStart + (i+1) * srcStepX;
                *pTexCoord++ = yCoordStart + j * srcStepY;
            }
    }
    else if(GPU_ROTATION_90 == rotation
        || GPU_ROTATION_270 == rotation)
    {
        for(GLuint i = 0; i < nVerticalBlocks; i++)
            for(GLuint j = 0; j < nHorizontalBlocks; j++)
            {
                *pTexCoord++ = xCoordStart + i * srcStepX;
                *pTexCoord++ = yCoordStart + j * srcStepY;

                *pTexCoord++ = xCoordStart + (i+1) * srcStepX;
                *pTexCoord++ = yCoordStart + j * srcStepY;

                *pTexCoord++ = xCoordStart + (i+1) * srcStepX;
                *pTexCoord++ = yCoordStart + (j+1) * srcStepY;

                *pTexCoord++ = xCoordStart + (i) * srcStepX;
                *pTexCoord++ = yCoordStart + (j+1) * srcStepY;
            }
    }

    return ret;
}

/********************************************************
**
** _gpu_ceu_init_block_tex_coords
**
**  Compute the block number needed, and calculate the texture
**  coordinate for each block.
**
**  INPUT:
**
**      CEUContext *pGpuContext
**          gpu ceu context.
**
**  OUTPUT:
**
**      Nothing
*/

ceuSTATUS _gpu_ceu_init_block_tex_coords(
    CEUContext *pGpuContext
    )
{
    ceuSTATUS ret = ceuSTATUS_SUCCESS;
    CEUuint srcWidth = pGpuContext->srcWidth;
    CEUuint srcHeight = pGpuContext->srcHeight;
    CEUuint outputWidth = pGpuContext->dstWidth;
    CEUuint outputHeight = pGpuContext->dstHeight;
    CEUfloat outStretchX = 1.0f;
    CEUfloat outStretchY = 1.0f;

    CEUfloat fOutBlockX;
    CEUfloat fOutBlockY;

    const GLfloat vertex[] = {
        -1.0f,  -1.0f,
        -1.0f,   1.0f,
         1.0f,   1.0f,
         1.0f,  -1.0f
    };

    /* This stretch factor is determined by output format, eg,
    ** when output format is UYVY, then 2 horizontal pixel generate
    ** 1 output, so need to stretch the texture coordinate by 2.
    */
    _gpu_ceu_get_stretch_factor(pGpuContext->dstSurfFormat, &outStretchX, &outStretchY);

    /* The number of block is output size divided by render buffer size,
    ** and then divide the block number by stretch factor according to
    ** output format.
    */
    fOutBlockX = (CEUfloat)outputWidth / pGpuContext->renderBufferWidth / outStretchX;
    pGpuContext->nHorizontalBlocks = (GLuint)ceil((double)fOutBlockX);
    fOutBlockY = (CEUfloat)outputHeight / pGpuContext->renderBufferHeight / outStretchY;
    pGpuContext->nVerticalBlocks = (GLuint)ceil((double)fOutBlockY);

    pGpuContext->nTotalBlocks = pGpuContext->nHorizontalBlocks * pGpuContext->nVerticalBlocks;

    /*There are nTotalBlocks position and texture coodinates pair, each pair have 4
            coordinates, and each coordinate have 2 componenets.*/
    if(pGpuContext->blockVertexPos)
    {
        gpu_ceu_free((void *)pGpuContext->blockVertexPos);
        pGpuContext->blockVertexPos = NULL;
    }
    pGpuContext->blockVertexPos = (GLfloat*)gpu_ceu_allocate(4 * 2 * pGpuContext->nTotalBlocks * sizeof(GLfloat));
    for(GLuint i = 0; i < pGpuContext->nTotalBlocks; i++)
    {
        gpu_ceu_memcpy(pGpuContext->blockVertexPos + i * 8, vertex, 8 * sizeof(GLfloat));
    }

    if(pGpuContext->blockTexCoords)
    {
        gpu_ceu_free((void*)pGpuContext->blockTexCoords);
        pGpuContext->blockTexCoords = NULL;
    }
    pGpuContext->blockTexCoords = (GLfloat*)gpu_ceu_allocate(4 * 2 * pGpuContext->nTotalBlocks * sizeof(GLfloat));

    if(pGpuContext->blockNormalizedTexCoords)
    {
        gpu_ceu_free((void*)pGpuContext->blockNormalizedTexCoords);
        pGpuContext->blockNormalizedTexCoords = NULL;
    }
    pGpuContext->blockNormalizedTexCoords = (GLfloat*)gpu_ceu_allocate(4 * 2 * pGpuContext->nTotalBlocks * sizeof(GLfloat));

    CEUfloat left, bottom, right, top;

    /* Convert image rect to texture coordinates.*/
    left = (CEUfloat)(pGpuContext->srcRect.left) / srcWidth;
    bottom = (CEUfloat)(srcHeight - pGpuContext->srcRect.bottom) / srcHeight;
    right = (CEUfloat)(pGpuContext->srcRect.right) / srcWidth;
    top = (CEUfloat)(srcHeight - pGpuContext->srcRect.top) / srcHeight;

    _gpu_ceu_calculate_tex_coords(pGpuContext->blockTexCoords, fOutBlockX, fOutBlockY,
                                    pGpuContext->dstRotation, left, top, right, bottom);

    left = 0.0f;
    bottom = 0.0f;
    right = 1.0f;
    top = 1.0f;

    _gpu_ceu_calculate_tex_coords(pGpuContext->blockNormalizedTexCoords, fOutBlockX, fOutBlockY,
                                    pGpuContext->dstRotation, left, top, right, bottom);

    return ret;
}

/********************************************************
**
** _gpu_ceu_upload_image_data
**
**  Upload source image data to hardware.
**
**  INPUT:
**
**      CEUContext *pContext
**          gpu ceu context.
**
**      CEUvoid *imgDataLogical
**          source image data logical address.
**
**      CEUvoid* imgDataPhysical
**          source image data physical address.
**
**      GPU_FORMAT srcFormat
**          source image format.
**
**      CEUuint srcWidth
**          source image width.
**
**      CEUuint srcHeight
**          source image height.
**
**      CEUuint srcStride
**          source image line stride.
**
**  OUTPUT:
**
**      Nothing
*/

ceuSTATUS _gpu_ceu_upload_image_data(
    CEUContext* pContext,
    CEUvoid* imgDataLogical,
    CEUvoid* imgDataPhysical,
    GPU_FORMAT srcFormat,
    CEUuint srcWidth,
    CEUuint srcHeight,
    CEUuint srcStride
    )
{
    ceuSTATUS ret = ceuSTATUS_SUCCESS;
    CEUbool bYUVFormat = CEU_FALSE;
    GLenum format;
    GLenum storageType;
    CEUvoid* imgAddress = CEU_NULL;
    CEUbool bUploadPhysical = CEU_FALSE;

    switch(srcFormat)
    {
    case GPU_FORMAT_ARGB8888:
    case GPU_FORMAT_XRGB8888:
        format = GL_BGRA_EXT;
        storageType = GL_UNSIGNED_BYTE;
        break;

    case GPU_FORMAT_ABGR8888:
    case GPU_FORMAT_XBGR8888:
        format = GL_RGBA;
        storageType = GL_UNSIGNED_BYTE;
        break;

    case GPU_FORMAT_RGB565:
        format = GL_RGB;
        storageType = GL_UNSIGNED_SHORT_5_6_5;
        break;

    case GPU_FORMAT_UYVY:
        format = GL_UYVY_MRVL;
        bYUVFormat = CEU_TRUE;
        break;

    case GPU_FORMAT_YUY2:
        format = GL_YUY2_MRVL;
        bYUVFormat = CEU_TRUE;
        break;

    case GPU_FORMAT_YV12:
        format = GL_YV12_MRVL;
        bYUVFormat = CEU_TRUE;
        break;

    case GPU_FORMAT_NV12:
        format = GL_NV12_MRVL;
        bYUVFormat = CEU_TRUE;
        break;

    case GPU_FORMAT_I420:
        format = GL_I420_MRVL;
        bYUVFormat = CEU_TRUE;
        break;

    default:
        ret = ceuSTATUS_FAILED;
        GPU_CEU_ERROR_LOG(("Un-supported input format!"));
        goto OnError;
    }

    if(bYUVFormat)
    {
        pContext->glTexImageVideo(GL_TEXTURE_2D, srcWidth, srcHeight, format, imgDataLogical);
    }
    else
    {
        if(GPU_PHY_NULL != imgDataPhysical)
        {
            glEnable(GL_MRVL_UPLOAD_TEXTURE_PHYSICAL);
            imgAddress = imgDataPhysical;
            bUploadPhysical = CEU_TRUE;
        }
        else if(GPU_VIR_NULL != imgDataLogical)
        {
            /*Enable hardware texture upload*/
            glEnable(GL_MRVL_UPLOAD_TEXTURE_RESOLVE);
            imgAddress = imgDataLogical;
        }
        else
        {
            ret = ceuSTATUS_FAILED;
            GPU_CEU_ERROR_LOG(("Invalid source address!"));
            goto OnError;
        }

        glTexImage2D(GL_TEXTURE_2D, 0, format, srcWidth, srcHeight, 0,
                         format, storageType, imgAddress);

        if(bUploadPhysical)
        {
            glDisable(GL_MRVL_UPLOAD_TEXTURE_PHYSICAL);
        }
        else
        {
            glDisable(GL_MRVL_UPLOAD_TEXTURE_RESOLVE);
        }
    }

    return ret;

OnError:
    return ret;
}

/********************************************************
**
** _gpu_ceu_render
**
**  Render function for GPU CEU framework.
**
**  INPUT:
**
**      CEUContext *pContext
**          gpu ceu context.
**
**      GPUSurface *src
**          source render surface.
**
**  OUTPUT:
**
**      Nothing
*/

ceuSTATUS _gpu_ceu_render(
    CEUContext *pContext,
    GPUSurface* src
    )
{
    ceuSTATUS ret = ceuSTATUS_SUCCESS;
    /*Upload texture data*/
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, pContext->imageTexture);
    _gpu_ceu_upload_image_data(pContext, src->pVertAddr, src->pPhyAddr, src->format, src->width, src->height, src->stride);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    g_effects[pContext->effectType].render(pContext);

    return ret;
}

/*********************************************************
** A common drawing function, if the effect do not draw anything special,
** it can use this drawing function.
*/
ceuSTATUS gpu_ceu_render_common(
    CEUContext* pContext
    )
{
    for(GLuint j = 0; j < pContext->nVerticalBlocks; j++)
        for(GLuint i = 0; i < pContext->nHorizontalBlocks; i++)
    {
        /* Set scissor for right-most and top-most blocks. */
        if(((pContext->nHorizontalBlocks - 1) == i)
        || ((pContext->nVerticalBlocks - 1) == j))
        {
            int scissorWidth = 0;
            int scissorHeight = 0;

            if((pContext->nHorizontalBlocks - 1) != i)
            {
                scissorWidth = pContext->renderBufferWidth;
            }
            else
            {
                scissorWidth = pContext->readBufferWidth - (i * pContext->renderBufferWidth);
            }

            if((pContext->nVerticalBlocks - 1) != j)
            {
                scissorHeight = pContext->renderBufferHeight;
            }
            else
            {
                scissorHeight = pContext->readBufferHeight - (j * pContext->renderBufferHeight);
            }

            glScissor(0, 0, scissorWidth, scissorHeight);

            glEnable(GL_SCISSOR_TEST);
        }

        glDrawArrays(GL_TRIANGLE_FAN, (j * pContext->nHorizontalBlocks + i) * 4, 4);

        pContext->glReadBuffer(i * pContext->renderBufferWidth,
                                j * pContext->renderBufferHeight,
                                pContext->renderBufferWidth,
                                pContext->renderBufferHeight);
        glFlush();

        if(((pContext->nHorizontalBlocks - 1) == i)
        || ((pContext->nVerticalBlocks - 1) == j))
        {
            glDisable(GL_SCISSOR_TEST);
        }
    }
    glFinish();

    return ceuSTATUS_SUCCESS;
}


