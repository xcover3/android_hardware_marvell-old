/******************************************************************************
// INTEL CONFIDENTIAL
// Copyright 2000 ~ 2006 Intel Corporation All Rights Reserved. 
// The source code contained or described herein and all documents related to 
// the source code (¡°Material¡±) are owned by Intel Corporation or its 
// suppliers or licensors. Title to the Material remains with Intel Corporation
// or its suppliers and licensors. The Material contains trade secrets and 
// proprietary and confidential information of Intel or its suppliers and 
// licensors. The Material is protected by worldwide copyright and trade secret 
// laws and treaty provisions. No part of the Material may be used, copied, 
// reproduced, modified, published, uploaded, posted, transmitted, distributed, 
// or disclosed in any way without Intel¡¯s prior express written permission.
//
// No license under any patent, copyright, trade secret or other intellectual 
// property right is granted to or conferred upon you by disclosure or delivery 
// of the Materials, either expressly, by implication, inducement, estoppel or 
// otherwise. Any license under such intellectual property rights must be 
// express and approved by Intel in writing.
******************************************************************************/
//#include <windows.h>
//#include <commctrl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codecMD.h"
#include "events.h"

/**********************************************************/
/* WAVETABLE CALLBACK IMPLEMENTATIONS */
/**********************************************************/
IppCodecStatus myOpenWT(void *pSrcStreamID, void **pDstStreamHandle, const char *mode)
{
	FILE *fp = NULL;

	fp = fopen((char*)pSrcStreamID, mode);
	
	if (fp)
	{
		*pDstStreamHandle = (void*)fp;
		return IPP_STATUS_NOERR;
	}
	else
	{
		/* it must be assigned a null pointer value if stream open fails */
		*pDstStreamHandle = NULL;
		printf("Open wavetable %s failed!\n", (char*)pSrcStreamID);
		return IPP_STATUS_ERR;
	}
}

IppCodecStatus myCloseWT(void *pSrcDstStreamHandle)
{
	if (fclose((FILE*)pSrcDstStreamHandle))
	{
		printf("Close wavetable failed!\n");
		return IPP_STATUS_ERR;
	}
	else
	{
		return IPP_STATUS_NOERR;
	}	
}

IppCodecStatus mySeekWT(void *pSrcDstStreamHandle, long offset, int origin)
{
	int	result = 0;
	switch(origin)
	{
		case IPP_MIDI_CB_SEEK_SET:	
			result = fseek((FILE*)pSrcDstStreamHandle, offset, SEEK_SET);
			break;
		case IPP_MIDI_CB_SEEK_CUR:	
			result = fseek((FILE*)pSrcDstStreamHandle, offset, SEEK_CUR);
			break;
		case IPP_MIDI_CB_SEEK_END:	
			result = fseek((FILE*)pSrcDstStreamHandle, offset, SEEK_END);
			break;
		default:
			printf("Unknown wavetable seek origin argument!\n");
			return IPP_STATUS_ERR;
	}

	if (result)
	{
		printf ("Seek wavetable failed!\n");
		return IPP_STATUS_ERR;
	}
	else
	{
		return IPP_STATUS_NOERR;
	}
}

IppCodecStatus myReadWT(void *pDstBuf, int size, int *pSrcDstCount, void *pSrcDstStreamHandle)
{
	int actual_count = 0;

	actual_count  = fread(pDstBuf, size, *pSrcDstCount, (FILE*)pSrcDstStreamHandle);
	
	if (actual_count < *pSrcDstCount)
	{
		*pSrcDstCount = actual_count;
		return IPP_STATUS_ERR;
	}
	else
	{
		*pSrcDstCount = actual_count;
		return IPP_STATUS_NOERR;
	}	
}

IppCodecStatus myTellWT(void *pSrcStreamHandle, long *pDstCurLoc)
{
	*pDstCurLoc = ftell((FILE*)pSrcStreamHandle);

	if (*pDstCurLoc == -1L)
	{
		return IPP_STATUS_ERR;
	}
	else
	{
		return IPP_STATUS_NOERR;
	}
}



/**********************************************************/
/* MEMORY CALLBACK IMPLEMENTATIONS */
/**********************************************************/
IppCodecStatus myMalloc(void **ppDstBuf, int size, IppMidiMemShareAttributes
						*pSrcDstMemShareAttribute)
{
	/* initialized to null pointer value */
	*ppDstBuf = NULL;

	/* the memory block can be shared by multiple processes */
	if (pSrcDstMemShareAttribute)
	{
		/* extra shared memory attach or map codes can be added here */
		*ppDstBuf = malloc(size);
		/* status must be filled to indicate whether it's newly allocated */
		pSrcDstMemShareAttribute->shmStatus = IPP_MIDI_MEM_CREATED;
	}
	/* the memory block is private to the calling process */
	else
	{
		/* simply allocate memory */
		*ppDstBuf = malloc(size);
	}
	
	if ((*ppDstBuf) == NULL)
	{
		printf("Insufficient memory available!\n");
		return IPP_STATUS_ERR;
	}
	else
	{
		return IPP_STATUS_NOERR;
	}
}

