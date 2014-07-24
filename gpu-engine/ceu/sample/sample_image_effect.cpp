#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "gpu_ceu.h"
#include "sample_utils.h"

char app_name[] = "gc_effect";

bool _parseArguments(int argc, char** argv, char** inputImageFileName, int* pEffect)
{
    if (argc != 3)
        goto OnError;

    *inputImageFileName = argv[1];
    *pEffect = atoi(argv[2]);

    return true;

OnError:
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "%s <image file name> <image effect>\n", app_name);
    return false;
}

int main(int argc, char** argv)
{
    int ret = 0;
    char* strInputImageFileName = NULL;
    int nEffect = -1;
    unsigned char* imgData = NULL;
    unsigned int inputWidth, inputHeight;
    unsigned int outWidth, outHeight;
    float scaleFactor = 1.0f;
    unsigned int inBpp, outBpp;

    if(!_parseArguments(argc, argv, &strInputImageFileName, &nEffect))
    {
        return -1;
    }

    ceu_sample_load_bmp_as_uyvy((void**)&imgData, strInputImageFileName, &inputWidth, &inputHeight);
    inBpp = 16;
    outWidth = inputWidth * scaleFactor;
    outHeight = inputHeight * scaleFactor;

    /* Allocate continuous memory. CEU performance is better when using contiguous memory. */
    void* srcMemoryHandle = NULL;
    void* srcVirtAddr = NULL;
    unsigned int srcPhyAddr = (unsigned int)GPU_PHY_NULL;

    void* dstMemoryHandle = NULL;
    void* dstVirtAddr = NULL;
    unsigned int dstPhyAddr = (unsigned int)GPU_PHY_NULL;

    srcMemoryHandle = ceu_sample_alloc_continuous_mem(inputWidth, inputHeight, inBpp,
                        &srcVirtAddr, &srcPhyAddr);

    outBpp = 16;
    dstMemoryHandle = ceu_sample_alloc_continuous_mem(outWidth, outHeight, outBpp,
                        &dstVirtAddr, &dstPhyAddr);

    /* Check whether the memory are ready. */
    if(srcMemoryHandle && dstMemoryHandle)
    {
        /* Using continous memory to create CEU surface. */
        memcpy(srcVirtAddr, imgData, inputWidth * inputHeight * inBpp / 8);

        gpu_context_t ceuContext;
        unsigned int initFlag = 0;
        /* initFlag |= GPU_CEU_FLAG_DEBUG */

        gpu_surface_t ceuSrcSurface;
        gpu_surface_t ceuDstSurface;

        gpu_format_t ceuSrcFormat = GPU_FORMAT_UYVY;
        unsigned int inputStride = inputWidth * 2;
        gpu_format_t ceuDstFormat = GPU_FORMAT_RGB565;
        unsigned int outStride = outWidth * 2;

        GPU_PROC_PARAM procParam;
        memset(&procParam, 0, sizeof(GPU_PROC_PARAM));
        procParam.pSrcRect = NULL;
        procParam.rotation = GPU_ROTATION_0;

        ceuContext = gpu_ceu_init(initFlag);

        if(NULL != ceuContext)
        {
            ceuSrcSurface = gpu_ceu_create_surface(
                                ceuContext,
                                ceuSrcFormat,
                                inputWidth,
                                inputHeight,
                                inputStride,
                                srcVirtAddr,
                                GPU_PHY_NULL /*srcPhyAddr*/);

            ceuDstSurface = gpu_ceu_create_surface(
                                ceuContext,
                                ceuDstFormat,
                                outWidth,
                                outHeight,
                                outStride,
                                dstVirtAddr,
                                GPU_PHY_NULL /*dstPhyAddr*/);

            if(ceuSrcSurface && ceuDstSurface)
            {
                gpu_ceu_process(
                    ceuContext,
                    ceuSrcSurface,
                    ceuDstSurface,
                    (gpu_effect_t)nEffect,
                    &procParam);

                ceu_sample_dump_bmp_file(dstVirtAddr, outWidth, outHeight, outStride, outBpp, "CEU_Process");
            }

            if(ceuSrcSurface)
            {
                gpu_ceu_destroy_surface(ceuContext, ceuSrcSurface);
            }

            if(ceuDstSurface)
            {
                gpu_ceu_destroy_surface(ceuContext, ceuDstSurface);
            }
        }

        if(ceuContext)
        {
            gpu_ceu_deinit(ceuContext);
            ceuContext = NULL;
        }
    }

    if(dstMemoryHandle)
    {
        ceu_sample_free_continuous_mem(dstMemoryHandle);
    }

    if(srcMemoryHandle)
    {
        ceu_sample_free_continuous_mem(srcMemoryHandle);
    }

    free((void*)imgData);
    return ret;
}

