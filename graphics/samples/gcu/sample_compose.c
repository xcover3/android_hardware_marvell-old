#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WINCE
#include <windows.h>
#endif

#include "gcu.h"

GCUvoid _gcuCompose(GCUContext pContext, GCU_COMPOSE_DATA* pData)
{
    GCUuint index, endIndex = pData->srcLayerCount;
    GCUComposeLayer* pSrcLayers = pData->pSrcLayers;
    GCU_BLEND_DATA   blendData;
    GCUComposeLayer* pSrcLayer;
    memset(&blendData, 0, sizeof(blendData));
    blendData.pDstSurface = pData->pDstSurface;
    blendData.pClipRect   = pData->pClipRect;

    for(index = 0; index < endIndex; ++index)
    {
        pSrcLayer = pSrcLayers + index;
        blendData.pDstRect       = pSrcLayer->pDstRect;
        blendData.pSrcSurface    = pSrcLayer->pSrcSurface;
        blendData.pSrcRect       = pSrcLayer->pSrcRect;
        blendData.rotation       = pSrcLayer->rotation;
        blendData.blendMode      = pSrcLayer->blendMode;
        blendData.srcGlobalAlpha = pSrcLayer->srcGlobalAlpha;
        blendData.dstGlobalAlpha = pSrcLayer->dstGlobalAlpha;
        gcuBlend(pContext, &blendData);
    }
}


#ifdef WINCE
int WINAPI WinMain (
    HINSTANCE   hInstance,
    HINSTANCE   hPrevInstance,
    LPTSTR      lpCmdLine,
    int         nCmdShow)
