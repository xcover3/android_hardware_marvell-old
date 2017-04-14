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

#ifndef _GPU_CEU_UTILS_H_
#define _GPU_CEU_UTILS_H_

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "gpu_ceu.h"
#include "gpu_ceu_types.h"

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GPU_ADDR_ALIGN_BYTES 64

/*choose egl config*/
ceuSTATUS _gpu_ceu_chooseConfigExact(
    EGLNativeDisplayType display,
    EGLConfig* config,
    EGLint attrib_list[],
    CEUint r,
    CEUint g,
    CEUint b,
    CEUint a
    );

/*Generate shader*/
GLuint _ceu_load_shader(GLenum shaderType, const char* pSource);
GLuint _ceu_create_program(GLuint vertShader, GLuint fragShader);

CEUbool _ceu_load_image_as_rgb565(
    void ** data,
    const char* filePathName,
    CEUint* imgWidth,
    CEUint* imgHeight
    );

CEUbool _ceu_load_image_as_rgba8888(
    void ** data,
    const char* filePathName,
    CEUint* imgWidth,
    CEUint* imgHeight
    );

ceuSTATUS _ceu_dump_image_from_memory(
    CEUvoid * srcMemory,
    CEUint width,
    CEUint height,
    CEUint stride,
    gpu_format_t format,
    const CEUchar * outputFile
    );

/* Get current thread ID. */
#ifdef ANDROID
pid_t _ceu_get_current_thread_id(void);
#else
CEUint _ceu_get_current_thread_id(void);
#endif

/* Check if current thread equals to the one create the context. */
CEUbool _isCreatorThread(
    CEUContext * pContext
    );

/*Test if address is aligned*/
bool _isAligned(
    const void* ptr,
    unsigned int alignment
    );

/*Allocate memory from heap*/
void *gpu_ceu_allocate(
    size_t size
    );

/*Allocate address aligned memory from heap*/
void *gpu_ceu_allocate_align(
    size_t align,
    size_t size
    );

/*Free Allocated memory*/
void gpu_ceu_free(
    void* ptr
    );

/*Copy memory*/
void * gpu_ceu_memcpy(
    void * destination,
    const void * source,
    size_t num);

/* Set buffers to a specific character. */
void *gpu_ceu_memset(
    void *dest,
    unsigned char c,
    size_t count
    );

/* Print function. */
void _ceuPrint(
    const CEUchar *messageFormat,
    va_list Arguments
    );

/* print log */
void gpu_ceu_log_print(
    const char* format,
    ...
    );

#define CEU_CLAMP(x, min, max)  (((x) < (min)) ? (min) : \
                                ((x) > (max)) ? (max) : (x))

/******************************************************************\
*************************** Log related macros ********************
\******************************************************************/

#define GPU_CEU_ENABLE_API_LOG              0
#define GPU_CEU_ENABLE_TIMING               0

#if GPU_CEU_ENABLE_TIMING
#define TIME_START() { \
    clock_t timeBegin = clock();

#define TIME_END(outString) timeBegin = clock() - timeBegin; \
    GPU_CEU_LOG(("%s Time Consumption : %f ms\n", \
    (outString), (float)timeBegin * 1000.0f / CLOCKS_PER_SEC)); \
    }
#else
#define TIME_START()
#define TIME_END(outString)
#endif

#if GPU_CEU_ENABLE_API_LOG
#define GPU_CEU_API_LOG(x)      gpu_ceu_log_print x
#else
#define GPU_CEU_API_LOG(x)
#endif

#define GPU_CEU_LOG(x)          gpu_ceu_log_print x
#define GPU_CEU_ERROR_LOG(x)    gpu_ceu_log_print x

#ifdef __cplusplus
}
#endif


#endif

