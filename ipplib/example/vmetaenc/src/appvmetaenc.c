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

/*vmeta os api*/
#include "vdec_os_api.h"

#define ALIGN16(x)                      ((x + 0xf) & (~0xf))
#define STREAM_BUF_SIZE                 (2047 * 1024) /*must be multiple of 1024*/
#define STREAM_BUF_NUM                  3
#define PICTURE_BUF_NUM                 2
#define LESSINFO_MAX_FRAME              10
int bLessInfo=0;

//#define IPP_Printf 

int LoadYUVData(Ipp8u *pPicBuf, IPP_FILE *infile, int inputYUVmode, int width, int height)
{
    Ipp8u	*tempPtr1;
    int     nStride = ALIGN16(width);
    int     nSliceHeight = ALIGN16(height);
    int     j = 0;

    if((pPicBuf == NULL) || (infile == NULL)){
        return IPP_FAIL;
    }

	tempPtr1 = pPicBuf;
	if (IPP_YCbCr420P == inputYUVmode) {
        for(j = 0; j < height; j++) {
            if ((width) != IPP_Fread(tempPtr1,1,width,infile) ) {
                return IPP_FAIL;
            }
            tempPtr1 += nStride;
        }
		tempPtr1 = pPicBuf + nStride*nSliceHeight;
		for(j = 0; j < height/2; j++) {
			if ((width/2) != IPP_Fread(tempPtr1,1,width/2, infile) ) {
				return IPP_FAIL;
			}
			tempPtr1 += nStride/2;
		}
		tempPtr1 = pPicBuf + nStride*nSliceHeight + nStride*nSliceHeight/4;
		for(j = 0; j < height/2; j++) {
			if ((width/2) != IPP_Fread(tempPtr1,1,width/2, infile) ) {
				return IPP_FAIL;
			}
			tempPtr1 += nStride/2;
		}
	} else if ((IPP_YCbCr420SP == inputYUVmode) || (IPP_YCrCb420SP == inputYUVmode)) {
        for(j = 0; j < height; j++) {
            if ((width) != IPP_Fread(tempPtr1,1,width,infile) ) {
                return IPP_FAIL;
            }
            tempPtr1 += nStride;
        }
		tempPtr1 = pPicBuf + nStride*nSliceHeight;
		for(j = 0; j < height/2; j++) {
			if ((width) != IPP_Fread(tempPtr1,1,width, infile) ) {
				return IPP_FAIL;
			}
			tempPtr1 += nStride;
		}
	} else if ((IPP_YCbCr422I == inputYUVmode) || (IPP_YCbYCr422I == inputYUVmode)) {
         for(j = 0; j < height; j++) {
            if ((width*2) != IPP_Fread(tempPtr1,1,width*2,infile) ) {
                return IPP_FAIL;
            }
            tempPtr1 += nStride*2;
        }
    } else {
        return IPP_FAIL;
    }

	return IPP_OK;
}

