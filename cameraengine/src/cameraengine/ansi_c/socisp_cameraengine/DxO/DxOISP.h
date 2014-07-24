// ============================================================================
// DxO Labs proprietary and confidential information
// Copyright (C) DxO Labs 1999-2011 - (All rights reserved)
// ============================================================================

#ifndef __DXO_ISP_H__
#define __DXO_ISP_H__

#include <stdint.h>

#ifndef DxOISP_PRIVATE_STAT_SIZE
	#define DxOISP_PRIVATE_STAT_SIZE    0
#endif

#define DxOISP_USER_EXTENSION_SIZE        100

/******************************************************/
/*                   DEFINITION                       */
/******************************************************/

typedef enum {
	DxOISP_MODE_STANDBY = 0,
	DxOISP_MODE_IDLE,
	DxOISP_MODE_PREVIEW,
	DxOISP_MODE_PRECAPTURE,
	DxOISP_MODE_CAPTURE_A,
	DxOISP_MODE_CAPTURE_B,
	DxOISP_MODE_VIDEOREC,
	DxOISP_NB_MODE
}te_DxOISPMode;

#define DxOISP_NB_MAX_CAPTURE             10

typedef enum {
	DxOISP_TEST_PATTERN_VERTICAL_COLOR = 0,
	DxOISP_TEST_PATTERN_DIAGONAL_GRADIENT,
	DxOISP_TEST_PATTERN_CROSS_HAIR,
	DxOISP_NB_TEST_PATTERN
}te_DxOISPTestPattern;

typedef enum {
	DxOISP_CHANNEL_GR = 0,
	DxOISP_CHANNEL_R,
	DxOISP_CHANNEL_B,
	DxOISP_CHANNEL_GB,
	DxOISP_NB_CHANNEL
}te_DxOISPChannel;

typedef enum {
	DxOISP_CAPTURE_SIMPLE = 0,
	DxOISP_CAPTURE_BURST,            //plain multiband capture if ucNbCaptureFrame = 1
	DxOISP_CAPTURE_ZERO_SHUTTER_LAG,
	DxOISP_CAPTURE_HDR,
	DxOISP_NB_CAPTURE_MODE
} te_DxOISPSubModeCapture;

typedef enum {
	DxOISP_VR_SIMPLE = 0,
	DxOISP_VR_STABILIZED = 0x1,
	DxOISP_VR_TEMPORAL_NOISE = 0x2,
	DxOISP_VR_STABILIZED_TEMPORAL_NOISE = DxOISP_VR_STABILIZED | DxOISP_VR_TEMPORAL_NOISE,
	DxOISP_NB_VR_MODE = 0x4
}te_DxOISPSubModeVideoRec;

#define DxOISP_FPS_FRAC_PART      4

typedef enum {
	DxOISP_FLIP_OFF_MIRROR_OFF = 0,
	DxOISP_FLIP_OFF_MIRROR_ON,
	DxOISP_FLIP_ON_MIRROR_OFF,
	DxOISP_FLIP_ON_MIRROR_ON
}te_DxOISPImageOrientation;

#define DxOISP_UPSCALING_TOLERANCE_FRAC_PART   5

typedef enum {
	DxOISP_FORMAT_YUV422 = 0,
	DxOISP_FORMAT_YUV420,
	DxOISP_FORMAT_RGB565,
	DxOISP_FORMAT_RGB888,
	DxOISP_FORMAT_RAW,
	DxOISP_NB_FORMAT
}te_DxOISPFormat;

typedef enum {
	DxOISP_RGBORDER_RGB,
	DxOISP_RGBORDER_RBG,
	DxOISP_RGBORDER_GRB,
	DxOISP_RGBORDER_GBR,
	DxOISP_RGBORDER_BGR,
	DxOISP_RGBORDER_BRG,
	DxOISP_NB_RGBORDER
}te_DxOISPRgbOrder;

