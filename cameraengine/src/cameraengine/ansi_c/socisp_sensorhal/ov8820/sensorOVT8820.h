// ============================================================================
// DxO Labs proprietary and confidential information
// Copyright (C) DxO Labs 1999-2010 - (All rights reserved)
// ============================================================================
/*******************************************************************************
//(C) Copyright [2010 - 2011] Marvell International Ltd.
//All Rights Reserved
*******************************************************************************/

/*****************************************************************************
 * Defined Functions:
 *                    - DxOSensor_OVT8820_Initialize()
 *                    - DxOSensor_OVT8820_GetStartGrp()
 *                    - DxOSensor_OVT8820_Get()
 *                    - DxOSensor_OVT8820_GetEndGrp()
 *                    - DxOSensor_OVT8820_SetStartGrp()
 *                    - DxOSensor_OVT8820_Set()
 *                    - DxOSensor_OVT8820_SetEndGrp()
 *                    - DxOSensor_OVT8820_Fire()
 *
 *****************************************************************************/
#if !defined(SENSOR_OVT8820_H)
#define SENSOR_OVT8820_H

#include <stddef.h>
#include <stdint.h>


void DxOSensor_OVT8820_Initialize  ( void );
void DxOSensor_OVT8820_Uninitialize ( void );

void DxOSensor_OVT8820_GetStartGrp ( void );
void DxOSensor_OVT8820_Get ( uint16_t usOffset, uint16_t usSize, void* pBuf );
void DxOSensor_OVT8820_GetEndGrp   ( void );


void DxOSensor_OVT8820_SetStartGrp ( void );
void DxOSensor_OVT8820_Set ( uint16_t usOffset, uint16_t usSize, void* pBuf );
void DxOSensor_OVT8820_SetEndGrp   ( void );

void DxOSensor_OVT8820_Fire ( void );

void DxOSensor_OVT8820_RawDump ( const char *fname );

void MrvlSensor_OVT8820_GetAttr ( ts_SENSOR_Attr *pstSensorAttr );

#endif //SENSOR_OVT8820_H
/****************************************************************************/
/****************************************************************************/
/* END OF FILE                                                              */
/****************************************************************************/
/****************************************************************************/