int VmetaEncoder(IPP_FILE *fpin, IPP_FILE *fpout, char *log_file_name, IppVmetaEncParSet *pEncParSet)
{
    void *pEncoderState;
    MiscGeneralCallbackTable CBTable;
    IppCodecStatus rtCode;
    IppVmetaBitstream BitStreamGroup[STREAM_BUF_NUM];
    int i;
    IppVmetaBitstream *pBitStream;
    IppVmetaPicture PictureGroup[PICTURE_BUF_NUM];
    IppVmetaEncInfo VideoEncInfo;
    IppVmetaPicture *pPicture;
    IppVmetaEncInfo *pEncInfo;
    int nReadNum;
    int nCurStrmBufIdx;
    int nCurPicBufIdx;
    int bFreeStrmBufFlag[STREAM_BUF_NUM];
    int bFreePicBufFlag[PICTURE_BUF_NUM];

    int bStop;
    int nTotalFrames, nEncodedFrames;
    int nTotalBytes;
    Ipp32u nTotalTime;
//    int nFreeStrmBufNum, nFreePicBufNum;
    int nPicBufSize;
    int nDataSizeY, nDataSizeU, nDataSizeV, nDataSizeUV, nDataSizeYUV;
    int nDataSizeYAlign16, nDataSizeUAlign16,nDataSizeVAlign16 ;
    int nFrameSize;
    int nOptHdrs;
//    int nStrmBufSize;
    int bEOS;

    int nEncTime, nPushPicTime, nPushStrmTime, nPopStrmTime, nPopPicTime, nCmdTime;
    int                         perf_push_strm_index;
    int                         perf_pop_strm_index;
    int                         perf_push_pic_index;
    int                         perf_pop_pic_index;
    int                         perf_cmd_index;
    int                         codec_perf_index;
    int                         total_perf_index;
    int                         thread_perf_index;

    int                         bInitLCD;
    DISPLAY_CB                  DispCB;
    DISPLAY_CB                  *hDispCB = &DispCB;

    Ipp32u nCumulTime, nOldCumulTime;
    Ipp32u nMaxFrameTime, nMinFrameTime, nFrameTime;
    Ipp32u nFrameCount;
    Ipp32u *pFrameTimeArray;
    int nMaxFrameCount;
    int bStartStat;

    int rtFlag = IPP_OK;
//    int bIntWaitEventDisable;
    int ret;
    Ipp32u nVmetaRTLVer;


    pEncoderState   = NULL;
    pFrameTimeArray = NULL;
    for (i = 0; i < STREAM_BUF_NUM; i++) {
        BitStreamGroup[i].pBuf = NULL;
    }
    for (i = 0; i < PICTURE_BUF_NUM; i++) {
        PictureGroup[i].pBuf   = NULL;
    }

    IPP_Printf("start encoding**********************************\n");

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

    IPP_Printf("stream buffer size(kB): %d\n", STREAM_BUF_SIZE/1024);
    IPP_Printf("stream buffer number  : %d\n", STREAM_BUF_NUM);
    for (i = 0; i < STREAM_BUF_NUM; i++) {
        BitStreamGroup[i].pBuf                      = (Ipp8u*)vdec_os_api_dma_alloc(STREAM_BUF_SIZE, VMETA_STRM_BUF_ALIGN, &(BitStreamGroup[i].nPhyAddr));
        if (NULL == BitStreamGroup[i].pBuf) {
            IPP_Log(log_file_name, "a", "error: no memory!\n");
            IPP_Printf("error: no memory!\n");
            goto fail_cleanup;
        }
        IPP_Printf("id = %d nPhyAddr = %p\n", i, BitStreamGroup[i].nPhyAddr);
        BitStreamGroup[i].nBufSize                  = STREAM_BUF_SIZE;
        BitStreamGroup[i].nDataLen                  = 0;
        BitStreamGroup[i].nOffset                   = 0;
        BitStreamGroup[i].nBufProp                  = 0;
        BitStreamGroup[i].pUsrData0                 = (void*)i;
        BitStreamGroup[i].pUsrData1                 = NULL;
        BitStreamGroup[i].pUsrData2                 = NULL;
        BitStreamGroup[i].pUsrData3                 = NULL;
        BitStreamGroup[i].nFlag                     = 0;
        bFreeStrmBufFlag[i]                         = 1;
    }

    nPicBufSize         = 0;
    nDataSizeYUV        = 0;
    nDataSizeY          = 0;
    nDataSizeYAlign16   = 0;
    nDataSizeUV         = 0;
    nDataSizeU          = 0;
    nDataSizeUAlign16   = 0;
    nDataSizeV          = 0;
    nDataSizeVAlign16   = 0;
    if ((IPP_YCbCr422I == pEncParSet->eInputYUVFmt) || (IPP_YCbYCr422I == pEncParSet->eInputYUVFmt)) {
        nPicBufSize         = ALIGN16(pEncParSet->nWidth) * ALIGN16(pEncParSet->nHeight) * 2;
        nDataSizeYUV        = pEncParSet->nWidth * pEncParSet->nHeight * 2;
        IPP_Printf("IPP_YCbCr422I or IPP_YCbYCr422I: nPicBufSize=%d nDataSizeYUV=%d\n", nPicBufSize, nDataSizeYUV);
    } else if ((IPP_YCbCr420SP == pEncParSet->eInputYUVFmt) || (IPP_YCrCb420SP == pEncParSet->eInputYUVFmt)) {
        nPicBufSize         = ALIGN16(pEncParSet->nWidth) * ALIGN16(pEncParSet->nHeight) * 3 / 2;
        nDataSizeY          = pEncParSet->nWidth * pEncParSet->nHeight;
        nDataSizeYAlign16   = ALIGN16(pEncParSet->nWidth) * ALIGN16(pEncParSet->nHeight);
        nDataSizeUV         = pEncParSet->nWidth * pEncParSet->nHeight / 2;
        IPP_Printf("IPP_YCbCr420SP or IPP_YCrCb420SP: nPicBufSize=%d nDataSizeY=%d nDataSizeYAlign16=%d, nDataSizeUV=%d\n", nPicBufSize, nDataSizeY, nDataSizeYAlign16, nDataSizeUV);
    } else if (IPP_YCbCr420P == pEncParSet->eInputYUVFmt) {
        nPicBufSize         = ALIGN16(pEncParSet->nWidth) * ALIGN16(pEncParSet->nHeight) * 3 / 2;
        nDataSizeY          = pEncParSet->nWidth * pEncParSet->nHeight;
        nDataSizeYAlign16   = ALIGN16(pEncParSet->nWidth) * ALIGN16(pEncParSet->nHeight);
        nDataSizeU          = pEncParSet->nWidth * pEncParSet->nHeight / 4;
        nDataSizeUAlign16   = ALIGN16(pEncParSet->nWidth) * ALIGN16(pEncParSet->nHeight) / 4;
        nDataSizeV          = pEncParSet->nWidth * pEncParSet->nHeight / 4;
        nDataSizeVAlign16   = ALIGN16(pEncParSet->nWidth) * ALIGN16(pEncParSet->nHeight) / 4;
    } else {
    }
    nFrameSize = nPicBufSize;

    IPP_Printf("pic buffer size  : %d\n", nPicBufSize);
    IPP_Printf("pic buffer number: %d\n", PICTURE_BUF_NUM);
    for (i = 0; i < PICTURE_BUF_NUM; i++) {
        PictureGroup[i].pBuf                      = (Ipp8u*)vdec_os_api_dma_alloc(nPicBufSize, VMETA_DIS_BUF_ALIGN, &(PictureGroup[i].nPhyAddr));
        if (NULL == PictureGroup[i].pBuf) {
            IPP_Log(log_file_name, "a", "error: no memory!\n");
            IPP_Printf("error: no memory!\n");
            goto fail_cleanup;
        }
        IPP_Printf("id = %d nPhyAddr = %p\n", i, PictureGroup[i].nPhyAddr);
        PictureGroup[i].nBufSize            = nPicBufSize;
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
    IPP_MemMalloc(&pFrameTimeArray, 4 * nMaxFrameCount, 4);
    if (NULL == pFrameTimeArray) {
        IPP_Log(log_file_name, "a", "error: no memory\n");
        goto fail_cleanup;
    }

    IPP_Printf("before EncoderInitAlloc_Vmeta\n");
    rtCode = EncoderInitAlloc_Vmeta(pEncParSet, &CBTable, &pEncoderState);
    if (IPP_STATUS_NOERR != rtCode) {
        EncoderFree_Vmeta(&pEncoderState);
        IPP_Log(log_file_name, "a", "error: encoder init fail, error code %d!\n", rtCode);
        IPP_Printf("error: encoder init fail, error code %d!\n", rtCode);
        goto fail_cleanup;
    }
    IPP_Printf("after EncoderInitAlloc_Vmeta\n");

    /* this command is to let encoder output pps and sps separately, only valid for h264*/
    /*
    nOptHdrs = 0 :  sps&pps are outputted with I frame.
    nOptHdrs = 1 :  the first sps&pps are outputted separately, and others will be skipped.
    nOptHdrs = 2 :  all sps&pps are outputted separately.
    */
    nOptHdrs = 1;
    EncodeSendCmd_Vmeta(IPPVC_OUTPUT_SPS_AND_PPS, &nOptHdrs, NULL, pEncoderState);

    rtCode = EncodeSendCmd_Vmeta(IPPVC_GET_HW_VERSION, NULL, &nVmetaRTLVer, pEncoderState);
    if (IPP_STATUS_NOERR != rtCode) {
        IPP_Printf("fail to get vmeta version, rtCode=%d\n", rtCode);
    } else {
        IPP_Printf("Vmeta RTL version is %p\n", nVmetaRTLVer);
    }

    bStop               = 0;
    pBitStream          = NULL;
    pPicture            = NULL;
    pEncInfo            = &VideoEncInfo;
    nCurStrmBufIdx      = 0;
    nCurPicBufIdx       = 0;
    nTotalFrames        = 0;
    nTotalTime          = 0;
    bInitLCD            = 0;
    nTotalBytes         = 0;
    nEncodedFrames      = 0;
    bEOS                = 0;

    nCumulTime          = 0;
    nOldCumulTime       = 0;
    nMaxFrameTime       = 0;
    nMinFrameTime       = 0xffffffff;
    nFrameTime          = 0;
    nFrameCount         = 0;
    bStartStat          = 0;

    IPP_StartPerfCounter(total_perf_index);
    while(!bStop) {
        IPP_StartPerfCounter(codec_perf_index);
        IPP_StartPerfCounter(thread_perf_index);
        rtCode = EncodeFrame_Vmeta(pEncInfo, pEncoderState);
        IPP_StopPerfCounter(thread_perf_index);
        IPP_StopPerfCounter(codec_perf_index);

        if (IPP_STATUS_NEED_OUTPUT_BUF == rtCode) {
            if (!bLessInfo || nTotalFrames<=LESSINFO_MAX_FRAME) {
                IPP_Printf("ID = %d IPP_STATUS_NEED_OUTPUT_BUF\n", pEncInfo->user_id);
            }
            nCurStrmBufIdx = 0;
            while ((0 == bFreeStrmBufFlag[nCurStrmBufIdx]) && (STREAM_BUF_NUM > nCurStrmBufIdx)) {
                nCurStrmBufIdx++;
            }
            if (STREAM_BUF_NUM <= nCurStrmBufIdx) {
                IPP_Printf("ID = %d error: strm buf is not enough!\n", pEncInfo->user_id);
                goto fail_cleanup;
            }

            pBitStream = &(BitStreamGroup[nCurStrmBufIdx]);
            bFreeStrmBufFlag[nCurStrmBufIdx] = 0;

            IPP_StartPerfCounter(perf_push_strm_index);
            rtCode = EncoderPushBuffer_Vmeta(IPP_VMETA_BUF_TYPE_STRM, (void*)pBitStream, pEncoderState);
            IPP_StopPerfCounter(perf_push_strm_index);
            if (IPP_STATUS_NOERR != rtCode) {
                IPP_Printf("ID = %d push stream buffer error!\n", pEncInfo->user_id);
                goto fail_cleanup;
            }
            if (!bLessInfo || nTotalFrames<=LESSINFO_MAX_FRAME) {
                IPP_Printf("ID = %d push strm buf finish. strm buf id: %d\n", pEncInfo->user_id, (int)pBitStream->pUsrData0);
            }
        } else if (IPP_STATUS_NEED_INPUT == rtCode) {
            if (!bLessInfo || nTotalFrames<=LESSINFO_MAX_FRAME) {
                IPP_Printf("ID = %d IPP_STATUS_NEED_INPUT\n", pEncInfo->user_id);
            }
            nCurPicBufIdx = 0;
            while ((0 == bFreePicBufFlag[nCurPicBufIdx]) && (PICTURE_BUF_NUM > nCurPicBufIdx)) {
                nCurPicBufIdx++;
            }
            if (PICTURE_BUF_NUM <= nCurPicBufIdx) {
                IPP_Printf("ID = %d error: dis pic buf is not enough!\n", pEncInfo->user_id);
                goto fail_cleanup;
            }
            pPicture = &(PictureGroup[nCurPicBufIdx]);

            if ((NULL != pPicture->pBuf) && (pPicture->nBufSize < pEncInfo->dis_buf_size)) {
                if (!bLessInfo || nTotalFrames<=LESSINFO_MAX_FRAME) {
                    IPP_Printf("ID = %d pre-allocated picture is smaller than needed, pre = %d needed = %d\n", pEncInfo->user_id, 
                        pPicture->nBufSize, pEncInfo->dis_buf_size);
                }
                vdec_os_api_dma_free(pPicture->pBuf);
                pPicture->pBuf      = NULL;
                pPicture->nPhyAddr  = 0;
                pPicture->nBufSize  = 0;
            }

            if (NULL == pPicture->pBuf) {
                pPicture->pBuf                      = (Ipp8u*)vdec_os_api_dma_alloc(pEncInfo->dis_buf_size, VMETA_DIS_BUF_ALIGN, &(pPicture->nPhyAddr));
                if (NULL == pPicture->pBuf) {
                    IPP_Printf("ID = %d error: no memory!\n", pEncInfo->user_id);
                    goto fail_cleanup;
                }
                pPicture->nBufSize                  = pEncInfo->dis_buf_size;
            }

            ret = LoadYUVData(pPicture->pBuf, fpin, pEncParSet->eInputYUVFmt, pEncParSet->nWidth, pEncParSet->nHeight);
            if (ret != IPP_OK) {
                bEOS = 1;
            }

            if (bEOS) {
                IPP_Printf("ID = %d send stream end command\n", pEncInfo->user_id);
                IPP_StartPerfCounter(perf_cmd_index);
                EncodeSendCmd_Vmeta(IPPVC_END_OF_STREAM, NULL, NULL, pEncoderState);
                IPP_StopPerfCounter(perf_cmd_index);
            } else {
                pPicture->nDataLen = nFrameSize; /*currently, nDataLen value will be ignored inside encoder*/
                IPP_StartPerfCounter(perf_push_pic_index);
                EncoderPushBuffer_Vmeta(IPP_VMETA_BUF_TYPE_PIC, (void*)pPicture, pEncoderState);
                IPP_StopPerfCounter(perf_push_pic_index);
                if (!bLessInfo || nTotalFrames<=LESSINFO_MAX_FRAME) {
                    IPP_Printf("ID = %d push dis buf finish. dis pic buf id: %d, FrameNum = %d\n", pEncInfo->user_id, (int)pPicture->pUsrData0, nTotalFrames);
                }
                bFreePicBufFlag[nCurPicBufIdx] = 0;
                nTotalFrames++;
            }
        } else if (IPP_STATUS_END_OF_PICTURE == rtCode) {
            if (!bLessInfo || nTotalFrames<=LESSINFO_MAX_FRAME) {        
                IPP_Printf("ID = %d IPP_STATUS_END_OF_PICTURE\n", pEncInfo->user_id);
            }
            while (1) {
                IPP_StartPerfCounter(perf_pop_pic_index);
                EncoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_PIC, (void**)&pPicture, pEncoderState);
                IPP_StopPerfCounter(perf_pop_pic_index);
                if (NULL == pPicture) {
                    break;
                }
                bFreePicBufFlag[(int)pPicture->pUsrData0] = 1;
                if (!bLessInfo || nTotalFrames<=LESSINFO_MAX_FRAME) {
                    IPP_Printf("ID = %d pop pic buf finish. pic buf id: %d\n", pEncInfo->user_id, (int)pPicture->pUsrData0);
                }
            }
            while (1) {
                IPP_StartPerfCounter(perf_pop_strm_index);
                EncoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_STRM, (void**)&pBitStream, pEncoderState);
                IPP_StopPerfCounter(perf_pop_strm_index);
                if (NULL == pBitStream) {
                    break;
                }
                bFreeStrmBufFlag[(int)pBitStream->pUsrData0] = 1;
                if (fpout) {
                    IPP_Fwrite(pBitStream->pBuf + pBitStream->nOffset, 1, pBitStream->nDataLen, fpout);
                }
                if (!bLessInfo || nTotalFrames<=LESSINFO_MAX_FRAME) {
                    IPP_Printf("ID = %d pop strm buf finish. strm buf id: %d len: %d offset = %d\n", 
                        pEncInfo->user_id, (int)pBitStream->pUsrData0, pBitStream->nDataLen, pBitStream->nOffset);
                }
                if (pBitStream->nFlag & IPP_VMETA_STRM_BUF_SEQ_HDR_DATA) {
                    IPP_Printf("ID = %d this is SPS\n", pEncInfo->user_id);
                }
                if (pBitStream->nFlag & IPP_VMETA_STRM_BUF_PIC_HDR_DATA) {
                    IPP_Printf("ID = %d this is PPS\n", pEncInfo->user_id);
                }
                if (pBitStream->nFlag & IPP_VMETA_STRM_BUF_END_OF_FRAME) {
                    if (!bLessInfo || nTotalFrames<=LESSINFO_MAX_FRAME) {
                        if (pBitStream->nFlag & IPP_VMETA_STRM_BUF_I_FRAME) {
                            IPP_Printf("ID = %d this is the last stream buffer (I frame) for frame %d!\n", pEncInfo->user_id, nEncodedFrames);
                        } else if (pBitStream->nFlag & IPP_VMETA_STRM_BUF_P_FRAME) {
                            IPP_Printf("ID = %d this is the last stream buffer (P frame) for frame %d!\n", pEncInfo->user_id, nEncodedFrames);
                        } else if (pBitStream->nFlag & IPP_VMETA_STRM_BUF_B_FRAME) {
                            IPP_Printf("ID = %d this is the last stream buffer (B frame) for frame %d!\n", pEncInfo->user_id, nEncodedFrames);
                        } else {
                            IPP_Printf("ID = %d this is the last stream buffer (unknown frame) for frame %d!\n", pEncInfo->user_id, nEncodedFrames);
                        }
                    }
                    nEncodedFrames++;
                }
                nTotalBytes += pBitStream->nDataLen;
            }

            if (bStartStat) {
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
                    IPP_MemRealloc(&pFrameTimeArray, 4 * nMaxFrameCount, 4 * nMaxFrameCount * 2);
                    if (NULL == pFrameTimeArray) {
                        IPP_Log(log_file_name, "a", "error: no memory\n");
                        goto fail_cleanup;
                    }
                    nMaxFrameCount = nMaxFrameCount * 2;
                }
            }
            bStartStat = 1; /*skip first end-of-picture event*/
        } else if (IPP_STATUS_RETURN_INPUT_BUF == rtCode) {
            if (!bLessInfo || nTotalFrames<=LESSINFO_MAX_FRAME) {        
                IPP_Printf("ID = %d IPP_STATUS_RETURN_INPUT_BUF\n", pEncInfo->user_id);
            }
            while (1) {
                IPP_StartPerfCounter(perf_pop_pic_index);
                EncoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_PIC, (void**)&pPicture, pEncoderState);
                IPP_StopPerfCounter(perf_pop_pic_index);
                if (NULL == pPicture) {
                    break;
                }
                bFreePicBufFlag[(int)pPicture->pUsrData0] = 1;
                if (!bLessInfo || nTotalFrames<=LESSINFO_MAX_FRAME) {
                    IPP_Printf("ID = %d pop pic buf finish. pic buf id: %d\n", pEncInfo->user_id, (int)pPicture->pUsrData0);
                }
            }
        } else if (IPP_STATUS_OUTPUT_DATA == rtCode) {
            if (!bLessInfo || nTotalFrames<=LESSINFO_MAX_FRAME) {
                IPP_Printf("ID = %d IPP_STATUS_OUTPUT_DATA\n", pEncInfo->user_id);
            }
            while (1) {
                IPP_StartPerfCounter(perf_pop_strm_index);
                EncoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_STRM, (void**)&pBitStream, pEncoderState);
                IPP_StopPerfCounter(perf_pop_strm_index);
                if (NULL == pBitStream) {
                    break;
                }
                bFreeStrmBufFlag[(int)pBitStream->pUsrData0] = 1;
                if (fpout) {
                    IPP_Fwrite(pBitStream->pBuf + pBitStream->nOffset, 1, pBitStream->nDataLen, fpout);
                }
                if (!bLessInfo || nTotalFrames<=LESSINFO_MAX_FRAME) {
                    IPP_Printf("ID = %d pop strm buf finish. strm buf id: %d len: %d offset = %d\n", 
                        pEncInfo->user_id, (int)pBitStream->pUsrData0, pBitStream->nDataLen, pBitStream->nOffset);
                }
                if (pBitStream->nFlag & IPP_VMETA_STRM_BUF_SEQ_HDR_DATA) {
                    IPP_Printf("ID = %d this is SPS\n", pEncInfo->user_id);
                }
                if (pBitStream->nFlag & IPP_VMETA_STRM_BUF_PIC_HDR_DATA) {
                    IPP_Printf("ID = %d this is PPS\n", pEncInfo->user_id);
                }
                if (pBitStream->nFlag & IPP_VMETA_STRM_BUF_END_OF_FRAME) {
                    if (!bLessInfo || nTotalFrames<=LESSINFO_MAX_FRAME) {
                        if (pBitStream->nFlag & IPP_VMETA_STRM_BUF_I_FRAME) {
                            IPP_Printf("ID = %d this is the last stream buffer (I frame) for frame %d!\n", pEncInfo->user_id, nEncodedFrames);
                        } else if (pBitStream->nFlag & IPP_VMETA_STRM_BUF_P_FRAME) {
                            IPP_Printf("ID = %d this is the last stream buffer (P frame) for frame %d!\n", pEncInfo->user_id, nEncodedFrames);
                        } else if (pBitStream->nFlag & IPP_VMETA_STRM_BUF_B_FRAME) {
                            IPP_Printf("ID = %d this is the last stream buffer (B frame) for frame %d!\n", pEncInfo->user_id, nEncodedFrames);
                        } else {
                            IPP_Printf("ID = %d this is the last stream buffer (unknown frame) for frame %d!\n", pEncInfo->user_id, nEncodedFrames);
                        }
                    }
                    nEncodedFrames++;
                }
                nTotalBytes += pBitStream->nDataLen;
            }          
        } else if (IPP_STATUS_END_OF_STREAM == rtCode){
            if (!bLessInfo || nTotalFrames<=LESSINFO_MAX_FRAME) {
                IPP_Printf("ID = %d IPP_STATUS_END_OF_STREAM\n", pEncInfo->user_id);
            }
            while (1) {
                IPP_StartPerfCounter(perf_pop_pic_index);
                EncoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_PIC, (void**)&pPicture, pEncoderState);
                IPP_StopPerfCounter(perf_pop_pic_index);
                if (NULL == pPicture) {
                    break;
                }
                bFreePicBufFlag[(int)pPicture->pUsrData0] = 1;
                if (!bLessInfo || nTotalFrames<=LESSINFO_MAX_FRAME) {
                    IPP_Printf("ID = %d pop pic buf finish. pic buf id: %d\n", pEncInfo->user_id, (int)pPicture->pUsrData0);
                }
            }

            while (1) {
                IPP_StartPerfCounter(perf_pop_strm_index);
                EncoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_STRM, (void**)&pBitStream, pEncoderState);
                IPP_StopPerfCounter(perf_pop_strm_index);
                if (NULL == pBitStream) {
                    break;
                }
                bFreeStrmBufFlag[(int)pBitStream->pUsrData0] = 1;
                if (fpout) {
                    IPP_Fwrite(pBitStream->pBuf + pBitStream->nOffset, 1, pBitStream->nDataLen, fpout);
                }
                if (!bLessInfo || nTotalFrames<=LESSINFO_MAX_FRAME) {
                    IPP_Printf("ID = %d pop strm buf finish. strm buf id: %d len: %d offset = %d\n", 
                        pEncInfo->user_id, (int)pBitStream->pUsrData0, pBitStream->nDataLen, pBitStream->nOffset);
                }
                if (pBitStream->nFlag & IPP_VMETA_STRM_BUF_END_OF_FRAME) {
                    if (!bLessInfo || nTotalFrames<=LESSINFO_MAX_FRAME) {
                        if (pBitStream->nFlag & IPP_VMETA_STRM_BUF_I_FRAME) {
                            IPP_Printf("ID = %d this is the last stream buffer (I frame) for frame %d!\n", pEncInfo->user_id, nEncodedFrames);
                        } else if (pBitStream->nFlag & IPP_VMETA_STRM_BUF_P_FRAME) {
                            IPP_Printf("ID = %d this is the last stream buffer (P frame) for frame %d!\n", pEncInfo->user_id, nEncodedFrames);
                        } else if (pBitStream->nFlag & IPP_VMETA_STRM_BUF_B_FRAME) {
                            IPP_Printf("ID = %d this is the last stream buffer (B frame) for frame %d!\n", pEncInfo->user_id, nEncodedFrames);
                        } else {
                            IPP_Printf("ID = %d this is the last stream buffer (unknown frame) for frame %d!\n", pEncInfo->user_id, nEncodedFrames);
                        }
                    }
                    nEncodedFrames++;
                }
                nTotalBytes += pBitStream->nDataLen;
            }
            break;
        } else {
            IPP_Log(log_file_name, "a", "error: deadly error! %d\n", rtCode);
            IPP_Printf("ID = %d error: deadly error! %d\n", pEncInfo->user_id, rtCode);
            goto fail_cleanup;
        }
    }
    IPP_StopPerfCounter(total_perf_index);

    IPP_Printf("ID = %d before EncoderFree_Vmeta\n", pEncInfo->user_id);
    rtCode = EncoderFree_Vmeta(&pEncoderState);
    IPP_Printf("ID = %d after EncoderFree_Vmeta rtCode = %d\n", pEncInfo->user_id, rtCode);
    
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

    nEncTime = IPP_GetPerfData(codec_perf_index);
    IPP_Printf("ID = %d enc time      : %d (ms)\n", pEncInfo->user_id, (nEncTime + 500) / 1000);
    nPushStrmTime = IPP_GetPerfData(perf_push_strm_index);
    IPP_Printf("ID = %d push strm time: %d (ms)\n", pEncInfo->user_id, (nPushStrmTime + 500) / 1000);
    nPopStrmTime = IPP_GetPerfData(perf_pop_strm_index);
    IPP_Printf("ID = %d pop strm time : %d (ms)\n", pEncInfo->user_id, (nPopStrmTime + 500) / 1000);
    nPushPicTime = IPP_GetPerfData(perf_push_pic_index);
    IPP_Printf("ID = %d push pic time : %d (ms)\n", pEncInfo->user_id, (nPushPicTime + 500) / 1000);
    nPopPicTime = IPP_GetPerfData(perf_pop_pic_index);
    IPP_Printf("ID = %d pop pic time  : %d (ms)\n", pEncInfo->user_id, (nPopPicTime + 500) / 1000);
    nCmdTime = IPP_GetPerfData(perf_cmd_index);
    IPP_Printf("ID = %d cmd time      : %d (ms)\n", pEncInfo->user_id, (nCmdTime + 500) / 1000);

    nTotalTime = nEncTime + nPushStrmTime + nPopStrmTime + nPushPicTime + nPopPicTime + nCmdTime;
    IPP_Printf("ID = %d [PERF] Codec Level: ", pEncInfo->user_id);
    IPP_Printf("ID = %d Total Frame: %d, Total Time: %d(ms), FPS: %f\n", pEncInfo->user_id, 
        nTotalFrames, (nTotalTime + 500) / 1000, (float)(1000.0 * 1000.0 * nTotalFrames / nTotalTime));

    nTotalTime = IPP_GetPerfData(total_perf_index);
    IPP_Printf("ID = %d [PERF] App Level  : ", pEncInfo->user_id);
    IPP_Printf("ID = %d Total Frame: %d, Total Time: %d(ms), FPS: %f\n", pEncInfo->user_id, 
        nTotalFrames, (nTotalTime + 500) / 1000, (float)(1000.0 * 1000.0 * nTotalFrames / nTotalTime));

    g_Tot_Time[IPP_VIDEO_INDEX]     = IPP_GetPerfData(codec_perf_index);
    g_Tot_CPUTime[IPP_VIDEO_INDEX]  = IPP_GetPerfData(thread_perf_index);
    g_Frame_Num[IPP_VIDEO_INDEX]    = nTotalFrames;

    IPP_Printf("ID = %d [PERF] CPU usage: %f\n", pEncInfo->user_id, (float)g_Tot_CPUTime[IPP_VIDEO_INDEX]/g_Tot_Time[IPP_VIDEO_INDEX]);
    IPP_Printf("ID = %d [PERF] FrameCompleteEvent = %d MaxFrameEncTime = %d (ms) MinFrameEncTime = %d (ms)\n", 
        pEncInfo->user_id, nFrameCount, nMaxFrameTime, nMinFrameTime);

    if (pFrameTimeArray) {
        IPP_MemFree(&pFrameTimeArray);
    }

    if (0 == pEncParSet->nFrameRateNum) {
        pEncParSet->nFrameRateNum = 30;
    }
    IPP_Printf("ID = %d Stream Size     = %.2f(kB)\n", pEncInfo->user_id, (float)nTotalBytes / 1024);
    IPP_Printf("ID = %d Frame Rate      = %.2f(f/s)\n", pEncInfo->user_id, (float)pEncParSet->nFrameRateNum);
    IPP_Printf("ID = %d Stream Duration = %.2f(s)\n", pEncInfo->user_id, 
        (float)nTotalFrames / pEncParSet->nFrameRateNum);
    IPP_Printf("ID = %d Stream BitRate  = %.2f(kbps)\n", pEncInfo->user_id, 
        (float)nTotalBytes * 8 / 1024 / (nTotalFrames / pEncParSet->nFrameRateNum));

    IPP_FreePerfCounter(codec_perf_index);
    IPP_FreePerfCounter(total_perf_index);
    IPP_FreePerfCounter(thread_perf_index);
    IPP_FreePerfCounter(perf_push_strm_index);
    IPP_FreePerfCounter(perf_pop_strm_index);
    IPP_FreePerfCounter(perf_push_pic_index);
    IPP_FreePerfCounter(perf_pop_pic_index);
    IPP_FreePerfCounter(perf_cmd_index);

    IPP_Printf("ID = %d before driver clean\n", pEncInfo->user_id);
    vdec_os_driver_clean();
    IPP_Printf("ID = %d after driver clean\n", pEncInfo->user_id);

    IPP_PysicalMemTest();

    return IPP_OK;