typedef enum {
	DxOISP_YUV422ORDER_YUYV,
	DxOISP_YUV422ORDER_YYUV,
	DxOISP_YUV422ORDER_UYYV,
	DxOISP_YUV422ORDER_UVYY,
	DxOISP_YUV422ORDER_UYVY,
	DxOISP_YUV422ORDER_YUVY,
	DxOISP_YUV422ORDER_YVYU,
	DxOISP_YUV422ORDER_YYVU,
	DxOISP_YUV422ORDER_VYYU,
	DxOISP_YUV422ORDER_VUYY,
	DxOISP_YUV422ORDER_VYUY,
	DxOISP_YUV422ORDER_YVUY,
	DxOISP_NB_YUV422ORDER
}te_DxOISPYuv422Order;

typedef enum {
	DxOISP_YUV420ORDER_YYU,
	DxOISP_YUV420ORDER_YUY,
	DxOISP_YUV420ORDER_UYY,
	DxOISP_NB_YUV420ORDER
}te_DxOISPYuv420Order;

typedef enum {
	DxOISP_ENCODING_YUV_601FS = 0,
	DxOISP_ENCODING_YUV_601FU,
	DxOISP_ENCODING_YUV_601U,
	DxOISP_ENCODING_YUV_601S,
	DxOISP_NB_ENCODING
}te_DxOISPEncoding;

typedef enum {
	DxOISP_SENSOR_PHASE_GRBG = 0,
	DxOISP_SENSOR_PHASE_RGGB,
	DxOISP_SENSOR_PHASE_BGGR,
	DxOISP_SENSOR_PHASE_GBRG
}te_DxOISPSensorPhase;

typedef enum {
	DxOISP_VISUAL_EFFECT_NATURAL = 0,
	DxOISP_VISUAL_EFFECT_SEPIA,
	DxOISP_VISUAL_EFFECT_SOLARIZE,
	DxOISP_VISUAL_EFFECT_BW
}te_DxOISPVisualEffect;


#define DxOISP_NB_FACES  6

typedef enum {
	DxOISP_AFK_MODE_AUTO = 0,
	DxOISP_AFK_MODE_MANUAL
}te_DxOISPAfkMode;

typedef enum {
	DxOISP_AFK_FREQ_0HZ,
	DxOISP_AFK_FREQ_50HZ,
	DxOISP_AFK_FREQ_60HZ
}te_DxOISPAfkFrequency;

typedef enum {
	DxOISP_AE_MODE_AUTO = 0,
	DxOISP_AE_MODE_MANUAL,
	DxOISP_AE_MODE_LOW_GAIN,
	DxOISP_AE_MODE_LOW_LIGHT,
	DxOISP_AE_MODE_ISO_PRIORITY,
	DxOISP_AE_MODE_SHUTTER_SPEED_PRIORITY,
	DxOISP_AE_MODE_APERTURE_PRIORITY
}te_DxOISPAeMode;

#define DxOISP_EVBIAS_FRAC_PART          5

#define DxOISP_EXPOSURETIME_FRAC_PART    16

#define DxOISP_APERTURE_FRAC_PART        8

#define DxOISP_GAIN_FRAC_PART            8

typedef enum {
	DxOISP_AWB_MODE_AUTO = 0,
	DxOISP_AWB_MODE_MANUAL,
	DxOISP_AWB_MODE_PRESET
}te_DxOISPAwbMode;

typedef enum {
	DxOISP_AWB_PRESET_HORIZON,
	DxOISP_AWB_PRESET_TUNGSTEN,
	DxOISP_AWB_PRESET_TL84,
	DxOISP_AWB_PRESET_COOLWHITE,
	DxOISP_AWB_PRESET_D50,
	DxOISP_AWB_PRESET_DAYLIGHT,
	DxOISP_AWB_PRESET_CLOUDY,
	DxOISP_AWB_PRESET_D65,
	DxOISP_AWB_PRESET_SHADE
}te_DxOISPAwbPreset;

