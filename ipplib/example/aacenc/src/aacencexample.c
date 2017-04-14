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


/* Standard IPP headers */
#include "ippdefs.h"
#include "codecAC.h"
#include "misc.h"
#include "ippLV.h"

#define MAX_PAR_NAME_LEN    1024
#define MAX_PAR_VALUE_LEN   2048

typedef struct
{	
	short    wFormatTag;		
	short    wChannels;		
	int   dwSamplesPerSec;	
	int   dwAvgBytesPerSec;    	
	short    wBlockAlign;		
	short    wBitsPerSample;		
}waveFormatPCM;

/****************
 True and false 
 constant
****************/ 
#define	FALSE			0		
#define	TRUE			1

/*************
 Prototypes 
**************/
void DisplayConsoleStartupBanner(void);
void	DisplayLibVersion(void);
int  ParseAACLCEncCmd(char *pCmd, char *pSrcFileName, char *pDstFileName , char *pLogFileName, int *bitrate, int *format, int *tnsFlag);
int ParseWavHdr(IPP_FILE *fpi,waveFormatPCM* pWavFormat);
int AACLCenc(char *pSrcFileName, char *pDstFileName, char *pLogFileName, int Bitrate, int Format, int TnsFlag);

/*********************************************************************************************
// Name:			 DisplayConsoleStartupBanner
//
// Description:		 Display startup message on the text console
//
// Input Arguments:  None.
//
// Output Arguments: None				
//
// Returns:			 None				
**********************************************************************************************/
void DisplayConsoleStartupBanner()
{
	IPP_Printf("\n\n**************************************************\n");
	IPP_Printf("AAC LC Encoder Demonstration, Version 0.1\n");
	IPP_Printf("ISO/IEC 13818-7 MPEG-2 AAC LC profile\n");
	IPP_Printf("ISO/IEC 14496-3	MPEG-4 AAC LC profile\n");
	IPP_Printf("**************************************************\n\n");
}

/******************************************************************************
// Name:			 DisplayLibVersion
//
// Description:		 Display library build information on the text console
//
// Input Arguments:  None.
//
// Output Arguments: None				
//
// Returns:			 None				
*******************************************************************************/
void DisplayLibVersion()
{
	char libversion[128]={'\0'};
	IppCodecStatus ret;
	ret = GetLibVersion_AACENC(libversion,sizeof(libversion));
	if(0 == ret){
		IPP_Printf("\n*****************************************************************\n");
	    IPP_Printf("This library is built from %s\n",libversion);
		IPP_Printf("*****************************************************************\n");
	}else{
		IPP_Printf("\n*****************************************************************\n");
		IPP_Printf("Can't find this library version information\n");
		IPP_Printf("*****************************************************************\n");
	}
}

