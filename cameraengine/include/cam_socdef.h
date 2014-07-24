/******************************************************************************
//(C) Copyright [2010 - 2011] Marvell International Ltd.
//All Rights Reserved
******************************************************************************/

#ifndef _CAM_SOC_DEF_H_
#define _CAM_SOC_DEF_H_

#ifdef __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////////////
// Graphics accelerator
//////////////////////////////////////////////////////////////////////////////
#define PLATFORM_GRAPHICS_SOFTWARE_MASK 0x10
#define PLATFORM_GRAPHICS_WMMX          0x11
#define PLATFORM_GRAPHICS_WMMX2         0x12

#define PLATFORM_GRAPHICS_GCU_MASK      0x20
#define PLATFORM_GRAPHICS_GCU           0x21

#define PLATFORM_GRAPGICS_GC_MASK       0x40
#define PLATFORM_GRAPHICS_GC200         0x41
#define PLATFORM_GRAPHICS_GC300         0x42
#define PLATFORM_GRAPHICS_GC500         0x43
#define PLATFORM_GRAPHICS_GC600         0x44
#define PLATFORM_GRAPHICS_GC800         0x45
#define PLATFORM_GRAPHICS_GC860         0x46
#define PLATFORM_GRAPHICS_GC1000        0x47
#define PLATFORM_GRAPHICS_GC1500        0x48
#define PLATFORM_GRAPHICS_GC2000        0x49

//////////////////////////////////////////////////////////////////////////////
// Video accelerator
//////////////////////////////////////////////////////////////////////////////
#define PLATFORM_VIDEO_SOFTWARE_MASK    0x10
#define PLATFORM_VIDEO_WMMX             0x11
#define PLATFORM_VIDEO_WMMX2            0x12

#define PLATFORM_VIDEO_MVED_MASK        0x20
#define PLATFORM_VIDEO_MVED             0x21

#define PLATFORM_VIDEO_CNM_MASK         0x40
#define PLATFORM_VIDEO_CNM              0x41

#define PLATFORM_VIDEO_VMETA_MASK       0x80
#define PLATFORM_VIDEO_VMETA_V12        0x81
#define PLATFORM_VIDEO_VMETA_V13        0x82
#define PLATFORM_VIDEO_VMETA_V14        0x83
#define PLATFORM_VIDEO_VMETA_V15        0x84
#define PLATFORM_VIDEO_VMETA_V16        0x85
#define PLATFORM_VIDEO_VMETA_V17        0x86
#define PLATFORM_VIDEO_VMETA_V18        0x87
#define PLATFORM_VIDEO_VMETA_V22        0x88


//////////////////////////////////////////////////////////////////////////////
// Camera interface
//////////////////////////////////////////////////////////////////////////////
#define PLATFORM_CAMERAIF_QCI           0x1
#define PLATFORM_CAMERAIF_CCIC          0x2
#define PLATFORM_CAMERAIF_ISPDMA        0x4

//////////////////////////////////////////////////////////////////////////////
// IRE interface
//////////////////////////////////////////////////////////////////////////////
#define PLATFORM_IRE_NONE               0x0
#define PLATFORM_IRE                    0x1

//////////////////////////////////////////////////////////////////////////////
// Processor features
//////////////////////////////////////////////////////////////////////////////
#if defined(PLATFORM_PROCESSOR_MHP) ||\
    defined(PLATFORM_PROCESSOR_MHL) ||\
    defined(PLATFORM_PROCESSOR_TAVORP) ||\
    defined(PLATFORM_PROCESSOR_TAVORL)

	#define PLATFORM_GRAPHICS_VALUE     PLATFORM_GRAPHICS_GCU
	#define PLATFORM_VIDEO_VALUE        PLATFORM_VIDEO_WMMX2
	#define PLATFORM_CAMERAIF_VALUE     PLATFORM_CAMERAIF_QCI
	#define PLATFORM_IRE_VALUE          PLATFORM_IRE_NONE

