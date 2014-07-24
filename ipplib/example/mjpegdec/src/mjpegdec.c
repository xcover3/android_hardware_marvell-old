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


#include "mjpegdec.h"
#include "misc.h"
#include "codecJP.h"


int JPEGSubSamplingTab[][2] = 
{
	/* width, height */
	{0, 0}, /* 444  */
	{1, 0}, /* 422, horizontal sub-sampling  */
	{0, 1}, /* 422I,vertical sub-sampling */
	{1, 1}, /* 411  */
};

int JPEGBitsPerPixel[] = 
{
    24, /* JPEG_BGR888 */
    16, /* JPEG_BGR555 */
    16, /* JPEG_BGR565 */
    32, /* JPEG_ABGR */
};


/*************************************************************************************************************
// Name:      appiDecodeJPEGFromAVI
// Description:
//      Decoder JPEG bit streams
// Input Arguments:
//      pBsStart        - Pointer to the start of the bit stream
//      pDstAVIPicture  - Pointer to the AVI picture structure
//      iBsLen          - Length of the bit stream
//      dstFile         - Pointer to the output file 
// Output Arguments:
//      None
// Returns:
//      IPP_STATUS_NOERR     - OK
//      IPP_STATUS_ERR       - Error  
//      IPP_STATUS_NOMEM_ERR - Memory alloc error
// Others:
//      None
*************************************************************************************************************/
IppCodecStatus appiDecodeJPEGFromAVI
                            ( Ipp8u * pBsStart,
                            Ipp32u iBsLen,
                            IppPicture *pDstAVIPicture,
                            void * dstFile)
{
    IppPicture DstPicture;
    IppBitstream SrcBitStream;  
    void * pDecoderState = NULL;
    MiscGeneralCallbackTable *pCallBackTable = NULL;
    int iOutBufSize;
    int nDstWidth, nDstHeight, nChromaSize = 0;
    Ipp8u *pYUVPtr = NULL;
    IppCodecStatus rtCode = IPP_STATUS_NOERR;
    IppJPEGDecoderParam DecoderPar;
    int mode  = 0;

    /* Init callback table */
    if ( miscInitGeneralCallbackTable(&pCallBackTable) != 0 ) {
        return IPP_STATUS_ERR; 
    }

    SrcBitStream.pBsBuffer = pBsStart;
    SrcBitStream.bsByteLen = iBsLen;
    SrcBitStream.pBsCurByte = pBsStart;
    SrcBitStream.bsCurBitOffset = 0;


    DecoderPar.srcROI.x = 0;
    DecoderPar.srcROI.y = 0;
    DecoderPar.srcROI.width = pDstAVIPicture->picWidth;
    DecoderPar.srcROI.height = pDstAVIPicture->picHeight;
    DecoderPar.nScale = 1*65536; /* no scale */
    DecoderPar.nDesiredColor = JPEG_YUV411;
    DecoderPar.nAlphaValue = 8;


    /* Init JPEG Decoder */
    rtCode = DecoderInitAlloc_JPEG (&SrcBitStream, &DstPicture, pCallBackTable, &pDecoderState, NULL);

    if(rtCode!=IPP_STATUS_NOERR){			
        if(NULL!=pDecoderState){
            DecoderFree_JPEG(&pDecoderState);
            pDecoderState = NULL;
        }
        if(NULL!=pCallBackTable){
            miscFreeGeneralCallbackTable(&pCallBackTable);
            pCallBackTable = NULL;
        }
        return rtCode;
    }
    /* Scaled width/height */
    nDstWidth = DstPicture.picWidth = ((DecoderPar.srcROI.width << 16) + DecoderPar.nScale - 1)/DecoderPar.nScale;
    nDstHeight = DstPicture.picHeight = ((DecoderPar.srcROI.height << 16) + DecoderPar.nScale - 1)/DecoderPar.nScale;

    /* Output the original compressed format */
    DecoderPar.nDesiredColor = DstPicture.picFormat;


    /* JPEG decoding */

    /* YUV space */
    mode = DecoderPar.nDesiredColor - JPEG_YUV444;
    if((DecoderPar.nDesiredColor == JPEG_YUV422) || (DecoderPar.nDesiredColor == JPEG_YUV411)){
        nDstWidth = ((nDstWidth >> 1) << 1);
        DstPicture.picWidth = nDstWidth;
    }

    if((DecoderPar.nDesiredColor == JPEG_YUV422I) || (DecoderPar.nDesiredColor == JPEG_YUV411)){
        nDstHeight = ((nDstHeight >> 1) << 1);
        DstPicture.picHeight = nDstHeight;
    }
    if(DstPicture.picChannelNum==3) {
        nChromaSize = (nDstWidth>>JPEGSubSamplingTab[mode][0]) * (nDstHeight>>JPEGSubSamplingTab[mode][1]);
    } else {
        nChromaSize = 0;
    }


    iOutBufSize = nDstWidth*nDstHeight + nChromaSize*2;

    if(NULL==pYUVPtr ){
        pCallBackTable->fMemCalloc(&pYUVPtr, iOutBufSize, 8);
        if(NULL==pYUVPtr){
            if(NULL!=pDecoderState){
                DecoderFree_JPEG(&pDecoderState);
                pDecoderState = NULL;
            }
            if(NULL!=pCallBackTable){
                miscFreeGeneralCallbackTable(&pCallBackTable);
                pCallBackTable = NULL;
            }
            return IPP_STATUS_NOMEM_ERR;
        }
    }

    DstPicture.picWidth  = nDstWidth;
    DstPicture.picHeight = nDstHeight;
    DstPicture.picPlaneStep[0] = nDstWidth;
    DstPicture.ppPicPlane[0]   = pYUVPtr;
    if(DstPicture.picChannelNum==3) {
        DstPicture.picPlaneStep[1] = ((nDstWidth -1 + (1 << JPEGSubSamplingTab[mode][0]))>>JPEGSubSamplingTab[mode][0]);
        DstPicture.picPlaneStep[2] = ((nDstWidth - 1 +(1 << JPEGSubSamplingTab[mode][0]))>>JPEGSubSamplingTab[mode][0]);
        DstPicture.ppPicPlane[1]   = (Ipp8u*)DstPicture.ppPicPlane[0] + nDstWidth*nDstHeight;
        DstPicture.ppPicPlane[2]   = (Ipp8u*)DstPicture.ppPicPlane[1] + nChromaSize;  
    }

    /* Call the JPEG decoder function */      
    rtCode = Decode_JPEG (&DstPicture, &DecoderPar, pDecoderState);

    if(rtCode!=IPP_STATUS_NOERR) {				
        if(NULL!=pYUVPtr){
            pCallBackTable->fMemFree(&pYUVPtr);
            pYUVPtr = NULL;
        }
        if(NULL!=pDecoderState){
            DecoderFree_JPEG(&pDecoderState);
            pDecoderState = NULL;
        }
        if(NULL!=pCallBackTable){
            miscFreeGeneralCallbackTable(&pCallBackTable);
            pCallBackTable = NULL;
        }
        return rtCode;
    }

    /* Output the YUV data */
    if(NULL!=dstFile){
        IPP_Fwrite(DstPicture.ppPicPlane[0], 1, nDstWidth*nDstHeight + nChromaSize*2, dstFile);
    }

    /* JPEG Free */
    if(NULL!=pYUVPtr){
        pCallBackTable->fMemFree(&pYUVPtr);
        pYUVPtr = NULL;
    }
    if(NULL!=pDecoderState){
        DecoderFree_JPEG(&pDecoderState);
        pDecoderState = NULL;
    }
    if(NULL!=pCallBackTable){
        miscFreeGeneralCallbackTable(&pCallBackTable);
        pCallBackTable = NULL;
    }

    return IPP_STATUS_NOERR;
}

/* EOF */