#define DxOISP_WBSCALE_FRAC_PART         8

typedef enum {
	DxOISP_FLASH_MODE_AUTO = 0,
	DxOISP_FLASH_MODE_MANUAL
}te_DxOISPFlashMode;

#define DxOISP_FLASH_POWER_FRAC_PART      0

typedef enum {
	DxOISP_DAF_MODE_AUTO = 0,
	DxOISP_DAF_MODE_MANUAL
}te_DxOISPDafMode;

typedef enum {
	DxOISP_DAF_FOCUS_DOCUMENT,
	DxOISP_DAF_FOCUS_SUPERMACRO,
	DxOISP_DAF_FOCUS_MACRO,
	DxOISP_DAF_FOCUS_PORTRAIT,
	DxOISP_DAF_FOCUS_LANDSCAPE,
	DxOISP_NB_DAF_FOCUS
}te_DxOISPDafFocus;

typedef enum {
	DxOISP_AF_MODE_AUTO = 0,
	DxOISP_AF_MODE_MANUAL,
	DxOISP_AF_MODE_HYPERFOCAL
}te_DxOISPAfMode;

typedef enum {
	DxOISP_AF_OFF,
	DxOISP_AF_FOCUSING,
	DxOISP_AF_FOCUSED,
	DxOISP_AF_UNKNOWN
}te_DxOISPAfConvergence;

typedef enum {
	DxOISP_NO_ERROR    = 0,
	DxOISP_PIXEL_STUCK = 1,
	DxOISP_SHORT_FRAME = 2
}te_DxOISPError;

#define DxOISP_AF_POSITION_FRAC_PART            12
#define DxOISP_ZOOMFOCAL_FRAC_PART               8
#define DxOISP_SYSTEM_CLOCK_FRAC_PART           16
#define DxOISP_MIN_INTERFRAME_TIME_FRAC_PART     8
#define DxOISP_MAX_BANDWIDTH_FRAC_PART           4

#define DxOISP_USBUFR_SIZE_Y                    30
#define DxOISP_USBUFR_SIZE_X                    40
#define DxOISP_USBUFB_SIZE_Y                    30
#define DxOISP_USBUFB_SIZE_X                    40
#define DxOISP_USBUFG_SIZE_Y                    48
#define DxOISP_USBUFG_SIZE_X                    40
#define DxOISP_USBUFUNSATRATIO_SIZE_X           40
#define DxOISP_USBUFUNSATRATIO_SIZE_Y           30
#define DxOISP_UIHISTOMAXRGB_SIZE              256
#define DxOISP_USBUFDELTAG_SIZE_X               12
#define DxOISP_USBUFDELTAG_SIZE_Y                9

/******************************************************/
/*              ELEMENTARY STRUCTURE                  */
/******************************************************/

typedef struct {
	uint16_t usGreenR;      //expressed in 8.8
	uint16_t usRed;         //expressed in 8.8
	uint16_t usBlue;        //expressed in 8.8
	uint16_t usGreenB;      //expressed in 8.8
}ts_DxOISPChannelU16;

typedef struct {
	uint16_t  usXAddrStart;
	uint16_t  usYAddrStart;
	uint16_t  usXAddrEnd;
	uint16_t  usYAddrEnd;
}ts_DxOISPCrop;

typedef struct {
	uint8_t       eFormat;         //te_DxOISPFormat
	uint8_t       eOrder;          //te_DxOISPRgbOrder, te_DxOISPYuv422Order or te_DxOISPYuv420Order according to the eFormat field
	uint8_t       eEncoding;       //te_DxOISPEncoding
	ts_DxOISPCrop stCrop;
	uint16_t      usSizeX;
	uint16_t      usSizeY;
}ts_DxOISPOutput;

