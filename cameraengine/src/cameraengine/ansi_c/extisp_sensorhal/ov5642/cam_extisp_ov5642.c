/*******************************************************************************
//(C) Copyright [2010 - 2011] Marvell International Ltd.
//All Rights Reserved
*******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "CameraOSAL.h"
#include "ippIP.h"

#include "cam_log.h"
#include "cam_utility.h"

#include "cam_extisp_sensorhal.h"
#include "cam_extisp_v4l2base.h"
#include "cam_extisp_ov5642.h"

// sensor vendor deliverable
#include "FaceTrack.h"

#if defined( BUILD_OPTION_WORKAROUND_V4L2_RESET_FOCUS_AFTER_STREAMON )
// XXX: since driver use power down to do stream off, which make AF related register invalid, so we need to download firmware after stream on,
//      but according test, we just need to download once even user close and re-open camera.
static CAM_Bool bIsDownloadFirmware = CAM_FALSE;
#endif

/* sensor capability tables */

/* NOTE: if you want to enable static resolution table to bypass sensor dynamically detect to save camera-off to viwerfinder-on latency,
 *       you can fill the following four tables according to your sensor's capability. And open macro
 *       BUILD_OPTION_STRATEGY_DISABLE_DYNAMIC_SENSOR_DETECT in CameraConfig.mk
 */

// video resolution
CAM_Size _OV5642VideoResTable[CAM_MAX_SUPPORT_IMAGE_SIZE_CNT] =
{
	{0, 0},
};

// still resolution
CAM_Size _OV5642StillResTable[CAM_MAX_SUPPORT_IMAGE_SIZE_CNT] =
{
	{0, 0},
};

// supported video formats
CAM_ImageFormat _OV5642VideoFormatTable[CAM_MAX_SUPPORT_IMAGE_FORMAT_CNT] =
{
	0,
};

// supported still formats
CAM_ImageFormat _OV5642StillFormatTable[CAM_MAX_SUPPORT_IMAGE_FORMAT_CNT] =
{
	0,
};


// supported flip/mirror/rotate styles
_CAM_RegEntry _OV5642Flip_None[] =
{
	{ 0x3818,5,6,5, 0x00 },
};

_CAM_RegEntry _OV5642Flip_Mirror[] =
{
	{ 0x3818,5,6,5, 0x40 },
};

_CAM_RegEntry _OV5642Flip_Flip[] =
{
	{ 0x3818,5,6,5, 0x20 },
};

_CAM_RegEntry _OV5642Flip_FM[] =
{
	{ 0x3818,5,6,5, 0x60 },
};

_CAM_ParameterEntry _OV5642FlipRotation[] =
{
	PARAM_ENTRY_USE_REGTABLE( CAM_FLIPROTATE_NORMAL, _OV5642Flip_None ),
	PARAM_ENTRY_USE_REGTABLE( CAM_FLIPROTATE_HFLIP,  _OV5642Flip_Mirror ),
	PARAM_ENTRY_USE_REGTABLE( CAM_FLIPROTATE_VFLIP,  _OV5642Flip_Flip ),
	PARAM_ENTRY_USE_REGTABLE( CAM_FLIPROTATE_180,    _OV5642Flip_FM ),
};

/* shooting parameters  */

// shot mode
_CAM_RegEntry _OV5642ShotMode_Auto[] =
{
	{ 0x3503,0,2,0, 0x00 },
	{ 0x3a05,0,7,0, 0x30 },
	{ 0x3a14,0,7,0, 0x03 },
	{ 0x3a15,0,7,0, 0x75 },
	{ 0x3a00,0,7,0, 0x78 },
};

_CAM_RegEntry _OV5642ShotMode_Portrait[] =
{
	{ 0x3503,0,2,0, 0x00 },
	{ 0x3a05,0,7,0, 0x30 },
	{ 0x3a14,0,7,0, 0x03 },
	{ 0x3a15,0,7,0, 0x75 },
	{ 0x3a00,0,7,0, 0x78 },
};

_CAM_RegEntry _OV5642ShotMode_Night[] =
{
	{ 0x3a05,0,7,0, 0x30},
	{ 0x3a02,0,7,0, 0x00},
	{ 0x3a03,0,7,0, 0xbb},
	{ 0x3a04,0,7,0, 0x80},
	{ 0x3a14,0,7,0, 0x00},
	{ 0x3a15,0,7,0, 0xbb},
	{ 0x3a16,0,7,0, 0x80},
	{ 0x3a00,0,7,0, 0x7c},
};

_CAM_ParameterEntry _OV564ShotMode[] =
{
	PARAM_ENTRY_USE_REGTABLE(CAM_SHOTMODE_AUTO,       _OV5642ShotMode_Auto),
	// PARAM_ENTRY_USE_REGTABLE(CAM_SHOTMODE_PORTRAIT,   _OV5642ShotMode_Portrait),
	PARAM_ENTRY_USE_REGTABLE(CAM_SHOTMODE_NIGHTSCENE, _OV5642ShotMode_Night),
};

// exposure mode
_CAM_RegEntry _OV5642ExpMode_Auto[] =
{
	{ OV5642_REG_AECAGC, 0x00 },
};

_CAM_RegEntry _OV5642ExpMode_Manual[] =
{
	{ OV5642_REG_AECAGC, 0x07 },
};

_CAM_ParameterEntry _OV5642ExpMode[] =
{
	PARAM_ENTRY_USE_REGTABLE(CAM_EXPOSUREMODE_AUTO,   _OV5642ExpMode_Auto),
	PARAM_ENTRY_USE_REGTABLE(CAM_EXPOSUREMODE_MANUAL, _OV5642ExpMode_Manual),
};

// exposure metering mode
_CAM_RegEntry _OV5642ExpMeter_Mean[] =
{
	{ OV5642_REG_EM_WIN0WEI, 0x01},
	{ OV5642_REG_EM_WIN1WEI, 0x10},
	{ OV5642_REG_EM_WIN2WEI, 0x01},
	{ OV5642_REG_EM_WIN3WEI, 0x10},
	{ OV5642_REG_EM_WIN4WEI, 0x01},
	{ OV5642_REG_EM_WIN5WEI, 0x10},
	{ OV5642_REG_EM_WIN6WEI, 0x01},
	{ OV5642_REG_EM_WIN7WEI, 0x10},
	{ OV5642_REG_EM_WIN8WEI, 0x01},
	{ OV5642_REG_EM_WIN9WEI, 0x10},
	{ OV5642_REG_EM_WIN10WEI,0x01},
	{ OV5642_REG_EM_WIN11WEI,0x10},
	{ OV5642_REG_EM_WIN12WEI,0x01},
	{ OV5642_REG_EM_WIN13WEI,0x10},
	{ OV5642_REG_EM_WIN14WEI,0x01},
	{ OV5642_REG_EM_WIN15WEI,0x10},
};

_CAM_RegEntry _OV5642ExpMeter_CW[] =
{
	{ OV5642_REG_EM_WIN0WEI, 0x02},
	{ OV5642_REG_EM_WIN1WEI, 0x60},
	{ OV5642_REG_EM_WIN2WEI, 0x06},
	{ OV5642_REG_EM_WIN3WEI, 0x20},
	{ OV5642_REG_EM_WIN4WEI, 0x06},
	{ OV5642_REG_EM_WIN5WEI, 0xe0},
	{ OV5642_REG_EM_WIN6WEI, 0x0e},
	{ OV5642_REG_EM_WIN7WEI, 0x60},
	{ OV5642_REG_EM_WIN8WEI, 0x0a},
	{ OV5642_REG_EM_WIN9WEI, 0xe0},
	{ OV5642_REG_EM_WIN10WEI,0x0e},
	{ OV5642_REG_EM_WIN11WEI,0xa0},
	{ OV5642_REG_EM_WIN12WEI,0x06},
	{ OV5642_REG_EM_WIN13WEI,0xa0},
	{ OV5642_REG_EM_WIN14WEI,0x0a},
	{ OV5642_REG_EM_WIN15WEI,0x60},
};

// TODO: this spot metering reference take 1920*1080 as the whole image window.
//       if you want smaller image window's spot metering, pls contact
//       with OVT
_CAM_RegEntry _OV5642ExpMeter_Spot[] =
{
	{ OV5642_REG_EM_WIN0WEI, 0x01},
	{ OV5642_REG_EM_WIN1WEI, 0x10},
	{ OV5642_REG_EM_WIN2WEI, 0x01},
	{ OV5642_REG_EM_WIN3WEI, 0x10},
	{ OV5642_REG_EM_WIN4WEI, 0x01},
	{ OV5642_REG_EM_WIN5WEI, 0x10},
	{ OV5642_REG_EM_WIN6WEI, 0x01},
	{ OV5642_REG_EM_WIN7WEI, 0x10},
	{ OV5642_REG_EM_WIN8WEI, 0x01},
	{ OV5642_REG_EM_WIN9WEI, 0x10},
	{ OV5642_REG_EM_WIN10WEI,0x01},
	{ OV5642_REG_EM_WIN11WEI,0x10},
	{ OV5642_REG_EM_WIN12WEI,0x01},
	{ OV5642_REG_EM_WIN13WEI,0x10},
	{ OV5642_REG_EM_WIN14WEI,0x01},
	{ OV5642_REG_EM_WIN15WEI,0x10},

};

_CAM_RegEntry _OV5642ExpMeter_Manual[] =
{
	{ OV5642_REG_EM_WIN0WEI, 0x01},
	{ OV5642_REG_EM_WIN1WEI, 0x10},
	{ OV5642_REG_EM_WIN2WEI, 0x01},
	{ OV5642_REG_EM_WIN3WEI, 0x10},
	{ OV5642_REG_EM_WIN4WEI, 0x01},
	{ OV5642_REG_EM_WIN5WEI, 0x10},
	{ OV5642_REG_EM_WIN6WEI, 0x01},
	{ OV5642_REG_EM_WIN7WEI, 0x10},
	{ OV5642_REG_EM_WIN8WEI, 0x01},
	{ OV5642_REG_EM_WIN9WEI, 0x10},
	{ OV5642_REG_EM_WIN10WEI,0x01},
	{ OV5642_REG_EM_WIN11WEI,0x10},
	{ OV5642_REG_EM_WIN12WEI,0x01},
	{ OV5642_REG_EM_WIN13WEI,0x10},
	{ OV5642_REG_EM_WIN14WEI,0x01},
	{ OV5642_REG_EM_WIN15WEI,0x10},

};

_CAM_ParameterEntry _OV5642ExpMeterMode[] =
{
	PARAM_ENTRY_USE_REGTABLE(CAM_EXPOSUREMETERMODE_AUTO,            _OV5642ExpMeter_Mean),
	PARAM_ENTRY_USE_REGTABLE(CAM_EXPOSUREMETERMODE_MEAN,            _OV5642ExpMeter_Mean),
	PARAM_ENTRY_USE_REGTABLE(CAM_EXPOSUREMETERMODE_CENTRALWEIGHTED, _OV5642ExpMeter_CW),
	PARAM_ENTRY_USE_REGTABLE(CAM_EXPOSUREMETERMODE_SPOT,            _OV5642ExpMeter_Spot),
	PARAM_ENTRY_USE_REGTABLE(CAM_EXPOSUREMETERMODE_MANUAL,          _OV5642ExpMeter_Manual),
};

// ISO setting
_CAM_RegEntry _OV5642IsoMode_Auto[] =
{
	{OV5642_REG_SET_ISOLOW, 0x7c},
};

_CAM_RegEntry _OV5642IsoMode_100[] =
{
	{OV5642_REG_SET_ISOHIGH, 0x00},
	{OV5642_REG_SET_ISOLOW,  0x1f},
};

_CAM_RegEntry _OV5642IsoMode_200[] =
{
	{OV5642_REG_SET_ISOHIGH, 0x00},
	{OV5642_REG_SET_ISOLOW,  0x3e},
};

_CAM_RegEntry _OV5642IsoMode_400[] =
{
	{OV5642_REG_SET_ISOHIGH, 0x00},
	{OV5642_REG_SET_ISOLOW,  0x7c},
};

_CAM_RegEntry _OV5642IsoMode_800[] =
{
	{OV5642_REG_SET_ISOHIGH, 0x00},
	{OV5642_REG_SET_ISOLOW,  0xf8},
};

_CAM_RegEntry _OV5642IsoMode_1600[] =
{
	{OV5642_REG_SET_ISOHIGH, 0x01},
	{OV5642_REG_SET_ISOLOW,  0xf0},
};

_CAM_ParameterEntry _OV5642IsoMode[] =
{
	PARAM_ENTRY_USE_REGTABLE(CAM_ISO_AUTO, _OV5642IsoMode_Auto),
	PARAM_ENTRY_USE_REGTABLE(CAM_ISO_100,  _OV5642IsoMode_100),
	PARAM_ENTRY_USE_REGTABLE(CAM_ISO_200,  _OV5642IsoMode_200),
	PARAM_ENTRY_USE_REGTABLE(CAM_ISO_400,  _OV5642IsoMode_400),
	PARAM_ENTRY_USE_REGTABLE(CAM_ISO_800,  _OV5642IsoMode_800),
	PARAM_ENTRY_USE_REGTABLE(CAM_ISO_1600, _OV5642IsoMode_1600),
};

// band filter setting, relative with sensor clock frequency
_CAM_RegEntry _OV5642BdFltMode_Auto[] =
{
	{OV5642_REG_BF_CTL,0x20},
	{0x3623, 0, 7, 0,  0x01},
	{0x3630, 0, 7, 0,  0x24},
	{0x3633, 0, 7, 0,  0x00},
	{0x3c00, 0, 7, 0,  0x00},
	{0x3c01, 0, 7, 0,  0x34},
	{0x3c04, 0, 7, 0,  0x28},
	{0x3c05, 0, 7, 0,  0x98},
	{0x3c06, 0, 7, 0,  0x00},
	{0x3c07, 0, 7, 0,  0x07},
	{0x3c08, 0, 7, 0,  0x01},
	{0x3c09, 0, 7, 0,  0xc2},
	{0x300d, 0, 7, 0,  0x02},
	{0x3104, 0, 7, 0,  0x01},
	{0x3c0a, 0, 7, 0,  0x4e},
	{0x3c0b, 0, 7, 0,  0x1f},
};

_CAM_RegEntry _OV5642BdFltMode_Off[] =
{
	{OV5642_REG_BF_CTL, 0x00},
};

_CAM_RegEntry _OV5642BdFltMode_50Hz[] =
{
	{OV5642_REG_BF_CTL,0x20},
	{0x3c01, 7, 7, 7,  0x80},
	{0x3c00, 0, 7, 0,  0x04},
};

_CAM_RegEntry _OV5642BdFltMode_60Hz[] =
{
	{OV5642_REG_BF_CTL, 0x20},
	{0x3c01, 7, 7, 7,   0x80},
	{0x3c00, 0, 7, 0,   0x00},
};

_CAM_ParameterEntry _OV5642BdFltMode[] =
{
	PARAM_ENTRY_USE_REGTABLE(CAM_BANDFILTER_AUTO, _OV5642BdFltMode_Auto),
	PARAM_ENTRY_USE_REGTABLE(CAM_BANDFILTER_OFF,  _OV5642BdFltMode_Off),
	PARAM_ENTRY_USE_REGTABLE(CAM_BANDFILTER_50HZ, _OV5642BdFltMode_50Hz),
	PARAM_ENTRY_USE_REGTABLE(CAM_BANDFILTER_60HZ, _OV5642BdFltMode_60Hz),
};


// white balance mode
_CAM_RegEntry _OV5642WBMode_Auto[] =
{
	{OV5642_REG_WB_CTL, 0x00},
};

_CAM_RegEntry _OV5642WBMode_Incandescent[] =
{
	{OV5642_REG_WB_CTL,  0x01},
	{OV5642_REG_WB_RHIGH,0x04},
	{OV5642_REG_WB_RLOW, 0x88},
	{OV5642_REG_WB_GHIGH,0x04},
	{OV5642_REG_WB_GLOW, 0x00},
	{OV5642_REG_WB_BHIGH,0x08},
	{OV5642_REG_WB_BLOW, 0xb6},
};

_CAM_RegEntry _OV5642WBMode_CoolFluorescent[] =
{
	{OV5642_REG_WB_CTL,  0x01},
	{OV5642_REG_WB_RHIGH,0x06},
	{OV5642_REG_WB_RLOW, 0x2a},
	{OV5642_REG_WB_GHIGH,0x04},
	{OV5642_REG_WB_GLOW, 0x00},
	{OV5642_REG_WB_BHIGH,0x07},
	{OV5642_REG_WB_BLOW, 0x24},
};

_CAM_RegEntry _OV5642WBMode_Daylight[] =
{
	{OV5642_REG_WB_CTL,  0x01},
	{OV5642_REG_WB_RHIGH,0x07},
	{OV5642_REG_WB_RLOW, 0x02},
	{OV5642_REG_WB_GHIGH,0x04},
	{OV5642_REG_WB_GLOW, 0x00},
	{OV5642_REG_WB_BHIGH,0x05},
	{OV5642_REG_WB_BLOW, 0x15},
};

_CAM_RegEntry _OV5642WBMode_Cloudy[] =
{
	{OV5642_REG_WB_CTL,  0x01},
	{OV5642_REG_WB_RHIGH,0x07},
	{OV5642_REG_WB_RLOW, 0x88},
	{OV5642_REG_WB_GHIGH,0x04},
	{OV5642_REG_WB_GLOW, 0x00},
	{OV5642_REG_WB_BHIGH,0x05},
	{OV5642_REG_WB_BLOW, 0x00},
};

_CAM_RegEntry _OV5642WBMode_Lock[] =
{
	{OV5642_REG_WB_CTL, 0x01},
};

_CAM_ParameterEntry _OV5642WBMode[] =
{
	PARAM_ENTRY_USE_REGTABLE(CAM_WHITEBALANCEMODE_AUTO,                  _OV5642WBMode_Auto),
	PARAM_ENTRY_USE_REGTABLE(CAM_WHITEBALANCEMODE_INCANDESCENT,          _OV5642WBMode_Incandescent),
	PARAM_ENTRY_USE_REGTABLE(CAM_WHITEBALANCEMODE_COOLWHITE_FLUORESCENT, _OV5642WBMode_CoolFluorescent),
	PARAM_ENTRY_USE_REGTABLE(CAM_WHITEBALANCEMODE_DAYLIGHT,              _OV5642WBMode_Daylight),
	PARAM_ENTRY_USE_REGTABLE(CAM_WHITEBALANCEMODE_CLOUDY,                _OV5642WBMode_Cloudy),
	PARAM_ENTRY_USE_REGTABLE(CAM_WHITEBALANCEMODE_LOCK,                  _OV5642WBMode_Lock),
};


// color effect mode
_CAM_RegEntry _OV5642ColorEffectMode_Off[] =
{
	{OV5642_REG_SDE_CTL,     0x00},
	{OV5642_REG_SAT_CTL,     0x00},
	{OV5642_REG_TONE_CTL,    0x00},
	{OV5642_REG_NEG_CTL,     0x00},
	{OV5642_REG_SOLARIZE_CTL,0x00},
};

_CAM_RegEntry _OV5642ColorEffectMode_Vivid[] =
{
	{OV5642_REG_SDE_CTL,     0x80},
	{OV5642_REG_SAT_CTL,     0x02},
	{OV5642_REG_TONE_CTL,    0x00},
	{OV5642_REG_GRAY_CTL,    0x00},
	{OV5642_REG_NEG_CTL,     0x00},
	{OV5642_REG_SOLARIZE_CTL,0x00},
	{OV5642_REG_SAT_U,       0x70},
	{OV5642_REG_SAT_V,       0x70},
};

_CAM_RegEntry _OV5642ColorEffectMode_Sepia[] =
{
	{OV5642_REG_SDE_CTL,     0x80},
	{OV5642_REG_TONE_CTL,    0x18},
	{OV5642_REG_NEG_CTL,     0x00},
	{OV5642_REG_SOLARIZE_CTL,0x00},
	{OV5642_REG_U_FIX,       0x40},
	{OV5642_REG_V_FIX,       0xa0},
};

_CAM_RegEntry _OV5642ColorEffectMode_Grayscale[] =
{
	{OV5642_REG_SDE_CTL,     0x80},
	{OV5642_REG_TONE_CTL,    0x18},
	{OV5642_REG_NEG_CTL,     0x00},
	{OV5642_REG_SAT_CTL,     0x00},
	{OV5642_REG_SOLARIZE_CTL,0x00},
	{OV5642_REG_U_FIX,       0x80},
	{OV5642_REG_V_FIX,       0x80},
};


_CAM_RegEntry _OV5642ColorEffectMode_Negative[] =
{
	{OV5642_REG_SDE_CTL,     0x80},
	{OV5642_REG_NEG_CTL,     0x40},
	{OV5642_REG_TONE_CTL,    0x00},
	{OV5642_REG_SAT_CTL,     0x00},
	{OV5642_REG_SOLARIZE_CTL,0x00},
};

_CAM_RegEntry _OV5642ColorEffectMode_Solarize[] =
{
	{OV5642_REG_SDE_CTL,     0x80},
	{OV5642_REG_NEG_CTL,     0x00},
	{OV5642_REG_TONE_CTL,    0x00},
	{OV5642_REG_SAT_CTL,     0x00},
	{OV5642_REG_SOLARIZE_CTL,0x80},
};


_CAM_ParameterEntry _OV5642ColorEffectMode[] =
{
	PARAM_ENTRY_USE_REGTABLE(CAM_COLOREFFECT_OFF,       _OV5642ColorEffectMode_Off),
	PARAM_ENTRY_USE_REGTABLE(CAM_COLOREFFECT_VIVID,     _OV5642ColorEffectMode_Vivid),
	PARAM_ENTRY_USE_REGTABLE(CAM_COLOREFFECT_SEPIA,     _OV5642ColorEffectMode_Sepia),
	PARAM_ENTRY_USE_REGTABLE(CAM_COLOREFFECT_GRAYSCALE, _OV5642ColorEffectMode_Grayscale),
	PARAM_ENTRY_USE_REGTABLE(CAM_COLOREFFECT_NEGATIVE,  _OV5642ColorEffectMode_Negative),
	PARAM_ENTRY_USE_REGTABLE(CAM_COLOREFFECT_SOLARIZE,  _OV5642ColorEffectMode_Solarize),
};

// EV compensation
_CAM_RegEntry _OV5642EvComp_N12[] =
{
	{OV5642_REG_AEC_ENT_HIGH,0x18},
	{OV5642_REG_AEC_ENT_LOW, 0x10},
	{OV5642_REG_AEC_OUT_HIGH,0x18},
	{OV5642_REG_AEC_OUT_LOW, 0x10},
	{OV5642_REG_AEC_VPT_HIGH,0x30},
	{OV5642_REG_AEC_VPT_LOW, 0x10},
};

_CAM_RegEntry _OV5642EvComp_N09[] =
{
	{OV5642_REG_AEC_ENT_HIGH,0x20},
	{OV5642_REG_AEC_ENT_LOW, 0x18},
	{OV5642_REG_AEC_OUT_HIGH,0x20},
	{OV5642_REG_AEC_OUT_LOW, 0x18},
	{OV5642_REG_AEC_VPT_HIGH,0x41},
	{OV5642_REG_AEC_VPT_LOW, 0x10},
};

