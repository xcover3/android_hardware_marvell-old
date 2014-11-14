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
#include "gpu_ceu_utils.h"

#ifdef ANDROID

#define SK_SUPPORT_LEGACY_SETCONFIG

#include <SkImageDecoder.h>
#include <SkImageEncoder.h>
#include <SkBitmap.h>
#include <android/log.h>
#include <unistd.h>
#include <sys/types.h>
#include <androidfw/AssetManager.h>
#endif

ceuSTATUS _gpu_ceu_chooseConfigExact(
    EGLNativeDisplayType display,
    EGLConfig* config,
    EGLint attrib_list[],
    CEUint r,
    CEUint g,
    CEUint b,
    CEUint a
    )
{
    ceuSTATUS ret = ceuSTATUS_FAILED;
    EGLConfig* configs = NULL;
    int i;
    EGLint totalConfigs = 0;
    EGLint retConfigs = 0;

    if(NULL == config)
    {
        return ceuSTATUS_INVALID_ARGUMENT;
    }

    *config = (EGLConfig)0;

    EGLint returnVal = eglGetConfigs(display, NULL, 0, &totalConfigs);
    if((EGL_FALSE == returnVal) || (totalConfigs == 0))
    {
        return ceuSTATUS_INVALID_ARGUMENT;
    }

    configs = (EGLConfig*)gpu_ceu_allocate(sizeof(EGLConfig) * totalConfigs);
    if(NULL == configs)
    {
        GPU_CEU_ERROR_LOG(("Can't allocate memory!"));
        return ceuSTATUS_FAILED;
    }

    returnVal = eglChooseConfig(display, attrib_list, configs, totalConfigs, &retConfigs );

    if((EGL_FALSE != returnVal) && (retConfigs != 0))
    {
        for(i = 0; i < retConfigs; i++)
        {
            int config_r, config_g, config_b, config_a;
            EGLint value;

            eglGetConfigAttrib(display, configs[i], EGL_RED_SIZE, &value);
            config_r = value;
            eglGetConfigAttrib(display, configs[i], EGL_GREEN_SIZE, &value);
            config_g = value;
            eglGetConfigAttrib(display, configs[i], EGL_BLUE_SIZE, &value);
            config_b = value;
            eglGetConfigAttrib(display, configs[i], EGL_ALPHA_SIZE, &value);
            config_a = value;

            if((config_r == r) &&
                (config_g == g)&&
                (config_b == b)&&
                (config_a == a)
                )
            {
                *config = configs[i];
                ret = ceuSTATUS_SUCCESS;
                break;
            }
        }
    }

    gpu_ceu_free((void*)configs);

    return ret;
}

GLuint _ceu_load_shader(
    GLenum shaderType,
    const char* pSource
    )
{
    GLuint shader = glCreateShader(shaderType);
    if (shader)
    {
        glShaderSource(shader, 1, &pSource, NULL);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled)
        {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen)
            {
                char* buf = (char*) gpu_ceu_allocate(infoLen);
                if (buf)
                {
                    glGetShaderInfoLog(shader, infoLen, NULL, buf);
                    GPU_CEU_ERROR_LOG(("Could not compile shader %d:\n%s",
                                        shaderType, buf));
                    gpu_ceu_free(buf);
                }
                glDeleteShader(shader);
                shader = 0;
            }
        }
    }
    return shader;
}

GLuint _ceu_create_program(
    GLuint vertShader,
    GLuint fragShader
    )
{
    GLuint program;

    program = glCreateProgram();
    if (program)
    {
        glAttachShader(program, vertShader);
        glAttachShader(program, fragShader);
        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE)
        {
            GLint bufLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength)
            {
                char* buf = (char*) gpu_ceu_allocate(bufLength);
                if (buf)
                {
                    glGetProgramInfoLog(program, bufLength, NULL, buf);
                    GPU_CEU_ERROR_LOG(("Could not link program:\n%s", buf));
                    gpu_ceu_free(buf);
                }
            }
            glDeleteProgram(program);
            program = 0;
        }
    }
    return program;
}

#ifndef ANDROID
#ifndef BI_RGB

#define BI_RGB        0L
#define BI_RLE8       1L
#define BI_RLE4       2L
#define BI_BITFIELDS  3L

typedef struct tagBITMAPINFOHEADER{
    unsigned int    biSize;
    int             biWidth;
    int             biHeight;
    short           biPlanes;
    short           biBitCount;
    unsigned int    biCompression;
    unsigned int    biSizeImage;
    int             biXPelsPerMeter;
    int             biYPelsPerMeter;
    unsigned int    biClrUsed;
    unsigned int    biClrImportant;
} __attribute((packed)) BITMAPINFOHEADER;