typedef enum {
	DxOISP_COARSE_GAIN_LOWLIGHT = 0,
	DxOISP_COARSE_GAIN_STANDARD,
	DxOISP_COARSE_GAIN_HIGHLIGHT,
	DxOISP_NB_COARSE_GAIN
} te_DxOISPCoarseGain ;


typedef enum {
	DxOISP_COARSE_ILLUMINANT_INDOOR = 0,
	DxOISP_COARSE_ILLUMINANT_OUTDOOR,
	DxOISP_COARSE_ILLUMINANT_FLASH,
	DxOISP_NB_COARSE_ILLUMINANT
} te_DxOISPCoarseIlluminant ;

typedef enum {
	DxOISP_FINE_ILLUMINANT_FLASH = 0,
	DxOISP_FINE_ILLUMINANT_HORIZON,
	DxOISP_FINE_ILLUMINANT_TUNGSTEN,
	DxOISP_FINE_ILLUMINANT_TL84,
	DxOISP_FINE_ILLUMINANT_COOLWHITE,
	DxOISP_FINE_ILLUMINANT_D50,
	DxOISP_FINE_ILLUMINANT_DAYLIGHT,
	DxOISP_FINE_ILLUMINANT_CLOUDY,
	DxOISP_FINE_ILLUMINANT_D65,
	DxOISP_FINE_ILLUMINANT_SHADE,
	DxOISP_NB_FINE_ILLUMINANT
} te_DxOISPfineIlluminant ;

typedef enum {
	DxOISP_DISTANCE_DOCUMENT = 0,
	DxOISP_DISTANCE_SUPERMACRO,
	DxOISP_DISTANCE_MACRO,
	DxOISP_DISTANCE_PORTRAIT,
	DxOISP_DISTANCE_LANDSCAPE,
	DxOISP_NB_DISTANCE
} te_DxOISPDistance ;

typedef enum {
	DxOISP_FINE_GAIN_ISORANGE1 = 0,
	DxOISP_FINE_GAIN_ISORANGE2,
	DxOISP_FINE_GAIN_ISORANGE3,
	DxOISP_FINE_GAIN_ISORANGE4,
	DxOISP_FINE_GAIN_ISORANGE5,
	DxOISP_NB_FINE_GAIN
}te_DxOISPFineGain;

typedef enum {
	DxOISP_EXPOSURE_LOW = 0,
	DxOISP_EXPOSURE_MEDIUM,
	DxOISP_EXPOSURE_HIGH,
	DxOISP_NB_EXPOSURE_TIME
} te_DxOISPExposureTime ;



typedef struct {
	uint32_t uiBase;
	uint32_t uiSize;
}ts_BufDescr;
/******************************************************/
/*                   SUB STRUCTURE                    */
/******************************************************/