IppCodecStatus myFree(void *pSrcBuf, IppMidiMemShareAttributes *pSrcDstMemShareAttribute)
{
	/* the memory block can be shared by multiple processes */
	if (pSrcDstMemShareAttribute)
	{
		/* extra shared memory detach or unmap codes can be added here */
		free(pSrcBuf);
	}
	/* the memory block is private to the calling process */
	else
	{
		free(pSrcBuf);
	}
	
	return IPP_STATUS_NOERR;
}

int ParseMidiCommandLine(int argc, char **argv, IppMidiDecoderParams *pMidiConfig,
						 IppMidiStream *pMidiFile, IppMidiStream *pWaveTable)
{
	int	 i, j;
	FILE *fpin = NULL;	
	
	/* argument check */
	if (argc < 3)
	{
		printf("Usage: midi_wmmx -inf infile -outf outfile  [-options]\n"
			   "       infile :  in midi file decoding, standard .mid input filename\n"
			   "                 in midi events decoding, no .mid input filename\n"
			   "       outfile:  pcm stream output filename\n"
			   "       options:	 -rv enable reverb effects;\n"
			   "                 -cr enable chorus effects;\n"
			   "                 -mv xx set master volume (xx: 0 -- 127);\n"
			   "                 -f  xx set frames per batch (xx: 1,2,3...);\n"
			   "                 -p  xx set polyphony (xx: 24 -- 96);\n"
			   "                 -b  xx set bits per sample (xx: 8 or 16);\n"
			   "                 -m  xx set channel mode (xx: 1 mono or 2 stereo);\n"
			   "                 -r  xx set sampling rate (xx: 22050 or 44100);\n"
			   "                 -q  xx select table quality (xx: 1 lower or 2 higher);\n"
			   "                 -h  for help information;\n");
		return -1;
	}

	/* if decoding midi file, dump whole midi file into memory */
	if(!strcmp(argv[1], "-inf"))
	{
		i = 5;
		pMidiFile->mode = IPP_MIDI_STREAM_MEMRES;
		fpin = fopen(argv[2], "rb");
		if (!fpin)
		{
			printf("Open input midi file error!\n");
			return -1;
		}
		fseek(fpin, 0, SEEK_END);
		pMidiFile->bufLen		= ftell(fpin);
		pMidiFile->pBuf			= malloc(pMidiFile->bufLen);
		pMidiFile->pCurByte	    = pMidiFile->pBuf;
		pMidiFile->curBitOffset = 0;
		fseek(fpin, 0, SEEK_SET);
		fread(pMidiFile->pBuf, 1, pMidiFile->bufLen, fpin);
		fclose(fpin);
	}else
	{
		i = 3;
	}

	/* wave table initialization */
	pWaveTable->mode = IPP_MIDI_STREAM_CB;
	pWaveTable->pId  = malloc(80);
	sprintf((char*)pWaveTable->pId, "SMALLP.BNK");

	/* default setting */
	pMidiConfig->chorusEnable	   = 0;
	
	pMidiConfig->numFramesPerBatch = 1;

	pMidiConfig->pcmBitsPerSample  = IPP_MIDI_16BPS;
	pMidiConfig->pcmMode		   = IPP_MIDI_STEREO;
	pMidiConfig->pcmSampleRate	   = IPP_MIDI_22050_HZ;
	pMidiConfig->polyphony		   = 24;
	pMidiConfig->reverbEnable	   = 0;
	pMidiConfig->volL			   = 127;
	pMidiConfig->volR			   = 127;

	/* callback functions */
	pMidiConfig->callbackTable.midiOpenWT   = myOpenWT;
	pMidiConfig->callbackTable.midiCloseWT  = myCloseWT;
	pMidiConfig->callbackTable.midiReadWT   = myReadWT;
	pMidiConfig->callbackTable.midiSeekWT	= mySeekWT;
	pMidiConfig->callbackTable.midiTellWT	= myTellWT;
	
	pMidiConfig->callbackTable.midiMemAlloc = myMalloc;
	pMidiConfig->callbackTable.midiMemFree  = myFree;
	
	pMidiConfig->callbackTable.midiOpenMF   = NULL;
	pMidiConfig->callbackTable.midiCloseMF  = NULL;
	pMidiConfig->callbackTable.midiReadMF   = NULL;
	pMidiConfig->callbackTable.midiSeekMF	= NULL;
	pMidiConfig->callbackTable.midiTellMF	= NULL;

	for (; i<argc; i++)	
	{
		if (!strcmp(argv[i], "-cr"))
		{
			pMidiConfig->chorusEnable = 1;			
		}
		else if (!strcmp(argv[i], "-rv"))
		{
			pMidiConfig->reverbEnable = 1;			
		}
		else if (!strcmp(argv[i], "-mv"))
		{
			pMidiConfig->volL = pMidiConfig->volR = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "-f"))
		{
			pMidiConfig->numFramesPerBatch = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "-p"))
		{
			pMidiConfig->polyphony = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "-b"))
		{
			j = atoi(argv[++i]);

			if (j == 16)
			{
				pMidiConfig->pcmBitsPerSample  = IPP_MIDI_16BPS;
			}
			else if (j == 8)
			{
				pMidiConfig->pcmBitsPerSample  = IPP_MIDI_8BPS;
			}
			else
			{
				printf("Warning: bit per sample should be either 8 or 16!\n");
				return -1;
			}
		}
		else if (!strcmp(argv[i], "-m"))
		{
			j = atoi(argv[++i]);
			
			if (j == 1)
			{
				pMidiConfig->pcmMode = IPP_MIDI_MONO;
			}
			else if (j == 2)
			{
				pMidiConfig->pcmMode = IPP_MIDI_STEREO;
			}
			else
			{
				printf("Warning: channel mode should be either 1 (mono) or 2 (stereo)!\n");
				return -1;
			}	
		}
		else if (!strcmp(argv[i], "-r"))
		{
			j = atoi(argv[++i]);

			if (j == 22050)
			{
				pMidiConfig->pcmSampleRate = IPP_MIDI_22050_HZ;
			}
			else if (j == 44100)
			{
				pMidiConfig->pcmSampleRate = IPP_MIDI_44100_HZ;
			}
			else
			{
				printf("Warning: sampling rate should be either 22050Hz or 44100Hz!\n");
				return -1;
			}
		}
		else if (!strcmp(argv[i], "-q"))	/* define wavetable*/
		{
			j = atoi(argv[++i]);
			
			if (j == 1)   /*wavetable can be sorted as 2 types ('1' and '2') according to quality*/		
			{
				sprintf((char*)pWaveTable->pId, "SMALLP.BNK");
			}
			else if (j == 2)
			{
				sprintf((char*)pWaveTable->pId, "LARGEP.BNK");
			}
			else
			{
				printf("Warning: table quality should be either 1 (lower) or 2 (higher)!\n");
				return -1;
			}
		}
		else
		{
			printf("Usage: midi_wmmx -inf infile -outf outfile  [-options]\n"
				   "       infile :  in midi file decoding, standard .mid input filename\n"
				   "                 in midi events decoding, no .mid input filename\n"
				   "       outfile:  pcm stream output filename\n"
				   "       options:	 -rv enable reverb effects;\n"
				   "                 -cr enable chorus effects;\n"
				   "                 -mv xx set master volume (xx: 0 -- 127);\n"
				   "                 -f  xx set frames per batch (xx: 1,2,3...);\n"
				   "                 -p  xx set polyphony (xx: 24 -- 96);\n"
				   "                 -b  xx set bits per sample (xx: 8 or 16);\n"
				   "                 -m  xx set channel mode (xx: 1 mono or 2 stereo);\n"
				   "                 -r  xx set sampling rate (xx: 22050 or 44100);\n"
				   "                 -q  xx select table quality (xx: 1 lower or 2 higher);\n"
				   "                 -h  for help information;\n");
			return -1;
			
		}
	}

	return 0;
}

