/******************************************************************************
// (C) Copyright 2009 Marvell International Ltd.
// All Rights Reserved
******************************************************************************/
#if !defined(_IPPOMXLOGGER_DEF_H_)
#define _IPPOMXLOGGER_DEF_H_

#define CALLSTACK_ENABLE    0x00000001
#define INFO_ENABLE         0x00000002
#define ERROR_ENABLE        0x00000004
/*add by ajwen 20091111*/

/** Remove all debug output lines
 */
#define OMX_LOG_DEB_LEV_NO_OUTPUT  0

/** Messages showing values related to the test and the component/s used
 */
#define OMX_LOG_DEB_LEV_PARAMS     1

/** Messages representing steps in the execution. These are the simple messages, because
 * they avoid iterations
 */
#define OMX_LOG_DEB_LEV_SIMPLE_SEQ 2

/** Messages representing steps in the execution. All the steps are described,
 * also with iterations. With this level of output the performances are
 * seriously compromised
 */
#define OMX_LOG_DEB_LEV_FULL_SEQ   4 


/** All the messages - max value
 */
#define OMX_LOG_DEB_ALL_MESS   255
enum{
AUDIO_COMPONENT_ID_AAC_DEC=0,
AUDIO_COMPONENT_ID_AAC_ENC      ,    
AUDIO_COMPONENT_ID_AMRWB_DEC    ,    
AUDIO_COMPONENT_ID_AMRWB_ENC    ,    
AUDIO_COMPONENT_ID_AMRNB_DEC    ,    
AUDIO_COMPONENT_ID_AMRNB_ENC    ,    
AUDIO_COMPONENT_ID_MP3_DEC      ,    
AUDIO_COMPONENT_ID_MP3_ENC      ,    
AUDIO_COMPONENT_ID_WMA_DEC      ,    
AUDIO_COMPONENT_ID_RENDERER_PCM ,    
AUDIO_COMPONENT_ID_MAX=30
};
enum{
VIDEO_COMPONENT_ID_H263_DEC=0       ,
VIDEO_COMPONENT_ID_H263_ENC       ,
VIDEO_COMPONENT_ID_H264_DEC       ,
VIDEO_COMPONENT_ID_H264_ENC       ,
VIDEO_COMPONENT_ID_H264_DEC_MVED  ,
VIDEO_COMPONENT_ID_H264_ENC_MVED  ,
VIDEO_COMPONENT_ID_MPEG4_DEC      ,
VIDEO_COMPONENT_ID_MPEG4_ENC      ,
VIDEO_COMPONENT_ID_MPEG4_DEC_MVED ,
VIDEO_COMPONENT_ID_MPEG4_ENC_MVED ,
VIDEO_COMPONENT_ID_WMV_DEC        ,
VIDEO_COMPONENT_ID_WMV_DEC_MVED   ,
VIDEO_COMPONENT_ID_VMETA_DEC      ,
VIDEO_COMPONENT_ID_VMETA_ENC      ,
VIDEO_COMPONENT_ID_MAX=30
};
enum{
IMAGE_COMPONENT_ID_GIF_DEC=0    ,
IMAGE_COMPONENT_ID_JPEG_DEC   ,
IMAGE_COMPONENT_ID_JPEG_ENC   ,
IMAGE_COMPONENT_ID_PNG_DEC    ,
IMAGE_COMPONENT_ID_RSZROTCSC  ,
IMAGE_COMPONENT_ID_MAX = 30
};
enum{
OTHER_COMPONENT_ID_CLOCK=0      ,
OTHER_COMPONENT_ID_MP4_DEMUXER,
OTHER_COMPONENT_ID_FILEREADER,
OTHER_COMPONENT_ID_FILEWRITER,
OTHER_COMPONENT_ID_MAX = 30
};
/*audio domain component*/                        
#define OMX_LOG_DEB_COMP_AAC_DEC        (1 << AUDIO_COMPONENT_ID_AAC_DEC)  
#define OMX_LOG_DEB_COMP_AAC_ENC        (1 << AUDIO_COMPONENT_ID_AAC_ENC)  
#define OMX_LOG_DEB_COMP_AMRWB_DEC      (1 << AUDIO_COMPONENT_ID_AMRWB_DEC)  
#define OMX_LOG_DEB_COMP_AMRWB_ENC      (1 << AUDIO_COMPONENT_ID_AMRWB_ENC)  
#define OMX_LOG_DEB_COMP_AMRNB_DEC      (1 << AUDIO_COMPONENT_ID_AMRNB_DEC)  
#define OMX_LOG_DEB_COMP_AMRNB_ENC      (1 << AUDIO_COMPONENT_ID_AMRNB_ENC)  
#define OMX_LOG_DEB_COMP_MP3_DEC        (1 << AUDIO_COMPONENT_ID_MP3_DEC)  
#define OMX_LOG_DEB_COMP_MP3_ENC        (1 << AUDIO_COMPONENT_ID_MP3_ENC)  
#define OMX_LOG_DEB_COMP_WMA_DEC        (1 << AUDIO_COMPONENT_ID_WMA_DEC)  
#define OMX_LOG_DEB_COMP_RENDERER_PCM   (1 << AUDIO_COMPONENT_ID_RENDERER_PCM )
                                                  