#else
int main(int argc, char** argv)
#endif
{
    if(argc < 2)
    {
        return 0;
    }
    else
    {
        GCUenum result;

        GCUContext pContextA;

        GCUSurface pSrc[20];
        GCUSurface pARGBSrc;
        GCUSurface pGREYSrc;
        GCUSurface pDst[10];
        GCUSurface pDumpDst;
        GCUSurface pClearDst;

        GCUVirtualAddr      srcVirtAddr = 0;
        GCUPhysicalAddr     srcPhysAddr = 0;
        GCUVirtualAddr      dstVirtAddr = 0;
        GCUPhysicalAddr     dstPhysAddr = 0;

        GCU_INIT_DATA initData;
        GCU_CONTEXT_DATA contextData;
        GCU_FILL_DATA fillData;
        GCU_BLT_DATA bltData;

        unsigned int        width       = 512;
        unsigned int        height      = 512;
        unsigned int        dstwidth    = 128;
        unsigned int        dstheight   = 128;

        GCU_RECT srcRects[20] = {
            {300, 312, 451, 451},
            {51, 51, 71, 81},
            {153, 153, 218, 290},
            {35, 35, 85, 135},
            {426, 431, 510, 451},
            {51, 351, 151, 451},
            {401, 91, 451, 151},
            {425, 224, 451, 251},
            {347, 186, 351, 191},
            {201, 483, 221, 501},
            {401, 121, 501, 221},
            {357, 95, 457, 195},
            {161, 251, 261, 351},
            {391, 351, 491, 451},
            {201, 201, 301, 301},
            {301, 301, 401, 401},
            {261, 151, 361, 251},
            {1, 1, 10, 10},
            {1, 2, 27, 8},
            {301, 103, 401, 109},
        };

        GCU_RECT dstRects[20] = {
            {300, 312, 451, 451},
            {51, 51, 71, 81},
            {153, 153, 218, 290},
            {35, 35, 85, 135},
            {426, 431, 510, 451},
            {51, 351, 151, 451},
            {401, 91, 451, 151},
            {425, 224, 451, 251},
            {347, 186, 351, 191},
            {201, 483, 221, 501},
            {401, 121, 501, 221},
            {357, 95, 457, 195},
            {161, 251, 261, 351},
            {391, 351, 491, 451},
            {201, 201, 301, 301},
            {301, 301, 401, 401},
            {261, 151, 361, 251},
            {1, 1, 10, 10},
            {1, 2, 27, 8},
            {301, 103, 401, 109},
        };

        GCU_RECT clipRects[1] = {
            {12, 14, 100, 106},
        };
        /*
        GCU_RECT srcRects[20] = {
            {0,  0,  64,   64},
            {32,  32,  96,   96},
            {448,  192,  512,   256},
            {0,  64,  64,   128},
            {64,  64,  128,   128},
            {128,  0,  192,   64},
            {0,  128,  64,   192},
            {128,  128,  192,   192},
            {192,  0,  256,   64},
            {192,  192,  256,   256},
            {64,  192,  128,   256},
            {128,  256,  192,   320},
            {320,  0,  384,   64},
            {0,  384,  64,   448},
            {192,  384,  256,   448},
            {448,  0,  512,   64},
            {0,  448,  64,   512},
            {448,  448,  512,   512},
            {192,  448,  256,   512},
            {448,  256,  512,   320},
        };

        GCU_RECT dstRects[16] = {
            {16,  16,  80,   80},
        };

        GCU_RECT clipRects[16] = {
            {16,  16,  80,   80},
        };
        */
        GCU_RECT srcRect = {0, 0, 2, 1};

        GCU_FORMAT formats[10] = {
            GCU_FORMAT_UYVY,
            GCU_FORMAT_YUY2,
            GCU_FORMAT_YV12,
            GCU_FORMAT_NV12,
            GCU_FORMAT_I420,
            GCU_FORMAT_NV16,
            GCU_FORMAT_NV21,
            GCU_FORMAT_NV61,
            GCU_FORMAT_RGBA8888,
            GCU_FORMAT_RGB565
        };

        GCUint i = 0, j = 0, k = 0, l = 0;
        GCU_COMPOSE_DATA compose;
        GCUuint          srcLayerCount = 0;
        GCUComposeLayer*    pSrcLayers;


        // Init GCU library
        memset(&initData, 0, sizeof(initData));
        initData.debug = 1;
        gcuInitialize(&initData);

        // Create Context
        memset(&contextData, 0, sizeof(contextData));
        pContextA = gcuCreateContext(&contextData);
        if(pContextA == NULL)
        {
            result = gcuGetError();
            exit(0);
        }

        pSrc[0] = _gcuLoadYUVSurfaceFromFile(pContextA, "UYVY.yuv", formats[0], width, height);
        pSrc[1] = _gcuLoadYUVSurfaceFromFile(pContextA, "YUY2.yuv", formats[1], width, height);
        pSrc[2] = _gcuLoadYUVSurfaceFromFile(pContextA, "YV12.yuv", formats[2], width, height);
        pSrc[3] = _gcuLoadYUVSurfaceFromFile(pContextA, "NV12.yuv", formats[3], width, height);
        pSrc[4] = _gcuLoadYUVSurfaceFromFile(pContextA, "I420.yuv", formats[4], width, height);
        pSrc[5] = _gcuLoadYUVSurfaceFromFile(pContextA, "NV16.yuv", formats[5], width, height);
        pSrc[6] = _gcuLoadYUVSurfaceFromFile(pContextA, "NV21.yuv", formats[6], width, height);
        pSrc[7] = _gcuLoadYUVSurfaceFromFile(pContextA, "NV61.yuv", formats[7], width, height);
        pSrc[8] = _gcuCreateBuffer(pContextA, width, height, formats[8], &srcVirtAddr, &srcPhysAddr);
        pSrc[9] = _gcuCreateBuffer(pContextA, width, height, formats[9], &dstVirtAddr, &dstPhysAddr);
        pSrc[10] = _gcuCreateBuffer(pContextA, width, height, GCU_FORMAT_BGRA8888, &dstVirtAddr, &dstPhysAddr);
        pSrc[11] = _gcuCreateBuffer(pContextA, width, height, GCU_FORMAT_ARGB8888, &dstVirtAddr, &dstPhysAddr);
        pSrc[12] = _gcuCreateBuffer(pContextA, width, height, GCU_FORMAT_ABGR8888, &dstVirtAddr, &dstPhysAddr);
        pSrc[13] = _gcuCreateBuffer(pContextA, width, height, GCU_FORMAT_ARGB4444, &dstVirtAddr, &dstPhysAddr);
        pSrc[14] = _gcuCreateBuffer(pContextA, width, height, GCU_FORMAT_ABGR1555, &dstVirtAddr, &dstPhysAddr);
        pSrc[15] = _gcuCreateBuffer(pContextA, width, height, GCU_FORMAT_BGRA8888, &dstVirtAddr, &dstPhysAddr);
        pSrc[16] = _gcuCreateBuffer(pContextA, width, height, GCU_FORMAT_ARGB8888, &dstVirtAddr, &dstPhysAddr);
        pSrc[17] = _gcuCreateBuffer(pContextA, width, height, GCU_FORMAT_ABGR8888, &dstVirtAddr, &dstPhysAddr);
        pSrc[18] = _gcuCreateBuffer(pContextA, width, height, GCU_FORMAT_ARGB4444, &dstVirtAddr, &dstPhysAddr);
        pSrc[19] = _gcuCreateBuffer(pContextA, width, height, GCU_FORMAT_ABGR1555, &dstVirtAddr, &dstPhysAddr);

        pARGBSrc = _gcuLoadRGBSurfaceFromFile(pContextA, "ARGB.bmp");
        pGREYSrc = _gcuLoadRGBSurfaceFromFile(pContextA, "ARGB_grey.bmp");
        pDumpDst = _gcuCreateBuffer(pContextA, dstwidth, dstheight, GCU_FORMAT_ARGB8888, &dstVirtAddr, &dstPhysAddr);
        pClearDst = _gcuCreateBuffer(pContextA, 16, 4, GCU_FORMAT_UYVY, &dstVirtAddr, &dstPhysAddr);

        *(unsigned int*)dstVirtAddr = 0x0f0f0f0f;

        /* dump src */
        memset(&bltData, 0, sizeof(bltData));
        bltData.pSrcSurface = pARGBSrc;
        bltData.pDstSurface = pSrc[8];
        bltData.pSrcRect = NULL;
        bltData.pDstRect = NULL;
        bltData.rotation = GCU_ROTATION_0;
        gcuBlit(pContextA, &bltData);

        gcuFinish(pContextA);
        for(i = 0; i < height; i++)
        {
            for(j = 0; j < width; j++)
            {
                unsigned char* ps = (unsigned char*)srcVirtAddr;
                ps += (j + i *width) * 4;
                *ps = (unsigned char)(i+j);
            }
        }

        bltData.pSrcSurface = pSrc[8];
        bltData.pDstSurface = pSrc[9];
        gcuBlit(pContextA, &bltData);

        bltData.pSrcSurface = pSrc[8];
        bltData.pDstSurface = pSrc[10];
        gcuBlit(pContextA, &bltData);

        bltData.pSrcSurface = pSrc[8];
        bltData.pDstSurface = pSrc[11];
        gcuBlit(pContextA, &bltData);

        bltData.pSrcSurface = pSrc[8];
        bltData.pDstSurface = pSrc[12];
        gcuBlit(pContextA, &bltData);

        bltData.pSrcSurface = pSrc[8];
        bltData.pDstSurface = pSrc[13];
        gcuBlit(pContextA, &bltData);

        bltData.pSrcSurface = pSrc[8];
        bltData.pDstSurface = pSrc[14];
        gcuBlit(pContextA, &bltData);

        bltData.pSrcSurface = pGREYSrc;
        bltData.pDstSurface = pSrc[8];
        gcuBlit(pContextA, &bltData);

        gcuFinish(pContextA);
        for(i = 0; i < height; i++)
        {
            for(j = 0; j < width; j++)
            {
                unsigned char* ps = (unsigned char*)srcVirtAddr;
                ps += (width-j + (height-i) *width) * 4;
                *ps = (unsigned char)(i+j);
            }
        }

        bltData.pSrcSurface = pSrc[8];
        bltData.pDstSurface = pSrc[15];
        gcuBlit(pContextA, &bltData);

        bltData.pSrcSurface = pSrc[8];
        bltData.pDstSurface = pSrc[16];
        gcuBlit(pContextA, &bltData);

        bltData.pSrcSurface = pSrc[8];
        bltData.pDstSurface = pSrc[17];
        gcuBlit(pContextA, &bltData);

        bltData.pSrcSurface = pSrc[8];
        bltData.pDstSurface = pSrc[18];
        gcuBlit(pContextA, &bltData);

        bltData.pSrcSurface = pSrc[8];
        bltData.pDstSurface = pSrc[19];
        gcuBlit(pContextA, &bltData);

        bltData.pSrcSurface = pSrc[10];
        bltData.pDstSurface = pSrc[8];
        gcuBlit(pContextA, &bltData);

        gcuFinish(pContextA);

    //    _gcuDumpSurface(pContextA, pARGBSrc, "./result_stretch/pSrc");

        gcuSet(pContextA, GCU_CLIP_RECT, GCU_TRUE);
        gcuSet(pContextA, GCU_QUALITY, GCU_QUALITY_NORMAL);


        for(i = 0; i < 10; i++)
        {
            pDst[i] = _gcuCreateBuffer(pContextA, dstwidth, dstheight, formats[i], &dstVirtAddr, &dstPhysAddr);
        }

        {
            srcLayerCount = argc > 20 ? 20 : argc - 1;
            pSrcLayers = (GCUComposeLayer*)malloc(srcLayerCount * sizeof(*pSrcLayers));
            memset(pSrcLayers, 0, srcLayerCount * sizeof(*pSrcLayers));
            for(i = 0; i < srcLayerCount; i++)
            {
                int index = atoi(argv[i+1]);
                int bStretch = 0;
                int w, h;
                if(index < 0)
                {
                    bStretch = 1;
                    index = -index;
                }
                index = index > 19 ? 19 : index;
                pSrcLayers[i].pSrcSurface = pSrc[index];
                pSrcLayers[i].pSrcRect = srcRects + i;
                pSrcLayers[i].rotation = 3;
                pSrcLayers[i].srcGlobalAlpha = 255- (i+1) * 255 / (srcLayerCount+1);
                if(index < 8){srcRects[i].left &= ~1,srcRects[i].right &= ~1,srcRects[i].top &= ~1,srcRects[i].bottom &= ~1,pSrcLayers[i].srcGlobalAlpha = 255;}
                dstRects[i].left = 10 + (i+1) * 80 / (srcLayerCount+1);
                dstRects[i].top  = 10 + (i+1) * 80 / (srcLayerCount+1);
                if(pSrcLayers[i].rotation & GCU_ROTATION_90)
                {
                    h = srcRects[i].right - srcRects[i].left;
                    w = srcRects[i].bottom - srcRects[i].top;
                }
                else
                {
                    w = srcRects[i].right - srcRects[i].left;
                    h = srcRects[i].bottom - srcRects[i].top;
                }
                if(bStretch) ++w, ++h;
                dstRects[i].right = dstRects[i].left + w;
                dstRects[i].bottom = dstRects[i].top + h;
                pSrcLayers[i].pDstRect = dstRects + i;
                pSrcLayers[i].dstGlobalAlpha = 255;
                pSrcLayers[i].blendMode = GCU_BLEND_SRC_OVER;
            }
            compose.pClipRect = clipRects;
            compose.pDstSurface = pDst[8];
            compose.pSrcLayers = pSrcLayers;
            compose.srcLayerCount = srcLayerCount;
            GCU_FILL_DATA fill = {pDst[8], NULL, 1, 0xFF00FF00, NULL};

            gcuFill(pContextA, &fill);
            gcuCompose(pContextA, &compose);
            gcuFinish(pContextA);
            _gcuDumpSurface(pContextA, pDst[8], "gcuCompose");

            gcuFill(pContextA, &fill);
            _gcuCompose(pContextA, &compose);
            gcuFinish(pContextA);
            _gcuDumpSurface(pContextA, pDst[8], "gcuCompose");
        gcuSet(pContextA, GCU_QUALITY, GCU_QUALITY_HIGH);

            gcuFill(pContextA, &fill);
            gcuCompose(pContextA, &compose);
            gcuFinish(pContextA);
            _gcuDumpSurface(pContextA, pDst[8], "gcuCompose");

            gcuFill(pContextA, &fill);
            _gcuCompose(pContextA, &compose);
            gcuFinish(pContextA);
            _gcuDumpSurface(pContextA, pDst[8], "gcuCompose");
        }

        for(i = 0; i < 10; i++)
        {
            if(pDst[i])
            {
                _gcuDestroyBuffer(pContextA, pDst[i]);
                pDst[i] = NULL;
            }
        }

        for(i = 0; i < 20; i++)
        {
            if(pSrc[i])
            {
                _gcuDestroyBuffer(pContextA, pSrc[i]);
                pSrc[i] = NULL;
            }
        }

        if(pARGBSrc)
        {
            _gcuDestroyBuffer(pContextA, pARGBSrc);
            pARGBSrc = NULL;
        }

        if(pGREYSrc)
        {
            _gcuDestroyBuffer(pContextA, pGREYSrc);
            pGREYSrc = NULL;
        }

        if(pDumpDst)
        {
            _gcuDestroyBuffer(pContextA, pDumpDst);
            pDumpDst = NULL;
        }
        gcuDestroyContext(pContextA);
        gcuTerminate();
    }
    return 0;
}