#elif defined(PLATFORM_PROCESSOR_MHLV) ||\
      defined(PLATFORM_PROCESSOR_TAVORPV) ||\
      defined(PLATFORM_PROCESSOR_TAVORP65)

	#define PLATFORM_GRAPHICS_VALUE     PLATFORM_GRAPHICS_GCU
	#define PLATFORM_VIDEO_VALUE        PLATFORM_VIDEO_MVED
	#define PLATFORM_CAMERAIF_VALUE     PLATFORM_CAMERAIF_QCI
	#define PLATFORM_IRE_VALUE          PLATFORM_IRE_NONE

#elif defined(PLATFORM_PROCESSOR_TAVORPV2)

	#define PLATFORM_GRAPHICS_VALUE     PLATFORM_GRAPHICS_GC500
	#define PLATFORM_VIDEO_VALUE        PLATFORM_VIDEO_WMMX2
	#define PLATFORM_CAMERAIF_VALUE     PLATFORM_CAMERAIF_CCIC
	#define PLATFORM_IRE_VALUE          PLATFORM_IRE_NONE

#elif defined(PLATFORM_PROCESSOR_TD) || \
      defined(PLATFORM_PROCESSOR_TTC)

	#define PLATFORM_GRAPHICS_VALUE     PLATFORM_GRAPHICS_GC500
	#define PLATFORM_VIDEO_VALUE        PLATFORM_VIDEO_CNM
	#define PLATFORM_CAMERAIF_VALUE     PLATFORM_CAMERAIF_CCIC
	#define PLATFORM_IRE_VALUE          PLATFORM_IRE_NONE

#elif defined(PLATFORM_PROCESSOR_MG1)

	#define PLATFORM_GRAPHICS_VALUE     PLATFORM_GRAPHICS_GC800
	#define PLATFORM_VIDEO_VALUE        PLATFORM_VIDEO_VMETA_V17
	#define PLATFORM_CAMERAIF_VALUE     PLATFORM_CAMERAIF_CCIC
	#define PLATFORM_IRE_VALUE          PLATFORM_IRE_NONE

#elif defined(PLATFORM_PROCESSOR_NEVO)

	#define PLATFORM_GRAPHICS_VALUE     PLATFORM_GRAPHICS_GC860
	#define PLATFORM_VIDEO_VALUE        PLATFORM_VIDEO_VMETA_V18
	#define PLATFORM_CAMERAIF_VALUE     PLATFORM_CAMERAIF_CCIC
	#define PLATFORM_IRE_VALUE          PLATFORM_IRE_NONE

#elif defined(PLATFORM_PROCESSOR_ASPEN)

	#define PLATFORM_GRAPHICS_VALUE     PLATFORM_GRAPHICS_GC300
	#define PLATFORM_VIDEO_VALUE        PLATFORM_VIDEO_WMMX2
	#define PLATFORM_CAMERAIF_VALUE     PLATFORM_CAMERAIF_CCIC
	#define PLATFORM_IRE_VALUE          PLATFORM_IRE_NONE

#elif defined(PLATFORM_PROCESSOR_MMP2)

	#define PLATFORM_GRAPHICS_VALUE     PLATFORM_GRAPHICS_GC860
	#define PLATFORM_VIDEO_VALUE        PLATFORM_VIDEO_VMETA_V18
	#define PLATFORM_CAMERAIF_VALUE     ( PLATFORM_CAMERAIF_ISPDMA | PLATFORM_CAMERAIF_CCIC )
	#define PLATFORM_IRE_VALUE          PLATFORM_IRE_NONE

#elif defined(PLATFORM_PROCESSOR_MMP3)

	#define PLATFORM_GRAPHICS_VALUE     PLATFORM_GRAPHICS_GC2000
	#define PLATFORM_VIDEO_VALUE        PLATFORM_VIDEO_VMETA_V18
	#define PLATFORM_CAMERAIF_VALUE     ( PLATFORM_CAMERAIF_ISPDMA | PLATFORM_CAMERAIF_CCIC )
	#define PLATFORM_IRE_VALUE          PLATFORM_IRE_NONE

#else

	#error can not define features for unknown processor

#endif


#ifdef __cplusplus
}
#endif

#endif  // _CAM_SOC_DEF_H_