int CodecTest(int argc, char **argv)
{
	void*					pMidiDecoder;		
	IppMidiDecoderParams	midiConfig;			
	IppMidiStream			midiFile;
	IppMidiStream			wavetable;
	IppMidiOutSound			pcmAudio;

	/*for midi event decoding*/
	int i;
	IppMidiEvent nextEvent, curEvent;
	IppMidiEventSynParams EventParms;
	
	FILE					*fpout = NULL;
	int						done_midi = 0;
	IppCodecStatus			retcode;

    int flag_file = 1;

#ifdef PROFILING
    DWORD dwMSDelta = 0, dwMSDec = 0;
#endif

	if (ParseMidiCommandLine(argc, argv, &midiConfig, &midiFile, &wavetable))
	{
		return -1;
	}

	if(!strcmp(argv[1], "-outf"))
	{
		flag_file = 0;
		EventParms.tempo = 500000;
		EventParms.division = 0x0078;
		fpout = fopen(argv[2], "wb");
	}else if(!strcmp(argv[1], "-inf"))
	{
		if(!strcmp(argv[3], "-outf"))
			fpout = fopen(argv[4], "wb");	
	}

	if(!fpout)
	{
		printf("Open output pcm file error!\n");	
		if(!strcmp(argv[1], "-inf"))
			free(midiFile.pBuf);
		free(wavetable.pId);
		return -1;
	}


	if (DecoderInitAlloc_MIDI(&wavetable, &pcmAudio, &midiConfig, &pMidiDecoder)
		!= IPP_STATUS_NOERR)
	{
		printf("Open wavetable error!\n");
		fclose(fpout);
		free(midiFile.pBuf);
		free(wavetable.pId);
		return -1;
	}

	/* decoding midi file */
	if(flag_file)
	{
		if (DecoderOpenStream_MIDI(&midiFile, &midiConfig, pMidiDecoder)
			!= IPP_STATUS_NOERR)
		{
			printf("Decoder init error!\n");
			DecoderFree_MIDI(&wavetable, &midiConfig, &pMidiDecoder);
			fclose(fpout);
			free(midiFile.pBuf);
			free(wavetable.pId);
			return -1;
		}

		while(!done_midi)
		{

#ifdef PROFILING
        dwMSDelta = GetTickCount();
#endif
        retcode = DecodeStream_MIDI(&midiFile, &pcmAudio, &wavetable,
			&midiConfig, pMidiDecoder);
#ifdef  PROFILING
        dwMSDec += GetTickCount() - dwMSDelta;
#endif
		switch (retcode)
		{
			case IPP_STATUS_FRAME_COMPLETE:
#ifndef PROFILING
				fwrite(pcmAudio.pSndFrame, 1, pcmAudio.sndLen, fpout);
#endif
				break;
			case IPP_STATUS_BS_END:
#ifndef PROFILING
				fwrite(pcmAudio.pSndFrame, 1, pcmAudio.sndLen, fpout);
#endif
					done_midi = 1;
					break;

				case IPP_STATUS_ERR:
					printf("Decoding error!\n");
					done_midi = 1;
					break;
				default:
					done_midi = 1;
					break;
			}		
			
		}

#ifdef PROFILING
        printf("Total decoding time is: %d ms\n", dwMSDec);
#endif

		DecoderCloseStream_MIDI(&midiFile, &midiConfig, pMidiDecoder);
		free(midiFile.pBuf);
	
	}else		/*decoding midi events*/
	{
		if(DecoderEventInit_MIDI(&EventParms, pMidiDecoder) != IPP_STATUS_NOERR)
		{
			printf("Decoder event init error!\n");
			DecoderFree_MIDI(&wavetable, &midiConfig, &pMidiDecoder);
			fclose(fpout);
			free(midiFile.pBuf);
			free(wavetable.pId);
			return -1;	
		}

		/* getting the first event */
		curEvent = midievents[0];

		/* parsing every events and synthesizing them*/
		for(i=1; i<76; i++)
		{
			nextEvent = midievents[i];
			
			curEvent.deltaTime = nextEvent.deltaTime - curEvent.deltaTime;
			
			while(1)
			{
				retcode = DecodeEvent_MIDI(curEvent, &pcmAudio, &wavetable, &midiConfig, 
				pMidiDecoder);
				
				if(retcode == IPP_STATUS_FRAME_COMPLETE)
					fwrite(pcmAudio.pSndFrame, 1, pcmAudio.sndLen, fpout);
				else if(retcode == IPP_STATUS_READEVENT)
					break;
				else
				{
					printf("Decoding error!\n");
					break;
				}

			}
			curEvent = nextEvent;
		}

	}
	
	DecoderFree_MIDI(&wavetable, &midiConfig, &pMidiDecoder);
	fclose(fpout);
	free(wavetable.pId);

	return 0;
}

int main(int argc, char* argv[])
{
	char *arg[20];
	char buffer[1024];
	char *p;
	int	 argNum=1;
	int  len = 0;

	FILE *pfConfig = fopen("midi.cfg","rb");
	
	if (!pfConfig) {
		printf ("Failed to open midi.cfg\n");
		return -1;
	}

	memset(buffer,0,1024);
	fread(buffer,1024, 1, pfConfig);
	p = buffer;

	while( *p!= NULL){
		len = 0;		
		while (*p != ' ' && *p != NULL){
			len++;
            		p++;
		}
	       	arg[argNum] = malloc(len+1);
		memset(arg[argNum],0,len+1);
		strncpy(arg[argNum],p-len,len);
		arg[argNum][len] = '\0';
		argNum++;
        	if (*p != NULL) p++;
	}

	CodecTest(argNum, &arg[0]);

	while (--argNum!=0){
	        free(arg[argNum]);
	}
	fclose(pfConfig);
	return 0;
}
