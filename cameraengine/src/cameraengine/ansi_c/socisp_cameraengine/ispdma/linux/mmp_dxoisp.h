/*
 *  arch/arm/mach-mmp/include/mach/mmp_dxoisp.h - MMP2/MMP3 ISP DMA driver
 *
 *  Copyright:	(C) Copyright 2010 Marvell International Ltd.
 *              Hans Li <hans.li@marvell.com>
 *
 * A driver for the ISP DMA wrapper device
 *
 * The data sheet for this device can be found at:
 *    http://www.marvell.com/products/pcconn/88ALP01.jsp
 *
 * This file may be distributed under the terms of the GNU General
 * Public License, version 2.
 */

#ifndef __INC_MACH_DXOISP_H
#define __INC_MACH_DXOISP_H

/* ---------------------------------------------- */
/*              IOCTL Definition                  */
/* ---------------------------------------------- */

#define         ISP_IOCTL_DEBUG_ENABLE				(('d'<<8) | 100)          /* bitwise or of TRACE_*** */
#define         ISP_IOCTL_DEBUG_GETSTAT				(('d'<<8) | 101)          /* get the statistic result */

#define         ISP_IOCTL_IPCREG_SET				(('d'<<8) | 110)          /* set ipc register */
#define         ISP_IOCTL_IPCREG_GET				(('d'<<8) | 111)          /* get ipc register setting */
#define         ISP_IOCTL_IPCREG_SET_MULTI			(('d'<<8) | 112)          /* set multiple ipc registers */
#define         ISP_IOCTL_IPCREG_GET_MULTI			(('d'<<8) | 113)          /* set multiple ipc register settings */

#define         ISP_IOCTL_EVENT_WAIT_IPC			(('d'<<8) | 120)          /* sync ipc event */

#define         ISP_IOCTL_ISPDMA_SETUP_INPUT		(('d'<<8) | 130)          /* setup input dma */
#define         ISP_IOCTL_ISPDMA_SETUP_DISPLAY		(('d'<<8) | 131)          /* setup display dma */
#define         ISP_IOCTL_ISPDMA_SETUP_CODEC		(('d'<<8) | 132)          /* setup codec dma */
#define         ISP_IOCTL_ISPDMA_SETUP_FB			(('d'<<8) | 133)          /* setup frame buffer dma */
#define         ISP_IOCTL_ISPDMA_SETUP_STREAMING	(('d'<<8) | 134)          /* setup the streaming state */

#define         ISP_IOCTL_BUFFER_POLL				(('d'<<8) | 140)          /* sync dma events */
#define         ISP_IOCTL_BUFFER_DEQUEUE			(('d'<<8) | 141)          /* dequeue buffer */	// ==> non-blocking
#define         ISP_IOCTL_BUFFER_ENQUEUE			(('d'<<8) | 142)          /* enqueue buffer */
#define         ISP_IOCTL_BUFFER_BLOCK_DEQUEUE		(('d'<<8) | 143)          /* dequeue buffer */	// ==> blocking
#define         ISP_IOCTL_BUFFER_FLUSH				(('d'<<8) | 144)          /* flush buffer */

/* FIXME: refine these two API, it's better to align tick to system tick */
#define         ISP_IOCTL_TIMER_GETFREQ				(('d'<<8) | 150)          /* get the timer frequency, unit: Hz */
#define         ISP_IOCTL_TIMER_GETTICK				(('d'<<8) | 151)          /* get current timer tick */

#define         ISP_IOCTL_PERF_SETFREQDIV			(('d'<<8) | 160)          /* set dxo frequency */
#define         ISP_IOCTL_PERF_GETFREQDIV			(('d'<<8) | 161)          /* get dxo frequency */



#define TRACE_ERR		0x01		// fatal error, driver can't continue to work
#define TRACE_WARN		0x02		// improper user input
#define TRACE_MSG		0x04		// used to tracking the the driver internal operations
#define TRACE_DEBUG1	0x08		// used for user to monitor the calling sequence
#define TRACE_DEBUG2	0x10		// used for driver debug
#define TRACE_DMA		0x20		// used for driver debug, dump all DMA register set/get
#define TRACE_APMU		0x40		// used for driver debug, dump all APMU register set/get
#define TRACE_IPC		0x80		// used for driver debug, dump all IPC register set/get


#define ISP_INPUT_PATH_CSI0				0x0	// csi 0
#define ISP_INPUT_PATH_CSI1				0x1	// csi 1
#define ISP_INPUT_PATH_P0				0x2	// parallel 0
#define ISP_INPUT_PATH_P1				0x3	// parallel 1
#define ISP_INPUT_PATH_DMA				0x4	// input DMA
#define ISP_INPUT_PATH_AHB				0x5	// AHB path
#define ISP_INPUT_PATH_PG				0x6	// pattern generator

