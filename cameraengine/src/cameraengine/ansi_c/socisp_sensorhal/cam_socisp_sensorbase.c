/*******************************************************************************
//(C) Copyright [2010 - 2011] Marvell International Ltd.
//All Rights Reserved
*******************************************************************************/

#include <sys/poll.h>
#include <linux/videodev2.h>
#include <errno.h>
#include "CameraOSAL.h"
#include "cam_utility.h"

#if defined ( BUILD_OPTION_STRATEGY_ENABLE_MC )
#include "mvisp.h"
extern int iIspCameraFd;
extern int iIspCCICFd;
#endif

#define MAP_BUFFER_NUM 8

#define V4L2_CID_ISP_MIPISETTING 0x980927
#define V4L2_CID_ISP_SET_DPHY5   0x980928

static int fd_camera = -1;
static int fd_ccic   = -1;

static uint32_t I2C_Write( uint16_t usOffset, uint8_t* ptucData, uint8_t ucSize )
{
	unsigned char i;
	int           ret;

#if !defined( BUILD_OPTION_STRATEGY_ENABLE_MC )
	struct v4l2_dbg_register s;
#else
	struct v4l2_sensor_register_access reg_access;
#endif

	for ( i = 0; i < ucSize; i++ )
	{
		// use uio ioctl for I2C operations

#if !defined( BUILD_OPTION_STRATEGY_ENABLE_MC )
		s.reg = usOffset;
		s.val = *ptucData;

		ret = ioctl( fd_camera, VIDIOC_DBG_S_REGISTER, &s );
#else
		reg_access.reg   = usOffset;
		reg_access.value = *ptucData;

		ret = ioctl( fd_camera, VIDIOC_PRIVATE_SENSER_REGISTER_SET, &reg_access );
#endif

		if ( ret < 0 )
		{
			return ret;
		}
		usOffset++;
		ptucData++;
	}

	return 0;
}


static uint32_t I2C_Read( uint16_t usOffset,uint8_t* ptucData, uint8_t ucSize )
{
	unsigned char            i;
	int                      ret;
#if !defined( BUILD_OPTION_STRATEGY_ENABLE_MC )
	struct v4l2_dbg_register s;
#else
	struct v4l2_sensor_register_access reg_access;
#endif

	for ( i = 0; i < ucSize; i++ )
	{
		// use uio ioctl for I2C operations
#if !defined( BUILD_OPTION_STRATEGY_ENABLE_MC )
		s.reg = usOffset;

		ret = ioctl( fd_camera, VIDIOC_DBG_G_REGISTER, &s );
		if ( ret < 0 )
		{
			return ret;
		}

		usOffset++;
		*ptucData = s.val;
#else
		reg_access.reg = usOffset;

		ret = ioctl( fd_camera, VIDIOC_PRIVATE_SENSER_REGISTER_GET, &reg_access );
		if ( ret < 0 )
		{
			return ret;
		}

		usOffset++;
		*ptucData = reg_access.value;
#endif
		ptucData ++;
	}

	return 0;
}

static void DeviceInit( const char *sDeviceNode, int iChannelId )
{
	int                 ret         = 0;
	struct v4l2_control stCtrlParam = { V4L2_CID_ISP_MIPISETTING, 0 };

	// init sensor device
#if !defined( BUILD_OPTION_STRATEGY_ENABLE_MC )
	fd_camera = open( sDeviceNode, O_RDWR );
	if ( fd_camera < 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to open camera device node[%s], error code[%s]( %s, %s, %d )\n", sDeviceNode, strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
		goto exit;
	}

	ret = ioctl( fd_camera, VIDIOC_S_INPUT, &iChannelId );
	if ( ret < 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to select sensor[%d], fd[%d], error code[%s]( %s, %s, %d )\n", iChannelId, fd_camera, strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
		goto exit;
	}

	ret = ioctl( fd_camera, VIDIOC_S_CTRL, &stCtrlParam );
	if ( ret < 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to set MIPI, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
		goto exit;
	}
#else
	fd_camera = iIspCameraFd;
	if ( fd_camera < 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to open camera device node[%s], fd[%d]( %s, %s, %d )\n",
		       sDeviceNode, fd_camera, __FILE__, __FUNCTION__, __LINE__ );
		goto exit;
	}

	fd_ccic = iIspCCICFd;
	if ( fd_ccic < 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to open ccic device node, fd[%d]( %s, %s, %d )\n",
		       fd_ccic, __FILE__, __FUNCTION__, __LINE__ );
		goto exit;
	}
#endif
	return ;

exit:
	if ( fd_camera >= 0 )
	{
		close( fd_camera );
		fd_camera = -1;
	}

	if ( fd_ccic >= 0 )
	{
		close( fd_ccic );
		fd_ccic = -1;
	}

	return;
}

static void DeviceDeinit()
{
#if !defined( BUILD_OPTION_STRATEGY_ENABLE_MC )
	if ( fd_camera != -1 )
	{
		close( fd_camera );
		fd_camera = -1;
	}
#else
	fd_camera = -1;
	fd_ccic   = -1;
#endif

	return;
}

