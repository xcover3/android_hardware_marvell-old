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
    GCUSurface pDstLinearSurface    = NULL;
    GCUSurface ptile4444            = NULL;
    GCUSurface plinear4444          = NULL;
    GCUSurface pSrctiTileSurface    = NULL;

    GCUVirtualAddr      srcVirtAddr = 0;
    GCUPhysicalAddr     srcPhysAddr = 0;
    GCUVirtualAddr      dstVirtAddr = 0;
    GCUPhysicalAddr     dstPhysAddr = 0;

    GCU_INIT_DATA           initData;
    GCU_CONTEXT_DATA        contextData;
    GCU_BLT_DATA            bltData;
    GCU_BLEND_DATA          blendData;
    GCU_SURFACE_DATA        surfaceData;
    GCU_SURFACE_LOCK_DATA   lockData;
    GCU_ALLOC_INFO          allocInfos[3];

    unsigned int        width       = 512;
    unsigned int        height      = 512;

    GCUuint i = 0;


    GCUuint iF = 4;

    char* sFormat[] = {
        "tile",
        "supertile",
        "multitile",
        "multisupertile"
    };

    char* sfile[] = {
        "ARGB8888-tiled.bmp",
        "ARGB8888-supertiled-2.bmp",
        "ARGB8888-multi-tiled.bmp",
        "ARGB8888-multi-supertiled-2.bmp",
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

    srcRect.left = 2;
    srcRect.top = 2;
    srcRect.right = width;
    srcRect.bottom = height;
    dstRect.left = 0;
    dstRect.top = 0;
    dstRect.right = width;
    dstRect.bottom = height;

    gcuSet(pContextA, GCU_DITHER, GCU_TRUE);
    printf("dither blend test\n");

    for(i = 0; i < 4; i++)
    {
        pSrcSurface = _gcuLoadRGBSurfaceFromFile(pContextA, "ARGB.bmp");

        memset(&surfaceData, 0, sizeof(surfaceData));
        surfaceData.location                     = GCU_SURFACE_LOCATION_VIDEO;
        surfaceData.dimention                    = GCU_SURFACE_2D;
        surfaceData.flag.value                   = 0;
        surfaceData.flag.bits.preAllocVirtual    = 0;
        surfaceData.flag.bits.preAllocPhysical   = 0;
        surfaceData.flag.bits.tileType           = 1;
        surfaceData.format                       = GCU_FORMAT_ARGB4444;
        surfaceData.width                        = width;
        surfaceData.height                       = height;
        surfaceData.arraySize                    = 1;
        surfaceData.pPreAllocInfos               = GCU_NULL;
        ptile4444 = gcuCreateSurface(pContextA, &surfaceData);

        plinear4444 = _gcuCreateBuffer(pContextA, width, height, GCU_FORMAT_ARGB4444, &dstVirtAddr, &dstPhysAddr);
        pDstLinearSurface = _gcuCreateBuffer(pContextA, width, height, GCU_FORMAT_ARGB8888, &dstVirtAddr, &dstPhysAddr);

        // blit linear 8888 => tiled 4444
        memset(&bltData, 0, sizeof(bltData));
        bltData.pSrcSurface = pSrcSurface;
        bltData.pDstSurface = ptile4444;
        bltData.pSrcRect = NULL;
        bltData.pDstRect = NULL;
        bltData.rotation = GCU_ROTATION_0;
        gcuBlit(pContextA, &bltData);
        gcuFinish(pContextA);

        // blit linear 8888 => linear 4444
        memset(&bltData, 0, sizeof(bltData));
        bltData.pSrcSurface = pSrcSurface;
        bltData.pDstSurface = plinear4444;
        bltData.pSrcRect = NULL;
        bltData.pDstRect = NULL;
        bltData.rotation = GCU_ROTATION_0;
        gcuBlit(pContextA, &bltData);
        gcuFinish(pContextA);

        pSrcARGBSurface = _gcuLoadRGBSurfaceFromFile(pContextA, sfile[i]);
        //creat tile/super/multi/supermulti 8888 surface
        memset(&lockData, 0, sizeof(lockData));
        lockData.pSurface = pSrcARGBSurface;
        lockData.flag.bits.preserve = 1;
        lockData.arraySize = 3;
        lockData.pAllocInfos = allocInfos;
        gcuLockSurface(pContextA, &lockData);
        memset(&surfaceData, 0, sizeof(surfaceData));
        surfaceData.location                     = GCU_SURFACE_LOCATION_VIDEO;
        surfaceData.dimention                    = GCU_SURFACE_2D;
        surfaceData.flag.value                   = 0;
        surfaceData.flag.bits.preAllocVirtual    = 1;
        surfaceData.flag.bits.preAllocPhysical   = 1;
        surfaceData.flag.bits.tileType           = i+1;
        surfaceData.format                       = GCU_FORMAT_ARGB8888;
        surfaceData.width                        = width;
        surfaceData.height                       = height;
        surfaceData.arraySize                    = 1;
        surfaceData.pPreAllocInfos               = allocInfos;
        pSrctiTileSurface = gcuCreateSurface(pContextA, &surfaceData);
        _gcuDumpSurface(pContextA, pSrctiTileSurface, sFormat[i]);

        // blend tile/super/multi/supermulti 8888 => tiled GCU_FORMAT_ARGB4444
        memset(&blendData, 0, sizeof(blendData));
        blendData.pSrcSurface = pSrctiTileSurface;
        blendData.pDstSurface = ptile4444;
        blendData.pSrcRect = &srcRect;
        blendData.pDstRect = &dstRect;
        blendData.rotation = GCU_ROTATION_90;
        blendData.blendMode = GCU_BLEND_SRC_OVER;
        blendData.srcGlobalAlpha = 122;
        blendData.dstGlobalAlpha = 255;
        gcuBlend(pContextA, &blendData);
        gcuFinish(pContextA);

        strcpy(filename1, "tile4444-");
        strcat(filename1, sFormat[i]);
        strcat(filename1, "8888");
        _gcuDumpSurface(pContextA, ptile4444, filename1);

        //blit tiled GCU_FORMAT_ARGB4444 => linear GCU_FORMAT_ARGB8888
        memset(&bltData, 0, sizeof(bltData));
        bltData.pSrcSurface = ptile4444;
        bltData.pDstSurface = pDstLinearSurface;
        bltData.pSrcRect = NULL;
        bltData.pDstRect = NULL;
        bltData.rotation = GCU_ROTATION_0;
        gcuBlit(pContextA, &bltData);
        gcuFinish(pContextA);

        strcpy(filename2, "linear8888-");
        strcat(filename2, filename1);
        _gcuDumpSurface(pContextA, pDstLinearSurface, filename2);


        // blend tile/super/multi/supermulti 8888 => linear GCU_FORMAT_ARGB4444
        memset(&blendData, 0, sizeof(blendData));
        blendData.pSrcSurface = pSrctiTileSurface;
        blendData.pDstSurface = plinear4444;
        blendData.pSrcRect = &srcRect;
        blendData.pDstRect = &dstRect;
        blendData.rotation = GCU_ROTATION_90;
        blendData.blendMode = GCU_BLEND_SRC_OVER;
        blendData.srcGlobalAlpha = 122;
        blendData.dstGlobalAlpha = 255;
        gcuBlend(pContextA, &blendData);
        gcuFinish(pContextA);
        strcpy(filename3, "linear4444-");
        strcat(filename3, sFormat[i]);
        strcat(filename3, "8888");
        _gcuDumpSurface(pContextA, plinear4444, filename3);

        _gcuDestroyBuffer(pContextA, plinear4444);
        plinear4444 = NULL;

        _gcuDestroyBuffer(pContextA, pDstLinearSurface);
        pDstLinearSurface = NULL;

        _gcuDestroyBuffer(pContextA, ptile4444);
        ptile4444 = NULL;

        _gcuDestroyBuffer(pContextA, pSrctiTileSurface);
        pSrctiTileSurface = NULL;

        _gcuDestroyBuffer(pContextA, pSrcARGBSurface);
        pSrcARGBSurface = NULL;

        _gcuDestroyBuffer(pContextA, pSrcSurface);
        pSrcSurface = NULL;
    }
    gcuDestroyContext(pContextA);
    gcuTerminate();

    return 0;
}