///////////////////////////////////////ts_DxOIspSyncCmd
typedef struct {
	struct {
		uint8_t          eMode;                  //te_DxOISPMode
		uint8_t          isTestPatternEnabled;
		uint8_t          ucSourceSelection;      //Sensor selection or pattern selection (te_DxOISPTestPattern) depending on isTestPatternEnabled register
		uint8_t          ucNbCaptureFrame;       //Number of frames for burst and zero shutter lag capture mode
		uint8_t          eSubModeCapture;        //te_DxOISPSubModeCapture
		uint8_t          eSubModeVideoRec;       //te_DxOISPSubModeVideoRec
		uint8_t          eImageOrientation;      //te_DxOISPImageOrientation
		uint8_t          ucUpscalingTolerance;   //expressed in 3.5
		uint8_t          isDisplayDisabled;      //boolean
		uint8_t          isRawOutputEnabled;     //boolean
		uint16_t         usFrameRate;            //FPS expressed in 12.4
		uint16_t         usZoomFocal;            //ratio between focal distance and sensor diagonal size expressed in 8.8
		uint16_t         usFrameToFrameLimit;    //maximum movement of any frame corner relative to width/height expressed in 0.16 (0xFFFF means inifnity)
		uint16_t         usBandSize;             //output band width in case of burst or HDR capture
		ts_DxOISPOutput  stOutputDisplay;
		ts_DxOISPOutput  stOutputImage;
		struct {
			uint16_t     usX;                //for cross-hair test pattern
			uint16_t     usY;                //for cross-hair test pattern
		}stTestPatternAttribute;
	}stControl;

	struct {
		uint8_t          eVisualEffect;          //te_DxOISPVisualEffect
	}stImage;

	struct {
		struct {
			ts_DxOISPCrop stFaces[DxOISP_NB_FACES];
			uint8_t       ucWeights[DxOISP_NB_FACES];
		}stROI;                                                      //Region Of Interest
		struct {
			uint8_t      eMode;                                  //te_DxOISPAfkMode
			struct {
				uint8_t  eFrequency;                         //te_DxOISPAfkFrequency
			}stForced;
		}stAfk;
		struct {
			uint8_t      eMode;                                  //te_DxOISPAeMode
			struct {
				int8_t   cEvBias;
				uint32_t uiMaxExposureTime;                  //ms expressed in 16.16
			}stControl;
			struct {
				uint32_t uiExposureTime;                     //ms expressed in 16.16
				uint16_t usAperture;                         //f number expressed in 8.8
				uint16_t usIspGain;                          //expressed in 8.8
				uint16_t usAnalogGain [DxOISP_NB_CHANNEL];   //expressed in 8.8
				uint16_t usDigitalGain[DxOISP_NB_CHANNEL];   //expressed in 8.8
			}stForced;
		}stAe;
		struct {
			uint8_t      eMode;                                  //te_DxOISPAwbMode
			struct {
				uint8_t  ePreset;                            //te_DxOISPAwbPreset
				uint16_t usWbRedScale;                       //expressed in 8.8
				uint16_t usWbBlueScale;                      //expressed in 8.8
			}stForced;
		}stAwb;
		struct {
			uint8_t      eMode;                                  //te_DxOISPFlashMode
			struct {
				uint16_t usPower;                            //lux at 1m on optical axis expressed in 16.0 (0 = off)
			}stForced;
		}stFlash;
		struct {
			uint8_t      eMode;                                  //te_DxOISPDafMode
			struct {
				uint8_t  eFocus;                             //te_DxOISPDafFocus
			}stForced;
		}stDaf;
		struct {
			uint8_t      eMode;                                  //te_DxOISPAfMode
			struct {
				uint16_t usPosition;                         //dioptry, expressed in 4.12 (to be moved as us)
			}stForced;
		}stAf;
	}stCameraControl;

	uint8_t ucUserExtension[DxOISP_USER_EXTENSION_SIZE];
}ts_DxOIspSyncCmd;