/*video domain componet*/    
#define OMX_LOG_DEB_COMP_H263_DEC       (1 << VIDEO_COMPONENT_ID_H263_DEC      ) 
#define OMX_LOG_DEB_COMP_H263_ENC       (1 << VIDEO_COMPONENT_ID_H263_ENC      ) 
#define OMX_LOG_DEB_COMP_H264_DEC       (1 << VIDEO_COMPONENT_ID_H264_DEC      ) 
#define OMX_LOG_DEB_COMP_H264_ENC       (1 << VIDEO_COMPONENT_ID_H264_ENC      ) 
#define OMX_LOG_DEB_COMP_H264_DEC_MVED  (1 << VIDEO_COMPONENT_ID_H264_DEC_MVED ) 
#define OMX_LOG_DEB_COMP_H264_ENC_MVED  (1 << VIDEO_COMPONENT_ID_H264_ENC_MVED ) 
#define OMX_LOG_DEB_COMP_MPEG4_DEC      (1 << VIDEO_COMPONENT_ID_MPEG4_DEC     ) 
#define OMX_LOG_DEB_COMP_MPEG4_ENC      (1 << VIDEO_COMPONENT_ID_MPEG4_ENC     ) 
#define OMX_LOG_DEB_COMP_MPEG4_DEC_MVED (1 << VIDEO_COMPONENT_ID_MPEG4_DEC_MVED) 
#define OMX_LOG_DEB_COMP_MPEG4_ENC_MVED (1 << VIDEO_COMPONENT_ID_MPEG4_ENC_MVED) 
#define OMX_LOG_DEB_COMP_WMV_DEC        (1 << VIDEO_COMPONENT_ID_WMV_DEC       ) 
#define OMX_LOG_DEB_COMP_WMV_DEC_MVED   (1 << VIDEO_COMPONENT_ID_WMV_DEC_MVED  ) 
#define OMX_LOG_DEB_COMP_VMETA_DEC      (1 << VIDEO_COMPONENT_ID_VMETA_DEC     ) 
#define OMX_LOG_DEB_COMP_VMETA_ENC      (1 << VIDEO_COMPONENT_ID_VMETA_ENC     ) 

/*image domain component*/                        
                                                  
#define OMX_LOG_DEB_COMP_GIF_DEC        (1 << IMAGE_COMPONENT_ID_GIF_DEC) 
#define OMX_LOG_DEB_COMP_JPEG_DEC       (1 << IMAGE_COMPONENT_ID_JPEG_DEC) 
#define OMX_LOG_DEB_COMP_JPEG_ENC       (1 << IMAGE_COMPONENT_ID_JPEG_ENC) 
#define OMX_LOG_DEB_COMP_PNG_DEC        (1 << IMAGE_COMPONENT_ID_PNG_DEC) 
#define OMX_LOG_DEB_COMP_RSZROTCSC      (1 << IMAGE_COMPONENT_ID_RSZROTCSC) 
                                                  
/*other domain component*/                        
#define OMX_LOG_DEB_COMP_CLOCK          (1 << OTHER_COMPONENT_ID_CLOCK) 
#define OMX_LOG_DEB_COMP_MP4_DEMUXER    (1 << OTHER_COMPONENT_ID_MP4_DEMUXER) 
#define OMX_LOG_DEB_COMP_FILEREADER     (1 << OTHER_COMPONENT_ID_FILEREADER)
#define OMX_LOG_DEB_COMP_FILEWRITE      (1 << OTHER_COMPONENT_ID_FILEWRITER)

#define OMX_LOG_DEB_COMP_ALL            (0xFFFFFFFF)


extern unsigned int g_IPPOMX_logFilterLevel;
extern unsigned int g_IPPOMX_logFilterCompAudio ;
extern unsigned int g_IPPOMX_logFilterCompVideo ;
extern unsigned int g_IPPOMX_logFilterCompImage ;
extern unsigned int g_IPPOMX_logFilterCompOther ;

/*end ajwen*/
extern unsigned int g_IPPOMX_logFilter;
extern char * logfileName;


#endif // _IPPOMXLOGGER_DEF_H_