/******************************************************************************
// Name:                ParseCmdTemplate
// Description:         Parse the user command 
//
// Input Arguments:
//      pCmdLine    :   Pointer to the input command line
//
// Output Arguments:
//      pSrcFileName:   Pointer to src file name
//      pDstFileName:   Pointer to dst file name
//      pLogFileName:   Pointer to log file name
//		bitrate:		bitrate of encoded stream, for 16kHz/mono/speech, 48kbps or above is preferred
//						for 16kHz/mono/audio, 64kbps or above is preferred
//		format:			MP4_ADTS or RAW data format is supported
// Returns:
//        [Success]     IPP_OK
//        [Failure]     IPP_FAIL
******************************************************************************/
int ParseAACEncCmd(char *pCmdLine, 
                     char *pSrcFileName, 
                     char *pDstFileName, 
                     char *pLogFileName, 
						int* bitrate,
						 int* format, 
						 int* tnsFlag)
{
    char *pCur, *pEnd;
    char par_name[MAX_PAR_NAME_LEN];
    int  par_name_len;
    int  par_value_len;
    char *p1, *p2, *p3;

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

        IPP_Strncpy(par_name, p1 + 1, par_name_len);
		 par_name[par_name_len] = '\0';
        if ((0 == IPP_Strcmp(par_name, "i")) || (0 == IPP_Strcmp(par_name, "I"))) {
            /*input file*/
            IPP_Strncpy(pSrcFileName, p2 + 1, par_value_len);
			  pSrcFileName[par_value_len] = '\0';
        } else if ((0 == IPP_Strcmp(par_name, "o")) || (0 == IPP_Strcmp(par_name, "O"))) {
            /*output file*/
            IPP_Strncpy(pDstFileName, p2 + 1, par_value_len);
			 pDstFileName[par_value_len] = '\0';
        } else if ((0 == IPP_Strcmp(par_name, "l")) || (0 == IPP_Strcmp(par_name, "L"))) {
            /*log file*/
            IPP_Strncpy(pLogFileName, p2 + 1, par_value_len);
			  pLogFileName[par_value_len] = '\0';
        } else if ((0 == IPP_Strcmp(par_name, "p")) || (0 == IPP_Strcmp(par_name, "P"))) {
            /*par file*/
            /*parse par file to fill pParSet*/
         } else if ((0 == IPP_Strcmp(par_name, "f")) || (0 == IPP_Strcmp(par_name, "F"))) {
            /*format type*/
            *format = IPP_Atoi(p2 + 1);
        } else if ((0 == IPP_Strcmp(par_name, "b")) || (0 == IPP_Strcmp(par_name, "B"))) {
            /*log file*/
            *bitrate = IPP_Atoi(p2 + 1);
        } else if ((0 == IPP_Strcmp(par_name, "t")) || (0 == IPP_Strcmp(par_name, "T"))) {
            /*tns flag*/
            *tnsFlag = IPP_Atoi(p2 + 1);
         } 

        pCur = p3;
    }

    return IPP_OK;
}


/*********************************************************************************************
// Name:			 ParseWavHdr
//
// Description:		 Parse wav header
//
// Input Arguments:  fpi				- wav file pointer
//
// Output Arguments: pWavFormat			- wav format including sample rate, data length, channal numbers etc. 
//
// Returns:			 None				
**********************************************************************************************/
int ParseWavHdr(IPP_FILE *fpi,waveFormatPCM* pWavFormat)
{
	Ipp8u buffer[128];
	int stopSearch = 0;
	int skipLen;
	int readByte = 0;
  
	IPP_Fread(buffer, sizeof(Ipp8u), 12, fpi);
	if((buffer[0] == 'R') && (buffer[1] == 'I') && (buffer[2] == 'F') && (buffer[3] == 'F')) {	
		readByte = IPP_Fread(buffer, sizeof(Ipp8u), 8, fpi);
	} else { 	  
		return IPP_FAIL;	  
	}
  
	if((buffer[0] == 'd') && (buffer[0] == 'a') && (buffer[0] == 't') && (buffer[0] == 'a')) {
	  stopSearch = 1;  
	} else {
		do {		  
			skipLen = buffer[4] + (buffer[5]<<8) + (buffer[6]<<16) + (buffer[7]<<24);		
  		  
			if((buffer[0] == 'd') && (buffer[1] == 'a') && (buffer[2] == 't') && (buffer[3] == 'a')) {	  
				stopSearch = 1;		  
				return IPP_OK;
			} else if((buffer[0] == 'f') && (buffer[1] == 'm') && (buffer[2] == 't')) {			
				readByte = IPP_Fread(buffer, sizeof(Ipp8u), skipLen, fpi);
				pWavFormat->wFormatTag = buffer[0] + (buffer[1] << 8);
				pWavFormat->wChannels = buffer[2] + (buffer[3] << 8);
				pWavFormat->dwSamplesPerSec = buffer[4] + (buffer[5]<<8) + (buffer[6]<<16) + (buffer[7]<<24);
				pWavFormat->dwAvgBytesPerSec = buffer[8] + (buffer[9]<<8) + (buffer[10]<<16) + (buffer[11]<<24);
				pWavFormat->wBlockAlign = buffer[12] + (buffer[13]<<8);
				pWavFormat->wBitsPerSample = buffer[14] + (buffer[15]<<8);
			} else {			
				IPP_Fseek(fpi, skipLen, IPP_SEEK_CUR);		  
			}
			IPP_Fread(buffer, sizeof(Ipp8u), 8, fpi);
		} while((!stopSearch) && (readByte == 0));  
	}
	return IPP_OK;
}




