// ============================================================================
// DxO Labs proprietary and confidential information
// Copyright (C) DxO Labs 1999-2009 - (All rights reserved)
// ============================================================================
/*******************************************************************************
//(C) Copyright [2010 - 2011] Marvell International Ltd.
//All Rights Reserved
*******************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/videodev2.h>

#include "cam_log.h"

#include "cam_socisp_sensorhal.h"

#include "registerMapOVT8820.h"

#define __ARM__

#undef FORCE_FULL_FRAME
//#define FORCE_FULL_FRAME

#undef I2C_DEBUG_ENABLE
//#define I2C_DEBUG_ENABLE

#define USE_SENSOR_GAIN
//#undef USE_SENSOR_GAIN  //use the real gain model

#define OV8820_DevAddr      (0x6c)
#define CCIC_TWSI_PORT_IDX      3


#ifndef NDEBUG
//#define CHECK_SENSOR_BEFORE_START
#endif

//
// Local defines:
//
#define SPECIAL_SENSOR_DEBUG
//#define CHECK_I2C

#if defined(__sparclet__)
#if defined(SPECIAL_SENSOR_DEBUG)
#undef SPECIAL_SENSOR_DEBUG
#endif
#endif

//
// Macros
//

#if defined(i386) && defined(FIRMWARE_ON_HOST) && !defined(ISS)
//
// Firmware runs on Linux host but Tango is on a Bartok board:
//
#if defined(SPECIAL_SENSOR_DEBUG)
#define PRINTF     TRACE
#define PRINTF_I2C TRACE
#else
#define PRINTF(...)
#define PRINTF_I2C(...)
#endif
#elif defined(__sparclet__) || defined(__arm__)
//
// Otherwise, firmware runs on a sparclet processor:
//
#if defined(CHECK_I2C) && !defined(NDEBUG)
#define PRINTF_I2C  cprintf
#define PRINTF(...)
#else
#define PRINTF(...)
#define PRINTF_I2C(...)
#endif
#else
//
// Finally, should not go there:
//
//#error "Unexpected use case"
#endif

#define I2C_SENSOR_ADDRESS                                              0x10
#define I2C_MASTER_INSTANCE_ID                                             0
#define I2C_MASTER_EXTENDED_ADDR                                           0
#define DXO_RAW_MULTIPLE                                                   8
#define DXOSENSOR_EXT_CLK                                                  4
#define EXT_CLK                                                           25
#define DXOSENSOR_EXT_CLK_FREQ_MHZ            (EXT_CLK << DXOSENSOR_EXT_CLK)
#define EXPO_TIME_FRAC_PART                                               16
#define GAIN_FRAC_PART                                                     8
#define DXOSENSOR_MAX_HOR_BINNING_FACTOR                                   2
#define DXOSENSOR_MAX_VER_BINNING_FACTOR                                   2
#define CLOCK_DIV_VALUE                                                  100
#define NB_MIPI_LANES                                                      2

#define WRITE_BY_CACHE(offset,value)     writeCacheDevice ( offset, value, Size_##offset, cache_##offset  )
#define READ_BY_CACHE(offset)            readCacheDevice  ( offset       , Size_##offset, cache_##offset  )
#define WRITE_DEVICE(offset,value)       writeDevice      ( offset, value, Size_##offset  )
#define READ_DEVICE(offset)              readDevice       ( offset       , Size_##offset  )


#define SHIFT_RND(x,n) ( ( (x)+((n)!=0?1<<((n)-1):0) )>>(n) )

/*****************************************************************************/
/* External prototype declaration                                            */
/*****************************************************************************/

extern void     cprintf       (      char* format, ...                      );


/*****************************************************************************/
/* Specific used source File(s)                                              */
/*****************************************************************************/
#ifndef __ARM__
#include INC(I2CDRIV_INC_DIR/i2cmaster_drv.h)
#else
#define DIV_UP(x,y)                       (((x)+(y)-1)/(y))
#define MIN(x,y)                          ((x)<(y)?(x):(y))
#define MAX(x,y)                          ((x)>(y)?(x):(y))
#endif
/*****************************************************************************/
/* types definitons                                                          */
/*****************************************************************************/
typedef struct {
	uint16_t     usAF                   ;
	uint16_t     usAG                   ;
	uint16_t     usDG[DxOISP_NB_CHANNEL];
	uint16_t     usYstart               ;
	uint16_t     usYstop                ;
	uint16_t     usXstart               ;
	uint16_t     usXstop                ;
	uint8_t      ucIncX                 ;
	uint8_t      ucIncY                 ;
	uint16_t     usOutSizeX             ;
	uint16_t     usOutSizeY             ;
	uint16_t     usLineLength           ;
	uint16_t     usFrameLength          ;
	uint32_t     uiExpoTime             ;
	uint8_t      ucTimingRegX           ;
	uint8_t      ucTimingRegY           ;
	uint8_t      ucMagicBinReg          ;
	uint8_t      ucPsRamCtrl0           ;
	uint8_t      isStreaming            ;
} ts_sensorRegister;

typedef struct {
	uint8_t ucX;
	uint8_t ucY;
} ts_Decim;

#define NO_ACCESS       -1
#define RW_ACCESS        0
#define SIZE_CACHE_DATA     (INTEGRATION_TIME_REG \
                          + ANALOG_GAIN_REG      \
                          + DIGITAL_GAIN_REG     \
                          + GEOMETRY_REG         \
                          + FRAME_RATE_REG       \
                          + MODE_SELECT_REG      \
                          + AF_REG               \
                          + CROP_REG             \
                          + TIMING_REG)

typedef struct {
	int32_t    iLastAccess;
	uint16_t   usData;
} ts_cache;

/*****************************************************************************/
/* static variables definitons                                               */
/*****************************************************************************/
static ts_SENSOR_Cmd       S_stCmd                     ;
static ts_SENSOR_Status    S_stStatus                  ;
static ts_sensorRegister   S_stRegister                ; // structure accessed by the set and get function
static uint32_t            S_uiSensorPixelClock        ;
static ts_cache            S_cache[SIZE_CACHE_DATA]    ;
// static uint16_t            S_usAppliedCoarseIT         ;
// static uint16_t            S_usAppliedFineIT           ;
// static uint16_t            S_usAppliedAGain            ;
// static uint16_t            S_usAppliedDGain_Gr         ;
// static uint16_t            S_usAppliedDGain_R          ;
// static uint16_t            S_usAppliedDGain_B          ;
// static uint16_t            S_usAppliedDGain_Gb         ;

/*****************************************************************************/
/* Static functions prototype declaration                                    */
/*****************************************************************************/
static uint32_t I2C_Write                    ( uint16_t usOffset, uint8_t* ptucData, uint8_t ucSize );
static uint32_t I2C_Read                     ( uint16_t usOffset, uint8_t* ptucData, uint8_t ucSize );

static void     writeDevice                  ( uint16_t offset, uint16_t usValue, uint8_t size                );
static void     writeCacheDevice             ( uint16_t offset, uint16_t usValue, uint8_t size, uint8_t ucIdx );
static uint16_t readDevice                   ( uint16_t offset, uint8_t  size                                 );
static uint16_t readCacheDevice              ( uint16_t offset, uint8_t  size                 , uint8_t ucIdx );
static void     initSensorCmd                ( ts_SENSOR_Cmd* pttsCmd, ts_SENSOR_Status* pttsStatus );
static void     setStaticSensorConfiguration ( int                      sel                         );
static uint32_t computePixelClock            ( void                                                 );
static void     getCapabilities              ( ts_SENSOR_Status*        pttsStatus                  );
static void     getDigitalGain               ( ts_SENSOR_Status*        pttsStatus                  );
static void     getAnalogGain                ( ts_SENSOR_Status*        pttsStatus                  );
static void     getImageFeature              ( ts_SENSOR_Status*        pttsStatus                  );
static void     getFrameRate                 ( ts_SENSOR_Status*        pttsStatus                  );
static void     setExposureTime              ( void                                                 );
static void     getExposureTime              ( ts_SENSOR_Status*        pttsStatus                  );
static void     getMode                      ( ts_SENSOR_Status*        pttsStatus                  );
static void     getPixelClock                ( ts_SENSOR_Status*        pttsStatus                  );
static void     getFrameValidTime            ( ts_SENSOR_Status*        ptttStatus                  );
static void     getEstimatedFocus            ( ts_SENSOR_Status*        pttsStatus                  );
static void     setGain                      ( uint16_t usAGain[] , uint16_t usDGain[]              );
static void     setCrop                      ( void                                                 );
static void     setOrientation               ( uint8_t                  ucOrientation               );
static void     setFrameRate                 ( uint16_t                 usFrameRate                 );
static void     setBinning                   ( uint8_t                  isBinning                   );
static ts_Decim setSkipping                  ( uint8_t ucSkipX,         uint8_t ucSkipY             );
static void     setDecimation                ( uint8_t ucDecimX,        uint8_t ucDecimY            );
static void     setOutputSize                ( ts_Decim stSkipping                                  );
static void     setMode                      ( uint8_t                  isStreaming                 );
static void     setMinimalBlanking           ( uint16_t usMinVertBlk, uint16_t usMinHorBlk          );
static void     applySettingsToSensor        ( void                                                 );
#if defined(CHECK_SENSOR_BEFORE_START)
static void     autoCheckPluggedSensor       ( void                                                 );
#endif