_CAM_RegEntry _OV5642EvComp_N06[] =
{
	{OV5642_REG_AEC_ENT_HIGH,0x28},
	{OV5642_REG_AEC_ENT_LOW, 0x20},
	{OV5642_REG_AEC_OUT_HIGH,0x28},
	{OV5642_REG_AEC_OUT_LOW, 0x20},
	{OV5642_REG_AEC_VPT_HIGH,0x51},
	{OV5642_REG_AEC_VPT_LOW, 0x10},
};

_CAM_RegEntry _OV5642EvComp_N03[] =
{
	{OV5642_REG_AEC_ENT_HIGH,0x30},
	{OV5642_REG_AEC_ENT_LOW, 0x28},
	{OV5642_REG_AEC_OUT_HIGH,0x30},
	{OV5642_REG_AEC_OUT_LOW, 0x28},
	{OV5642_REG_AEC_VPT_HIGH,0x61},
	{OV5642_REG_AEC_VPT_LOW, 0x10},
};

_CAM_RegEntry _OV5642EvComp_000[] =
{
	{OV5642_REG_AEC_ENT_HIGH,0x38},
	{OV5642_REG_AEC_ENT_LOW, 0x30},
	{OV5642_REG_AEC_OUT_HIGH,0x38},
	{OV5642_REG_AEC_OUT_LOW, 0x30},
	{OV5642_REG_AEC_VPT_HIGH,0x61},
	{OV5642_REG_AEC_VPT_LOW, 0x10},
};

_CAM_RegEntry _OV5642EvComp_P03[] =
{
	{OV5642_REG_AEC_ENT_HIGH,0x40},
	{OV5642_REG_AEC_ENT_LOW, 0x38},
	{OV5642_REG_AEC_OUT_HIGH,0x40},
	{OV5642_REG_AEC_OUT_LOW, 0x38},
	{OV5642_REG_AEC_VPT_HIGH,0x71},
	{OV5642_REG_AEC_VPT_LOW, 0x10},
};

_CAM_RegEntry _OV5642EvComp_P06[] =
{
	{OV5642_REG_AEC_ENT_HIGH,0x48},
	{OV5642_REG_AEC_ENT_LOW, 0x40},
	{OV5642_REG_AEC_OUT_HIGH,0x48},
	{OV5642_REG_AEC_OUT_LOW, 0x40},
	{OV5642_REG_AEC_VPT_HIGH,0x80},
	{OV5642_REG_AEC_VPT_LOW, 0x20},
};

_CAM_RegEntry _OV5642EvComp_P09[] =
{
	{OV5642_REG_AEC_ENT_HIGH,0x50},
	{OV5642_REG_AEC_ENT_LOW, 0x48},
	{OV5642_REG_AEC_OUT_HIGH,0x50},
	{OV5642_REG_AEC_OUT_LOW, 0x48},
	{OV5642_REG_AEC_VPT_HIGH,0x90},
	{OV5642_REG_AEC_VPT_LOW, 0x20},
};

_CAM_RegEntry _OV5642EvComp_P12[] =
{
	{OV5642_REG_AEC_ENT_HIGH,0x58},
	{OV5642_REG_AEC_ENT_LOW, 0x50},
	{OV5642_REG_AEC_OUT_HIGH,0x58},
	{OV5642_REG_AEC_OUT_LOW, 0x50},
	{OV5642_REG_AEC_VPT_HIGH,0x91},
	{OV5642_REG_AEC_VPT_LOW, 0x20},
};

_CAM_ParameterEntry _OV5642EvCompMode[] =
{
	PARAM_ENTRY_USE_REGTABLE(-78644,  _OV5642EvComp_N12),
	PARAM_ENTRY_USE_REGTABLE(-58983,  _OV5642EvComp_N09),
	PARAM_ENTRY_USE_REGTABLE(-39322,  _OV5642EvComp_N06),
	PARAM_ENTRY_USE_REGTABLE(-19661,  _OV5642EvComp_N03),
	PARAM_ENTRY_USE_REGTABLE(0,       _OV5642EvComp_000),
	PARAM_ENTRY_USE_REGTABLE(19661,   _OV5642EvComp_P03),
	PARAM_ENTRY_USE_REGTABLE(39322,   _OV5642EvComp_P06),
	PARAM_ENTRY_USE_REGTABLE(58983,   _OV5642EvComp_P09),
	PARAM_ENTRY_USE_REGTABLE(78644,   _OV5642EvComp_P12),
};


// focus mode
_CAM_RegEntry _OV5642FocusMode_AutoOneShot[] =
{
	{0, 0, 0, 0, 0},
};

_CAM_RegEntry _OV5642FocusMode_AutoContinuous[] =
{
	{OV5642_REG_AF_ACK,   0x01},
	{OV5642_REG_AF_MAIN,  0x04},
};


_CAM_RegEntry _OV5642FocusMode_Macro[] =
{
	{OV5642_REG_AF_PARA3, 0x00},
	{OV5642_REG_AF_PARA4, 0xff},
	{OV5642_REG_AF_ACK,   0x01},
	{OV5642_REG_AF_MAIN,  0x1a},
};

_CAM_RegEntry _OV5642FocusMode_Infinity[] =
{
	{OV5642_REG_AF_PARA3, 0x00},
	{OV5642_REG_AF_PARA4, 0x00},
	{OV5642_REG_AF_ACK,   0x01},
	{OV5642_REG_AF_MAIN,  0x1a},
};

_CAM_ParameterEntry _OV5642FocusMode[] =
{
	PARAM_ENTRY_USE_REGTABLE(CAM_FOCUS_INFINITY,               _OV5642FocusMode_Infinity),
#if defined( BUILD_OPTION_STRATEGY_ENABLE_AUTOFOCUS )
	PARAM_ENTRY_USE_REGTABLE(CAM_FOCUS_AUTO_ONESHOT,           _OV5642FocusMode_AutoOneShot),
	PARAM_ENTRY_USE_REGTABLE(CAM_FOCUS_AUTO_CONTINUOUS_PICTURE,_OV5642FocusMode_AutoContinuous),
	PARAM_ENTRY_USE_REGTABLE(CAM_FOCUS_MACRO,                  _OV5642FocusMode_Macro),
#endif
};

// focus zone
_CAM_RegEntry _OV5642FocusZone_Center[] =
{
	{ OV5642_REG_AF_PARA0, 40 },
	{ OV5642_REG_AF_PARA1, 30 },
	{ OV5642_REG_AF_ACK,  0x01},
	{ OV5642_REG_AF_MAIN, 0x81},
};

_CAM_RegEntry _OV5642FocusZone_Multizone1[] =
{
	{0, 0, 0, 0, 0},
};

_CAM_RegEntry _OV5642FocusZone_Multizone2[] =
{
	{0, 0, 0, 0, 0},
};

_CAM_RegEntry _OV5642FocusZone_Manual[] =
{
	{0, 0, 0, 0, 0},
};

_CAM_ParameterEntry _OV5642FocusZone[] =
{
#if defined( BUILD_OPTION_STRATEGY_ENABLE_AUTOFOCUS )
	PARAM_ENTRY_USE_REGTABLE(CAM_FOCUSZONEMODE_CENTER,     _OV5642FocusZone_Center),
	// PARAM_ENTRY_USE_REGTABLE(CAM_FOCUSZONEMODE_MULTIZONE1, _OV5642FocusZone_Multizone1),
	// PARAM_ENTRY_USE_REGTABLE(CAM_FOCUSZONEMODE_MULTIZONE2, _OV5642FocusZone_Multizone2),
	PARAM_ENTRY_USE_REGTABLE(CAM_FOCUSZONEMODE_MANUAL,     _OV5642FocusZone_Manual),
#endif
};


// flash Mode
_CAM_RegEntry _OV5642FlashMode_Off[] =
{
	{OV5642_REG_FLASH_TYPE,  0x03},
	{OV5642_REG_FLASH_ONOFF, 0x00},
};

_CAM_RegEntry _OV5642FlashMode_On[] =
{
	{OV5642_REG_FLASH_TYPE,  0x03},
	{OV5642_REG_FLASH_ONOFF, 0x80},
};

_CAM_RegEntry _OV5642FlashMode_Torch[] =
{
	{0, 0, 0, 0, 0},
};

_CAM_RegEntry _OV5642FlashMode_Auto[] =
{
	{0, 0, 0, 0, 0},
};

_CAM_RegEntry _OV5642FlashMode_RedEye[] =
{
	{0, 0, 0, 0, 0},
};

_CAM_ParameterEntry _OV5642FlashMode[] =
{
#if defined( BUILD_OPTION_STRATEGY_ENABLE_FLASH )
	PARAM_ENTRY_USE_REGTABLE(CAM_FLASH_OFF,     _OV5642FlashMode_Off),
	PARAM_ENTRY_USE_REGTABLE(CAM_FLASH_ON,      _OV5642FlashMode_On),
	PARAM_ENTRY_USE_REGTABLE(CAM_FLASH_TORCH,   _OV5642FlashMode_Torch),
	PARAM_ENTRY_USE_REGTABLE(CAM_FLASH_AUTO,    _OV5642FlashMode_Auto),
	PARAM_ENTRY_USE_REGTABLE(CAM_FLASH_REDEYE,  _OV5642FlashMode_RedEye),
#endif
};


// function declaration
static CAM_Error _OV5642SaveAeAwb( void* );
static CAM_Error _OV5642RestoreAeAwb( void* );
static CAM_Error _OV5642StartFlash( void* );
static CAM_Error _OV5642StopFlash( void* );

static CAM_Error _OV5642SetDigitalZoom( OV5642State*, CAM_Int32s );
static CAM_Error _OV5642UpdateFocusZone( OV5642State*, CAM_FocusZoneMode, CAM_MultiROI* );
static CAM_Error _OV5642SetFrameRate( OV5642State*, CAM_Int32s );
static CAM_Error _OV5642FillFrameShotInfo( OV5642State*, CAM_ImageBuffer* );
static CAM_Error _OV5642SetJpegParam( OV5642State*, CAM_JpegParam* );
static CAM_Error _OV5642SetBandFilter( CAM_Int32s );
static CAM_Error _OV5642InitSetting( OV5642State*, _CAM_ShotParam* );
static CAM_Error _OV5642SetFocusMode( OV5642State*, CAM_FocusMode );

// shot mode capability
static void _OV5642GetShotParamCap( CAM_ShotModeCapability* );
static void _OV5642AutoModeCap( CAM_ShotModeCapability* );
static void _OV5642PortraitModeCap( CAM_ShotModeCapability* );
static void _OV5642NightModeCap( CAM_ShotModeCapability* );

// face detection
unsigned int      _OV5642FaceDetectCallback( unsigned int value );
static CAM_Int32s _OV5642FaceDetectionThread( OV5642State *pState );
static CAM_Int32s _YCbCr422ToGrayRsz_8u_C2_C( const Ipp8u *pSrc, int srcStep,  IppiSize srcSize, Ipp8u *pDst, int dstStep, IppiSize dstSize );
static CAM_Error  _YUVToGrayRsz_8u( CAM_ImageBuffer *pImgBuf, CAM_Int8u *pDstBuf, CAM_Size stDstImgSize );

static CAM_Int32s _ov5642_set_paramsreg( CAM_Int32s, _CAM_ParameterEntry*, CAM_Int32s, CAM_Int32s );
static CAM_Int32s _ov5642_set_paramsreg_nearest( CAM_Int32s*, _CAM_ParameterEntry*, CAM_Int32s, CAM_Int32s );
static CAM_Int32s _ov5642_wait_afcmd_ready( CAM_Int32s );
static CAM_Int32s _ov5642_wait_afcmd_completed( CAM_Int32s );
static CAM_Error  _ov5642_save_whitebalance( CAM_Int32s iSensorFD, _OV5642_3AState *pCurrent3A );
static CAM_Error  _ov5642_set_whitebalance( CAM_Int32s iSensorFD, _OV5642_3AState *p3Astat );
static CAM_Error  _ov5642_save_exposure( CAM_Int32s iSensorFD, _OV5642_3AState *pCurrent3A );
static CAM_Error  _ov5642_set_exposure( CAM_Int32s iSensorFD, double dNewExposureValue );
static CAM_Error  _ov5642_calc_framerate( CAM_Int32s iSensorFD, CAM_Int32s *pFrameRate100 );
static CAM_Error  _ov5642_update_ref_roi( OV5642State *pState );
static CAM_Error  _ov5642_update_metering_window( OV5642State *pState, CAM_ExposureMeterMode eExpMeterMode, CAM_MultiROI *pNewMultiROI );
static CAM_Error  _ov5642_pre_focus( OV5642State *pState );

static inline CAM_Error _ov5642_calc_sysclk( CAM_Int32s iSensorFD, double *pSysClk );
static inline CAM_Error _ov5642_calc_hvts( CAM_Int32s iSensorFD, CAM_Int32s *pHts, CAM_Int32s *pVts );

// shot mode cap function table
OV5642_ShotModeCap _OV5642ShotModeCap[CAM_SHOTMODE_NUM] = { _OV5642AutoModeCap    ,   // CAM_SHOTMODE_AUTO = 0,
                                                            NULL                  ,   // CAM_SHOTMODE_MANUAL,
                                                            _OV5642PortraitModeCap,   // CAM_SHOTMODE_PORTRAIT,
                                                            NULL                  ,   // CAM_SHOTMODE_LANDSCAPE,
                                                            NULL                  ,   // CAM_SHOTMODE_NIGHTPORTRAIT,
                                                            _OV5642NightModeCap   ,   // CAM_SHOTMODE_NIGHTSCENE,
                                                            NULL                  ,   // CAM_SHOTMODE_CHILD,
                                                            NULL                  ,   // CAM_SHOTMODE_INDOOR,
                                                            NULL                  ,   // CAM_SHOTMODE_PLANTS,
                                                            NULL                  ,   // CAM_SHOTMODE_SNOW,
                                                            NULL                  ,   // CAM_SHOTMODE_BEACH,
                                                            NULL                  ,   // CAM_SHOTMODE_FIREWORKS,
                                                            NULL                  ,   // CAM_SHOTMODE_SUBMARINE,
                                                            NULL                  ,   // CAM_SHOTMODE_WHITEBOARD,
                                                            NULL                  ,   // CAM_SHOTMODE_SPORTS
                                                          };

// NOTE: for OV face detection library, OV face detection lib defines many global variables, even face output is global, I think it's a *BAD* practice
static CAM_Int32s giSensorFD = -1;

/*--------------------------------------------------------------------------------------------------------------------
 * APIs 
 *-------------------------------------------------------------------------------------------------------------------*/

#if defined( PLATFORM_BOARD_BROWNSTONE )

#define OV5642_FACING CAM_SENSOR_FACING_FRONT

#else

#define OV5642_FACING CAM_SENSOR_FACING_BACK

#endif

extern _CAM_SmartSensorFunc func_ov5642;
CAM_Error OV5642_SelfDetect( _CAM_SmartSensorAttri *pSensorInfo )
{
	CAM_Error error = CAM_ERROR_NONE;
	
	/* NOTE:  If you open macro BUILD_OPTION_STRATEGY_DISABLE_DYNAMIC_SENSOR_DETECT in CameraConfig.mk
	 *        to bypass sensor dynamically detect to save camera-off to viwerfinder-on latency, you should initilize
	 *        _OV5642VideoResTable/_OV5642StillResTable/_OV5642VideoFormatTable/_OV5642StillFormatTable manually.
	 */
#if !defined( BUILD_OPTION_STRATEGY_DISABLE_DYNAMIC_SENSOR_DETECT )
	error = V4L2_SelfDetect( pSensorInfo, "marvell_ccic", &func_ov5642,
	                         _OV5642VideoResTable, _OV5642StillResTable,
	                         _OV5642VideoFormatTable, _OV5642StillFormatTable );
	pSensorInfo->stSensorProp.eInstallOrientation = CAM_FLIPROTATE_90R;
	pSensorInfo->stSensorProp.iFacing             = OV5642_FACING;
	pSensorInfo->stSensorProp.iOutputProp         = CAM_OUTPUT_SINGLESTREAM;
#else
	{
		_V4L2SensorEntry *pSensorEntry = (_V4L2SensorEntry*)( pSensorInfo->cReserved );
		strcpy( pSensorInfo->stSensorProp.sName, "omarvell_ccic" );
		pSensorInfo->stSensorProp.eInstallOrientation = CAM_FLIPROTATE_NORMAL;
		pSensorInfo->stSensorProp.iFacing             = CAM_SENSOR_FACING_BACK;
		pSensorInfo->stSensorProp.iOutputProp         = CAM_OUTPUT_SINGLESTREAM;

		pSensorInfo->pFunc                            = &func_ov5642;

		// FIXME: the following is just an example in Marvell platform, you should change it according to your driver implementation
		pSensorEntry->iV4L2SensorID = 1;
		strcpy( pSensorEntry->sDeviceName, "/dev/video0" );
	}
#endif
	return error;
}

CAM_Error OV5642_GetCapability( _CAM_SmartSensorCapability *pCapability )
{
	CAM_Int32s i = 0;

	// preview/video supporting 
	pCapability->iSupportedVideoFormatCnt = 0;
	for ( i = 0; i < CAM_MAX_SUPPORT_IMAGE_FORMAT_CNT; i++ )
	{
		if ( _OV5642VideoFormatTable[i] == 0 )
		{
			break;
		}
		pCapability->eSupportedVideoFormat[pCapability->iSupportedVideoFormatCnt] = _OV5642VideoFormatTable[i];
		pCapability->iSupportedVideoFormatCnt++;
	}

	pCapability->bArbitraryVideoSize     = CAM_FALSE;
	pCapability->iSupportedVideoSizeCnt  = 0;
	pCapability->stMinVideoSize.iWidth   = _OV5642VideoResTable[0].iWidth;
	pCapability->stMinVideoSize.iHeight  = _OV5642VideoResTable[0].iHeight;
	pCapability->stMaxVideoSize.iWidth   = _OV5642VideoResTable[0].iWidth;
	pCapability->stMaxVideoSize.iHeight  = _OV5642VideoResTable[0].iHeight;
	for ( i = 0; i < CAM_MAX_SUPPORT_IMAGE_SIZE_CNT; i++ )
	{
		if ( _OV5642VideoResTable[i].iWidth == 0 || _OV5642VideoResTable[i].iHeight == 0 )
		{
			break;
		}
		pCapability->stSupportedVideoSize[pCapability->iSupportedVideoSizeCnt] = _OV5642VideoResTable[i];
		pCapability->iSupportedVideoSizeCnt++;

		if ( ( pCapability->stMinVideoSize.iWidth > _OV5642VideoResTable[i].iWidth ) ||
		     ( ( pCapability->stMinVideoSize.iWidth == _OV5642VideoResTable[i].iWidth ) &&
		       ( pCapability->stMinVideoSize.iHeight > _OV5642VideoResTable[i].iHeight )
		      )
		    )
		{
			pCapability->stMinVideoSize.iWidth = _OV5642VideoResTable[i].iWidth;
			pCapability->stMinVideoSize.iHeight = _OV5642VideoResTable[i].iHeight;
		}
		if ( ( pCapability->stMaxVideoSize.iWidth < _OV5642VideoResTable[i].iWidth) ||
		     ( ( pCapability->stMaxVideoSize.iWidth == _OV5642VideoResTable[i].iWidth ) &&
		       ( pCapability->stMaxVideoSize.iHeight < _OV5642VideoResTable[i].iHeight )
		      )
		    )
		{
			pCapability->stMaxVideoSize.iWidth = _OV5642VideoResTable[i].iWidth;
			pCapability->stMaxVideoSize.iHeight = _OV5642VideoResTable[i].iHeight;
		}
	}

	// still capture supporting
	pCapability->iSupportedStillFormatCnt           = 0;
	pCapability->stSupportedJPEGParams.bSupportJpeg = CAM_FALSE;
	for ( i = 0; i < CAM_MAX_SUPPORT_IMAGE_FORMAT_CNT; i++ )
	{
		if ( _OV5642StillFormatTable[i] == CAM_IMGFMT_JPEG )
		{
			// JPEG encoder functionalities
			pCapability->stSupportedJPEGParams.bSupportJpeg      = CAM_TRUE;
			pCapability->stSupportedJPEGParams.bSupportExif      = CAM_FALSE;
			pCapability->stSupportedJPEGParams.bSupportThumb     = CAM_FALSE;
			pCapability->stSupportedJPEGParams.iMinQualityFactor = 20;
			pCapability->stSupportedJPEGParams.iMaxQualityFactor = 95;
		}
		if ( _OV5642StillFormatTable[i] == 0 )
		{
			break;
		}
		pCapability->eSupportedStillFormat[pCapability->iSupportedStillFormatCnt] = _OV5642StillFormatTable[i];
		pCapability->iSupportedStillFormatCnt++;
	}
	pCapability->bArbitraryStillSize    = CAM_FALSE;
	pCapability->iSupportedStillSizeCnt = 0;
	pCapability->stMinStillSize.iWidth  = _OV5642StillResTable[0].iWidth;
	pCapability->stMinStillSize.iHeight = _OV5642StillResTable[0].iHeight;
	pCapability->stMaxStillSize.iWidth  = _OV5642StillResTable[0].iWidth;
	pCapability->stMaxStillSize.iHeight = _OV5642StillResTable[0].iHeight;
	for ( i = 0; i < CAM_MAX_SUPPORT_IMAGE_SIZE_CNT; i++ )
	{
		if ( _OV5642StillResTable[i].iWidth == 0 || _OV5642StillResTable[i] .iHeight == 0 )
		{
			break;
		}

		pCapability->stSupportedStillSize[pCapability->iSupportedStillSizeCnt] = _OV5642StillResTable[i];
		pCapability->iSupportedStillSizeCnt++;

		if ( ( pCapability->stMinStillSize.iWidth > _OV5642StillResTable[i].iWidth ) ||
		     ( ( pCapability->stMinStillSize.iWidth == _OV5642StillResTable[i].iWidth ) &&
		       ( pCapability->stMinStillSize.iHeight > _OV5642StillResTable[i].iHeight )
		     )
		   )
		{
			pCapability->stMinStillSize.iWidth  = _OV5642StillResTable[i].iWidth;
			pCapability->stMinStillSize.iHeight = _OV5642StillResTable[i].iHeight;
		}
		if ( ( pCapability->stMaxStillSize.iWidth < _OV5642StillResTable[i].iWidth ) ||
		     ( ( pCapability->stMaxStillSize.iWidth == _OV5642StillResTable[i].iWidth ) &&
		       ( pCapability->stMaxStillSize.iHeight < _OV5642StillResTable[i].iHeight )
		     )
		   )
		{
			pCapability->stMaxStillSize.iWidth  = _OV5642StillResTable[i].iWidth;
			pCapability->stMaxStillSize.iHeight = _OV5642StillResTable[i].iHeight;
		}
	}

	// flip/rotate
	pCapability->iSupportedRotateCnt = _ARRAY_SIZE(_OV5642FlipRotation);
	for ( i = 0; i < pCapability->iSupportedRotateCnt; i++ )
	{
		pCapability->eSupportedRotate[i] = (CAM_FlipRotate)_OV5642FlipRotation[i].iParameter;
	}

	// shot mode capability
	pCapability->iSupportedShotModeCnt = _ARRAY_SIZE(_OV564ShotMode);
	for ( i = 0; i < pCapability->iSupportedShotModeCnt; i++ )
	{
		pCapability->eSupportedShotMode[i] = (CAM_ShotMode)_OV564ShotMode[i].iParameter;
	}

	// all supported shot parameters
	_OV5642GetShotParamCap( &pCapability->stSupportedShotParams );

	// whether support face detection
#if defined( BUILD_OPTION_STRATEGY_ENABLE_FACEDETECTION )
	pCapability->iMaxFaceNum = OV5642_FACEDETECTION_MAXFACENUM;
#else
	pCapability->iMaxFaceNum = 0;
#endif

	return CAM_ERROR_NONE;
}

