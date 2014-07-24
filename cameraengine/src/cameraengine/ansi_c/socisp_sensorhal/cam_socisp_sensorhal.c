// ============================================================================
// DxO Labs proprietary and confidential information
// Copyright (C) DxO Labs 1999-2011 - (All rights reserved)
// ============================================================================
/*******************************************************************************
//(C) Copyright [2010 - 2011] Marvell International Ltd.
//All Rights Reserved
*******************************************************************************/

#include "cam_socisp_sensorhal.h"

typedef struct {
	void (*Initialize)(void);
	void (*Uninitialize)(void);
	void (*StatusGroupOpen)(void);
	void (*StatusGet)(uint16_t usOffset, uint16_t usSize, void* pBuf);
	void (*StatusGroupClose)(void);
	void (*CommandGroupOpen)(void);
	void (*CommandSet)(uint16_t usOffset, uint16_t ussize, void* pBuf);
	void (*CommandGroupClose)(void);
	void (*Fire)(void);
	void (*RawDump)(const char *fname);
	void (*GetAttr)(ts_SENSOR_Attr *pstSensorAttr);
} DxOSensorItf;

//#define SENSOR0_VX6852_PLUGGED
#define SENSOR0_OVT8820_PLUGGED

//#define SENSOR1_VX6852_PLUGGED
#define SENSOR1_OVT8820_PLUGGED

#if defined(SENSOR0_SIMU_PLUGGED) || defined(SENSOR1_SIMU_PLUGGED)
#define ONE_SENSOR_PLUGGED
#include INC(firmware/sensor/Sensor_Simu_Driver.c)

static DxOSensorItf DxOSensor_SIMU_Itf = {
	DxOSensor_SIMU_Initialize,
	NULL,
	DxOSensor_SIMU_GetStartGrp,
	DxOSensor_SIMU_Get,
	DxOSensor_SIMU_GetEndGrp,
	DxOSensor_SIMU_SetStartGrp,
	DxOSensor_SIMU_Set,
	DxOSensor_SIMU_SetEndGrp,
	DxOSensor_SIMU_Fire,
	NULL,
	NULL,
};
#endif


#if defined(SENSOR0_VX6852_PLUGGED) || defined(SENSOR1_VX6852_PLUGGED)
#define ONE_SENSOR_PLUGGED
//#include "sensorVx6852.h"

static DxOSensorItf DxOSensor_VX6852_Itf = {
	DxOSensor_VX6852_Initialize,
	DxOSensor_VX6852_Uninitialize,
	DxOSensor_VX6852_GetStartGrp,
	DxOSensor_VX6852_Get,
	DxOSensor_VX6852_GetEndGrp,
	DxOSensor_VX6852_SetStartGrp,
	DxOSensor_VX6852_Set,
	DxOSensor_VX6852_SetEndGrp,
	DxOSensor_VX6852_Fire,
	DxOSensor_VX6852_RawDump,
	NULL,
};
#endif

#if defined(SENSOR0_OVT8820_PLUGGED) || defined(SENSOR1_OVT8820_PLUGGED)
#define ONE_SENSOR_PLUGGED
#include "sensorOVT8820.h"

static DxOSensorItf DxOSensor_OVT8820_Itf = {
	DxOSensor_OVT8820_Initialize,
	DxOSensor_OVT8820_Uninitialize,
	DxOSensor_OVT8820_GetStartGrp,
	DxOSensor_OVT8820_Get,
	DxOSensor_OVT8820_GetEndGrp,
	DxOSensor_OVT8820_SetStartGrp,
	DxOSensor_OVT8820_Set,
	DxOSensor_OVT8820_SetEndGrp,
	DxOSensor_OVT8820_Fire,
	DxOSensor_OVT8820_RawDump,
	MrvlSensor_OVT8820_GetAttr,
};

#endif

#if !defined(ONE_SENSOR_PLUGGED)
#error NO SENSOR IS PLUGGED TO THE SENSOR WRAPPER!!!
#endif

