#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WINCE
#include <windows.h>
#endif

#include "gcu.h"


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
    GCUenum result;

    GCUContext pContextA;

    GCUSurface pSrcARGBSurface      = NULL;
    GCUSurface pSrcSurface          = NULL;
    GCUSurface pDstSurface1         = NULL;
    GCUSurface pDstSurface2         = NULL;
    GCUSurface pDstLinearSurface    = NULL;

    GCUVirtualAddr      srcVirtAddr = 0;
    GCUPhysicalAddr     srcPhysAddr = 0;
    GCUVirtualAddr      dstVirtAddr = 0;
    GCUPhysicalAddr     dstPhysAddr = 0;

    GCU_INIT_DATA       initData;
    GCU_CONTEXT_DATA    contextData;
    GCU_BLT_DATA        bltData;
    GCU_SURFACE_DATA    surfaceData;

    unsigned int        width       = 512;
    unsigned int        height      = 512;

    GCUuint i = 0, j = 0, k = 0, m = 0;

    GCU_FORMAT aFormat[] = {
        GCU_FORMAT_ARGB8888,
        GCU_FORMAT_ARGB1555,
        GCU_FORMAT_RGB565,
        GCU_FORMAT_ARGB4444,
        GCU_FORMAT_UYVY
    };

    GCUuint iF = sizeof(aFormat)/sizeof(GCU_FORMAT);

    char* sFormat[] = {
        "8888",
        "1555",
        "565",
        "4444",
        "UYVY"
    };

    char filename1[256];
    char filename2[256];
    char filename3[256];
    char filename4[256];

    // Init GCU library
    memset(&initData, 0, sizeof(initData));
    gcuInitialize(&initData);

    // Create Context
    memset(&contextData, 0, sizeof(contextData));
    pContextA = gcuCreateContext(&contextData);
    if(pContextA == NULL)
    {
        result = gcuGetError();
        exit(0);
    }

    GCU_RECT srcRect;
    GCU_RECT dstRect;

    int index = atoi(argv[1]);
    if(index == 0)
    {
        srcRect.left = 0;
        srcRect.top = 0;
        srcRect.right = width;
        srcRect.bottom = height;
        dstRect.left = 0;
        dstRect.top = 0;
        dstRect.right = width;
        dstRect.bottom = height;
        printf("bit blit test\n");
    }
    else
    {
        srcRect.left = 2;
        srcRect.top = 2;
        srcRect.right = width;
        srcRect.bottom = height;
        dstRect.left = 0;
        dstRect.top = 0;
        dstRect.right = width;
        dstRect.bottom = height;
        if(index == 1)
        {
            gcuSet(pContextA, GCU_QUALITY, GCU_QUALITY_NORMAL);
            printf("normal quality test\n");
        }
        else if(index == 2)
        {
            gcuSet(pContextA, GCU_QUALITY, GCU_QUALITY_HIGH);
            printf("high quality test\n");
        }
        else if(index == 3)
        {
            gcuSet(pContextA, GCU_QUALITY, GCU_QUALITY_BEST);
            printf("best quality test\n");
        }
        else
        {
            printf("bad argv\n. 0 for bit blit\n1 for normal quality\n2 for high quality\n3 for best\n");
        }
    }

    pSrcARGBSurface = _gcuLoadRGBSurfaceFromFile(pContextA, "ARGB.bmp");
    for(i = 0; i < iF; i++)
    {
        pSrcSurface = _gcuCreateBuffer(pContextA, width, height, aFormat[i], &srcVirtAddr, &srcPhysAddr);

        //blit linear ARGB8888 => linear aFormat[i]
        memset(&bltData, 0, sizeof(bltData));
        bltData.pSrcSurface = pSrcARGBSurface;
        bltData.pDstSurface = pSrcSurface;
        bltData.pSrcRect = NULL;
        bltData.pDstRect = NULL;
        bltData.rotation = GCU_ROTATION_0;
        gcuBlit(pContextA, &bltData);
        gcuFinish(pContextA);
        strcpy(filename1, "linear");
        strcat(filename1, sFormat[i]);
        _gcuDumpSurface(pContextA, pSrcSurface, filename1);

        for(j = 0; j < iF; j++)
        {
            memset(&surfaceData, 0, sizeof(surfaceData));
            surfaceData.location                     = GCU_SURFACE_LOCATION_VIDEO;
            surfaceData.dimention                    = GCU_SURFACE_2D;
            surfaceData.flag.value                   = 0;
            surfaceData.flag.bits.preAllocVirtual    = 0;
            surfaceData.flag.bits.preAllocPhysical   = 0;
            surfaceData.flag.bits.tileType           = 1;
            surfaceData.format                       = aFormat[j];
            surfaceData.width                        = width;
            surfaceData.height                       = height;
            surfaceData.arraySize                    = 1;
            surfaceData.pPreAllocInfos               = GCU_NULL;
            pDstSurface1 = gcuCreateSurface(pContextA, &surfaceData);

            // blit linear aFormat[i] => tiled aFormat[j]
            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrcSurface;
            bltData.pDstSurface = pDstSurface1;
            bltData.pSrcRect = &srcRect;
            bltData.pDstRect = &dstRect;
            bltData.rotation = GCU_ROTATION_90;
            gcuBlit(pContextA, &bltData);
            gcuFinish(pContextA);
            strcpy(filename2, "tile");
            strcat(filename2, sFormat[j]);
            strcat(filename2, "-");
            strcat(filename2, filename1);
            _gcuDumpSurface(pContextA, pDstSurface1, filename2);

            for(m = 0; m < iF; m++)
            {
                memset(&surfaceData, 0, sizeof(surfaceData));
                surfaceData.location                     = GCU_SURFACE_LOCATION_VIDEO;
                surfaceData.dimention                    = GCU_SURFACE_2D;
                surfaceData.flag.value                   = 0;
                surfaceData.flag.bits.preAllocVirtual    = 0;
                surfaceData.flag.bits.preAllocPhysical   = 0;
                surfaceData.flag.bits.tileType           = 1;
                surfaceData.format                       = aFormat[m];
                surfaceData.width                        = width;
                surfaceData.height                       = height;
                surfaceData.arraySize                    = 1;
                surfaceData.pPreAllocInfos               = GCU_NULL;
                pDstSurface2 = gcuCreateSurface(pContextA, &surfaceData);

                // blit tiled aFormat[j] => tiled aFormat[m]
                memset(&bltData, 0, sizeof(bltData));
                bltData.pSrcSurface = pDstSurface1;
                bltData.pDstSurface = pDstSurface2;
                bltData.pSrcRect = &srcRect;
                bltData.pDstRect = &dstRect;
                bltData.rotation = GCU_ROTATION_90;
                gcuBlit(pContextA, &bltData);
                gcuFinish(pContextA);
                strcpy(filename3, "tile");
                strcat(filename3, sFormat[m]);
                strcat(filename3, "-");
                strcat(filename3, filename2);
                _gcuDumpSurface(pContextA, pDstSurface2, filename3);

                for(k = 0; k < iF; k++)
                {
                    pDstLinearSurface = _gcuCreateBuffer(pContextA, width, height, aFormat[k], &dstVirtAddr, &dstPhysAddr);

                    //blit tiled aFormat[m] => linear aFormat[k]
                    memset(&bltData, 0, sizeof(bltData));
                    bltData.pSrcSurface = pDstSurface2;
                    bltData.pDstSurface = pDstLinearSurface;
                    bltData.pSrcRect = &srcRect;
                    bltData.pDstRect = &dstRect;
                    bltData.rotation = GCU_ROTATION_180;
                    gcuBlit(pContextA, &bltData);
                    gcuFinish(pContextA);
                    strcpy(filename4, "linear");
                    strcat(filename4, sFormat[k]);
                    strcat(filename4, "-");
                    strcat(filename4, filename3);
                    _gcuDumpSurface(pContextA, pDstLinearSurface, filename4);

                    _gcuDestroyBuffer(pContextA, pDstLinearSurface);
                    pDstLinearSurface = NULL;
                }
                _gcuDestroyBuffer(pContextA, pDstSurface2);
                pDstSurface2 = NULL;

            }
            _gcuDestroyBuffer(pContextA, pDstSurface1);
            pDstSurface1 = NULL;

        }
        _gcuDestroyBuffer(pContextA, pSrcSurface);
        pSrcSurface = NULL;

    }

    if(pSrcARGBSurface)
    {
        _gcuDestroyBuffer(pContextA, pSrcARGBSurface);
        pSrcARGBSurface = NULL;
    }
    gcuDestroyContext(pContextA);
    gcuTerminate();

    return 0;
}
