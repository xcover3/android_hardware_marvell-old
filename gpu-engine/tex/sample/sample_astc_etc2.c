#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "gpu_tex.h"

#define GPU_TEX_ASTC_HEADER_LEN 16

int main(int argc, char **argv)
{
    /*preparation for astc decoder*/
    int init_result;
    init_result = gpu_tex_initialize();
    if(!init_result)
        return GPU_TEX_FALSE;

    if (argc < 4)
    {
    printf("\n./astc_etc2 xxxx.astc xxxx.tga width height\ntoo few arguments\n\n");
    return GPU_TEX_FALSE;
    }

    const char *input_filename = argv[1];
    const char *output_filename = argv[2];
    int width = atoi(argv[3]);
    int height = atoi(argv[4]);
    int stride = width*4;

    struct timeval tpstart, tpend;
    long timeuse;
    FILE *f = fopen(input_filename, "rb");
    if (!f)
        return GPU_TEX_FALSE;

    fseek(f, 0, SEEK_END);
    int buffersize = ftell(f) - GPU_TEX_ASTC_HEADER_LEN;
    uint8_t* input_buffer = (uint8_t*)malloc(buffersize);
    fseek(f, GPU_TEX_ASTC_HEADER_LEN, SEEK_SET);
    fread(input_buffer, 1, buffersize, f);
    GPU_TEX_FORMAT input_format = GPU_TEX_FORMAT_RGBA_8x8_ASTC;
    GPU_TEX_ASTC_MODE decode_mode = GPU_TEX_DECODE_LDR_SRGB;
    uint8_t* astc_decode_buffer = (uint8_t*)malloc(width*height*4);
    /*codec begins*/
    /*rgba*/
    gettimeofday(&tpstart, NULL);
    long start = (long)tpstart.tv_sec*1000000 + tpstart.tv_usec;
    gpu_tex_astc_decoder((void *)input_buffer, input_format, decode_mode, width, height, stride, GPU_TEX_FORMAT_RGBA, (void *)astc_decode_buffer);
    gettimeofday(&tpend, NULL);
    long end = (long)tpend.tv_sec*1000000 + tpend.tv_usec;
    timeuse = (end - start)/1000.0;
    printf("\nASTC Decode Elapsed time:\n     %ld ms\n", timeuse);
    printf("decode\n");
    gpu_tex_save_tga((void*)astc_decode_buffer, GPU_TEX_FORMAT_RGBA, width, height, stride, "rgba_astc.tga");
    uint8_t* etc2_encode_buffer = (uint8_t*)malloc(width*height);
    gettimeofday(&tpstart, NULL);
    start = (long)tpstart.tv_sec*1000000 + tpstart.tv_usec;
    gpu_tex_etc2_encoder_fast(astc_decode_buffer, GPU_TEX_FORMAT_RGBA, width, height, stride, GPU_TEX_FORMAT_RGB8_PUNCHTHROUGH_ALPHA1_ETC2, etc2_encode_buffer);
    gettimeofday(&tpend, NULL);
    end = (long)tpend.tv_sec*1000000 + tpend.tv_usec;
    timeuse = (end - start)/1000.0;
    printf("\nETC2 Encode Elapsed time:\n     %ld ms\n", timeuse);
    uint8_t* etc2_decode_buffer = (uint8_t*)malloc(width*height*4);
    gpu_tex_etc2_decoder(etc2_encode_buffer, GPU_TEX_FORMAT_RGB8_PUNCHTHROUGH_ALPHA1_ETC2, width, height, stride, GPU_TEX_FORMAT_RGBA, etc2_decode_buffer);
    gpu_tex_save_tga((void*)etc2_decode_buffer, GPU_TEX_FORMAT_RGBA, width, height, stride, output_filename);
    /*rgb*/
    printf("\n*******\nrgb:\n");
    stride = 3*width;
    gettimeofday(&tpstart, NULL);
    start = (long)tpstart.tv_sec*1000000 + tpstart.tv_usec;
    gpu_tex_astc_decoder((void *)input_buffer, input_format, decode_mode, width, height, stride, GPU_TEX_FORMAT_RGB, (void *)astc_decode_buffer);
    gettimeofday(&tpend, NULL);
    end = (long)tpend.tv_sec*1000000 + tpend.tv_usec;
    timeuse = (end - start)/1000.0;
    printf("\nASTC Decode Elapsed time:\n     %ld ms\n", timeuse);
    printf("decode\n");
    gpu_tex_save_tga((void*)astc_decode_buffer, GPU_TEX_FORMAT_RGB, width, height, stride, "rgb_astc.tga");
    gettimeofday(&tpstart, NULL);
    start = (long)tpstart.tv_sec*1000000 + tpstart.tv_usec;
    gpu_tex_etc2_encoder_fast(astc_decode_buffer, GPU_TEX_FORMAT_RGB, width, height, stride, GPU_TEX_FORMAT_RGB8_ETC2, etc2_encode_buffer);
    gettimeofday(&tpend, NULL);
    end = (long)tpend.tv_sec*1000000 + tpend.tv_usec;
    timeuse = (end - start)/1000.0;
    printf("\nETC2 Encode Elapsed time:\n     %ld ms\n", timeuse);
    gpu_tex_etc2_decoder(etc2_encode_buffer, GPU_TEX_FORMAT_RGB8_ETC2, width, height, stride, GPU_TEX_FORMAT_RGB, etc2_decode_buffer);
    gpu_tex_save_tga((void*)etc2_decode_buffer, GPU_TEX_FORMAT_RGB, width, height, stride, "rgb_etc2.tga");
    printf("save\n");
    free(input_buffer);
    free(astc_decode_buffer);
    free(etc2_encode_buffer);
    free(etc2_decode_buffer);
    fclose(f);
    /*tile linear*/
    unsigned startx = 3, starty = 3, endx = 257, endy = 257;
    unsigned w = (endx-startx);
    unsigned h = (endy-starty);
    int srcStride = 4*(endx-startx), tarStride = 4*(endx-startx);
    int num = endx*endy;

    char* source = (char*)malloc(sizeof(char)*4*num);
    int i,k;
    for(i = 0; i < num; i++)
        for(k = 0; k < 4; k++)
            {
            source[i*4+k] = rand()%256;
            }
    char* dest = (char*)malloc(sizeof(char)*4*num*10);
    FILE* f1 = fopen("input.txt","wb");
    FILE* f2 = fopen("output.txt","wb");
    gpu_tex_linear2tile_ABGR2ARGB(dest, tarStride, (const char*)source, srcStride, startx, starty, w, h);
    fwrite(source,1,sizeof(char)*4*num,f1);
    fwrite(dest,1,sizeof(char)*4*num,f2);
    fclose(f1);
    fclose(f2);
    free(source);
    free(dest);

    return GPU_TEX_TRUE;
}



