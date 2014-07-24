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
        printf("# %5s: \t Looping %d times elapse %lld us. \t%.1f ms \t%.1f M pixel/sec\n", str, PERF_LOOP, total_time, (float)total_time/PERF_LOOP/1000,f);\
        total_time = 0;\
    }

//This case test ARGB8888 -> UYVY/I420/NV21 neon primitives performance
//(src and dst are all cacheable)

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
    GCUSurface pSrcSurface2 = NULL;
    GCUSurface pSrcSurface3 = NULL;
    GCUSurface pDstSurface1 = NULL;
    GCUSurface pDstSurface2 = NULL;
    GCUSurface pDstSurface3 = NULL;
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

    unsigned int        width;
    unsigned int        height;
    int j = 0;
    for(j=0;j<5;j++)
    {
        if(j==0)
        {
            width =320;
            height=480;
        }
        if(j==1)
        {
            width =640;
            height=480;
        }
        if(j==2)
        {
            width =800;
            height=480;
        }
        if(j==3)
        {
            width =1280;
            height=720;
        }
        if(j==4)
        {
            width =1920;
            height=1080;
        }


        printf("width = %d, height = %d \n", width, height);

        pSrcSurface1 = _gcuLoadRGBSurfaceFromFile(pContextA, "source.bmp");

        void* addrSrc1 = malloc(width*height*4+63);
        void* addrSrcAlign1 = (void*)ALIGN((unsigned int)addrSrc1, 64);
        pSrcSurface2 = _gcuCreatePreAllocBuffer(pContextA, width, height, GCU_FORMAT_ARGB8888, GCU_TRUE, (GCUVirtualAddr)addrSrcAlign1, GCU_FALSE, (GCUPhysicalAddr)GCU_NULL);

        void* addrSrc2 = malloc(width*height*4+63);
        void* addrSrcAlign2 = (void*)ALIGN((unsigned int)addrSrc2, 64);
        pSrcSurface3 = _gcuCreatePreAllocBuffer(pContextA, width, height, GCU_FORMAT_ARGB8888, GCU_TRUE, (GCUVirtualAddr)addrSrcAlign2, GCU_FALSE, (GCUPhysicalAddr)GCU_NULL);

        void* addrDst1 = malloc(width*height*3/2+63);
        void* addrDstAlign1 = (void*)ALIGN((unsigned int)addrDst1, 64);
        pDstSurface1 = _gcuCreatePreAllocBuffer(pContextA, width, height, GCU_FORMAT_I420, GCU_TRUE, (GCUVirtualAddr)addrDstAlign1, GCU_FALSE, (GCUPhysicalAddr)GCU_NULL);

        void* addrDst2 = malloc(width*height*2+63);
        void* addrDstAlign2 = (void*)ALIGN((unsigned int)addrDst2, 64);
        pDstSurface2 = _gcuCreatePreAllocBuffer(pContextA, width, height, GCU_FORMAT_UYVY, GCU_TRUE, (GCUVirtualAddr)addrDstAlign2, GCU_FALSE, (GCUPhysicalAddr)GCU_NULL);

        void* addrDst3 = malloc(width*height*3/2+63);
        void* addrDstAlign3 = (void*)ALIGN((unsigned int)addrDst3, 64);
        pDstSurface3 = _gcuCreatePreAllocBuffer(pContextA, width, height, GCU_FORMAT_NV21, GCU_TRUE, (GCUVirtualAddr)addrDstAlign3, GCU_FALSE, (GCUPhysicalAddr)GCU_NULL);

        if(pSrcSurface1 && pSrcSurface2 && pSrcSurface3 && pDstSurface1 && pDstSurface2 && pDstSurface3)
        {

            GCU_BLT_DATA bltData;
            GCU_FILL_DATA fillData;

            memset(&bltData, 0, sizeof(bltData));
            bltData.pSrcSurface = pSrcSurface1;
            bltData.pDstSurface = pSrcSurface2;

            bltData.rotation = GCU_ROTATION_0;
            _gcuFlushCache(pContextA,(void*)addrSrcAlign1,width*height*4,GCU_INVALIDATE);
            gcuBlit(pContextA, &bltData);
            gcuFinish(pContextA);

            PERF_START();
            Copy8888To8888(pContextA, pSrcSurface2, pSrcSurface3);
            PERF_STOP();
            PERF_PROFILE("Copy8888To8888");
            _gcuDumpSurface(pContextA, pSrcSurface3, "Copy");

            PERF_START();
            _gcuConvertARGB2YUV(pContextA, pSrcSurface2, pDstSurface1);
            PERF_STOP();
            PERF_PROFILE("YUV_source");
            _gcuDumpSurface(pContextA, pDstSurface1, "YUV_source");


            PERF_START();
            _gcuConvertARGB2YUV(pContextA, pSrcSurface2, pDstSurface2);
            PERF_STOP();
            PERF_PROFILE("UYVY_source");
            _gcuDumpSurface(pContextA, pDstSurface2, "UYVY_source");


            PERF_START();
            _gcuConvertARGB2YUV(pContextA, pSrcSurface2, pDstSurface3);
            PERF_STOP();
            PERF_PROFILE("NV21_source");
            _gcuDumpSurface(pContextA, pDstSurface3, "NV21_source");
        }
        if(pSrcSurface1)
        {
            _gcuDestroyBuffer(pContextA, pSrcSurface1);
        }
        if(pSrcSurface2)
        {
            _gcuDestroyBuffer(pContextA, pSrcSurface2);
        }
        if(pSrcSurface3)
        {
            _gcuDestroyBuffer(pContextA, pSrcSurface3);
        }
        if(pDstSurface1)
        {
            _gcuDestroyBuffer(pContextA, pDstSurface1);
        }
        if(pDstSurface2)
        {
            _gcuDestroyBuffer(pContextA, pDstSurface2);
        }
        if(pDstSurface3)
        {
            _gcuDestroyBuffer(pContextA, pDstSurface3);
        }
        free(addrSrc1);
        free(addrSrc2);
        free(addrDst1);
        free(addrDst2);
        free(addrDst3);
    }


    gcuDestroyContext(pContextA);
    gcuTerminate();

    return 0;
}

