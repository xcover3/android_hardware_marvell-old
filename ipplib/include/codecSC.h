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


#ifndef _CODECSC_H_
#define _CODECSC_H_

/* Include Intel IPP external header file(s). */
#include "codecDef.h"	/* General Codec external header file*/
#include "ippSC.h"		/* Speech Codec IPP external header file*/


#ifdef __cplusplus
extern "C" {
#endif

#ifndef _CALLBACK
#define _CALLBACK __STDCALL
#endif


/***** Data Types, Data Structures and Constants ******************************/

/***** Codec-specific Definitions *****/
/* Speech Codecs */
//***********************
// IPP GSM-AMR constants
//***********************/
#define		IPP_GSMAMR_FRAME_LEN		160		/* GSM-AMR frame length, in terms of PCM samples */
#define		IPP_GSMAMR_SUBFRAME_LEN		40		/* GSM-AMR subframe length, in terms of PCM samples */
#define		IPP_GSMAMR_NUM_SUBFRAME		4		/* GSM-AMR number of subframes contained in one frame */
#define		IPP_GSMAMR_LPC_ORDER		10		/* GSM-AMR LPC analysis order */
#define		IPP_GSMAMR_MAXLAG			143		/* GSM-AMR maximum pitch lag */

/**************************************
// IPP GSM-AMR bitstream buffer lengths
***************************************/
#define		IPP_GSMAMR_BITSTREAM_LEN_475	15	/* GSM-AMR 4.75 kbps minimum packed frame buffer byte allocation */
#define		IPP_GSMAMR_BITSTREAM_LEN_515	16	/* GSM-AMR 5.15           "             "             "          */
#define		IPP_GSMAMR_BITSTREAM_LEN_59		18	/* GSM-AMR 5.9            "	            "             "          */
#define		IPP_GSMAMR_BITSTREAM_LEN_67		20	/* GSM-AMR 6.7            "	            "             "          */   
#define		IPP_GSMAMR_BITSTREAM_LEN_74		22	/* GSM-AMR 7.4            "	            "             "          */       
#define		IPP_GSMAMR_BITSTREAM_LEN_795	23	/* GSM-AMR 7.95           "	            "             "          */           
#define		IPP_GSMAMR_BITSTREAM_LEN_102	29	/* GSM-AMR 10.2           "	            "             "          */               
#define		IPP_GSMAMR_BITSTREAM_LEN_122	34	/* GSM-AMR 12.2           "	            "             "          */     
#define		IPP_GSMAMR_BITSTREAM_LEN_DTX	8	/* SID/SID UPDATE         "	            "             "          */     
#define		IPP_GSMAMR_DECODER_TV_LEN		250	/* GSM-AMR decoder test vector length, 16-bit words (3GPP unpacked format) */

/**************************************
// AMR-WB constants
***************************************/
#define		IPP_AMRWB_FRAMELENGTH			320	/* AMR wideband frame length */


/**************************************
// 13K QCELP constants
***************************************/
#define IPP_QCELP_FRAME_LEN   160/* Overall frame size */

/***************************************
// IPP GSM-AMR enumerated RX frame types 
***************************************/
typedef enum
{
	RX_SPEECH_GOOD = 0, 
	RX_SPEECH_PROBABLY_DEGRADED, 
	RX_SPARE,
	RX_SPEECH_BAD, 
	RX_SID_FIRST, 
	RX_SID_UPDATE, 
	RX_SID_BAD, 
	RX_NO_DATA,
	RX_N_FRAMETYPES,
	RX_SPEECH_LOST,

	IPPGSMAMRRXFRAMETYPE_IPPENUM_FORCE32BIT_RESERVE = 0x7fffffff
} IppGSMAMRRXFrameType;

/***************************************
// IPP GSM-AMR enumerated TX frame types 
***************************************/
typedef enum
{	
	TX_SPEECH = 0,
	TX_SID_FIRST,
	TX_SID_UPDATE,
	TX_NO_DATA,

	IPPGSMAMRTXFRAMETYPE_IPPENUM_FORCE32BIT_RESERVE = 0x7fffffff
} IppGSMAMRTXFrameType;

/****************************************
// IPP GSM-AMR enumerated DTX state types 
****************************************/
typedef enum
{
	SPEECH = 0, 
	DTX, 
	DTX_MUTE,

	IPPGSMAMRDTXSTATETYPE_IPPENUM_FORCE32BIT_RESERVE = 0x7fffffff
} IppGSMAMRDTXStateType;  

typedef enum
{
	IF1_FORMAT = 0,
	IF2_FORMAT,
	HEADERLESS_IF1_FORMAT,
	TV_COMPLIANCE_FORMAT,

	IPPGSMAMRSTREAMFORMATID_IPPENUM_FORCE32BIT_RESERVE = 0x7fffffff
} IppGSMAMRStreamFormatId;

typedef enum
{
	AMRWB_IF1_FORMAT = 0,
	AMRWB_IF2_FORMAT,
	AMRWB_MIME_FILE_FORMAT,
	AMRWB_TV_COMPLIANCE_FORMAT,

	IPPAMRWBSTREAMFORMATID_IPPENUM_FORCE32BIT_RESERVE = 0x7fffffff
} IppAMRWBStreamFormatId;

/****************************************
// IPP G.711 enumerated law types 
****************************************/
typedef enum 
{
	IPP_ALAW = 0,
	IPP_ULAW = 1,

	IPPG711LAW_IPPENUM_FORCE32BIT_RESERVE = 0x7fffffff
} IppG711Law;

typedef struct {
	IppGSMAMRStreamFormatId formatInd;	
									
	int frameType;				/* Encoder's output, decoder's input, only used for 
								// formatInd = 2 */
	IppSpchBitRate bitRate;		/*  Input for both encoder and decoder */

} IppGSMAMRFormatConfig;

typedef struct {
	IppAMRWBStreamFormatId bitstreamFormatId; 
									
	IppSpchBitRate bitRate;		/*  Input for the encoder */

} IppAMRWBCodecConfig;

/********************
 G.723.1/A constants  
 ********************/
#define IPP_G723_FRAME_LEN			240		/* G.723.1/A frame length, in terms of PCM samples */
#define IPP_G723_SUBFRAME_LEN		60		/* G.723.1/A subframe length, in terms of PCM samples */
#define IPP_G723_LPC_ORDER			10		/* G.723.1/A LPC analysis order */
#define IPP_G723_LPCWIN_LEN			180		/* G.723.1/A LPC analysis Hamming window length */	
#define IPP_G723_NUM_SUBFRAME		4		/* G.723.1/A number of subframes contained in one frame */
#define IPP_G723_MAXLAG				145		/* G.723.1/A longest possible pitch lag (55 Hz) */
#define IPP_G723_TAMING_PARAMS		5		/* G.723.1/A error taming parameter vector length */
#define IPP_G723_COVMATDIM			416		/* G.723.1/A size of Toepliz covariance matrix for ACELP CB search */
#define IPP_G723_GSU_INIT			0x1000	/* G.723.1/A decoder gain scaling unit history initialization constant */
#define	IPP_G723_HPF_ENABLE		    1		/* G.723.1/A encoder highpass filter enable (on) */
#define	IPP_G723_HPF_DISABLE		0		/* G.723.1/A encoder highpass filter disable (off) */
#define	IPP_G723_POSTFILT_ENABLE	1		/* G.723.1/A decoder postfilter enable (on) */
#define	IPP_G723_POSTFILT_DISABLE   0		/* G.723.1/A decoder postfilter disable (off) */
#define	IPP_G723_ERASURE_FRAME		1		/* G.723.1/A decoder erasure frame detected (invalid frame) */
#define	IPP_G723_NON_ERASURE_FRAME  0		/* G.723.1/A decoder non-erasure frame (valid frame) */
#define IPP_G723_FRAMETYPE_NONTX    0       /* G.723.1/A non-transmitted silence frame (nonTX SID) */
#define IPP_G723_FRAMETYPE_VOICE    1		/* G.723.1/A active speech frame (5.3 or 6.3 kbps) */   
#define IPP_G723_FRAMETYPE_SID      2		/* G.723.1/A SID (silence interval description) frame */   

//***************************************
// IPP G723.1/A bitstream buffer lengths
//***************************************
#define	IPP_G723_BITSTREAM_LEN_5300	20		/* G.723.1/A 5.3 kbps minimum packed frame buffer byte allocation */
#define	IPP_G723_BITSTREAM_LEN_6300	24		/* G.723.1/A 5.3 kbps minimum packed frame buffer byte allocation */

/************************
 G.723.1 encoder state   
 ************************/
typedef struct
{
	Ipp16s	highpassFilterZfir;												/* preprocesser HPF nonrecursive memory */
	Ipp32s	highpassFilterZiir;												/* preprocesser HPF recursive memory */
	Ipp16s	prevSpch[2*IPP_G723_SUBFRAME_LEN];								/* half-frame (2 subframes) speech history buffer for LP analysis */
	Ipp16s	sineDtct;														/* sinusoidal input detection parameter */
	Ipp16s	prevLsf[IPP_G723_LPC_ORDER];									/* previous frame LSP history buffer */
	Ipp16s	perceptualWeightFilterZfir[IPP_G723_LPC_ORDER];					/* perceptual weighting filter nonrecursive memory */
	Ipp16s	perceptualWeightFilterZiir[IPP_G723_LPC_ORDER];					/* perceptual weighting filter recursive memory */
	Ipp16s	prevWgtSpch[IPP_G723_MAXLAG];									/* perceptually weighted speech history buffer for OLPS */
	Ipp16s	combinedFilterZfir[IPP_G723_LPC_ORDER];							/* combined filter (LPC synthesis+HNS+PWF) nonrecursive memory */
	Ipp16s	combinedFilterZiir[IPP_G723_MAXLAG];							/* combined filter (LPC synthesis+HNS+PWF) recursive memory */
	Ipp16s	prevExcitation[IPP_G723_MAXLAG];								/* previous frame excitation sequence history */
	Ipp32s	errorTamingParams[IPP_G723_TAMING_PARAMS];						/* error "taming" parameters for adaptive codebook search constraint */
	Ipp16s	openLoopPitchLag[IPP_G723_NUM_SUBFRAME];						/* VAD OL pitch lag history for voicing classification */ 
	Ipp16s	vadLpc[IPP_G723_LPC_ORDER];										/* VAD LPC inverse filter coefficients */
	Ipp16s	randomSeedCNG;													/* CNG excitation generator random seed */			
	Ipp16s	prevDTXFrameType;												/* DTX frame type history */
	Ipp16s	sidGain;														/* quantized CNG excitation gain (G~sid, [3], p. 9) */
	Ipp16s	targetExcitationGain;											/* CNG target excitation gain (G~t, [3], p. 10) */
	Ipp16s	frameAutoCorr[(IPP_G723_LPC_ORDER+1)*IPP_G723_NUM_SUBFRAME];	/* VAD frame (summation) autocorrelation histories */
	Ipp16s	frameAutoCorrExp[IPP_G723_NUM_SUBFRAME];						/* VAD frame (summation) autocorrelation exponent histories */
	Ipp16s	sidLsf[IPP_G723_LPC_ORDER];										/* CNG SID LSF vector (p~t associated with Asid(z)) */
	IppG723AVadState vadState;												/* VAD state */
	IppG723ADtxState dtxState;												/* DTX state */
} IppG723EncoderState; 

/************************
 G.723.1 decoder state   
 ************************/
typedef struct
{
	Ipp16s	consecutiveFrameErasures;							/* consective frame erasure count */
	Ipp16s	prevLsf[IPP_G723_LPC_ORDER];						/* previous frame LSP history buffer */
	Ipp16s	interpolationGain;									/* interpolation frame excitation gain */
	Ipp16s	interpolationIndex;									/* interpolation frame adaptive codebook index */
	Ipp16s	prevExcitation[IPP_G723_MAXLAG];					/* previous frame excitation sequence history */
	Ipp16s	randomSeed;											/* random seed for interplation frame unvoiced excitation synthesis */ 
	Ipp16s	synthesisFilterZiir[IPP_G723_LPC_ORDER];			/* speech synthesis filter recursive memory */
	Ipp16s	formantPostfilterZfir[IPP_G723_LPC_ORDER];			/* formant postfilter nonrecursive memory */
	Ipp16s	formantPostfilterZiir[IPP_G723_LPC_ORDER];			/* formant postfilter recursive memory */
	Ipp16s	autoCorr;											/* formant postfilter correlation coefficient (k) */
	Ipp16s	prevGain;											/* gain scaling unit gain history (previous frame gain) */
	Ipp16s	randomSeedCNG;										/* CNG excitation generator random seed */			
	Ipp16s	prevDTXFrameType;									/* DTX frame type history */
	Ipp16s	sidGain;											/* quantized CNG excitation gain (G~sid, [3], p. 9) */
	Ipp16s	targetExcitationGain;								/* CNG target excitation gain (G~t, [3], p. 10) */
	Ipp16s	sidLsf[IPP_G723_LPC_ORDER];							/* CNG SID LSF vector (p~t associated with Asid(z)) */
	IppSpchBitRate bitRate;										/* bitrate history */
	Ipp16s  tst[240];
} IppG723DecoderState;


/************************
 13K QCELP enum types
 ************************/
/* Rate Reduce mode for QCELP */
typedef enum {
    IPP_QCELP_RRM_DISABLE   = 0,
    IPP_QCELP_RRM_LEVEL1    = 1,
    IPP_QCELP_RRM_LEVEL2    = 2,
    IPP_QCELP_RRM_LEVEL3    = 3,
    IPP_QCELP_RRM_LEVEL4    = 4,

    IPPQCELPRATEREDUCEMODE_IPPENUM_FORCE32BIT_RESERVE = 0x7fffffff
} IppQCELPRateReduceMode;

/* encoded rate */
typedef enum {
    IPP_QCELP_BLANK			= 0,
    IPP_QCELP_EIGHTH		= 1,
    IPP_QCELP_QUARTER		= 2,
    IPP_QCELP_HALF			= 3,
    IPP_QCELP_FULL			= 4,   
    IPP_QCELP_ERASURE		= 0xe,

    IPPQCELPFRAMETYPE_IPPENUM_FORCE32BIT_RESERVE = 0x7fffffff
} IppQCELPFrameType;

typedef enum {
    IPP_QCELP_CHANGE_RRM = 0,

    IPPQCELPCOMMAND_IPPENUM_FORCE32BIT_RESERVE = 0x7fffffff
} IpppQCELPCommand;

/***********************
  IPP G.722 constants
***********************/
#define IPP_G722_PCM_ALIGN	8
#define IPP_G722_BITSTREAM_ALIGN	2

typedef enum {
	IPP_G722_CMD_RESET		=	0,
	IPP_G722_CMD_RESETMODE  =   1,

	IPPG722CMD_IPPENUM_FORCE32BIT_RESERVE = 0x7fffffff
} IppG722Cmd;


/***** Multimedia Codec Functions *************************************************/

/***** Speech Codecs *****/

/* GSM-AMR Codec */

/* Encoder */
IPPCODECAPI( IppCodecStatus, EncoderInitAlloc_GSMAMR, (
	void **ppDs, 
	int dtxEnable, 
	int vadModelSelect, 
	MiscGeneralCallbackTable* pCallBackTable
))

IPPCODECAPI(IppCodecStatus, EncoderFree_GSMAMR, (
	void **ppDs, 
	MiscGeneralCallbackTable* pCallBackTable
))

IPPCODECAPI( IppCodecStatus, Encode_GSMAMR, (
	IppSound *pSrcSpeech, 
	IppBitstream *pDstBitStream, 
	void *pGSMAMREncoderState, 
	IppGSMAMRFormatConfig *pFormatConfig
)) 

/* Decoder */
IPPCODECAPI( IppCodecStatus, DecoderInitAlloc_GSMAMR, (
	void	**ppDs, 
	MiscGeneralCallbackTable* pCallBackTable
))

IPPCODECAPI(IppCodecStatus, DecoderFree_GSMAMR, (
	void **ppDs, 
	MiscGeneralCallbackTable* pCallBackTable
))

IPPCODECAPI( IppCodecStatus, Decode_GSMAMR, (
	IppBitstream *pSrcBitstream, 
	IppSound *pDstSpeech, 
	void *pDecoderState, 
	IppGSMAMRFormatConfig *pFormatConfig
))

/* G.723.1 Codec */
IPPCODECAPI( IppCodecStatus,  EncoderInitAlloc_G723, (
	void **ppDs, 
	MiscGeneralCallbackTable* pCallBackTable
))

IPPCODECAPI(IppCodecStatus, EncoderFree_G723,(
	void **ppDs, 
	MiscGeneralCallbackTable* pCallBackTable
))

IPPCODECAPI( IppCodecStatus, Encode_G723, (
	IppSound *pSrcSpeech,
	IppBitstream *pDstBitstream, 
	IppSpchBitRate bitRate, 
	int enableVad, 
	int enableHighpassFilter,
	void *pEncoderState
))

IPPCODECAPI( IppCodecStatus,	DecoderInitAlloc_G723, (
	void **ppDs, 
	MiscGeneralCallbackTable* pCallBackTable
))

IPPCODECAPI(IppCodecStatus, DecoderFree_G723, (
	void **ppDs, 
	MiscGeneralCallbackTable* pCallBackTable
))

IPPCODECAPI( IppCodecStatus, Decode_G723, (
	IppBitstream *pSrcBitstream, 
	IppSound *pDstSpeech, 
	Ipp16s erasureFrame, 
	int enablePostFilter,
	void *pDecoderState
))

/* AMR-WB codec */

/* Encoder functions for AMR-WB */
IPPCODECAPI(IppCodecStatus, EncoderInitAlloc_AMRWB,(
			void **ppDs, int dtxEnable, MiscGeneralCallbackTable* pCallBackTable
))

IPPCODECAPI(IppCodecStatus, Encode_AMRWB, (
	IppSound *pSrcSpeech, IppBitstream *pDstBitStream, 
	IppAMRWBCodecConfig  *pFormatConfig, void *pEncoderState))

IPPCODECAPI(IppCodecStatus, EncoderFree_AMRWB, (
			void **ppDs, MiscGeneralCallbackTable* pCallBackTable
))

/* Decoder functions for AMR_WB */
IPPCODECAPI(IppCodecStatus, DecoderInitAlloc_AMRWB, (
			void	**ppDs, MiscGeneralCallbackTable* pCallBackTable
))

IPPCODECAPI(IppCodecStatus, DecoderFree_AMRWB, (
			void **ppDs, MiscGeneralCallbackTable* pCallBackTable
))

IPPCODECAPI (IppCodecStatus,  Decode_AMRWB, (
				IppBitstream		 *pSrcBitstream, 
				IppSound			 *pDstSpeech, 
				void				 *pDecoderState, 
				IppAMRWBCodecConfig  *pDecoderConfig
				))

/* ==================== G.729 Codec definitions ================= */
/* G729 has three flavors.  Note that annex A is a requirement for annex B (VAD). */
typedef	enum {
	G729, G729A, G729AB,

	IPPG729CODECTYPE_IPPENUM_FORCE32BIT_RESERVE = 0x7fffffff
} IppG729CodecType;
/* ==================== G.729 Codec API ================= */
IPPCODECAPI (IppCodecStatus, EncoderInitAlloc_G729, (void **ppDst, 
										   IppG729CodecType codecType,
										   MiscGeneralCallbackTable* pCallBackTable))
IPPCODECAPI (IppCodecStatus, EncoderFree_G729, (void **ppDst, 
											MiscGeneralCallbackTable* pCallBackTable))
IPPCODECAPI (IppCodecStatus, Encode_G729, (void* pEncoderState, 
                             const Ipp16s *pSrcSpeech, Ipp16s *pDstBitstream, IppG729CodecType codecType))

IPPCODECAPI (IppCodecStatus, DecoderInitAlloc_G729, (void **ppDst, 
											IppG729CodecType codecType,
											MiscGeneralCallbackTable* pCallBackTable))
IPPCODECAPI (IppCodecStatus, DecoderFree_G729, (void **ppDst,
											MiscGeneralCallbackTable* pCallBackTable))
IPPCODECAPI (IppCodecStatus, Decode_G729, (void* pSrcDecoderState,
                                 const Ipp16s *pSrcBitstream, Ipp16s *pDstSpeech, IppG729CodecType codecType))



/* Encoder functions for QCELP */

IPPCODECAPI(IppCodecStatus, EncoderInitAlloc_QCELP,(
	void **ppDs, MiscGeneralCallbackTable* pCallBackTable))

IPPCODECAPI(IppCodecStatus, Encode_QCELP, (
	IppSound *pSrcSpeech, IppBitstream *pDstBitStream, 
	IppQCELPFrameType *mode, void *pEncoderState))

IPPCODECAPI(IppCodecStatus, EncoderFree_QCELP, (
	void **ppDs, MiscGeneralCallbackTable* pCallBackTable))

IPPCODECAPI(IppCodecStatus, EncodeSendCmd_QCELP, (
            int cmd, void *pInParam, void *pEncoderState))

// Decoder functions for QCELP 
IPPCODECAPI(IppCodecStatus, DecoderInitAlloc_QCELP, (
	void **ppDs, MiscGeneralCallbackTable* pCallBackTable))

IPPCODECAPI(IppCodecStatus, DecoderFree_QCELP, (
	void **ppDs, MiscGeneralCallbackTable* pCallBackTable))

IPPCODECAPI (IppCodecStatus,  Decode_QCELP, (
	IppBitstream *pSrcBitstream, IppSound *pDstSpeech,
	int mode, void *pDecoderState))
    
/*============================ G.711 Codec API ====================================*/
IPPCODECAPI(IppCodecStatus, Encode_G711, (const Ipp16s *pSrc, int length, IppG711Law lawused, Ipp8u *pDst))

IPPCODECAPI(IppCodecStatus, Decode_G711, (const Ipp8u *pSrc, int length, IppG711Law lawused, Ipp16s *pDst))

IPPCODECAPI(IppCodecStatus, ALawToULaw_G711, (const Ipp8u *pSrc, int length, Ipp8u *pDst))

IPPCODECAPI(IppCodecStatus, ULawToALaw_G711, (const Ipp8u *pSrc, int length, Ipp8u *pDst))

/* ==================== G.722 Codec definitions ================= */
// G722 encoder function
IPPCODECAPI(IppCodecStatus, EncoderInitAlloc_G722,(
			void **ppEncoderObj, MiscGeneralCallbackTable* pCallBackTable))
IPPCODECAPI (IppCodecStatus, EncoderFree_G722,(
			void **ppEncoderObj ))
IPPCODECAPI (IppCodecStatus, Encode_G722, (
			IppSound* pSrcPcm,
			IppBitstream* pDstBitstream,
			void* pEncoderObj ))

// G722 decoder function
IPPCODECAPI(IppCodecStatus, DecoderInitAlloc_G722,(
			void **ppDecoderObj, 
			MiscGeneralCallbackTable* pCallBackTable
			))
IPPCODECAPI (IppCodecStatus, DecoderFree_G722,(
			void ** ppDecoderObj 
			))
IPPCODECAPI (IppCodecStatus, Decode_G722, (
		    IppBitstream* pSrcBitstream,
			IppSound* pDstPcm,
			int mode,
			void* pDecoderObj
			))

//packet loss concealment for G722 decoder
IPPCODECAPI (IppCodecStatus, PacketLossConceal_G722, (
			 void *pDecoderObj, 
			 Ipp32s nFrameLen, 
			 IppSound *pDstPcm
			 ))

//G722 codec send command
IPPCODECAPI (IppCodecStatus, CodecSendCmd_G722, (
			IppG722Cmd cmd,
			void* pInParam,
			void* pOutParam,
			void* pCodecObj ))
			
#ifdef __cplusplus
}
#endif

#endif    /* #ifndef _CODECSC_H_ */

/* EOF */