CAM_Error OV5642_GetShotModeCapability( CAM_ShotMode eShotMode, CAM_ShotModeCapability *pShotModeCap )
{
	CAM_Int32u i     = 0;
	CAM_Error  error = CAM_ERROR_NONE;

	// BAC check
	for ( i = 0; i < _ARRAY_SIZE( _OV564ShotMode ); i++ )
	{
		if ( (CAM_ShotMode)_OV564ShotMode[i].iParameter == eShotMode )
		{
			break;
		}
	}

	if ( i >= _ARRAY_SIZE( _OV564ShotMode ) || pShotModeCap == NULL || _OV5642ShotModeCap[eShotMode] == NULL )
	{
		return CAM_ERROR_BADARGUMENT;
	}

	(_OV5642ShotModeCap[eShotMode])( pShotModeCap );

	return error;
}

CAM_Error OV5642_Init( void **ppDevice, void *pSensorEntry )
{
	CAM_Error            error        = CAM_ERROR_NONE;
	OV5642State          *pState      = (OV5642State*)malloc( sizeof(OV5642State) );
	_V4L2SensorEntry     *pSensor     = (_V4L2SensorEntry*)(pSensorEntry);
	CAM_Int32u           i            = 0;
	_CAM_ShotParam       *pShotParams = NULL;
	_V4L2SensorAttribute _OV5642Attri;

	*ppDevice = pState;
	if ( *ppDevice == NULL )
	{
		return CAM_ERROR_OUTOFMEMORY;
	}

	memset( &_OV5642Attri, 0, sizeof( _V4L2SensorAttribute ) );
	memset( pState, -1, sizeof( OV5642State ) );

	_OV5642Attri.stV4L2SensorEntry.iV4L2SensorID = pSensor->iV4L2SensorID;
	strcpy( _OV5642Attri.stV4L2SensorEntry.sDeviceName, pSensor->sDeviceName );

#if defined( BUILD_OPTION_DEBUG_DISABLE_SAVE_RESTORE_3A )
	_OV5642Attri.iUnstableFrameCnt    = 20;
	_OV5642Attri.fnSaveAeAwb          = NULL;
	_OV5642Attri.fnRestoreAeAwb       = NULL;
	_OV5642Attri.pSaveRestoreUserData = NULL;
#else
	_OV5642Attri.iUnstableFrameCnt    = 2;
	_OV5642Attri.fnSaveAeAwb          = _OV5642SaveAeAwb;
	_OV5642Attri.fnRestoreAeAwb       = _OV5642RestoreAeAwb;
	_OV5642Attri.pSaveRestoreUserData = (void*)pState;
#endif

	_OV5642Attri.fnStartFlash         = _OV5642StartFlash;
	_OV5642Attri.fnStopFlash          = _OV5642StopFlash;

	error = V4L2_Init( &pState->stV4L2, &_OV5642Attri );
	if ( error != CAM_ERROR_NONE )
	{
		free( pState );
		pState    = NULL;
		*ppDevice = NULL;
		return error;
	}

	// init settings, including: jpeg and shot parameters
	for ( i = 0; i < _ARRAY_SIZE( _OV564ShotMode ); i++ )
	{
		if ( CAM_SHOTMODE_AUTO == _OV5642DefaultShotParams[i].eShotMode )
		{
			pShotParams = &_OV5642DefaultShotParams[i];
			break;
		}
	}
	ASSERT( pShotParams != NULL );

	error = _OV5642InitSetting( pState, pShotParams );
	if ( error != CAM_ERROR_NONE )
	{
		return error;
	}
	pState->stV4L2.stShotParamSetting = *pShotParams;

	// face detection related
	giSensorFD                   = pState->stV4L2.iSensorFD;
	pState->iFaceDetectFrameSkip = 0;
	pState->pUserBuf             = NULL;
	pState->pFaceDetectProcBuf   = NULL;
	pState->pFaceDetectInputBuf  = NULL;
	
	pState->stFaceDetectThread.bExitThread                     = CAM_FALSE;
	pState->stFaceDetectThread.stThreadAttribute.hEventWakeup  = NULL;
	pState->stFaceDetectThread.stThreadAttribute.hEventExitAck = NULL;
	pState->stFaceDetectThread.iThreadID                       = -1;

	pState->bIsFaceDetectionOpen = CAM_FALSE;
	
	// init digital zoom ref ROI
	memset( &pState->stRefROI, 0, sizeof( CAM_Rect ) );
	error = _ov5642_update_ref_roi( pState );
	if ( error != CAM_ERROR_NONE )
	{
		return error;
	}

#if !defined( BUILD_OPTION_WORKAROUND_V4L2_RESET_FOCUS_AFTER_STREAMON )
	// af related
#if defined( BUILD_OPTION_STRATEGY_ENABLE_AUTOFOCUS ) || defined( BUILD_OPTION_STRATEGY_ENABLE_FACEDETECTION )
	// download OV5642 AF firmware to sensor if needed
	{
#if defined( CAM_PERF )
		CAM_Tick t = -CAM_TimeGetTickCount();
#endif
		error = V4L2_FirmwareDownload( &pState->stV4L2 );
#if defined( CAM_PERF )
		t += CAM_TimeGetTickCount();
		CELOG( "Perf: AF firmware download cost: %.2f ms\n", t / 1000.0 );
#endif
	}
#endif
#endif

	return error;
}

CAM_Error OV5642_Deinit( void **ppDevice )
{
	CAM_Error   error   = CAM_ERROR_NONE;
	CAM_Int32s  ret     = 0;
	OV5642State *pState = (OV5642State*)(*ppDevice);

	// V4L2 de-init
	error = V4L2_Deinit( &pState->stV4L2 );
	if ( error != CAM_ERROR_NONE )
	{
		return error;
	}

	// face detection related
	giSensorFD = -1;
	ASSERT( pState->bIsFaceDetectionOpen == CAM_FALSE );
	ASSERT( pState->pUserBuf == NULL );
	ASSERT( pState->stFaceDetectThread.iThreadID == -1 );

	// free handle
	free( pState );
	pState    = NULL;
	*ppDevice = NULL;

	return error;
}

CAM_Error OV5642_RegisterEventHandler( void *pDevice, CAM_EventHandler fnEventHandler, void *pUserData )
{
	OV5642State *pState = (OV5642State*)pDevice;
	CAM_Error   error   = CAM_ERROR_NONE;

	error = V4L2_RegisterEventHandler( &pState->stV4L2, fnEventHandler, pUserData );

	return error;
}

CAM_Error OV5642_Config( void *pDevice, _CAM_SmartSensorConfig *pSensorConfig )
{
	OV5642State *pState = (OV5642State*)pDevice;
	CAM_Error   error   = CAM_ERROR_NONE;
	CAM_Int32s  ret     = 0;

	// XXX: set digital zoom back to 1
	// _OV5642SetDigitalZoom( pState, 1 << 16 );

	error = V4L2_Config( &pState->stV4L2, pSensorConfig );
	if ( error != CAM_ERROR_NONE )
	{
		return error;
	}

	if ( pSensorConfig->eState != CAM_CAPTURESTATE_IDLE )
	{
		// update digital zoom reference ROI
		CELOG( "update digital zoom ref ROI( %s, %d )\n", __FILE__, __LINE__ );
		error = _ov5642_update_ref_roi( pState );
		if ( error != CAM_ERROR_NONE )
		{
			return error;
		}
		// NOTE: after each configure, zoom registers will be flushed, and need update
		pState->stV4L2.stShotParamSetting.iDigZoomQ16 = ( 1 << 16 );

		// configure JPEG quality to sensor
		if ( pSensorConfig->eFormat == CAM_IMGFMT_JPEG )
		{
			error = _OV5642SetJpegParam( pState, &pSensorConfig->stJpegParam );
			if ( error != CAM_ERROR_NONE )
			{
				return error;
			}
		}

		// restore AG/AE to current scene mode when go to preview state, since AE/AG will be set to auto according driver yuv setting
		if ( pSensorConfig->eState == CAM_CAPTURESTATE_PREVIEW && pState->stV4L2.stShotParamSetting.eExpMode != CAM_EXPOSUREMODE_MANUAL )
		{
			ret = _ov5642_set_paramsreg( pState->stV4L2.stShotParamSetting.eShotMode, _OV564ShotMode, _ARRAY_SIZE( _OV564ShotMode ), pState->stV4L2.iSensorFD );
			if ( ret == -2 )
			{
				return CAM_ERROR_NOTSUPPORTEDARG;
			}
		}

#if !defined( BUILD_OPTION_WORKAROUND_V4L2_RESET_FOCUS_AFTER_STREAMON )
		// resume CAF
		if ( pState->stV4L2.stShotParamSetting.eFocusMode == CAM_FOCUS_AUTO_CONTINUOUS_PICTURE &&
		     pState->stFocusState.bIsFocusStop == CAM_FALSE && pSensorConfig->eState == CAM_CAPTURESTATE_PREVIEW )
		{
			CELOG( "Resume CAF\n" );
			error = _ov5642_pre_focus( pState );
			if ( error != CAM_ERROR_NONE )
			{
				return error;
			}

			ret = _ov5642_set_paramsreg( CAM_FOCUS_AUTO_CONTINUOUS_PICTURE, _OV5642FocusMode, _ARRAY_SIZE( _OV5642FocusMode ), pState->stV4L2.iSensorFD );
			if ( ret == -2 )
			{
				return CAM_ERROR_NOTSUPPORTEDARG;
			}

			pState->stFocusState.bIsFocusStop = CAM_FALSE;
			pState->stFocusState.bIsFocused   = CAM_FALSE;
		}
#endif
		if( pSensorConfig->eState == CAM_CAPTURESTATE_PREVIEW )
		{
			// since after configure sensor, WB/EV/SCENE_MODE all will be invalid
			error = _OV5642InitSetting( pState, &pState->stV4L2.stShotParamSetting );
			if ( error != CAM_ERROR_NONE )
			{
				return error;
			}
		}
	}

	// if complete reset, need restore shooting parameters to sensor
	if ( pSensorConfig->iResetType == CAM_RESET_COMPLETE )
	{
		error = _OV5642InitSetting( pState, &pState->stV4L2.stShotParamSetting );
		if ( error != CAM_ERROR_NONE )
		{
			return error;
		}
	}

	// face detection related
	if ( pSensorConfig->eState == CAM_CAPTURESTATE_PREVIEW )
	{
		if ( pState->bIsFaceDetectionOpen )
		{
			pState->iFaceDetectFrameSkip = 0;
		}
	}

	pState->stV4L2.stConfig = *pSensorConfig;

	// XXX: since banding filter is highly related with stream, so we need update it after stream configure. It is better to do this is driver
	// _OV5642SetBandFilter( pState->stV4L2.iSensorFD );

	return CAM_ERROR_NONE;
}

CAM_Error OV5642_GetBufReq( void *pDevice, _CAM_SmartSensorConfig *pSensorConfig, CAM_ImageBufferReq *pBufReq )
{
	OV5642State *pState = (OV5642State*)pDevice;
	CAM_Error   error   = CAM_ERROR_NONE;

	error = V4L2_GetBufReq( &pState->stV4L2, pSensorConfig, pBufReq );

	return error;
}

CAM_Error OV5642_Enqueue( void *pDevice, CAM_ImageBuffer *pImgBuf )
{
	OV5642State *pState = (OV5642State*)pDevice;
	CAM_Error   error   = CAM_ERROR_NONE;

	error = V4L2_Enqueue( &pState->stV4L2, pImgBuf );

	return error;
}

CAM_Error OV5642_Dequeue( void *pDevice, CAM_ImageBuffer **ppImgBuf )
{
	OV5642State *pState     = (OV5642State*)pDevice;
	CAM_Error   error       = CAM_ERROR_NONE;
	CAM_Int32s  ret         = 0;
	CAM_Bool    bIsStreamOn = pState->stV4L2.bIsStreamOn;

	error = V4L2_Dequeue( &pState->stV4L2, ppImgBuf );

	if ( error == CAM_ERROR_NONE )
	{
#if defined( BUILD_OPTION_WORKAROUND_V4L2_RESET_FOCUS_AFTER_STREAMON )
		// XXX: mk2 driver use power down to do stream off, which make AF related register invalid in stream off stage.
		//      To workaround this issue, move all focus register operation after stream on.
		if ( !bIsStreamOn && pState->stV4L2.bIsStreamOn && pState->stV4L2.stConfig.eState == CAM_CAPTURESTATE_PREVIEW )
		{
			if ( !bIsDownloadFirmware )
			{
				CAM_Tick t = -CAM_TimeGetTickCount();
				error = V4L2_FirmwareDownload( &pState->stV4L2 );
				ASSERT_CAM_ERROR( error );

				t += CAM_TimeGetTickCount();
				CELOG( "Perf: AF firmware download cost: %.2f ms\n", t / 1000.0 );

				bIsDownloadFirmware = CAM_TRUE;
			}

			// reset sensor MCU to ensure AF work
			_set_sensor_reg( pState->stV4L2.iSensorFD, 0x3000, 5, 5, 5, 0x20 );
			CAM_Sleep( 5000 );
			_set_sensor_reg( pState->stV4L2.iSensorFD, 0x3000, 5, 5, 5, 0x00 );

			ret = _ov5642_set_paramsreg( pState->stV4L2.stShotParamSetting.eFocusMode, _OV5642FocusMode, _ARRAY_SIZE( _OV5642FocusMode ), pState->stV4L2.iSensorFD );
			if ( ret == -2 )
			{
				return CAM_ERROR_NOTSUPPORTEDARG;
			}
		}
#endif
		// face detection pre-processing
		if ( pState->bIsFaceDetectionOpen && pState->stV4L2.stConfig.eState == CAM_CAPTURESTATE_PREVIEW )
		{
			if ( pState->iFaceDetectFrameSkip == 0 )
			{
				if ( pState->hFaceBufWriteMutex )
				{
					ret = CAM_MutexLock( pState->hFaceBufWriteMutex, CAM_INFINITE_WAIT, NULL );
					ASSERT( ret == 0 );

					if ( pState->pFaceDetectInputBuf && pState->bIsFaceDetectionOpen )
					{
						CAM_Size stDstSize = { OV5642_FACEDETECTION_INBUFWIDTH, OV5642_FACEDETECTION_INBUFHEIGHT };
						CAM_Error error1   = CAM_ERROR_NONE;

						error1 = _YUVToGrayRsz_8u( *ppImgBuf, pState->pFaceDetectInputBuf, stDstSize );
						ASSERT_CAM_ERROR( error1 );

						// trigger _OV5642FaceDetectionThread
						if ( pState->stFaceDetectThread.stThreadAttribute.hEventWakeup )
						{
							ret = CAM_EventSet( pState->stFaceDetectThread.stThreadAttribute.hEventWakeup );
							ASSERT( ret == 0 );
						}
					}

					ret = CAM_MutexUnlock( pState->hFaceBufWriteMutex );
					ASSERT( ret == 0 );
				}
			}
			pState->iFaceDetectFrameSkip++;
			pState->iFaceDetectFrameSkip %= OV5642_FACEDETECTION_INTERVAL;
		}

		// fill frame info if need
		if ( (*ppImgBuf)->bEnableShotInfo )
		{
			error = _OV5642FillFrameShotInfo( pState, *ppImgBuf );
		}
	}

	return error;
}

CAM_Error OV5642_Flush( void *pDevice )
{
	OV5642State *pState = (OV5642State*)pDevice;
	CAM_Error   error   = CAM_ERROR_NONE;

	error = V4L2_Flush( &pState->stV4L2 );

	if ( error == CAM_ERROR_NONE )
	{
		// XXX: after flush, zoom registers will be flushed, we need follow up this issue with driver
		pState->stV4L2.stShotParamSetting.iDigZoomQ16 = ( 1 << 16 );
	}

	return error;
}

