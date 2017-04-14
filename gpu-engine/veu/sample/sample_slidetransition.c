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
    VEUSurface pSrcSurface1 = NULL;
    VEUSurface pSrcSurface2 = NULL;
    VEUSurface pDstSurface = NULL;

    VEU_INIT_DATA               initData;
    M4xVSS_internal_SlideTransitionSettings slideData;
    M4VSS3GPP_ExternalProgress        process;

    unsigned int        width   = 640;
    unsigned int        height  = 480;

    int i = 0;

    memset(&initData, 0, sizeof(initData));
    veuInitialize(&initData);

    pSrcSurface1 = _veuLoadSurface("test.yuv", VEU_FORMAT_I420, width, height);
    pSrcSurface2 = _veuLoadSurface("test.yuv", VEU_FORMAT_I420, width, height);
    pDstSurface  = veuCreateSurface(width , height, VEU_FORMAT_I420);

    if(pSrcSurface1 && pSrcSurface2 && pDstSurface)
    {
        while(i < 1)
        {
            memset(&slideData, 0, sizeof(slideData));
            process.uiProgress         = 500;
            slideData.direction        = M4xVSS_SlideTransition_RightOutLeftIn;
            veuSlideTransitionProc(&slideData, pSrcSurface1, pSrcSurface2, pDstSurface, &process, 0);
            _veuSaveSurface(pDstSurface, "slide_RightOutLeftIn");

            process.uiProgress         = 400;
            slideData.direction        = M4xVSS_SlideTransition_LeftOutRightIn;
            veuSlideTransitionProc(&slideData, pSrcSurface1, pSrcSurface2, pDstSurface, &process, 0);
            _veuSaveSurface(pDstSurface, "slide_LeftOutRightIn");

            process.uiProgress         = 800;
            slideData.direction        = M4xVSS_SlideTransition_TopOutBottomIn;
            veuSlideTransitionProc(&slideData, pSrcSurface1, pSrcSurface2, pDstSurface, &process, 0);
            _veuSaveSurface(pDstSurface, "slide_TopOutBottomIn");

            process.uiProgress         = 300;
            slideData.direction        = M4xVSS_SlideTransition_BottomOutTopIn;
            veuSlideTransitionProc(&slideData, pSrcSurface1, pSrcSurface2, pDstSurface, &process, 0);
            _veuSaveSurface(pDstSurface, "slide_BottomOutTopIn");

            process.uiProgress         = 1000;
            slideData.direction        = M4xVSS_SlideTransition_RightOutLeftIn;
            veuSlideTransitionProc(&slideData, pSrcSurface1, pSrcSurface2, pDstSurface, &process, 0);
            _veuSaveSurface(pDstSurface, "slide_RightOutLeftIn_full");

            process.uiProgress         = 1000;
            slideData.direction        = M4xVSS_SlideTransition_LeftOutRightIn;
            veuSlideTransitionProc(&slideData, pSrcSurface1, pSrcSurface2, pDstSurface, &process, 0);
            _veuSaveSurface(pDstSurface, "slide_LeftOutRightIn_full");

            process.uiProgress         = 1000;
            slideData.direction        = M4xVSS_SlideTransition_TopOutBottomIn;
            veuSlideTransitionProc(&slideData, pSrcSurface1, pSrcSurface2, pDstSurface, &process, 0);
            _veuSaveSurface(pDstSurface, "slide_TopOutBottomIn_full");

            process.uiProgress         = 1000;
            slideData.direction        = M4xVSS_SlideTransition_BottomOutTopIn;
            veuSlideTransitionProc(&slideData, pSrcSurface1, pSrcSurface2, pDstSurface, &process, 0);
            _veuSaveSurface(pDstSurface, "slide_BottomOutTopIn_full");
            i++;

        }
    }

    if(pSrcSurface1)
    {
        veuDestroySurface(pSrcSurface1);
    }
    if(pSrcSurface2)
    {
        veuDestroySurface(pSrcSurface2);
    }
    if(pDstSurface)
    {
        veuDestroySurface(pDstSurface);
    }
    veuTerminate();

    return 0;
}

