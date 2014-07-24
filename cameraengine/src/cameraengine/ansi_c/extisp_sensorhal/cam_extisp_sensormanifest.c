/*******************************************************************************
// (C) Copyright [2010 - 2011] Marvell International Ltd.
// All Rights Reserved
*******************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cam_extisp_sensorhal.h"

#define JION(x, y)  XJION(x, y)
#define XJION(x, y) x##y
#define static_assert(e) typedef char JION(assertion_failed_at_line_, __LINE__) [(e) ? 1 : -1]

// sensors Camera Engine has tuned in reference implementation, also an interface for Fast Validation Pass -- func_basicsensor
extern _CAM_SmartSensorFunc func_gt2005;
extern _CAM_SmartSensorFunc func_ov5642;
extern _CAM_SmartSensorFunc func_icphd;
extern _CAM_SmartSensorFunc func_basicsensor;
#if defined( BUILD_OPTION_DEBUG_ENABLE_FAKE_SENSOR )
extern _CAM_SmartSensorFunc func_fakesensor;
#endif

#if !defined( BUILD_OPTION_STRATEGY_DISABLE_DYNAMIC_SENSOR_DETECT )
const _CAM_SmartSensorEntry gSmartSensorTable[] =
{
	{"marvell_ccic", &func_basicsensor},
};
#else
// XXX: add sensor's in your platforms here in BUILD_OPTION_STRATEGY_DISABLE_DYNAMIC_SENSOR_DETECT option, we will only detect only these sensors
const _CAM_SmartSensorEntry gSmartSensorTable[] =
{
	{"marvell_ccic", &func_basicsensor},
};
#endif

static_assert( sizeof( gSmartSensorTable ) >= sizeof( _CAM_SmartSensorEntry ) );
const CAM_Int32s gSmartSensorCnt = _ARRAY_SIZE( gSmartSensorTable );