static void     initCache                    ( void                                                 );
static void     historyMngt                  ( uint8_t ucMode                                       );


#include "cam_socisp_sensorbase.c"

/*****************************************************************************/
/* Static functions definition                                               */
/*****************************************************************************/

static void assertf0(int a, char *p)
{
	if(a == 0)
	{
		printf ("%s", p) ;
	}
}

static void assertf1(int a, char *p, unsigned int i)
{
	if(a == 0)
	{
		printf ("%s, %d",p, i) ;
	}
}

static void assertf2(int a, char *p, unsigned int i, unsigned int j)
{
	if(a == 0)
	{
		printf ("%s, %d, %d",p, i, j) ;
	}
}

static void writeDevice( uint16_t usOffset, uint16_t usValue, uint8_t ucSize ) {
	uint32_t uiError;
	uint8_t  ptucData[2];

	switch(ucSize) {
		case 1:
			ptucData[0] =             (uint8_t)usValue;
			ptucData[1] =                            0;
			break;
		case 2:
			ptucData[0] = (( usValue & 0xff00 ) >> 8 );
			ptucData[1] = (  usValue & 0x00ff        );
			break;
		default:
			assertf1(0,"Illegal length = %d", ucSize );
			break;
	}
	uiError = I2C_Write( usOffset, ptucData, ucSize );

	if(ucSize == 1) {
		PRINTF_I2C("0x%04X=0x%02X\n", usOffset, ptucData[0]);
	}
	else {
		PRINTF_I2C("0x%04X=0x%02X\n", usOffset, ptucData[0]);
		PRINTF_I2C("0x%04X=0x%02X\n", usOffset+1, ptucData[1]);
	}

	//PRINTF_I2C("0x%04X = 0x%02X%02X\n", usOffset, ucSize==1?0x00:ptucData[0], ucSize==1?ptucData[0]:ptucData[1]);

	assertf1(!uiError,"reading device error (err=%lu)", uiError );
}


static uint16_t readDevice( uint16_t usOffset, uint8_t ucSize ) {
	uint32_t uiError;
	uint16_t usValue = 0;
	uint8_t  ptucData[2];

	uiError = I2C_Read( usOffset, ptucData, ucSize );
	// CELOG("I2C READ: [0x%04X]:[%02X|%02X]\n", usOffset, ucSize==1?0x00:ptucData[0], ucSize==1?ptucData[0]:ptucData[1]);
	assertf2( !uiError, "reading device error (err=%lu (0x%08x))", uiError , uiError );

	switch(ucSize) {
		case 1:
			usValue = ptucData[0];
			break;
		case 2:
			usValue  = (ptucData[0]      << 8);
			usValue |= (ptucData[1] & 0x00ff );
			break;
		default:
			assertf1(0,"Illegal length = %d", ucSize );
			break;
	}
	return usValue;
}


static void writeCacheDevice( uint16_t usOffset, uint16_t usValue, uint8_t ucSize, uint8_t ucIdx ) {
	switch (S_cache[ucIdx].iLastAccess) {
		case NO_ACCESS:
			//readCacheDevice(usOffset,ucSize,ucIdx);
			writeDevice(usOffset,usValue,ucSize);
			S_cache[ucIdx].usData = usValue;
			S_cache[ucIdx].iLastAccess = RW_ACCESS;
			break;
		case RW_ACCESS:
			if ( usValue != S_cache[ucIdx].usData ) {
				writeDevice(usOffset,usValue,ucSize);
				S_cache[ucIdx].usData = usValue;
			}

			//S_cache[ucIdx].iLastAccess = RW_ACCESS;
			break;
		default:
			assertf2(0,"unexpected access status [%d]=%lu", ucIdx, S_cache[ucIdx].iLastAccess );
			break;
	}
}


static uint16_t readCacheDevice( uint16_t usOffset, uint8_t ucSize, uint8_t ucIdx ) {
	uint16_t usTmp;
	switch (S_cache[ucIdx].iLastAccess) {
		case NO_ACCESS:
			usTmp = readDevice(usOffset,ucSize);
			S_cache[ucIdx].usData = usTmp;
		case RW_ACCESS:
			S_cache[ucIdx].iLastAccess = RW_ACCESS;
			break;
		default:
			assertf2(0,"unexpected access status [%d]=%lu", ucIdx, S_cache[ucIdx].iLastAccess );
			break;
	}
	return S_cache[ucIdx].usData;
}


static void getCapabilities( ts_SENSOR_Status* pttsStatus ) {
	pttsStatus->stCapabilities.stImageFeature.ucMaxHorDownsamplingFactor  = 2; //Only binning is used as the frame rate is enough
	pttsStatus->stCapabilities.stImageFeature.ucMaxVertDownsamplingFactor = 2;
	pttsStatus->stCapabilities.stImageFeature.usBayerWidth                = X_OUTPUT_SIZE_MAX   >> 1;
	pttsStatus->stCapabilities.stImageFeature.usBayerHeight               = Y_OUTPUT_SIZE_MAX   >> 1;
	pttsStatus->stCapabilities.stShootingParam.usMaxAGain                 = 62<<GAIN_FRAC_PART;//((1<<5) * (0xF/16 + 1))<<8;
	pttsStatus->stCapabilities.stShootingParam.usMaxDGain                 = DIGITAL_GAIN_MAX;
	pttsStatus->stCapabilities.stImageFeature.usMinHorBlanking            = MIN_LINE_BLANKING_PCK;
	pttsStatus->stCapabilities.stImageFeature.usMinVertBlanking           = MIN_FRAME_BLANKING_LINE;
	pttsStatus->stCapabilities.stImageFeature.eImageType                  = DxOISP_FORMAT_RAW;
	pttsStatus->stCapabilities.stImageFeature.ucPixelWidth                = 10;
	pttsStatus->stCapabilities.stImageFeature.ePhase                      = DxOISP_SENSOR_PHASE_BGGR;
	pttsStatus->stCapabilities.stImageFeature.eEncoding                   = 0;
	pttsStatus->stCapabilities.stImageFeature.ucMaxDelayImageFeature      = 1;
	pttsStatus->stCapabilities.stImageFeature.uiMaxPixelClock             = computePixelClock()<<12; //pixel clock is fixed
	pttsStatus->stCapabilities.stImageFeature.ucNbUpperDummyLines         = 0;
	pttsStatus->stCapabilities.stImageFeature.ucNbLeftDummyColumns        = 0;
	pttsStatus->stCapabilities.stImageFeature.ucNbLowerDummyLines         = 0;
	pttsStatus->stCapabilities.stImageFeature.ucNbRightDummyColumns       = 0;
	pttsStatus->stCapabilities.stImageFeature.usPedestal                  = 32;
	pttsStatus->stCapabilities.stAfPosition.usMin                         = MIN_AF_POSITION;
	pttsStatus->stCapabilities.stAfPosition.usMax                         = MAX_AF_POSITION;
	pttsStatus->stCapabilities.stAperture.usMin                           =
	pttsStatus->stCapabilities.stAperture.usMax                           = 2007;
	pttsStatus->stCapabilities.stZoomFocal.usMin                          =
	pttsStatus->stCapabilities.stZoomFocal.usMax                          = 0;
	pttsStatus->stCapabilities.stEdof.isEdof                              = 0;
	pttsStatus->stCapabilities.stFlash.usMaxPower                         = 0;
	pttsStatus->stCapabilities.enableInterframeCmdOnly                    = 0;
	pttsStatus->isImageFeatureStable                                      = 1; //Sensor can mask invalid frame
	pttsStatus->isImageQualityStable                                      = 1;
	pttsStatus->isFrameInvalid                                            = 0;
	pttsStatus->stAppliedCmd.usAperture                                   = 2007; //Fixed aperture
	pttsStatus->eCurrentPhase                                             = DxOISP_SENSOR_PHASE_BGGR;
}


static void getDigitalGain( ts_SENSOR_Status* pttsStatus ) {
	pttsStatus->stAppliedCmd.stShootingParam.usDGain[DxOISP_CHANNEL_GB] =
	pttsStatus->stAppliedCmd.stShootingParam.usDGain[DxOISP_CHANNEL_GR] = READ_BY_CACHE( Offset_DG_G ) >> 2;
	pttsStatus->stAppliedCmd.stShootingParam.usDGain[DxOISP_CHANNEL_R ] = READ_BY_CACHE( Offset_DG_R ) >> 2;
	pttsStatus->stAppliedCmd.stShootingParam.usDGain[DxOISP_CHANNEL_B ] = READ_BY_CACHE( Offset_DG_B ) >> 2;
	//DEBUGMSG(TTC_DBG_LEVEL,("DG:%d\n", pttsStatus->stAppliedCmd.stShootingParam.usDGain[DxOISP_CHANNEL_B ]));
}