fail_cleanup:
    IPP_Printf("encoding fail: clean up\n");

    IPP_Printf("free codec state\n");
    if (pEncoderState) {
        EncoderFree_Vmeta(&pEncoderState);
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
        IPP_MemFree(&pFrameTimeArray);
    }

    IPP_Printf("driver clean\n");
    vdec_os_driver_clean();

    IPP_Printf("physical memory check\n");
    IPP_PysicalMemTest();

    return IPP_FAIL;
}


#define VMETA_ENC_PARA_NUM 13

typedef struct {
    char *name;
    void *p;
} ParaTable;

static IppVmetaEncParSet g_VmetaEncParSet;

static ParaTable Map[VMETA_ENC_PARA_NUM]={
    {"eInputYUVFmt",            &g_VmetaEncParSet.eInputYUVFmt             },

    {"eOutputStrmFmt",          &g_VmetaEncParSet.eOutputStrmFmt           },
    {"nWidth",                  &g_VmetaEncParSet.nWidth                   },
    {"nHeight",                 &g_VmetaEncParSet.nHeight                  },
    {"nPBetweenI",              &g_VmetaEncParSet.nPBetweenI               },
    {"nBFrameNum",              &g_VmetaEncParSet.nBFrameNum               },

    {"bRCEnable",               &g_VmetaEncParSet.bRCEnable                },
    {"nRCType",                 &g_VmetaEncParSet.nRCType                  },
    {"nQP",                     &g_VmetaEncParSet.nQP                      },
    
    {"nRCBitRate",              &g_VmetaEncParSet.nRCBitRate               },
    {"nMaxBitRate",             &g_VmetaEncParSet.nMaxBitRate              },
    {"nFrameRateNum",           &g_VmetaEncParSet.nFrameRateNum            },
    {"bMultiIns",               &g_VmetaEncParSet.bMultiIns                },
    {"bFirstUser",              &g_VmetaEncParSet.bFirstUser               },
    
};

