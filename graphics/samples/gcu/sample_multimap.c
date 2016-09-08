#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WINCE
#include <windows.h>
#endif

#define MALLOC_PTR_CNT (200)
#define MULTIMAP_SURFACE_CNT (2000)
#define MALLOC_THRESHOLD (0.3f)
#define FREE_THRESHOLD (0.5f)

#include "gcu.h"
//---------------------------------------------------------------------
//---------------------------------------------------------------------
// Random functions.
static unsigned long int random_value = 32557;
#define RANDOM_MAX 32767

void random_srand(unsigned int seed)
{
    random_value = seed;
}

int random_rand(void)
{
    random_value = random_value * 1103515245 + 12345;
    return (unsigned int)(random_value / 65536) % 32768;
}

float Random_r(float Random_low, float Random_hi)
{
    float x;
    float y, z;

    x = (float)random_rand()/(RANDOM_MAX);
    y = (1 - x)*Random_low;
    z = x * Random_hi;

    return y + z ;
}

unsigned long int Random_i(unsigned long int Random_low, unsigned long int Random_hi)
{
    unsigned long int x, y;
    x = random_rand();
    if ( (y = (Random_hi - Random_low) ) > RANDOM_MAX)
        y = Random_low + y / (RANDOM_MAX) * x;
    else
        y = Random_low + (y * x + (RANDOM_MAX)/y) / (RANDOM_MAX);
    return y;
}