CAM_Error OV5642_SetShotParam( void *pDevice, _CAM_ShotParam *pShotParam )
{
	CAM_Int32s     ret                = 0;
	CAM_Error      error              = CAM_ERROR_NONE;
	OV5642State    *pState            = (OV5642State*)pDevice;
	_CAM_ShotParam stShotParamSetting = *pShotParam;

	// shot mode setting
	if ( stShotParamSetting.eShotMode != pState->stV4L2.stShotParamSetting.eShotMode )
	{
		CAM_Int32s i = 0;

		CELOG( "shot mode\n" );
		ret = _ov5642_set_paramsreg( stShotParamSetting.eShotMode, _OV564ShotMode, _ARRAY_SIZE( _OV564ShotMode ), pState->stV4L2.iSensorFD );
		if ( ret == -2 )
		{
			return CAM_ERROR_NOTSUPPORTEDARG;
		}

		for ( i = 0; i < 3; i++ )
		{
			if ( stShotParamSetting.eShotMode == _OV5642DefaultShotParams[i].eShotMode )
			{
				stShotParamSetting = _OV5642DefaultShotParams[i];
				break;
			}
		}

		pState->stV4L2.stShotParamSetting.eShotMode = stShotParamSetting.eShotMode;
	}

	/*
	// sensor orientation
	if ( stShotParamSetting.eSensorRotation != pState->stV4L2.stShotParamSetting.eSensorRotation )
	{
		ret = _ov5642_set_paramsreg( stShotParamSetting.eSensorRotation, _OV5642FlipRotation, _ARRAY_SIZE(_OV5642FlipRotation), pState->stV4L2.iSensorFD );
		if ( ret == -2 )
		{
			return CAM_ERROR_NOTSUPPORTEDARG;
		}

		pState->stV4L2.stShotParamSetting.eSensorRotation = stShotParamSetting.eSensorRotation;
	}
	*/

	// exposure mode
	if ( stShotParamSetting.eExpMode != pState->stV4L2.stShotParamSetting.eExpMode )
	{
		CELOG( "Exposure Mode\n" );

		ret = _ov5642_set_paramsreg( stShotParamSetting.eExpMode, _OV5642ExpMode, _ARRAY_SIZE(_OV5642ExpMode), pState->stV4L2.iSensorFD );
		if ( ret == -2 )
		{
			return CAM_ERROR_NOTSUPPORTEDARG;
		}

		// in exposure manual mode, save current expousre value for reference
		if ( stShotParamSetting.eExpMode == CAM_EXPOSUREMODE_MANUAL )
		{
			error = _ov5642_save_exposure( pState->stV4L2.iSensorFD, &pState->st3Astat );
			CHECK_CAM_ERROR( error );
		}
		pState->stV4L2.stShotParamSetting.eExpMode = stShotParamSetting.eExpMode;
	}


	// exposure metering mode
	if ( stShotParamSetting.eExpMeterMode != pState->stV4L2.stShotParamSetting.eExpMeterMode || stShotParamSetting.eExpMeterMode == CAM_EXPOSUREMETERMODE_MANUAL )
	{
		CAM_MultiROI *pOldExpMeterROI = &pState->stV4L2.stShotParamSetting.stExpMeterROI;
		CAM_MultiROI *pNewExpMeterROI = &stShotParamSetting.stExpMeterROI;
		// metering mode changing always means metering ROI changed
		CAM_Bool bIsExpMeterROIUpdate = CAM_TRUE;
		CAM_Int32s ret                = 0;

		if ( stShotParamSetting.eExpMeterMode == pState->stV4L2.stShotParamSetting.eExpMeterMode )
		{
			ret = _is_roi_update( pOldExpMeterROI, pNewExpMeterROI, &bIsExpMeterROIUpdate );
			ASSERT( ret == 0 );
		}

		if ( pState->stV4L2.stShotParamSetting.eExpMode == CAM_EXPOSUREMODE_MANUAL )
		{
			if ( bIsExpMeterROIUpdate )
			{
				TRACE( CAM_ERROR, "Error: changing exposure metering parameters is not allowed in exposure mode[manual]!( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
				return CAM_ERROR_NOTSUPPORTEDARG;
			}
		}
		else
		{
			if ( bIsExpMeterROIUpdate )
			{
				CELOG( "Exposure Metering mode\n" );
				error = _ov5642_update_metering_window( pState, stShotParamSetting.eExpMeterMode, &stShotParamSetting.stExpMeterROI );
				CHECK_CAM_ERROR( error );

				ret = _ov5642_set_paramsreg( stShotParamSetting.eExpMeterMode, _OV5642ExpMeterMode, _ARRAY_SIZE(_OV5642ExpMeterMode), pState->stV4L2.iSensorFD );
				if ( ret == -2 )
				{
					return CAM_ERROR_NOTSUPPORTEDARG;
				}

				pState->stV4L2.stShotParamSetting.eExpMeterMode = stShotParamSetting.eExpMeterMode;

				if ( stShotParamSetting.eExpMeterMode == CAM_EXPOSUREMETERMODE_MANUAL )
				{
					pState->stV4L2.stShotParamSetting.stExpMeterROI = stShotParamSetting.stExpMeterROI;
				}
			}
		}
	}

	// ISO setting
	if ( stShotParamSetting.eIsoMode != pState->stV4L2.stShotParamSetting.eIsoMode )
	{
		CELOG( "ISO\n" );
		ret = _ov5642_set_paramsreg( stShotParamSetting.eIsoMode, _OV5642IsoMode, _ARRAY_SIZE(_OV5642IsoMode), pState->stV4L2.iSensorFD );
		if ( ret == -2 )
		{
			return CAM_ERROR_NOTSUPPORTEDARG;
		}

		pState->stV4L2.stShotParamSetting.eIsoMode = stShotParamSetting.eIsoMode;
	}

	// banding filter mode
	if ( stShotParamSetting.eBandFilterMode != pState->stV4L2.stShotParamSetting.eBandFilterMode )
	{
		CELOG( "Banding Filter mode\n" );
		// ret = _ov5642_set_paramsreg( stShotParamSetting.eBandFilterMode, _OV5642BdFltMode, _ARRAY_SIZE(_OV5642BdFltMode), pState->stV4L2.iSensorFD );
		ret = 0;

		if ( ret == -2 )
		{
			return CAM_ERROR_NOTSUPPORTEDARG;
		}
		pState->stV4L2.stShotParamSetting.eBandFilterMode = stShotParamSetting.eBandFilterMode;
	}

	// flash mode
	if ( stShotParamSetting.eFlashMode != pState->stV4L2.stShotParamSetting.eFlashMode )
	{
		CELOG( "flash mode\n" );

		_set_sensor_reg( pState->stV4L2.iSensorFD, 0x3000, 3, 3, 3, 0x00 );
		_set_sensor_reg( pState->stV4L2.iSensorFD, 0x3004, 3, 3, 3, 0xff );
		_set_sensor_reg( pState->stV4L2.iSensorFD, 0x3016, 1, 1, 1, 0x02 );
		_set_sensor_reg( pState->stV4L2.iSensorFD, 0x3b07, 0, 1, 0, 0x0a );
		if ( stShotParamSetting.eFlashMode == CAM_FLASH_TORCH )
		{
			ret = _ov5642_set_paramsreg( CAM_FLASH_ON, _OV5642FlashMode, _ARRAY_SIZE( _OV5642FlashMode ), pState->stV4L2.iSensorFD );
			if ( ret == -2 )
			{
				return CAM_ERROR_NOTSUPPORTEDARG;
			}
			pState->stV4L2.bIsFlashOn = CAM_TRUE;
		}
		else if ( pState->stV4L2.stShotParamSetting.eFlashMode == CAM_FLASH_TORCH )
		{
			ret = _ov5642_set_paramsreg( CAM_FLASH_OFF, _OV5642FlashMode, _ARRAY_SIZE( _OV5642FlashMode ), pState->stV4L2.iSensorFD );
			if ( ret == -2 )
			{
				return CAM_ERROR_NOTSUPPORTEDARG;
			}
			pState->stV4L2.bIsFlashOn = CAM_FALSE;
		}
		pState->stV4L2.stShotParamSetting.eFlashMode = stShotParamSetting.eFlashMode;
	}

	// white balance setting
	if ( stShotParamSetting.eWBMode != pState->stV4L2.stShotParamSetting.eWBMode )
	{
		CELOG( "White Balance\n" );
		ret = _ov5642_set_paramsreg( stShotParamSetting.eWBMode, _OV5642WBMode, _ARRAY_SIZE(_OV5642WBMode), pState->stV4L2.iSensorFD );
		if ( ret == -2 )
		{
			return CAM_ERROR_NOTSUPPORTEDARG;
		}
		pState->stV4L2.stShotParamSetting.eWBMode = stShotParamSetting.eWBMode;
	}

	// focus mode setting
	if ( stShotParamSetting.eFocusMode != pState->stV4L2.stShotParamSetting.eFocusMode )
	{
		CELOG( "focus mode\n" );
		error = _OV5642SetFocusMode( pState, stShotParamSetting.eFocusMode );
		CHECK_CAM_ERROR( error );

		pState->stV4L2.stShotParamSetting.eFocusMode = stShotParamSetting.eFocusMode;
	}

	// focus zone setting
	if ( stShotParamSetting.eFocusZoneMode != pState->stV4L2.stShotParamSetting.eFocusZoneMode ||
	     stShotParamSetting.eFocusZoneMode == CAM_FOCUSZONEMODE_MANUAL )
	{
		CAM_MultiROI *pOldFocusROI  = &pState->stV4L2.stShotParamSetting.stFocusROI;
		CAM_MultiROI *pNewFocusROI  = &stShotParamSetting.stFocusROI;
		// focus zone mode changing always means focus ROI changed
		CAM_Bool bIsFocusZoneUpdate = CAM_TRUE;
		CAM_Int32s ret              = 0;

		if ( stShotParamSetting.eFocusZoneMode == pState->stV4L2.stShotParamSetting.eFocusZoneMode )
		{
			ret = _is_roi_update( pOldFocusROI, pNewFocusROI, &bIsFocusZoneUpdate );
			ASSERT( ret == 0 );
		}
		if ( bIsFocusZoneUpdate )
		{
			CELOG( "focus zone mode\n" );
#if defined( BUILD_OPTION_WORKAROUND_V4L2_RESET_FOCUS_AFTER_STREAMON )
			if ( pState->stV4L2.bIsStreamOn )
#endif
			{
				error = _OV5642UpdateFocusZone( pState, stShotParamSetting.eFocusZoneMode, &(stShotParamSetting.stFocusROI) );
				CHECK_CAM_ERROR( error );
			}

			pState->stV4L2.stShotParamSetting.eFocusZoneMode = stShotParamSetting.eFocusZoneMode;
			if ( stShotParamSetting.eFocusZoneMode != CAM_FOCUSZONEMODE_MANUAL )
			{
				memset( &(pState->stV4L2.stShotParamSetting.stFocusROI), 0, sizeof( CAM_MultiROI ) );
			}
			else
			{
				pState->stV4L2.stShotParamSetting.stFocusROI = stShotParamSetting.stFocusROI;
			}
		}
	}

	// color effect setting
	if ( stShotParamSetting.eColorEffect != pState->stV4L2.stShotParamSetting.eColorEffect )
	{
		CELOG( "Color Effect\n" );
		ret = _ov5642_set_paramsreg( stShotParamSetting.eColorEffect, _OV5642ColorEffectMode, _ARRAY_SIZE(_OV5642ColorEffectMode), pState->stV4L2.iSensorFD );

		if ( ret == -2 )
		{
			return CAM_ERROR_NOTSUPPORTEDARG;
		}
		pState->stV4L2.stShotParamSetting.eColorEffect = stShotParamSetting.eColorEffect;
	}

	// EV compensation setting
	if ( stShotParamSetting.iEvCompQ16 != pState->stV4L2.stShotParamSetting.iEvCompQ16 )
	{
		CELOG( "EV compensation\n" );
		if ( pState->stV4L2.stShotParamSetting.eExpMode != CAM_EXPOSUREMODE_MANUAL )
		{
			ret = _ov5642_set_paramsreg_nearest( &stShotParamSetting.iEvCompQ16, _OV5642EvCompMode, _ARRAY_SIZE(_OV5642EvCompMode), pState->stV4L2.iSensorFD );
			if ( ret == -2 )
			{
				return CAM_ERROR_NOTSUPPORTEDARG;
			}
		}
		else
		{
			pState->st3Astat.dExpVal = pState->st3Astat.dExpVal * pow( 2, (double)(stShotParamSetting.iEvCompQ16 - pState->stV4L2.stShotParamSetting.iEvCompQ16) / 19661.0 );

			error = _ov5642_set_exposure( pState->stV4L2.iSensorFD, pState->st3Astat.dExpVal );
			CHECK_CAM_ERROR( error );
		}

		pState->stV4L2.stShotParamSetting.iEvCompQ16 = stShotParamSetting.iEvCompQ16;
	}


	// NOTE: the followings are function style parameters setting

	// digital zoom
	if ( stShotParamSetting.iDigZoomQ16 != pState->stV4L2.stShotParamSetting.iDigZoomQ16 )
	{
		error = _OV5642SetDigitalZoom( pState, stShotParamSetting.iDigZoomQ16 );
		CHECK_CAM_ERROR( error );
	}

	// brightness setting
	if ( stShotParamSetting.iBrightness != pState->stV4L2.stShotParamSetting.iBrightness )
	{
		CELOG( "Brightness\n" );
		CAM_Int16u  brt = 0;
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_SDE_CTL, 0x80 );
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_CST_CTL, 0x04 );
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_SAT_CTL, 0x02 );

		if ( stShotParamSetting.iBrightness < 0 )
		{
			brt = (CAM_Int16u)( -stShotParamSetting.iBrightness );
			_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_BRIT_SIGN, 0x08 );
		}
		else
		{
			brt = (CAM_Int16u)( stShotParamSetting.iBrightness );
			_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_BRIT_SIGN, 0x00 );
		}
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_BRIT_VAL, brt ); 
		
		pState->stV4L2.stShotParamSetting.iBrightness = stShotParamSetting.iBrightness;
	}

	// contrast setting
	if ( stShotParamSetting.iContrast != pState->stV4L2.stShotParamSetting.iContrast )
	{
		CELOG( "Contrast\n" );
		CAM_Int16u  crt = 0;
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_SDE_CTL, 0x80 );
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_CST_CTL, 0x04 );
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_SAT_CTL, 0x02 );
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_CST_SIGNCTL, 0x00 );

		crt = (CAM_Int16u)(stShotParamSetting.iContrast);
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_CST_OFFSET, crt );
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_CST_VAL, crt );

		pState->stV4L2.stShotParamSetting.iContrast = stShotParamSetting.iContrast;
	}

	// saturation setting
	if ( stShotParamSetting.iSaturation != pState->stV4L2.stShotParamSetting.iSaturation )
	{
		CELOG( "Saturation\n" );
		CAM_Int16u  sat = 0;
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_SDE_CTL, 0x80 );
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_SAT_CTL, 0x02 );
		
		sat = (CAM_Int16u)( stShotParamSetting.iSaturation );
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_SAT_U, sat );
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_SAT_V, sat );

		pState->stV4L2.stShotParamSetting.iSaturation = stShotParamSetting.iSaturation;
	}

	// fps
	if ( stShotParamSetting.iFpsQ16 != pState->stV4L2.stShotParamSetting.iFpsQ16 )
	{
		CELOG( "Frame Rate\n" );

		error = _OV5642SetFrameRate( pState, stShotParamSetting.iFpsQ16 );
		CHECK_CAM_ERROR( error );
	}

	// still sub-mode
	if ( stShotParamSetting.eStillSubMode != pState->stV4L2.stShotParamSetting.eStillSubMode ||
	     stShotParamSetting.stStillSubModeParam.iBurstCnt != pState->stV4L2.stShotParamSetting.stStillSubModeParam.iBurstCnt ||
	     stShotParamSetting.stStillSubModeParam.iZslDepth != pState->stV4L2.stShotParamSetting.stStillSubModeParam.iZslDepth )
	{
		CELOG( "Still sub-mode\n" );
		// TODO: if your sensor/ISP originally support burst/zsl, set ISP sepecific params here

		pState->stV4L2.stShotParamSetting.eStillSubMode       = stShotParamSetting.eStillSubMode;
		pState->stV4L2.stShotParamSetting.stStillSubModeParam = stShotParamSetting.stStillSubModeParam;
	}

	// TODO: other settings: shutter speed, HDR, image stabilizer if supported

	return CAM_ERROR_NONE;
}

CAM_Error OV5642_GetShotParam( void *pDevice, _CAM_ShotParam *pShotParam )
{
	OV5642State *pState = (OV5642State*)pDevice;

	*pShotParam         = pState->stV4L2.stShotParamSetting;

	pShotParam->iFpsQ16 = pState->stV4L2.iActualFpsQ16;

	return CAM_ERROR_NONE;
}

CAM_Error OV5642_StartFocus( void *pDevice )
{
	OV5642State *pState = (OV5642State*)pDevice;
	CAM_Int32s  ret     = 0;
	CAM_Int32s  cnt     = 3000;
	CAM_Int16u  reg     = 0;
	CAM_Error   error   = CAM_ERROR_NONE;

	if ( pState->stV4L2.stShotParamSetting.eFocusMode == CAM_FOCUS_AUTO_ONESHOT )
	{
		error = _ov5642_pre_focus( pState );
		if ( error != CAM_ERROR_NONE )
		{
			return error;
		}

		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_AF_ACK,  0x01 );
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_AF_MAIN, 0x03 );
		// wait until cmd accepted
		ret = _ov5642_wait_afcmd_ready( pState->stV4L2.iSensorFD );
		if ( ret != 0 )
		{
			TRACE( CAM_ERROR, "Error: AF cmd send failure( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_DRIVEROPFAILED;
		}

		pState->stFocusState.bIsFocusStop  = CAM_FALSE;
		pState->stFocusState.bIsFocused    = CAM_FALSE;
	}
	else if ( pState->stV4L2.stShotParamSetting.eFocusMode == CAM_FOCUS_AUTO_CONTINUOUS_PICTURE )
	{
		if ( pState->stFocusState.bIsFocusStop == CAM_TRUE )
		{
			TRACE( CAM_ERROR, "Can not start focus in CAF mode, please call CancelFocus to resume CAF first( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_NOTSUPPORTEDCMD;
		}
		CELOG( "Pause CAF\n" );
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_AF_ACK,  0x01 );
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_AF_MAIN, 0x06 );
		// wait until cmd accepted
		ret = _ov5642_wait_afcmd_ready( pState->stV4L2.iSensorFD );
		if ( ret != 0 )
		{
			TRACE( CAM_ERROR, "Error: AF cmd send failure( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_DRIVEROPFAILED;
		}
	}
	else if ( pState->stV4L2.stShotParamSetting.eFocusMode == CAM_FOCUS_MACRO )
	{
		/*FALLTHROUGH*/;
	}
	else
	{
		TRACE( CAM_ERROR, "Error: focus mode[%d] doesn't has auto-focus trigger( %s, %s, %d )\n", pState->stV4L2.stShotParamSetting.eFocusMode, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_NOTSUPPORTEDARG;
	}

	pState->stFocusState.tFocusStart = CAM_TimeGetTickCount();

	return CAM_ERROR_NONE;
}

CAM_Error OV5642_CancelFocus( void *pDevice )
{
	OV5642State *pState = (OV5642State*)pDevice;
	CAM_Int32s  ret     = 0;
	CAM_Error   error   = CAM_ERROR_NONE;

	if ( pState->stV4L2.stShotParamSetting.eFocusMode == CAM_FOCUS_AUTO_CONTINUOUS_PICTURE )
	{
		CELOG( "Resume CAF\n" );
#if defined( BUILD_OPTION_WORKAROUND_V4L2_RESET_FOCUS_AFTER_STREAMON )
		if ( pState->stV4L2.bIsStreamOn )
#endif
		{
			error = _ov5642_pre_focus( pState );
			if ( error != CAM_ERROR_NONE )
			{
				return error;
			}

			ret = _ov5642_set_paramsreg( CAM_FOCUS_AUTO_CONTINUOUS_PICTURE, _OV5642FocusMode, _ARRAY_SIZE( _OV5642FocusMode ), pState->stV4L2.iSensorFD );
			if ( ret == -2 )
			{
				return CAM_ERROR_NOTSUPPORTEDARG;
			}
		}

		pState->stFocusState.bIsFocusStop = CAM_FALSE;
		pState->stFocusState.bIsFocused   = CAM_FALSE;
	}
	else
	{
		if ( pState->stFocusState.bIsFocusStop == CAM_FALSE )
		{
#if defined( BUILD_OPTION_WORKAROUND_V4L2_RESET_FOCUS_AFTER_STREAMON )
			if ( pState->stV4L2.bIsStreamOn )
#endif
			{
				// CELOG( "Set focus to idle\n" );
				_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_AF_ACK,  0x01 );
				_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_AF_MAIN, 0x08 );
				ret = _ov5642_wait_afcmd_completed( pState->stV4L2.iSensorFD );
				if ( ret != 0 )
				{
					TRACE( CAM_ERROR, "Error: AF cmd failure( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
					return CAM_ERROR_DRIVEROPFAILED;
				}
			}

			pState->stFocusState.bIsFocusStop = CAM_TRUE;
			pState->stFocusState.bIsFocused   = CAM_FALSE;
		}
	}

	return CAM_ERROR_NONE;
}

CAM_Error OV5642_CheckFocusState( void *pDevice, CAM_Bool *pFocusAutoStopped, CAM_FocusState *pFocusState )
{
	OV5642State *pState = (OV5642State*)pDevice;
	CAM_Error   error   = CAM_ERROR_NONE;
	CAM_Tick    t       = 0;
	CAM_Int16u  reg     = 0;
	CAM_Int32s  cnt     = 1;

	if ( pState->stV4L2.stShotParamSetting.eFocusMode == CAM_FOCUS_AUTO_ONESHOT ||
	     pState->stV4L2.stShotParamSetting.eFocusMode == CAM_FOCUS_AUTO_CONTINUOUS_PICTURE )
	{
		while ( cnt-- )
		{
			_get_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_AF_ACK, &reg );
			if ( reg == OV5642_FOCUS_CMD_COMPLETED )
			{
				pState->stFocusState.bIsFocusStop = CAM_TRUE;
				_get_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_AF_PARA4, &reg );
				if ( reg != 0x00 )
				{
					pState->stFocusState.bIsFocused   = CAM_TRUE;
					*pFocusState                      = CAM_FOCUSSTATE_INFOCUS;
					*pFocusAutoStopped                = pState->stFocusState.bIsFocusStop;
				}
				else
				{
					pState->stFocusState.bIsFocused   = CAM_FALSE;
					*pFocusState                      = CAM_FOCUSSTATE_OUTOFFOCUS;
					*pFocusAutoStopped                = pState->stFocusState.bIsFocusStop;
				}
				break;
			}
		}
		if ( cnt < 0 )
		{
			t = CAM_TimeGetTickCount();
			if ( ( t - pState->stFocusState.tFocusStart ) >= OV5642_AF_MAX_TIMEOUT &&
			     pState->stV4L2.stShotParamSetting.eFocusMode == CAM_FOCUS_AUTO_ONESHOT )
			{
				error = OV5642_CancelFocus( pDevice );
				CHECK_CAM_ERROR( error );

				*pFocusAutoStopped = CAM_TRUE;
				*pFocusState       = CAM_FOCUSSTATE_OUTOFFOCUS;
			}
			else
			{
				*pFocusAutoStopped = CAM_FALSE;
				*pFocusState       = CAM_FOCUSSTATE_OUTOFFOCUS;
			}
		}
	}
	else if ( pState->stV4L2.stShotParamSetting.eFocusMode == CAM_FOCUS_MACRO )
	{
		*pFocusState       = CAM_FOCUSSTATE_INFOCUS;
		*pFocusAutoStopped = CAM_TRUE;
	}
	else
	{
		TRACE( CAM_ERROR, "Error: check AF status should not be in focus mode[%d]( %s, %s, %d )\n", pState->stV4L2.stShotParamSetting.eFocusMode,  __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_NOTSUPPORTEDCMD;
	}
	return error;
}


CAM_Error OV5642_GetDigitalZoomCapability( CAM_Int32s iWidth, CAM_Int32s iHeight, CAM_Int32s *pSensorDigZoomQ16 )
{
	if ( iWidth <= 0 || iHeight <= 0 || iWidth > OV5642_SENSOR_MAX_WIDTH || iHeight > OV5642_SENSOR_MAX_HEIGHT )
	{
		return CAM_ERROR_BADARGUMENT;
	}

	if ( ( iWidth <= OV5642_2XZOOM_MAX_WIDTH ) && ( iHeight <= OV5642_2XZOOM_MAX_HEIGHT ) )
	{
		*pSensorDigZoomQ16 =  2 << 16;
	}
	else
	{
		*pSensorDigZoomQ16 =  1 << 16;
	}

	return CAM_ERROR_NONE;
}

CAM_Error OV5642_StartFaceDetection( void *pDevice )
{
	OV5642State *pState = (OV5642State*)pDevice;
	CAM_Int32s  ret     = 0;

	// init face detection lib
	ASSERT( !pState->bIsFaceDetectionOpen );
	ASSERT( !pState->pUserBuf );
	pState->pUserBuf = (CAM_Int8u*)malloc( OV5642_FACEDETECTION_PROCBUFLEN + OV5642_FACEDETECTION_INBUFLEN + 16 );
	if ( pState->pUserBuf == NULL )
	{
		TRACE( CAM_ERROR, "Error: no enough memory for face detection( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_OUTOFMEMORY;
	}

	pState->pFaceDetectInputBuf = (CAM_Int8u*)_ALIGN_TO( (CAM_Int32s)pState->pUserBuf, 8 );
	pState->pFaceDetectProcBuf  = pState->pFaceDetectInputBuf + OV5642_FACEDETECTION_INBUFLEN;

	// NOTE: refer to FaceTrack.h for API information
	ret = FaceTrackOpen( pState->pFaceDetectProcBuf, _OV5642FaceDetectCallback, 0x05 );
	ASSERT( ret == 0 );

	pState->iFaceDetectFrameSkip = 0;

	// create face-detection thread for better efficiency
	pState->stFaceDetectThread.stThreadAttribute.iDetachState  = PTHREAD_CREATE_DETACHED;
	pState->stFaceDetectThread.stThreadAttribute.iPolicy       = CAM_THREAD_POLICY_NORMAL;
	pState->stFaceDetectThread.stThreadAttribute.iPriority     = CAM_THREAD_PRIORITY_NORMAL;

	ret = CAM_EventCreate( &pState->stFaceDetectThread.stThreadAttribute.hEventWakeup );
	ASSERT( ret == 0 );

	ret = CAM_EventCreate( &pState->stFaceDetectThread.stThreadAttribute.hEventExitAck );
	ASSERT( ret == 0 );

	ret = CAM_ThreadCreate( &pState->stFaceDetectThread, _OV5642FaceDetectionThread, (void*)pState );
	ASSERT( ret == 0 );

	// mutex used for FaceDetection buffer
	ret = CAM_MutexCreate( &pState->hFaceBufWriteMutex );
	ASSERT( ret == 0 );

	pState->bIsFaceDetectionOpen = CAM_TRUE;

	CELOG( "Face detection init sucess\n" );

	return CAM_ERROR_NONE;
}

CAM_Error OV5642_CancelFaceDetection( void *pDevice )
{
	OV5642State *pState = (OV5642State*)pDevice;
	CAM_Int32s  ret     = 0;

	pState->bIsFaceDetectionOpen = CAM_FALSE;

	ret = CAM_MutexLock( pState->hFaceBufWriteMutex, CAM_INFINITE_WAIT, NULL );
	ASSERT( ret == 0 );

	FaceTrackClose();

	free( pState->pUserBuf );
	pState->pUserBuf             = NULL;
	pState->pFaceDetectProcBuf   = NULL;
	pState->pFaceDetectInputBuf  = NULL;

	ret = CAM_MutexUnlock( pState->hFaceBufWriteMutex );
	ASSERT( ret == 0 );

	// XXX: yield for possible other lock waiting to finish, damn Android
	CAM_Sleep( 0 );

	ret = CAM_MutexDestroy( &pState->hFaceBufWriteMutex );
	ASSERT( ret == 0 );

	// destroy face-detection thread
	ret = CAM_ThreadDestroy( &pState->stFaceDetectThread );
	ASSERT( ret == 0 );

	if ( pState->stFaceDetectThread.stThreadAttribute.hEventWakeup )
	{
		ret = CAM_EventDestroy( &pState->stFaceDetectThread.stThreadAttribute.hEventWakeup );
		ASSERT( ret == 0 );
	}

	if ( pState->stFaceDetectThread.stThreadAttribute.hEventExitAck )
	{
		ret = CAM_EventDestroy( &pState->stFaceDetectThread.stThreadAttribute.hEventExitAck );
		ASSERT( ret == 0 );
	}

	pState->iFaceDetectFrameSkip = 0;

	CELOG( "Face detection deInit sucess\n" );

	return CAM_ERROR_NONE;
}

_CAM_SmartSensorFunc func_ov5642 =
{
	OV5642_GetCapability,
	OV5642_GetShotModeCapability,

	OV5642_SelfDetect,
	OV5642_Init,
	OV5642_Deinit,
	OV5642_RegisterEventHandler,
	OV5642_Config,
	OV5642_GetBufReq,
	OV5642_Enqueue,
	OV5642_Dequeue,
	OV5642_Flush,
	OV5642_SetShotParam,
	OV5642_GetShotParam,

	OV5642_StartFocus,
	OV5642_CancelFocus,
	OV5642_CheckFocusState,
	OV5642_GetDigitalZoomCapability,
	OV5642_StartFaceDetection,
	OV5642_CancelFaceDetection,
};

static CAM_Error _OV5642SetBandFilter( CAM_Int32s iSensorFD )
{
	CELOG( "%s\n", __FUNCTION__ );
	double     sys_clk;
	CAM_Int16u reg;
	CAM_Int32s hts, vts;
	CAM_Int32s band_value, max_bands;


	_ov5642_calc_sysclk( iSensorFD, &sys_clk );
	_ov5642_calc_hvts( iSensorFD, &hts, &vts );

	// 60Hz banding filter settings
	band_value = sys_clk * 16 * 1000000 / 120 / hts;
	max_bands  = vts / ( band_value / 16 ) - 1;
	CELOG( "band_value: 0x%x, max_bands: 0x%x\n", band_value, max_bands );
	reg = band_value & 0xff ;
	_set_sensor_reg( iSensorFD, 0x3a0b, 0, 7, 0, reg );
	reg = ( band_value >> 8 ) & 0x3f;
	_set_sensor_reg( iSensorFD, 0x3a0a, 0, 5, 0, reg );
	reg = max_bands & 0x3f;
	_set_sensor_reg( iSensorFD, 0x3a0d, 0, 5, 0, reg );


	// 50Hz banding filter settings
	band_value = sys_clk * 16 * 1000000 / 100 / hts;
	max_bands  = vts / ( band_value / 16 ) - 1;
	CELOG( "band_value: 0x%x, max_bands: 0x%x\n", band_value, max_bands );
	reg = band_value & 0xff;
	_set_sensor_reg( iSensorFD, 0x3a09, 0, 7, 0, 0x4a );
	reg = ( band_value >> 8 ) & 0x3f;
	_set_sensor_reg( iSensorFD, 0x3a08, 0, 5, 0, 0x14 );
	reg = max_bands & 0x3f;
	_set_sensor_reg( iSensorFD, 0x3a0e, 0, 5, 0, 0x2 );

	return CAM_ERROR_NONE;
}

static CAM_Error _OV5642UpdateFocusZone( OV5642State *pState, CAM_FocusZoneMode eFocusZoneMode, CAM_MultiROI *pFocusROI )
{
	CAM_Int32s ret = 0;

	if ( eFocusZoneMode != CAM_FOCUSZONEMODE_MANUAL )
	{
		ret = _ov5642_set_paramsreg( eFocusZoneMode, _OV5642FocusZone, _ARRAY_SIZE( _OV5642FocusZone ), pState->stV4L2.iSensorFD );
		if ( ret == -2 )
		{
			return CAM_ERROR_NOTSUPPORTEDARG;
		}
	}
	else
	{
		CELOG( "Touch focus\n" );
		if ( pFocusROI->iROICnt == 1 && pFocusROI->stWeiRect[0].stRect.iWidth == 1 && pFocusROI->stWeiRect[0].stRect.iHeight == 1 )
		{
			CELOG( "update touch focus ROI\n" );
			CAM_Int16u xc = ( pFocusROI->stWeiRect[0].stRect.iLeft + CAM_NORMALIZED_FRAME_WIDTH / 2 ) * OV5642_AFFIRMWARE_MAX_WIDTH / CAM_NORMALIZED_FRAME_WIDTH;
			CAM_Int16u yc = ( pFocusROI->stWeiRect[0].stRect.iTop + CAM_NORMALIZED_FRAME_HEIGHT / 2 ) * OV5642_AFFIRMWARE_MAX_HEIGH / CAM_NORMALIZED_FRAME_HEIGHT;
			_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_AF_PARA0, xc );
			_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_AF_PARA1, yc );
			_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_AF_ACK,  0x01 );
			_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_AF_MAIN, 0x81 );
		}
		else
		{
			// TODO: add your ROI-based AF code here
			return CAM_ERROR_NOTSUPPORTEDARG;
		}
	}

	ret = _ov5642_wait_afcmd_completed( pState->stV4L2.iSensorFD );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: set focus zone mode failed( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_DRIVEROPFAILED;
	}

	return CAM_ERROR_NONE;
}