#define BIT(w)   (1U<<(w))
// provide a mask of w bits. Does not generate warning with w from 0 to 32.
#define MSK(w)   ( ( (w)>>5 ? 0U : BIT((w)&0x1f) ) - 1 )

static void setGain(uint16_t usAGain[], uint16_t usDGain[])
{
	int i;
	unsigned int uiMult2 = 0;
	uint16_t usTotalGain[DxOISP_NB_CHANNEL];
	uint16_t usTotalGainMin = 0xffff;
	uint16_t usAgain;
	uint16_t usGainTmp;
	uint32_t uiInvGain;

	for (i=0; i<DxOISP_NB_CHANNEL; i++) {
		usTotalGain[i] = (usAGain[i] * usDGain[i]) >> GAIN_FRAC_PART;
		usTotalGainMin = MIN(usTotalGain[i], usTotalGainMin);
	}

	//Compute Analog gain
#ifdef USE_SENSOR_GAIN
	usGainTmp = usTotalGainMin>>4;
	while(usGainTmp > 31) {
		uiMult2++;
		usGainTmp>>=1;
	}
	if(uiMult2>5) { //Saturation to max analog gain
		uiMult2   = 5;
		usGainTmp = 31;
	}
	S_stRegister.usAG = (MSK(uiMult2) << 4) + (usGainTmp-16);

	//compute real analog gain applied
	usAgain = ((1<<uiMult2) * usGainTmp)<<4; //8.8
#else
	S_stRegister.usAG = MIN(usTotalGainMin>>4, 0x3FF);
	usAgain = S_stRegister.usAG << 4;
#endif

	uiInvGain = (1 << (GAIN_FRAC_PART * 2)) / usAgain;
	for (i=0; i<DxOISP_NB_CHANNEL; i++) {
		S_stRegister.usDG[i] = SHIFT_RND(usTotalGain[i] * uiInvGain, (GAIN_FRAC_PART-2));
	}
	//DEBUGMSG(TTC_DBG_LEVEL,("usDG:%d\n", S_stRegister.usDG[0]));
}

static void getAnalogGain( ts_SENSOR_Status* pttsStatus ) {
	unsigned int i;
	uint16_t usReg    = READ_BY_CACHE( Offset_AG );

#ifdef USE_SENSOR_GAIN
	uint8_t ucBitField = (uint8_t)(usReg>>4);
	uint8_t ucMult    = 0;
	for(i=0;i<5;i++) {
		if(ucBitField&(1<<i)) ucMult++;
	}

	if(ucMult==0)
		ucMult = 1;
	else
		ucMult = 1<<ucMult;

	pttsStatus->stAppliedCmd.stShootingParam.usAGain[DxOISP_CHANNEL_GR] =
	pttsStatus->stAppliedCmd.stShootingParam.usAGain[DxOISP_CHANNEL_B]  =
	pttsStatus->stAppliedCmd.stShootingParam.usAGain[DxOISP_CHANNEL_R]  =
	pttsStatus->stAppliedCmd.stShootingParam.usAGain[DxOISP_CHANNEL_GB] = (((usReg&0x000f) + 16) *  ucMult)<<4;
#else

	pttsStatus->stAppliedCmd.stShootingParam.usAGain[DxOISP_CHANNEL_GR] =
	pttsStatus->stAppliedCmd.stShootingParam.usAGain[DxOISP_CHANNEL_B]  =
	pttsStatus->stAppliedCmd.stShootingParam.usAGain[DxOISP_CHANNEL_R]  =
	pttsStatus->stAppliedCmd.stShootingParam.usAGain[DxOISP_CHANNEL_GB] = usReg<<4;
#endif
	//DEBUGMSG(TTC_DBG_LEVEL,("AG=%d\n", pttsStatus->stAppliedCmd.stShootingParam.usAGain[DxOISP_CHANNEL_GB]));
}


static void setCrop( void ) {
	assertf1( S_stCmd.stImageFeature.usYBayerEnd < S_stStatus.stCapabilities.stImageFeature.usBayerHeight
	        , "usYBayerEnd = %d", S_stCmd.stImageFeature.usYBayerEnd );
	assertf1( S_stCmd.stImageFeature.usXBayerEnd < S_stStatus.stCapabilities.stImageFeature.usBayerWidth
	        , "usXBayerEnd = %d", S_stCmd.stImageFeature.usXBayerEnd );

	S_stRegister.usYstop   = S_stCmd.stImageFeature.usYBayerEnd   * 2 + 1;
	S_stRegister.usXstop   = S_stCmd.stImageFeature.usXBayerEnd   * 2 + 1;
	S_stRegister.usYstart  = S_stCmd.stImageFeature.usYBayerStart * 2    ;
	S_stRegister.usXstart  = S_stCmd.stImageFeature.usXBayerStart * 2    ;
	//DEBUGMSG(TTC_DBG_LEVEL,("XS:%d YS:%d XE:%d YE:%d\n", S_stRegister.usXstart, S_stRegister.usYstart, S_stRegister.usXstop, S_stRegister.usYstop));
}


static void setOrientation( uint8_t ucOrientation ) {
	assertf1( ucOrientation <= 3, "unexpected orientation (%d)", ucOrientation );
	if(ucOrientation&0x01)
		S_stRegister.ucTimingRegX |= 0x6;
	else
		S_stRegister.ucTimingRegX &= ~0x6;

	if(ucOrientation&0x02)
		S_stRegister.ucTimingRegY |= 0x6;
	else
		S_stRegister.ucTimingRegY &= ~0x6;
}


static uint32_t computePixelClock( void ) {
#ifdef PLATFORM_PROCESSOR_MMP3
	// the MMP3 sensor MCLK is 26MHZ not as the expected 25MHZ.
	return 26 * 3333 / 25;
#else
	return 3333;//2134;//133.367040<<DXOSENSOR_EXT_CLK; //TO DO: compute this value
#endif
}


static void getImageFeature( ts_SENSOR_Status* pttsStatus ) {
	pttsStatus->stAppliedCmd.stImageFeature.usXBayerStart = READ_BY_CACHE( Offset_x_addr_start      ) / 2;
	pttsStatus->stAppliedCmd.stImageFeature.usYBayerStart = READ_BY_CACHE( Offset_y_addr_start      ) / 2;
	pttsStatus->stAppliedCmd.stImageFeature.usXBayerEnd   = READ_BY_CACHE( Offset_x_addr_end        ) / 2;
	pttsStatus->stAppliedCmd.stImageFeature.usYBayerEnd   = READ_BY_CACHE( Offset_y_addr_end        ) / 2;

	pttsStatus->stAppliedCmd.stImageFeature.eOrientation  = ((READ_BY_CACHE(Offset_timing_tc_reg21)&0x2)>>1)  +   (READ_BY_CACHE(Offset_timing_tc_reg20)&0x2);

	pttsStatus->stAppliedCmd.stImageFeature.usMinVertBlanking = READ_BY_CACHE( Offset_frame_length_lines ) - READ_BY_CACHE( Offset_y_output_size );
	pttsStatus->stAppliedCmd.stImageFeature.usMinHorBlanking  = READ_BY_CACHE( Offset_line_length_pck )    - READ_BY_CACHE( Offset_x_output_size );

	pttsStatus->ucHorSkippingFactor                                  =
	pttsStatus->stAppliedCmd.stImageFeature.ucHorDownsamplingFactor  = ((READ_BY_CACHE(Offset_x_odd_even_inc)>>4) + 1 ) / 2;
	pttsStatus->ucVertSkippingFactor                                 =
	pttsStatus->stAppliedCmd.stImageFeature.ucVertDownsamplingFactor = ((READ_BY_CACHE(Offset_y_odd_even_inc)>>4) + 1 ) / 2;

	if ( READ_BY_CACHE(Offset_timing_tc_reg20)&0x01) {
		pttsStatus->ucHorSkippingFactor  >>= 1;
		pttsStatus->ucVertSkippingFactor >>= 1;
	}

	//DEBUGMSG(TTC_DBG_LEVEL,("XE=%d YE:%d\n", pttsStatus->stAppliedCmd.stImageFeature.usXBayerEnd, pttsStatus->stAppliedCmd.stImageFeature.usYBayerEnd));
}


static uint16_t computeFps(uint16_t usLineLenght, uint16_t usFrameLenght) {
	uint32_t uiImageSize = usLineLenght * usFrameLenght;
	return (((((S_uiSensorPixelClock*1000)<<8)+uiImageSize/2) / uiImageSize) * 1000)>> 8;
}

static void getFrameRate( ts_SENSOR_Status* pttsStatus ) {
	pttsStatus->stAppliedCmd.stImageFeature.usFrameRate = computeFps(READ_BY_CACHE( Offset_line_length_pck ), READ_BY_CACHE( Offset_frame_length_lines ));
}