static void DeviceStart()
{
#if !defined( BUILD_OPTION_STRATEGY_ENABLE_MC )
	int                 ret         = 0;
	struct v4l2_control stCtrlParam = { V4L2_CID_ISP_SET_DPHY5, 0x00 };

	if ( NB_MIPI_LANES == 2 )
	{
		stCtrlParam.value = 0x33;
	}
	else if ( NB_MIPI_LANES == 4 )
	{
		stCtrlParam.value = 0xff;
	}
	else
	{
		TRACE( CAM_ERROR, "Error: don't support MIPI lane number[%d]( %s, %s, %d )\n", NB_MIPI_LANES, __FILE__, __FUNCTION__, __LINE__ );
		ASSERT( 0 );
	}

	ret = ioctl( fd_camera, VIDIOC_S_CTRL, &stCtrlParam );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to set start DPHY5 streaming, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
	}
#else
	int                              ret = -1;
	struct v4l2_ccic_config_mipi     stMIPISetting = { 0 };

	stMIPISetting.start_mipi = 1;
	ret = ioctl( fd_ccic, VIDIOC_PRIVATE_CCIC_CONFIG_MIPI, &stMIPISetting );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to start MIPI, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
	}
#endif

	return;
}

static void DeviceStop()
{
#if !defined( BUILD_OPTION_STRATEGY_ENABLE_MC )
	int                 ret         = 0;
	struct v4l2_control stCtrlParam = { V4L2_CID_ISP_SET_DPHY5, 0x00 };

	ret = ioctl( fd_camera, VIDIOC_S_CTRL, &stCtrlParam );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to set stop DPHY5 streaming, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
	}
#else
	int                              ret = -1;
	struct v4l2_ccic_config_mipi     stMIPISetting = { 0 };

	stMIPISetting.start_mipi = 0;
	ret = ioctl( fd_ccic, VIDIOC_PRIVATE_CCIC_CONFIG_MIPI, &stMIPISetting );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to stop MIPI, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
	}
#endif

	return;
}

static void CopyConvert10bitBayer2U16( unsigned char *pSrc, unsigned short *pDst, unsigned int iWidth, unsigned int iHeight )
{
	unsigned int   x, y, i;
	unsigned short d0, d4;
	for ( y = 0; y < iHeight; y++ )
	{
		for ( x = 0; x < iWidth; x += 4 )
		{
			for ( i = 0; i < 4; i++ )
			{
				d0 = ((unsigned short) pSrc[i]) << 2;
				d4 = ((unsigned short) pSrc[4]) >> (i * 2);
				d4 &= 0x3;
				pDst[i] = d0 | d4;
			}
			pSrc += 5;
			pDst += 4;
		}
	}
	return;
}