static DxOSensorItf* DxOSensor_Itf[] = {
#ifdef SENSOR0_SIMU_PLUGGED
	&DxOSensor_SIMU_Itf,
#endif
#ifdef SENSOR0_VX6852_PLUGGED
	&DxOSensor_VX6852_Itf,
#endif
#ifdef SENSOR0_OVT8820_PLUGGED
	&DxOSensor_OVT8820_Itf,
#endif

#ifdef SENSOR1_SIMU_PLUGGED
	&DxOSensor_SIMU_Itf,
#endif
#ifdef SENSOR1_VX6852_PLUGGED
	&DxOSensor_VX6852_Itf,
#endif
#ifdef SENSOR1_OVT8820_PLUGGED
	&DxOSensor_OVT8820_Itf,
#endif
};

#if 1 // XM: Enable this for camera sensor

void DxOISP_SensorInitialize( uint8_t ucSensorId )
{
	#if defined(SENSOR0_SIMU_PLUGGED) || defined(SENSOR1_SIMU_PLUGGED)
	debugSensorInit(ucSensorId) ;
	#endif
	DxOSensor_Itf[ucSensorId]->Initialize();

	return;
}

void DxOISP_SensorUninitialize( uint8_t ucSensorId )
{
	#if defined(SENSOR0_SIMU_PLUGGED) || defined(SENSOR1_SIMU_PLUGGED)
	debugSensorUninit(ucSensorId) ;
	#endif

	DxOSensor_Itf[ucSensorId]->Uninitialize();

	return;
}

void DxOISP_SensorStatusGroupOpen(uint8_t ucSensorId)
{
	#if defined(SENSOR0_SIMU_PLUGGED) || defined(SENSOR1_SIMU_PLUGGED)
	debugSensorStatusGroupOpen(ucSensorId) ;
	#endif
	DxOSensor_Itf[ucSensorId]->StatusGroupOpen();
}

void DxOISP_SensorStatusGet(uint8_t ucSensorId ,uint16_t usOffset, uint16_t usSize, void* pBuf)
{
	DxOSensor_Itf[ucSensorId]->StatusGet( usOffset, usSize, pBuf );
	#if defined(SENSOR0_SIMU_PLUGGED) || defined(SENSOR1_SIMU_PLUGGED)
	debugSensorStatusGet(ucSensorId, usOffset, usSize, pBuf);
	#endif
}

void DxOISP_SensorStatusGroupClose(uint8_t ucSensorId)
{
	#if defined(SENSOR0_SIMU_PLUGGED) || defined(SENSOR1_SIMU_PLUGGED)
	debugSensorStatusGroupClose(ucSensorId) ;
	#endif
	DxOSensor_Itf[ucSensorId]->StatusGroupClose();
}

void DxOISP_SensorCommandGroupOpen(uint8_t ucSensorId)
{
	#if defined(SENSOR0_SIMU_PLUGGED) || defined(SENSOR1_SIMU_PLUGGED)
	debugSensorCommandGroupOpen(ucSensorId) ;
	#endif
	DxOSensor_Itf[ucSensorId]->CommandGroupOpen();
}

void DxOISP_SensorCommandSet(uint8_t ucSensorId, uint16_t usOffset, uint16_t usSize, void* pBuf)
{
	#if defined(SENSOR0_SIMU_PLUGGED) || defined(SENSOR1_SIMU_PLUGGED)
	debugSensorCommandSet(ucSensorId, usOffset, usSize, pBuf);
	#endif
	DxOSensor_Itf[ucSensorId]->CommandSet( usOffset, usSize, pBuf );
}

void DxOISP_SensorCommandGroupClose(uint8_t ucSensorId)
{
	#if defined(SENSOR0_SIMU_PLUGGED) || defined(SENSOR1_SIMU_PLUGGED)
	debugSensorCommandGroupClose(ucSensorId);
	#endif
	DxOSensor_Itf[ucSensorId]->CommandGroupClose();
}

void DxOISP_SensorFire(uint8_t ucSensorId)
{
	#if defined(SENSOR0_SIMU_PLUGGED) || defined(SENSOR1_SIMU_PLUGGED)
	debugSensorFire(ucSensorId) ;
	#endif
	DxOSensor_Itf[ucSensorId]->Fire();
}

void DxOISP_SensorGetAttr(uint8_t ucSensorId, ts_SENSOR_Attr *pstSensorAttr )
{
	#if defined(SENSOR0_SIMU_PLUGGED) || defined(SENSOR1_SIMU_PLUGGED)
	debugSensorFire(ucSensorId) ;
	#endif
	DxOSensor_Itf[ucSensorId]->GetAttr( pstSensorAttr );
}
#endif