static void setFrameRate( uint16_t usNewFrameRate ) {
	uint32_t uiResult;
	uint32_t uiTmp = S_stRegister.usLineLength * usNewFrameRate;
	assertf0( usNewFrameRate!=0, "unexpected frame rate set to 0!!!" );

	uiResult = (   (  (((S_uiSensorPixelClock*1000)<<8)+uiTmp/2) / uiTmp )   *1000)>>8;
	S_stRegister.usFrameLength = MIN(MAX_FRAME_LENGTH_LINE, MAX(MIN_FRAME_LENGTH_LINE, uiResult));
}


static void getExposureTime( ts_SENSOR_Status* pttsStatus ) {
	uint32_t uiLineLenghtPixClk = READ_BY_CACHE(Offset_line_length_pck);
	uint16_t usExpoMSB          = READ_BY_CACHE(Offset_ExposureMSB);
	uint16_t usExpo             = READ_BY_CACHE(Offset_Exposure);
	uint32_t uiLineLength       = uiLineLenghtPixClk << 12;
	uint32_t uiLineTime         = uiLineLength / S_uiSensorPixelClock;        //in ns expressed in 24.8

	pttsStatus->stAppliedCmd.stShootingParam.uiExpoTime = ((((((usExpoMSB<<16) +  usExpo)>>4) * uiLineTime)/1000)<<8);
	//DEBUGMSG(TTC_DBG_LEVEL,("getET:%d\n", pttsStatus->stAppliedCmd.stShootingParam.uiExpoTime));
}


static void setBinning ( uint8_t isBinning ) {
	//S_stRegister.ucTimingRegY |= (isBinning&0x1);
	S_stRegister.ucTimingRegX = isBinning ? (S_stRegister.ucTimingRegX|0x01) : (S_stRegister.ucTimingRegX&(~0x01));
	S_stRegister.ucMagicBinReg = isBinning << 3;
	S_stRegister.ucPsRamCtrl0  = (!isBinning) << 1;
}


static ts_Decim setSkipping( uint8_t ucNewSkipX,uint8_t ucNewSkipY ) {
	ts_Decim stSkip;
	stSkip.ucX = MIN( S_stStatus.stCapabilities.stImageFeature.ucMaxHorDownsamplingFactor,  ucNewSkipX );
	stSkip.ucY = MIN( S_stStatus.stCapabilities.stImageFeature.ucMaxVertDownsamplingFactor, ucNewSkipY );

	S_stRegister.ucIncX  = ((2 * stSkip.ucX - 1)<<4) + 1;
	S_stRegister.ucIncY  = ((2 * stSkip.ucY - 1)<<4) + 1;

	return stSkip;
}

static void setDecimation( uint8_t ucDecimX, uint8_t ucDecimY ) {
	uint8_t isBinning=0;

	assertf1(((ucDecimX>=1) && (ucDecimX<=4)),"ucDecimX (%d) is uncorrect!", ucDecimX );
	assertf1(((ucDecimY>=1) && (ucDecimY<=4)),"ucDecimY (%d) is uncorrect!", ucDecimY );

	if ((ucDecimX>=2) && (ucDecimX%2 == 0) && (ucDecimY>=2) && (ucDecimY%2 == 0)) isBinning = 1;
	setBinning(isBinning);
	setOutputSize(setSkipping(ucDecimX,ucDecimY));
}

static void setOutputSize(ts_Decim stSkip ) {
	uint16_t usTmp,usXStop,usYStop;

	usTmp   = S_stRegister.usXstop  - S_stRegister.usXstart +1;
	usTmp   = DIV_UP ( MAX(usTmp,X_OUTPUT_SIZE_MIN)/2 , stSkip.ucX )*2;
	usTmp   = (uint16_t)(DIV_UP(usTmp, (DXO_RAW_MULTIPLE/stSkip.ucX)) * (DXO_RAW_MULTIPLE/stSkip.ucX));
	usXStop = usTmp*stSkip.ucX + S_stRegister.usXstart -1 + 2 * HORIZONTAL_OFFSET*stSkip.ucX;
	if(usXStop <= X_ADDR_MAX)         {
		S_stRegister.usXstop    = usXStop;
		S_stRegister.usOutSizeX = usTmp;
	}
	else {
		S_stRegister.usXstop    = X_ADDR_MAX;
		S_stRegister.usOutSizeX = DIV_UP((X_ADDR_MAX + 1)/2, stSkip.ucX)*2 ;
	}

	usTmp   = S_stRegister.usYstop  - S_stRegister.usYstart +1;
	usTmp   = DIV_UP ( MAX(usTmp,Y_OUTPUT_SIZE_MIN)/2, stSkip.ucY )*2;
	usYStop = usTmp*stSkip.ucY + S_stRegister.usYstart -1 + 2 * VERTICAL_OFFSET * stSkip.ucY;
	if(usYStop <= Y_ADDR_MAX)         {
		S_stRegister.usYstop    = usYStop;
		S_stRegister.usOutSizeY = usTmp;
	}
	else {
		S_stRegister.usYstop    = Y_ADDR_MAX;
		S_stRegister.usOutSizeY = DIV_UP((Y_ADDR_MAX + 1)/2, stSkip.ucY)*2 ;
	}

	//DEBUGMSG(TTC_DBG_LEVEL,("XS:%d YS:%d XE:%d YE:%d outX:%d outY:%d\n", S_stRegister.usXstart, S_stRegister.usYstart, S_stRegister.usXstop, S_stRegister.usYstop, S_stRegister.usOutSizeX, S_stRegister.usOutSizeY));
}


static void setMode( uint8_t isStreaming ) {
	S_stRegister.isStreaming = isStreaming;
}


static void getMode ( ts_SENSOR_Status* pttsStatus ) {
	pttsStatus->stAppliedCmd.isSensorStreaming =  (uint8_t)READ_BY_CACHE( Offset_mode_select );
}

static void setMinimalBlanking( uint16_t usMinVertBlk, uint16_t usMinHorBlk ) {
	uint32_t uiLineTime;
	uint16_t usVertical;
	//uint16_t usHorizontal = MAX( usMinHorBlk,  MIN_LINE_BLANKING_PCK);
	//This rule rule has been determined by experiences
	uint16_t usHorizontal = MAX( usMinHorBlk,  ((S_stRegister.usOutSizeX * 17236 >> 16) - 30));

	S_stRegister.usLineLength = usHorizontal + S_stRegister.usOutSizeX;
	S_stRegister.usLineLength = MAX( MIN_LINE_LENGTH_PCK, S_stRegister.usLineLength );
	S_stRegister.usLineLength = MIN( MAX_LINE_LENGTH_PCK, S_stRegister.usLineLength );

	//forbidden value for usLineLength: 0x1c/0x1d 0x5c/0x5d, 0x9c/0x9d and 0xdc/0xdd
	if((S_stRegister.usLineLength & 0x003e) == 0x1c) {
		S_stRegister.usLineLength += 2;
	}

	uiLineTime = (S_stRegister.usLineLength << 15) / (S_uiSensorPixelClock<<3);         //in ns expressed in 24.8
	usVertical = MAX((DIV_UP(usMinVertBlk<<12,uiLineTime)*1000) >> 12, MIN_FRAME_BLANKING_LINE) ;
	S_stRegister.usFrameLength = usVertical + S_stRegister.usOutSizeY;

	//DEBUGMSG(TTC_DBG_LEVEL,("LL:%d FL:%d\n", S_stRegister.usLineLength, S_stRegister.usFrameLength));

	S_stRegister.usFrameLength = MAX( MIN_FRAME_LENGTH_LINE, S_stRegister.usFrameLength );
#if (MAX_FRAME_LENGTH_LINE != 0xFFFF)
	S_stRegister.usFrameLength = MIN( MAX_FRAME_LENGTH_LINE, S_stRegister.usFrameLength );
#endif
}

static void setAF(void)
{
	uint16_t usAfPos;
	usAfPos = MIN(MAX(S_stCmd.usAfPosition, MIN_AF_POSITION), MAX_AF_POSITION);
	//DEBUGMSG(TTC_DBG_LEVEL,("Af c:%d\n",S_stCmd.usAfPosition));
	S_stRegister.usAF = ((((usAfPos&0x000f)<<4) + 0xf)<<8) + ((usAfPos>>4)&0xff);
	//DEBUGMSG(TTC_DBG_LEVEL,("Af reg:%d\n",S_stRegister.usAF));

}

static void getAF( ts_SENSOR_Status* pttsStatus )
{
	uint16_t usReg = READ_BY_CACHE(Offset_Af);
	pttsStatus->stAppliedCmd.usAfPosition = (usReg-0)>>4;
}