static int dump2file( const char *sFileName, unsigned char *pBuffer, unsigned int iWidth, unsigned int iHeight )
{
	int            iDesiredLen = 0;
	int            iActualLen  = 0;
	unsigned short *pRaw16     = NULL;
	FILE           *pFile      = NULL;

	if ( !pBuffer )
	{
		TRACE( CAM_ERROR, "Error: input paramters should not be NULL( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	pFile = fopen( sFileName, "ab" );
	if ( pFile == NULL )
	{
		TRACE( CAM_ERROR, "Error: open file[%s] failed( %s, %s, %d )\n", sFileName, __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	iDesiredLen = iWidth * iHeight * 2;
	pRaw16 = (unsigned short*)malloc( iDesiredLen );
	if ( !pRaw16 )
	{
		TRACE( CAM_ERROR, "Error: out of memory to allocate Raw16 buffer( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	CopyConvert10bitBayer2U16( pBuffer, pRaw16, iWidth, iHeight );

	iActualLen = fwrite( pRaw16, iDesiredLen, 1, pFile );
	if ( iActualLen != iDesiredLen )
	{
		TRACE( CAM_WARN, "Warning: only [%d] bytes out of [%d] bytes are written to file[%s]( %s, %s, %d )\n", iActualLen, iDesiredLen, sFileName, __FILE__, __FUNCTION__, __LINE__ );
	}
	fflush( pFile );

	free( pRaw16 );
	fclose( pFile );

	return 0;
}

static void DeviceRawDump( const char *fname, int width, int height )
{
	struct buffer_s
	{
		void *  pAddr;
		size_t  iBufLen;
	};
	struct buffer_s             *astBufList       = NULL;  // buffer list
	enum   v4l2_buf_type        stBufType         = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	struct  v4l2_streamparm     stParam;
	struct  v4l2_format         stFormat;
	unsigned int                uiPicBufSize;
	struct  v4l2_buffer         stBuf;
	void                        *pVirAddr         = NULL;
	CAM_Int32u                  uiPhyAddr         = 0;
	int                         ret               = -1;
	int                         iBufCount         = 0;
	struct  v4l2_requestbuffers stReqBuf;
	struct  v4l2_buffer         stCamBuf;
	struct pollfd               stCamPollFd;
	unsigned char               *pucSrc           = NULL;
	int                         iLen              = 0;

	/* set desired capture parameter */
	stParam.type                                  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	stParam.parm.capture.timeperframe.numerator   = 0;
	stParam.parm.capture.timeperframe.denominator = 0;
	stParam.parm.capture.capturemode              = 0;  // set as video mode
	ret = ioctl( fd_camera, VIDIOC_S_PARM, &stParam );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to set params, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
		goto exit;
	}

	/* it is due to the firmware acquire one more cloumn and one more line for isp performance */
	stFormat.fmt.pix.width       = _ALIGN_TO( (width + 2), 8); /*due to sensor line need to multiply by 8*/
	stFormat.fmt.pix.height      = height + 2;
	stFormat.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
	stFormat.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	uiPicBufSize                 = stFormat.fmt.pix.width * stFormat.fmt.pix.height * 16 / 8;
	ret = ioctl( fd_camera, VIDIOC_S_FMT, &stFormat );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to set format, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
		goto exit;
	}

	uiPicBufSize = _ALIGN_TO( uiPicBufSize, 4096);
	astBufList= (struct buffer_s *)calloc( MAP_BUFFER_NUM, sizeof ( struct buffer_s ) );
	if ( !astBufList )
	{
		TRACE( CAM_ERROR, "Error: can not malloc memory( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		goto exit;
	}

	memset( &stReqBuf, 0, sizeof( stReqBuf ) );
	stReqBuf.type    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	stReqBuf.memory  = V4L2_MEMORY_USERPTR;
	stReqBuf.count   = MAP_BUFFER_NUM;
	ret = ioctl( fd_camera, VIDIOC_REQBUFS, &stReqBuf );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to require buffer, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
		goto exit;
	}

	pVirAddr = osalbmm_malloc( uiPicBufSize * MAP_BUFFER_NUM, OSALBMM_ATTR_NONCACHED );
	if ( pVirAddr == NULL )
	{
		TRACE( CAM_ERROR, "Error: failed to malloc picture buffer( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		goto exit;
	}
	uiPhyAddr = (CAM_Int32u)osalbmm_get_paddr( pVirAddr );

	for ( iBufCount = 0; iBufCount < MAP_BUFFER_NUM; iBufCount++ )
	{

		memset( pVirAddr, 0xFF, uiPicBufSize );
		memset( &stBuf, 0, sizeof( stBuf ) );
		stBuf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		stBuf.memory      = V4L2_MEMORY_USERPTR;
		stBuf.index       = iBufCount;
		stBuf.m.userptr   = (unsigned long )pVirAddr + iBufCount * uiPicBufSize;
		stBuf.length      = uiPicBufSize;

		ret = ioctl( fd_camera, VIDIOC_QBUF, &stBuf );
		if ( ret != 0 )
		{
			TRACE( CAM_ERROR, "Error: failed to enqueue buffer, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
			goto exit;
		}
		astBufList[iBufCount].pAddr = (void*)stBuf.m.userptr;
	}

	ret = ioctl( fd_camera, VIDIOC_STREAMON, &stBufType );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to stream on camera, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
		goto exit;
	}

	stCamPollFd.fd         = fd_camera;
	stCamPollFd.events     = POLLIN;
	do
	{
		ret = poll( &stCamPollFd, 1, -1 );
	} while ( !ret );

	memset( &stCamBuf, 0, sizeof( stCamBuf ) );
	stCamBuf.type    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	stCamBuf.memory  = V4L2_MEMORY_USERPTR;

	/* dequeue buffer, get a buffer fetch data */
	ret = ioctl( fd_camera, VIDIOC_DQBUF, &stCamBuf );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to dequeue buffer, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
		goto exit;
	}
	pucSrc = astBufList[stCamBuf.index].pAddr;
	iLen   = astBufList[stCamBuf.index].iBufLen;
	dump2file( fname, (unsigned char *)pucSrc, stFormat.fmt.pix.width, stFormat.fmt.pix.height );
	memset( pucSrc,0x00, iLen );

	ret = ioctl( fd_camera, VIDIOC_QBUF, &stCamBuf );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to enqueue buffer, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
		goto exit;
	}

	ret = ioctl( fd_camera, VIDIOC_STREAMOFF, &stBufType );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to stream off camera, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
		goto exit;
	}

	struct v4l2_control stCtrlParam = { V4L2_CID_ISP_MIPISETTING, 0 };
	ret = ioctl( fd_camera, VIDIOC_S_CTRL, &stCtrlParam );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to set camera control parameter, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
		goto exit;
	}

exit:
	if ( pVirAddr )
	{
		osalbmm_free( pVirAddr );
		pVirAddr = NULL;
	}
	if ( astBufList )
	{
		free( astBufList );
		astBufList = NULL;
	}
	return;
}
