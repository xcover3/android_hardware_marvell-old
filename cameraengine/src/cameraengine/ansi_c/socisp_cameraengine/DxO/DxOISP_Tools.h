// ============================================================================
// DxO Labs proprietary and confidential information
// Copyright (C) DxO Labs 1999-2010 - (All rights reserved)
// ============================================================================
/******************************************************************************
//(C) Copyright [2010 - 2011] Marvell International Ltd.
//All Rights Reserved
******************************************************************************/

#ifndef __DXO_ISP_TOOLS_H__
#define __DXO_ISP_TOOLS_H__

#include <stdint.h>
#define INC(f) <f>
// #include INC(CHIP_VENDOR_PATH_ISP_INC_DIR/DxOISP.h)

#define NB_FRAME_BUFFER                                  4

#include "DxOISP.h"

void DxOISP_ComputeFrameBufferSize(
	ts_DxOIspSyncCmd* pstIspSyncCmd
,	uint16_t          usSensorWidth
,	uint16_t          usSensorHeight
,	uint8_t           ucPixelWidth
,	uint8_t*          pucNbFrameBuffer
,	uint8_t*          pucNbBand
,	uint32_t*         uiBufferSizeTab);

uint16_t DxOISP_ComputeBandSize(ts_DxOIspSyncCmd* pstIspSyncCmd);

#endif
