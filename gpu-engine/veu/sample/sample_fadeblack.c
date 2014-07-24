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
    VEUSurface pSrcSurface = NULL;
    VEUSurface pDstSurface = NULL;

    VEU_INIT_DATA        initData;
    M4VSS3GPP_ExternalProgress process;

    unsigned int   width  = 640;
    unsigned int   height = 480;

    int i = 0;

    memset(&initData, 0, sizeof(initData));
    veuInitialize(&initData);

    pSrcSurface = _veuLoadSurface("test.yuv", VEU_FORMAT_I420, width, height);
    pDstSurface = veuCreateSurface(width , height, VEU_FORMAT_I420);

    if(pSrcSurface && pDstSurface)
    {
        while(i < 1)
        {
            process.uiProgress         = 0;
            veuFadeBlackProc(VEU_NULL, pSrcSurface, pSrcSurface, pDstSurface, &process, 0);
            _veuSaveSurface(pDstSurface, "fadeblack");

            process.uiProgress         = 200;
            veuFadeBlackProc(VEU_NULL, pSrcSurface, pSrcSurface, pDstSurface, &process, 0);
            _veuSaveSurface(pDstSurface, "fadeblack");

            process.uiProgress         = 400;
            veuFadeBlackProc(VEU_NULL, pSrcSurface, pSrcSurface, pDstSurface, &process, 0);
            _veuSaveSurface(pDstSurface, "fadeblack");

            process.uiProgress         = 430;
            veuFadeBlackProc(VEU_NULL, pSrcSurface, pSrcSurface, pDstSurface, &process, 0);
            _veuSaveSurface(pDstSurface, "fadeblack");

            process.uiProgress         = 450;
            veuFadeBlackProc(VEU_NULL, pSrcSurface, pSrcSurface, pDstSurface, &process, 0);
            _veuSaveSurface(pDstSurface, "fadeblack");

            process.uiProgress         = 500;
            veuFadeBlackProc(VEU_NULL, pSrcSurface, pSrcSurface, pDstSurface, &process, 0);
            _veuSaveSurface(pDstSurface, "fadeblack");

            process.uiProgress         = 540;
            veuFadeBlackProc(VEU_NULL, pSrcSurface, pSrcSurface, pDstSurface, &process, 0);
            _veuSaveSurface(pDstSurface, "fadeblack");

            process.uiProgress         = 550;
            veuFadeBlackProc(VEU_NULL, pSrcSurface, pSrcSurface, pDstSurface, &process, 0);
            _veuSaveSurface(pDstSurface, "fadeblack");

            process.uiProgress         = 570;
            veuFadeBlackProc(VEU_NULL, pSrcSurface, pSrcSurface, pDstSurface, &process, 0);
            _veuSaveSurface(pDstSurface, "fadeblack");

            process.uiProgress         = 600;
            veuFadeBlackProc(VEU_NULL, pSrcSurface, pSrcSurface, pDstSurface, &process, 0);
            _veuSaveSurface(pDstSurface, "fadeblack");

            process.uiProgress         = 1000;
            veuFadeBlackProc(VEU_NULL, pSrcSurface, pSrcSurface, pDstSurface, &process, 0);
            _veuSaveSurface(pDstSurface, "fadeblack");

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

