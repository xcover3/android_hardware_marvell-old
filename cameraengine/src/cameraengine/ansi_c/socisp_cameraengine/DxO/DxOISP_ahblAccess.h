// ============================================================================
// DxO Labs proprietary and confidential information
// Copyright (C) DxO Labs 1999-2010 - (All rights reserved)
// ============================================================================

#include <stdint.h>

typedef struct {
	uint8_t*  puiSrc ;             /* DDR source to copy to DxOIPC or DDR destination to copy from DxOIPC */ 
	uint32_t  uiNbAccess ;         /* number of R/W access */
	uint32_t  uiOffset ;           /* value to set to ptr offset either in W or R access */
} ts_block ;

typedef struct {
	uint32_t   uiNbBlock ;         /* number of block to handle */
	ts_block*  ptsBlock ;          /* pointer to the blocks to handle - to use as an array */
	void*      pDstBase ;          /* base address of devices addressed inside DxO IP */
	uint32_t   uiDstOffset ;       /* byte data offset in destination device */
	uint32_t   uiDstPtrOffset ;    /* byte ptr offset in destination device */
} ts_segment ;

void     DxOISP_setRegister     (const uint32_t    uiOffset   ,        uint32_t  uiValue                         ) ;
uint32_t DxOISP_getRegister     (const uint32_t    uiOffset                                                      ) ;
void     DxOISP_setMultiRegister(const uint32_t    uiOffset   ,  const uint32_t* puiSrc      , uint32_t uiNbElem ) ;
void     DxOISP_getMultiRegister(const uint32_t    uiOffset   ,        uint32_t* puiDst      , uint32_t uiNbElem ) ;
void     DxOISP_setMultiSegment (const ts_segment* ptsSegment ,        uint32_t  uiNbSegment                     ) ;
void     DxOISP_getMultiSegment (const ts_segment* ptsSegment ,        uint32_t  uiNbSegment                     ) ;


#define DXOIPC_ID_CODE_MEM    0
#define DXOIPC_ID_PARAM_MEM   1
#define DXOIPC_ID_ZONE_MEM    2
#define DXOIPC_NB_MEM         3
#define DXO_IP_OFFSET_IPC_CODE_MEM  0x00000118
#define DXO_IP_OFFSET_IPC_PARAM_MEM 0x00000120
#define DXO_IP_OFFSET_IPC_ZONE_MEM  0x0000011c
#define DXO_IP_OFFSET_IPC_PTR       0x00000100
#define DxO_IP_OFFSET_PRH_MEM       0x000080f8
#define DxO_IP_OFFSET_PRH_PTR       0x00008000
