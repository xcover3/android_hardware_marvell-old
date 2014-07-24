// ============================================================================
// DxO Labs proprietary and confidential information
// Copyright (C) DxO Labs 1999-2011 - (All rights reserved)
// ============================================================================
/******************************************************************************
//(C) Copyright [2010 - 2011] Marvell International Ltd.
//All Rights Reserved
******************************************************************************/


#include <stdint.h>
#include <assert.h>
#define INC(f) <f>
// #include INC(CHIP_VENDOR_PATH_ISP_INC_DIR/DxOISP.h)
#include "DxOISP.h"
#include "DxOISP_Tools.h"

#define VIDEO_STAB_EXT                                   5 // in %
#define DxOISP_BAND_SIZE                              1296
#define XBORDER                                         38
#define XBORDER_HDR                                     46
#define NB_FRAME_BUFFER                                  4

#define PACK(a)                         (((a)+(1<<6)-1)>>6)
#define DIV_UP(x,y)                       (((x)+(y)-1)/(y))
#define MIN(x,y)                          ((x)<(y)?(x):(y))
#define MAX(x,y)                          ((x)>(y)?(x):(y))
#define SHIFT_RND(x,n)   (((x)+((n)!=0?1<<((n)-1):0))>>(n))


static void computeBurstMode(
	ts_DxOIspSyncCmd* pstIspSyncCmd
,	uint8_t           ucPixelWidth
,	uint8_t*          pucNbBand
,	uint32_t*         uiBufferSizeTab)
{
	ts_DxOISPCrop* pstCropCapt   = &pstIspSyncCmd->stControl.stOutputImage.stCrop;
	uint16_t       usInputWidth  = pstCropCapt->usXAddrEnd - pstCropCapt->usXAddrStart + 1;
	uint16_t       usInputHeight = pstCropCapt->usYAddrEnd - pstCropCapt->usYAddrStart + 1;
	uint8_t        ucNbCapture   = pstIspSyncCmd->stControl.ucNbCaptureFrame;
	uint16_t       usOutputWidth = pstIspSyncCmd->stControl.stOutputImage.usSizeX;
	uint16_t       usRemainingWidth;
	uint16_t       usInputBandWidth;
	uint8_t        i;


	for(i=0;i<NB_FRAME_BUFFER;i++) uiBufferSizeTab[i] = 0;

	*pucNbBand       = DIV_UP(usOutputWidth, pstIspSyncCmd->stControl.usBandSize);
	usInputBandWidth = SHIFT_RND(usInputWidth * (((pstIspSyncCmd->stControl.usBandSize)<<16)/usOutputWidth), 16);
	usRemainingWidth = usInputWidth;
	if(*pucNbBand>1) {
		uiBufferSizeTab[0] = PACK((usInputBandWidth + 4 + XBORDER + 2) * (usInputHeight+1) * ucPixelWidth) * ucNbCapture;
		usRemainingWidth -= usInputBandWidth;
		for(i=1;i<*pucNbBand-1;i++) {
			uiBufferSizeTab[i] = PACK((usInputBandWidth + 4 + 2*(XBORDER + 2)) * (usInputHeight+1) * ucPixelWidth) * ucNbCapture;
			usRemainingWidth  -= usInputBandWidth;
		}
		uiBufferSizeTab[*pucNbBand-1] = PACK((usRemainingWidth + 4 + XBORDER + 2) * (usInputHeight+1) * ucPixelWidth) * ucNbCapture;
	}
	else {
		uiBufferSizeTab[0] = PACK((usInputWidth + 6) * (usInputHeight+1) * ucPixelWidth) * ucNbCapture;
	}
}


static void computeHdrMode(
	ts_DxOIspSyncCmd* pstIspSyncCmd
,	uint8_t*          pucNbBand
,	uint32_t*         uiBufferSizeTab)
{
	ts_DxOISPCrop* pstCropCapt   = &pstIspSyncCmd->stControl.stOutputImage.stCrop;
	uint16_t       usInputWidth  = pstCropCapt->usXAddrEnd - pstCropCapt->usXAddrStart + 1;
	uint16_t       usInputHeight = pstCropCapt->usYAddrEnd - pstCropCapt->usYAddrStart + 1;
	uint16_t       usOutputWidth = pstIspSyncCmd->stControl.stOutputImage.usSizeX;
	uint16_t       usInputBandWidth;
	uint8_t        i;
	uint8_t        ucNbBandStep0;

	for(i=0;i<NB_FRAME_BUFFER;i++) uiBufferSizeTab[i] = 0;

	*pucNbBand       = DIV_UP(usOutputWidth, pstIspSyncCmd->stControl.usBandSize);
	ucNbBandStep0    = DIV_UP(*pucNbBand, 2);

	usInputBandWidth = SHIFT_RND(usInputWidth * (((pstIspSyncCmd->stControl.usBandSize)<<16)/usOutputWidth), 16);

	uiBufferSizeTab[0] =
	uiBufferSizeTab[1] = PACK(MIN((usInputBandWidth * 2 + 4 + XBORDER_HDR + 2)*4, 4*(usInputWidth+3)) * (usInputHeight+1) * 12);
	if(ucNbBandStep0>1) {
		uiBufferSizeTab[2] =
		uiBufferSizeTab[3] = PACK((usOutputWidth - 2*usInputBandWidth + 4 + XBORDER_HDR + 2) * 4 * (usInputHeight+1) * 12);
	}

	for(i=0;i<NB_FRAME_BUFFER;i++) {
		uiBufferSizeTab[i] *= 4;
	}
}