/*************************************
 Console-mode application entry point 
**************************************/
/******************************************************************************
// Name:			 main
//
// Description:		 AAC LC encoder sample code
//					 Demonstrates the Marvell Integrated Performance Primitives 
//					 (IPP) AAC LC encoder API.
//					
// Input Arguments:
//      pSrcFileName:   Pointer to src file name
//      pDstFileName:   Pointer to dst file name
//      pLogFileName:   Pointer to log file name
//		bitrate:		bitrate of encoded stream, for 16kHz/mono/speech, 48kbps or above is preferred
//						for 16kHz/mono/audio, 64kbps or above is preferred
//		format:			MP4_ADTS or RAW data format is supported
// Returns:
//        [Success]     IPP_OK
//        [Failure]     IPP_FAIL
//
******************************************************************************/

int AACLCenc(char *pSrcFileName, char *pDstFileName, char *pLogFileName, int Bitrate, int Format, int TnsFlag)
{
	IPP_FILE *fpi = NULL;
	IPP_FILE *fpo = NULL;
	IPP_FILE *logFile = NULL;
	
	int	 inputBufSize;
	Ipp8u	*pInputBuf = NULL;
	Ipp8u	*pOutputBuf = NULL;

	void *pEncoderState = NULL;
	IppSound		sound;
	IppBitstream	bitStream;
	IppAACEncoderConfig        encoderConfig;
	MiscGeneralCallbackTable	*pCallBackTable = NULL;
	Ipp8u			*pPrevBsCurByte;

	int bitrate = Bitrate;
	int format = Format;
	int tnsFlag = TnsFlag;

	int done = FALSE;							/* Main loop boolean status */
	int iResult, nReadByte;
	int frameNum = 0;
	int remainSample = 0;

	long long TotTime;
	int perf_index;
	waveFormatPCM waveFormat;
	
	DisplayConsoleStartupBanner();
	DisplayLibVersion();

	/* Open input and output files */
	if (NULL == (fpi = IPP_Fopen(pSrcFileName, "rb"))) {
		IPP_Log(pLogFileName,"a","IPP Error:can not open src file :%s\n",pSrcFileName);
		return IPP_FAIL;
	}
	
	if (NULL == (fpo = IPP_Fopen(pDstFileName, "wb"))) {
		IPP_Log(pLogFileName,"a","IPP Error:can not open dst file :%s\n",pDstFileName);
	}

	if(ParseWavHdr(fpi,&waveFormat) != 0){
		IPP_Log(pLogFileName,"a","IPP Error:invalid wave file\n");
		if(fpi != NULL) 	IPP_Fclose(fpi);
		if(fpo != NULL)		IPP_Fclose(fpo);
		return IPP_FAIL;
	}
	
	if((waveFormat.wBitsPerSample != 16) || (waveFormat.wFormatTag != 1)){
		IPP_Log(pLogFileName,"a","IPP Error:unsupported wave file\n");
		if(fpi != NULL) 	IPP_Fclose(fpi);
		if(fpo != NULL)		IPP_Fclose(fpo);
		return IPP_FAIL;
	}
	

	/* Init callback table */
	if ( miscInitGeneralCallbackTable(&pCallBackTable) != 0 ) {
		IPP_Log(pLogFileName,"a","IPP Error:init callback table failed\n");
		if(fpi != NULL) 	IPP_Fclose(fpi);
		if(fpo != NULL)		IPP_Fclose(fpo);
		return IPP_FAIL;
	}

	/* initialize configuration structure */
	encoderConfig.bitRate = bitrate;
	encoderConfig.bsFormat = format;
	encoderConfig.channelConfiguration = waveFormat.wChannels;
	encoderConfig.samplingRate = waveFormat.dwSamplesPerSec;

	IPP_Printf("sample rate %d\n",encoderConfig.samplingRate);


	/* Allocate the input stream buffer */
	inputBufSize = AAC_FRAME_SIZE * sizeof(Ipp16s) * waveFormat.wChannels;		/* 16 bit PCM sample */
	IPP_MemMalloc(&pInputBuf,inputBufSize,4);
	IPP_Memset(pInputBuf,0,inputBufSize);

	if (NULL == pInputBuf) {
		IPP_Log(pLogFileName,"a","IPP Error:malloc SrcBitStream failed\n");
		if(fpi != NULL) 	IPP_Fclose(fpi);
		if(fpo != NULL)		IPP_Fclose(fpo);
		return IPP_FAIL;
	}
	sound.pSndFrame = (Ipp16s*)pInputBuf;
	sound.sndChannelNum =  waveFormat.wChannels;
	sound.sndLen = inputBufSize;	

	/* Allocate the output buffer */
	bitStream.bsByteLen = sizeof(Ipp16s) * AAC_FRAME_SIZE * encoderConfig.channelConfiguration;
	IPP_MemMalloc(&pOutputBuf, bitStream.bsByteLen, 4);
	IPP_Memset(pOutputBuf,0,bitStream.bsByteLen);
	if(NULL == pOutputBuf) {
		IPP_Log(pLogFileName,"a","IPP Error:malloc DstBitStream failed\n");
		if(fpi != NULL) 	IPP_Fclose(fpi);
		if(fpo != NULL)		IPP_Fclose(fpo);
		return IPP_FAIL;
	}
	bitStream.pBsBuffer = pOutputBuf;
	bitStream.pBsCurByte = pOutputBuf;
	bitStream.bsCurBitOffset = 0;

	/* Initialize AAC LC encoder */
	iResult = EncoderInitAlloc_AAC(&encoderConfig, pCallBackTable, &pEncoderState);
	if(iResult != IPP_STATUS_INIT_OK) {
		IPP_Log(pLogFileName,"a","IPP Error: AAC LC encoder initialization failed !!!\n");
		EncoderFree_AAC(&pEncoderState);
		miscFreeGeneralCallbackTable(&pCallBackTable);
		if(NULL != pInputBuf)
			IPP_MemFree(&pInputBuf);
		if(NULL != pOutputBuf)
			IPP_MemFree(&pOutputBuf);
		if(fpi != NULL) 	IPP_Fclose(fpi);
		if(fpo != NULL)		IPP_Fclose(fpo);
		return IPP_FAIL;
	}

	/* Read source stream */
	nReadByte = (int)IPP_Fread(sound.pSndFrame,1,AAC_FRAME_SIZE * sizeof(Ipp16s) * sound.sndChannelNum,fpi);
	if(nReadByte<AAC_FRAME_SIZE * sizeof(Ipp16s) * sound.sndChannelNum) {
		IPP_Log(pLogFileName,"a","IPP Error:Insufficient input data!!!\n");
		if(fpi != NULL) 	IPP_Fclose(fpi);
		if(fpo != NULL)		IPP_Fclose(fpo);
		IPP_MemFree(&(sound.pSndFrame));
		return IPP_FAIL;
	}
	/* Set TNS mode. TNS will improve the quality for transient and some speech cases but will also consume more power */
	if(TnsFlag == 1) {
		iResult = EncodeSendCmd_AAC(IPPAC_AAC_SET_TNS_MODE, &TnsFlag, NULL, pEncoderState);
		if(iResult != IPP_STATUS_NOERR) {
			return IPP_FAIL;
		}
	}

	/* initialize for performance data collection */
	IPP_GetPerfCounter(&perf_index, DEFAULT_TIMINGFUNC_START,DEFAULT_TIMINGFUNC_STOP);
	IPP_ResetPerfCounter(perf_index);
	
	remainSample = 1600;
	while (!done)
	{
		pPrevBsCurByte = bitStream.pBsCurByte;

		IPP_StartPerfCounter(perf_index);
		iResult = Encode_AAC(&sound, &bitStream, pEncoderState);
		IPP_StopPerfCounter(perf_index);

		switch (iResult)
		{
		    case IPP_STATUS_FRAME_COMPLETE:
		    case IPP_STATUS_NOERR:
			    frameNum ++;
				nReadByte = (int)IPP_Fread(sound.pSndFrame,1,AAC_FRAME_SIZE * sizeof(Ipp16s) * sound.sndChannelNum,fpi);

			    if(nReadByte<AAC_FRAME_SIZE * sizeof(Ipp16s) * sound.sndChannelNum) {
				    /* padding zero to the end of last frame */
				    IPP_Memset(sound.pSndFrame + nReadByte/ sizeof(Ipp16s), 0, (sound.sndLen - nReadByte));
				    if((nReadByte == 0) && (remainSample <= 0)) {				    
				      done = TRUE;
			      }
			    }
			    remainSample += nReadByte / (sizeof(Ipp16s) * sound.sndChannelNum) - 1024;
			    if(fpo != NULL) IPP_Fwrite(pPrevBsCurByte, 1,(bitStream.pBsCurByte-pPrevBsCurByte),fpo);
			     break;
		    case IPP_STATUS_OUTPUT_DATA:
			    bitStream.pBsCurByte = bitStream.pBsBuffer;
			    bitStream.bsCurBitOffset = 0;
			    break;
		    default:
			    done = TRUE;
			    break;
		    }
	    }

 	/* log codec performance and exit performance counting */
	TotTime = IPP_GetPerfData(perf_index);
	
	IPP_FreePerfCounter(perf_index);

	g_Frame_Num[IPP_AUDIO_INDEX] = frameNum;
	g_Tot_Time[IPP_AUDIO_INDEX] = TotTime;
	IPP_Log(pLogFileName,"a","IPP Status:Encoding success!\n");

	IPP_Printf("IPP AAC LC encoder: total cost %lld us, %d frames, duration %f sec\n", TotTime, frameNum, frameNum*1024./(encoderConfig.samplingRate));//for AAC LC, each frame has 1024 sample


	/* AAC LC encoder free*/
	if(NULL != pInputBuf)
		IPP_MemFree(&pInputBuf);
	if(NULL != pOutputBuf)
		IPP_MemFree(&pOutputBuf);

	EncoderFree_AAC(&pEncoderState);
	miscFreeGeneralCallbackTable(&pCallBackTable);

	/* Close file handler */
	if(fpi != NULL) 	IPP_Fclose(fpi);
	if(fpo != NULL)		IPP_Fclose(fpo);

	return IPP_OK;
} /* End of AACLCenc() */



