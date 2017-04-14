/***************************************************************************************** 
Copyright (c) 2009, Marvell International Ltd. 
All Rights Reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Marvell nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY MARVELL ''AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL MARVELL BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************************/

#include "codecVC.h"
#include "misc.h"
#include "ippLV.h"

//#define IPP_Fscanf fscanf

/*vmeta os api*/
#include "vdec_os_api.h"

//temporary
#define STREAM_BUF_SIZE                 (2047 * 1024) /*must equal to or greater than 64k*/
#define STREAM_BUF_NUM                  4
#define PICTURE_BUF_NUM                 20
#define LESSINFO_MAX_FRAME              10

static int g_bOptChksum = 1;
int bFreePicBufFlag[PICTURE_BUF_NUM]; /* declaring as global variable is for manchac requirement*/
//#define IPP_Printf

typedef struct _IppVmetaDecParSetEx {
    Ipp32u              pp_hscale;                  /*Horizontal down-scaling ratio (JPEG only), 0 or 1: no scaling, 2: 1/2, 4: 1/4, */
    Ipp32u              pp_vscale;                  /*Vertical down-scaling ratio (JPEG only), 0 or 1: no scaling, 2: 1/2, 4: 1/4, 8: 1/8*/
    IppiRect            roi;                        /*roi region*/
    IppVmetaFastMode    eFastMode;                  /*fast mode*/
    Ipp32s              bNoOutputDelay;             /*disable/enable no output delay*/
    Ipp32s              bDrmEnable;                 /*enable/disable drm decoding*/
    IppVmetaInputMode   eInputMode;                 /*input mode*/
    Ipp32s              bDeblocking;                /*deblocking*/
    Ipp32s              bDisableFewerDpb;           /*disable fewer dpb mode*/
    Ipp32s              bLessInfo;                  /*only keep log info for begin 10 frames and non-each frame log*/
    Ipp32s              bNoResoChange;              /*disable resolution change for h264*/
}IppVmetaDecParSetEx;

/*
***************************************************************************************
*this function output the checksum value which is used for test.Customer need not use it.
* input:
* pPic:the picture structure whose info will be logged.
* checksum_file:the file used to log checksum value
*
*No output
***************************************************************************************/
void Output_checksum(IppVmetaPicture *pPic, IPP_FILE *f)
{
     if (1 == pPic->PicDataInfo.pic_type) {
        /*top field*/
        IPP_Fprintf(f, "CHKSUM>> [T] %08x-%08x-%08x-%08x-%08x-%08x-%08x-%08x\n",
                pPic->PicDataInfo.chksum_data[0][0],
                pPic->PicDataInfo.chksum_data[0][1],
                pPic->PicDataInfo.chksum_data[0][2],
                pPic->PicDataInfo.chksum_data[0][3],
                pPic->PicDataInfo.chksum_data[0][4],
                pPic->PicDataInfo.chksum_data[0][5],
                pPic->PicDataInfo.chksum_data[0][6],
                pPic->PicDataInfo.chksum_data[0][7]);

        /*bottom field*/
        IPP_Fprintf(f, "CHKSUM>> [B] %08x-%08x-%08x-%08x-%08x-%08x-%08x-%08x\n",
                pPic->PicDataInfo.chksum_data[1][0],
                pPic->PicDataInfo.chksum_data[1][1],
                pPic->PicDataInfo.chksum_data[1][2],
                pPic->PicDataInfo.chksum_data[1][3],
                pPic->PicDataInfo.chksum_data[1][4],
                pPic->PicDataInfo.chksum_data[1][5],
                pPic->PicDataInfo.chksum_data[1][6],
                pPic->PicDataInfo.chksum_data[1][7]);
    } else {
        /*non-interlaced frame, or interlaced frame, or non-paired field*/
        IPP_Fprintf(f, "CHKSUM>> [F] %08x-%08x-%08x-%08x-%08x-%08x-%08x-%08x\n",
                pPic->PicDataInfo.chksum_data[0][0],
                pPic->PicDataInfo.chksum_data[0][1],
                pPic->PicDataInfo.chksum_data[0][2],
                pPic->PicDataInfo.chksum_data[0][3],
                pPic->PicDataInfo.chksum_data[0][4],
                pPic->PicDataInfo.chksum_data[0][5],
                pPic->PicDataInfo.chksum_data[0][6],
                pPic->PicDataInfo.chksum_data[0][7]);
    }
}

void OutputPicture_Video(IppVmetaPicture *pPic, IPP_FILE *f, IppVmetaDecInfo *pDecInfo) 
{
    Ipp8u *p, *pStart;
    int i, stride;
    //int offset;
    int height, width;
    int top, left;
    int bROI = 1;

    if (NULL == f){
        return;
    }

    if (g_bOptChksum) {
        Output_checksum(pPic, f);
        return;
    }

    if (bROI) {
        if (pPic->pic.picFormat == IPP_YCbCr422I) {
            height  = pPic->pic.picROI.height;
            width   = pPic->pic.picROI.width;
            top     = pPic->pic.picROI.y;
            left    = pPic->pic.picROI.x;
            stride  = pPic->pic.picPlaneStep[0];
            //IPP_Printf("height %d width %d top %d left %d stride %d\n", height, width, top , left, stride);

            pStart = (Ipp8u*)pPic->pic.ppPicPlane[0];
            /*UYVY, if left odd number, the left Y should be dismissed and the right Y should be remained*/
            /*it is difficult to handle, so just shift to the left by one pixel*/
            if (left % 2) {
                left--;
            }
            if (width % 2) {
                width++;
            }
            p = pStart + top * stride + left * 2;
            for (i = 0; i < height; i++) {
                IPP_Fwrite(p, 2, width, f);
                p += stride;
            }
        } else if (pPic->pic.picFormat == IPP_YCbCr420P) {
            height  = pPic->pic.picROI.height;
            width   = pPic->pic.picROI.width;
            top     = pPic->pic.picROI.y;
            left    = pPic->pic.picROI.x;

            left &= ~1;
            top  &= ~1;
            width  = (width  +1) & ~1;
            height = (height +1) & ~1;
            stride  = pPic->pic.picPlaneStep[0];

            p = (Ipp8u*)pPic->pic.ppPicPlane[0] + top * stride + left;
            for (i = 0; i < height; i++) {
                IPP_Fwrite(p, 1, width, f);
                p += stride;
            }

            left   >>= 1;
            top    >>= 1;
            width  >>= 1;
            height >>= 1;
            stride   = pPic->pic.picPlaneStep[1];

            p = (Ipp8u*)pPic->pic.ppPicPlane[1] + top * stride + left;
            for (i = 0; i < height; i++) {
                IPP_Fwrite(p, 1, width, f);
                p += stride;
            }
            stride   = pPic->pic.picPlaneStep[2];
            p = (Ipp8u*)pPic->pic.ppPicPlane[2] + top * stride + left;
            for (i = 0; i < height; i++) {
                IPP_Fwrite(p, 1, width, f);
                p += stride;
            }
        }
    } else {
        //IPP_Printf("datalen = %d\n", pPic->nDataLen);
        //p = (Ipp8u*)pPic->pic.ppPicPlane[0];
        //IPP_Fwrite(p, 1, pPic->nDataLen, f);
        /* */
        height  = pPic->pic.picHeight;
        width   = pPic->pic.picWidth;
        top     = 0;
        left    = 0;
        stride  = pPic->pic.picPlaneStep[0];
        //IPP_Printf("height %d width %d top %d left %d stride %d\n", height, width, top , left, stride);
        if (width % 2) {
            width++;
        }

        p = (Ipp8u*)pPic->pic.ppPicPlane[0];
        for (i = 0; i < height; i++) {
            IPP_Fwrite(p, 2, width, f);
            p += stride;
        }
       
    }
}



int descramble_fake(Ipp32u nSeed0, Ipp32u nSeed1, Ipp8u *pSrc, Ipp8u *pDst, int nSize, int bResume)
{
    IPP_Memcpy(pDst, pSrc, nSize);
    return 0;
}

int scramble_fake(IppVmetaBitstream *pBitStream)
{
    return 0;
}