/******************************************************************************
// Name:             ParseItem
// Description:      parse a item from input command line
//
// Input Arguments:	
//      content		    - command line
//      pIdx		    - current position
//      len		        - the length of command line
//
//	Output Arguments:
//      item            - parsed item
//
// Returns:
//      int             - parameter index
******************************************************************************/
int ParseItem(char *content, int *pIdx, int len, char *item)
{
    int i;
    int start = 0;
    i = 0;
    while (*pIdx < len){
        if ((content[*pIdx] != ' ') && (content[*pIdx] != '\t')){
            item[i++] = content[(*pIdx)];
            (*pIdx)++;
            start = 1;
        } else {
            (*pIdx)++;
            if (1 == start){
                break;
            }
        }
    }
    item[i] = '\0';

    return start;
}

/******************************************************************************
// Name:             GetParaIndex
// Description:      get the parameter index according to parameter name
//
// Input Arguments:	
//      ParaName		- parameter name
//
//	Output Arguments:
//
// Returns:
//      int             - parameter index
******************************************************************************/
int GetParaIndex(char *ParaName)
{
    int i;

    for (i = 0; i < VMETA_ENC_PARA_NUM; i++){
        if (0 == IPP_Strcmp(ParaName, Map[i].name)){
            return i;
        }
    }
    return i;
}