static void setExposureTime( void )
{
	uint32_t uiLineLength        = S_stRegister.usLineLength << 12;
	uint32_t uiLineTime          = uiLineLength / (S_uiSensorPixelClock);        //in ns expressed in 24.8
	uint16_t usNewframeRate      = 0;

	//S_stCmd.stShootingParam.uiExpoTime = 15<<16;

	//Note: we remove 4 LSB bits to avoid a bug of Ov8820
	S_stRegister.uiExpoTime = ((((S_stCmd.stShootingParam.uiExpoTime<<4) /uiLineTime)*1000) >> 8) & 0x000ffff0; //16.4

#if MIN_INTEGRATION_TIME != 0
	S_stRegister.uiExpoTime  = MAX( MIN_INTEGRATION_TIME                    , S_stRegister.usCoarseIntTime );
#endif

	//DEBUGMSG(TTC_DBG_LEVEL,("Req:%d ET:%d\n", S_stCmd.stShootingParam.uiExpoTime, S_stRegister.uiExpoTime));

	//Determine new Frame_length_lines Min for this exposure Time
	S_stRegister.usFrameLength    = MAX ((S_stRegister.uiExpoTime>>4) + 20, S_stRegister.usFrameLength );

	usNewframeRate = computeFps(S_stRegister.usLineLength, S_stRegister.usFrameLength);

	//DEBUGMSG(TTC_DBG_LEVEL,("FL:%d\n",S_stRegister.usFrameLength));
	if (usNewframeRate > S_stCmd.stImageFeature.usFrameRate)
		setFrameRate(S_stCmd.stImageFeature.usFrameRate);

	//DEBUGMSG(TTC_DBG_LEVEL,("fps:%d LL:%d FL:%d\n", S_stCmd.stImageFeature.usFrameRate, S_stRegister.usLineLength, S_stRegister.usFrameLength));
}

static void getPixelClock ( ts_SENSOR_Status* pttsStatus ) {
	pttsStatus->uiPixelClock = S_uiSensorPixelClock << 12; //28.4 to 16.16
}

#if defined(CHECK_SENSOR_BEFORE_START)
void autoCheckPluggedSensor ( void )  {
	uint16_t usModelId     = READ_DEVICE ( Offset_model_id        );
	uint16_t usRevNumber   = READ_DEVICE ( Offset_revision_number );
	uint16_t usManuId      = READ_DEVICE ( Offset_manufacturer_id );
	uint16_t usSmiaVers    = READ_DEVICE ( Offset_smia_version    );

	assertf2( REG_MODEL_ID        == usModelId  , "unexpected model id (0x%04X instead of 0x%04X", usModelId  , REG_MODEL_ID        );
//	assertf2( REG_REVISION_NUMBER == usRevNumber, "unexpected rev nb   (0x%04X instead of 0x%04X", usRevNumber, REG_REVISION_NUMBER );
	assertf2( REG_MANUFACTURER_ID == usManuId   , "unexpected man id   (0x%04X instead of 0x%04X", usManuId   , REG_MANUFACTURER_ID );
	assertf2( REG_SMIA_VERSION    == usSmiaVers , "unexpected rev smia (0x%04X instead of 0x%04X", usSmiaVers , REG_SMIA_VERSION    );
}
#endif


static void initSensorCmd ( ts_SENSOR_Cmd*  pttsCmd, ts_SENSOR_Status* pttsStatus ) {
	#define DEFAULT_FRAMERATE                   10<<4
	#define DEFAULT_EXPOSURE_TIME               30<<EXPO_TIME_FRAC_PART
	#define DEFAULT_ANALOG_GAIN                 1<<GAIN_FRAC_PART
	#define DEFAULT_DIGITAL_GAIN                1<<GAIN_FRAC_PART

	pttsCmd->stImageFeature.eOrientation             = DxOISP_FLIP_OFF_MIRROR_OFF;
	pttsCmd->stImageFeature.usXBayerEnd              = pttsStatus->stCapabilities.stImageFeature.usBayerWidth  - 1;
	pttsCmd->stImageFeature.usYBayerEnd              = pttsStatus->stCapabilities.stImageFeature.usBayerHeight - 1;
	pttsCmd->stImageFeature.usXBayerStart            = 0;
	pttsCmd->stImageFeature.usYBayerStart            = 0;
	pttsCmd->stImageFeature.ucHorDownsamplingFactor  = 1;
	pttsCmd->stImageFeature.ucVertDownsamplingFactor = 1;

	pttsCmd->stShootingParam.usAGain[0]            =
	pttsCmd->stShootingParam.usAGain[1]            =
	pttsCmd->stShootingParam.usAGain[2]            =
	pttsCmd->stShootingParam.usAGain[3]            = DEFAULT_ANALOG_GAIN;

	pttsCmd->stShootingParam.usDGain[0]            =
	pttsCmd->stShootingParam.usDGain[1]            =
	pttsCmd->stShootingParam.usDGain[2]            =
	pttsCmd->stShootingParam.usDGain[3]            = DEFAULT_DIGITAL_GAIN;

	pttsCmd->stImageFeature.usMinVertBlanking      = pttsStatus->stCapabilities.stImageFeature.usMinVertBlanking;
	pttsCmd->stImageFeature.usMinHorBlanking       = pttsStatus->stCapabilities.stImageFeature.usMinHorBlanking;

	pttsCmd->stShootingParam.uiExpoTime            = DEFAULT_EXPOSURE_TIME;
	pttsCmd->stImageFeature.usFrameRate            = DEFAULT_FRAMERATE;
	pttsCmd->usAfPosition                          = MIN_AF_POSITION;
}


#define WRITE_DEVICE4(read,write0,write1,write2,write3)    \
	{	uint8_t ucData;                                    \
		ucData = readDevice(read, 1);                      \
		writeDevice(write0, ucData, 1);                    \
		writeDevice(write1, ucData, 1);                    \
		writeDevice(write2, ucData, 1);                    \
		writeDevice(write3, ucData, 1); }                  \


#define ov8820_write(a,b) writeDevice((a),(b),1)

