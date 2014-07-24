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

    GCUSurface pSrcSurface1 = NULL;
    GCUSurface pSrcSurface2 = NULL;
    GCUSurface pDstSurface = NULL;

    GCU_RECT srcRect;
    GCU_RECT fillRect;
    GCU_RECT dstRect;
    GCU_RECT clipRect;


    GCUVirtualAddr      dstVirtAddr = 0;
    GCUPhysicalAddr     dstPhysAddr = 0;

    GCU_INIT_DATA initData;
    GCU_CONTEXT_DATA contextData;
    GCU_FILL_DATA fillData;
    GCU_BLT_DATA blitData;
    GCU_BLEND_DATA blendData;

    unsigned int        width       = 800;
    unsigned int        height      = 480;
    unsigned int        picwidth       = 800;
    unsigned int        picheight      = 480;

    int i = 0;

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

    pSrcSurface2 = _gcuLoadYUVSurfaceFromFile(pContextA, "YV12_176x144.yuv", GCU_FORMAT_YV12, 176, 144);
    pSrcSurface1 = _gcuLoadRGBSurfaceFromFile(pContextA, "dophin.bmp");

    pDstSurface = _gcuCreateBuffer(pContextA, width , height, GCU_FORMAT_RGB565, &dstVirtAddr, &dstPhysAddr);
    memset(&blendData, 0, sizeof(blendData));
    memset(&blitData, 0, sizeof(blitData));
    /* fill dst surface with green */
    memset(&fillData, 0, sizeof(fillData));

    fillRect.left = 0;
    fillRect.top = 0;
    fillRect.right = width;
    fillRect.bottom = height;

    srcRect.left = 0;
    srcRect.top = 0;

    dstRect.left = 0-500;
    dstRect.top = 0-300;
    dstRect.right = 800+500;
    dstRect.bottom = 480+300;

    clipRect.left = 113;
    clipRect.right = 497;
    clipRect.top = 79;
    clipRect.bottom = 311;

    fillData.pSurface = pDstSurface;
    fillData.color = 0xff00ff00;
    fillData.bSolidColor = GCU_TRUE;
    fillData.pRect = &fillRect;

    blendData.pDstSurface = pDstSurface;
    blendData.pSrcRect = &srcRect;
    blendData.pDstRect = &dstRect;
    blendData.rotation = GCU_ROTATION_90;
    blendData.pClipRect = &clipRect;
    blendData.blendMode = GCU_BLEND_SRC_OVER;
    blendData.srcGlobalAlpha = 0xa0;
    blendData.dstGlobalAlpha = 0xFF;

    blitData.pDstSurface = pDstSurface;
    blitData.pSrcRect = &srcRect;
    blitData.pDstRect = &dstRect;
    blitData.rotation = GCU_ROTATION_90;
    blitData.pClipRect = &clipRect;

    if(pSrcSurface1 && pSrcSurface2 && pDstSurface)
    {
        GCU_SURFACE_DATA surfaceData;

        while(i < 8)
        {
            gcuSet (pContextA, GCU_CLIP_RECT, GCU_TRUE);
            if(i & 1)
            {
                gcuSet(pContextA, GCU_QUALITY, GCU_QUALITY_HIGH);
            }
            else
            {
                gcuSet(pContextA, GCU_QUALITY, GCU_QUALITY_NORMAL);
            }
            if(i & 2)
            {
                gcuSet(pContextA, GCU_DITHER, GCU_TRUE);
            }
            else
            {
                gcuSet(pContextA, GCU_DITHER, GCU_FALSE);
            }
            if(i & 4)
            {
                blitData.pSrcSurface = pSrcSurface1;
                blendData.pSrcSurface = pSrcSurface1;//note that blend do not support yuv
            }
            else
            {
                blitData.pSrcSurface = pSrcSurface2;
                blendData.pSrcSurface = pSrcSurface2;
            }
            gcuQuerySurfaceInfo(pContextA, blitData.pSrcSurface, &surfaceData);
            picwidth = surfaceData.width;
            picheight = surfaceData.height;

            srcRect.right = picwidth;
            srcRect.bottom = picheight;



            gcuFill(pContextA, &fillData);

            clipRect.left = 0;
            clipRect.right = 800;
            clipRect.top = 0;
            clipRect.bottom = 480;

            gcuBlit(pContextA, &blitData);


            gcuFinish(pContextA);
            _gcuDumpSurface(pContextA, pDstSurface, "cliprect");


            gcuFill(pContextA, &fillData);


            clipRect.left = 113;
            clipRect.right = 697;
            clipRect.top = 79;
            clipRect.bottom = 401;

            gcuBlit(pContextA, &blitData);


            gcuFinish(pContextA);
            _gcuDumpSurface(pContextA, pDstSurface, "cliprect");


            gcuFill(pContextA, &fillData);

            clipRect.left = 113;
            clipRect.right = 597;
            clipRect.top = 79;
            clipRect.bottom = 351;

            gcuBlit(pContextA, &blitData);


            gcuFinish(pContextA);
            _gcuDumpSurface(pContextA, pDstSurface, "cliprect");


            gcuFill(pContextA, &fillData);

            clipRect.left = 113;
            clipRect.right = 497;
            clipRect.top = 79;
            clipRect.bottom = 311;

            gcuBlit(pContextA, &blitData);


            gcuFinish(pContextA);
            _gcuDumpSurface(pContextA, pDstSurface, "cliprect");


            gcuFill(pContextA, &fillData);

            clipRect.left = 0;
            clipRect.right = 800;
            clipRect.top = 0;
            clipRect.bottom = 480;

            gcuBlend(pContextA, &blendData);


            gcuFinish(pContextA);
            _gcuDumpSurface(pContextA, pDstSurface, "cliprect");


            gcuFill(pContextA, &fillData);


            clipRect.left = 113;
            clipRect.right = 697;
            clipRect.top = 79;
            clipRect.bottom = 401;

            gcuBlend(pContextA, &blendData);


            gcuFinish(pContextA);
            _gcuDumpSurface(pContextA, pDstSurface, "cliprect");


            gcuFill(pContextA, &fillData);

            clipRect.left = 113;
            clipRect.right = 597;
            clipRect.top = 79;
            clipRect.bottom = 351;

            gcuBlend(pContextA, &blendData);


            gcuFinish(pContextA);
            _gcuDumpSurface(pContextA, pDstSurface, "cliprect");


            gcuFill(pContextA, &fillData);

            clipRect.left = 113;
            clipRect.right = 497;
            clipRect.top = 79;
            clipRect.bottom = 311;

            gcuBlend(pContextA, &blendData);


            gcuFinish(pContextA);
            _gcuDumpSurface(pContextA, pDstSurface, "cliprect");


            i++;
        }
    }

    if(pSrcSurface1)
    {
        _gcuDestroyBuffer(pContextA, pSrcSurface1);
    }
    if(pSrcSurface2)
    {
        _gcuDestroyBuffer(pContextA, pSrcSurface2);
    }

    if(pDstSurface)
    {
        _gcuDestroyBuffer(pContextA, pDstSurface);
    }
    gcuDestroyContext(pContextA);
    gcuTerminate();

    return 0;
}