///////////////////////////////////////ts_DxOIspAsyncCmd
typedef struct {
	struct {
		uint32_t   uiSystemClock ;       //Frequency of the DxOISP. (in MHz expressed in 16.16)
		uint32_t   uiMaxBandwidth;       //Maximum instantaneous bandwidth supported (in MBit/s expressed in 24.4)
		uint16_t   usMinInterframeTime ; //Interrupt reaction time + Execution time until start of frame. (in ms expressed in 8.8)
	} stSystem ;

	struct {
		uint8_t ucLensShading   [DxOISP_NB_COARSE_GAIN][DxOISP_NB_COARSE_ILLUMINANT] ;
		uint8_t ucColorFringing [DxOISP_NB_DISTANCE][DxOISP_NB_COARSE_ILLUMINANT] ;
		uint8_t ucSharpness     [DxOISP_NB_DISTANCE][DxOISP_NB_COARSE_ILLUMINANT] ;
	} stLensCorrection;

	struct {
		uint8_t ucBrightPixels     [DxOISP_NB_EXPOSURE_TIME] ;
		uint8_t ucDarkPixels       [DxOISP_NB_EXPOSURE_TIME] ;
	} stSensorDefectCorrection ;

	struct {
		uint8_t ucSegmentation     [DxOISP_NB_FINE_GAIN] ;
		uint8_t ucGrain            [DxOISP_NB_FINE_GAIN] ;
		uint8_t ucLuminanceNoise   [DxOISP_NB_FINE_GAIN] ;
		uint8_t ucChrominanceNoise [DxOISP_NB_FINE_GAIN] ;
	} stNoiseAndDetails ;

	struct {
		int8_t   cWbBiasRed  [DxOISP_NB_FINE_ILLUMINANT] ;
		int8_t   cWbBiasBlue [DxOISP_NB_FINE_ILLUMINANT] ;
		int8_t   cSaturation [DxOISP_NB_COARSE_GAIN][DxOISP_NB_COARSE_ILLUMINANT] ;
	} stColor ;

	struct {
		int8_t  cBrightness ;
		int8_t  cContrast ;
		uint8_t ucBlackLevel[DxOISP_NB_COARSE_GAIN][DxOISP_NB_COARSE_ILLUMINANT];
		uint8_t ucWhiteLevel[DxOISP_NB_COARSE_GAIN][DxOISP_NB_COARSE_ILLUMINANT];
		uint8_t ucGamma     [DxOISP_NB_COARSE_GAIN][DxOISP_NB_COARSE_ILLUMINANT];
	}stExposure;

	uint8_t   isDebugEnable;  //Only available for dbg library
}ts_DxOIspAsyncCmd;

///////////////////////////////////////ts_DxOIspImageInfo

typedef struct {
	struct {
		uint16_t usAnalogGain [DxOISP_NB_CHANNEL];   //expressed in 8.8
		uint16_t usDigitalGain[DxOISP_NB_CHANNEL];   //expressed in 8.8
		uint16_t usIspDigitalGain;                   //expressed in 8.8
		uint32_t uiExposureTime;                     //ms expressed in 16.16
		uint16_t usAperture;                         //f number expressed in 8.8
	}stAe;
	struct {
		uint8_t  eSelectedPreset;                    //te_DxOISPAwbPreset
		uint16_t usWbRedScale;                       //expressed in 8.8
		uint16_t usWbBlueScale;                      //expressed in 8.8
	}stAwb;
	struct {
		uint8_t  eDistance;                          //te_DxOISPDafFocus
	}stDaf;
	struct {
		uint8_t  eConvergence;                       //te_DxOISPAfConvergence
		uint16_t usPosition;                         //dioptry, expressed in 4.12
	}stAf;
	struct {
		uint16_t usPower;                            //lux at 1m on optical axis expressed in 16.0 (0 = off)
	}stFlash;
	struct {
		uint8_t  eFrequency;                         //te_DxOISPAfkFrequency
	}stAfk;
	uint8_t      ucDownsamplingX;                        //Sensor+IPC
	uint8_t      ucDownsamplingY;                        //Sensor+IPC
	uint8_t      ucBinningX;                             //Sensor+IPC
	uint8_t      ucBinningY;                             //Sensor+IPC
}ts_DxOIspImageInfo;

///////////////////////////////////////ts_DxOIspSyncStatus
typedef struct {
	struct {
		uint8_t        eMode;             //te_DxOISPMode
		uint8_t        eSubModeVideoRec;  //te_DxOISPSubModeVideoRec
		uint8_t        isDisplayDisabled; //boolean
		uint16_t       usFrameRate;       //fps expressed in (12.4)
		uint32_t       uiFrameCount;
		uint32_t       uiErrorCode;       //te_DxOISPError
	}stSystem;

	ts_DxOIspImageInfo stImageInfo;
	ts_DxOISPOutput    stOutputDisplay;
	ts_DxOISPOutput    stOutputImage;
	uint16_t           usOutputRawWidth;
	uint16_t           usOutputRawHeight;
}ts_DxOIspSyncStatus;

