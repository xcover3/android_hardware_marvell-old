#include <stdio.h>
#include <stdlib.h>
#include "gcu.h"
#include "sample_utils.h"

static GCUContext gcuContext = NULL;
static int nAllocatedMemory = 0;

void* ceu_sample_alloc_continuous_mem(unsigned int width, unsigned int height, unsigned int bpp,
    void** vir_addr, unsigned int * phy_addr)
{
    if(NULL == gcuContext)
    {
        GCU_INIT_DATA initData;
        GCU_CONTEXT_DATA contextData;

        // Init GCU library
        memset(&initData, 0, sizeof(initData));
        gcuInitialize(&initData);

        // Create Context
        memset(&contextData, 0, sizeof(contextData));
        gcuContext = gcuCreateContext(&contextData);

        if(gcuContext == NULL)
        {
            return NULL;
        }
    }

    GCUSurface memHandle = NULL;
    GCUVirtualAddr virtAddr = NULL;
    GCUPhysicalAddr phyAddr = 0;
    GCU_FORMAT format;

    switch(bpp)
    {
    case 16:
        format = GCU_FORMAT_RGB565;
        break;

    case 32:
        format = GCU_FORMAT_ARGB8888;
        break;

    default:
        goto OnError;
    }

    memHandle = _gcuCreateBuffer(gcuContext, width, height, format, &virtAddr, &phyAddr);

    if(memHandle)
    {
        if(vir_addr)
        {
            *vir_addr = virtAddr;
        }
        if(phy_addr)
        {
            *phy_addr = phyAddr;
        }

        nAllocatedMemory++;
        return memHandle;
    }
    else
    {
        goto OnError;
    }

OnError:
    if(0 == nAllocatedMemory)
    {
        gcuDestroyContext(gcuContext);
        gcuContext = NULL;
        gcuTerminate();
    }

    return NULL;
}

void ceu_sample_free_continuous_mem(void* memoryHandle)
{
    if(NULL == gcuContext || NULL == memoryHandle)
    {
        return;
    }

    _gcuDestroyBuffer(gcuContext, memoryHandle);

    nAllocatedMemory--;

    if(0 == nAllocatedMemory)
    {
        gcuDestroyContext(gcuContext);
        gcuContext = NULL;
        gcuTerminate();
    }
}

#ifndef BI_RGB
#define BI_RGB        0L
#define BI_RLE8       1L
#define BI_RLE4       2L
#define BI_BITFIELDS  3L

typedef struct tagBITMAPINFOHEADER{
    unsigned int    biSize;
    int             biWidth;
    int             biHeight;
    short           biPlanes;
    short           biBitCount;
    unsigned int    biCompression;
    unsigned int    biSizeImage;
    int             biXPelsPerMeter;
    int             biYPelsPerMeter;
    unsigned int    biClrUsed;
    unsigned int    biClrImportant;
} __attribute((packed)) BITMAPINFOHEADER;

typedef struct tagBITMAPFILEHEADER {
    short           bfType;
    unsigned int    bfSize;
    short           bfReserved1;
    short           bfReserved2;
    unsigned int    bfOffBits;
} __attribute((packed)) BITMAPFILEHEADER;
#endif

