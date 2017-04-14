#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WINCE
#include <windows.h>
#endif

#include "gpu_csc.h"
#include <sys/time.h>

#define ALIGN(n, align) \
( \
         ((n) + ((align) - 1)) & ~((align) - 1) \
)
typedef unsigned long long  MVU64;
MVU64 GetTickCountMicroSec()
{
    struct timeval g_tv;
    struct timezone g_tz;
    gettimeofday(&g_tv, &g_tz);
    return g_tv.tv_sec * 1000000 + g_tv.tv_usec;
}
int PERF_LOOP = 100;

static MVU64 total_time;
int p;
#define PERF_START() \
    {\
        MVU64 start = GetTickCountMicroSec();\
        for(p=0; p<PERF_LOOP;p++) {

#define PERF_STOP() \
        }\
        MVU64 end = GetTickCountMicroSec();\
        total_time += end-start;\
    }
#define PERF_PROFILE(str)\
    {\
        printf(" # %5s: \t Looping %d times elapse %lld us.\t %.1f ms\n", str, PERF_LOOP, total_time, (float)total_time/PERF_LOOP/1000);\
        total_time = 0;\
    }

//This case test performace of YUV420P ->GPU resize -> ARGB8888 -> neon csc ->NV21
//3264*2448 -> 1280*720
//1280*720 -> 3264*2448
//1280*720 -> 1280*720

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
    GCUSurface pYUVNonCache = NULL;
    GCUSurface pARGBCache = NULL;
    GCUSurface pARGBNonCache = NULL;
    GCUSurface pNV21Cache = NULL;
    GCUSurface pNV21NonCache = NULL;
    GCUVirtualAddr      srcVirtAddr = 0;
    GCUPhysicalAddr     srcPhysAddr = 0;
    GCUVirtualAddr      dstVirtAddr = 0;
    GCUPhysicalAddr     dstPhysAddr = 0;

    GCU_INIT_DATA initData;
    GCUContext pContextA;
    GCU_CONTEXT_DATA contextData;
    GCUenum result;

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


    unsigned int        srcwidth       = 3264;
    unsigned int        srcheight      = 2448;
    unsigned int        dstwidth       = 1280;
    unsigned int        dstheight      = 720;

    int j = 0;
    for(j=0;j<3;j++)
    {
        if(j == 1)
        {
            srcwidth = 1280;
            srcheight =720;
            dstwidth =3264;
            dstheight = 2448;
        }

        if(j == 2)
        {
            srcwidth = 1280;
            srcheight =720;
            dstwidth =1280;
            dstheight = 720;
        }
        printf("Src: %d * %d    Dst: %d * %d\n",srcwidth,srcheight,dstwidth,dstheight);
        int i=0;
        memset(&initData, 0, sizeof(initData));

        pYUVNonCache = _gcuCreateBuffer(pContextA, srcwidth , srcheight, GCU_FORMAT_I420, &dstVirtAddr, &dstPhysAddr);
        pARGBNonCache= _gcuCreateBuffer(pContextA, dstwidth , dstheight, GCU_FORMAT_ARGB8888, &dstVirtAddr, &dstPhysAddr);
        pNV21NonCache= _gcuCreateBuffer(pContextA, dstwidth , dstheight, GCU_FORMAT_NV21, &dstVirtAddr, &dstPhysAddr);

        void* addrSrc2 = malloc(dstwidth*dstheight*4+63);
        void* addrSrcAlign2 = (void*)ALIGN((unsigned int)addrSrc2, 64);
        pARGBCache= _gcuCreatePreAllocBuffer(pContextA, dstwidth, dstheight, GCU_FORMAT_ARGB8888, GCU_TRUE, addrSrcAlign2, GCU_FALSE, 0);

        void* addrDst3 = malloc(dstwidth*dstheight*3/2+63);
        void* addrDstAlign3 = (void*)ALIGN((unsigned int)addrDst3, 64);
        pNV21Cache = _gcuCreatePreAllocBuffer(pContextA, dstwidth, dstheight, GCU_FORMAT_NV21, GCU_TRUE, addrDstAlign3, GCU_FALSE, 0);

        if(pYUVNonCache && pARGBNonCache && pARGBCache && pNV21NonCache && pNV21Cache)
        {
            while(i < 1)
            {

                GCU_BLT_DATA bltData;
                memset(&bltData, 0, sizeof(bltData));
                bltData.pSrcSurface = pYUVNonCache;
                bltData.pDstSurface = pARGBNonCache;
                bltData.rotation = GCU_ROTATION_0;
                gcuSet(pContextA, GCU_QUALITY, GCU_QUALITY_HIGH);
                PERF_START();
                gcuBlit(pContextA, &bltData);
                gcuFinish(pContextA);
                PERF_STOP();
                PERF_PROFILE("resize noncache");

                PERF_START();
                _gcuConvertARGB2YUV(pContextA, pARGBNonCache, pNV21NonCache);
                PERF_STOP();
                PERF_PROFILE("csc noncache");
                _gcuDumpSurface(pContextA, pARGBNonCache, "resize-noncache");
                _gcuDumpSurface(pContextA, pNV21NonCache, "csc-noncache");


                bltData.pSrcSurface = pYUVNonCache;
                bltData.pDstSurface = pARGBCache;
                bltData.rotation = GCU_ROTATION_0;
                gcuSet(pContextA, GCU_QUALITY, GCU_QUALITY_HIGH);

                PERF_START();
                _gcuFlushCache(pContextA,(void*)addrSrcAlign2,dstwidth*dstheight*4,GCU_INVALIDATE);
                PERF_STOP();
                PERF_PROFILE("flush dst");

                PERF_START();
                gcuBlit(pContextA, &bltData);
                gcuFinish(pContextA);
                PERF_STOP();
                PERF_PROFILE("resize cache");

                PERF_START();
                _gcuConvertARGB2YUV(pContextA, pARGBCache, pNV21Cache);
                PERF_STOP();
                PERF_PROFILE("csc cache");
                _gcuDumpSurface(pContextA, pARGBCache, "resize-cache");
                _gcuDumpSurface(pContextA, pNV21Cache, "csc-cache");

                i++;

            }
        }
        if(pYUVNonCache)
        {
            _gcuDestroyBuffer(pContextA, pYUVNonCache);
        }
        if(pARGBCache)
        {
            _gcuDestroyBuffer(pContextA, pARGBCache);
        }
        if(pARGBNonCache)
        {
            _gcuDestroyBuffer(pContextA, pARGBNonCache);
        }
        if(pNV21Cache)
        {
            _gcuDestroyBuffer(pContextA, pNV21Cache);
        }
        if(pNV21NonCache)
        {
            _gcuDestroyBuffer(pContextA, pNV21NonCache);
        }

        free(addrSrc2);
        free(addrDst3);
    }



    gcuDestroyContext(pContextA);
    gcuTerminate();


    return 0;
}