int VmetaDecoder(IPP_FILE *fpin, IPP_FILE *fpout, char *log_file_name, IPP_FILE *fplen, IppVmetaDecParSet *pDecParSet, IppVmetaDecParSetEx *pDecParSetEx)
{
    void *pDecoderState;
    MiscGeneralCallbackTable CBTable;
    IppVmetaExtCbTbl ExtCbTbl;
    IppCodecStatus rtCode;
    IppVmetaBitstream BitStreamGroup[STREAM_BUF_NUM];
    IppVmetaDrmUsrData DrmUsrData[STREAM_BUF_NUM];
    int i;
    IppVmetaBitstream *pBitStream;
    IppVmetaPicture PictureGroup[PICTURE_BUF_NUM];
    IppVmetaDecInfo VideoDecInfo;
    IppVmetaPicture *pPicture;
    IppVmetaDecInfo *pDecInfo;
    int nReadNum;
    int nCurStrmBufIdx;
    int nCurPicBufIdx;
    int bFreeStrmBufFlag[STREAM_BUF_NUM];

    int bStop;
    int nTotalFrames, nSkipFrames;
    Ipp32u nTotalTime;
    int nFreeStrmBufNum, nFreePicBufNum;
    int nDecTime, nPushPicTime, nPushStrmTime, nPopStrmTime, nPopPicTime, nCmdTime;

    IppVmetaEnableDrmPar DrmPar;


    int                         bInitLCD;
    int                         codec_perf_index;
    int                         total_perf_index;
    int                         thread_perf_index;

    int                         perf_push_strm_index;
    int                         perf_pop_strm_index;
    int                         perf_push_pic_index;
    int                         perf_pop_pic_index;
    int                         perf_cmd_index;

    DISPLAY_CB                  DispCB;
    DISPLAY_CB                  *hDispCB = &DispCB;

    int rtFlag = IPP_OK;
    int bIntWaitEventDisable;

    int ret;
    IPP_FILE *flen = NULL;
    int nFrameLen;
    int nLeftBytes;

    int bReadSeqLayer;
    Ipp8u pReadBuf[10];
    vc1m_seq_header Vc1mSeqHdr;
    Ipp32u nVal;
    Ipp32u nCumulTime, nOldCumulTime;
    Ipp32u nMaxFrameTime, nMinFrameTime, nFrameTime;
    Ipp32u nFrameCount;
    Ipp32u *pFrameTimeArray;
    int nMaxFrameCount;
    Ipp32u nVmetaRTLVer;
    IppEnableSwMpeg4DecPar EnableSwMpeg4DecPar;
    IppVmetaInputMode eSwMpeg4InputMode;

    pDecoderState   = NULL;
    pFrameTimeArray = NULL;
    for (i = 0; i < STREAM_BUF_NUM; i++) {
        BitStreamGroup[i].pBuf = NULL;
    }
    for (i = 0; i < PICTURE_BUF_NUM; i++) {
        PictureGroup[i].pBuf   = NULL;
    }

    IPP_Printf("start decoding**********************************\n");

    IPP_Printf("no-reording: %d\n", pDecParSet->no_reordering);
    IPP_Printf("stream format: %d\n", pDecParSet->strm_fmt);
    IPP_Printf("input buffer number: %d\n", STREAM_BUF_NUM);
    IPP_Printf("input buffer size: %d\n", STREAM_BUF_SIZE);
    IPP_Printf("output buffer number: %d\n", PICTURE_BUF_NUM);
    if (NULL != fplen) {
        IPP_Printf("entire frame mode!\n");
    } else {
        if (IPP_VMETA_INPUT_MODE_PARTIAL_END_OF_FRAME == pDecParSetEx->eInputMode) {
            IPP_Printf("error:to set frame-in mode, but no length file!\n");
            IPP_Log(log_file_name, "a", "error:to set frame-in mode, but no length file\n");
            return IPP_FAIL;
        }
        IPP_Printf("stream in mode!\n");
    }
    IPP_Printf("before driver init\n");
    ret = vdec_os_driver_init();
    if (0 > ret) {
        IPP_Printf("error: driver init fail! ret = %d\n", ret);
        IPP_Log(log_file_name, "a", "error: driver init fail!\n");
        goto fail_cleanup;
    }
    IPP_Printf("after driver init\n");

    IPP_GetPerfCounter(&codec_perf_index, DEFAULT_TIMINGFUNC_START, DEFAULT_TIMINGFUNC_STOP);
    IPP_ResetPerfCounter(codec_perf_index);


    IPP_GetPerfCounter(&perf_push_strm_index, DEFAULT_TIMINGFUNC_START, DEFAULT_TIMINGFUNC_STOP);
    IPP_ResetPerfCounter(perf_push_strm_index);

    IPP_GetPerfCounter(&perf_push_pic_index, DEFAULT_TIMINGFUNC_START, DEFAULT_TIMINGFUNC_STOP);
    IPP_ResetPerfCounter(perf_push_pic_index);


    IPP_GetPerfCounter(&perf_pop_strm_index, DEFAULT_TIMINGFUNC_START, DEFAULT_TIMINGFUNC_STOP);
    IPP_ResetPerfCounter(perf_pop_strm_index);

    IPP_GetPerfCounter(&perf_pop_pic_index, DEFAULT_TIMINGFUNC_START, DEFAULT_TIMINGFUNC_STOP);
    IPP_ResetPerfCounter(perf_pop_pic_index);

    IPP_GetPerfCounter(&perf_cmd_index, DEFAULT_TIMINGFUNC_START, DEFAULT_TIMINGFUNC_STOP);
    IPP_ResetPerfCounter(perf_cmd_index);

    
    IPP_GetPerfCounter(&total_perf_index, IPP_TimeGetTickCount, IPP_TimeGetTickCount);
    IPP_ResetPerfCounter(total_perf_index);
    IPP_GetPerfCounter(&thread_perf_index, IPP_TimeGetThreadTime, IPP_TimeGetThreadTime);
    IPP_ResetPerfCounter(thread_perf_index);
    CBTable.fMemMalloc  = IPP_MemMalloc;
    CBTable.fMemFree    = IPP_MemFree;
    /*to enable sw mpeg4 decoder for mpeg4 dp streams, one more callback function CBTable.fMemCalloc is needed*/
    CBTable.fMemCalloc  = IPP_MemCalloc;

    for (i = 0; i < STREAM_BUF_NUM; i++) {
        BitStreamGroup[i].pBuf                      
            = (Ipp8u*)vdec_os_api_dma_alloc(STREAM_BUF_SIZE, VMETA_STRM_BUF_ALIGN, &(BitStreamGroup[i].nPhyAddr));
        if (NULL == BitStreamGroup[i].pBuf) {
            IPP_Log(log_file_name, "a", "error: no memory!\n");
            IPP_Printf("error: no memory!\n");
            goto fail_cleanup;
        }
        IPP_Printf("allocate stream buffer %d, paddr = %p, vaddr = %p\n",i, BitStreamGroup[i].nPhyAddr, BitStreamGroup[i].pBuf);
        BitStreamGroup[i].nBufSize                  = STREAM_BUF_SIZE;
        BitStreamGroup[i].nDataLen                  = 0;
        BitStreamGroup[i].nOffset                   = 0;
        BitStreamGroup[i].nBufProp                  = 0;
        BitStreamGroup[i].pUsrData0                 = (void*)i;
        BitStreamGroup[i].pUsrData1                 = NULL;
        BitStreamGroup[i].pUsrData2                 = NULL;
        if (pDecParSetEx->bDrmEnable) {
            DrmUsrData[i].nSeed0                    = 0;
            DrmUsrData[i].nSeed1                    = 0;
            BitStreamGroup[i].pUsrData3             = &(DrmUsrData[i]);
        } else {
            BitStreamGroup[i].pUsrData3             = NULL;
        }
        BitStreamGroup[i].nFlag                     = 0;
        bFreeStrmBufFlag[i]                         = 1;
    }

    for (i = 0; i < PICTURE_BUF_NUM; i++) {
        PictureGroup[i].pBuf                = NULL;
        PictureGroup[i].nPhyAddr            = 0;
        PictureGroup[i].nBufSize            = 0;
        PictureGroup[i].nDataLen            = 0;
        PictureGroup[i].nOffset             = 0;
        PictureGroup[i].nBufProp            = 0;
        PictureGroup[i].pUsrData0           = (void*)i;
        PictureGroup[i].pUsrData1           = NULL;
        PictureGroup[i].pUsrData2           = NULL;
        PictureGroup[i].pUsrData3           = NULL;
        PictureGroup[i].nFlag               = 0;
        IPP_Memset((void*)&(PictureGroup[i].pic), 0, sizeof(IppPicture));
        bFreePicBufFlag[i]                  = 1;
    }

    nMaxFrameCount      = 10000;
    IPP_MemMalloc((void**)(&pFrameTimeArray), 4 * nMaxFrameCount, 4);
    if (NULL == pFrameTimeArray) {
        IPP_Printf("error: no memory\n");
        IPP_Log(log_file_name, "a", "error: no memory\n");
        goto fail_cleanup;
    }

    IPP_Printf("before DecoderInitAlloc_Vmeta!\n");
    rtCode = DecoderInitAlloc_Vmeta(pDecParSet, &CBTable, &pDecoderState);
    IPP_Printf("after DecoderInitAlloc_Vmeta!\n");
    if (IPP_STATUS_NOERR != rtCode) {
        IPP_Printf("error: decoder init fail, error code %d!\n", rtCode);
        IPP_Log(log_file_name, "a", "error: decoder init fail, error code %d!\n", rtCode);
        goto fail_cleanup;
    }

    IPP_Printf("set fast mode %d\n", pDecParSetEx->eFastMode);
    rtCode = DecodeSendCmd_Vmeta(IPPVC_SET_FASTMODE, &(pDecParSetEx->eFastMode), NULL, pDecoderState);

    IPP_Printf("set no output delay %d\n", pDecParSetEx->bNoOutputDelay);
    rtCode = DecodeSendCmd_Vmeta(IPPVC_SET_NO_OUTPUT_DELAY, &(pDecParSetEx->bNoOutputDelay), NULL, pDecoderState);
    if ((!pDecParSet->no_reordering) && pDecParSetEx->bNoOutputDelay) {
        IPP_Printf("reordering delay still exist\n");
    }

    rtCode = DecodeSendCmd_Vmeta(IPPVC_GET_HW_VERSION, NULL, &nVmetaRTLVer, pDecoderState);
    if (IPP_STATUS_NOERR != rtCode) {
        IPP_Printf("fail to get vmeta version, rtCode=%d\n", rtCode);
    } else {
        IPP_Printf("Vmeta RTL version is %p\n", nVmetaRTLVer);
    }

    /* disable sw mpeg4dec, vmeta hal support dp since 20111230 */
#if 0
    if (fplen) {
        eSwMpeg4InputMode  = IPP_VMETA_INPUT_MODE_PARTIAL_END_OF_FRAME;
    } else {
        eSwMpeg4InputMode  = IPP_VMETA_INPUT_MODE_STREAM_IN;
    }

    EnableSwMpeg4DecPar.eInputMode                                      = eSwMpeg4InputMode;
    EnableSwMpeg4DecPar.SwMpeg4DecCbTbl.DecodeSendCmd_MPEG4Video        = DecodeSendCmd_MPEG4Video;
    EnableSwMpeg4DecPar.SwMpeg4DecCbTbl.DecoderInitAlloc_MPEG4Video     = DecoderInitAlloc_MPEG4Video;
    EnableSwMpeg4DecPar.SwMpeg4DecCbTbl.Decode_MPEG4Video               = Decode_MPEG4Video;
    EnableSwMpeg4DecPar.SwMpeg4DecCbTbl.DecoderFree_MPEG4Video          = DecoderFree_MPEG4Video;
    rtCode = DecodeSendCmd_Vmeta(IPPVC_ENABLE_SW_MPEG4DEC, &EnableSwMpeg4DecPar, NULL, pDecoderState);
    if (IPP_STATUS_NOERR != rtCode) {
        IPP_Printf("fail to enable sw mpeg4 decoder, rtCode=%d\n", rtCode);
    } else {
        IPP_Printf("enable sw mpeg4 decoder for dp stream\n");
    }
#endif

    if (IPP_VMETA_INPUT_MODE_UNAVAIL != pDecParSetEx->eInputMode) {
        rtCode = DecodeSendCmd_Vmeta(IPPVC_SET_INPUT_MODE, &(pDecParSetEx->eInputMode), NULL, pDecoderState);
        IPP_Printf("set input mode %d, rtCode=%d\n", pDecParSetEx->eInputMode, rtCode);
    }

    if (pDecParSetEx->bDrmEnable) {
        if (!fplen) {
            IPP_Printf("Error: to enable drm, must be frame-in mode\n");
            goto fail_cleanup;
        }
        IPP_Memset(&ExtCbTbl, 0, sizeof(IppVmetaExtCbTbl));
        ExtCbTbl.fThreadCreateEx        = IPP_ThreadCreateEx;
        ExtCbTbl.fThreadDestroyEx       = IPP_ThreadDestroyEx;
        ExtCbTbl.fThreadGetAttributeEx  = IPP_ThreadGetAttribute;
        ExtCbTbl.fThreadSetAttributeEx  = IPP_ThreadSetAttribute;

        ExtCbTbl.fMutexCreate           = IPP_MutexCreate;
        ExtCbTbl.fMutexDestroy          = IPP_MutexDestroy;
        ExtCbTbl.fMutexLock             = IPP_MutexLock;
        ExtCbTbl.fMutexUnlock           = IPP_MutexUnlock;
        ExtCbTbl.fEventCreate           = IPP_EventCreate;
        ExtCbTbl.fEventDestroy          = IPP_EventDestroy;
        ExtCbTbl.fEventReset            = IPP_EventReset;
        ExtCbTbl.fEventSet              = IPP_EventSet;
        ExtCbTbl.fEventWait             = IPP_EventWait;
        ExtCbTbl.fSleep                 = IPP_Sleep;
        rtCode = DecodeSendCmd_Vmeta(IPPVC_SET_EXTCALLBACKTABLE, &ExtCbTbl, NULL, pDecoderState);
        IPP_Printf("set extended callback table rtCode=%d\n", rtCode);

        DrmPar.fDescramble  = descramble_fake;
        rtCode = DecodeSendCmd_Vmeta(IPPVC_SET_DRM_MODE, &DrmPar, NULL, pDecoderState);
        if (IPP_STATUS_NOERR != rtCode) {
            IPP_Printf("fail to set drm decoding mode, rtCode=%d\n", rtCode);
            IPP_Log(log_file_name, "a", "fail to set drm decoding mode, rtCode=%d\n", rtCode);
            goto fail_cleanup;
        }
        IPP_Printf("enable drm decoding mode\n");
    }

    if (pDecParSetEx->bDeblocking) {
        /*currently, only valid for mpeg2, mpeg4, h263*/
        rtCode = DecodeSendCmd_Vmeta(IPPVC_ENABLE_DEBLOCKING, NULL, NULL, pDecoderState);
        IPP_Printf("enable deblocking for format %d, rtCode=%d\n", pDecParSet->strm_fmt, rtCode);
    }

    if (pDecParSetEx->bDisableFewerDpb) {
        rtCode = DecodeSendCmd_Vmeta(IPPVC_DISABLE_FEWER_DPB_MODE, NULL, NULL, pDecoderState);
        IPP_Printf("disable fewer dpb mode, rtCode=%d\n", rtCode);
    }

    if (pDecParSetEx->bNoResoChange) {
        rtCode = DecodeSendCmd_Vmeta(IPPVC_SET_NO_RESO_CHANGE, NULL, NULL, pDecoderState);
        IPP_Printf("set no resolution change for h264, rtCode=%d\n", rtCode);
    }

    bStop               = 0;
    pBitStream          = NULL;
    pPicture            = NULL;
    pDecInfo            = &VideoDecInfo;
    nCurStrmBufIdx      = 0;
    nCurPicBufIdx       = 0;
    nTotalFrames        = 0;
    nSkipFrames         = 0;
    nTotalTime          = 0;
    nFreeStrmBufNum     = STREAM_BUF_NUM;
    nFreePicBufNum      = PICTURE_BUF_NUM;
    bInitLCD            = 0;
    nLeftBytes          = 0;
    bReadSeqLayer       = 1;

    nCumulTime          = 0;
    nOldCumulTime       = 0;
    nMaxFrameTime       = 0;
    nMinFrameTime       = 0xffffffff;
    nFrameTime          = 0;
    nFrameCount         = 0;

    IPP_StartPerfCounter(total_perf_index);
    while(!bStop) {
        IPP_StartPerfCounter(codec_perf_index);
        IPP_StartPerfCounter(thread_perf_index);
        rtCode = DecodeFrame_Vmeta(pDecInfo, pDecoderState);
        IPP_StopPerfCounter(thread_perf_index);
        IPP_StopPerfCounter(codec_perf_index);

        if (IPP_STATUS_NEED_INPUT == rtCode) {
            if (0 >= nFreeStrmBufNum) {
                /*for multi-instance, input buffer may not be explicitly returned*/
                if( (0 == pDecParSetEx->bLessInfo) || nTotalFrames<=LESSINFO_MAX_FRAME) {
                    IPP_Printf("ID = %d nFreeStrmBufNum == 0, try to pop implicitly returned input buffers\n", pDecInfo->user_id);
                }
                while (1) {
                    IPP_StartPerfCounter(perf_pop_strm_index);
                    DecoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_STRM, (void**)&pBitStream, pDecoderState);
                    IPP_StopPerfCounter(perf_pop_strm_index);
                    if (NULL == pBitStream) {
                        break;
                    }
                    bFreeStrmBufFlag[(int)pBitStream->pUsrData0] = 1;
                    nFreeStrmBufNum++;
                    if( (0 == pDecParSetEx->bLessInfo) || nTotalFrames<=LESSINFO_MAX_FRAME) {
                        IPP_Printf("ID = %d pop strm buf finish. strm buf id: %d\n", pDecInfo->user_id, (int)pBitStream->pUsrData0);
                    }
                }
            }
            if( (0 == pDecParSetEx->bLessInfo) || nTotalFrames<=LESSINFO_MAX_FRAME) {
                IPP_Printf("ID = %d nFreeStrmBufNum = %d\n", pDecInfo->user_id, nFreeStrmBufNum);
            }
            nCurStrmBufIdx = 0;
            while ((0 == bFreeStrmBufFlag[nCurStrmBufIdx]) && (STREAM_BUF_NUM > nCurStrmBufIdx)) {
                nCurStrmBufIdx++;
            }
            if (STREAM_BUF_NUM <= nCurStrmBufIdx) {
                IPP_Printf("ID = %d error: strm buf is not enough!\n", pDecInfo->user_id);
                goto fail_cleanup;
            }

            pBitStream = &(BitStreamGroup[nCurStrmBufIdx]);
            bFreeStrmBufFlag[nCurStrmBufIdx] = 0;
            nFreeStrmBufNum--;
            pBitStream->nDataLen    = 0;
            pBitStream->nOffset     = 0;
            pBitStream->nFlag       = 0; /*default value means neither end of frame nor end of unit*/

            if (fplen || (IPP_VIDEO_STRM_FMT_VC1M == pDecParSet->strm_fmt)) {
                if (nLeftBytes) {
                    if (nLeftBytes > STREAM_BUF_SIZE) {
                        nFrameLen = STREAM_BUF_SIZE;
                    } else {
                        nFrameLen = nLeftBytes;
                    }
                } else {
                    if (IPP_VIDEO_STRM_FMT_VC1M == pDecParSet->strm_fmt) {
#if 0
                        if (bReadSeqLayer) {
                            /*parse sequence layer data*/
                            IPP_Memset(&Vc1mSeqHdr, 0, sizeof(vc1m_seq_header));
                            IPP_Fread(pReadBuf, 1, 4, fpin);
                            /*num_frames(not need)*/
                            //Vc1mSeqHdr.num_frames = (pReadBuf[2] << 16) | (pReadBuf[1] << 8) | (pReadBuf[0]);
                            IPP_Fread(pReadBuf, 1, 4, fpin);

                            nVal = (pReadBuf[3] << 24) | (pReadBuf[2] << 16) | (pReadBuf[1] << 8) | (pReadBuf[0]);
                            IPP_Fread(pReadBuf, 1, nVal, fpin);
                            if ((4 != nVal) && (5 != nVal)) {
                                IPP_Printf("ID = %d exthdr size is not equal to 4 or 5, size = %d\n", pDecInfo->user_id, nVal);
                                break;
                            }
                            /*exthdr(need)*/  
                            for (i = 0; i < nVal; i++) {
                                Vc1mSeqHdr.exthdr[i]    = pReadBuf[i];
                            }
                            Vc1mSeqHdr.exthdrsize = nVal;

                            IPP_Fread(pReadBuf, 1, 4, fpin);
                            /*vert_size(need)*/
                            Vc1mSeqHdr.vert_size    = (pReadBuf[3] << 24) | (pReadBuf[2] << 16) | (pReadBuf[1] << 8) | (pReadBuf[0]);
                            IPP_Fread(pReadBuf, 1, 4, fpin);
                            /*horiz_size(need)*/
                            Vc1mSeqHdr.horiz_size   = (pReadBuf[3] << 24) | (pReadBuf[2] << 16) | (pReadBuf[1] << 8) | (pReadBuf[0]);
                            IPP_Fread(pReadBuf, 1, 4, fpin);
                            IPP_Fread(pReadBuf, 1, 4, fpin);
                            /*level, cbr, res1 and hrd_buffer(not need)*/
                            //Vc1mSeqHdr.level        = (pReadBuf[0] >> 5) & 0x7;
                            //Vc1mSeqHdr.cbr          = (pReadBuf[0] >> 4) & 0x1;
                            //Vc1mSeqHdr.hrd_buffer   = (pReadBuf[3] << 16) | (pReadBuf[2] << 8) | (pReadBuf[1]);

                            IPP_Fread(pReadBuf, 1, 4, fpin);
                            /*hrd_rate(not need)*/
                            //Vc1mSeqHdr.hrd_rate     = (pReadBuf[3] << 24) | (pReadBuf[2] << 16) | (pReadBuf[1] << 8) | (pReadBuf[0]);
                            IPP_Fread(pReadBuf, 1, 4, fpin);
                            /*frame_rate(not need)*/
                            //Vc1mSeqHdr.frame_rate   = (pReadBuf[3] << 24) | (pReadBuf[2] << 16) | (pReadBuf[1] << 8) | (pReadBuf[0]);

                            IPP_Printf("ID = %d send set seq info command!\n", pDecInfo->user_id);
                            IPP_StartPerfCounter(perf_cmd_index);
                            rtCode = DecodeSendCmd_Vmeta(IPPVC_SET_VC1M_SEQ_INFO, &Vc1mSeqHdr, NULL, pDecoderState);
                            IPP_StopPerfCounter(perf_cmd_index);
                            IPP_Printf("ID = %d send set seq info command!, rtCode = %d\n", pDecInfo->user_id, rtCode);
                            bReadSeqLayer       = 0;
                        }
#endif

                        /* this offset is not necessary, but with it, maybe avoid internal memory copy*/
                        pBitStream->nOffset += VMETA_COM_PKT_HDR_SIZE;
                        if (bReadSeqLayer) {
                            /*read sequence layer data*/
                            IPP_Fread(pReadBuf, 1, 4, fpin);
                            IPP_Fread(pReadBuf, 1, 4, fpin);
                            IPP_Fseek(fpin, 0, IPP_SEEK_SET);
                            nVal = (pReadBuf[3] << 24) | (pReadBuf[2] << 16) | (pReadBuf[1] << 8) | (pReadBuf[0]);
                            nFrameLen           = 36 + (nVal - 4);
                            bReadSeqLayer       = 0;
                        } else {
                            /*read frame layer data*/
                            nReadNum = IPP_Fread(pReadBuf, 1, 4, fpin);
                            if (4 == nReadNum) {
                                nFrameLen   = (pReadBuf[2] << 16) | (pReadBuf[1] << 8) | (pReadBuf[0]);
#if 0
                                /*not include framesize and timestamp*/
                                nReadNum    = IPP_Fread(pReadBuf, 1, 4, fpin);
#else
                                /*include framesize and timestamp*/
                                IPP_Fseek(fpin, -4, IPP_SEEK_CUR);
                                nFrameLen  += 8; /* the size of timestamp*/
#endif
                                nLeftBytes              = nFrameLen;
                            } else {
                                nFrameLen               = 0;
                            }
                        }
                        nLeftBytes = nFrameLen;
                        if (STREAM_BUF_SIZE < (nFrameLen + pBitStream->nOffset)) {
                            nFrameLen = STREAM_BUF_SIZE - pBitStream->nOffset;
                        }
                        if( (0 == pDecParSetEx->bLessInfo) || nTotalFrames<=LESSINFO_MAX_FRAME) {
                            IPP_Printf("ID = %d framelen = %d to read num = %d\n", pDecInfo->user_id, nLeftBytes, nFrameLen);
                        }
                    } else {
                        IPP_Fscanf(fplen, "%d", &nFrameLen);
                        nLeftBytes = nFrameLen;
                        if (STREAM_BUF_SIZE < nFrameLen) {
                            nFrameLen = STREAM_BUF_SIZE;
                        }
                    }
                }

                nReadNum = IPP_Fread(pBitStream->pBuf + pBitStream->nOffset + pBitStream->nDataLen, 1, nFrameLen, fpin);
                if (nReadNum) {
                    pBitStream->nDataLen    += nReadNum;
                    nLeftBytes              -= nReadNum;

                    if( (0 == pDecParSetEx->bLessInfo) || nTotalFrames<=LESSINFO_MAX_FRAME) {
                        IPP_Printf("ID = %d framelen = %d readnum = %d\n", pDecInfo->user_id, nFrameLen, nReadNum);
                    }

                    if (0 >= nLeftBytes) {
                        if (pDecParSetEx->bNoOutputDelay) {
                            /*for no-output-delay feature, user must attach end-of-frame flag.*/
                            pBitStream->nFlag  |= IPP_VMETA_STRM_BUF_END_OF_FRAME;
                        } else {
                            /*if user doesn't need no-output-delay feature, end-of-unit flag is enough. end-of-frame flag is also right.*/
                            pBitStream->nFlag  |= IPP_VMETA_STRM_BUF_END_OF_UNIT;
                        }
                        nLeftBytes          = 0;
                    }

                    if (pDecParSetEx->bDrmEnable) {
                        scramble_fake(pBitStream);
                    }

                    IPP_StartPerfCounter(perf_push_strm_index);
                    rtCode = DecoderPushBuffer_Vmeta(IPP_VMETA_BUF_TYPE_STRM, (void*)pBitStream, pDecoderState);
                    IPP_StopPerfCounter(perf_push_strm_index);

                    if (IPP_STATUS_NOERR != rtCode) {
                        IPP_Printf("ID = %d push stream buffer error!\n", pDecInfo->user_id);
                        goto fail_cleanup;
                    }

                    if( (0 == pDecParSetEx->bLessInfo) || nTotalFrames<=LESSINFO_MAX_FRAME) {
                        IPP_Printf("ID = %d push strm buf finish. strm buf id: %d\n", pDecInfo->user_id, (int)pBitStream->pUsrData0);
                    }
                } else {
                    bFreeStrmBufFlag[nCurStrmBufIdx] = 1;
                    nFreeStrmBufNum++;
                }
                if ((nFrameLen != nReadNum) || (0 == nReadNum)) {
                    IPP_Printf("ID = %d send end of stream command!, pre datalen = %d\n", pDecInfo->user_id, nReadNum);
                    IPP_StartPerfCounter(perf_cmd_index);
                    DecodeSendCmd_Vmeta(IPPVC_END_OF_STREAM, NULL, NULL, pDecoderState);
                    IPP_StopPerfCounter(perf_cmd_index);
                }
            } else {
                nReadNum = IPP_Fread(pBitStream->pBuf, 1, STREAM_BUF_SIZE, fpin);
                pBitStream->nDataLen = STREAM_BUF_SIZE;
                if (STREAM_BUF_SIZE != nReadNum) {
                    /*end of file*/
                    IPP_Printf("ID = %d add end of unit flag!\n", pDecInfo->user_id);
                    pBitStream->nFlag      |= IPP_VMETA_STRM_BUF_END_OF_UNIT;
                    pBitStream->nDataLen    = nReadNum;                    
                }
                IPP_StartPerfCounter(perf_push_strm_index);
                rtCode = DecoderPushBuffer_Vmeta(IPP_VMETA_BUF_TYPE_STRM, (void*)pBitStream, pDecoderState);
                IPP_StopPerfCounter(perf_push_strm_index);
                if (IPP_STATUS_NOERR != rtCode) {
                    IPP_Printf("ID = %d push stream buffer error!\n", pDecInfo->user_id);
                    goto fail_cleanup;
                }
                if( (0 == pDecParSetEx->bLessInfo) || nTotalFrames<=LESSINFO_MAX_FRAME) {
                    IPP_Printf("ID = %d push strm buf finish. strm buf id: %d datalen=%d\n", pDecInfo->user_id, (int)pBitStream->pUsrData0, pBitStream->nDataLen);
                }
                if (STREAM_BUF_SIZE != pBitStream->nDataLen) {
                    IPP_Printf("ID = %d send end of stream command!, pre datalen = %d\n", pDecInfo->user_id, pBitStream->nDataLen);
                    IPP_StartPerfCounter(perf_cmd_index);
                    rtCode = DecodeSendCmd_Vmeta(IPPVC_END_OF_STREAM, NULL, NULL, pDecoderState);
                    IPP_StopPerfCounter(perf_cmd_index);
                    IPP_Printf("ID = %d send end of stream command!, rtCode = %d\n", pDecInfo->user_id, rtCode);
                }
            }
        } else if (IPP_STATUS_NEED_OUTPUT_BUF == rtCode) {
            if (0 >= nFreePicBufNum) {
                /*for multi-instance, output buffer may not be explicitly returned*/
                if( (0 == pDecParSetEx->bLessInfo) || nTotalFrames<=LESSINFO_MAX_FRAME) {
                    IPP_Printf("ID = %d nFreePicBufNum == 0, try to pop implicitly returned output buffers\n", pDecInfo->user_id);
                }
                while (1) {
                    IPP_StartPerfCounter(perf_pop_pic_index);
                    DecoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_PIC, (void**)&pPicture, pDecoderState);
                    IPP_StopPerfCounter(perf_pop_pic_index);
                    if (NULL == pPicture) {
                        break;
                    }
                    bFreePicBufFlag[(int)pPicture->pUsrData0] = 1;
                    nFreePicBufNum++;

                    if (0 < pPicture->nDataLen) {
                        OutputPicture_Video(pPicture, fpout, pDecInfo);
                        display_frame(hDispCB, &pPicture->pic);

                        nTotalFrames++;
                        if( (0 == pDecParSetEx->bLessInfo) || nTotalFrames<=LESSINFO_MAX_FRAME) {
                            IPP_Printf("ID = %d IPP_STATUS_FRAME_COMPLETE %d id=%d pic_tye = %d POC = %d %d, coded pic = [%dx%d] roi=[%d %d %d %d] coded_type = %d %d\n", pDecInfo->user_id, 
                                nTotalFrames, (int)pPicture->pUsrData0, 
                                pPicture->PicDataInfo.pic_type,
                                pPicture->PicDataInfo.poc[0], pPicture->PicDataInfo.poc[1], 
                                pPicture->pic.picWidth, pPicture->pic.picHeight,
                                pPicture->pic.picROI.x, pPicture->pic.picROI.y,
                                pPicture->pic.picROI.width, pPicture->pic.picROI.height, 
                                pPicture->PicDataInfo.coded_type[0], pPicture->PicDataInfo.coded_type[1]);
                        }
                    } else {
                        if( (0 == pDecParSetEx->bLessInfo) || nTotalFrames<=LESSINFO_MAX_FRAME) {
                            IPP_Printf("ID = %d pop empty picture buffer! id = %d\n", pDecInfo->user_id, (int)pPicture->pUsrData0);
                        }
                    }
                }
            }
            if( (0 == pDecParSetEx->bLessInfo) || nTotalFrames<=LESSINFO_MAX_FRAME) {
                IPP_Printf("ID = %d nFreePicBufNum = %d\n", pDecInfo->user_id, nFreePicBufNum);
            }
            nCurPicBufIdx = 0;
            while ((0 == bFreePicBufFlag[nCurPicBufIdx]) && (PICTURE_BUF_NUM > nCurPicBufIdx)) {
                nCurPicBufIdx++;
            }
            if (PICTURE_BUF_NUM <= nCurPicBufIdx) {
                IPP_Printf("ID = %d error: dis pic buf is not enough!\n", pDecInfo->user_id);
                goto fail_cleanup;
            }
            pPicture = &(PictureGroup[nCurPicBufIdx]);
            if (NULL == pPicture->pBuf) {
                pPicture->pBuf = 
                    (Ipp8u*)vdec_os_api_dma_alloc(pDecInfo->seq_info.dis_buf_size, VMETA_DIS_BUF_ALIGN, &(pPicture->nPhyAddr));
                if (NULL == pPicture->pBuf) {
                     IPP_Printf("ID = %d error: no memory for display!\n", pDecInfo->user_id);
                     IPP_Log(log_file_name, "a", "error: no memory for display!\n");
                     goto fail_cleanup;
                }
                IPP_Printf("ID = %d allocate display buffer %d, paddr = %p, vaddr = %p\n",pDecInfo->user_id, nCurPicBufIdx, pPicture->nPhyAddr, pPicture->pBuf);
                pPicture->nBufSize = pDecInfo->seq_info.dis_buf_size;
            } else {
                if (pPicture->nBufSize < pDecInfo->seq_info.dis_buf_size) {
                    IPP_Printf("ID = %d reallocate display buffer! id = %d presize = %d cursize = %d, ", pDecInfo->user_id, (int)pPicture->pUsrData0, 
                        pPicture->nBufSize, pDecInfo->seq_info.dis_buf_size);
                    vdec_os_api_dma_free(pPicture->pBuf);
                    pPicture->pBuf = NULL;
                    pPicture->pBuf = (Ipp8u*)vdec_os_api_dma_alloc(pDecInfo->seq_info.dis_buf_size, VMETA_DIS_BUF_ALIGN, &(pPicture->nPhyAddr));
                    if (NULL == pPicture->pBuf) {
                        IPP_Printf("\nID = %d error: no memory for display!\n", pDecInfo->user_id);
                        IPP_Log(log_file_name, "a", "error: no memory for display!\n");
                        goto fail_cleanup;
                    }
                    IPP_Printf("paddr = %p, vaddr = %p\n",pPicture->nPhyAddr, pPicture->pBuf);
                    pPicture->nBufSize = pDecInfo->seq_info.dis_buf_size;
                }
            }
            IPP_StartPerfCounter(perf_push_pic_index);
            DecoderPushBuffer_Vmeta(IPP_VMETA_BUF_TYPE_PIC, (void*)pPicture, pDecoderState);
            IPP_StopPerfCounter(perf_push_pic_index);
            if( (0 == pDecParSetEx->bLessInfo) || nTotalFrames<=LESSINFO_MAX_FRAME) {
                IPP_Printf("ID = %d push dis buf finish. dis pic buf id: %d , buf size: %d\n", pDecInfo->user_id, (int)pPicture->pUsrData0, pPicture->nBufSize);
            }
            bFreePicBufFlag[nCurPicBufIdx] = 0;
            nFreePicBufNum--;
        } else if (IPP_STATUS_RETURN_INPUT_BUF == rtCode) {
            while (1) {
                IPP_StartPerfCounter(perf_pop_strm_index);
                DecoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_STRM, (void**)&pBitStream, pDecoderState);
                IPP_StopPerfCounter(perf_pop_strm_index);
                if (NULL == pBitStream) {
                    break;
                }
                bFreeStrmBufFlag[(int)pBitStream->pUsrData0] = 1;
                nFreeStrmBufNum++;
                if( (0 == pDecParSetEx->bLessInfo) || nTotalFrames<=LESSINFO_MAX_FRAME) {
                    IPP_Printf("ID = %d pop strm buf finish. strm buf id: %d\n", pDecInfo->user_id, (int)pBitStream->pUsrData0);
                }
            }
        } else if (IPP_STATUS_FRAME_COMPLETE == rtCode) {
            while (1) {
                IPP_StartPerfCounter(perf_pop_pic_index);
                DecoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_PIC, (void**)&pPicture, pDecoderState);
                IPP_StopPerfCounter(perf_pop_pic_index);
                if (NULL == pPicture) {
                    break;
                }
                bFreePicBufFlag[(int)pPicture->pUsrData0] = 1;
                nFreePicBufNum++;

                if (0 < pPicture->nDataLen) {
                    OutputPicture_Video(pPicture, fpout, pDecInfo);
                    display_frame(hDispCB, &pPicture->pic);

                    nTotalFrames++;
                    if( (0 == pDecParSetEx->bLessInfo) || nTotalFrames<=LESSINFO_MAX_FRAME) {
                        IPP_Printf("ID = %d IPP_STATUS_FRAME_COMPLETE %d id=%d pic_tye = %d POC = %d %d, coded pic = [%dx%d] roi=[%d %d %d %d] coded_type = %d %d\n", pDecInfo->user_id, 
                            nTotalFrames, (int)pPicture->pUsrData0, 
                            pPicture->PicDataInfo.pic_type,
                            pPicture->PicDataInfo.poc[0], pPicture->PicDataInfo.poc[1], 
                            pPicture->pic.picWidth, pPicture->pic.picHeight,
                            pPicture->pic.picROI.x, pPicture->pic.picROI.y,
                            pPicture->pic.picROI.width, pPicture->pic.picROI.height, 
                            pPicture->PicDataInfo.coded_type[0], pPicture->PicDataInfo.coded_type[1]);
                    }
                } else {
                    if (pPicture->PicDataInfo.is_skipped) {
                        nTotalFrames++;
                        nSkipFrames++;
                        if( (0 == pDecParSetEx->bLessInfo) || nTotalFrames<=LESSINFO_MAX_FRAME) {
                            IPP_Printf("ID = %d pop skip picture buffer! id = %d POC= %d %d\n", 
                                pDecInfo->user_id, (int)pPicture->pUsrData0, pPicture->PicDataInfo.poc[0], pPicture->PicDataInfo.poc[1]);
                        }
                    } else {
                        if( (0 == pDecParSetEx->bLessInfo) || nTotalFrames<=LESSINFO_MAX_FRAME) {
                            IPP_Printf("ID = %d pop empty picture buffer! id = %d\n", pDecInfo->user_id, (int)pPicture->pUsrData0);
                        }
                    }
                }
            }
            /*for multi-instance, input buffer may be returned at this event.*/
            while (1) {
                IPP_StartPerfCounter(perf_pop_strm_index);
                DecoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_STRM, (void**)&pBitStream, pDecoderState);
                IPP_StopPerfCounter(perf_pop_strm_index);
                if (NULL == pBitStream) {
                    break;
                }
                bFreeStrmBufFlag[(int)pBitStream->pUsrData0] = 1;
                nFreeStrmBufNum++;
                if( (0 == pDecParSetEx->bLessInfo) || nTotalFrames<=LESSINFO_MAX_FRAME) {
                    IPP_Printf("ID = %d pop strm buf finish. strm buf id: %d\n", pDecInfo->user_id, (int)pBitStream->pUsrData0);
                }
            }

            nCumulTime  = (IPP_GetPerfData(codec_perf_index) + 500) / 1000;
            nCumulTime += (IPP_GetPerfData(perf_push_strm_index) + 500) / 1000;
            nCumulTime += (IPP_GetPerfData(perf_pop_strm_index) + 500) / 1000;
            nCumulTime += (IPP_GetPerfData(perf_push_pic_index) + 500) / 1000;
            nCumulTime += (IPP_GetPerfData(perf_pop_pic_index) + 500) / 1000;
            nCumulTime += (IPP_GetPerfData(perf_cmd_index) + 500) / 1000;
            nFrameTime  = nCumulTime - nOldCumulTime;
            nOldCumulTime = nCumulTime;
            if (nFrameTime > nMaxFrameTime) {
                nMaxFrameTime = nFrameTime;
            }
            if (nFrameTime < nMinFrameTime) {
                nMinFrameTime = nFrameTime;
            }
            pFrameTimeArray[nFrameCount++] = nFrameTime;
            if (nFrameCount >= nMaxFrameCount) {
                IPP_MemRealloc((void**)(&pFrameTimeArray), 4 * nMaxFrameCount, 4 * nMaxFrameCount * 2);
                if (NULL == pFrameTimeArray) {
                    IPP_Log(log_file_name, "a", "error: no memory\n");
                    goto fail_cleanup;
                }
                nMaxFrameCount = nMaxFrameCount * 2;
            }
        } else if (IPP_STATUS_NEW_VIDEO_SEQ == rtCode) {
            // According to current vmeta hal logic, the following block is acturally not necessary, 
            // all display buffers should have already been popped in the last IPP_STATUS_FRAME_COMPLETE
            while (1) {
                IPP_StartPerfCounter(perf_pop_pic_index);
                DecoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_PIC, (void**)&pPicture, pDecoderState);
                IPP_StopPerfCounter(perf_pop_pic_index);
                if (NULL == pPicture) {
                    break;
                }
                bFreePicBufFlag[(int)pPicture->pUsrData0] = 1;
                nFreePicBufNum++;
                IPP_Printf("ID = %d pop display buffer id = %d when encounter new sequence!\n", pDecInfo->user_id, (int)pPicture->pUsrData0);
                if (0 < pPicture->nDataLen) {
                    OutputPicture_Video(pPicture, fpout, pDecInfo);
                    display_frame(hDispCB, &pPicture->pic);

                    nTotalFrames++;
                    IPP_Printf("ID = %d IPP_STATUS_FRAME_COMPLETE %d id=%d pic_tye = %d POC = %d %d, coded pic = [%dx%d] roi=[%d %d %d %d] coded_type = %d %d\n", pDecInfo->user_id, 
                        nTotalFrames, (int)pPicture->pUsrData0, 
                        pPicture->PicDataInfo.pic_type,
                        pPicture->PicDataInfo.poc[0], pPicture->PicDataInfo.poc[1], 
                        pPicture->pic.picWidth, pPicture->pic.picHeight,
                        pPicture->pic.picROI.x, pPicture->pic.picROI.y,
                        pPicture->pic.picROI.width, pPicture->pic.picROI.height, 
                        pPicture->PicDataInfo.coded_type[0], pPicture->PicDataInfo.coded_type[1]);
                } else {
                    if (pPicture->PicDataInfo.is_skipped) {
                        nTotalFrames++;
                        nSkipFrames++;
                        IPP_Printf("ID = %d pop skip picture buffer! id = %d POC= %d %d\n", 
                            pDecInfo->user_id, (int)pPicture->pUsrData0, pPicture->PicDataInfo.poc[0], pPicture->PicDataInfo.poc[1]);
                    } else {
                        IPP_Printf("ID = %d pop empty picture buffer! id = %d\n", pDecInfo->user_id, (int)pPicture->pUsrData0);
                    }
                }
            }
            IPP_Printf("ID = %d IPP_STATUS_NEW_VIDEO_SEQ\n", pDecInfo->user_id);
            IPP_Printf("ID = %d picsize width*height: %d*%d\n", pDecInfo->user_id, pDecInfo->seq_info.max_width, pDecInfo->seq_info.max_height);
            if (!pDecInfo->seq_info.is_intl_seq) {
                IPP_Printf("ID = %d is a progressive sequence!\n", pDecInfo->user_id);
            } else {
                IPP_Printf("ID = %d may be a interleave sequence!\n", pDecInfo->user_id);
            }
            IPP_Printf("ID = %d display buffer num: %d display buffer size: %d\n", pDecInfo->user_id, 
                pDecInfo->seq_info.dis_buf_num, pDecInfo->seq_info.dis_buf_size);
            IPP_Printf("ID = %d frame rate num: %d frame rate den: %d\n", pDecInfo->user_id, 
                pDecInfo->seq_info.frame_rate_num, pDecInfo->seq_info.frame_rate_den);
            IPP_Printf("ID = %d display area: [x = %d, y = %d width = %d height = %d]\n", pDecInfo->user_id, 
                pDecInfo->seq_info.picROI.x, pDecInfo->seq_info.picROI.y,
                pDecInfo->seq_info.picROI.width, pDecInfo->seq_info.picROI.height);
            display_open(hDispCB, pDecInfo->seq_info.max_width, pDecInfo->seq_info.max_height);

            if (IPP_VIDEO_STRM_FMT_JPEG == pDecParSet->strm_fmt) {
                IppVmetaJPEGDecParSet jpegpar;
                jpegpar.roi.x       = pDecParSetEx->roi.x;
                jpegpar.roi.y       = pDecParSetEx->roi.y;
                jpegpar.roi.width   = pDecParSetEx->roi.width;
                jpegpar.roi.height  = pDecParSetEx->roi.height;
                jpegpar.pp_hscale   = pDecParSetEx->pp_hscale;
                jpegpar.pp_vscale   = pDecParSetEx->pp_vscale;
                IPP_Printf("ID = %d set jpeg roi: [x = %d, y = %d width = %d height = %d] scale: [%d %d]\n", pDecInfo->user_id, 
                    jpegpar.roi.x, jpegpar.roi.y, jpegpar.roi.width, jpegpar.roi.height, jpegpar.pp_hscale, jpegpar.pp_vscale);
                rtCode = DecodeSendCmd_Vmeta(IPPVC_RECONFIG, &jpegpar, NULL, pDecoderState);
                if (IPP_STATUS_NOERR != rtCode) {
                    IPP_Printf("ID = %d set jpeg roi failed. rtCode = %d\n", pDecInfo->user_id, rtCode);
                    goto fail_cleanup;
                }
            }
        } else if (IPP_STATUS_END_OF_STREAM == rtCode) {
            IPP_Printf("ID = %d IPP_STATUS_END_OF_STREAM\n", pDecInfo->user_id);
            while (1) {
                IPP_StartPerfCounter(perf_pop_strm_index);
                DecoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_STRM, (void**)&pBitStream, pDecoderState);
                IPP_StopPerfCounter(perf_pop_strm_index);
                if (NULL == pBitStream) {
                    break;
                }
                bFreeStrmBufFlag[(int)pBitStream->pUsrData0] = 1;
                nFreeStrmBufNum++;
                IPP_Printf("ID = %d pop strm buf finish. strm buf id: %d\n", pDecInfo->user_id, (int)pBitStream->pUsrData0);
            }
            while (1) {
                IPP_StartPerfCounter(perf_pop_pic_index);
                DecoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_PIC, (void**)&pPicture, pDecoderState);
                IPP_StopPerfCounter(perf_pop_pic_index);
                if (NULL == pPicture) {
                    break;
                }
                bFreePicBufFlag[(int)pPicture->pUsrData0] = 1;
                nFreePicBufNum++;

                if (0 < pPicture->nDataLen) {
                    OutputPicture_Video(pPicture, fpout, pDecInfo);
                    display_frame(hDispCB, &pPicture->pic);

                    nTotalFrames++;
                    IPP_Printf("ID = %d IPP_STATUS_FRAME_COMPLETE %d id=%d pic_tye = %d POC = %d %d, coded pic = [%dx%d] roi=[%d %d %d %d] coded_type = %d %d\n", pDecInfo->user_id, 
                        nTotalFrames, (int)pPicture->pUsrData0, 
                        pPicture->PicDataInfo.pic_type,
                        pPicture->PicDataInfo.poc[0], pPicture->PicDataInfo.poc[1], 
                        pPicture->pic.picWidth, pPicture->pic.picHeight,
                        pPicture->pic.picROI.x, pPicture->pic.picROI.y,
                        pPicture->pic.picROI.width, pPicture->pic.picROI.height, 
                        pPicture->PicDataInfo.coded_type[0], pPicture->PicDataInfo.coded_type[1]);
                } else {
                    IPP_Printf("ID = %d pop empty picture buffer! id = %d\n", pDecInfo->user_id, (int)pPicture->pUsrData0);
                }
            }
            break;
        } else {
            IPP_Log(log_file_name, "a", "error: deadly error! %d\n", rtCode);
            IPP_Printf("ID = %d error: deadly error! %d\n", pDecInfo->user_id, rtCode);
            goto fail_cleanup;
        }
    }
    IPP_StopPerfCounter(total_perf_index);

    IPP_Printf("ID = %d before DecoderFree_Vmeta\n", pDecInfo->user_id);
    rtCode = DecoderFree_Vmeta(&pDecoderState);
    IPP_Printf("ID = %d after DecoderFree_Vmeta rtCode = %d\n", pDecInfo->user_id, rtCode);

    for (i = 0; i < STREAM_BUF_NUM; i++) {
        if (BitStreamGroup[i].pBuf) {
            vdec_os_api_dma_free(BitStreamGroup[i].pBuf);
        }
    }

    for (i = 0; i < PICTURE_BUF_NUM; i++) {
        if (PictureGroup[i].pBuf) {
            vdec_os_api_dma_free(PictureGroup[i].pBuf);
        }
    }

    nDecTime = IPP_GetPerfData(codec_perf_index);
    IPP_Printf("ID = %d dec time      : %d (ms)\n", pDecInfo->user_id, (nDecTime + 500) / 1000);
    nPushStrmTime = IPP_GetPerfData(perf_push_strm_index);
    IPP_Printf("ID = %d push strm time: %d (ms)\n", pDecInfo->user_id, (nPushStrmTime + 500) / 1000);
    nPopStrmTime = IPP_GetPerfData(perf_pop_strm_index);
    IPP_Printf("ID = %d pop strm time : %d (ms)\n", pDecInfo->user_id, (nPopStrmTime + 500) / 1000);

    nPushPicTime = IPP_GetPerfData(perf_push_pic_index);
    IPP_Printf("ID = %d push pic time : %d (ms)\n", pDecInfo->user_id, (nPushPicTime + 500) / 1000);
    nPopPicTime = IPP_GetPerfData(perf_pop_pic_index);
    IPP_Printf("ID = %d pop pic time  : %d (ms)\n", pDecInfo->user_id, (nPopPicTime + 500) / 1000);

    nCmdTime = IPP_GetPerfData(perf_cmd_index);
    IPP_Printf("ID = %d cmd time      : %d (ms)\n", pDecInfo->user_id, (nCmdTime + 500) / 1000);

    nTotalTime = nDecTime + nPushStrmTime + nPopStrmTime + nPushPicTime + nPopPicTime + nCmdTime;
    IPP_Printf("ID = %d [PERF] Codec Level: ", pDecInfo->user_id);
    IPP_Printf("Total Frame: %d(Skip Frame: %d), Total Time: %d(ms), FPS: %f\n", 
        nTotalFrames, nSkipFrames, (nTotalTime + 500) / 1000, (float)(1000.0 * 1000.0 * nTotalFrames / nTotalTime));

    nTotalTime = IPP_GetPerfData(total_perf_index);
    IPP_Printf("ID = %d [PERF] App Level  : ", pDecInfo->user_id);
    IPP_Printf("Total Frame: %d(Skip Frame: %d), Total Time: %d(ms), FPS: %f\n", 
        nTotalFrames, nSkipFrames, (nTotalTime + 500) / 1000, (float)(1000.0 * 1000.0 * nTotalFrames / nTotalTime));

    g_Tot_Time[IPP_VIDEO_INDEX]     = IPP_GetPerfData(codec_perf_index);
    g_Tot_CPUTime[IPP_VIDEO_INDEX]  = IPP_GetPerfData(thread_perf_index);
    g_Frame_Num[IPP_VIDEO_INDEX]    = nTotalFrames;

    IPP_Printf("ID = %d [PERF] CPU usage: %f\n", pDecInfo->user_id, (float)g_Tot_CPUTime[IPP_VIDEO_INDEX]/g_Tot_Time[IPP_VIDEO_INDEX]);
    IPP_Printf("ID = %d [PERF] FrameCompleteEvent = %d MaxFrameDecTime = %d (ms) MinFrameDecTime = %d (ms)\n", 
        pDecInfo->user_id, nFrameCount, nMaxFrameTime, nMinFrameTime);

    if (pFrameTimeArray) {
#if 0
        IPP_FILE *f;
        f = IPP_Fopen("ft.txt", "w");
        for (i = 0; i < nFrameCount; i++) {
            IPP_Fprintf(f, "%d %u\n", i, pFrameTimeArray[i]);
        }
        IPP_Fclose(f);
#endif
        IPP_MemFree((void**)(&pFrameTimeArray));
    }

    IPP_FreePerfCounter(codec_perf_index);
    IPP_FreePerfCounter(total_perf_index);
    IPP_FreePerfCounter(thread_perf_index);
    IPP_FreePerfCounter(perf_push_strm_index);
    IPP_FreePerfCounter(perf_pop_strm_index);
    IPP_FreePerfCounter(perf_push_pic_index);
    IPP_FreePerfCounter(perf_pop_pic_index);
    IPP_FreePerfCounter(perf_cmd_index);
    display_close();

    IPP_Printf("ID = %d before driver clean\n", pDecInfo->user_id);
    vdec_os_driver_clean();
    IPP_Printf("ID = %d after driver clean\n", pDecInfo->user_id);

    IPP_PysicalMemTest();

    return IPP_OK;