// OUTPUT DMA PORT
#define ISP_OUTPUT_PORT_NULL			0x0
#define ISP_OUTPUT_PORT_DISPLAY			0x1
#define ISP_OUTPUT_PORT_CODEC			0x2
#define ISP_OUTPUT_PORT_BOTH			(ISP_OUTPUT_PORT_DISPLAY | ISP_OUTPUT_PORT_CODEC)

// Format
#define ISP_FORMAT_UYVY					0
#define ISP_FORMAT_YUV420P				1
#define ISP_FORMAT_YUV420SP				2
#define ISP_FORMAT_RGB565				3
#define ISP_FORMAT_BGR565				4
#define ISP_FORMAT_RGB888				5
#define ISP_FORMAT_BGR888				6
#define ISP_FORMAT_FVTS					7

// Codec Mode
#define CODEC_VIDEO_REC_MODE			0x00
#define CODEC_CAPTURE_MODE				0x01


struct dxo_regs
{
	unsigned int		offset;
	unsigned int		value;
	const unsigned int	*src;
	unsigned int		*dst;
	unsigned int		cnt;
};

#define ISPDMA_CSI_LITTLEENDIAN		0
#define ISPDMA_CSI_BIGENDIAN		1

struct ispdma_in_par_csi
{
	int endian;

	int reserved;
};

struct ispdma_in_par_parallel
{
	int polarity_invert;

	int reserved;
};

struct ispdma_in_par_ahb
{
	int pitch;
	unsigned int phyAddr;

	int reserved;
};

struct ispdma_in_par_dma
{
	unsigned int phyAddr;
	int continous;			// 0 for 1-frame, 1 for continous frames
	int burst;				// 64, 128 or 256, unit: byte

	int reserved;
};

#define PG_PATTERN_FIXED	0
#define PG_PATTERN_INVERT	1
#define PG_PATTERN_SHIFT	2

struct ispdma_in_par_pg
{
	int pattern;
	int bayer[4];
	int continous;			// 0 for 1-frame, 1 for continous frames

	int reserved;
};

union ispdma_in_par
{
	struct ispdma_in_par_csi		csi;
	struct ispdma_in_par_parallel	parallel;
	struct ispdma_in_par_ahb		ahb;
	struct ispdma_in_par_dma		dma;
	struct ispdma_in_par_pg			pg;
};

struct ispdma_in_config
{
	unsigned int		path;		// must be one of ISP_INPUT_PATH_xxx

	int					bpp;		// 8, 10, 12
	int					w;			// image width in unit of pixels
	int					h;			// image height in unit of pixels

	union ispdma_in_par	par;
};

struct ispdma_display_config
{
	int					format;		// format of the display image
	int					w;			// image width in unit of pixels
	int					h;			// image height in unit of pixels
	int					burst;		// 64, 128 or 256, unit: byte
};

struct ispdma_codec_config
{
	int					format;		// format of the codec image
	int					w;			// image width in unit of pixels
	int					h;			// image height in unit of pixels
	int					burst;		// 64, 128 or 256, unit: byte
	int					vbSize;
	int					vbNum;
	int					reorder;	// 1 for reorder, 0 for no-reorder
	int 					mode; 		// Yanhua: 0 for Video Rec, 1 for Capture
};

struct ispdma_fb_config
{
	void				*virAddr[4];
	int					phyAddr[4];
	int					size[4];
	int					fbcnt;
	int					burst_read;		// 64, 128 or 256, unit: byte
	int					burst_write;	// 64, 128 or 256, unit: byte
	int					flush_fifo_by_timer;
};

struct ispdma_streaming_config
{
	int					enable_in;
	int					enable_disp;
	int					enable_codec;
	int					enable_fbrx;
	int					enable_fbtx;
};

struct ispdma_buffer
{
	int					port;		// must be ISP_OUTPUT_PORT_***

	int					format;
	int					w;
	int					h;
	int					pitch[3];

	void				*virAddr[3];
	int					phyAddr[3];

	int					user_index;
	void				*user_data;

	unsigned int		tick;
	int					errcnt;
};

struct ispdma_pollinfo
{
	int					timeout;	// unit: ms
	int					request;

	int					result;
};

struct ispdma_ipcwait
{
	int					timeout;	// unit: ms

	unsigned int		tick;
};

struct ispdma_statistic
{
	int					disp_eof_cnt;
	int					codec_eof_cnt;
	int					band_eof_cnt;
	int					disp_ovrwt_cnt;
	int					codec_ovrwt_cnt;
	int					disp_drop_cnt;
	int					codec_drop_cnt;
	int					disp_cpudrop_cnt;
	int					codec_cpudrop_cnt;
};

#endif /* __INC_MACH_DXOISP_H */
