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
        float f = width*height/((float)total_time/PERF_LOOP);\
        printf(" # %5s: \t Looping %d times elapse %lld us.\t %.1f ms\t %.1f M pixel/sec\n", str, PERF_LOOP, total_time, (float)total_time/PERF_LOOP/1000,f);\
        total_time = 0;\
    }

//This case test Cacheable/non-cacheable performance
GCUbool Copy8888To8888(GCUContext  pContext, GCUSurface pSurfaceARGB, GCUSurface pDstSurface);

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
    GCUSurface pSrcSurface1 = NULL;
    GCUSurface pSrcSurfaceCache1 = NULL;
    GCUSurface pSrcSurfaceCache2 = NULL;
    GCUSurface pSrcSurfaceNonCache1 = NULL;
    GCUSurface pSrcSurfaceNonCache2 = NULL;
    GCUSurface pDstSurfaceCache = NULL;
    GCUSurface pDstSurfaceNonCache = NULL;
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

    unsigned int        width       = 1280;
    unsigned int        height      = 720;
    int j = 0;
    int k = 0;

    int i = 0;
    printf("width = %d, height = %d \n", width, height);

    memset(&initData, 0, sizeof(initData));

    pSrcSurface1 = _gcuLoadRGBSurfaceFromFile(pContextA, "source.bmp");
    pSrcSurfaceNonCache1= _gcuCreateBuffer(pContextA, width , height, GCU_FORMAT_ARGB8888, &dstVirtAddr, &dstPhysAddr);
    pSrcSurfaceNonCache2= _gcuCreateBuffer(pContextA, width , height, GCU_FORMAT_ARGB8888, &dstVirtAddr, &dstPhysAddr);
    pDstSurfaceNonCache = _gcuCreateBuffer(pContextA, width , height, GCU_FORMAT_NV21, &dstVirtAddr, &dstPhysAddr);

    void* addrSrc1 = malloc(width*height*4+63);
    void* addrSrcAlign1 = (void*)ALIGN((unsigned int)addrSrc1, 64);
    pSrcSurfaceCache1= _gcuCreatePreAllocBuffer(pContextA, width, height, GCU_FORMAT_ARGB8888, GCU_TRUE, addrSrcAlign1, GCU_FALSE, 0);

    void* addrSrc2 = malloc(width*height*4+63);
    void* addrSrcAlign2 = (void*)ALIGN((unsigned int)addrSrc2, 64);
    pSrcSurfaceCache2 = _gcuCreatePreAllocBuffer(pContextA, width, height, GCU_FORMAT_ARGB8888, GCU_TRUE, addrSrcAlign2, GCU_FALSE, 0);

    void* addrDst3 = malloc(width*height*3/2+63);
    void* addrDstAlign3 = (void*)ALIGN((unsigned int)addrDst3, 64);
    pDstSurfaceCache = _gcuCreatePreAllocBuffer(pContextA, width, height, GCU_FORMAT_NV21, GCU_TRUE, addrDstAlign3, GCU_FALSE, 0);
    if(pSrcSurface1 && pSrcSurfaceCache1 && pSrcSurfaceCache2 && pSrcSurfaceNonCache1 && pSrcSurfaceNonCache2&& pDstSurfaceNonCache && pDstSurfaceCache)
    {
        while(i < 1)
        {

            GCU_BLT_DATA bltData;
            memset(&bltData, 0, sizeof(bltData));

            bltData.pSrcSurface = pSrcSurface1;
            bltData.pDstSurface = pSrcSurfaceCache1;

            bltData.rotation = GCU_ROTATION_0;
            _gcuFlushCache(pContextA,(void*)addrSrcAlign1,width*height*4,GCU_INVALIDATE);
            gcuBlit(pContextA, &bltData);

            bltData.pDstSurface = pSrcSurfaceNonCache1;
            gcuBlit(pContextA, &bltData);
            gcuFinish(pContextA);


            printf("Src Cache, Dst Cache\n");
            PERF_START();
            Copy8888To8888(pContextA, pSrcSurfaceCache1, pSrcSurfaceCache2);
            PERF_STOP();
            PERF_PROFILE("Copy8888To8888");

            PERF_START();
            _gcuConvertARGB2YUV(pContextA, pSrcSurfaceCache1, pDstSurfaceCache);
            PERF_STOP();
            PERF_PROFILE("NV21_source");


            printf("Src Cache, Dst NonCache\n");
            PERF_START();
            Copy8888To8888(pContextA, pSrcSurfaceCache1, pSrcSurfaceNonCache2);
            PERF_STOP();
            PERF_PROFILE("Copy8888To8888");


            PERF_START();
            _gcuConvertARGB2YUV(pContextA, pSrcSurfaceCache1, pDstSurfaceNonCache);
            PERF_STOP();
            PERF_PROFILE("NV21_source");

            printf("Src NonCache, Dst Cache\n");
            PERF_START();
            Copy8888To8888(pContextA, pSrcSurfaceNonCache1, pSrcSurfaceCache2);
            PERF_STOP();
            PERF_PROFILE("Copy8888To8888");

            PERF_START();
            _gcuConvertARGB2YUV(pContextA, pSrcSurfaceNonCache1, pDstSurfaceCache);
            PERF_STOP();
            PERF_PROFILE("NV21_source");


            printf("Src NonCache, Dst NonCache\n");
            PERF_START();
            Copy8888To8888(pContextA, pSrcSurfaceNonCache1, pSrcSurfaceNonCache2);
            PERF_STOP();
            PERF_PROFILE("Copy8888To8888");


            PERF_START();
            _gcuConvertARGB2YUV(pContextA, pSrcSurfaceNonCache1, pDstSurfaceNonCache);
            PERF_STOP();
            PERF_PROFILE("NV21_source");


            i++;

        }
    }

    if(pSrcSurface1)
    {
         _gcuDestroyBuffer(pContextA, pSrcSurface1);
    }
    if(pSrcSurfaceCache1)
    {
         _gcuDestroyBuffer(pContextA, pSrcSurfaceCache1);
    }
    if(pSrcSurfaceCache2)
    {
         _gcuDestroyBuffer(pContextA, pSrcSurfaceCache2);
    }
    if(pDstSurfaceCache)
    {
         _gcuDestroyBuffer(pContextA, pDstSurfaceCache);
    }
    if(pDstSurfaceNonCache)
    {
         _gcuDestroyBuffer(pContextA, pDstSurfaceNonCache);
    }
    if(pSrcSurfaceNonCache1)
    {
         _gcuDestroyBuffer(pContextA, pSrcSurfaceNonCache1);
    }
    if(pSrcSurfaceNonCache2)
    {
         _gcuDestroyBuffer(pContextA, pSrcSurfaceNonCache2);
    }
    free(addrSrc1);
    free(addrSrc2);
    free(addrDst3);
    gcuDestroyContext(pContextA);
    gcuTerminate();


    return 0;
}

