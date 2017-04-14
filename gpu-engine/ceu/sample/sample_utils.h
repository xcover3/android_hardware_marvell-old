#ifndef _CEU_SAMPLE_UTILS_H_
#define _CEU_SAMPLE_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

void* ceu_sample_alloc_continuous_mem(
    unsigned int width,
    unsigned int height,
    unsigned int bpp,
    void** vir_addr,
    unsigned int * phy_addr);

void ceu_sample_free_continuous_mem(
    void* memoryHandle);

int ceu_sample_dump_bmp_file(void* dumpBase, /*logic address*/
                 int dumpWidth,
                 int dumpHeight,
                 int dumpStride,
                 int dumpBpp,
                 const char* strFileName);

bool ceu_sample_load_bmp_as_uyvy(
    void ** data,
    const char* filePathName,
    unsigned int* imgWidth,
    unsigned int* imgHeight);

bool ceu_sample_load_bmp_as_rgb565(
    void ** data,
    const char* filePathName,
    unsigned int* imgWidth,
    unsigned int* imgHeight);

bool ceu_sample_dump_yuv_file(
    void* pData,
    unsigned int nTotalBytes,
    const char* strFileName);


#ifdef __cplusplus
}
#endif

#endif