typedef struct tagBITMAPFILEHEADER {
    short           bfType;
    unsigned int    bfSize;
    short           bfReserved1;
    short           bfReserved2;
    unsigned int    bfOffBits;
} __attribute((packed)) BITMAPFILEHEADER;

#endif
#endif

CEUbool _ceu_load_image_as_rgb565(
    void ** data,
    const char* filePathName,
    CEUint* imgWidth,
    CEUint* imgHeight
    )
{
#ifndef ANDROID
    BITMAPFILEHEADER    bmFileHeader;
    BITMAPINFOHEADER    bmInfoHeader;
    FILE                *pFile = NULL;
    unsigned char       *pImageData = NULL;


    pFile = fopen(filePathName, "rb");
    if(!pFile)
    {
        GPU_CEU_ERROR_LOG(("Can not open file %s\n", filePathName));
        return CEU_FALSE;
    }

    fread(&bmFileHeader, sizeof(BITMAPFILEHEADER), 1, pFile);
    fread(&bmInfoHeader, sizeof(BITMAPINFOHEADER), 1, pFile);

    if(bmInfoHeader.biBitCount != 24)
    {
        GPU_CEU_ERROR_LOG(("Only 24bit color depth bitmap supported!\n"));
        return CEU_FALSE;
    }

    pImageData = (unsigned char*)gpu_ceu_allocate(bmInfoHeader.biSizeImage);
    if(pImageData == NULL)
    {
        GPU_CEU_ERROR_LOG(("%s(%d): Allocate memory error!\n", __FILE__, __LINE__));
        return CEU_FALSE;
    }
    fread(pImageData, 1, bmInfoHeader.biSizeImage, pFile);  //here data is BGR
    fclose(pFile);

    *data = (unsigned char*) gpu_ceu_allocate_align(GPU_ADDR_ALIGN_BYTES,
                                                bmInfoHeader.biWidth * bmInfoHeader.biHeight * 2);
    /*Transform BGR data to RGB565*/
    unsigned short* pOutData = (unsigned short*)*data;
    unsigned char* pInData = pImageData;
    for(int j = 0; j < bmInfoHeader.biHeight; j++)
        for(int i = 0; i < bmInfoHeader.biWidth; i++)
        {
            unsigned char R5, G6, B5;
            R5 = (unsigned char)(*(pInData + 2)/255.0*31);
            G6 = (unsigned char)(*(pInData + 1)/255.0*63);
            B5 = (unsigned char)(*(pInData)/255.0*31);

            *pOutData = (unsigned short)0;
            *pOutData |= (R5 & 0x001f) << 11;
            *pOutData |= (G6 & 0x003f) << 5;
            *pOutData |= (B5 & 0x001f);
            pOutData++;
            pInData += 3;
        }

    *imgWidth = bmInfoHeader.biWidth;
    *imgHeight = bmInfoHeader.biHeight;

    gpu_ceu_free((void*)pImageData);

    return CEU_TRUE;
#else
    android::AssetManager resManager;
    android::Asset * asset;
    android::String8 path("/system/app/GPUEngine-CEU-Res.apk");
    SkBitmap bm;

    if(!resManager.addAssetPath(path, NULL))
    {
        GPU_CEU_ERROR_LOG(("Open Resource Package failed!\n"));
        return CEU_FALSE;
    }

    asset = resManager.open(filePathName, android::Asset::ACCESS_BUFFER);
    if(!asset)
    {
        GPU_CEU_ERROR_LOG(("Failed to Open Resource File: %s", filePathName));
        return CEU_FALSE;
    }

    if(SkImageDecoder::DecodeMemory(
                        asset->getBuffer(false),
                        asset->getLength(),
                        &bm,
                        kRGB_565_SkColorType,
                        SkImageDecoder::kDecodePixels_Mode,
                        NULL))
    {
        CEUint imgRowBytes = bm.rowBytes();
        *imgWidth = bm.width();
        *imgHeight = bm.height();

        *data = gpu_ceu_allocate_align(GPU_ADDR_ALIGN_BYTES, (*imgHeight) * imgRowBytes);
        if(GPU_VIR_NULL == *data)
        {
            GPU_CEU_ERROR_LOG(("Can not allocate memory!"));
            return CEU_FALSE;
        }

        gpu_ceu_memcpy(*data, bm.getPixels(), (*imgHeight) * imgRowBytes);

        return CEU_TRUE;
    }

    return CEU_FALSE;
#endif
}