int Random_int(int h)
{
    return random_rand() % h;
}

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

    GCUSurface pSurface [MULTIMAP_SURFACE_CNT] = {0};
    char * ptr_malloc[MULTIMAP_SURFACE_CNT] = {0};
    int num_malloc = 0;

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



    int i = 0;
    int j = 0;
    int count =0 ;
    int success = 1;
    int random[] = {16, 160, 64, 400, 416};

    char * ptr_ori[MALLOC_PTR_CNT] = {0};
    char * prt_ali[MALLOC_PTR_CNT] = {0};
    int width[MALLOC_PTR_CNT] = {0};
    int height[MALLOC_PTR_CNT] = {0};

    random_srand(2299);

    for(j = 0; j < MALLOC_PTR_CNT; j++)
    {
        width[j] = random[Random_int(sizeof(random)/sizeof(int))];
        height[j] = random[Random_int(sizeof(random)/sizeof(int))];
        ptr_ori[j] = malloc(width[j]*height[j]*4 + 63);
        if(!ptr_ori[j])
        {
            success = 0;
            break;
        }
        prt_ali[j] = (char*)(((unsigned int)ptr_ori[j] + 63) & ~63);
        memset(prt_ali[j], 0, width[j]*height[j]*4);
    }
    if (!success)
    {
        for(j = 0; j < MALLOC_PTR_CNT; j++)
        {
            if(!ptr_ori[j])
            {
                free(ptr_ori[j]);
            }
        }
        return 1;
    }

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
        gcuTerminate();
        for(j = 0; j < MALLOC_PTR_CNT; j++)
        {
            if(!ptr_ori[j])
            {
                free(ptr_ori[j]);
            }
        }

        exit(result);
    }
    pDstSurface = _gcuCreateBuffer(pContextA, 800 , 600, GCU_FORMAT_ARGB8888, &dstVirtAddr, &dstPhysAddr);
    if(pDstSurface == NULL)
    {
        result = gcuGetError();
        gcuDestroyContext(pContextA);
        gcuTerminate();
        for(j = 0; j < MALLOC_PTR_CNT; j++)
        {
            if(!ptr_ori[j])
            {
                free(ptr_ori[j]);
            }
        }
        exit(result);
    }

    for(j = 0; j < MULTIMAP_SURFACE_CNT; j++)
    {
        int index = Random_int(MALLOC_PTR_CNT);
        GCU_FORMAT format =  random_rand() % 2 ? GCU_FORMAT_ARGB8888 : GCU_FORMAT_RGB565;
        int tmpHeight = random_rand() % 2 ? height[index] : height[index]/2;
        int tmpWidth = width[index];
        if(Random_r(0, 1) < MALLOC_THRESHOLD)
        {
            int tempW = Random_i(1, 20) * 16;
            int tempH = Random_i(1, 40) * 4;
            int size  = tempW * tempH * ((format==GCU_FORMAT_ARGB8888)?4:2) + 63;
            ptr_malloc[num_malloc] = malloc(size);
            if(ptr_malloc[num_malloc] == NULL)
            {
                pSurface[j] = _gcuCreatePreAllocBuffer(pContextA, tmpWidth, tmpHeight,
                    format, 1, (GCUVirtualAddr) prt_ali[index], 0, 0);
                printf("Create pSurface[%d] %p from new malloc failed\n",j,pSurface[j]);fflush(stdout);
            }
            else
            {
                tmpWidth = tempW;
                tmpHeight = tempH;
                GCUVirtualAddr aligned = (GCUVirtualAddr) (((unsigned int)ptr_malloc[num_malloc]+63)&~63);
                memset(ptr_malloc[num_malloc], 0, size);
                pSurface[j] = _gcuCreatePreAllocBuffer(pContextA, tmpWidth, tmpHeight,
                    format, 1, aligned, 0, 0);
                num_malloc++;
                printf("Create pSurface[%d] %p from new malloc succeed\n",j,pSurface[j]);fflush(stdout);
            }
        }
        else
        {
            pSurface[j] = _gcuCreatePreAllocBuffer(pContextA, tmpWidth, tmpHeight,
                format, 1, (GCUVirtualAddr) prt_ali[index], 0, 0);
            printf("Create pSurface[%d] %p from old malloc\n",j, pSurface[j]);fflush(stdout);
        }
        if(j > 0 && Random_r(0, 1) < FREE_THRESHOLD)
        {
            int freeindex = Random_int(j);
            if(pSurface[freeindex])
            {
         _gcuDestroyBuffer(pContextA, pSurface[freeindex]);
        pSurface[freeindex] = NULL;
            }
        }

        if(pSurface[j]==GCU_NULL)
        {
            count++;
        }
        memset(&fillData, 0, sizeof(fillData));
        fillData.pSurface = pSurface[j];
        fillData.color = ((unsigned int)Random_int(256) << 24)|((unsigned int)Random_int(256) << 16)|((unsigned int)Random_int(256) << 8)|((unsigned int)Random_int(256));
        fillData.bSolidColor = GCU_TRUE;

        srcRect.left = Random_int(tmpWidth - 2);
        srcRect.top = Random_int(tmpHeight - 2);
        srcRect.right = Random_i(srcRect.left+1, tmpWidth - 1);
        srcRect.bottom = Random_i(srcRect.top+1, tmpHeight - 1);
        fillData.pRect = &srcRect;
        gcuFill(pContextA, &fillData);
        memset(&blendData, 0, sizeof(blendData));
        blendData.pSrcSurface = pSurface[j];
        blendData.pDstSurface = pDstSurface;
        blendData.pSrcRect = NULL;
        blendData.pDstRect = NULL;
        blendData.rotation = (GCU_ROTATION)Random_int(6);
        if((int)blendData.rotation >= 5)
        {
            blendData.rotation = GCU_ROTATION_270;
        }
        blendData.blendMode = GCU_BLEND_SRC_OVER;
        blendData.srcGlobalAlpha = 15;
        blendData.dstGlobalAlpha = 0xff;

        gcuBlend(pContextA, &blendData);
        gcuFinish(pContextA);
    }
    printf("Create pSurface failed %d times\n",count);

    gcuFinish(pContextA);
    _gcuDumpSurface(pContextA, pDstSurface, "multimap");

    for(j = 0; j < MULTIMAP_SURFACE_CNT; j++)
    {
        if(pSurface[j])
        {
            _gcuDestroyBuffer(pContextA, pSurface[j]);
            pSurface[j] = NULL;
        }
    }

    _gcuDestroyBuffer(pContextA, pDstSurface);
    gcuDestroyContext(pContextA);
    gcuTerminate();

    for(j = 0; j < MALLOC_PTR_CNT; j++)
    {
        if(!ptr_ori[j])
        {
            free(ptr_ori[j]);
        }
    }
    for(j = 0; j < num_malloc; j++)
    {
        if(!ptr_malloc[j])
        {
            free(ptr_malloc[j]);
        }
    }

    return 0;
}




