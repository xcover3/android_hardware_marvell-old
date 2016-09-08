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
    GCUSurface pSrcSurface3 = NULL;
    GCUSurface pDstSurface = NULL;

    GCU_RECT srcRect;
    GCU_RECT dstRect;

    GCUVirtualAddr      srcVirtAddr = 0;
    GCUPhysicalAddr     srcPhysAddr = 0;
    GCUVirtualAddr      dstVirtAddr = 0;
    GCUPhysicalAddr     dstPhysAddr = 0;
    GCU_INIT_DATA initData;
    GCU_CONTEXT_DATA contextData;
    GCU_FILL_DATA fillData;
    GCU_BLT_DATA bltData;
    GCU_BLEND_DATA blendData;

    unsigned int        width;
    unsigned int        height;

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

    pSrcSurface1 = _gcuLoadRGBSurfaceFromFile(pContextA, "shun.bmp");
    pSrcSurface2 = _gcuLoadYUVSurfaceFromFile(pContextA, "I420_176x144.yuv", GCU_FORMAT_I420, 176, 144);
    pSrcSurface3 = _gcuCreateBuffer(pContextA, 256, 256, GCU_FORMAT_ARGB8888, &srcVirtAddr, &srcPhysAddr);
    pDstSurface = _gcuCreateBuffer(pContextA, 1024, 1200, GCU_FORMAT_ARGB8888, &dstVirtAddr, &dstPhysAddr);
    if(pSrcSurface1 && pSrcSurface2 && pSrcSurface3 && pDstSurface)
    {
        GCU_SURFACE_DATA surfaceData;
        gcuQuerySurfaceInfo(pContextA, pSrcSurface1, &surfaceData);
        width = surfaceData.width;
        height = surfaceData.height;
        while(i < 2)
        {
            /* fill with red background */
            memset(&fillData, 0, sizeof(fillData));
            fillData.pSurface = pDstSurface;
            fillData.color = 0xffff0000;
            fillData.bSolidColor = GCU_TRUE;
            dstRect.left = 0;
            dstRect.top = 0;
            dstRect.right = 1024;
            dstRect.bottom = 1200;
            fillData.pRect = &dstRect;
            gcuFill(pContextA, &fillData);


            /* fill with white background */
            memset(&fillData, 0, sizeof(fillData));
            fillData.pSurface = pSrcSurface3;
            fillData.color = 0xff000000;
            fillData.bSolidColor = GCU_TRUE;
            fillData.pRect = NULL;
            gcuFill(pContextA, &fillData);


            /* rgba rotation 0 */
            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrcSurface1;
            bltData.pDstSurface = pDstSurface;
            srcRect.left = 0;
            srcRect.top = 0;
            srcRect.right = width;
            srcRect.bottom = height;
            bltData.pSrcRect = &srcRect;
            dstRect.left = 100;
            dstRect.top = 100;
            dstRect.right = width+100;
            dstRect.bottom = height+100;
            bltData.pDstRect = &dstRect;
            bltData.rotation = GCU_ROTATION_0;
            gcuBlit(pContextA, &bltData);

            /* rgba rotation FLIP_H */
            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrcSurface1;
            bltData.pDstSurface = pSrcSurface3;
            srcRect.left = 0;
            srcRect.top = 0;
            srcRect.right = width;
            srcRect.bottom = height;
            bltData.pSrcRect = &srcRect;
            dstRect.left = 0;
            dstRect.top = 0;
            dstRect.right = width;
            dstRect.bottom = height;
            bltData.pDstRect = &dstRect;
            bltData.rotation = GCU_ROTATION_FLIP_H;
            gcuBlit(pContextA, &bltData);


            /* rgba rotation FLIP_H */
            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrcSurface1;
            bltData.pDstSurface = pDstSurface;
            srcRect.left = 0;
            srcRect.top = 0;
            srcRect.right = width;
            srcRect.bottom = height;
            bltData.pSrcRect = &srcRect;
            dstRect.left = 100;
            dstRect.top = 250;
            dstRect.right = width+100;
            dstRect.bottom = height+250;
            bltData.pDstRect = &dstRect;
            bltData.rotation = GCU_ROTATION_FLIP_H;
            gcuBlit(pContextA, &bltData);

            /* rgba rotation 90 */
            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrcSurface3;
            bltData.pDstSurface = pDstSurface;
            srcRect.left = 0;
            srcRect.top = 0;
            srcRect.right = width;
            srcRect.bottom = height;
            bltData.pSrcRect = &srcRect;
            dstRect.left = 100;
            dstRect.top = 400;
            dstRect.right = height+100;
            dstRect.bottom = width+400;
            bltData.pDstRect = &dstRect;
            bltData.rotation = GCU_ROTATION_90;
            gcuBlit(pContextA, &bltData);

            /* fill with white background */
            memset(&fillData, 0, sizeof(fillData));
            fillData.pSurface = pSrcSurface3;
            fillData.color = 0xff000000;
            fillData.bSolidColor = GCU_TRUE;
            fillData.pRect = NULL;
            gcuFill(pContextA, &fillData);

            /* rgba rotation FLIP_H rect*/
            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrcSurface1;
            bltData.pDstSurface = pSrcSurface3;
            srcRect.left = 50;
            srcRect.top = 50;
            srcRect.right = width/2+50;
            srcRect.bottom = height/2+50;
            bltData.pSrcRect = &srcRect;
            dstRect.left = 0;
            dstRect.top = 0;
            dstRect.right = width/2;
            dstRect.bottom = height/2;
            bltData.pDstRect = &dstRect;
            bltData.rotation = GCU_ROTATION_FLIP_H;
            gcuBlit(pContextA, &bltData);

            /* rgba rotation FLIP_H rect*/
            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrcSurface1;
            bltData.pDstSurface = pDstSurface;
            srcRect.left = 50;
            srcRect.top = 50;
            srcRect.right = width/2+50;
            srcRect.bottom = height/2+50;
            bltData.pSrcRect = &srcRect;
            dstRect.left = 250;
            dstRect.top = 250;
            dstRect.right = width/2+250;
            dstRect.bottom = height/2+250;
            bltData.pDstRect = &dstRect;
            bltData.rotation = GCU_ROTATION_FLIP_H;
            gcuBlit(pContextA, &bltData);

            /* rgba rotation 90 rect with scale and blend*/
            memset(&blendData, 0, sizeof(blendData));
            blendData.pSrcSurface = pSrcSurface3;
            blendData.pDstSurface = pDstSurface;
            srcRect.left = 0;
            srcRect.top = 0;
            srcRect.right = width/2;
            srcRect.bottom = height/2;
            blendData.pSrcRect = &srcRect;
            dstRect.left = 250;
            dstRect.top = 400;
            dstRect.right = height+250;
            dstRect.bottom = width+400;
            blendData.pDstRect = &dstRect;
            blendData.rotation = GCU_ROTATION_90;
            blendData.blendMode = GCU_BLEND_SRC_OVER;
            blendData.srcGlobalAlpha = 0x88;
            blendData.dstGlobalAlpha = 0xFF;
            gcuBlend(pContextA, &blendData);

            /* fill with white background */
            memset(&fillData, 0, sizeof(fillData));
            fillData.pSurface = pSrcSurface3;
            fillData.color = 0xff000000;
            fillData.bSolidColor = GCU_TRUE;
            fillData.pRect = NULL;
            gcuFill(pContextA, &fillData);

            /* rgba rotation 0 */
            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrcSurface1;
            bltData.pDstSurface = pDstSurface;
            srcRect.left = 0;
            srcRect.top = 0;
            srcRect.right = width;
            srcRect.bottom = height;
            bltData.pSrcRect = &srcRect;
            dstRect.left = 600;
            dstRect.top = 100;
            dstRect.right = width+600;
            dstRect.bottom = height+100;
            bltData.pDstRect = &dstRect;
            bltData.rotation = GCU_ROTATION_0;
            gcuBlit(pContextA, &bltData);

            /* rgba rotation FLIP_V */
            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrcSurface1;
            bltData.pDstSurface = pSrcSurface3;
            srcRect.left = 0;
            srcRect.top = 0;
            srcRect.right = width;
            srcRect.bottom = height;
            bltData.pSrcRect = &srcRect;
            dstRect.left = 0;
            dstRect.top = 0;
            dstRect.right = width;
            dstRect.bottom = height;
            bltData.pDstRect = &dstRect;
            bltData.rotation = GCU_ROTATION_FLIP_V;
            gcuBlit(pContextA, &bltData);

            /* rgba rotation FLIP_V */
            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrcSurface1;
            bltData.pDstSurface = pDstSurface;
            srcRect.left = 0;
            srcRect.top = 0;
            srcRect.right = width;
            srcRect.bottom = height;
            bltData.pSrcRect = &srcRect;
            dstRect.left = 600;
            dstRect.top = 250;
            dstRect.right = width+600;
            dstRect.bottom = height+250;
            bltData.pDstRect = &dstRect;
            bltData.rotation = GCU_ROTATION_FLIP_V;
            gcuBlit(pContextA, &bltData);

            /* rgba rotation 90 */
            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrcSurface3;
            bltData.pDstSurface = pDstSurface;
            srcRect.left = 0;
            srcRect.top = 0;
            srcRect.right = width;
            srcRect.bottom = height;
            bltData.pSrcRect = &srcRect;
            dstRect.left = 600;
            dstRect.top = 400;
            dstRect.right = height+600;
            dstRect.bottom = width+400;
            bltData.pDstRect = &dstRect;
            bltData.rotation = GCU_ROTATION_90;
            gcuBlit(pContextA, &bltData);


            /* fill with white background */
            memset(&fillData, 0, sizeof(fillData));
            fillData.pSurface = pSrcSurface3;
            fillData.color = 0xff000000;
            fillData.bSolidColor = GCU_TRUE;
            fillData.pRect = NULL;
            gcuFill(pContextA, &fillData);

            /* rgba rotation FLIP_H rect*/
            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrcSurface1;
            bltData.pDstSurface = pSrcSurface3;
            srcRect.left = 50;
            srcRect.top = 50;
            srcRect.right = width/2+50;
            srcRect.bottom = height/2+50;
            bltData.pSrcRect = &srcRect;
            dstRect.left = 0;
            dstRect.top = 0;
            dstRect.right = width/2;
            dstRect.bottom = height/2;
            bltData.pDstRect = &dstRect;
            bltData.rotation = GCU_ROTATION_FLIP_V;
            gcuBlit(pContextA, &bltData);

            /* rgba rotation FLIP_V rect */
            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrcSurface1;
            bltData.pDstSurface = pDstSurface;
            srcRect.left = 50;
            srcRect.top = 50;
            srcRect.right = width/2+50;
            srcRect.bottom = height/2+50;
            bltData.pSrcRect = &srcRect;
            dstRect.left = 750;
            dstRect.top = 250;
            dstRect.right = width/2+750;
            dstRect.bottom = height/2+250;
            bltData.pDstRect = &dstRect;
            bltData.rotation = GCU_ROTATION_FLIP_V;
            gcuBlit(pContextA, &bltData);

            /* rgba rotation 90 rect with scale and blend*/
            memset(&blendData, 0, sizeof(blendData));
            blendData.pSrcSurface = pSrcSurface3;
            blendData.pDstSurface = pDstSurface;
            srcRect.left = 0;
            srcRect.top = 0;
            srcRect.right = width/2;
            srcRect.bottom = height/2;
            blendData.pSrcRect = &srcRect;
            dstRect.left = 750;
            dstRect.top = 400;
            dstRect.right = height+750;
            dstRect.bottom = width+400;
            blendData.pDstRect = &dstRect;
            blendData.rotation = GCU_ROTATION_90;
            blendData.blendMode = GCU_BLEND_SRC_OVER;
            blendData.srcGlobalAlpha = 0x88;
            blendData.dstGlobalAlpha = 0xFF;
            gcuBlend(pContextA, &blendData);

            /* rgba GCU_ROTATION_FLIP_H|GCU_ROTATION_90 */
            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrcSurface1;
            bltData.pDstSurface = pDstSurface;
            srcRect.left = 0;
            srcRect.top = 0;
            srcRect.right = width;
            srcRect.bottom = height;
            bltData.pSrcRect = &srcRect;
            dstRect.left = 100;
            dstRect.top = 600;
            dstRect.right = height+100;
            dstRect.bottom = width+600;
            bltData.pDstRect = &dstRect;
            bltData.rotation = GCU_ROTATION_FLIP_H|GCU_ROTATION_90;
            gcuBlit(pContextA, &bltData);

            /* rgba GCU_ROTATION_FLIP_H|GCU_ROTATION_90 rect with scale and blend*/
            memset(&blendData, 0, sizeof(blendData));
            blendData.pSrcSurface = pSrcSurface1;
            blendData.pDstSurface = pDstSurface;
            srcRect.left = 50;
            srcRect.top = 50;
            srcRect.right = width/2+50;
            srcRect.bottom = height/2+50;
            blendData.pSrcRect = &srcRect;
            dstRect.left = 250;
            dstRect.top = 600;
            dstRect.right = height+250;
            dstRect.bottom = width+600;
            blendData.pDstRect = &dstRect;
            blendData.rotation =  GCU_ROTATION_FLIP_H|GCU_ROTATION_90;
            blendData.blendMode = GCU_BLEND_SRC_OVER;
            blendData.srcGlobalAlpha = 0x88;
            blendData.dstGlobalAlpha = 0xFF;
            gcuBlend(pContextA, &blendData);


            /* rgba GCU_ROTATION_FLIP_V|GCU_ROTATION_90 */
            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrcSurface1;
            bltData.pDstSurface = pDstSurface;
            srcRect.left = 0;
            srcRect.top = 0;
            srcRect.right = width;
            srcRect.bottom = height;
            bltData.pSrcRect = &srcRect;
            dstRect.left = 600;
            dstRect.top = 600;
            dstRect.right = height+600;
            dstRect.bottom = width+600;
            bltData.pDstRect = &dstRect;
            bltData.rotation = GCU_ROTATION_FLIP_V|GCU_ROTATION_90;
            gcuBlit(pContextA, &bltData);

            /* rgba GCU_ROTATION_FLIP_V|GCU_ROTATION_90 rect with scale and blend*/
            memset(&blendData, 0, sizeof(blendData));
            blendData.pSrcSurface = pSrcSurface1;
            blendData.pDstSurface = pDstSurface;
            srcRect.left = 50;
            srcRect.top = 50;
            srcRect.right = width/2+50;
            srcRect.bottom = height/2+50;
            blendData.pSrcRect = &srcRect;
            dstRect.left = 750;
            dstRect.top = 600;
            dstRect.right = height+750;
            dstRect.bottom = width+600;
            blendData.pDstRect = &dstRect;
            blendData.rotation =  GCU_ROTATION_FLIP_V|GCU_ROTATION_90;
            blendData.blendMode = GCU_BLEND_SRC_OVER;
            blendData.srcGlobalAlpha = 0x88;
            blendData.dstGlobalAlpha = 0xFF;
            gcuBlend(pContextA, &blendData);


            /* fill with white background */
            memset(&fillData, 0, sizeof(fillData));
            fillData.pSurface = pSrcSurface3;
            fillData.color = 0xff000000;
            fillData.bSolidColor = GCU_TRUE;
            fillData.pRect = NULL;
            gcuFill(pContextA, &fillData);

            /* yuv rotation FLIP_H*/
            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrcSurface2;
            bltData.pDstSurface = pSrcSurface3;
            srcRect.left = 0;
            srcRect.top = 0;
            srcRect.right = 176;
            srcRect.bottom = 144;
            bltData.pSrcRect = &srcRect;
            dstRect.left = 0;
            dstRect.top = 0;
            dstRect.right = 176;
            dstRect.bottom = 144;
            bltData.pDstRect = &dstRect;
            bltData.rotation = GCU_ROTATION_FLIP_H;
            gcuBlit(pContextA, &bltData);

            /* yuv rotation 90*/
            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrcSurface3;
            bltData.pDstSurface = pDstSurface;
            srcRect.left = 0;
            srcRect.top = 0;
            srcRect.right = 176;
            srcRect.bottom = 144;
            bltData.pSrcRect = &srcRect;
            dstRect.left = 100;
            dstRect.top = 800;
            dstRect.right = 100+144;
            dstRect.bottom = 800+176;
            bltData.pDstRect = &dstRect;
            bltData.rotation = GCU_ROTATION_90;
            gcuBlit(pContextA, &bltData);

            /* fill with white background */
            memset(&fillData, 0, sizeof(fillData));
            fillData.pSurface = pSrcSurface3;
            fillData.color = 0xff000000;
            fillData.bSolidColor = GCU_TRUE;
            fillData.pRect = NULL;
            gcuFill(pContextA, &fillData);

            /* yuv rotation FLIP_H rect*/
            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrcSurface2;
            bltData.pDstSurface = pSrcSurface3;
            srcRect.left = 50;
            srcRect.top = 50;
            srcRect.right = 176/2+50;
            srcRect.bottom = 144/2+50;
            bltData.pSrcRect = &srcRect;
            dstRect.left = 0;
            dstRect.top = 0;
            dstRect.right = 176/2;
            dstRect.bottom = 144/2;
            bltData.pDstRect = &dstRect;
            bltData.rotation = GCU_ROTATION_FLIP_H;
            gcuBlit(pContextA, &bltData);

            /* yuv rotation 90 rect with scale*/
            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrcSurface3;
            bltData.pDstSurface = pDstSurface;
            srcRect.left = 0;
            srcRect.top = 0;
            srcRect.right = 176/2;
            srcRect.bottom = 144/2;
            bltData.pSrcRect = &srcRect;
            dstRect.left = 250;
            dstRect.top = 800;
            dstRect.right = 250+144;
            dstRect.bottom = 800+176;
            bltData.pDstRect = &dstRect;
            bltData.rotation = GCU_ROTATION_90;
            gcuBlit(pContextA, &bltData);

            /* fill with white background */
            memset(&fillData, 0, sizeof(fillData));
            fillData.pSurface = pSrcSurface3;
            fillData.color = 0xff000000;
            fillData.bSolidColor = GCU_TRUE;
            fillData.pRect = NULL;
            gcuFill(pContextA, &fillData);

            /* yuv rotation FLIP_V*/
            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrcSurface2;
            bltData.pDstSurface = pSrcSurface3;
            srcRect.left = 0;
            srcRect.top = 0;
            srcRect.right = 176;
            srcRect.bottom = 144;
            bltData.pSrcRect = &srcRect;
            dstRect.left = 0;
            dstRect.top = 0;
            dstRect.right = 176;
            dstRect.bottom = 144;
            bltData.pDstRect = &dstRect;
            bltData.rotation = GCU_ROTATION_FLIP_V;
            gcuBlit(pContextA, &bltData);

            /* yuv rotation 90*/
            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrcSurface3;
            bltData.pDstSurface = pDstSurface;
            srcRect.left = 0;
            srcRect.top = 0;
            srcRect.right = 176;
            srcRect.bottom = 144;
            bltData.pSrcRect = &srcRect;
            dstRect.left = 600;
            dstRect.top = 800;
            dstRect.right = 600+144;
            dstRect.bottom = 800+176;
            bltData.pDstRect = &dstRect;
            bltData.rotation = GCU_ROTATION_90;
            gcuBlit(pContextA, &bltData);

            /* fill with white background */
            memset(&fillData, 0, sizeof(fillData));
            fillData.pSurface = pSrcSurface3;
            fillData.color = 0xff000000;
            fillData.bSolidColor = GCU_TRUE;
            fillData.pRect = NULL;
            gcuFill(pContextA, &fillData);

            /* yuv rotation FLIP_V rect*/
            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrcSurface2;
            bltData.pDstSurface = pSrcSurface3;
            srcRect.left = 50;
            srcRect.top = 50;
            srcRect.right = 176/2+50;
            srcRect.bottom = 144/2+50;
            bltData.pSrcRect = &srcRect;
            dstRect.left = 0;
            dstRect.top = 0;
            dstRect.right = 176/2;
            dstRect.bottom = 144/2;
            bltData.pDstRect = &dstRect;
            bltData.rotation = GCU_ROTATION_FLIP_V;
            gcuBlit(pContextA, &bltData);

            /* yuv rotation 90 rect with scale*/
            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrcSurface3;
            bltData.pDstSurface = pDstSurface;
            srcRect.left = 0;
            srcRect.top = 0;
            srcRect.right = 176/2;
            srcRect.bottom = 144/2;
            bltData.pSrcRect = &srcRect;
            dstRect.left = 750;
            dstRect.top = 800;
            dstRect.right = 750+144;
            dstRect.bottom = 800+176;
            bltData.pDstRect = &dstRect;
            bltData.rotation = GCU_ROTATION_90;
            gcuBlit(pContextA, &bltData);

            /* yuv GCU_ROTATION_FLIP_H|GCU_ROTATION_90 */
            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrcSurface2;
            bltData.pDstSurface = pDstSurface;
            srcRect.left = 0;
            srcRect.top = 0;
            srcRect.right = 176;
            srcRect.bottom = 144;
            bltData.pSrcRect = &srcRect;
            dstRect.left = 100;
            dstRect.top = 1000;
            dstRect.right = 100+144;
            dstRect.bottom = 1000+176;
            bltData.pDstRect = &dstRect;
            bltData.rotation = GCU_ROTATION_FLIP_H|GCU_ROTATION_90;
            gcuBlit(pContextA, &bltData);


            /* yuv GCU_ROTATION_FLIP_H|GCU_ROTATION_90 rect with scale */
            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrcSurface2;
            bltData.pDstSurface = pDstSurface;
            srcRect.left = 50;
            srcRect.top = 50;
            srcRect.right = 176/2+50;
            srcRect.bottom = 144/2+50;
            bltData.pSrcRect = &srcRect;
            dstRect.left = 250;
            dstRect.top = 1000;
            dstRect.right = 144+250;
            dstRect.bottom = 176+1000;
            bltData.pDstRect = &dstRect;
            bltData.rotation = GCU_ROTATION_FLIP_H|GCU_ROTATION_90;
            gcuBlit(pContextA, &bltData);


            /* yuv GCU_ROTATION_FLIP_V|GCU_ROTATION_90 */
            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrcSurface2;
            bltData.pDstSurface = pDstSurface;
            srcRect.left = 0;
            srcRect.top = 0;
            srcRect.right = 176;
            srcRect.bottom = 144;
            bltData.pSrcRect = &srcRect;
            dstRect.left = 600;
            dstRect.top = 1000;
            dstRect.right = 144+600;
            dstRect.bottom = 176+1000;
            bltData.pDstRect = &dstRect;
            bltData.rotation = GCU_ROTATION_FLIP_V|GCU_ROTATION_90;
            gcuBlit(pContextA, &bltData);

            /* yuv GCU_ROTATION_FLIP_V|GCU_ROTATION_90 rect with scale */
            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrcSurface2;
            bltData.pDstSurface = pDstSurface;
            srcRect.left = 50;
            srcRect.top = 50;
            srcRect.right = 176/2+50;
            srcRect.bottom = 144/2+50;
            bltData.pSrcRect = &srcRect;
            dstRect.left = 750;
            dstRect.top = 1000;
            dstRect.right = 144+750;
            dstRect.bottom = 176+1000;
            bltData.pDstRect = &dstRect;
            bltData.rotation = GCU_ROTATION_FLIP_V|GCU_ROTATION_90;
            gcuBlit(pContextA, &bltData);

            gcuFinish(pContextA);
            _gcuDumpSurface(pContextA, pDstSurface, "flip_rotate");
            gcuSet(pContextA, GCU_QUALITY, GCU_QUALITY_HIGH);
            i++;
        }
    }

    if(pSrcSurface1)
    {
        _gcuDestroyBuffer(pContextA, pSrcSurface1);
        pSrcSurface1 = NULL;
    }

    if(pSrcSurface2)
    {
        _gcuDestroyBuffer(pContextA, pSrcSurface2);
        pSrcSurface2 = NULL;
    }

    if(pDstSurface)
    {
        _gcuDestroyBuffer(pContextA, pDstSurface);
        pDstSurface = NULL;
    }
    if(pSrcSurface3)
    {
        _gcuDestroyBuffer(pContextA, pSrcSurface3);
        pDstSurface = NULL;
    }
    gcuDestroyContext(pContextA);
    gcuTerminate();

    return 0;
}