/******************************************************************************
// Name:             ParaConfig
// Description:      parse config file to get encoding parameters
//
// Input Arguments:	
//      f		        - Config file
//
//	Output Arguments:
//		pPar	        - Encoding parameter
//
// Returns:
//      1               - success
******************************************************************************/
int ReadConfigFile(IPP_FILE *f)
{
    int rtCode;
    char content[2048];
    char name[256];
    char value[256];
    int len;
    int nIdx, nParaIdx;
    int *p;
    char *rt;
    while (!IPP_Feof(f)){
        rt = IPP_Fgets(content, 512, f);
        if (NULL != rt){
            if (content[0] == '\0' || content[0] == '#' || content[0] == '\n'){
                /*comments or useless content*/
            } else {
                len = IPP_Strlen(content);
                nIdx = 0;
                rtCode = ParseItem(content, &nIdx, len, name);
                if (0 != rtCode){
                    nParaIdx = GetParaIndex(name);
                    if (nParaIdx < VMETA_ENC_PARA_NUM){
                        /* '=' */
                        ParseItem(content, &nIdx, len, value);
                        /* value */
                        rtCode = ParseItem(content, &nIdx, len, value);
                        if (0 != rtCode ){
                            p = (int *)Map[nParaIdx].p;
                            *p = IPP_Atoi(value);
                        }
                    }
                }
            }
        }
    }

    return 1;
}