static CAM_Error _OV5642SetJpegParam( OV5642State *pState, CAM_JpegParam *pJpegParam )
{
	CAM_Int16u  usQuantStep = 0;
	CAM_Int32s  ret         = 0;

	if ( pJpegParam->iQualityFactor != pState->stV4L2.stConfig.stJpegParam.iQualityFactor )
	{
		if ( pJpegParam->iQualityFactor < 20 || pJpegParam->iQualityFactor > 95 )
		{
			return CAM_ERROR_NOTSUPPORTEDARG;
		}

		// compute quantization scale from quality factor
		if ( pJpegParam->iQualityFactor < 50 )
		{
			usQuantStep = 800 / pJpegParam->iQualityFactor;
		}
		else
		{
			usQuantStep = 32 - ((pJpegParam->iQualityFactor) << 4) / 50;
		}

		ret = _set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_JPEG_QSCTL, usQuantStep );

		if ( ret != 0 )
		{
			return CAM_ERROR_DRIVEROPFAILED;
		}
	}

	return CAM_ERROR_NONE;
}

static CAM_Error _OV5642InitSetting( OV5642State *pState, _CAM_ShotParam *pShotParamSetting )
{
	CAM_Int32s ret = 0;

	// shot mode
	ret = _ov5642_set_paramsreg( pShotParamSetting->eShotMode, _OV564ShotMode, _ARRAY_SIZE(_OV564ShotMode), pState->stV4L2.iSensorFD );

	// ISO
	ret = _ov5642_set_paramsreg( pShotParamSetting->eIsoMode, _OV5642IsoMode, _ARRAY_SIZE(_OV5642IsoMode), pState->stV4L2.iSensorFD );

	// color effect
	ret = _ov5642_set_paramsreg( pShotParamSetting->eColorEffect, _OV5642ColorEffectMode, _ARRAY_SIZE(_OV5642ColorEffectMode), pState->stV4L2.iSensorFD );

	// white balance
	ret = _ov5642_set_paramsreg( pShotParamSetting->eWBMode, _OV5642WBMode, _ARRAY_SIZE(_OV5642WBMode), pState->stV4L2.iSensorFD );

	// exposure metering mode
	ret = _ov5642_set_paramsreg( pShotParamSetting->eExpMeterMode, _OV5642ExpMeterMode, _ARRAY_SIZE(_OV5642ExpMeterMode), pState->stV4L2.iSensorFD );

	// banding filter mode
	// ret = _ov5642_set_paramsreg( pShotParamSetting->eBandFilterMode, _OV5642BdFltMode, _ARRAY_SIZE(_OV5642BdFltMode), pState->stV4L2.iSensorFD );

	// flash mode: off

	// EV compensation setting
	ret = _ov5642_set_paramsreg( pShotParamSetting->iEvCompQ16, _OV5642EvCompMode, _ARRAY_SIZE(_OV5642EvCompMode), pState->stV4L2.iSensorFD );

	// focus mode: off

	// digital zoom: 1 << 16

	// fps
	_OV5642SetFrameRate( pState, pShotParamSetting->iFpsQ16 );

	// brightness
	{
		CAM_Int16u  brt = 0;
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_SDE_CTL, 0x80 );
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_CST_CTL, 0x04 );
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_SAT_CTL, 0x02 );

		if ( pShotParamSetting->iBrightness <  0)
		{
			brt = (CAM_Int16u)( -pShotParamSetting->iBrightness );
			_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_BRIT_SIGN, 0x08 );
		}
		else
		{
			brt = (CAM_Int16u)( pShotParamSetting->iBrightness );
			_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_BRIT_SIGN, 0x00 );
		}
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_BRIT_VAL, brt ); 
	}

	// contrast
	{
		CAM_Int16u  crt = 0;
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_SDE_CTL, 0x80 );
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_CST_CTL, 0x04 );
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_SAT_CTL, 0x02 );
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_CST_SIGNCTL, 0x00 );
		
		crt = (CAM_Int16u)( pShotParamSetting->iContrast );
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_CST_OFFSET, crt );
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_CST_VAL, crt );
	}

	// saturation
	{
		CAM_Int16u  sat = 0;
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_SDE_CTL, 0x80 );
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_SAT_CTL, 0x02 );
		
		sat = (CAM_Int16u)( pShotParamSetting->iSaturation );
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_SAT_U, sat );
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_SAT_V, sat );
	}

	// burst count: 1

	return CAM_ERROR_NONE;
}

static CAM_Error _OV5642SaveAeAwb( void *pUserData )
{
	CAM_Error   error   = CAM_ERROR_NONE;
	OV5642State *pState = (OV5642State*)pUserData;

	TRACE( CAM_INFO, "Info: %s in\n", __FUNCTION__ );

	/* exposure */
	error = _ov5642_save_exposure( pState->stV4L2.iSensorFD, &pState->st3Astat );

	/* white balance
	 * OV declares that restore white balance is unnecessary
	 */
	// XXX: some driver implementation will use power down to do stream off, which will lose ISP settings, so save white balance here for insurance.
#if defined BUILD_OPTION_WORKAROUND_V4L2_SAVE_RESTORE_WB
	error = _ov5642_save_whitebalance( pState->stV4L2.iSensorFD, &pState->st3Astat );
#endif

	TRACE( CAM_INFO, "Info: %s out\n", __FUNCTION__ );
	return error;
}

static CAM_Error _OV5642RestoreAeAwb( void *pUserData )
{
	CAM_Error   error   = CAM_ERROR_NONE;
	OV5642State *pState = (OV5642State*)pUserData;

	TRACE( CAM_INFO, "Info: %s in\n", __FUNCTION__ );

	/* exposure */
	error = _ov5642_set_exposure( pState->stV4L2.iSensorFD, pState->st3Astat.dExpVal );

	/* white balance
	 * OV declares that restore white balance is unnecessary
	 */
	// XXX: some driver implementation will use power down to do stream off, which will lose ISP settings, so restore white balance here.
#if defined BUILD_OPTION_WORKAROUND_V4L2_SAVE_RESTORE_WB
	error = _ov5642_set_whitebalance( pState->stV4L2.iSensorFD, &pState->st3Astat );
#endif

	TRACE( CAM_INFO, "Info: %s out\n", __FUNCTION__ );
	return error;
}

static CAM_Error _OV5642StartFlash( void *pData )
{
	CAM_Int32s       ret         = 0;
	CAM_Bool         bOpenFlash  = CAM_FALSE;
	_V4L2SensorState *pV4l2State = (_V4L2SensorState*)pData; 

	if ( pV4l2State->stShotParamSetting.eFlashMode == CAM_FLASH_AUTO )
	{
		CAM_Int16u reg = 0;
		_get_sensor_reg( pV4l2State->iSensorFD, OV5642_REG_AVG_LUX, &reg );

		// TODO: just reference, you can modify it according to your requirement
		if ( reg < 85 && !pV4l2State->bIsFlashOn )
		{
			bOpenFlash = CAM_TRUE;
		}
	}
	else if ( pV4l2State->stShotParamSetting.eFlashMode == CAM_FLASH_ON ) 
	{
		if ( !pV4l2State->bIsFlashOn )
		{
			bOpenFlash = CAM_TRUE;
		}
	}
	else if ( pV4l2State->stShotParamSetting.eFlashMode == CAM_FLASH_REDEYE )
	{
		// flash twice to remove red eye
		ret = _ov5642_set_paramsreg( CAM_FLASH_ON, _OV5642FlashMode, _ARRAY_SIZE( _OV5642FlashMode ), pV4l2State->iSensorFD );
		if ( ret == -2 )
		{
			return CAM_ERROR_NOTSUPPORTEDARG;
		}
		CAM_Sleep( 30000 );

		ret = _ov5642_set_paramsreg( CAM_FLASH_OFF, _OV5642FlashMode, _ARRAY_SIZE( _OV5642FlashMode ), pV4l2State->iSensorFD );
		if ( ret == -2 )
		{
			return CAM_ERROR_NOTSUPPORTEDARG;
		}
		// TODO: may need sleep more to wait eye adapta flash light
		CAM_Sleep( 200000 );
		bOpenFlash = CAM_TRUE;
	}

	// start flash
	if ( bOpenFlash == CAM_TRUE )
	{
		ret = _ov5642_set_paramsreg( CAM_FLASH_ON, _OV5642FlashMode, _ARRAY_SIZE( _OV5642FlashMode ), pV4l2State->iSensorFD );
		if ( ret == -2 ) 
		{
			return CAM_ERROR_NOTSUPPORTEDARG;
		}
		pV4l2State->bIsFlashOn = CAM_TRUE;
	}

	return CAM_ERROR_NONE;
}

static CAM_Error _OV5642StopFlash( void *pData )
{
	CAM_Int32s       ret         = 0;
	_V4L2SensorState *pV4l2State = (_V4L2SensorState*)pData; 

	if ( pV4l2State->stShotParamSetting.eFlashMode != CAM_FLASH_TORCH )
	{
		if ( pV4l2State->bIsFlashOn )
		{
			ret = _ov5642_set_paramsreg( CAM_FLASH_OFF, _OV5642FlashMode, _ARRAY_SIZE( _OV5642FlashMode ), pV4l2State->iSensorFD );
			if ( ret == -2 ) 
			{
				return CAM_ERROR_NOTSUPPORTEDARG;
			}
		}
		pV4l2State->bIsFlashOn = CAM_FALSE;
	}

	return CAM_ERROR_NONE;
}

// set digital zoom
static CAM_Error _OV5642SetDigitalZoom( OV5642State *pState, CAM_Int32s iDigitalZoomQ16 )
{
	CAM_Rect         stRefROI, stNewROI;
	CAM_Int32s       iCenterX, iCenterY;
	CAM_Int16u       reg   = 0;
	CAM_Error        error = CAM_ERROR_NONE;

	_CHECK_BAD_POINTER( pState );

	stRefROI = pState->stRefROI;
	iCenterX = stRefROI.iLeft + stRefROI.iWidth  / 2;
	iCenterY = stRefROI.iTop  + stRefROI.iHeight / 2;

	/* calc new zoom settings */
	stNewROI.iWidth  = _ALIGN_TO( stRefROI.iWidth  * ( 1 << 16 ) / iDigitalZoomQ16, 4 );
	stNewROI.iHeight = _ALIGN_TO( stRefROI.iHeight * ( 1 << 16 ) / iDigitalZoomQ16, 4 );

	stNewROI.iLeft = _ALIGN_TO( iCenterX - stNewROI.iWidth / 2, 2 );
	stNewROI.iTop  = _ALIGN_TO( iCenterY - stNewROI.iHeight / 2, 2 );

#if defined( PLATFORM_PROCESSOR_MG1 )
	/* XXX: for MG familiy chip, SCI is sensitive and seems un-recoverable, so we need use S_CROP
	 *      ioctl to do digital zoom, and driver can hide a SCI reset operation in the calling
	 */
	{
		struct v4l2_crop stCrop;
		CAM_Int32s       ret    = 0;
		stCrop.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		stCrop.c.width  = stNewROI.iWidth;
		stCrop.c.height = stNewROI.iHeight;
		stCrop.c.top    = stNewROI.iTop;
		stCrop.c.left   = stNewROI.iLeft;
		ret = ioctl( pState->stV4L2.iSensorFD, VIDIOC_S_CROP, &stCrop );
		ASSERT( ret == 0 );
	}
#else
	// enable group 0
	// _set_sensor_reg( pState->stV4L2.iSensorFD, 0x3212, 0, 7, 0, 0x00 );

	/* set new digital zoom to sensor */
	reg = ( stNewROI.iLeft >> 8 ) & 0xff;
	_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_SENSORCROP_LEFT_HIGH, reg );
	reg = stNewROI.iLeft & 0xff;
	_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_SENSORCROP_LEFT_LOW, reg );

	reg = ( stNewROI.iTop >> 8 ) & 0xff;
	_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_SENSORCROP_TOP_HIGH, reg );
	reg = stNewROI.iTop & 0xff;
	_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_SENSORCROP_TOP_LOW, reg );

	reg = ( stNewROI.iWidth >> 8 ) & 0x0f;
	_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_DVPIN_WIDTH_HIGH, reg );
	reg = stNewROI.iWidth & 0xff;
	_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_DVPIN_WIDTH_LOW, reg );
	reg = ( stNewROI.iHeight >> 8 ) & 0x0f;
	_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_DVPIN_HEIGHT_HIGH, reg );
	reg = stNewROI.iHeight & 0xff;
	_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_DVPIN_HEIGHT_LOW, reg );

	if ( pState->stV4L2.stConfig.eState == CAM_CAPTURESTATE_PREVIEW &&
	     pState->stV4L2.stShotParamSetting.eFocusMode == CAM_FOCUS_AUTO_CONTINUOUS_PICTURE )
	{
#if defined( BUILD_OPTION_WORKAROUND_V4L2_RESET_FOCUS_AFTER_STREAMON )
		if ( pState->stV4L2.bIsStreamOn )
#endif
		{
			CAM_Int32s ret = 0;
			CELOG( "Re-launch focus zone\n" );
			_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_AF_ACK,  0x01 );
			_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_AF_MAIN, 0x12 );
			ret = _ov5642_wait_afcmd_completed( pState->stV4L2.iSensorFD );
			if ( ret != 0 )
			{
				TRACE( CAM_ERROR, "Error: AF cmd failure( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
				return CAM_ERROR_DRIVEROPFAILED;
			}
		}
	}

	// end group0
	// _set_sensor_reg( pState->stV4L2.iSensorFD, 0x3212, 0, 7, 0, 0x10 );

	// launch group0
	// _set_sensor_reg( pState->stV4L2.iSensorFD, 0x3212, 0, 7, 0, 0xa0 );

	// NOTE: skip 2 frames after digital zoom settings since sensor is unstable in the first 2 frames
	pState->stV4L2.iRemainSkipCnt = 2;
#endif

	CELOG( "digital zoom ROI: left:%d, top:%d, width:%d, height:%d, zoom:%.2f\n", stNewROI.iLeft, stNewROI.iTop,
	       stNewROI.iWidth, stNewROI.iHeight, iDigitalZoomQ16 / 65536.0 );

	/* update AEC windows */
	error = _ov5642_update_metering_window( pState, pState->stV4L2.stShotParamSetting.eExpMeterMode, NULL );
	CHECK_CAM_ERROR( error );

	/* update digital zoom */
	pState->stV4L2.stShotParamSetting.iDigZoomQ16 = iDigitalZoomQ16;

	return CAM_ERROR_NONE;
}

static CAM_Error _OV5642SetFrameRate( OV5642State *pState, CAM_Int32s iFpsQ16 )
{
	CAM_Error  error = CAM_ERROR_NONE;
#if 0
	CAM_Int32s iOldVts, iNewVts, iOldHts, iNewHts;
	CAM_Int16u reg;

	_CHECK_BAD_POINTER( pState );

	// get current VTS
	error = _ov5642_calc_hvts( pState->stV4L2.iSensorFD, &iOldHts, &iOldVts );
	ASSERT_CAM_ERROR( error );

	CELOG( "before frame rate: %.2f, target fps: %.2f \n", pState->stV4L2.iActualFpsQ16 / 65536.0, iFpsQ16 );

	// change frame rate by adjust VTS
	iNewVts = pState->stV4L2.iActualFpsQ16 * iOldVts / iFpsQ16;
	iNewVts &= 0xfff;

	reg = ( iNewVts >> 8 ) & 0x0f;
	_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_TIMINGVTSHIGH, reg );
	reg = iNewVts & 0xff;
	_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_TIMINGVTSLOW, reg );

	pState->stV4L2.iActualFpsQ16              = pState->stV4L2.iActualFpsQ16 * iOldVts / iNewVts;
	pState->stV4L2.stShotParamSetting.iFpsQ16 = iFpsQ16;

	CELOG( "after frame rate: %.2f\n", pState->stV4L2.iActualFpsQ16 / 65536.0 );
#else
	pState->stV4L2.iActualFpsQ16              = iFpsQ16;
	pState->stV4L2.stShotParamSetting.iFpsQ16 = iFpsQ16;
#endif

	return error;
}

// get shot info
static CAM_Error _OV5642FillFrameShotInfo( OV5642State *pState, CAM_ImageBuffer *pImgBuf )
{
	// TODO: will contact sensor/module vendor to get the real value of these parameters

	CAM_Error    error      = CAM_ERROR_NONE;
	CAM_ShotInfo *pShotInfo = &(pImgBuf->stShotInfo);

	pShotInfo->iExposureTimeQ16    = (1 << 16) / 30;
	pShotInfo->iFNumQ16            = (CAM_Int32s)( 2.8 * 65536 + 0.5 );
	pShotInfo->eExposureMode       = pState->stV4L2.stShotParamSetting.eExpMode;
	pShotInfo->eExpMeterMode       = CAM_EXPOSUREMETERMODE_MEAN;
	pShotInfo->iISOSpeed           = 100;
	pShotInfo->iSubjectDistQ16     = 0;
	pShotInfo->bIsFlashOn          = ( pState->stV4L2.stConfig.eState == CAM_CAPTURESTATE_STILL ) ? pState->stV4L2.bIsCaptureFlashOn : pState->stV4L2.bIsFlashOn;
	pShotInfo->iFocalLenQ16        = (CAM_Int32s)( 3.79 * 65536 + 0.5 );
	pShotInfo->iFocalLen35mm       = 0;

	return error;
}

static CAM_Error _OV5642SetFocusMode( OV5642State *pState, CAM_FocusMode eNewFocusMode )
{
	CAM_Error     error         = CAM_ERROR_NONE;
	CAM_FocusMode eOldFocusMode = pState->stV4L2.stShotParamSetting.eFocusMode;
	CAM_Int16u    reg;
	CAM_Int32s    cnt = 3000;
	CAM_Int32s    ret = 0;

	if ( eNewFocusMode == CAM_FOCUS_AUTO_CONTINUOUS_PICTURE )
	{
#if defined( BUILD_OPTION_WORKAROUND_V4L2_RESET_FOCUS_AFTER_STREAMON )
		if ( pState->stV4L2.bIsStreamOn )
#endif
		{
			error = _ov5642_pre_focus( pState );
			if ( error != CAM_ERROR_NONE )
			{
				return error;
			}
		}

		pState->stFocusState.bIsFocusStop = CAM_FALSE;
		pState->stFocusState.bIsFocused   = CAM_FALSE;
	}

	// XXX: why do not wait CAF pause to switch focus mode, since if there is not frame dequeue out, the CAF can not be paused.
	if (  eOldFocusMode == CAM_FOCUS_AUTO_CONTINUOUS_PICTURE && pState->stFocusState.bIsFocusStop == CAM_FALSE )
	{
#if defined( BUILD_OPTION_WORKAROUND_V4L2_RESET_FOCUS_AFTER_STREAMON )
		if ( pState->stV4L2.bIsStreamOn )
#endif
		{
			// Reset sensor MCU
			_set_sensor_reg( pState->stV4L2.iSensorFD, 0x3000, 5, 5, 5, 0x20 );
			CAM_Sleep( 5000 );
			_set_sensor_reg( pState->stV4L2.iSensorFD, 0x3000, 5, 5, 5, 0x00 );
		}

		pState->stFocusState.bIsFocusStop = CAM_TRUE;
	}

	ret = _ov5642_set_paramsreg( eNewFocusMode, _OV5642FocusMode, _ARRAY_SIZE( _OV5642FocusMode ), pState->stV4L2.iSensorFD );
	if ( ret == -2 )
	{
		return CAM_ERROR_NOTSUPPORTEDARG;
	}

	return CAM_ERROR_NONE;

}