static void  setStaticSensorConfiguration ( int nbLanes ) {
	ov8820_write(0x0103, 0x01);  //SOFT_RST
#ifdef __ARM__
	    usleep(100000);
#endif
	ov8820_write(0x3000, 0x02); //Vsync output enable
	ov8820_write(0x3001, 0x00);	//not_used
	ov8820_write(0x3002, 0x6c);	//SCCB_ID
	ov8820_write(0x3003, 0xce);	//PLL_CTRL0
	if(nbLanes == 2) {
		ov8820_write(0x3004, 0xce);	//PLL_CTRL1
		ov8820_write(0x3005, 0x00);	//PLL_CTRL2
	}
	else{
		ov8820_write(0x3004, 0xbf);	//PLL_CTRL1
		ov8820_write(0x3005, 0x10);	//PLL_CTRL2
	}
	ov8820_write(0x3006, 0x00);	//PLL_CTRL3
	ov8820_write(0x3007, 0x3b);	//PLL_CTRL4
	ov8820_write(0x300d, 0x00);	//Output value (Shutter, Vsync Frex, Strobe)
	ov8820_write(0x3010, 0x00);	//io_selection
	if(nbLanes == 2) {
		ov8820_write(0x3011, 0x01);	//PLL_CTRL_S2 -> 1:2lanes 2:4lanes
		ov8820_write(0x3012, 0x00);	//PLL_CTRL_S1
	}
	else {
		ov8820_write(0x3011, 0x02);	//PLL_CTRL_S2 -> 1:2lanes 2:4lanes
		ov8820_write(0x3012, 0x80);	//PLL_CTRL_S1
	}
	ov8820_write(0x3013, 0x39);	//PLL_CTRL_S0
	ov8820_write(0x3018, 0x00);	//MIPI_SC_CTRL
	ov8820_write(0x3104, 0x20);	//SCCB_PLL
	/*ov8820_write(0x3200, 0x00);
	ov8820_write(0x3201, 0x80);
	ov8820_write(0x3204, 0x80);
	ov8820_write(0x3205, 0x80);  */
	//ov8820_write(0x3209, 0x00);     //Grp 0 lenght
	//ov8820_write(0x320a, 0x00);     //Grp 1 lenght
	WRITE_DEVICE(Offset_group_switch, 5);
	ov8820_write(0x3300, 0x00);	// ??
	ov8820_write(0x3503, 0x07); //Gain has no delay, manual VTS, AGC, AEC
	// ov8820_write(0x3509, 0x1f);
#ifdef USE_SENSOR_GAIN
	ov8820_write(0x3509, 0x00); //Analog gain mode 0x00(sensor gain) or 0x10(real gain) => use sensor gain 0x350A 0x350B
#else
	ov8820_write(0x3509, 0x10);
#endif
	ov8820_write(0x3600, 0x05); //0x08 Analog ctrl register ??
	ov8820_write(0x3601, 0x32); //0x44 Analog ctrl register ??
	ov8820_write(0x3602, 0x44); //0x75 Analog ctrl register ??
	ov8820_write(0x3603, 0x5c); //0x1c Analog ctrl register ??
	ov8820_write(0x3604, 0x98); //Analog ctrl register ??
	ov8820_write(0x3605, 0xe9); //Analog ctrl register ??
	ov8820_write(0x3609, 0xb8); //Analog ctrl register ??
	ov8820_write(0x360a, 0xbc); //Analog ctrl register ??
	ov8820_write(0x360b, 0xb4); //0xb8 Analog ctrl register ??
	ov8820_write(0x360c, 0x0d); //0x0c Analog ctrl register ??
	ov8820_write(0x3612, 0x00); //0x03 Analog ctrl register ??
	ov8820_write(0x3613, 0x02); //Analog ctrl register ??
	ov8820_write(0x3614, 0x0f); //Analog ctrl register ??
	ov8820_write(0x3615, 0x00); //Analog ctrl register ??
	ov8820_write(0x3616, 0x03); //Analog ctrl register ??
	ov8820_write(0x3617, 0xa1); //0x01 Analog ctrl register ??
	ov8820_write(0x361a, 0x46); //VCM_MID1 (AF)
	ov8820_write(0x361b, 0x05); //VCM_MID2 (AF)
	ov8820_write(0x361e, 0x01); //VSYNC ctrl
	ov8820_write(0x3700, 0x20); //SENSOR_CTRL0 ??
	ov8820_write(0x3701, 0x44); //SENSOR_CTRL1 ??
	ov8820_write(0x3702, 0x70); //0x50 SENSOR_CTRL2 ??
	ov8820_write(0x3703, 0x4f); //0xcc SENSOR_CTRL3 ??
	ov8820_write(0x3704, 0x69); //0x19 SENSOR_CTRL4 ??
	ov8820_write(0x3706, 0x7b); //0x4b SENSOR_CTRL6 ??
	ov8820_write(0x3707, 0x63); //SENSOR_CTRL7 ??
	ov8820_write(0x3708, 0x85); //0x84 SENSOR_CTRL8 ??
	ov8820_write(0x3709, 0x40); //SENSOR_CTRL9 ??
	ov8820_write(0x370a, 0x12); //SENSOR_CTRLA ??
	ov8820_write(0x370b, 0x01); //SENSOR_CTRLB ??
	ov8820_write(0x370c, 0x50); //SENSOR_CTRLC ??
	ov8820_write(0x370d, 0x0c); //SENSOR_CTRLD ??
	ov8820_write(0x370e, 0x00); //0x00 SENSOR_CTRLE ??
	ov8820_write(0x3711, 0x01); //SENSOR_CTRL11 ??
	ov8820_write(0x3712, 0xcc); //0x9c SENSOR_CTRL12 ??
	ov8820_write(0x3810, 0x00); //Horizontal offset [12:8] for scaler
	ov8820_write(0x3811, HORIZONTAL_OFFSET); //Horizontal offset [ 7:0] for scaler
	ov8820_write(0x3812, 0x00); //Vertical offset   [12:8] for scaler
	ov8820_write(0x3813, VERTICAL_OFFSET); //Vertical offset   [ 7:0] for scaler

	WRITE_BY_CACHE(Offset_x_odd_even_inc, 0x11);
	WRITE_BY_CACHE(Offset_y_odd_even_inc, 0x11);

	ov8820_write(0x3816, 0x02); //Hsync_start [15:8]
	ov8820_write(0x3817, 0x40); //Hsync_start [7:0]
	ov8820_write(0x3818, 0x00); //Hsync_end [15:8]
	ov8820_write(0x3819, 0x40); //Hsync_end [7:0]
	WRITE_BY_CACHE(Offset_timing_tc_reg20, 0x00); //Flip   OFF
	WRITE_BY_CACHE(Offset_timing_tc_reg21, 0x00); //0x16 Mirror ON

	////////////////////////////////////////RESET OTP
	ov8820_write(0x3d00, 0x00);
	ov8820_write(0x3d01, 0x00);
	ov8820_write(0x3d02, 0x00);
	ov8820_write(0x3d03, 0x00);
	ov8820_write(0x3d04, 0x00);
	ov8820_write(0x3d05, 0x00);
	ov8820_write(0x3d06, 0x00);
	ov8820_write(0x3d07, 0x00);
	ov8820_write(0x3d08, 0x00);
	ov8820_write(0x3d09, 0x00);
	ov8820_write(0x3d0a, 0x00);
	ov8820_write(0x3d0b, 0x00);
	ov8820_write(0x3d0c, 0x00);
	ov8820_write(0x3d0d, 0x00);
	ov8820_write(0x3d0e, 0x00);
	ov8820_write(0x3d0f, 0x00);
	ov8820_write(0x3d10, 0x00);
	ov8820_write(0x3d11, 0x00);
	ov8820_write(0x3d12, 0x00);
	ov8820_write(0x3d13, 0x00);
	ov8820_write(0x3d14, 0x00);
	ov8820_write(0x3d15, 0x00);
	ov8820_write(0x3d16, 0x00);
	ov8820_write(0x3d17, 0x00);
	ov8820_write(0x3d18, 0x00);
	ov8820_write(0x3d19, 0x00);
	ov8820_write(0x3d1a, 0x00);
	ov8820_write(0x3d1b, 0x00);
	ov8820_write(0x3d1c, 0x00);
	ov8820_write(0x3d1d, 0x00);
	ov8820_write(0x3d1e, 0x00);
	ov8820_write(0x3d1f, 0x00);
	//////////////////////////////////////////END OF OTP RESET
	ov8820_write(0x3d80, 0x00); //program OTP Bit[0]
	ov8820_write(0x3d81, 0x00); //load/dump OTP Bit[0]
	ov8820_write(0x3d84, 0x00); //Bit[3]: bank mode enable, Bit[2:0] bank index
	ov8820_write(0x3f01, 0xfc); //PSRAM_CTRL1 ??
	ov8820_write(0x3f05, 0x10); //PSRAM_CTRL5 ??
	ov8820_write(0x3f06, 0x00); //PSRAM_CTRL6 ??
	ov8820_write(0x3f07, 0x00); //PSRAM_CTRL7 ??
	ov8820_write(0x4000, 0x29); //BLC CTRL0 (Enabled - Set 0x61 to BLC Bypass)
	ov8820_write(0x4001, 0x02); //BLC Start line
	ov8820_write(0x4002, 0xc5); //BLC CTRL2
	ov8820_write(0x4003, 0x08); //BLC CTRL3
	ov8820_write(0x4004, 0x04); //BLC CTRL4
	ov8820_write(0x4005, 0x1a); //BLC CTRL5
	ov8820_write(0x4008, 0x00); // Black level target
	ov8820_write(0x4009, 0x40); // Black target level
	ov8820_write(0x4300, 0xff); //Max clip value [9:2]
	//ov8820_write(0x4301, 0x00); //Min clip value [9:2]
	//ov8820_write(0x4302, 0x0c); //Max clip value [3:2] / Min clip value [1:0]
	ov8820_write(0x4303, 0x00); //FMT_CTRL3
	ov8820_write(0x4304, 0x08); //FMT_CTRL4
	ov8820_write(0x4307, 0x00); //FMT CTRL: Embedded ctrl
	ov8820_write(0x4600, 0x24); //VFIFO_CTRL0
	ov8820_write(0x4601, 0x01); //VFIFO_READ_ST_HIGH
	ov8820_write(0x4602, 0x00); //VFIFO_READ_ST_LOW
	ov8820_write(0x4800, 0x04); //MIPI_CTRL0
	ov8820_write(0x4801, 0x0f); //MIPI_CTRL1
	ov8820_write(0x4837, 0x2c); //MIPI_PCLK_PREIOD
	ov8820_write(0x4843, 0x02);
	ov8820_write(0x5000, 0x06); //ISP_CTRL0 (WhitePixel/BlackPixel Enable - LENC Disable)
	ov8820_write(0x5001, 0x01); //Manual white balance enable
	ov8820_write(0x5002, 0x00); //Not used
	ov8820_write(0x501f, 0x00); //Bypass ISP disable
	ov8820_write(0x5c00, 0x80); //Pre BLC CTRL0 ??
	ov8820_write(0x5c01, 0x00); //Pre BLC CTRL1 ??
	ov8820_write(0x5c02, 0x00); //Pre BLC CTRL2 ??
	ov8820_write(0x5c03, 0x00); //Pre BLC CTRL3 ??
	ov8820_write(0x5c04, 0x00); //Pre BLC CTRL4 ??
	ov8820_write(0x5c05, 0x00); //Pre BLC CTRL5 ??
	ov8820_write(0x5c06, 0x00); //Pre BLC CTRL6 ??
	ov8820_write(0x5c07, 0x80); //Pre BLC CTRL7 ??
	ov8820_write(0x5c08, 0x10); //Pre BLC CTRL7 ??
	ov8820_write(0x5e00, 0x00);   //Test pattern off
	//ov8820_write(0x5e00, 0x80); //Test pattern on
	ov8820_write(0x6700, 0x05); //TPM CTRL register
	ov8820_write(0x6701, 0x19); //TPM CTRL register
	ov8820_write(0x6702, 0xfd); //TPM CTRL register
	ov8820_write(0x6703, 0xd7); //TPM CTRL register
	ov8820_write(0x6704, 0xff); //TPM CTRL register
	ov8820_write(0x6705, 0xff); //TPM CTRL register
	ov8820_write(0x6800, 0x10); // ??
	ov8820_write(0x6801, 0x02); // ??
	ov8820_write(0x6802, 0x90); // ??
	ov8820_write(0x6803, 0x10); // ??
	ov8820_write(0x6804, 0x59); // ??
	ov8820_write(0x6900, 0x61); //CADC CTRL0
	ov8820_write(0x6901, 0x05); //CADC CTRL1
	//ov8820_write(0x0100, 0x01); //start streaming
	WRITE_BY_CACHE( Offset_mode_select, 0x00 );
	/////////////////////////////LENC calibration
	ov8820_write(0x5800, 0x08);
	ov8820_write(0x5801, 0x03);
	ov8820_write(0x5802, 0x03);
	ov8820_write(0x5803, 0x03);
	ov8820_write(0x5804, 0x05);
	ov8820_write(0x5805, 0x0f);
	ov8820_write(0x5806, 0x01);
	ov8820_write(0x5807, 0x02);
	ov8820_write(0x5808, 0x01);
	ov8820_write(0x5809, 0x01);
	ov8820_write(0x580a, 0x02);
	ov8820_write(0x580b, 0x04);
	ov8820_write(0x580c, 0x03);
	ov8820_write(0x580d, 0x01);
	ov8820_write(0x580e, 0x00);
	ov8820_write(0x580f, 0x00);
	ov8820_write(0x5810, 0x02);
	ov8820_write(0x5811, 0x05);
	ov8820_write(0x5812, 0x04);
	ov8820_write(0x5813, 0x03);
	ov8820_write(0x5814, 0x01);
	ov8820_write(0x5815, 0x02);
	ov8820_write(0x5816, 0x05);
	ov8820_write(0x5817, 0x09);
	ov8820_write(0x5818, 0x07);
	ov8820_write(0x5819, 0x09);
	ov8820_write(0x581a, 0x07);
	ov8820_write(0x581b, 0x08);
	ov8820_write(0x581c, 0x0a);
	ov8820_write(0x581d, 0x0d);
	ov8820_write(0x581e, 0x18);
	ov8820_write(0x581f, 0x10);
	ov8820_write(0x5820, 0x10);
	ov8820_write(0x5821, 0x10);
	ov8820_write(0x5822, 0x13);
	ov8820_write(0x5823, 0x23);
	ov8820_write(0x5824, 0x62);
	ov8820_write(0x5825, 0x42);
	ov8820_write(0x5826, 0x44);
	ov8820_write(0x5827, 0x42);
	ov8820_write(0x5828, 0x60);
	ov8820_write(0x5829, 0x44);
	ov8820_write(0x582a, 0x42);
	ov8820_write(0x582b, 0x42);
	ov8820_write(0x582c, 0x42);
	ov8820_write(0x582d, 0x26);
	ov8820_write(0x582e, 0x24);
	ov8820_write(0x582f, 0x40);
	ov8820_write(0x5830, 0x42);
	ov8820_write(0x5831, 0x44);
	ov8820_write(0x5832, 0x04);
	ov8820_write(0x5833, 0x26);
	ov8820_write(0x5834, 0x22);
	ov8820_write(0x5835, 0x02);
	ov8820_write(0x5836, 0x04);
	ov8820_write(0x5837, 0x2a);
	ov8820_write(0x5838, 0x22);
	ov8820_write(0x5839, 0x24);
	ov8820_write(0x583a, 0x28);
	ov8820_write(0x583b, 0x28);
	ov8820_write(0x583c, 0x42);
	ov8820_write(0x583d, 0xce);
	/////////////////////////////End of LENC calibration

#ifdef FORCE_FULL_FRAME
	ov8820_write(0x3800, 0x00);
	ov8820_write(0x3801, 0x00);
	ov8820_write(0x3802, 0x00);
	ov8820_write(0x3803, 0x00);
	ov8820_write(0x3804, 0x0c);
	ov8820_write(0x3805, 0xdf);
	ov8820_write(0x3806, 0x09);
	ov8820_write(0x3807, 0x9b);
	ov8820_write(0x3808, 0x0c);
	ov8820_write(0x3809, 0xc0);
	ov8820_write(0x380a, 0x09);
	ov8820_write(0x380b, 0x90);
	//ov8820_write(0x380c, 0x10);
	//ov8820_write(0x380d, 0x00);
	//ov8820_write(0x380e, 0x0e);
	//ov8820_write(0x380f, 0xb0);
	ov8820_write(0x3810, 0x00);
	ov8820_write(0x3811, 0x10);
	ov8820_write(0x3812, 0x00);
	ov8820_write(0x3813, 0x06);
	S_stRegister.usLineLength = 0x1000;
	WRITE_BY_CACHE( Offset_line_length_pck          ,              S_stRegister.usLineLength );
	S_stRegister.usFrameLength = 0x0eb0;
	WRITE_BY_CACHE( Offset_frame_length_lines       ,              S_stRegister.usFrameLength );
#endif

// 	    DEBUGMSG(TTC_DBG_LEVEL,("End of Init Seq\n"));
}