void DxOISP_ComputeFrameBufferSize(
	ts_DxOIspSyncCmd* pstIspSyncCmd
,	uint16_t          usSensorWidth
,	uint16_t          usSensorHeight
,	uint8_t           ucPixelWidth
,	uint8_t*          pucNbFrameBuffer
,	uint8_t*          pucNbBand
,	uint32_t*         uiBufferSizeTab)
{
	ts_DxOISPCrop* pstCropCapt   = &pstIspSyncCmd->stControl.stOutputImage.stCrop;
	ts_DxOISPCrop* pstCropDisp   = &pstIspSyncCmd->stControl.stOutputDisplay.stCrop;
	*pucNbFrameBuffer            = 0;
	*pucNbBand                   = 0;

	// FIXME: DXO issue workaround
	ucPixelWidth                 = 12;

	switch(pstIspSyncCmd->stControl.eMode) {
		case DxOISP_MODE_PRECAPTURE:
			if(pstIspSyncCmd->stControl.eSubModeCapture != DxOISP_CAPTURE_ZERO_SHUTTER_LAG) {
				*pucNbFrameBuffer = 0;
				*pucNbBand        = 0;
			}
			else {
				assert(pstIspSyncCmd->stControl.usBandSize>0);
				computeBurstMode(pstIspSyncCmd, ucPixelWidth, pucNbBand, uiBufferSizeTab);
				*pucNbFrameBuffer = *pucNbBand;
			}
		break;
		case DxOISP_MODE_CAPTURE_A:
		case DxOISP_MODE_CAPTURE_B:
			assert(pstIspSyncCmd->stControl.usBandSize>0);
			if(pstIspSyncCmd->stControl.eSubModeCapture == DxOISP_CAPTURE_HDR) {
				computeHdrMode(pstIspSyncCmd, pucNbBand, uiBufferSizeTab);
				*pucNbFrameBuffer = (*pucNbBand) + (((*pucNbBand)&0x01) ? 1 : 0);
			}
			else {
				computeBurstMode(pstIspSyncCmd, ucPixelWidth, pucNbBand, uiBufferSizeTab);
				*pucNbFrameBuffer = *pucNbBand;
			}
		break;

		case DxOISP_MODE_PREVIEW:
		case DxOISP_MODE_VIDEOREC:
		{
			uint8_t eSubModeVideoRec = pstIspSyncCmd->stControl.eSubModeVideoRec;
			if(eSubModeVideoRec != DxOISP_VR_SIMPLE) {
				uint16_t usInputWidth, usInputHeight;
				uint16_t usAddBayerX , usAddBayerY;
				uint16_t usXStart    , usXEnd;
				uint16_t usYStart    , usYEnd;
				uint16_t usOutX      , usOutY;
				uint16_t usBayerWidth, usBayerHeight;
				uint16_t usDecim;
				uint8_t  isDispUsed    = !pstIspSyncCmd->stControl.isDisplayDisabled;
				uint8_t  isCodecUsed   = pstIspSyncCmd->stControl.eMode == DxOISP_MODE_VIDEOREC ? 1 : 0;
				if(isCodecUsed) {
					usXStart      = isDispUsed ? MIN(pstCropDisp->usXAddrStart, pstCropCapt->usXAddrStart) : pstCropCapt->usXAddrStart;
					usXEnd        = isDispUsed ? MAX(pstCropDisp->usXAddrEnd  , pstCropCapt->usXAddrEnd  ) : pstCropCapt->usXAddrEnd  ;
					usYStart      = isDispUsed ? MIN(pstCropDisp->usYAddrStart, pstCropCapt->usYAddrStart) : pstCropCapt->usYAddrStart;
					usYEnd        = isDispUsed ? MAX(pstCropDisp->usYAddrEnd  , pstCropCapt->usYAddrEnd  ) : pstCropCapt->usYAddrEnd  ;
					usBayerWidth  = (usXEnd - usXStart + 1)/2;
					usBayerHeight = (usYEnd - usYStart + 1)/2;
					usOutX        = pstIspSyncCmd->stControl.stOutputImage.usSizeX/2;
					usOutY        = pstIspSyncCmd->stControl.stOutputImage.usSizeY/2;
					if(usBayerWidth*usOutY >= usBayerHeight*usOutX) {
						usDecim = DIV_UP(usBayerWidth, 2*usOutX);
						if(DIV_UP(usBayerWidth, usDecim) >= (2*usOutX)) {
							usDecim++;
						}
					} else {
						usDecim = DIV_UP(usBayerHeight, 2*usOutY);
						if(DIV_UP(usBayerHeight, usDecim) >= (2*usOutY)) {
							usDecim++;
						}
					}
				} else {
					usXStart      = pstCropDisp->usXAddrStart;
					usXEnd        = pstCropDisp->usXAddrEnd;
					usYStart      = pstCropDisp->usYAddrStart;
					usYEnd        = pstCropDisp->usYAddrEnd;
					usBayerWidth  = (usXEnd - usXStart + 1)/2;
					usBayerHeight = (usYEnd - usYStart + 1)/2;
					usDecim       = 1;
				}

				if(eSubModeVideoRec == DxOISP_VR_STABILIZED || eSubModeVideoRec == DxOISP_VR_STABILIZED_TEMPORAL_NOISE) {
					usAddBayerX   = DIV_UP(usBayerWidth * VIDEO_STAB_EXT, 100);
					usAddBayerY   = DIV_UP(usBayerHeight* VIDEO_STAB_EXT, 100);
				}
				else {
					usAddBayerX   = 0;
					usAddBayerY   = 0;
				}
				usXStart       = MAX(0, (int)(usXStart - usAddBayerX*2));
				usYStart       = MAX(0, (int)(usYStart - usAddBayerX*2));
				usXEnd         = MIN(usSensorWidth - 1, usXEnd + usAddBayerX*2);
				usYEnd         = MIN(usSensorHeight- 1, usYEnd + usAddBayerY*2);
				usInputWidth   = usXEnd - usXStart + 1;
				usInputHeight  = usYEnd - usYStart + 1;

				uiBufferSizeTab[0] = PACK((DIV_UP(usInputWidth, usDecim) + 6) * (DIV_UP(usInputHeight, usDecim) + 1) * ucPixelWidth);

				if(eSubModeVideoRec == DxOISP_VR_STABILIZED_TEMPORAL_NOISE) {
					uiBufferSizeTab[1] = uiBufferSizeTab[0];
					*pucNbFrameBuffer = 2;
				}else {
					*pucNbFrameBuffer = 1;
				}
				*pucNbBand = 1;
			}
		}
		break;

		default:
			*pucNbFrameBuffer =
			*pucNbBand        = 0;
		break;
	}

	// Added by Hans:
	// 1) make sure the output size is 32 bytes aligned
	// 2) workaround to solve still image capture image content corrupt problem - increase buffer size
	//    (29/16) is so far the minimum ratio we have tried to make sure the output image is correct
	int i;
	for ( i = 0; i < *pucNbFrameBuffer; i++)
		uiBufferSizeTab[i] = (uiBufferSizeTab[i] * 8 + 31) & ~31;
}