fail_cleanup:
    IPP_Printf("decoding fail: clean up\n");

    IPP_Printf("free codec state\n");
    if (pDecoderState) {
        DecoderFree_Vmeta(&pDecoderState);
    }

    IPP_Printf("free stream buffer\n");
    for (i = 0; i < STREAM_BUF_NUM; i++) {
        if (BitStreamGroup[i].pBuf) {
            vdec_os_api_dma_free(BitStreamGroup[i].pBuf);
        }
    }

    IPP_Printf("free picture buffer\n");
    for (i = 0; i < PICTURE_BUF_NUM; i++) {
        if (PictureGroup[i].pBuf) {
            vdec_os_api_dma_free(PictureGroup[i].pBuf);
        }
    }

    IPP_Printf("free time array\n");
    if (pFrameTimeArray) {
        IPP_MemFree((void**)(&pFrameTimeArray));
    }

    IPP_Printf("driver clean\n");
    vdec_os_driver_clean();

    IPP_Printf("physical memory check\n");
    IPP_PysicalMemTest();

    return IPP_FAIL;
}

/******************************************************************************
// Name:                ParseVmetaDecCmd
// Description:         Parse the user command 
//
// Input Arguments:
//      pCmdLine    :   Pointer to the input command line
//
// Output Arguments:
//      pSrcFileName:   Pointer to src file name
//      pDstFileName:   Pointer to dst file name
//      pLogFileName:   Pointer to log file name
//      pParSet     :   Pointer to codec parameter set
// Returns:
//        [Success]     IPP_OK
//        [Failure]     IPP_FAIL
******************************************************************************/
int ParseVmetaDecCmd(char *pCmdLine, 
                    char *pSrcFileName, 
                    char *pDstFileName, 
                    char *pLogFileName,
                    char *pLenFileName,
                    void *pParSet,
                    void *pParSetEx)
{
#define MAX_PAR_NAME_LEN    1024
#define MAX_PAR_VALUE_LEN   2048
#define STRNCPY(dst, src, len) \
{\
    IPP_Strncpy((dst), (src), (len));\
    (dst)[(len)] = '\0';\
}
    char *pCur, *pEnd;
    char par_name[MAX_PAR_NAME_LEN];
    char par_value[MAX_PAR_VALUE_LEN];
    int  par_name_len;
    int  par_value_len;
    char *p1, *p2, *p3;
    IppVmetaDecParSet   *pDecParSet   = (IppVmetaDecParSet*)pParSet;
    IppVmetaDecParSetEx *pDecParSetEx = (IppVmetaDecParSetEx*)pParSetEx;

    pCur = pCmdLine;
    pEnd = pCmdLine + IPP_Strlen(pCmdLine);

    while((p1 = IPP_Strstr(pCur, "-"))){
        p2 = IPP_Strstr(p1, ":");
        if (NULL == p2) {
            return IPP_FAIL;
        }
        p3 = IPP_Strstr(p2, " "); /*one space*/
        if (NULL == p3) {
            p3 = pEnd;
        }

        par_name_len    = p2 - p1 - 1;
        par_value_len   = p3 - p2 - 1;

        if ((0 >= par_name_len)  || (MAX_PAR_NAME_LEN <= par_name_len) ||
            (0 >= par_value_len) || (MAX_PAR_VALUE_LEN <= par_value_len)) {
            return IPP_FAIL;
        }

        STRNCPY(par_name, p1 + 1, par_name_len);
        if ((0 == IPP_Strcmp(par_name, "i")) || (0 == IPP_Strcmp(par_name, "I"))) {
            /*input file*/
            STRNCPY(pSrcFileName, p2 + 1, par_value_len);
        } else if ((0 == IPP_Strcmp(par_name, "o")) || (0 == IPP_Strcmp(par_name, "O"))) {
            /*output file*/
            STRNCPY(pDstFileName, p2 + 1, par_value_len);
        } else if ((0 == IPP_Strcmp(par_name, "l")) || (0 == IPP_Strcmp(par_name, "L"))) {
            /*log file*/
            STRNCPY(pLogFileName, p2 + 1, par_value_len);
        } else if (0 == IPP_Strcmp(par_name, "len")) {
            /*log file*/
            STRNCPY(pLenFileName, p2 + 1, par_value_len);
        } else if ((0 == IPP_Strcmp(par_name, "p")) || (0 == IPP_Strcmp(par_name, "P"))) {
            /*par file*/
            /*parse par file to fill pParSet*/
        } else if ((0 == IPP_Strcmp(par_name, "c")) || (0 == IPP_Strcmp(par_name, "C"))) {
            STRNCPY(par_value, p2 + 1, par_value_len);
            g_bOptChksum = IPP_Atoi(par_value);
        } else if (0 == IPP_Strcmp(par_name, "fmt")) {
            STRNCPY(par_value, p2 + 1, par_value_len);
            pDecParSet->strm_fmt = IPP_Atoi(par_value);
        } else if (0 == IPP_Strcmp(par_name, "hs")) {
            STRNCPY(par_value, p2 + 1, par_value_len);
            pDecParSetEx->pp_hscale = IPP_Atoi(par_value);
        } else if (0 == IPP_Strcmp(par_name, "vs")) {
            STRNCPY(par_value, p2 + 1, par_value_len);
            pDecParSetEx->pp_vscale = IPP_Atoi(par_value);
        } else if (0 == IPP_Strcmp(par_name, "mi")) {
            STRNCPY(par_value, p2 + 1, par_value_len);
            pDecParSet->bMultiIns = IPP_Atoi(par_value);
        } else if (0 == IPP_Strcmp(par_name, "fu")) {
            STRNCPY(par_value, p2 + 1, par_value_len);
            pDecParSet->bFirstUser = IPP_Atoi(par_value);
        } else if (0 == IPP_Strcmp(par_name, "nr")) {
            STRNCPY(par_value, p2 + 1, par_value_len);
            pDecParSet->no_reordering = IPP_Atoi(par_value);
        } else if (0 == IPP_Strcmp(par_name, "roix")) {
            STRNCPY(par_value, p2 + 1, par_value_len);
            pDecParSetEx->roi.x = IPP_Atoi(par_value);
        } else if (0 == IPP_Strcmp(par_name, "roiy")) {
            STRNCPY(par_value, p2 + 1, par_value_len);
            pDecParSetEx->roi.y = IPP_Atoi(par_value);
        } else if (0 == IPP_Strcmp(par_name, "roiw")) {
            STRNCPY(par_value, p2 + 1, par_value_len);
            pDecParSetEx->roi.width = IPP_Atoi(par_value);
        } else if (0 == IPP_Strcmp(par_name, "roih")) {
            STRNCPY(par_value, p2 + 1, par_value_len);
            pDecParSetEx->roi.height = IPP_Atoi(par_value);
        } else if (0 == IPP_Strcmp(par_name, "fm")) {
            STRNCPY(par_value, p2 + 1, par_value_len);
            pDecParSetEx->eFastMode = IPP_Atoi(par_value);
        } else if (0 == IPP_Strcmp(par_name, "nd")) {
            STRNCPY(par_value, p2 + 1, par_value_len);
            pDecParSetEx->bNoOutputDelay = IPP_Atoi(par_value);
        } else if (0 == IPP_Strcmp(par_name, "drm")) {
            STRNCPY(par_value, p2 + 1, par_value_len);
            pDecParSetEx->bDrmEnable = IPP_Atoi(par_value);
        } else if (0 == IPP_Strcmp(par_name, "im")) {
            STRNCPY(par_value, p2 + 1, par_value_len);
            pDecParSetEx->eInputMode = IPP_Atoi(par_value);
        } else if (0 == IPP_Strcmp(par_name, "db")) {
            STRNCPY(par_value, p2 + 1, par_value_len);
            pDecParSetEx->bDeblocking = IPP_Atoi(par_value);
        } else if (0 == IPP_Strcmp(par_name, "of")) {
            STRNCPY(par_value, p2 + 1, par_value_len);
            pDecParSet->opt_fmt = IPP_Atoi(par_value);
        } else if (0 == IPP_Strcmp(par_name, "dd")) {
            STRNCPY(par_value, p2 + 1, par_value_len);
            pDecParSetEx->bDisableFewerDpb = IPP_Atoi(par_value);
        } else if (0 == IPP_Strcmp(par_name, "lessinfo")) {
            STRNCPY(par_value, p2 + 1, par_value_len);
            pDecParSetEx->bLessInfo = IPP_Atoi(par_value);
        } else if (0 == IPP_Strcmp(par_name, "norc")) {
            STRNCPY(par_value, p2 + 1, par_value_len);
            pDecParSetEx->bNoResoChange = IPP_Atoi(par_value);
        } else {
            /*parse other parameters for encoder*/
        }

        pCur = p3;
    }

    return IPP_OK;
}