int ceu_sample_dump_bmp_file(void* dumpBase, /*logic address*/
                 int dumpWidth,
                 int dumpHeight,
                 int dumpStride,
                 int dumpBpp,
                 const char* strFileName)
{

    int                 ret = 0;
    void*               pData = NULL;
    int                 bpp = 0;
    unsigned int        width = 0;
    unsigned int        height = 0;
    unsigned int        stride = 0;

    static int          index = 0;
    char                filename[64] = {0};

    BITMAPFILEHEADER    bmFileHeader;
    BITMAPINFOHEADER    bmInfoHeader;
    FILE*               pFile = NULL;

    unsigned int        dstStride = 0;
    unsigned int        dstSize   = 0;
    unsigned char*      pDstRaw = NULL;
    unsigned char*      pDst = NULL;
    unsigned char*      pSrc = NULL;
    unsigned int        i = 0;
    unsigned int        j = 0;


    bpp         = dumpBpp;
    width       = dumpWidth;
    height      = dumpHeight;

    // Bottom Left
    //pData       = (unsigned int)dumpBase + dumpStride * dumpHeight - dumpStride;
    //stride      = (unsigned int)dumpStride * -1;

    // Top Left
    pData       = dumpBase;
    stride      = dumpStride;

    index++;
    sprintf(filename, "/sdcard/dump/out%s_%03d.bmp", strFileName, index);
    pFile = fopen(filename, "wb");
    if(!pFile)
    {
        printf("cannot open dump file\n");
        return 0;
    }

    memset(&bmFileHeader, 0, sizeof(BITMAPFILEHEADER));
    bmFileHeader.bfType        = 0x4D42;
    bmFileHeader.bfSize        = sizeof(BITMAPFILEHEADER);
    bmFileHeader.bfReserved1   = 0;
    bmFileHeader.bfReserved2   = 0;
    bmFileHeader.bfOffBits     = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    memset(&bmInfoHeader, 0, sizeof(BITMAPINFOHEADER));
    bmInfoHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmInfoHeader.biWidth       = width;
    bmInfoHeader.biHeight      = height;
    bmInfoHeader.biPlanes      = 1;
    bmInfoHeader.biBitCount    = 24;
    bmInfoHeader.biCompression = BI_RGB;

    dstStride = width * 3; /* (width * 3 + 3) / 4 * 4; */
    dstSize = dstStride * height;
    pDstRaw = (unsigned char*)malloc(dstSize);
    if(!pDstRaw)
    {
        if(pFile)
        {
            fclose(pFile);
            pFile = NULL;
        }
    }

    switch(bpp)
    {
    case 16:
        {
            unsigned char* pSrcLine = (unsigned char*)pData;
            unsigned char* pDstLine = (unsigned char*)pDstRaw;
            for(i = 0; i < height; i++)
            {
                pSrc = pSrcLine;
                pDst = pDstLine;

                for(j = 0; j < width; j++)
                {
                    unsigned short rgb = *(unsigned short*)pSrc;

                    pDst[0] = ((rgb & 0x001f) << 3) | ((rgb & 0x001c) >> 2);       /*  B */
                    pDst[1] =  ((rgb & 0x07e0) >> 3) | ((rgb & 0x0600) >> 9);       /*  G */
                    pDst[2] = ((rgb & 0xf800) >> 8) | (rgb >> 13);                 /*  R */
#if 0
                    /* pDst[0] = ((rgb & 0x001f) << 3); */      /*  B */
                    pDst[1] =  ((rgb & 0x07e0) >> 3);       /*  G */
                    pDst[2] = ((rgb & 0xf800) >> 8);                 /*  R*/ */
#endif
                    pSrc += 2;
                    pDst += 3;
                }

                pSrcLine += stride;
                pDstLine += dstStride;
            }
        }
        break;

    case 24:
        {
            unsigned char* pSrcLine = (unsigned char*)pData;
            unsigned char* pDstLine = (unsigned char*)pDstRaw;
            for(i = 0; i < height; i++)
            {
                pSrc = pSrcLine;
                pDst = pDstLine;

                for(j = 0; j < width; j++)
                {
                    pDst[0] = pSrc [2]; // b
                    pDst[1] = pSrc [1]; // g
                    pDst[2] = pSrc [0]; // r

                    pSrc += 3;
                    pDst += 3;
                }

                pSrcLine += stride;
                pDstLine += dstStride;
            }
        }
        break;

    case 32:
        {
            unsigned char* pSrcLine = (unsigned char*)pData;
            unsigned char* pDstLine = (unsigned char*)pDstRaw;
            for(i = 0; i < height; i++)
            {
                pSrc = pSrcLine;
                pDst = pDstLine;

                for(j = 0; j < width; j++)
                {
                    pDst[0] = pSrc [2]; // b
                    pDst[1] = pSrc [1]; // g
                    pDst[2] = pSrc [0]; // r

                    pSrc += 4;
                    pDst += 3;
                }

                pSrcLine += stride;
                pDstLine += dstStride;
            }
        }
        break;

    default:
        break;
    }


    if(pFile)
    {
        fwrite(&bmFileHeader, 1, sizeof(BITMAPFILEHEADER), pFile);
        fwrite(&bmInfoHeader, 1, sizeof(BITMAPINFOHEADER), pFile);
        fwrite(pDstRaw, 1, dstSize, pFile);

        fclose(pFile);
        pFile = NULL;

        ret = 1;

    }

    if(pDstRaw)
    {
        free(pDstRaw);
        pDstRaw = NULL;
    }

    return ret;
}

bool _SW_BGR8882UYVY(unsigned char* pBGR888, unsigned char* pOutput, unsigned int width, unsigned int height)
{
    unsigned char *pInput = pBGR888;

    for(unsigned int j = 0; j < height; j++)
        for(unsigned int i = 0; i < width; i += 2)
        {
            int32_t Y,U,V,R,G,B;
            int32_t Y1, U1, V1, R1, G1, B1;
            B = *pInput++;
            G = *pInput++;
            R = *pInput++;

            B1 = *pInput++;
            G1 = *pInput++;
            R1 = *pInput++;

            Y = (int32_t)(0.299f * R + 0.587f * G + 0.114f * B);
            U = (int32_t)(-0.1687 * R -  0.3313 * G +  0.5 * B + 128);
            V = (int32_t)(0.5 * R -  0.4187 * G - 0.0813 * B + 128);

            Y1 = (int32_t)(0.299f * R1 + 0.587f * G1 + 0.114f * B1);
            U1 = (int32_t)(-0.1687 * R1 -  0.3313 * G1 +  0.5 * B1 + 128);
            V1 = (int32_t)(0.5 * R1 -  0.4187 * G1 - 0.0813 * B1 + 128);

            U = (U + U1)/2;
            V = (V + V1)/2;

            Y= Y > 255 ? 255 : (Y < 0 ? 0 : Y);
            U= U > 255 ? 255 : (U < 0 ? 0 : U);
            V= V > 255 ? 255 : (V < 0 ? 0 : V);
            Y1= Y1 > 255 ? 255 : (Y1 < 0 ? 0 : Y1);

            *pOutput++ = U;
            *pOutput++ = Y;
            *pOutput++ = V;
            *pOutput++ = Y1;
        }

    return true;
}

