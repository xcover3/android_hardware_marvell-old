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

    GCUSurface pDstLinearSurface    = NULL;
    GCUSurface pSrctiTileSurface    = NULL;
    GCUSurface pSrcSurface          = NULL;

    GCUVirtualAddr      srcVirtAddr = 0;
    GCUPhysicalAddr     srcPhysAddr = 0;
    GCUVirtualAddr      dstVirtAddr = 0;
    GCUPhysicalAddr     dstPhysAddr = 0;

    unsigned int        width       = 512;
    unsigned int        height      = 512;

    GCU_INIT_DATA           initData;
    GCU_CONTEXT_DATA        contextData;
    GCU_BLT_DATA            bltData;
    GCU_FILL_DATA           fillData;
    GCU_SURFACE_DATA        surfaceData;
    GCU_SURFACE_LOCK_DATA   lockData;
    GCU_ALLOC_INFO          allocInfos[3];

    GCUuint i = 0, j = 0;

    GCU_FORMAT aFormat[] = {
        GCU_FORMAT_ARGB8888,
        GCU_FORMAT_ARGB1555,
        GCU_FORMAT_RGB565,
        GCU_FORMAT_ARGB4444
    };
    GCUuint iF = sizeof(aFormat)/sizeof(GCU_FORMAT);

    char* sFormat[] = {
        "8888",
        "1555",
        "565",
        "4444"
    };

    char* sTile[] = {
        "tile",
        "supertile",
        "multitile",
        "supermultitile"
    };
    //Fill only support tile now.
    GCUuint iT = 1;

    char filename1[256];
    char filename2[256];

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

    GCU_RECT dstRect;

    dstRect.left = 60;
    dstRect.top = 60;
    dstRect.right = width-60;
    dstRect.bottom = height-60;

    pDstLinearSurface = _gcuCreateBuffer(pContextA, width, height, GCU_FORMAT_ARGB8888, &dstVirtAddr, &dstPhysAddr);
    for(i = 0; i < iT; i++)
    {

        for(j = 0; j < iF; j++)
        {
            pSrcSurface = _gcuCreateBuffer(pContextA, width, height, aFormat[j], &srcVirtAddr, &srcPhysAddr);
            memset(&fillData, 0, sizeof(fillData));
            fillData.pSurface       = pSrcSurface;
            fillData.color          = 0xff00ff00;
            fillData.bSolidColor    = GCU_TRUE;
            fillData.pRect          = NULL;
            gcuFill(pContextA, &fillData);
            gcuFinish(pContextA);
            _gcuDumpSurface(pContextA, pSrcSurface, sFormat[j]);

            memset(&lockData, 0, sizeof(lockData));
            lockData.pSurface = pSrcSurface;
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
            surfaceData.flag.bits.tileType           = 1;
            surfaceData.format                       = aFormat[j];
            surfaceData.width                        = width;
            surfaceData.height                       = height;
            surfaceData.arraySize                    = 1;
            surfaceData.pPreAllocInfos               = allocInfos;
            pSrctiTileSurface = gcuCreateSurface(pContextA, &surfaceData);

            memset(&fillData, 0, sizeof(fillData));
            fillData.pSurface       = pSrctiTileSurface;
            fillData.color          = 0xffff0000;
            fillData.bSolidColor    = GCU_TRUE;
            fillData.pRect          = &dstRect;
            gcuFill(pContextA, &fillData);
            gcuFinish(pContextA);
            strcpy(filename1, sTile[i]);
            strcat(filename1, sFormat[j]);
            _gcuDumpSurface(pContextA, pSrctiTileSurface, filename1);

            // blit tile/super/multi/supermulti aFormat[j]=> linear 8888
            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrctiTileSurface;
            bltData.pDstSurface = pDstLinearSurface;
            bltData.pSrcRect    = NULL;
            bltData.pDstRect    = NULL;
            bltData.rotation    = GCU_ROTATION_0;
            gcuBlit(pContextA, &bltData);
            gcuFinish(pContextA);
            strcpy(filename2, "linear8888-");
            strcat(filename2, filename1);
            _gcuDumpSurface(pContextA, pDstLinearSurface, filename2);

            _gcuDestroyBuffer(pContextA, pSrctiTileSurface);
            pSrctiTileSurface = NULL;

            _gcuDestroyBuffer(pContextA, pSrcSurface);
            pSrcSurface = NULL;

        }
    }
    _gcuDestroyBuffer(pContextA, pDstLinearSurface);
    pDstLinearSurface = NULL;

    gcuDestroyContext(pContextA);
    gcuTerminate();

    return 0;
}
