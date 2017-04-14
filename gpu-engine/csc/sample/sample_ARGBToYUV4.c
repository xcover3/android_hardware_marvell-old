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
    GCUSurface pSrcSurface, pDstSurface, pDumpSurface, pTempSurface;
    GCU_INIT_DATA initData;
    GCUContext pContextA;
    GCU_CONTEXT_DATA contextData;
    GCUenum result;
    GCU_BLT_DATA bltData;
    GCU_FILL_DATA fillData;

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

    pSrcSurface = _gcuLoadRGBSurfaceFromFile(pContextA, "source1.bmp");

    pDstSurface = _gcuCreateBuffer(pContextA, 1920 , 1080, GCU_FORMAT_I420, 0, 0);
    pDumpSurface = _gcuCreateBuffer(pContextA, 1920 , 1080, GCU_FORMAT_ARGB8888, 0, 0);
    pTempSurface = _gcuCreateBuffer(pContextA, 1920 , 1080, GCU_FORMAT_ARGB8888, 0, 0);


    memset(&bltData, 0, sizeof(bltData));
    bltData.pSrcSurface = pSrcSurface;
    bltData.pDstSurface = pTempSurface;

    bltData.rotation = GCU_ROTATION_0;
    gcuBlit(pContextA, &bltData);
    gcuFinish(pContextA);
    _gcuConvertARGB2YUV(pContextA, pTempSurface, pDstSurface);

    memset(&bltData, 0, sizeof(bltData));
    bltData.pSrcSurface = pDstSurface;
    bltData.pDstSurface = pDumpSurface;

    bltData.rotation = GCU_ROTATION_0;
    gcuBlit(pContextA, &bltData);
    gcuFinish(pContextA);

    _gcuDumpSurface(pContextA, pTempSurface, "src1");
    _gcuDumpSurface(pContextA, pDstSurface, "yuv1");
    _gcuDumpSurface(pContextA, pDumpSurface, "dest1");
    _gcuDestroyBuffer(pContextA, pSrcSurface);
    pSrcSurface = _gcuLoadRGBSurfaceFromFile(pContextA, "source2.bmp");

    memset(&bltData, 0, sizeof(bltData));
    bltData.pSrcSurface = pSrcSurface;
    bltData.pDstSurface = pTempSurface;

    bltData.rotation = GCU_ROTATION_0;
    gcuBlit(pContextA, &bltData);
    gcuFinish(pContextA);
    _gcuConvertARGB2YUV(pContextA, pTempSurface, pDstSurface);

    memset(&bltData, 0, sizeof(bltData));
    bltData.pSrcSurface = pDstSurface;
    bltData.pDstSurface = pDumpSurface;

    bltData.rotation = GCU_ROTATION_0;
    gcuBlit(pContextA, &bltData);
    gcuFinish(pContextA);

    _gcuDumpSurface(pContextA, pTempSurface, "src2");
    _gcuDumpSurface(pContextA, pDstSurface, "yuv2");
    _gcuDumpSurface(pContextA, pDumpSurface, "dest2");
    _gcuDestroyBuffer(pContextA, pSrcSurface);
    pSrcSurface = _gcuLoadRGBSurfaceFromFile(pContextA, "source3.bmp");


    memset(&bltData, 0, sizeof(bltData));
    bltData.pSrcSurface = pSrcSurface;
    bltData.pDstSurface = pTempSurface;

    bltData.rotation = GCU_ROTATION_0;
    gcuBlit(pContextA, &bltData);
    gcuFinish(pContextA);
    _gcuConvertARGB2YUV(pContextA, pTempSurface, pDstSurface);

    memset(&bltData, 0, sizeof(bltData));
    bltData.pSrcSurface = pDstSurface;
    bltData.pDstSurface = pDumpSurface;

    bltData.rotation = GCU_ROTATION_0;
    gcuBlit(pContextA, &bltData);
    gcuFinish(pContextA);

    _gcuDumpSurface(pContextA, pTempSurface, "src3");
    _gcuDumpSurface(pContextA, pDstSurface, "yuv3");
    _gcuDumpSurface(pContextA, pDumpSurface, "dest3");
    _gcuDestroyBuffer(pContextA, pSrcSurface);
    pSrcSurface = _gcuLoadRGBSurfaceFromFile(pContextA, "source4.bmp");


    memset(&bltData, 0, sizeof(bltData));
    bltData.pSrcSurface = pSrcSurface;
    bltData.pDstSurface = pTempSurface;

    bltData.rotation = GCU_ROTATION_0;
    gcuBlit(pContextA, &bltData);
    gcuFinish(pContextA);
    _gcuConvertARGB2YUV(pContextA, pTempSurface, pDstSurface);

    memset(&bltData, 0, sizeof(bltData));
    bltData.pSrcSurface = pDstSurface;
    bltData.pDstSurface = pDumpSurface;

    bltData.rotation = GCU_ROTATION_0;
    gcuBlit(pContextA, &bltData);
    gcuFinish(pContextA);

    _gcuDumpSurface(pContextA, pTempSurface, "src4");
    _gcuDumpSurface(pContextA, pDstSurface, "yuv4");
    _gcuDumpSurface(pContextA, pDumpSurface, "dest4");
    _gcuDestroyBuffer(pContextA, pSrcSurface);
    pSrcSurface = _gcuLoadRGBSurfaceFromFile(pContextA, "source5.bmp");


    memset(&bltData, 0, sizeof(bltData));
    bltData.pSrcSurface = pSrcSurface;
    bltData.pDstSurface = pTempSurface;

    bltData.rotation = GCU_ROTATION_0;
    gcuBlit(pContextA, &bltData);
    gcuFinish(pContextA);
    _gcuConvertARGB2YUV(pContextA, pTempSurface, pDstSurface);

    memset(&bltData, 0, sizeof(bltData));
    bltData.pSrcSurface = pDstSurface;
    bltData.pDstSurface = pDumpSurface;

    bltData.rotation = GCU_ROTATION_0;
    gcuBlit(pContextA, &bltData);
    gcuFinish(pContextA);

    _gcuDumpSurface(pContextA, pTempSurface, "src5");
    _gcuDumpSurface(pContextA, pDstSurface, "yuv5");
    _gcuDumpSurface(pContextA, pDumpSurface, "dest5");
    _gcuDestroyBuffer(pContextA, pSrcSurface);
    pSrcSurface = _gcuLoadRGBSurfaceFromFile(pContextA, "source6.bmp");

    memset(&bltData, 0, sizeof(bltData));
    bltData.pSrcSurface = pSrcSurface;
    bltData.pDstSurface = pTempSurface;

    bltData.rotation = GCU_ROTATION_0;
    gcuBlit(pContextA, &bltData);
    gcuFinish(pContextA);
    _gcuConvertARGB2YUV(pContextA, pTempSurface, pDstSurface);

    memset(&bltData, 0, sizeof(bltData));
    bltData.pSrcSurface = pDstSurface;
    bltData.pDstSurface = pDumpSurface;

    bltData.rotation = GCU_ROTATION_0;
    gcuBlit(pContextA, &bltData);
    gcuFinish(pContextA);

    _gcuDumpSurface(pContextA, pTempSurface, "src6");
    _gcuDumpSurface(pContextA, pDstSurface, "yuv6");
    _gcuDumpSurface(pContextA, pDumpSurface, "dest6");
    _gcuDestroyBuffer(pContextA, pSrcSurface);
    pSrcSurface = _gcuLoadRGBSurfaceFromFile(pContextA, "source7.bmp");


    memset(&bltData, 0, sizeof(bltData));
    bltData.pSrcSurface = pSrcSurface;
    bltData.pDstSurface = pTempSurface;

    bltData.rotation = GCU_ROTATION_0;
    gcuBlit(pContextA, &bltData);
    gcuFinish(pContextA);
    _gcuConvertARGB2YUV(pContextA, pTempSurface, pDstSurface);

    memset(&bltData, 0, sizeof(bltData));
    bltData.pSrcSurface = pDstSurface;
    bltData.pDstSurface = pDumpSurface;

    bltData.rotation = GCU_ROTATION_0;
    gcuBlit(pContextA, &bltData);
    gcuFinish(pContextA);

    _gcuDumpSurface(pContextA, pTempSurface, "src7");
    _gcuDumpSurface(pContextA, pDstSurface, "yuv7");
    _gcuDumpSurface(pContextA, pDumpSurface, "dest7");
    _gcuDestroyBuffer(pContextA, pSrcSurface);

    _gcuDestroyBuffer(pContextA, pDstSurface);
    _gcuDestroyBuffer(pContextA, pDumpSurface);
    _gcuDestroyBuffer(pContextA, pTempSurface);
    gcuDestroyContext(pContextA);
    gcuTerminate();

    return 0;


}