bool ceu_sample_load_bmp_as_uyvy(void ** data, const char* filePathName, unsigned int* imgWidth, unsigned int* imgHeight)
{
    BITMAPFILEHEADER    bmFileHeader;
    BITMAPINFOHEADER    bmInfoHeader;
    FILE*               pFile = NULL;
    unsigned char       *pImageData = NULL;

    pFile = fopen(filePathName, "rb");
    if(!pFile)
    {
        fprintf(stderr, "Can not open file %s\n", filePathName);
        return false;
    }

    fread(&bmFileHeader, sizeof(BITMAPFILEHEADER), 1, pFile);
    fread(&bmInfoHeader, sizeof(BITMAPINFOHEADER), 1, pFile);

    if(bmInfoHeader.biBitCount != 24)
    {
        fprintf(stderr, "Only 24bit color depth bitmap supported!\n");
        return false;
    }

    pImageData = (unsigned char*)malloc(bmInfoHeader.biSizeImage);
    if(pImageData == NULL)
    {
        fprintf(stderr, "%s(%d): Allocate memory error!\n", __FILE__, __LINE__);
        return false;
    }
    fread(pImageData, 1, bmInfoHeader.biSizeImage, pFile);  //here data is BGR
    fclose(pFile);

    *data = (unsigned char*) memalign(64, bmInfoHeader.biWidth * bmInfoHeader.biHeight * 2);

    _SW_BGR8882UYVY(pImageData, (unsigned char*)*data, bmInfoHeader.biWidth, bmInfoHeader.biHeight);

    *imgWidth = bmInfoHeader.biWidth;
    *imgHeight = bmInfoHeader.biHeight;

    free((void*)pImageData);
    return true;
}

bool ceu_sample_load_bmp_as_rgb565(
    void ** data,
    const char* filePathName,
    unsigned int* imgWidth,
    unsigned int* imgHeight)
{
    BITMAPFILEHEADER    bmFileHeader;
    BITMAPINFOHEADER    bmInfoHeader;
    FILE*               pFile = NULL;
    unsigned char       *pImageData = NULL;

    pFile = fopen(filePathName, "rb");
    if(!pFile)
    {
        fprintf(stderr, "Can not open file %s\n", filePathName);
        return false;
    }

    fread(&bmFileHeader, sizeof(BITMAPFILEHEADER), 1, pFile);
    fread(&bmInfoHeader, sizeof(BITMAPINFOHEADER), 1, pFile);

    if(bmInfoHeader.biBitCount != 24)
    {
        fprintf(stderr, "Only 24bit color depth bitmap supported!\n");
        return false;
    }

    pImageData = (unsigned char*)malloc(bmInfoHeader.biSizeImage);
    if(pImageData == NULL)
    {
        fprintf(stderr, "%s(%d): Allocate memory error!\n", __FILE__, __LINE__);
        return false;
    }
    fread(pImageData, 1, bmInfoHeader.biSizeImage, pFile);  //here data is BGR
    fclose(pFile);

    *data = (unsigned char*) malloc(bmInfoHeader.biWidth * bmInfoHeader.biHeight * 2);
    /*Transform BGR data to RGB565*/
    unsigned short* pOutData = (unsigned short*)*data;
    unsigned char* pInData = pImageData;
    for(int j = 0; j < bmInfoHeader.biHeight; j++)
        for(int i = 0; i < bmInfoHeader.biWidth; i++)
        {
            unsigned char R5, G6, B5;
            R5 = (unsigned char)(*(pInData + 2)/255.0*31);
            G6 = (unsigned char)(*(pInData + 1)/255.0*63);
            B5 = (unsigned char)(*(pInData)/255.0*31);

            *pOutData = (unsigned short)0;
            *pOutData |= (R5 & 0x001f) << 11;
            *pOutData |= (G6 & 0x003f) << 5;
            *pOutData |= (B5 & 0x001f);
            pOutData++;
            pInData += 3;
        }

    *imgWidth = bmInfoHeader.biWidth;
    *imgHeight = bmInfoHeader.biHeight;

    free((void*)pImageData);
    return true;
}

bool ceu_sample_dump_yuv_file(
    void* pData,
    unsigned int nTotalBytes,
    const char* strFileName)
{
    char filename[64] = {0};
    FILE* pOutputFile = NULL;

    sprintf(filename, "/sdcard/dump/%s.YUV", strFileName);
    pOutputFile = fopen(filename, "wb");
    if(!pOutputFile)
    {
        fprintf(stderr, "Can't Open %s for write!\n", filename);
        return false;
    }
    fwrite(pData, 1, nTotalBytes, pOutputFile);
    fflush(pOutputFile);
    fclose(pOutputFile);
    return true;
}



