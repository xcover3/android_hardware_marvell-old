#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "gpu_ceu.h"
#include "sample_utils.h"

char app_name[] = "RGB2UYVY";

bool _parseArguments(int argc, char** argv, char** inputImageFileName)
{
    if (argc != 2)
        goto OnError;

    *inputImageFileName = argv[1];

    return true;

OnError:
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "%s <image file name>\n", app_name);
    return false;
}

int main(int argc, char** argv)
{
    int ret = 0;
    char* strInputImageFileName = NULL;
    unsigned char* imgData = NULL;
    unsigned int inputWidth, inputHeight;
    unsigned int outWidth, outHeight;
    float scaleFactor = 1.0f;
    unsigned int inBpp, outBpp;

    if(!_parseArguments(argc, argv, &strInputImageFileName))
    {
        return -1;
    }

    ceu_sample_load_bmp_as_rgb565((void**)&imgData, strInputImageFileName, &inputWidth, &inputHeight);
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

        gpu_format_t ceuSrcFormat = GPU_FORMAT_RGB565;
        unsigned int inputStride = inputWidth * 2;
        gpu_format_t ceuDstFormat = GPU_FORMAT_UYVY;
        unsigned int outStride = outWidth * 2;

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
                    GPU_RGB2UYVY,
                    NULL);

                ceu_sample_dump_yuv_file(dstVirtAddr, outWidth * outHeight * 2, "CEU_RGB2UYVY");
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