static void _OV5642GetShotParamCap( CAM_ShotModeCapability *pShotModeCap )
{
	CAM_Int32s i = 0;

	// exposure mode
	pShotModeCap->iSupportedExpModeCnt  = _ARRAY_SIZE(_OV5642ExpMode);
	for ( i = 0; i < pShotModeCap->iSupportedExpModeCnt; i++ )
	{
		pShotModeCap->eSupportedExpMode[i]  = (CAM_ExposureMode)_OV5642ExpMode[i].iParameter;
	}

	// exposure metering mode
	pShotModeCap->iSupportedExpMeterCnt = _ARRAY_SIZE(_OV5642ExpMeterMode);
	for ( i = 0; i < pShotModeCap->iSupportedExpMeterCnt; i++ )
	{
		pShotModeCap->eSupportedExpMeter[i] = (CAM_ExposureMeterMode)_OV5642ExpMeterMode[i].iParameter;
	}

	// EV compensation
	pShotModeCap->iMinEVCompQ16  = -78644;
	pShotModeCap->iMaxEVCompQ16  = 78644;
	pShotModeCap->iEVCompStepQ16 = 19661;

	// ISO mode
	pShotModeCap->iSupportedIsoModeCnt = _ARRAY_SIZE(_OV5642IsoMode);
	for ( i = 0; i < pShotModeCap->iSupportedIsoModeCnt; i++ )
	{
		pShotModeCap->eSupportedIsoMode[i] = (CAM_ISOMode)_OV5642IsoMode[i].iParameter;
	}

	// shutter speed
	pShotModeCap->iMinShutSpdQ16 = -1;
	pShotModeCap->iMaxShutSpdQ16 = -1;

	// F-number
	pShotModeCap->iMinFNumQ16 = (CAM_Int32s)(2.8 * 65536 + 0.5);
	pShotModeCap->iMaxFNumQ16 = (CAM_Int32s)(2.8 * 65536 + 0.5);

	// band filter
	pShotModeCap->iSupportedBdFltModeCnt = _ARRAY_SIZE(_OV5642BdFltMode);
	for ( i = 0; i < pShotModeCap->iSupportedBdFltModeCnt; i++ )
	{
		pShotModeCap->eSupportedBdFltMode[i] = (CAM_BandFilterMode)_OV5642BdFltMode[i].iParameter;
	}

	// flash mode
	pShotModeCap->iSupportedFlashModeCnt = _ARRAY_SIZE(_OV5642FlashMode);
	for ( i = 0; i < pShotModeCap->iSupportedFlashModeCnt; i++ )
	{
		pShotModeCap->eSupportedFlashMode[i] = (CAM_FlashMode)_OV5642FlashMode[i].iParameter;
	}

	// white balance mode
	pShotModeCap->iSupportedWBModeCnt = _ARRAY_SIZE(_OV5642WBMode);
	for ( i = 0; i < pShotModeCap->iSupportedWBModeCnt; i++ )
	{
		pShotModeCap->eSupportedWBMode[i] = (CAM_ISOMode)_OV5642WBMode[i].iParameter;
	}

	// focus mode
	pShotModeCap->iSupportedFocusModeCnt = _ARRAY_SIZE( _OV5642FocusMode );
	for ( i = 0; i < pShotModeCap->iSupportedFocusModeCnt; i++ )
	{
		pShotModeCap->eSupportedFocusMode[i] = (CAM_FocusMode)_OV5642FocusMode[i].iParameter;
	}

	// focus zone mode
	pShotModeCap->iSupportedFocusZoneModeCnt = _ARRAY_SIZE(_OV5642FocusZone);
	for ( i = 0; i < pShotModeCap->iSupportedFocusZoneModeCnt; i++ )
	{
		pShotModeCap->eSupportedFocusZoneMode[i] = (CAM_FocusZoneMode)_OV5642FocusZone[i].iParameter;
	}

	// color effect
	pShotModeCap->iSupportedColorEffectCnt = _ARRAY_SIZE( _OV5642ColorEffectMode );
	for ( i = 0; i < pShotModeCap->iSupportedColorEffectCnt; i++ )
	{
		pShotModeCap->eSupportedColorEffect[i] = (CAM_ISOMode)_OV5642ColorEffectMode[i].iParameter;
	}

	// video sub-mode
	pShotModeCap->iSupportedVideoSubModeCnt = 1;
	pShotModeCap->eSupportedVideoSubMode[0] = CAM_VIDEOSUBMODE_SIMPLE;

	// still sub-mode
	pShotModeCap->iSupportedStillSubModeCnt = 2;
	pShotModeCap->eSupportedStillSubMode[0] = CAM_STILLSUBMODE_SIMPLE;
	pShotModeCap->eSupportedStillSubMode[1] = CAM_STILLSUBMODE_BURST;

	// optical zoom mode
	pShotModeCap->iMinOptZoomQ16 = (CAM_Int32s)(1.0 * 65536 + 0.5);
	pShotModeCap->iMaxOptZoomQ16 = (CAM_Int32s)(1.0 * 65536 + 0.5);

	// digital zoom mode
	pShotModeCap->iMinDigZoomQ16        = (CAM_Int32s)(1.0 * 65536 + 0.5);
	pShotModeCap->iMaxDigZoomQ16        = (CAM_Int32s)(2.0 * 65536 + 0.5);
	pShotModeCap->iDigZoomStepQ16       = (CAM_Int32s)(0.2 * 65536 + 0.5);
	pShotModeCap->bSupportSmoothDigZoom = CAM_FALSE;

	// fps
	pShotModeCap->iMinFpsQ16 = 1 << 16;
	pShotModeCap->iMaxFpsQ16 = 30 << 16;

	// brightness
	pShotModeCap->iMinBrightness = -255;
	pShotModeCap->iMaxBrightness = 255;

	// contrast
	pShotModeCap->iMinContrast = 0;
	pShotModeCap->iMaxContrast = 255;

	// saturation
	pShotModeCap->iMinSaturation = 0;
	pShotModeCap->iMaxSaturation = 255;

	// sharpness
	pShotModeCap->iMinSharpness = 0;
	pShotModeCap->iMaxSharpness = 0;

	// take 6 shots burst capture as example: you can detemine
	// the maximum burst number by referencing sensor's fps capability
	pShotModeCap->iMaxBurstCnt = 6;
	pShotModeCap->iMaxZslDepth = 0; // set 0 because sensor not support
	pShotModeCap->iMaxExpMeterROINum = OV5642_EXPMETER_MAX_ROI_NUM;
	pShotModeCap->iMaxFocusROINum    = OV5642_FOCUSZONE_MAX_ROI_NUM;

	return;
}

/*--------------------------------------------------------------------------------------------------------------------------------
 * OV5642 shotmode capability 
 *--------------------------------------------------------------------------------------------------------------------------------*/
static void _OV5642AutoModeCap( CAM_ShotModeCapability *pShotModeCap )
{
	CAM_Int32s i      = 0;

	// exposure mode
	pShotModeCap->iSupportedExpModeCnt  = _ARRAY_SIZE(_OV5642ExpMode);
	for ( i = 0; i < pShotModeCap->iSupportedExpModeCnt; i++ )
	{
		pShotModeCap->eSupportedExpMode[i]  = (CAM_ExposureMode)_OV5642ExpMode[i].iParameter;
	}

	// exposure metering mode
	pShotModeCap->iSupportedExpMeterCnt = _ARRAY_SIZE(_OV5642ExpMeterMode);
	for ( i = 0; i < pShotModeCap->iSupportedExpMeterCnt; i++ ) 
	{
		pShotModeCap->eSupportedExpMeter[i] = (CAM_ExposureMeterMode)_OV5642ExpMeterMode[i].iParameter;
	}

	// EV compensation
	pShotModeCap->iMinEVCompQ16  = -78644;
	pShotModeCap->iMaxEVCompQ16  = 78644;
	pShotModeCap->iEVCompStepQ16 = 19661;

	// ISO mode
	pShotModeCap->iSupportedIsoModeCnt = _ARRAY_SIZE(_OV5642IsoMode);
	for ( i = 0; i < pShotModeCap->iSupportedIsoModeCnt; i++ )
	{
		pShotModeCap->eSupportedIsoMode[i] = (CAM_ISOMode)_OV5642IsoMode[i].iParameter;
	}

	// shutter speed
	pShotModeCap->iMinShutSpdQ16 = -1;
	pShotModeCap->iMaxShutSpdQ16 = -1;

	// F-number
	pShotModeCap->iMinFNumQ16 = (CAM_Int32s)(2.8 * 65536 + 0.5);
	pShotModeCap->iMaxFNumQ16 = (CAM_Int32s)(2.8 * 65536 + 0.5);

	// band filter
	pShotModeCap->iSupportedBdFltModeCnt = _ARRAY_SIZE(_OV5642BdFltMode);
	for ( i = 0; i < pShotModeCap->iSupportedBdFltModeCnt; i++ )
	{
		pShotModeCap->eSupportedBdFltMode[i] = (CAM_BandFilterMode)_OV5642BdFltMode[i].iParameter;
	}

	// flash mode
	pShotModeCap->iSupportedFlashModeCnt = _ARRAY_SIZE(_OV5642FlashMode);
	for ( i = 0; i < pShotModeCap->iSupportedFlashModeCnt; i++ )
	{
		pShotModeCap->eSupportedFlashMode[i] = (CAM_FlashMode)_OV5642FlashMode[i].iParameter;
	}

	// white balance mode
	pShotModeCap->iSupportedWBModeCnt = _ARRAY_SIZE(_OV5642WBMode);
	for ( i = 0; i < pShotModeCap->iSupportedWBModeCnt; i++ )
	{
		pShotModeCap->eSupportedWBMode[i] = (CAM_ISOMode)_OV5642WBMode[i].iParameter;
	}

	// focus mode
	pShotModeCap->iSupportedFocusModeCnt = _ARRAY_SIZE(_OV5642FocusMode);
	for ( i = 0; i < pShotModeCap->iSupportedFocusModeCnt; i++ )
	{
		pShotModeCap->eSupportedFocusMode[i] = (CAM_FocusMode)_OV5642FocusMode[i].iParameter;
	}

	// focus zone mode
	pShotModeCap->iSupportedFocusZoneModeCnt = _ARRAY_SIZE(_OV5642FocusZone);
	for ( i = 0; i < pShotModeCap->iSupportedFocusZoneModeCnt; i++ )
	{
		pShotModeCap->eSupportedFocusZoneMode[i] = (CAM_FocusZoneMode)_OV5642FocusZone[i].iParameter;
	}

	// color effect
	pShotModeCap->iSupportedColorEffectCnt = _ARRAY_SIZE(_OV5642ColorEffectMode);
	for ( i = 0; i < pShotModeCap->iSupportedColorEffectCnt; i++ )
	{
		pShotModeCap->eSupportedColorEffect[i] = (CAM_ISOMode)_OV5642ColorEffectMode[i].iParameter;
	}

	// video sub-mode
	pShotModeCap->iSupportedVideoSubModeCnt = 1;
	pShotModeCap->eSupportedVideoSubMode[0] = CAM_VIDEOSUBMODE_SIMPLE;

	// still sub-mode
	pShotModeCap->iSupportedStillSubModeCnt = 2;
	pShotModeCap->eSupportedStillSubMode[0] = CAM_STILLSUBMODE_SIMPLE;
	pShotModeCap->eSupportedStillSubMode[1] = CAM_STILLSUBMODE_BURST;

	// optical zoom mode
	pShotModeCap->iMinOptZoomQ16 = (CAM_Int32s)(1.0 * 65536 + 0.5);
	pShotModeCap->iMaxOptZoomQ16 = (CAM_Int32s)(1.0 * 65536 + 0.5);

	// digital zoom mode
	pShotModeCap->iMinDigZoomQ16        = (CAM_Int32s)(1.0 * 65536 + 0.5);
	pShotModeCap->iMaxDigZoomQ16        = (CAM_Int32s)(2.0 * 65536 + 0.5);
	pShotModeCap->iDigZoomStepQ16       = (CAM_Int32s)(0.2 * 65536 + 0.5);
	pShotModeCap->bSupportSmoothDigZoom = CAM_FALSE;

	// fps
	pShotModeCap->iMinFpsQ16 = 1 << 16;
	pShotModeCap->iMaxFpsQ16 = 30 << 16;

	// brightness
	pShotModeCap->iMinBrightness = -255;
	pShotModeCap->iMaxBrightness = 255;

	// contrast
	pShotModeCap->iMinContrast = 0;
	pShotModeCap->iMaxContrast = 255;

	// saturation
	pShotModeCap->iMinSaturation = 0;
	pShotModeCap->iMaxSaturation = 255;

	// sharpness
	pShotModeCap->iMinSharpness = 0;
	pShotModeCap->iMaxSharpness = 0;

	// take 6 shots burst capture as example: you can detemine
	// the maximum burst number by referencing sensor's fps capability
	pShotModeCap->iMaxBurstCnt = 6;
	pShotModeCap->iMaxZslDepth = 0;
	pShotModeCap->iMaxExpMeterROINum = OV5642_EXPMETER_MAX_ROI_NUM;
	pShotModeCap->iMaxFocusROINum    = OV5642_FOCUSZONE_MAX_ROI_NUM;

	return;
}

static void _OV5642PortraitModeCap( CAM_ShotModeCapability *pShotModeCap )
{
	CAM_Int32s i = 0;

	// exposure mode
	pShotModeCap->iSupportedExpModeCnt  = 1;
	pShotModeCap->eSupportedExpMode[0]  = CAM_EXPOSUREMODE_AUTO;

	// exposure metering mode
	pShotModeCap->iSupportedExpMeterCnt = 1;
	pShotModeCap->eSupportedExpMeter[0] = CAM_EXPOSUREMETERMODE_AUTO;

	// EV compensation
	pShotModeCap->iEVCompStepQ16 = 0;
	pShotModeCap->iMinEVCompQ16  = 0;
	pShotModeCap->iMaxEVCompQ16  = 0;

	// ISO mode
	pShotModeCap->iSupportedIsoModeCnt = 1;
	pShotModeCap->eSupportedIsoMode[0] = CAM_ISO_AUTO;

	// shutter speed
	pShotModeCap->iMinShutSpdQ16 = -1;
	pShotModeCap->iMaxShutSpdQ16 = -1;

	// F-number
	pShotModeCap->iMinFNumQ16 = (CAM_Int32s)(2.8 * 65536 + 0.5);
	pShotModeCap->iMaxFNumQ16 = (CAM_Int32s)(2.8 * 65536 + 0.5);

	// band filter
	pShotModeCap->iSupportedBdFltModeCnt = 1; 
	pShotModeCap->eSupportedBdFltMode[0] = CAM_BANDFILTER_50HZ;

	// flash mode
	pShotModeCap->iSupportedFlashModeCnt = _ARRAY_SIZE(_OV5642FlashMode);
	for ( i = 0; i < pShotModeCap->iSupportedFlashModeCnt; i++ )
	{
		pShotModeCap->eSupportedFlashMode[i] = (CAM_FlashMode)_OV5642FlashMode[i].iParameter;
	}

	// white balance mode
	pShotModeCap->iSupportedWBModeCnt = 1;
	pShotModeCap->eSupportedWBMode[0] = CAM_WHITEBALANCEMODE_AUTO;

	// focus mode
	pShotModeCap->iSupportedFocusModeCnt = _ARRAY_SIZE(_OV5642FocusMode);
	for ( i = 0; i < pShotModeCap->iSupportedFocusModeCnt; i++ )
	{
		pShotModeCap->eSupportedFocusMode[i] = (CAM_FocusMode)_OV5642FocusMode[i].iParameter;
	}

	// focus zone mode
	pShotModeCap->iSupportedFocusZoneModeCnt = _ARRAY_SIZE(_OV5642FocusZone);
	for ( i = 0; i < pShotModeCap->iSupportedFocusZoneModeCnt; i++ )
	{
		pShotModeCap->eSupportedFocusZoneMode[i] = (CAM_FocusZoneMode)_OV5642FocusZone[i].iParameter;
	}

	// optical zoom mode
	pShotModeCap->iMinOptZoomQ16 = (CAM_Int32s)(1.0 * 65536 + 0.5);
	pShotModeCap->iMaxOptZoomQ16 = (CAM_Int32s)(1.0 * 65536 + 0.5);

	// digital zoom mode
	pShotModeCap->iMinDigZoomQ16        = (CAM_Int32s)(1.0 * 65536 + 0.5);
	pShotModeCap->iMaxDigZoomQ16        = (CAM_Int32s)(2.0 * 65536 + 0.5);
	pShotModeCap->iDigZoomStepQ16       = (CAM_Int32s)(0.2 * 65536 + 0.5);
	pShotModeCap->bSupportSmoothDigZoom = CAM_FALSE;

	// fps
	pShotModeCap->iMinFpsQ16 = (CAM_Int32s)(1.0 * 65536 + 0.5);
	pShotModeCap->iMaxFpsQ16 = (CAM_Int32s)(30.0 * 65536 + 0.5);

	// color effect
	pShotModeCap->iSupportedColorEffectCnt = 1;
	pShotModeCap->eSupportedColorEffect[0] = CAM_COLOREFFECT_OFF;

	// video sub-mode
	pShotModeCap->iSupportedVideoSubModeCnt = 1;
	pShotModeCap->eSupportedVideoSubMode[0] = CAM_VIDEOSUBMODE_SIMPLE;

	// still sub-mode
	pShotModeCap->iSupportedStillSubModeCnt = 1;
	pShotModeCap->eSupportedStillSubMode[0] = CAM_STILLSUBMODE_SIMPLE;

	// brightness
	pShotModeCap->iMinBrightness = 0;
	pShotModeCap->iMaxBrightness = 0;

	// contrast
	pShotModeCap->iMinContrast = 32;
	pShotModeCap->iMaxContrast = 32;

	// saturation
	pShotModeCap->iMinSaturation = 64;
	pShotModeCap->iMaxSaturation = 64;

	// sharpness
	pShotModeCap->iMinSharpness = 0;
	pShotModeCap->iMaxSharpness = 0;

	pShotModeCap->iMaxBurstCnt = 1;
	pShotModeCap->iMaxZslDepth = 0;
	pShotModeCap->iMaxExpMeterROINum = 0; // since CAM_EXPOSUREMETERMODE_MANUAL is not supported
	pShotModeCap->iMaxFocusROINum    = OV5642_FOCUSZONE_MAX_ROI_NUM;

	return;
}

static void _OV5642NightModeCap( CAM_ShotModeCapability *pShotModeCap )
{
	CAM_Int32s i      = 0;
	CAM_Int32s iIndex = 0;

	// exposure mode
	pShotModeCap->iSupportedExpModeCnt  = 1;
	pShotModeCap->eSupportedExpMode[0]  = CAM_EXPOSUREMODE_AUTO;

	// exposure metering mode
	pShotModeCap->iSupportedExpMeterCnt = 1;
	pShotModeCap->eSupportedExpMeter[0] = CAM_EXPOSUREMETERMODE_AUTO;

	// EV compensation
	pShotModeCap->iEVCompStepQ16 = 0;
	pShotModeCap->iMinEVCompQ16  = 0;
	pShotModeCap->iMaxEVCompQ16  = 0;

	// ISO mode
	pShotModeCap->iSupportedIsoModeCnt = _ARRAY_SIZE(_OV5642IsoMode);
	for ( i = 0; i < pShotModeCap->iSupportedIsoModeCnt; i++ )
	{
		pShotModeCap->eSupportedIsoMode[i] = (CAM_ISOMode)_OV5642IsoMode[i].iParameter;
	}

	// shutter speed
	pShotModeCap->iMinShutSpdQ16 = -1;
	pShotModeCap->iMaxShutSpdQ16 = -1;

	// F-number
	pShotModeCap->iMinFNumQ16 = (CAM_Int32s)(2.8 * 65536 + 0.5);
	pShotModeCap->iMaxFNumQ16 = (CAM_Int32s)(2.8 * 65536 + 0.5);

	// band filter
	pShotModeCap->iSupportedBdFltModeCnt = 1;
	pShotModeCap->eSupportedBdFltMode[0] = CAM_BANDFILTER_50HZ;

	// flash mode
	pShotModeCap->iSupportedFlashModeCnt = _ARRAY_SIZE(_OV5642FlashMode);
	for ( i = 0; i < pShotModeCap->iSupportedFlashModeCnt; i++ )
	{
		pShotModeCap->eSupportedFlashMode[i] = (CAM_FlashMode)_OV5642FlashMode[i].iParameter;
	}

	// white balance mode
	pShotModeCap->iSupportedWBModeCnt = 1;
	pShotModeCap->eSupportedWBMode[0] = CAM_WHITEBALANCEMODE_AUTO;

	// focus mode
	pShotModeCap->iSupportedFocusModeCnt = _ARRAY_SIZE(_OV5642FocusMode);
	for ( i = 0; i < pShotModeCap->iSupportedFocusModeCnt; i++ )
	{
		pShotModeCap->eSupportedFocusMode[i] = (CAM_FocusMode)_OV5642FocusMode[i].iParameter;
	}

	// focus zone mode
	pShotModeCap->iSupportedFocusZoneModeCnt = _ARRAY_SIZE(_OV5642FocusZone);
	for ( i = 0; i < pShotModeCap->iSupportedFocusZoneModeCnt; i++ )
	{
		pShotModeCap->eSupportedFocusZoneMode[i] = (CAM_FocusZoneMode)_OV5642FocusZone[i].iParameter;
	}

	// color effect
	pShotModeCap->iSupportedColorEffectCnt = 1;
	pShotModeCap->eSupportedColorEffect[0] = CAM_COLOREFFECT_OFF;

	// video sub-mode
	pShotModeCap->iSupportedVideoSubModeCnt = 1;
	pShotModeCap->eSupportedVideoSubMode[0] = CAM_VIDEOSUBMODE_SIMPLE;

	// still sub-mode
	pShotModeCap->iSupportedStillSubModeCnt = 2;
	pShotModeCap->eSupportedStillSubMode[0] = CAM_STILLSUBMODE_SIMPLE;
	pShotModeCap->eSupportedStillSubMode[1] = CAM_STILLSUBMODE_BURST;

	// optical zoom mode
	pShotModeCap->iMinOptZoomQ16 = (CAM_Int32s)(1.0 * 65536 + 0.5);
	pShotModeCap->iMaxOptZoomQ16 = (CAM_Int32s)(1.0 * 65536 + 0.5);

	// digital zoom mode
	pShotModeCap->iMinDigZoomQ16        = (CAM_Int32s)(1.0 * 65536 + 0.5);
	pShotModeCap->iMaxDigZoomQ16        = (CAM_Int32s)(2.0 * 65536 + 0.5);
	pShotModeCap->iDigZoomStepQ16       = (CAM_Int32s)(0.2 * 65536 + 0.5);
	pShotModeCap->bSupportSmoothDigZoom = CAM_FALSE;

	// fps
	pShotModeCap->iMinFpsQ16 = (CAM_Int32s)(1.0 * 65536 + 0.5);
	pShotModeCap->iMaxFpsQ16 = (CAM_Int32s)(30.0 * 65536 + 0.5);

	// brightness
	pShotModeCap->iMinBrightness = 0;
	pShotModeCap->iMaxBrightness = 0;

	// contrast
	pShotModeCap->iMinContrast = 32;
	pShotModeCap->iMaxContrast = 32;

	// saturation
	pShotModeCap->iMinSaturation = 64;
	pShotModeCap->iMaxSaturation = 64;

	// sharpness
	pShotModeCap->iMinSharpness = 0;
	pShotModeCap->iMaxSharpness = 0;

	// take 3 shots burst capture as example: you can detemine
	// the maximum burst number by referencing sensor's fps capability
	pShotModeCap->iMaxBurstCnt = 3;
	pShotModeCap->iMaxZslDepth = 0;
	pShotModeCap->iMaxExpMeterROINum = 0; // since CAM_EXPOSUREMETERMODE_MANUAL is not supported
	pShotModeCap->iMaxFocusROINum    = OV5642_FOCUSZONE_MAX_ROI_NUM;

	return;
}