CEUbool _ceu_load_image_as_rgba8888(
    void ** data,
    const char* filePathName,
    CEUint* imgWidth,
    CEUint* imgHeight
    )
{
#ifdef ANDROID
    android::AssetManager resManager;
    android::Asset * asset;
    android::String8 path("/system/app/GPUEngine-CEU-Res.apk");
    SkBitmap bm;

    if(!resManager.addAssetPath(path, NULL))
    {
        GPU_CEU_ERROR_LOG(("Open Resource Package failed!\n"));
        return CEU_FALSE;
    }

    asset = resManager.open(filePathName, android::Asset::ACCESS_BUFFER);
    if(!asset)
    {
        GPU_CEU_ERROR_LOG(("Failed to Open Resource File: %s", filePathName));
        return CEU_FALSE;
    }

    if(SkImageDecoder::DecodeMemory(
                        asset->getBuffer(false),
                        asset->getLength(),
                        &bm,
                        kRGBA_8888_SkColorType,  // fix me, should be ARGB_8888
                        SkImageDecoder::kDecodePixels_Mode,
                        NULL))
    {
        CEUint imgRowBytes = bm.rowBytes();
        *imgWidth = bm.width();
        *imgHeight = bm.height();

        *data = gpu_ceu_allocate_align(GPU_ADDR_ALIGN_BYTES, (*imgHeight) * imgRowBytes);
        if(GPU_VIR_NULL == *data)
        {
            GPU_CEU_ERROR_LOG(("Can not allocate memory!"));
            return CEU_FALSE;
        }

        gpu_ceu_memcpy(*data, bm.getPixels(), (*imgHeight) * imgRowBytes);

        return CEU_TRUE;
    }

    return CEU_FALSE;
#else
    return CEU_FALSE;
#endif
}

ceuSTATUS _ceu_dump_image_from_memory(
    CEUvoid * srcMemory,
    CEUint width,
    CEUint height,
    CEUint stride,
    gpu_format_t format,
    const CEUchar * outputFile
    )
{
    ceuSTATUS ret = ceuSTATUS_FAILED;

#ifdef ANDROID
    SkBitmap bm;
    SkBitmap::Config config;

    switch(format)
    {
    case GPU_FORMAT_RGB565:
        config = SkBitmap::kRGB_565_Config;
        break;

    case GPU_FORMAT_RGBA8888:
        config = SkBitmap::kARGB_8888_Config;
        break;

    default:
        GPU_CEU_ERROR_LOG(("%s: Unsupported Dump format!", __FUNCTION__));
        return ceuSTATUS_FAILED;
    }

    bm.setConfig(config, width, height, stride, kIgnore_SkAlphaType);

    bm.setPixels(srcMemory);

    if(!SkImageEncoder::EncodeFile(outputFile, bm, SkImageEncoder::kJPEG_Type, 80))
    {
        GPU_CEU_ERROR_LOG(("%s: Can not dump data to file : %s", __FUNCTION__, outputFile));
    }
    else
    {
        GPU_CEU_LOG(("%s: Succeeded dumping data to file : %s", __FUNCTION__, outputFile));
        ret = ceuSTATUS_SUCCESS;
    }
#endif

    return ret;
}

void *gpu_ceu_allocate(
    size_t size
    )
{
    return malloc(size);
}

void *gpu_ceu_allocate_align(
    size_t align,
    size_t size
    )
{
    return memalign(align, size);
}

void gpu_ceu_free(
    void* ptr
    )
{
    free(ptr);
}

void * gpu_ceu_memcpy(
    void * destination,
    const void * source,
    size_t num)
{
    return memcpy(destination, source, num);
}

bool _isAligned(
    const void* ptr,
    unsigned int alignment
    )
{
    return !((unsigned int)ptr & (alignment - 1));
}

void *gpu_ceu_memset(
    void *dest,
    unsigned char c,
    size_t count
    )
{
    return memset(dest, c, count);
}

void _ceuPrint(
    const CEUchar *messageFormat,
    va_list Arguments
    )
{
    char buffer[1024];
    int n;

    n = vsnprintf(buffer, sizeof(buffer) - 1, messageFormat, Arguments);

    if((n <= 0) || (buffer[n - 1] != '\n'))
    {
        strncat(buffer, "\n", 1);
    }

#ifdef ANDROID
    __android_log_print(ANDROID_LOG_DEBUG, "libceu", "<tid=%d> %s", _ceu_get_current_thread_id(), buffer);
#else
    fprintf(stderr, "%s", buffer);
#endif
}

void gpu_ceu_log_print(const char* format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    _ceuPrint(format, arguments);
    va_end(arguments);
}

#ifdef ANDROID
pid_t _ceu_get_current_thread_id(
    void
    )
{
    return gettid();
}
#else
CEUint _ceu_get_current_thread_id(
    void
    )
{
    return 0;
}
#endif

/* Check if current thread equals to the one create the context. */
CEUbool _isCreatorThread(
    CEUContext * pContext
    )
{
    if(_ceu_get_current_thread_id() == pContext->nCreatorThreadID)
    {
        return CEU_TRUE;
    }
    else
    {
        return CEU_FALSE;
    }
}