/******************************************************************************
// Name:				CodecTest
// Description:			Main entry for PNG decoder test
//
// Input Arguments:
//						N/A
// Output Arguments:	
//						N/A
// Returns:
******************************************************************************/
int CodecTest(int argc, char** argv)
{
	char pSrcFileName[256];
	char pDstFileName[256];
	char pLogFileName[256];
	int Bitrate = 64000;
	int Format = AAC_SF_MP4ADTS;
	int TnsFlag = 0;
    
	IPP_Memset(pSrcFileName, 0x0, 256);
	IPP_Memset(pDstFileName, 0x0, 256);
	IPP_Memset(pLogFileName, 0x0, 256);
	if(argc == 2 && ParseAACEncCmd(argv[1],pSrcFileName, pDstFileName, pLogFileName, &Bitrate, &Format, &TnsFlag) == 0){
		return(AACLCenc(pSrcFileName, pDstFileName, pLogFileName, Bitrate, Format, TnsFlag));
	}
	else
	{
		IPP_Printf("Command is incorrect!\n         \
			 Usage:appAACLCenc.exe \"-i:t.wav -o:t.aac -l:log.txt -b:64000 -f:1\"	\n \
			    -i: <input file> is the name of a valid .wav file \n \
			    -o: <output file> is the desired aac output file \n \
			    -l: <log file> is the log information file \n			\
			    -b: user defined bitrate 	\n	\
			    -f:  user defined output format. 1 for ADTS, 6 for RAW data		\n	\
				-t: user defined TNS status. 0 for TNS disable(default), 1 for TNS enable		\n	\
		");
		return IPP_FAIL;
	}
}
/* EOF */