void InitVmetaEncPara(IppVmetaEncParSet *pEncParSet)
{
    pEncParSet->eInputYUVFmt                = IPP_YCbCr422I;
    pEncParSet->eOutputStrmFmt              = IPP_VIDEO_STRM_FMT_H264;
    pEncParSet->nWidth                      = 176;
    pEncParSet->nHeight                     = 144;

    pEncParSet->bRCEnable                   = 0;
    
    pEncParSet->nPBetweenI                  = 29;
    pEncParSet->nBFrameNum                  = 0;
    pEncParSet->nQP                         = 30;
    pEncParSet->nRCBitRate                  = 256000;
    pEncParSet->nFrameRateNum               = 30;
    pEncParSet->nRCType                     = 0;
    pEncParSet->nMaxBitRate                 = 0;
    pEncParSet->bMultiIns                   = 1;
    pEncParSet->bFirstUser                  = 0;

};

void OutputVmetaPara(IppVmetaEncParSet *pPar)
{
    IPP_Printf("#############################################################\n");
    IPP_Printf("#            Encoding Parameter Setting:                    #\n");
    IPP_Printf("#############################################################\n");
    IPP_Printf("eInputYUVFmt                                %d\n", pPar->eInputYUVFmt);
    IPP_Printf("eOutputStrmFmt                              %d\n", pPar->eOutputStrmFmt);
    IPP_Printf("nWidth                                      %d\n", pPar->nWidth);
    IPP_Printf("nHeight                                     %d\n", pPar->nHeight);

    IPP_Printf("nPBetweenI                                  %d\n", pPar->nPBetweenI);
    IPP_Printf("nBFrameNum                                  %d\n", pPar->nBFrameNum);
    IPP_Printf("bRCEnable                                   %d\n", pPar->bRCEnable);
    IPP_Printf("nRCType                                     %d\n", pPar->nRCType);
    IPP_Printf("nQP                                         %d\n", pPar->nQP);
    
    IPP_Printf("nRCBitRate                                  %d\n", pPar->nRCBitRate);
    IPP_Printf("nMaxBitRate                                 %d\n", pPar->nMaxBitRate);
    IPP_Printf("nFrameRateNum                               %d\n", pPar->nFrameRateNum);
    
    IPP_Printf("bMultiIns                                   %d\n", pPar->bMultiIns);
    IPP_Printf("bFirstUser                                  %d\n", pPar->bFirstUser);
    IPP_Printf("#############################################################\n");
    IPP_Printf("#############################################################\n");
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
int ParseVmetaEncCmd(char *pCmdLine, 
                    char *pSrcFileName, 
                    char *pDstFileName, 
                    char *pLogFileName,
                    char *pRecFileName,
                    void *pParSet)
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
    IppVmetaEncParSet *pEncParSet = (IppVmetaEncParSet*)pParSet;
    IPP_FILE *fConfig;
    int rtCode;
    int nParaIdx;
    int *p;

    /*step1: initialize parameters and set default value*/
    InitVmetaEncPara(pEncParSet);

    /*step 2: read parameters setting from config file*/
    fConfig = IPP_Fopen("VmetaEncConfig.cfg", "r");
    if (NULL == fConfig){
        IPP_Printf("Can't open default config file: %s\n", "VmetaEncConfig.cfg");
    } else {
        rtCode = ReadConfigFile(fConfig);
        if (0 == rtCode){
            IPP_Printf("Incorrect parameter setting in default config file\n");
            return IPP_FAIL;
        }
        IPP_Fclose(fConfig);
    }


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
        } else if (0 == IPP_Strcmp(par_name, "lessinfo")) {
            STRNCPY(par_value, p2 + 1, par_value_len);
            bLessInfo = IPP_Atoi(par_value);
        } else if ((0 == IPP_Strcmp(par_name, "p")) || (0 == IPP_Strcmp(par_name, "P"))) {
            /*par file*/
            /*parse par file to fill pParSet*/
            STRNCPY(par_value, p2 + 1, par_value_len);
            fConfig = IPP_Fopen(par_value, "r");
            if (NULL == fConfig){
                IPP_Printf("Can't open config file: %s\n", par_value);
                return IPP_FAIL;
            } else {
                rtCode = ReadConfigFile(fConfig);
                if (0 == rtCode){
                    IPP_Printf("Incorrect parameter setting in config file\n");
                    IPP_Fclose(fConfig);
                    return IPP_FAIL;
                }
                IPP_Fclose(fConfig);
                IPP_Printf("use user-defined config file: %s\n", par_value);
            }
        } else {
            /*parse other parameters for encoder*/
            nParaIdx = GetParaIndex(par_name);
            if (VMETA_ENC_PARA_NUM > nParaIdx){
                STRNCPY(par_value, p2 + 1, par_value_len);
                p = (int *)Map[nParaIdx].p;
                *p = IPP_Atoi(par_value);
            } else {
                return IPP_FAIL;
            }
        }

        pCur = p3;
    }


    OutputVmetaPara(pEncParSet);
    return IPP_OK;
}