uint16_t DxOISP_ComputeBandSize(ts_DxOIspSyncCmd* pstIspSyncCmd)
{
	uint16_t usTotalPreDecim;
	ts_DxOISPOutput* pstOutput = &pstIspSyncCmd->stControl.stOutputImage;
	ts_DxOISPCrop*   pstcrop   = &pstOutput->stCrop;
	uint16_t usInX             = (pstcrop->usXAddrEnd - pstcrop->usXAddrStart + 1)/2;
	uint16_t usInY             = (pstcrop->usYAddrEnd - pstcrop->usYAddrStart + 1)/2;
	uint16_t usOutX            = pstOutput->usSizeX/2;
	uint16_t usOutY            = pstOutput->usSizeY/2;

	if(usInX*usOutY >= usInY*usOutX) {
		usTotalPreDecim = DIV_UP(usInX, 2*usOutX);
		if(DIV_UP(usInX, usTotalPreDecim*2) >= usOutX) {
			usTotalPreDecim++;
		}
	} else {
		usTotalPreDecim = DIV_UP(usInY, 2*usOutY);
		if(DIV_UP(usInY, usTotalPreDecim*2) >= usOutY) {
			usTotalPreDecim++;
		}
	}

	usInX = DIV_UP(usInX, usTotalPreDecim); //image width processed by IPC in bayer

	return MIN(MIN(((DxOISP_BAND_SIZE/2 * usOutX) / usInX) * 2, usOutX*2), DxOISP_BAND_SIZE);
}