///////////////////////////////////////ts_DxOIspCapabilities
typedef	struct {
	uint16_t usMajorId;
	uint16_t usMinorId;
	struct {
		uint16_t usStillFrameSizeX;
		uint16_t usStillFrameSizeY;
		uint16_t usVideoFrameSizeX;
		uint16_t usVideoFrameSizeY;
		uint16_t usScreenFrameSizeX;
		uint16_t usScreenFrameSizeY;
		uint16_t usStillThroughput;    //fps expresssed in 12.4 (0 = not supported)
		uint16_t usVideoThroughput;    //fps expresssed in 12.4 (0 = not supported)
		uint8_t  ucNbSensors;
		uint8_t  ucRawPixelWidth;
		uint8_t  isScreenScaler;
		uint8_t  isCaptureScaler;
		uint8_t  isRawOutput;
		uint16_t usBandSize;           //Size of all bands except the last one (in pixels)
	}stHardware;
	struct {
		uint8_t  isAfk;
		uint8_t  isAe;
		uint8_t  isAwb;
		uint8_t  isFlash;
		uint8_t  isDaf;
		uint8_t  isAf;
	}stCameraControl;
}ts_DxOIspCapabilities;

///////////////////////////////////////ts_DxOIspSensorCapabilities
typedef struct {
	uint8_t      enableInterframeCmdOnly;           // boolean value
	struct {
		uint8_t  eImageType;                    // te_DxOISPFormat
		uint8_t  ucPixelWidth;                  // pixel width in bits (8, 10 or 12)
		uint8_t  eEncoding;                     // te_DxOISPEncoding for YUV sensors
		uint8_t  ePhase;                        // te_SensorPhase
		uint16_t usBayerWidth;                  // in Bayer, native sensor width
		uint16_t usBayerHeight;                 // in Bayer, native sensor height
		uint16_t usMinHorBlanking;              // minimum horizontal blanking expressed in pixels
		uint16_t usMinVertBlanking;             // minimum vertical blanking expressed in lines
		uint8_t  ucMaxHorDownsamplingFactor;    // maximal horizontal downsampling that can be provided by the sensor
		uint8_t  ucMaxVertDownsamplingFactor;   // maximal vertical downsampling that can be provided by the sensor
		uint32_t uiMaxPixelClock;               // unit: Mhz, expressed in 16.16
		uint8_t  ucMaxDelayImageFeature;        // Delay max in frame to reach a new crop/downsampling/blanking configuration
		uint8_t  ucMaxDelayImageQuality;        // Delay max in frame to reach a given image quality
		uint8_t  ucNbUpperDummyLines;           // Number of dummy lines sent by the sensor on top of the image to the DxO ISP
		uint8_t  ucNbLeftDummyColumns;          // Number of dummy columns sent by the sensor on the left of the image to the DxO ISP
		uint8_t  ucNbLowerDummyLines;           // Number of dummy lines sent by the sensor on bottom of the image to the DxO ISP
		uint8_t  ucNbRightDummyColumns;         // Number of dummy columns sent by the sensor on the right of the image to the DxO ISP
		uint16_t usPedestal;                    // Minimal value of a pixel
	} stImageFeature;

	struct {
		uint16_t  usIso;                        // ISO for AGain = DGain = 1
		uint16_t  usMaxAGain;                   // maximum analog gain set to the sensor. expressed in 8.8
		uint16_t  usMaxDGain;                   // maximum digital gain set to the sensor. expressed in 8.8
	} stShootingParam;

	struct {
		uint16_t usMaxPower;                    // in Cd on optical axis (16.0) (0 = no flash)
	} stFlash;

	struct {
		uint16_t usMin;                        // unit: dioptry, expressed in 4.12
		uint16_t usMax;                        // unit: dioptry, expressed in 4.12
	} stAfPosition;                                // if (min = max) then no mechanical AF

	struct {
		uint8_t  isEdof;
	}stEdof;

	struct {
		uint16_t usMin;                       // ratio between focal distance and sensor diagonal size expressed in 8.8
		uint16_t usMax;                       // ratio between focal distance and sensor diagonal size expressed in 8.8
	} stZoomFocal;                                // if (min = max) then no optical zoom

	struct {
		uint16_t usMin;                      // f number expressed in 8.8
		uint16_t usMax;                      // f number expressed in 8.8
	} stAperture;

} ts_DxOIspSensorCapabilities;