/*Interface for IPP sample code template*/
int CodecTest(int argc, char **argv)
{
    IPP_FILE *fpin = NULL, *fpout = NULL, *fplog = NULL;
    char input_file_name[2048]  = {'\0'};
    char output_file_name[2048] = {'\0'};
    char log_file_name[2048]    = {'\0'};
    char rec_file_name[2048]    = {'\0'};
    int rtFlag;
    Ipp8u libversion[256]         = {'\0'};

    IPP_Printf("enter CodecTest**********************************\n");
    if (2 > argc) {
        IPP_Printf("Usage: ./appVmetaEnc.exe -i:input.cmp -o:output.yuv -l:enc.log -p:xxx.cfg -lessinfo:0/1!\n");
        return IPP_FAIL;
    } else if (2 == argc){
        /*for validation*/
        IPP_Printf("Input command line: %s\n", argv[1]);
        if (IPP_OK != ParseVmetaEncCmd(argv[1], input_file_name, output_file_name, log_file_name, rec_file_name, &g_VmetaEncParSet)){
            IPP_Log(log_file_name, "w", 
                "command line is wrong! %s\nUsage: ./appVmetaEnc.exe -i:input.cmp -o:output.yuv -l:enc.log -p:xxx.cfg!\n", argv[1]);
            return IPP_FAIL;
        }
    } else {
        /*for internal debug*/
        ParseVmetaEncCmd(argv[1], input_file_name, output_file_name, log_file_name, rec_file_name, &g_VmetaEncParSet);
        IPP_Strcpy(input_file_name, argv[1]);
        IPP_Strcpy(output_file_name, argv[2]);   
    }

    IPP_Printf("input: %s\n", input_file_name);
    IPP_Printf("output: %s\n", output_file_name);
    if (0 == IPP_Strcmp(input_file_name, "\0")) {
        IPP_Log(log_file_name, "a", "input file name is null!\n");
        return IPP_FAIL;
    } else {     
        fpin = IPP_Fopen(input_file_name, "rb");
        if (!fpin) {
            IPP_Log(log_file_name, "a", "Fails to open file %s!\n", input_file_name);
            return IPP_FAIL;
        }
    }

    if (0 == IPP_Strcmp(output_file_name, "\0")) {
        IPP_Log(log_file_name, "a", "output file name is null!\n");
    } else {
        fpout = IPP_Fopen(output_file_name, "wb");
        if (!fpout) {
            IPP_Log(log_file_name, "a", "Fails to open file %s\n", output_file_name);
            return IPP_FAIL;
        }
    }

    GetLibVersion_VmetaENC(libversion,sizeof(libversion));
    IPP_Printf("lib version: %s\n", libversion);
    IPP_Log(log_file_name, "a", "command line: %s\n", argv[1]);
    IPP_Log(log_file_name, "a", "input file  : %s\n", input_file_name);
    IPP_Log(log_file_name, "a", "output file : %s\n", output_file_name);
    IPP_Log(log_file_name, "a", "begin to encode\n");
    IPP_Printf("bMultiIns : %d\n", g_VmetaEncParSet.bMultiIns);
    IPP_Printf("bFirstUser: %d\n", g_VmetaEncParSet.bFirstUser);
    rtFlag = VmetaEncoder(fpin, fpout, log_file_name, &g_VmetaEncParSet);
    if (IPP_OK == rtFlag) {
        IPP_Log(log_file_name, "a", "everything is OK!\n");
    } else {
        IPP_Log(log_file_name, "a", "encoding fail!\n");
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