/*Interface for IPP sample code template*/
int CodecTest(int argc, char **argv)
{
    IPP_FILE *fpin = NULL, *fpout = NULL, *fplen = NULL;
    IppVmetaDecParSet DecParSet;
    IppVmetaDecParSetEx DecParSetEx;
    char input_file_name[2048]  = {'\0'};
    char output_file_name[2048] = {'\0'};
    char log_file_name[2048]    = {'\0'};
    char len_file_name[2048]    = {'\0'};
    int rtFlag;
    Ipp8u libversion[256]         = {'\0'};

    DecParSet.no_reordering     = 0;
    DecParSet.opt_fmt           = IPP_YCbCr422I;
    DecParSet.strm_fmt          = IPP_VIDEO_STRM_FMT_H264;
    DecParSet.pp_hscale         = 1;
    DecParSet.pp_vscale         = 1;
    DecParSet.bMultiIns         = 1;
    DecParSet.bFirstUser        = 0;

    DecParSetEx.pp_hscale       = 1;
    DecParSetEx.pp_vscale       = 1;
    DecParSetEx.roi.x           = 0;
    DecParSetEx.roi.y           = 0;
    DecParSetEx.roi.width       = 0;
    DecParSetEx.roi.height      = 0;
    DecParSetEx.eFastMode       = IPP_VMETA_FASTMODE_DISABLE;
    DecParSetEx.bNoOutputDelay  = 0;
    DecParSetEx.bDrmEnable      = 0;
    DecParSetEx.eInputMode      = IPP_VMETA_INPUT_MODE_UNAVAIL;
    DecParSetEx.bDeblocking     = 0;
    DecParSetEx.bDisableFewerDpb= 0;
    DecParSetEx.bLessInfo       = 0;

    if (2 > argc) {
        IPP_Printf("Usage: appVmetaDec.exe \"-i:input.cmp -o:output.yuv -l:dec.log -fmt:xx -c:xx\"\n");
        IPP_Printf("       fmt:1(mpeg2), 2(mpeg4), 4(h263), 5(h264), 6(vc-1 ap), 7(jpeg), 8(mjpeg), 10(vc-1 mp&sp)\n");
        IPP_Printf("       c:0(output yuv), 1(output checksum)\n");
        return IPP_FAIL;
    } else if (2 == argc){
        /*for validation*/
        IPP_Printf("Input command line: %s\n", argv[1]);
        if (IPP_OK != ParseVmetaDecCmd(argv[1], input_file_name, output_file_name, log_file_name, len_file_name, &DecParSet, &DecParSetEx)){
            IPP_Log(log_file_name, "w", 
                "Usage: appVmetaDec.exe \"-i:input.cmp -o:output.yuv -l:dec.log -fmt:xx -c:xx!\"\n", argv[1]);
            IPP_Printf("Usage: appVmetaDec.exe \"-i:input.cmp -o:output.yuv -l:dec.log -fmt:xx -c:xx!\"\n", argv[1]);
            return IPP_FAIL;
        }
    } else {
        /*for internal debug*/
        IPP_Strcpy(input_file_name, argv[1]);
        IPP_Strcpy(output_file_name, argv[2]);
        if (3 < argc) {
            DecParSet.strm_fmt = IPP_Atoi(argv[3]);
        }
        if (4 < argc) {
            IPP_Strcpy(len_file_name, argv[4]);
        }
    }

    IPP_Printf("input: %s\n", input_file_name);
    IPP_Printf("output: %s\n", output_file_name);
    if (0 == IPP_Strcmp(input_file_name, "\0")) {
        IPP_Printf("input file name is null!\n");
        IPP_Log(log_file_name, "a", "input file name is null!\n");
        return IPP_FAIL;
    } else {       
        fpin = IPP_Fopen(input_file_name, "rb");
        if (!fpin) {
            IPP_Printf("Fails to open file %s!\n", input_file_name);
            IPP_Log(log_file_name, "a", "Fails to open file %s!\n", input_file_name);
            return IPP_FAIL;
        }
    }

    if (0 == IPP_Strcmp(output_file_name, "\0")) {
        IPP_Printf("output file name is null!\n");
        IPP_Log(log_file_name, "a", "output file name is null!\n");
    } else {
        if (g_bOptChksum) {
            fpout = IPP_Fopen(output_file_name, "w");
        } else {
            fpout = IPP_Fopen(output_file_name, "wb");
        }
        
        if (!fpout) {
            IPP_Printf("Fails to open file %s\n", output_file_name);
            IPP_Log(log_file_name, "a", "Fails to open file %s\n", output_file_name);
            return IPP_FAIL;
        }
    }

    fplen = IPP_Fopen(len_file_name, "rb");
    if (!fplen) {
        IPP_Printf("Fails to open len file %s!\n", len_file_name);
    } else {
        IPP_Printf("Success to open len file %s!\n", len_file_name);
    }

    GetLibVersion_VmetaDEC(libversion,sizeof(libversion));
    IPP_Printf("lib version: %s\n", libversion);
    IPP_Printf("format: %d\n", DecParSet.strm_fmt);
    IPP_Log(log_file_name, "a", "command line: %s\n", argv[1]);
    IPP_Log(log_file_name, "a", "input file  : %s\n", input_file_name);
    IPP_Log(log_file_name, "a", "output file : %s\n", output_file_name);
    IPP_Log(log_file_name, "a", "begin to decode\n");
    IPP_Printf("bMultiIns : %d\n", DecParSet.bMultiIns);
    IPP_Printf("bFirstUser: %d\n", DecParSet.bFirstUser);
    rtFlag = VmetaDecoder(fpin, fpout, log_file_name, fplen, &DecParSet, &DecParSetEx);
    if (IPP_OK == rtFlag) {
        IPP_Log(log_file_name, "a", "everything is OK!\n");
    } else {
        IPP_Log(log_file_name, "a", "decoding fail!\n");
    }

    if (fpin) {
        IPP_Fclose(fpin);
    }

    if (fpout) {
        IPP_Fclose(fpout);
    }

    IPP_Printf("exit!\n");
    return rtFlag;
}

/* EOF */