///////////////////////////////////////ts_DxOIspAsyncStatus


typedef struct {
	ts_DxOIspCapabilities         stIspCapabilities;
	ts_DxOIspSensorCapabilities   stSensorCapabilities;
	struct {
		uint16_t usWinCountR;
		uint16_t usBufR           [DxOISP_USBUFR_SIZE_Y][DxOISP_USBUFR_SIZE_X];
		uint16_t usWinCountB;
		uint16_t usBufB           [DxOISP_USBUFB_SIZE_Y][DxOISP_USBUFB_SIZE_X];
		uint16_t usWinCountG;
		uint16_t usBufG           [DxOISP_USBUFG_SIZE_Y][DxOISP_USBUFG_SIZE_X];
		uint16_t usWinCountUnsatRatio;
		uint16_t usBufUnsatRatio  [DxOISP_USBUFUNSATRATIO_SIZE_Y][DxOISP_USBUFUNSATRATIO_SIZE_X];
		uint32_t uiSumHistoMaxRGB;
		uint32_t uiHistoMaxRGB    [DxOISP_UIHISTOMAXRGB_SIZE];
		uint16_t usWinCountDeltaG;
		uint16_t usBufDeltaG      [DxOISP_USBUFDELTAG_SIZE_Y][DxOISP_USBUFDELTAG_SIZE_X];
		#if DxOISP_PRIVATE_STAT_SIZE
		uint16_t usPrivate        [DxOISP_PRIVATE_STAT_SIZE];
		#endif
	}stStat;
}ts_DxOIspAsyncStatus;

/******************************************************/
/*                   REGISTER MAP                     */
/******************************************************/

typedef struct {
	ts_DxOIspSyncCmd         stSync;
	ts_DxOIspAsyncCmd        stAsync;
}ts_DxOIspCmd;

typedef struct {
	ts_DxOIspSyncStatus      stSync;
	ts_DxOIspAsyncStatus     stAsync;
}ts_DxOIspStatus;


/******************************************************/
/*              OFFSET STRUCTURE	              */
/******************************************************/

#define DxOISP_CMD_OFFSET(x)         offsetof(ts_DxOIspCmd   , x)
#define DxOISP_STATUS_OFFSET(x)      offsetof(ts_DxOIspStatus, x)

/******************************************************/
/*              EXPORTED FUNCTIONS	              */
/******************************************************/

void     DxOISP_Init             (void);
void     DxOISP_Event            (void);
void     DxOISP_PostEvent        (void);

void     DxOISP_CommandGroupOpen    (void);
void     DxOISP_CommandSet          (uint16_t usOffset, uint16_t usSize, void* pBuf);
uint32_t DxOISP_CommandGroupClose   (void);

void     DxOISP_StatusGroupOpen     (void);
void     DxOISP_StatusGet           (uint16_t usOffset, uint16_t usSize, void* pBuf);
void     DxOISP_StatusGroupClose    (void);

/******************************************************/
/*               Added by Marvell                     */
/******************************************************/

typedef struct {
	uint8_t    id;
	int        *ptr;
	int        *dummy;
	uint8_t    port;
} ts_DxOISPInfo;


extern void getInfoStreamState( ts_DxOISPInfo *ptr, uint32_t *state );
extern uint32_t *const infoInstTab[];

#endif //__DXO_ISP_H__

