// ============================================================================
// DxO Labs proprietary and confidential information
// Copyright (C) DxO Labs 1999-2011 - (All rights reserved)
// ============================================================================
/*******************************************************************************
//(C) Copyright [2010 - 2011] Marvell International Ltd.
//All Rights Reserved
*******************************************************************************/

/*****************************************************************************
 * Defined Functions:
 *        - DxOISP_SensorInitialize()
 *        - DxOISP_SensorGetStartGroup()
 *        - DxOISP_SensorGetEndGroup()
 *        - DxOISP_SensorSetStartGroup()
 *        - DxOISP_SensorSetEndGroup()
 *        - DxOISP_SensorSet()
 *        - DxOISP_SensorGet()
 *        - DxOISP_SensorFire()
 *
 *****************************************************************************/

#ifndef __DxOISP_SENSOR_API_H__
#define __DxOISP_SENSOR_API_H__

// XM: #include <stddef.h>
// XM: #include <stdint.h>

#include "DxOISP.h"

/****************************************************************************/
/* TYPES DEFINITION                                                         */
/****************************************************************************/

#define SENSOR_USER_STATUS_SIZE 10

typedef enum {
	E_SENSOR_PHASE_GRBG = 0, ///< Gr R B Gb
	E_SENSOR_PHASE_RGGB,     ///< R Gr Gb B
	E_SENSOR_PHASE_BGGR,     ///< B Gb Gr R
	E_SENSOR_PHASE_GBRG,     ///< Gb B R Gr
	E_SENSOR_NB_PHASE
}te_SensorPhase;




typedef struct {
	uint8_t isSensorStreaming;                // streaming off(0), streaming on(1)

	struct {
		uint16_t usXBayerStart;               // Crop applied on the sensor.
		uint16_t usYBayerStart;               // unit: values expressed in bayer.
		uint16_t usXBayerEnd;                 // usXBayerStart, usYBayerStart, usXBayerEnd, usYBayerEnd are defined in the sensor array.
		uint16_t usYBayerEnd;                 // Does not take into account optionnal flip and/or mirror.
		uint8_t  eOrientation;                // te_DxOISPImageOrientation
		uint16_t usMinHorBlanking;            // minimum horizontal blanking expressed in pixels
		uint16_t usMinVertBlanking;           // minimum vertical blanking expressed in ms expressed in 8.8
		uint16_t usFrameRate;                 // target frame rate. in frame per second, expressed in 12.4
		uint8_t  ucHorDownsamplingFactor;     // horizontal downsampling that can be provided by the sensor
		uint8_t  ucVertDownsamplingFactor;    // vertical downsampling that can be provided by the sensor
	} stImageFeature;

	struct {
		uint32_t uiExpoTime;                  // exposure time in ms expressed in 16.16
		uint16_t usAGain[DxOISP_NB_CHANNEL];  // Analog gain: expressed in 8.8
		uint16_t usDGain[DxOISP_NB_CHANNEL];  // Digital gain: expressed in 8.8
		uint16_t usWBScales[2];               // [R/B] white balance scales expressed in 8.8 applied by ISP
	} stShootingParam;

	uint16_t usFlashPower;                    // in Cd on optical axis (16.0)
	uint16_t usAfPosition;                    // unit: dioptry, expressed in 4.12
	uint16_t usZoomFocal;                     // ratio between focal distance and sensor diagonal size expressed in 8.8
	uint16_t usAperture;                      // f number expressed in 8.8
} ts_SENSOR_Cmd;

typedef struct {
	uint8_t                      isSensorFireNeeded;
	ts_SENSOR_Cmd                stAppliedCmd;
	uint8_t                      eCurrentPhase;        //te_SensorPhase: phase of the image (may depend of the orientation)
	uint8_t                      ucHorSkippingFactor;
	uint8_t                      ucVertSkippingFactor;
	uint8_t                      isImageFeatureStable; //true if crop, downsampling, mirror/flip and blanking will not change any further
	uint8_t                      isFrameInvalid;       //true if actual frame does not correspond to the reported information
	uint8_t                      isImageQualityStable; //true all other settings will not change any further
	                                                   //(stShootingParam, usFlashPower, usAfPosition, usZoomFocal, usAperture)
	uint32_t                     uiPixelClock;         //unit: MHz, expressed in 16.16
	uint32_t                     uiFrameValidTime;     //in ms expressed in 16.16 (time from first pixel out of sensor to last pixel out of sensor)
	ts_DxOIspSensorCapabilities  stCapabilities;
	uint8_t                      ucUserStatus[SENSOR_USER_STATUS_SIZE];
} ts_SENSOR_Status;

typedef struct {
	uint8_t                      uiStillSkipFrameNum;
} ts_SENSOR_Attr;

/******************************************************/
/*              OFFSET STRUCTURE                      */
/******************************************************/
#define SENSOR_CMD_OFFSET(x)         offsetof(ts_SENSOR_Cmd   , x)
#define SENSOR_STATUS_OFFSET(x)      offsetof(ts_SENSOR_Status, x)


void DxOISP_SensorInitialize    (uint8_t ucSensorId);
void DxOISP_SensorUninitialize  (uint8_t ucSensorId);

void DxOISP_SensorCommandGroupOpen  (uint8_t ucSensorId);
void DxOISP_SensorCommandSet        (uint8_t ucSensorId, uint16_t usOffset, uint16_t usSize, void* pBuf );
void DxOISP_SensorCommandGroupClose (uint8_t ucSensorId);

void DxOISP_SensorStatusGroupOpen  (uint8_t ucSensorId);
void DxOISP_SensorStatusGet        (uint8_t ucSensorId, uint16_t usOffset, uint16_t usSize, void* pBuf );
void DxOISP_SensorStatusGroupClose (uint8_t ucSensorId);

void DxOISP_SensorFire             (uint8_t ucSensorId);
void DxOISP_SensorGetAttr          (uint8_t ucSensorId, ts_SENSOR_Attr *pstSensorAttr );

#endif //__DxOISP_SENSOR_API_H__