/*--------------------------------------------------------------------------------------------------------------------------------
 * OV5642 utility functions part
 *-------------------------------------------------------------------------------------------------------------------------------*/

static CAM_Int32s _ov5642_wait_afcmd_ready( CAM_Int32s iSensorFD )
{
	CAM_Int16u reg = 0;
	CAM_Int32s cnt = 3000;
	while ( cnt-- )
	{
		_get_sensor_reg( iSensorFD, OV5642_REG_AF_MAIN, &reg );
		if ( reg == 0x00 )
		{
			// TRACE( CAM_INFO, "Info: AF cmd convergence( %s, %d )\n", __FILE__, __LINE__ );
			return 0;
		}
		CAM_Sleep( 1000 );
	}

	return -1;
}

static CAM_Int32s _ov5642_wait_afcmd_completed( CAM_Int32s iSensorFD )
{
	CAM_Int16u reg = 0;
	CAM_Int32s cnt = 3000;
	CAM_Tick    t  = 0;
#if defined( CAM_PERF )
	t = -CAM_TimeGetTickCount();
#endif
	while ( cnt-- )
	{
		_get_sensor_reg( iSensorFD, OV5642_REG_AF_ACK, &reg );
		if ( reg == 0x00 )
		{
			// TRACE( CAM_INFO, "Info: AF cmd is completed( %s, %d )\n", __FILE__, __LINE__ );
#if defined( CAM_PERF )
			t += CAM_TimeGetTickCount();
			CELOG( "Perf: Waitting AF cmd completed cost %.2f ms, cnt = %d \n", t / 1000.0, 3000 - cnt );
#endif
			return 0;
		}
		CAM_Sleep( 1000 );
	}

	return -1;
}

static CAM_Int32s _ov5642_set_paramsreg( CAM_Int32s param, _CAM_ParameterEntry *pOptionArray, CAM_Int32s iOptionSize, CAM_Int32s iSensorFD )
{
	CAM_Int32s i   = 0;
	CAM_Int32s ret = 0;

	for ( i = 0; i < iOptionSize; i++ )
	{
		if ( param == pOptionArray[i].iParameter )
		{
			break;
		}
	}
	
	if ( i >= iOptionSize )
	{
		return -2;
	}

	if ( pOptionArray[i].stSetParam.stRegTable.pEntries[0].reg == 0x0000 )
	{
		// NOTE: in this case, no sensor registers need to be set
		ret = 0;
	}
	else
	{
		ret = _set_reg_array( iSensorFD, pOptionArray[i].stSetParam.stRegTable.pEntries, pOptionArray[i].stSetParam.stRegTable.iLength );
	}

	return ret;
}


static CAM_Int32s _ov5642_set_paramsreg_nearest( CAM_Int32s *pParam, _CAM_ParameterEntry *pOptionArray, CAM_Int32s iOptionSize, CAM_Int32s iSensorFD )
{
	CAM_Int32s i = 0, index;
	CAM_Int32s ret = 0;
	CAM_Int32s diff, min_diff;

	index = 0;
	min_diff = _ABSDIFF( pOptionArray[0].iParameter, *pParam );

	for ( i = 0; i < iOptionSize; i++ )
	{
		diff = _ABSDIFF( pOptionArray[i].iParameter, *pParam );
		if ( diff < min_diff )
		{
			min_diff = diff;
			index = i;
		}
	}

	ret = _set_reg_array( iSensorFD, pOptionArray[index].stSetParam.stRegTable.pEntries, pOptionArray[index].stSetParam.stRegTable.iLength );

	*pParam = pOptionArray[index].iParameter;

	return ret;
}

/*
 *@ OV5642 clac system clock
 */
static inline CAM_Error _ov5642_calc_sysclk( CAM_Int32s iSensorFD, double *pSysClk )
{

	double pre_div_map[8]   = { 1, 1.5, 2, 2.5, 3, 4, 6, 8 };
	double div1to2p5_map[4] = { 1, 1, 2, 2.5 };
	int    m1_div_map[2]    = { 1, 2 };
	int    seld_map[4]      = { 1, 1, 4, 5 };
	int    divs_map[8]      = { 1, 2, 3, 4, 5, 6, 7, 8 };

	double     vco_freq;
	double     pre_div, div1to2p5;
	int        m1_div, divp, divs, seld;
	CAM_Int16u reg;

	// vco frequency
	reg = 0;
	_get_sensor_reg( iSensorFD, 0x3012, 0, 2, 0, &reg );
	pre_div = pre_div_map[reg];

	reg = 0;
	_get_sensor_reg( iSensorFD, 0x3011, 0, 5, 0, &reg );
	divp = reg;

	reg = 0;
	_get_sensor_reg( iSensorFD, 0x300f, 0, 1, 0, &reg );
	seld = seld_map[reg];

	vco_freq = PLATFORM_CCIC_SENSOR_MCLK / pre_div * divp * seld;
	// TRACE( CAM_INFO, "vco_freq: %.2f\n", vco_freq );
	CELOG( "vco_freq: %.2f\n", vco_freq );

	// system clk: the clk before ISP
	reg = 0;
	_get_sensor_reg( iSensorFD, 0x3010, 4, 7, 0, &reg );
	divs = divs_map[reg];

	reg = 0;
	_get_sensor_reg( iSensorFD, 0x300f, 0, 1, 0, &reg );
	div1to2p5 = div1to2p5_map[reg];

	reg = 0;
	_get_sensor_reg( iSensorFD, 0x3029, 0, 0, 0, &reg );
	m1_div = m1_div_map[reg];

	*pSysClk = vco_freq / divs / div1to2p5 / m1_div / 4;
	// TRACE( CAM_INFO, "sys_clk: %.2f\n", *pSysClk );
	CELOG( "sys_clk: %.2f\n", *pSysClk );

	return CAM_ERROR_NONE;
}

/*
*@ OV5642 hts & vts
*/
static inline CAM_Error _ov5642_calc_hvts( CAM_Int32s iSensorFD, CAM_Int32s *pHts, CAM_Int32s *pVts )
{
	CAM_Int16u reg;

	// vts
	reg = 0;
	_get_sensor_reg( iSensorFD, OV5642_REG_TIMINGVTSHIGH, &reg );
	*pVts = (reg & 0x000f);
	*pVts <<= 8;

	reg = 0;
	_get_sensor_reg( iSensorFD, OV5642_REG_TIMINGVTSLOW, &reg );
	*pVts += (reg & 0x00ff);
	CELOG( "vts: %d\n", *pVts );

	// hts
	reg = 0;
	_get_sensor_reg( iSensorFD, OV5642_REG_TIMINGHTSHIGH, &reg );
	*pHts = (reg & 0x000f);
	*pHts <<= 8;

	reg = 0;
	_get_sensor_reg( iSensorFD, OV5642_REG_TIMINGHTSLOW, &reg );
	*pHts += (reg & 0x00ff);
	CELOG( "hts: %d\n", *pHts );

	return CAM_ERROR_NONE;
}

/*
 *@ OV5642 frame rate calc, reference formula:
 *frame_rate = sys_clk * 1000000 / ( HTS * VTS );
 *sys_clk = vco_freq / divs / div1to2p5 / m1_div / 4;
 *vco_freq = ref_clk / pre_div * divp * seld5
*/
static CAM_Error _ov5642_calc_framerate( CAM_Int32s iSensorFD, CAM_Int32s *pFrameRate100 )
{
	int    hts, vts;
	double sys_clk;
	double frame_rate;

	// get sensor system clk
	_ov5642_calc_sysclk( iSensorFD, &sys_clk );

	// get sensor hts & vts
	_ov5642_calc_hvts( iSensorFD, &hts, &vts );

	frame_rate = sys_clk * 1000000 / ( hts * vts );
	// TRACE( CAM_INFO, "frame_rate: %.2f\n", frame_rate );
	CELOG( "frame_rate: %.2f\n", frame_rate );

	*pFrameRate100 = (CAM_Int32s)(frame_rate * 100);

	return CAM_ERROR_NONE;
}

static CAM_Error _ov5642_save_whitebalance( CAM_Int32s iSensorFD, _OV5642_3AState *pCurrent3A )
{
	CAM_Int16u reg      = 0;

	CAM_Int16u awbgainR = 0;
	CAM_Int16u awbgainG = 0;
	CAM_Int16u awbgainB = 0;

	// BAC check
	_CHECK_BAD_POINTER( pCurrent3A );


	// T0DO: check if AEC/AGC/AWB convergence
	if ( 0 )
	{
		;
	}

	/* white balance
	 * OV declares that restore white balance is unnecessary
	 */
	// workaround for WB, since driver use power down to do stream off,
	// which will destroy OV WB assuption.
	reg = 0;
	_get_sensor_reg( iSensorFD, OV5642_REG_CURRENT_AWBRHIGH, &reg );
	awbgainR = ( reg & 0x000f );
	awbgainR <<= 8;
	reg = 0;
	_get_sensor_reg( iSensorFD, OV5642_REG_CURRENT_AWBRLOW, &reg );
	awbgainR |= reg;
	pCurrent3A->iAWBGainR = awbgainR;

	reg = 0;
	_get_sensor_reg( iSensorFD, OV5642_REG_CURRENT_AWBGHIGH, &reg );
	awbgainG = ( reg & 0x000f );
	awbgainG <<= 8;
	reg = 0;
	_get_sensor_reg( iSensorFD, OV5642_REG_CURRENT_AWBGLOW, &reg );
	awbgainG |= reg;
	pCurrent3A->iAWBGainG = awbgainG;

	reg = 0;
	_get_sensor_reg( iSensorFD, OV5642_REG_CURRENT_AWBBHIGH, &reg );
	awbgainB = ( reg & 0x000f );
	awbgainB <<= 8;
	reg = 0;
	_get_sensor_reg( iSensorFD, OV5642_REG_CURRENT_AWBBLOW, &reg );
	awbgainB |= reg;
	pCurrent3A->iAWBGainB = awbgainB;

	return CAM_ERROR_NONE;
}

static CAM_Error _ov5642_set_whitebalance( CAM_Int32s iSensorFD, _OV5642_3AState *p3Astat )
{
	CAM_Int16u reg = 0;

	CAM_Int32s ret = 0;

	/* white balance
	 * OV declares that restore white balance is unnecessary
	 */
	// workaround for WB, since driver use power down to do stream off,
	// which will destroy OV WB assuption.
	reg = 0x0083;
	ret = _set_sensor_reg( iSensorFD, OV5642_REG_AWBC_22, reg );

	reg = (p3Astat->iAWBGainR & 0x0f00);
	reg = (reg >> 8);
	ret = _set_sensor_reg( iSensorFD, OV5642_REG_SET_AWBRHIGH, reg );
	reg = (p3Astat->iAWBGainR & 0x00ff);
	ret = _set_sensor_reg( iSensorFD, OV5642_REG_SET_AWBRLOW, reg );

	reg = (p3Astat->iAWBGainG & 0x0f00);
	reg = (reg >> 8);
	ret = _set_sensor_reg( iSensorFD, OV5642_REG_SET_AWBGHIGH, reg );
	reg = (p3Astat->iAWBGainG & 0x00ff);
	ret = _set_sensor_reg( iSensorFD, OV5642_REG_SET_AWBGLOW, reg );

	reg = (p3Astat->iAWBGainB & 0x0f00);
	reg = (reg >> 8);
	ret = _set_sensor_reg( iSensorFD, OV5642_REG_SET_AWBBHIGH, reg );
	reg = (p3Astat->iAWBGainB & 0x00ff);
	ret = _set_sensor_reg( iSensorFD, OV5642_REG_SET_AWBBLOW, reg );

	return CAM_ERROR_NONE;
}

// for face detection

// grayscale image generation
static CAM_Int32s _YCbCr422ToGrayRsz_8u_C2_C( const Ipp8u *pSrc, int srcStep, IppiSize srcSize,
                                              Ipp8u *pDst, int dstStep, IppiSize dstSize )
{
	int    x, y;
	Ipp32s ay, fx, fx1, ay1;
	int    dstWidth, dstHeight;
	Ipp32s xLimit, yLimit;
	Ipp8u  *pDstY; 
	Ipp8u  *pSrcY1, *pSrcY2;
	Ipp8u  *pDstY1, *pDstY2, *pDstY3, *pDstY4;
	int    stepX, stepY;
	Ipp32s yLT, yRT, yLB, yRB, yT, yB, yR;
	Ipp8u  Y1, Y2, Y3, Y4;
	int    col, dcol, drow;
	int    xNum, yNum;
	int    rcpRatiox, rcpRatioy;

	ASSERT( pSrc && pDst );

	dstWidth  = dstSize.width;
	dstHeight = dstSize.height;

	rcpRatiox = ( ( ( srcSize.width & ~1 ) - 1 ) << 16 ) / ( ( dstSize.width & ~1 ) - 1 );
	rcpRatioy = ( ( ( srcSize.height & ~1 ) - 1 ) << 16 ) / ( ( dstSize.height & ~1 ) - 1 );

	xLimit = (srcSize.width - 1) << 16;
	yLimit = (srcSize.height - 1) << 16;
	xNum   = dstSize.width;
	yNum   = dstSize.height;

	ay      = 0;
	ay1     = rcpRatioy;
	pSrcY1  = (Ipp8u*)(pSrc + 1);
	pSrcY2  = (Ipp8u*)(pSrcY1 + (ay1 >> 16) * srcStep);
	pDstY   = (Ipp8u*)pDst;

	// ippCameraRotateDisable
	pDstY1 = pDstY;
	pDstY2 = pDstY + 1;
	pDstY3 = pDstY + dstStep;
	pDstY4 = pDstY3 + 1;
	stepX  = 2;
	stepY  = 2 * dstStep - dstSize.width;

	for ( y = 0; y < yNum; y += 2 )
	{
		fx = 0;
		fx1 = rcpRatiox;
		for ( x = 0; x < xNum; x += 2 )
		{
			col = fx >> 16;
			col = col * 2;
			dcol = fx & 0xffff;
			drow = ay & 0xffff;
			yLT = pSrcY1[col];
			yRT = pSrcY1[col + 2];
			yLB = pSrcY1[col + srcStep];
			yRB = pSrcY1[col + srcStep + 2];

			yT = ((dcol * (yRT - yLT)) >> 16) + yLT;
			yB = ((dcol * (yRB - yLB)) >> 16) + yLB;
			yR = ((drow * (yB - yT)) >> 16) + yT;
			Y1 = (Ipp8u)yR;

			col = fx1 >> 16;
			col = col * 2;
			dcol = fx1 & 0xffff;
			drow = ay & 0xffff;
			yLT = pSrcY1[col];
			yLB = pSrcY1[col + srcStep];

			if ( fx1 >= xLimit )
			{
				yRT = pSrcY1[col];
				yRB = pSrcY1[col + srcStep];
			}
			else
			{
				yRT = pSrcY1[col + 2];
				yRB = pSrcY1[col + srcStep + 2];
			}

			yT = ((dcol * (yRT - yLT)) >> 16) + yLT;
			yB = ((dcol * (yRB - yLB)) >> 16) + yLB;
			yR = ((drow * (yB - yT)) >> 16) + yT;
			Y2 = (Ipp8u)yR;

			col = fx >> 16;
			col = col * 2;
			dcol = fx & 0xffff;
			drow = ay1 & 0xffff;
			yLT = pSrcY2[col];
			yRT = pSrcY2[col + 2];
			yLB = pSrcY2[col + srcStep];
			yRB = pSrcY2[col + srcStep + 2];
			yT = ((dcol * (yRT - yLT)) >> 16) + yLT;
			yB = ((dcol * (yRB - yLB)) >> 16) + yLB;
			yR = ((drow * (yB - yT)) >> 16) + yT;
			Y3 = (Ipp8u)yR;

			col = fx1 >> 16;
			col = col * 2;
			dcol = fx1 & 0xffff;
			drow = ay1 & 0xffff;
			yLT = pSrcY2[col];
			yLB = pSrcY2[col + srcStep];

			if ( fx1 >= xLimit )
			{
				yRT = pSrcY2[col];
				yRB = pSrcY2[col + srcStep];
			}
			else
			{
				yRT = pSrcY2[col + 2];
				yRB = pSrcY2[col + srcStep + 2];
			}

			yT = ((dcol * (yRT - yLT)) >> 16) + yLT;
			yB = ((dcol * (yRB - yLB)) >> 16) + yLB;
			yR = ((drow * (yB - yT)) >> 16) + yT;
			Y4 = (Ipp8u)yR;

			fx += rcpRatiox << 1;
			fx1 += rcpRatiox << 1;

			/* 2.store Y */
			*pDstY1 = Y1;
			pDstY1 += stepX;
			*pDstY2 = Y2;
			pDstY2 += stepX;
			*pDstY3 = Y3;
			pDstY3 += stepX;
			*pDstY4 = Y4;
			pDstY4 += stepX;
		}
		pDstY1 += stepY;
		pDstY2 += stepY;
		pDstY3 += stepY;
		pDstY4 += stepY;

		ay  += 2 * rcpRatioy;
		ay1 += 2 * rcpRatioy;
		if ( ay1 >= yLimit )
		{
			ay1 = yLimit - 1;
		}

		pSrcY1 = (Ipp8u*)(pSrc + 1 + (ay >> 16) * srcStep);
		pSrcY2 = (Ipp8u*)(pSrc + 1 + (ay1 >> 16) * srcStep);
	}

	return 0;
}

