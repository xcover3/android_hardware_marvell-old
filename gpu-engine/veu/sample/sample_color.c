#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WINCE
#include <windows.h>
#endif

#include "veu.h"

#include "M4VSS3GPP_API.h"
#include "M4xVSS_API.h"
#include "M4xVSS_Internal.h"

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
    VEUSurface      pSrcSurface = NULL;
    VEUSurface      pDstSurface = NULL;

    VEU_INIT_DATA   initData;
    M4xVSS_ColorStruct colorData;

    unsigned int    width       = 640;
    unsigned int    height      = 480;

    int i = 0;

    memset(&initData, 0, sizeof(initData));
    veuInitialize(&initData);

    pSrcSurface = _veuLoadSurface("test.yuv", VEU_FORMAT_I420, width, height);
    pDstSurface = veuCreateSurface(width , height, VEU_FORMAT_I420);
    if(pSrcSurface && pDstSurface)
    {
        while(i < 1)
        {
            memset(&colorData, 0, sizeof(colorData));
            colorData.colorEffectType = M4xVSS_kVideoEffectType_BlackAndWhite;
            veuEffectColorProc(&colorData, pSrcSurface, pDstSurface, VEU_NULL, 0);
            _veuSaveSurface(pDstSurface, "color_BlackAndWhite");

            colorData.colorEffectType = M4xVSS_kVideoEffectType_Pink;
            veuEffectColorProc(&colorData, pSrcSurface, pDstSurface, VEU_NULL, 0);
            _veuSaveSurface(pDstSurface, "color_Pink");

            colorData.colorEffectType = M4xVSS_kVideoEffectType_Green;
            veuEffectColorProc(&colorData, pSrcSurface, pDstSurface, VEU_NULL, 0);
            _veuSaveSurface(pDstSurface, "color_Green");

            colorData.colorEffectType = M4xVSS_kVideoEffectType_Sepia;
            veuEffectColorProc(&colorData, pSrcSurface, pDstSurface, VEU_NULL, 0);
            _veuSaveSurface(pDstSurface, "color_Sepia");

            colorData.colorEffectType = M4xVSS_kVideoEffectType_Gradient;
            colorData.rgb16ColorData  = 0xf800;
            veuEffectColorProc(&colorData, pSrcSurface, pDstSurface, VEU_NULL, 0);
            _veuSaveSurface(pDstSurface, "color_Gradient_red");

            colorData.colorEffectType = M4xVSS_kVideoEffectType_Gradient;
            colorData.rgb16ColorData  = 0x07e0;
            veuEffectColorProc(&colorData, pSrcSurface, pDstSurface, VEU_NULL, 0);
            _veuSaveSurface(pDstSurface, "color_Gradient_green");

            colorData.colorEffectType = M4xVSS_kVideoEffectType_Gradient;
            colorData.rgb16ColorData  = 0x001f;
            veuEffectColorProc(&colorData, pSrcSurface, pDstSurface, VEU_NULL, 0);
            _veuSaveSurface(pDstSurface, "color_Gradient_blue");

            colorData.colorEffectType = M4xVSS_kVideoEffectType_ColorRGB16;
            colorData.rgb16ColorData  = 0xf800;
            veuEffectColorProc(&colorData, pSrcSurface, pDstSurface, VEU_NULL, 0);
            _veuSaveSurface(pDstSurface, "color_RGB16_red");

            colorData.colorEffectType = M4xVSS_kVideoEffectType_ColorRGB16;
            colorData.rgb16ColorData  = 0x07e0;
            veuEffectColorProc(&colorData, pSrcSurface, pDstSurface, VEU_NULL, 0);
            _veuSaveSurface(pDstSurface, "color_RGB16_green");

            colorData.colorEffectType = M4xVSS_kVideoEffectType_ColorRGB16;
            colorData.rgb16ColorData  = 0x001f;
            veuEffectColorProc(&colorData, pSrcSurface, pDstSurface, VEU_NULL, 0);
            _veuSaveSurface(pDstSurface, "color_RGB16_blue");

            colorData.colorEffectType = M4xVSS_kVideoEffectType_Negative;
            veuEffectColorProc(&colorData, pSrcSurface, pDstSurface, VEU_NULL, 0);
            _veuSaveSurface(pDstSurface, "color_Negative");

            i++;

        }
    }

    if(pSrcSurface)
    {
        veuDestroySurface(pSrcSurface);
    }

    if(pDstSurface)
    {
        veuDestroySurface(pDstSurface);
    }
    veuTerminate();

    return 0;
}

