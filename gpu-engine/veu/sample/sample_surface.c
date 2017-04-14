#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WINCE
#include <windows.h>
#endif

#include "veu.h"
#include "gcu.h"
#include "M4VSS3GPP_API.h"
#include "M4xVSS_API.h"
#include "M4xVSS_Internal.h"


/* Sample for veuCreatePreAllocSurface() and veuUpdatePreAllocBuffer() */
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
    VEUSurface      pSrcSurface  = NULL;
    VEUSurface      pDstSurface  = NULL;
    VEUSurface      pDstSurface1 = NULL;
    VEUSurface      pDstSurface2 = NULL;
    VEUSurface      pDstSurface3 = NULL;
    VEUSurface      pDstSurface4 = NULL;
    VEUSurface      pDstSurface5 = NULL;

    VEUVirtualAddr  virtualAddr1,virtualAddr2,virtualAddr3;
    VEUPhysicalAddr physicalAddr1,physicalAddr2,physicalAddr3;

    VEU_INIT_DATA   initData;

    unsigned int    width       = 640;
    unsigned int    height      = 480;

    int i = 0;
    M4xVSS_ColorStruct effectData;

    memset(&initData, 0, sizeof(initData));
    veuInitialize(&initData);

    pSrcSurface   = _veuLoadSurface("test.yuv", VEU_FORMAT_I420, width, height);
    pDstSurface1  = veuCreateSurface(width , height, VEU_FORMAT_I420);
    pDstSurface2  = veuCreateSurface(width , height, VEU_FORMAT_I420);

    GCU_SURFACE_LOCK_DATA   lockData;
    GCU_ALLOC_INFO          allocInfos[3];
    VEUbool                 bLockOk = VEU_FALSE;

    VEUContext pContext = _veuGetContext();
    memset(allocInfos, 0, sizeof(GCU_ALLOC_INFO)*3);
    memset(&lockData, 0, sizeof(lockData));
    lockData.pSurface = pDstSurface1;
    lockData.flag.bits.preserve = 1;
    lockData.arraySize = 3;
    lockData.pAllocInfos = allocInfos;
    bLockOk = gcuLockSurface(pContext, &lockData);

    pDstSurface = veuCreatePreAllocSurface(width, height, VEU_FORMAT_I420, VEU_TRUE, VEU_TRUE,
                                           allocInfos[0].virtualAddr,
                                           allocInfos[1].virtualAddr,
                                           allocInfos[2].virtualAddr,
                                           allocInfos[0].physicalAddr,
                                           allocInfos[1].physicalAddr,
                                           allocInfos[2].physicalAddr);
    gcuUnlockSurface(pContext, pDstSurface1);

    if(pSrcSurface && pDstSurface1 && pDstSurface2 && pDstSurface)
    {
        while(i < 1)
        {

            memset(&effectData, 0, sizeof(effectData));
            effectData.colorEffectType = M4xVSS_kVideoEffectType_BlackAndWhite;
            veuEffectColorProc(&effectData, pSrcSurface, pDstSurface, VEU_NULL, 0);
            _veuSaveSurface(pDstSurface, "surface");

            memset(&allocInfos, 0, sizeof(allocInfos));
            memset(&lockData, 0, sizeof(lockData));
            lockData.pSurface = pDstSurface2;
            lockData.flag.bits.preserve = 1;
            lockData.arraySize = 3;
            lockData.pAllocInfos = allocInfos;
            bLockOk = gcuLockSurface(pContext, &lockData);
            veuUpdatePreAllocSurface(pDstSurface, VEU_TRUE, VEU_TRUE,
                                     allocInfos[0].virtualAddr,
                                     allocInfos[1].virtualAddr,
                                     allocInfos[2].virtualAddr,
                                     allocInfos[0].physicalAddr,
                                     allocInfos[1].physicalAddr,
                                     allocInfos[2].physicalAddr);
            gcuUnlockSurface(pContext, pDstSurface2);

            effectData.colorEffectType = M4xVSS_kVideoEffectType_Negative;
            veuEffectColorProc(&effectData, pSrcSurface, pDstSurface, VEU_NULL, 0);
            _veuSaveSurface(pDstSurface, "surface");

            pDstSurface3 = _gcuCreateBuffer(pContext, width, height, GCU_FORMAT_A8, &virtualAddr1, &physicalAddr1);
            pDstSurface4 = _gcuCreateBuffer(pContext, width/2, height/2, GCU_FORMAT_A8, &virtualAddr2, &physicalAddr2);
            pDstSurface5 = _gcuCreateBuffer(pContext, width/2, height/2, GCU_FORMAT_A8, &virtualAddr3, &physicalAddr3);
            veuUpdatePreAllocSurface(pDstSurface, VEU_TRUE, VEU_TRUE,
                                     virtualAddr1,
                                     virtualAddr2,
                                     virtualAddr3,
                                     physicalAddr1,
                                     physicalAddr2,
                                     physicalAddr3);

            effectData.colorEffectType = M4xVSS_kVideoEffectType_Sepia;
            veuEffectColorProc(&effectData, pSrcSurface, pDstSurface, VEU_NULL, 0);
            _veuSaveSurface(pDstSurface, "surface");

            i++;

        }
    }

    if(pSrcSurface)
    {
        veuDestroySurface(pSrcSurface);
    }
    if(pDstSurface1)
    {
        veuDestroySurface(pDstSurface1);
    }
    if(pDstSurface)
    {
        veuDestroySurface(pDstSurface);
    }
    if(pDstSurface2)
    {
        veuDestroySurface(pDstSurface2);
    }
    if(pDstSurface3)
    {
        veuDestroySurface(pDstSurface3);
    }
    if(pDstSurface4)
    {
        veuDestroySurface(pDstSurface4);
    }
    if(pDstSurface5)
    {
        veuDestroySurface(pDstSurface5);
    }
    veuTerminate();

    return 0;
}