/*
// pImgBuf: src image, only CAM_IMGFMT_YCC420P( planar format ) and CAM_IMGFMT_CbYCrY( packet ) supported.
// pDstBuf: pointer of Y data. 8 byte align requirment.
*/
static CAM_Error _YUVToGrayRsz_8u( CAM_ImageBuffer *pImgBuf, CAM_Int8u *pDstBuf, CAM_Size stDstImgSize )
{
	IppiSize           stSrcSize, stDstSize;
	CAM_Int32s         ret = 0;

	if ( pImgBuf == NULL || pDstBuf == NULL || stDstImgSize.iWidth <= 0 || stDstImgSize.iHeight <= 0 )
	{
		return CAM_ERROR_BADARGUMENT;
	}

	stSrcSize.width     = pImgBuf->iWidth;
	stSrcSize.height    = pImgBuf->iHeight;
	stDstSize.width     = stDstImgSize.iWidth;
	stDstSize.height    = stDstImgSize.iHeight;

	if ( pImgBuf->eFormat == CAM_IMGFMT_YCbCr420P || pImgBuf->eFormat == CAM_IMGFMT_YCrCb420P )
	{
		CAM_Int8u  *pHeap   = NULL;
		CAM_Int8u  *pBuf[3] = {NULL};
		CAM_Int32s iStep[3] = {0};
		CAM_Int32s rcpRatioxQ16, rcpRatioyQ16;
		IppStatus  eIppRet  = ippStsNoErr;
		
		pHeap = (CAM_Int8u*)malloc( stDstSize.width * ( stDstSize.height >> 1 ) + 128 );
		if ( pHeap == NULL )
		{
			TRACE( CAM_ERROR, "Error: no enough memory for face detection( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_OUTOFMEMORY;
		}

		iStep[0] = stDstSize.width;
		iStep[1] = stDstSize.width >> 1;
		iStep[2] = stDstSize.width >> 1;
		pBuf[0]  = pDstBuf;
		pBuf[1]  = (CAM_Int8u*)_ALIGN_TO( pHeap, 8 );
		pBuf[2]  = (CAM_Int8u*)_ALIGN_TO( pBuf[1] + iStep[1] * ( stDstSize.height >> 1 ), 8 );

		rcpRatioxQ16 = ( ( ( stSrcSize.width & ~1 ) - 1 ) << 16 ) / ( ( stDstSize.width & ~1 ) - 1 );
		rcpRatioyQ16 = ( ( ( stSrcSize.height & ~1 ) - 1 ) << 16 ) / ( ( stDstSize.height & ~1 ) - 1 );

		if ( stSrcSize.width == stDstSize.width && stSrcSize.height == stDstSize.height )
		{
			// just copy Y here
			memmove( pBuf[0], pImgBuf->pBuffer[0], pImgBuf->iFilledLen[0] );
		}
		else
		{
			eIppRet = ippiYCbCr420RszRot_8u_P3R( (const Ipp8u**)pImgBuf->pBuffer, pImgBuf->iStep, stSrcSize,
			                                     pBuf, iStep, stDstSize,
			                                     ippCameraInterpBilinear, ippCameraRotateDisable, rcpRatioxQ16, rcpRatioyQ16 );
		}

		ASSERT( eIppRet == ippStsNoErr );
		free( pHeap );
	}
	else if ( pImgBuf->eFormat == CAM_IMGFMT_CbYCrY )
	{
		/*
		CELOG( "src buffer: %p, src buffer step: %d, src size: %d * %d, dst buffer: %p, dst step: %d, dst size: %d * %d\n",
		       pImgBuf->pBuffer[0], pImgBuf->iStep[0], stSrcSize.width, stSrcSize.height,
		       pDstBuf, stDstSize.width, stDstSize.width, stDstSize.height );
		*/
		ret = _YCbCr422ToGrayRsz_8u_C2_C( (const Ipp8u*)pImgBuf->pBuffer[0], pImgBuf->iStep[0], stSrcSize,
		                                   pDstBuf, stDstSize.width, stDstSize );
		ASSERT( ret == 0 );
	}
	else
	{
		TRACE( CAM_ERROR, "Error: unsupported format[%d]( %s, %s, %d )\n", pImgBuf->eFormat, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_NOTIMPLEMENTED;
	}

#if 0
	{
		FILE   *pSrcFile = NULL;
		FILE   *pDstFile = NULL;
		pSrcFile = fopen( "/data/preview.yuv", "w" );
		pDstFile = fopen( "/data/face.y", "w" );

		fwrite( pImgBuf->pBuffer[0], 1, 2 * stSrcSize.width * stSrcSize.height, pSrcFile );
		fwrite( pDstBuf, 1, stDstSize.width * stDstSize.height, pDstFile );
		fclose( pSrcFile );
		fclose( pDstFile );
	}
#endif
	return CAM_ERROR_NONE;
}

// face detection lib callback function
unsigned int _OV5642FaceDetectCallback( unsigned int value )
{
	CAM_Int16u  reg;
	CAM_Int8u   ucIn[4];
	CAM_Int32u  tmp  = 0;
	CAM_Int32u  ret  = 0;
	CAM_Int32s  cnt  = 1000;

	ucIn[0] = value & 0xff;
	ucIn[1] = ( value >> 8 ) & 0xff;
	ucIn[2] = ( value >> 16 ) & 0xff;
	ucIn[3] = ( value >> 24 ) & 0xff;

	_set_sensor_reg( giSensorFD, OV5642_REG_AF_ACK, 0x01 );
	reg = ucIn[3] & 0xff;
	_set_sensor_reg( giSensorFD, OV5642_REG_AF_PARA0, reg );
	reg = ucIn[2] & 0xff;
	_set_sensor_reg( giSensorFD, OV5642_REG_AF_PARA1, reg );
	reg = ucIn[1] & 0xff;
	_set_sensor_reg( giSensorFD, OV5642_REG_AF_PARA2, reg );
	reg = ucIn[0] & 0xff;
	_set_sensor_reg( giSensorFD, OV5642_REG_AF_PARA3, reg );

	_set_sensor_reg( giSensorFD, OV5642_REG_AF_MAIN, 0xec );

	reg = 1;
	while ( cnt-- )
	{
		_get_sensor_reg( giSensorFD,  OV5642_REG_AF_ACK, &reg );
		if ( reg == 0 )
		{
			break;
		}
		CAM_Sleep( 1000 );
	}

	reg = 0;
	_get_sensor_reg( giSensorFD,  OV5642_REG_AF_PARA0, &reg );
	tmp = reg;
	ret = (tmp << 24);

	reg = 0;
	_get_sensor_reg( giSensorFD,  OV5642_REG_AF_PARA1, &reg );
	tmp = reg;
	ret += (tmp << 16);

	reg = 0;
	_get_sensor_reg( giSensorFD,  OV5642_REG_AF_PARA2, &reg );
	tmp = reg;
	ret += (tmp << 8);

	reg = 0;
	_get_sensor_reg( giSensorFD,  OV5642_REG_AF_PARA3, &reg );
	tmp = reg;
	ret += tmp;

	return ret;
}

// face detection process thread
static CAM_Int32s _OV5642FaceDetectionThread( OV5642State *pState )
{
	CAM_Int32s   ret = 0, ret1 = 0;
	CAM_Int32s   i   = 0;
	CAM_MultiROI stFaceROI;

	for ( ; ; )
	{
		if ( pState->stFaceDetectThread.stThreadAttribute.hEventWakeup )
		{
			ret = CAM_EventWait( pState->stFaceDetectThread.stThreadAttribute.hEventWakeup, CAM_INFINITE_WAIT, NULL );
			ASSERT( ret == 0 );
		}

		if ( pState->stFaceDetectThread.bExitThread )
		{
			if ( pState->stFaceDetectThread.stThreadAttribute.hEventExitAck )
			{
				ret = CAM_EventSet( pState->stFaceDetectThread.stThreadAttribute.hEventExitAck );
				ASSERT( ret == 0 );
			}
			break;
		}

		ret = CAM_MutexLock( pState->hFaceBufWriteMutex, CAM_INFINITE_WAIT, NULL );
		ASSERT( ret == 0 );

		ret = TrackInFrame( pState->pFaceDetectInputBuf, OV5642_FACEDETECTION_INBUFWIDTH, OV5642_FACEDETECTION_INBUFHEIGHT );
		if ( ret == StatusErrCallback )
		{
			TRACE( CAM_ERROR, "Error: face detection callBack Function failed, please check whether implement callback funtion correctly "
			       "or download related firmware( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );

			ret1 = CAM_MutexUnlock( pState->hFaceBufWriteMutex );
			ASSERT( ret1 == 0 );

			continue;
		}

		ret = CAM_MutexUnlock( pState->hFaceBufWriteMutex );
		ASSERT( ret == 0 );

		ASSERT( ret == StatusSuccess || ret == StatusNoFace );

		// send call back no matter with facing or not
		stFaceROI.iROICnt = iFaceNum;
		for ( i = 0; i < stFaceROI.iROICnt; i++ )
		{
			stFaceROI.stWeiRect[i].iWeight        = 1;
			stFaceROI.stWeiRect[i].stRect.iLeft   = iFaceArea[i].iTl.iX * CAM_NORMALIZED_FRAME_WIDTH / OV5642_FACEDETECTION_INBUFWIDTH - CAM_NORMALIZED_FRAME_WIDTH / 2;
			stFaceROI.stWeiRect[i].stRect.iTop    = iFaceArea[i].iTl.iY * CAM_NORMALIZED_FRAME_HEIGHT / OV5642_FACEDETECTION_INBUFHEIGHT  - CAM_NORMALIZED_FRAME_HEIGHT / 2;
			stFaceROI.stWeiRect[i].stRect.iWidth  = ( iFaceArea[i].iBr.iX - iFaceArea[i].iTl.iX + 1 ) * CAM_NORMALIZED_FRAME_WIDTH / OV5642_FACEDETECTION_INBUFWIDTH;
			stFaceROI.stWeiRect[i].stRect.iHeight = ( iFaceArea[i].iBr.iY - iFaceArea[i].iTl.iY + 1 ) * CAM_NORMALIZED_FRAME_HEIGHT / OV5642_FACEDETECTION_INBUFHEIGHT;
		}

		// send update face event
		if ( pState->stV4L2.fnEventHandler != NULL )
		{
			pState->stV4L2.fnEventHandler( CAM_EVENT_FACE_UPDATE, &stFaceROI, pState->stV4L2.pUserData );
		}

		// TODO: trigger face-region AF/exposure metering here
	}

	return 0;
}

static CAM_Error _ov5642_update_ref_roi( OV5642State *pState )
{
	CAM_Rect         stRefROI;
	CAM_Error        error   = CAM_ERROR_NONE;

	_CHECK_BAD_POINTER( pState );

#if defined( PLATFORM_PROCESSOR_MG1 )
	{
		struct v4l2_crop stCrop;
		CAM_Int32s       ret = 0;
		stCrop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		ret = ioctl( pState->stV4L2.iSensorFD, VIDIOC_G_CROP, &stCrop );
		ASSERT( ret == 0 );

		stRefROI.iTop    = stCrop.c.top;
		stRefROI.iLeft   = stCrop.c.left;
		stRefROI.iHeight = stCrop.c.height;
		stRefROI.iWidth  = stCrop.c.width;
	}
#else
	{
		CAM_Int16u reg;
		// width
		reg = 0;
		_get_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_DVPIN_WIDTH_HIGH, &reg );
		stRefROI.iWidth = reg;
		reg = 0;
		_get_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_DVPIN_WIDTH_LOW, &reg );
		stRefROI.iWidth = ( stRefROI.iWidth << 8 ) | reg;

		// height
		reg = 0;
		_get_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_DVPIN_HEIGHT_HIGH, &reg );
		stRefROI.iHeight = reg;
		reg = 0;
		_get_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_DVPIN_HEIGHT_LOW, &reg );
		stRefROI.iHeight = ( stRefROI.iHeight << 8 ) | reg;

		// left corner
		reg = 0;
		_get_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_SENSORCROP_LEFT_HIGH, &reg );
		stRefROI.iLeft = reg;
		reg = 0;
		_get_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_SENSORCROP_LEFT_LOW, &reg );
		stRefROI.iLeft = ( stRefROI.iLeft << 8 ) | reg;

		// top corner
		reg = 0;
		_get_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_SENSORCROP_TOP_HIGH, &reg );
		stRefROI.iTop = reg;
		reg = 0;
		_get_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_SENSORCROP_TOP_LOW, &reg );
		stRefROI.iTop = ( stRefROI.iTop << 8 ) | reg;
	}
#endif

	pState->stRefROI.iWidth  = stRefROI.iWidth;
	pState->stRefROI.iHeight = stRefROI.iHeight;
	pState->stRefROI.iLeft   = stRefROI.iLeft;
	pState->stRefROI.iTop    = stRefROI.iTop;

	CELOG( "digital zoom reference ROI: left:%d, top:%d, width:%d, height:%d\n", pState->stRefROI.iLeft, pState->stRefROI.iTop,
	       pState->stRefROI.iWidth, pState->stRefROI.iHeight );

	return error;
}

static CAM_Error  _ov5642_update_metering_window( OV5642State *pState, CAM_ExposureMeterMode eExpMeterMode, CAM_MultiROI *pNewMultiROI )
{
	CAM_MultiROI *pCurrentMultiROI = NULL;
	CAM_Size     stSensorArraySize = {0, 0};
	CAM_Error    error             = CAM_ERROR_NONE;
	CAM_Bool     bUpdateWindow     = CAM_TRUE;
	CAM_Int32s   iXStart, iXEnd, iYStart, iYEnd, iXCenter, iYCenter, iWinWidth, iWinHeight;
	CAM_Int16u   reg;

	_CHECK_BAD_POINTER( pState );

	pCurrentMultiROI = &pState->stV4L2.stShotParamSetting.stExpMeterROI;

	// get current valid sensor array size
	reg = 0;
	_get_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_DVPIN_WIDTH_HIGH, &reg );
	stSensorArraySize.iWidth = reg;
	reg = 0;
	_get_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_DVPIN_WIDTH_LOW, &reg );
	stSensorArraySize.iWidth = ( stSensorArraySize.iWidth << 8 ) | reg;
	reg = 0;
	_get_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_DVPIN_HEIGHT_HIGH, &reg );
	stSensorArraySize.iHeight = reg;
	reg = 0;
	_get_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_DVPIN_HEIGHT_LOW, &reg );
	stSensorArraySize.iHeight = ( stSensorArraySize.iHeight << 8 ) | reg;

	switch ( eExpMeterMode )
	{
	case CAM_EXPOSUREMETERMODE_AUTO:
	case CAM_EXPOSUREMETERMODE_MEAN:
	case CAM_EXPOSUREMETERMODE_CENTRALWEIGHTED:
		{
			iXStart    = 0;
			iYStart    = 0;
			iXEnd      = stSensorArraySize.iWidth;
			iYEnd      = stSensorArraySize.iHeight;
		}
		break;

	case CAM_EXPOSUREMETERMODE_SPOT:
		{
			iXCenter = stSensorArraySize.iWidth / 2;
			iYCenter = stSensorArraySize.iHeight / 2;
			// Fixed metering sopt size with the length ratio of 3/16.
			iWinWidth   = stSensorArraySize.iWidth * 3 / 16;
			iWinHeight  = stSensorArraySize.iHeight * 3 / 16;
			iXStart  = iXCenter - iWinWidth / 2;
			iXEnd    = iXCenter + iWinWidth / 2;
			iYStart  = iYCenter - iWinHeight / 2;
			iYEnd    = iYCenter + iWinHeight / 2;
		}
		break;

	case CAM_EXPOSUREMETERMODE_MANUAL:
		{
			pCurrentMultiROI = ( pNewMultiROI == NULL ) ? pCurrentMultiROI : pNewMultiROI;
			iWinWidth  = pCurrentMultiROI->stWeiRect[0].stRect.iWidth * stSensorArraySize.iWidth / CAM_NORMALIZED_FRAME_WIDTH;
			iWinHeight = pCurrentMultiROI->stWeiRect[0].stRect.iHeight * stSensorArraySize.iHeight / CAM_NORMALIZED_FRAME_HEIGHT;
			iXStart    = ( pCurrentMultiROI->stWeiRect[0].stRect.iLeft + CAM_NORMALIZED_FRAME_WIDTH / 2 ) * stSensorArraySize.iWidth / CAM_NORMALIZED_FRAME_WIDTH;
			iYStart    = ( pCurrentMultiROI->stWeiRect[0].stRect.iTop + CAM_NORMALIZED_FRAME_HEIGHT / 2 ) * stSensorArraySize.iHeight / CAM_NORMALIZED_FRAME_HEIGHT;
			iXEnd      = iXStart + iWinWidth;
			iYEnd      = iYStart + iWinHeight;
		}
		break;

	case CAM_EXPOSUREMETERMODE_MATRIX:
		bUpdateWindow = CAM_FALSE;
		break;

	default:
		TRACE( CAM_ERROR, "Error: unknown ExpMeterMode[%d]( %s, %s, %d )\n", eExpMeterMode, __FILE__, __FUNCTION__,  __LINE__ );
		bUpdateWindow = CAM_FALSE;
		error         = CAM_ERROR_BADARGUMENT;
		break;
	}

	if ( bUpdateWindow )
	{
		reg = ( iXStart >> 8 ) & 0x0f;
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_WIN_XSTART_HIGH, reg );
		reg = iXStart & 0xff;
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_WIN_XSTART_LOW,  reg );
		reg = ( iYStart >> 8 ) & 0x07;
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_WIN_YSTART_HIGH, reg );
		reg = iYStart & 0xff;
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_WIN_YSTART_LOW,  reg );
		reg = ( iXEnd >> 8 ) & 0x0f;
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_WIN_XEND_HIGH,   reg );
		reg = iXEnd & 0xff;
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_WIN_XEND_LOW,    reg );
		reg = ( iYEnd >> 8 ) & 0x07;
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_WIN_YEND_HIGH,   reg );
		reg = iYEnd & 0xff;
		_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_WIN_YEND_LOW,    reg );
	}

	return error;
}

static CAM_Error _ov5642_save_exposure( CAM_Int32s iSensorFD, _OV5642_3AState *pCurrent3A )
{
	CAM_Int16u  reg      = 0;

	CAM_Int32u exposure  = 0;
	CAM_Int32u gain      = 0;
	CAM_Int32u maxLines  = 0;
	CAM_Int32s fps100    = 0;
	CAM_Int32s binFactor = 0;

	// BAC check
	_CHECK_BAD_POINTER( pCurrent3A );

	// T0DO: check if AEC/AGC/AWB convergence
	if ( 0 )
	{
		;
	}

	/* frame rate */
	_ov5642_calc_framerate( iSensorFD, &fps100 );

	/* gain */
	reg = 0;
	_get_sensor_reg( iSensorFD, OV5642_REG_GAINLOW, &reg );
	gain = (reg & 0x000f) + 16;
	if ( reg & 0x0010 )
	{
		gain <<= 1;
	}
	if ( reg & 0x0020 )
	{
		gain <<= 1;
	}
	if ( reg & 0x0040 )
	{
		gain <<= 1;
	}
	if ( reg & 0x0080 )
	{
		gain <<= 1;
	}

	/* exposure */
	reg = 0;
	_get_sensor_reg( iSensorFD, OV5642_REG_EXPOSUREHIGH, &reg );
	exposure = (reg & 0x000f);
	exposure <<= 12;

	reg = 0;
	_get_sensor_reg( iSensorFD, OV5642_REG_EXPOSUREMID, &reg );
	reg = (reg & 0x00ff);
	exposure += (reg << 4);

	reg = 0;
	_get_sensor_reg( iSensorFD, OV5642_REG_EXPOSURELOW, &reg );
	reg = (reg & 0x00f0);
	exposure += (reg >> 4);

	/* exposure VTS */
	reg = 0;
	_get_sensor_reg( iSensorFD, OV5642_REG_TIMINGVTSHIGH, &reg );
	maxLines = (reg & 0x00ff);
	maxLines <<= 8;

	reg = 0;
	_get_sensor_reg( iSensorFD, OV5642_REG_TIMINGVTSLOW, &reg );
	maxLines += ( reg & 0x00ff);

	/* binning factor */
	reg = 0;
	_get_sensor_reg( iSensorFD, OV5642_REG_BIN_HORIZONTAL, &reg );
	binFactor = ( 1 << reg );

	reg = 0;
	_get_sensor_reg( iSensorFD, OV5642_REG_BIN_VERTICAL, &reg );
	binFactor <<= reg;
	TRACE( CAM_INFO, "bin factor: %d\n", binFactor );

	if ( maxLines == 0 )
	{
		maxLines = 1;
	}

	pCurrent3A->dExpVal = (double)gain * exposure * binFactor / maxLines / fps100;

	return CAM_ERROR_NONE;
}

static CAM_Error _ov5642_set_exposure( CAM_Int32s iSensorFD, double dNewExposureValue )
{
	CAM_Int16u reg             = 0;

	// line_10ms is Banding Filter Value
	// here 10ms means the time unit is 10 mili-seconds
	CAM_Int32u lines_10ms      = 0;
	CAM_Int32u newMaxLines     = 0;
	CAM_Int32s newFrameRate100;
	CAM_Int32s newBinFactor;
	CAM_Int32s ret             = 0;

	CAM_Int16u newGainQ4       = 0; // the lowest 4 bit of gainQ4 arefractional part
	CAM_Int64u newExposure     = 0;
	CAM_Int64u newExposureGain = 0;

	CAM_Int8u  exposureLow     = 0;
	CAM_Int8u  exposureMid     = 0;
	CAM_Int8u  exposureHigh    = 0;

	CAM_Int16u gain            = 0;
	CAM_Int16u base            = 0;

	// get capture fps
	_ov5642_calc_framerate( iSensorFD, &newFrameRate100 );

	// stop AEC/AGC
	reg = 0x0007;
	_set_sensor_reg( iSensorFD, OV5642_REG_AECAGC, reg );

	// get capture exposure VTS
	reg = 0;
	_get_sensor_reg( iSensorFD, OV5642_REG_TIMINGVTSHIGH, &reg );
	newMaxLines = (reg & 0x00ff);
	newMaxLines <<= 8;

	reg = 0;
	_get_sensor_reg( iSensorFD, OV5642_REG_TIMINGVTSLOW, &reg );
	newMaxLines += (reg & 0x00ff);

	// get capture's banding filter value
	// TODO: 50Hz flicker as example
	lines_10ms = newFrameRate100 * newMaxLines / 10000;

	if ( lines_10ms == 0 )
	{
		lines_10ms = 1;
	}

	// get capture bin factor
	reg = 0;
	_get_sensor_reg( iSensorFD, OV5642_REG_BIN_HORIZONTAL, &reg );
	newBinFactor = ( 1 << reg );

	reg = 0;
	_get_sensor_reg( iSensorFD, OV5642_REG_BIN_VERTICAL, &reg );
	newBinFactor <<= reg;

	TRACE( CAM_INFO, "new bin factor: %d\n", newBinFactor );

	// calculate exposure * gain for capture mode
	newExposureGain = (CAM_Int64u)( dNewExposureValue * newMaxLines * newFrameRate100 / newBinFactor );

	// get exposure for capture
	if ( newExposureGain < ((CAM_Int64u)newMaxLines << 4) )
	{
		newExposure = (newExposureGain >> 4);
		if ( newExposure > lines_10ms )
		{
			newExposure /= lines_10ms;
			newExposure *= lines_10ms;
		}
	}
	else
	{
		newExposure = newMaxLines;
		newExposure /= lines_10ms;
		newExposure *= lines_10ms;
	}

	if ( newExposure == 0 )
	{
		newExposure = 1;
	}

	// get gain for capture
	newGainQ4 = (CAM_Int16u)( (newExposureGain * 2 / newExposure + 1) / 2 );

	// calculate exposure value for OV5642 register
	exposureLow  = (CAM_Int8u)(((newExposure) & 0x000f) << 4);
	exposureMid  = (CAM_Int8u)(((newExposure) & 0x0ff0) >> 4);
	exposureHigh = (CAM_Int8u)(((newExposure) & 0xf000) >> 12);

	// calculate gain value for OV5642 register
	base = 0x10;
	while ( newGainQ4 > 31 )
	{
		gain   |=  base;
		newGainQ4 >>= 1;
		base   <<= 1;
	}

	if ( newGainQ4 > 16 )
	{
		gain |= ((newGainQ4 - 16) & 0x0f);
	}

	// I still don't know the real meaning of the following statement
	if ( gain == 0x10 )
	{
		gain = 0x11;
	}

	// write gain and exposure to OV5642 registers

	/* gain */
	reg = (CAM_Int16u)(gain & 0xff);
	ret = _set_sensor_reg( iSensorFD, OV5642_REG_GAINLOW, reg );

	reg = (CAM_Int16u)((gain >> 8) & 0x1);
	ret = _set_sensor_reg( iSensorFD, OV5642_REG_GAINHIGH, reg );

	/* exposure */
	reg = (CAM_Int16u)exposureLow;
	ret = _set_sensor_reg( iSensorFD, OV5642_REG_EXPOSURELOW, reg );

	reg = (CAM_Int16u)exposureMid;
	ret = _set_sensor_reg( iSensorFD, OV5642_REG_EXPOSUREMID, reg );

	reg = (CAM_Int16u)exposureHigh;
	ret = _set_sensor_reg( iSensorFD, OV5642_REG_EXPOSUREHIGH, reg );

	return CAM_ERROR_NONE;
}

static CAM_Error _ov5642_pre_focus( OV5642State *pState )
{
	CAM_Int16u    reg;
	CAM_Int32s    cnt = 3000;
	CAM_Int32s    ret = 0;
	CAM_Error     error = CAM_ERROR_NONE;

	// reset sensor MCU to ensure AF work
	_set_sensor_reg( pState->stV4L2.iSensorFD, 0x3000, 5, 5, 5, 0x20 );
	CAM_Sleep( 5000 );
	_set_sensor_reg( pState->stV4L2.iSensorFD, 0x3000, 5, 5, 5, 0x00 );

#if 0
	while ( cnt )
	{
		_get_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_AF_FOCUSSTA, &reg );
		if ( reg == OV5642_FOCUS_STA_IDLE || reg == OV5642_FOCUS_STA_FOCUSED )
		{
			break;
		}
		else if ( reg == OV5642_FOCUS_STA_STARTUP )
		{
			CAM_Sleep( 1000 );
			cnt--;
		}
		else
		{
			TRACE( CAM_ERROR, "Error: AF firmware fatal error[%d]( %s, %s, %d )\n", reg, __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_DRIVEROPFAILED;
		}
	}
	if ( cnt < 0 )
	{
		TRACE( CAM_ERROR, "Error: AF firmware start too long( %s, %s, %d )\n", __FILE__, __FUNCTION__,  __LINE__ );
		return CAM_ERROR_DRIVEROPFAILED;
	}
#endif

	// set AF as IDLE
	_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_AF_ACK,  0x01 );
	_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_AF_MAIN, 0x08 );

	ret = _ov5642_wait_afcmd_completed( pState->stV4L2.iSensorFD );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: AF cmd failure( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_DRIVEROPFAILED;
	}

	error = _OV5642UpdateFocusZone( pState, pState->stV4L2.stShotParamSetting.eFocusZoneMode, &(pState->stV4L2.stShotParamSetting.stFocusROI) );
	if ( error != CAM_ERROR_NONE )
	{
		return error;
	}

	CELOG( "Re-launch focus zone\n" );
	_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_AF_ACK,  0x01 );
	_set_sensor_reg( pState->stV4L2.iSensorFD, OV5642_REG_AF_MAIN, 0x12 );
	ret = _ov5642_wait_afcmd_completed( pState->stV4L2.iSensorFD );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: AF cmd failure( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_DRIVEROPFAILED;
	}

	return error;
}