static uint8_t ucGrpIdx = 0;
static uint8_t isNewGrpCreated = 0;

static inline void openSensorGroup(void)
{
	if(   S_stRegister.uiExpoTime              != (uint32_t)(READ_BY_CACHE(Offset_ExposureMSB) << 16) + READ_BY_CACHE(Offset_Exposure)
	   || S_stRegister.usAG                    != READ_BY_CACHE(Offset_AG   )
	   || S_stRegister.usDG[DxOISP_CHANNEL_GR] != READ_BY_CACHE(Offset_DG_G )
	   || S_stRegister.usDG[DxOISP_CHANNEL_R]  != READ_BY_CACHE(Offset_DG_R )
	   || S_stRegister.usDG[DxOISP_CHANNEL_B]  != READ_BY_CACHE(Offset_DG_B ))
	{
		WRITE_DEVICE(Offset_group_access, ucGrpIdx);
		isNewGrpCreated = 1;
	}
	else {
		isNewGrpCreated = 0;
	}
}

static inline void closeSensorGroup(void)
{
	if(isNewGrpCreated) {
		WRITE_DEVICE(Offset_group_access, (0x10 + ucGrpIdx));
		WRITE_DEVICE(Offset_group_access, (0xa0 + ucGrpIdx));
		ucGrpIdx = !ucGrpIdx;
		isNewGrpCreated = 0;
	}
}

void applySettingsToSensor( void )
{
	uint8_t ucMode;

	WRITE_BY_CACHE( Offset_Af                      ,                         S_stRegister.usAF);

	//openSensorGroup();
#ifndef FORCE_FULL_FRAME
	WRITE_BY_CACHE( Offset_x_addr_start             ,                   S_stRegister.usXstart );
	WRITE_BY_CACHE( Offset_y_addr_start             ,                   S_stRegister.usYstart );
	WRITE_BY_CACHE( Offset_x_addr_end               ,                    S_stRegister.usXstop );
	WRITE_BY_CACHE( Offset_y_addr_end               ,                    S_stRegister.usYstop );
	WRITE_BY_CACHE( Offset_x_odd_even_inc           ,                     S_stRegister.ucIncX );
	WRITE_BY_CACHE( Offset_y_odd_even_inc           ,                     S_stRegister.ucIncY );
	WRITE_BY_CACHE( Offset_x_output_size            ,                 S_stRegister.usOutSizeX );
	WRITE_BY_CACHE( Offset_y_output_size            ,                 S_stRegister.usOutSizeY );
	WRITE_BY_CACHE( Offset_line_length_pck          ,               S_stRegister.usLineLength );
	WRITE_BY_CACHE( Offset_frame_length_lines       ,              S_stRegister.usFrameLength );
#endif
	WRITE_BY_CACHE( Offset_timing_tc_reg21          ,               S_stRegister.ucTimingRegX );
	WRITE_BY_CACHE( Offset_timing_tc_reg20          ,               S_stRegister.ucTimingRegY );
	WRITE_BY_CACHE( Offset_magic_bin_reg            ,               S_stRegister.ucMagicBinReg);
	WRITE_BY_CACHE( Offset_psram_ctrl0              ,               S_stRegister.ucPsRamCtrl0 );

	openSensorGroup();
	WRITE_BY_CACHE( Offset_ExposureMSB              ,            (S_stRegister.uiExpoTime>>16));
	WRITE_BY_CACHE( Offset_Exposure                 ,                 S_stRegister.uiExpoTime );
	WRITE_BY_CACHE( Offset_AG                       ,                       S_stRegister.usAG );
	WRITE_BY_CACHE( Offset_DG_G                     ,    S_stRegister.usDG[DxOISP_CHANNEL_GR] );
	WRITE_BY_CACHE( Offset_DG_R                     ,     S_stRegister.usDG[DxOISP_CHANNEL_R] );
	WRITE_BY_CACHE( Offset_DG_B                     ,     S_stRegister.usDG[DxOISP_CHANNEL_B] );
	closeSensorGroup();

	ucMode = READ_BY_CACHE(Offset_mode_select);
	if(!ucMode && S_stRegister.isStreaming) {
		S_stStatus.isSensorFireNeeded = 1;
#ifdef __ARM__
		//BU_REG_WRITE( CCIC_CSI2_DPHY5, 0x00);
		DeviceStop();
#endif
	}else {
		if (!S_stRegister.isStreaming && ucMode) {
			//Reset this register to its default before to stop the sensor
			WRITE_BY_CACHE( Offset_timing_tc_reg21, 0x16);
		}

		WRITE_BY_CACHE( Offset_mode_select      ,                    S_stRegister.isStreaming );
	}

	historyMngt(ucMode);


	//DEBUGMSG(TTC_DBG_LEVEL,("LL:%d FL:%d\n", S_stRegister.usLineLength, S_stRegister.usFrameLength));

}

static void initCache ( void ) {
	uint32_t i;
	for ( i = 0; i < SIZE_CACHE_DATA; i++ ) {
		S_cache[i].iLastAccess = NO_ACCESS;
	}
}

static void historyMngt(uint8_t ucMode)
{
	static uint16_t S_usOldOutputSizeX, S_usOldOutputSizeY;

	if(ucMode == 1 && (S_stRegister.usOutSizeX != S_usOldOutputSizeX || S_stRegister.usOutSizeY != S_usOldOutputSizeY)) {
		S_stStatus.isFrameInvalid = 1;
	}
	else if(S_stStatus.isSensorFireNeeded) {
		S_stStatus.isFrameInvalid = 1;
	}
	else {
		S_stStatus.isFrameInvalid = 0;
	}
	S_usOldOutputSizeX = S_stRegister.usOutSizeX;
	S_usOldOutputSizeY = S_stRegister.usOutSizeY;
}

static void getFrameValidTime ( ts_SENSOR_Status* pttsStatus ) {
	uint16_t usSizeY         = READ_BY_CACHE( Offset_y_output_size );
	uint16_t usLineLengthPck = READ_BY_CACHE( Offset_line_length_pck );
	//version 64bits: does not work with armcc
	//pttsStatus->uiFrameValidTime =  ((uint64_t)(usSizeY * usLineLengthPck) << 32) / ((uint64_t)(pttsStatus->uiPixelClock*1000));
	//version 32bits:
	pttsStatus->uiFrameValidTime = ((((usSizeY * usLineLengthPck <<4) / 1000) << 8 ) / (pttsStatus->uiPixelClock>>12)) << 8;
}

static void getEstimatedFocus  ( ts_SENSOR_Status* pttsStatus ) {
	pttsStatus->ucUserStatus[0] = DxOISP_DAF_FOCUS_PORTRAIT;
}

/*****************************************************************************/
/* Exported functions definition                                             */
/*****************************************************************************/

void DxOSensor_OVT8820_Initialize ( void ) {
	DeviceInit("/dev/video0", 1);

//	I2C_SetSpeed( CLOCK_DIV_VALUE );
#if defined(CHECK_SENSOR_BEFORE_START)
	autoCheckPluggedSensor();
#endif

	initCache();
	setStaticSensorConfiguration ( NB_MIPI_LANES);
	getCapabilities    ( &S_stStatus                                    );
	S_uiSensorPixelClock = computePixelClock();

	initSensorCmd      ( &S_stCmd, &S_stStatus                          );
	setCrop            ( );
	setOrientation     ( S_stCmd.stImageFeature.eOrientation            );
	setDecimation      ( S_stCmd.stImageFeature.ucHorDownsamplingFactor,
	                     S_stCmd.stImageFeature.ucVertDownsamplingFactor);
	setGain            ( S_stCmd.stShootingParam.usAGain,
	                     S_stCmd.stShootingParam.usDGain);
#ifndef FORCE_FULL_FRAME
	setMinimalBlanking ( S_stCmd.stImageFeature.usMinVertBlanking,
	                     S_stCmd.stImageFeature.usMinHorBlanking        );
#endif
	setExposureTime    ( );
	setAF              ( );
	applySettingsToSensor();

	//DEBUGMSG(TTC_DBG_LEVEL,("REVID=%x\n", readDevice(0x302A, 1)));
}

void DxOSensor_OVT8820_Uninitialize ( void ) {
	DeviceDeinit();

	return;
}

void DxOSensor_OVT8820_RawDump ( const char *fname ) {
	PRINTF("image size W:%d H:%d\n", S_stRegister.usOutSizeX, S_stRegister.usOutSizeY);

	DeviceRawDump(fname, S_stRegister.usOutSizeX, S_stRegister.usOutSizeY);

	return;
}

void DxOSensor_OVT8820_SetStartGrp ( void ) {
	return;
}


void DxOSensor_OVT8820_Set ( uint16_t usOffset, uint16_t usSize, void* pBuf )
{
	uint16_t usTmpSize = usSize;
	if ( (usOffset + usSize)  > sizeof(S_stCmd)) {
		assertf1(( (usOffset + usSize)  <= sizeof(S_stCmd)), " uncorrect offset (%d)", (usOffset + usSize) );
		usTmpSize = sizeof(S_stCmd) - usOffset;
	}
	memcpy( (uint8_t*)&S_stCmd + usOffset, pBuf, usTmpSize );
}

void DxOSensor_OVT8820_SetEndGrp(void)
{
	setMode              ( S_stCmd.isSensorStreaming                      );
	setCrop              ();
	setOrientation       ( S_stCmd.stImageFeature.eOrientation            );
	setDecimation        ( S_stCmd.stImageFeature.ucHorDownsamplingFactor,
	                       S_stCmd.stImageFeature.ucVertDownsamplingFactor);
	setGain              ( S_stCmd.stShootingParam.usAGain,
	                       S_stCmd.stShootingParam.usDGain);
#ifndef FORCE_FULL_FRAME
	setMinimalBlanking   ( S_stCmd.stImageFeature.usMinVertBlanking,
	                       S_stCmd.stImageFeature.usMinHorBlanking        );
#endif
	setExposureTime      ();

	if(S_stRegister.usFrameLength&0x1)
		S_stRegister.usFrameLength++;

	setAF                ();
//	DEBUGMSG(TTC_DBG_LEVEL,("CMD: XE=%d YE:%d\n", S_stCmd.stImageFeature.usXBayerEnd, S_stCmd.stImageFeature.usYBayerEnd));

	applySettingsToSensor();
}

void DxOSensor_OVT8820_Get ( uint16_t usOffset, uint16_t usSize, void* pBuf ) {
	uint16_t usTmpSize = usSize;
	if ( (usOffset + usSize)  > sizeof(S_stStatus)) {
		assertf1(( (usOffset + usSize) <= sizeof(S_stStatus)), " uncorrect offset (%d)", (usOffset + usSize) );
		usTmpSize = sizeof(S_stStatus) -usOffset;
	}
	memcpy(pBuf,(uint8_t *)&S_stStatus+usOffset,usTmpSize);
}

void DxOSensor_OVT8820_GetEndGrp ( void ) {
	return;
}

void DxOSensor_OVT8820_GetStartGrp ( void )
{
// 	cprintf("OUTPUT: Again=%d Dgain=%d ET:%d\n", S_stStatus.stAppliedCmd.stShootingParam.usAGain[0], S_stStatus.stAppliedCmd.stShootingParam.usDGain[0], S_stStatus.stAppliedCmd.stShootingParam.uiExpoTime);
	S_stStatus.isImageFeatureStable = 1;
	S_stStatus.isImageQualityStable = 1;
	S_stStatus.uiPixelClock         = S_uiSensorPixelClock<<12;

	getMode            ( &S_stStatus );
	getImageFeature    ( &S_stStatus );
	getFrameRate       ( &S_stStatus );
	getExposureTime    ( &S_stStatus );
	getAnalogGain      ( &S_stStatus );
	getDigitalGain     ( &S_stStatus );
	getPixelClock      ( &S_stStatus );
	getFrameValidTime  ( &S_stStatus );
	getEstimatedFocus  ( &S_stStatus );
	getAF              ( &S_stStatus );
}

void DxOSensor_OVT8820_Fire ( void )
{
#ifdef __ARM__
	DeviceStart();
#endif

	WRITE_BY_CACHE( Offset_mode_select, S_stRegister.isStreaming );
	S_stStatus.isSensorFireNeeded = 0;
}

void MrvlSensor_OVT8820_GetAttr ( ts_SENSOR_Attr *pstSensorAttr )
{
	pstSensorAttr->uiStillSkipFrameNum = 2;
}
