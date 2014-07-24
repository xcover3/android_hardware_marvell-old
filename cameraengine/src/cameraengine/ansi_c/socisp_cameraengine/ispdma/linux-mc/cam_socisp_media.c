/*******************************************************************************
//(C) Copyright [2010 - 2011] Marvell International Ltd.
//All Rights Reserved
*******************************************************************************/


#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "cam_socisp_media.h"
#include "CameraEngine.h"
#include "cam_log.h"

#include <linux/media.h>
#include <linux/v4l2-subdev.h>
#include "mvisp.h"
#include "CameraOSAL.h"
#include <sys/poll.h>

CAM_Int32s mc_dbg_ioctl( CAM_Int32s device, CAM_Int32s cmd, void *data,
                         const char *str_device, const char *str_cmd, const char *str_data,
                         const char *file, CAM_Int32s line )
{
	CAM_Int32s ret  = 0;
	CAM_Tick   t1   = 0;
	int        i    = 0;

	CELOG( "%s - %d:\n ioctl(%s, %s, %s)\n", file, line, str_device, str_cmd, str_data );
	CELOG( "%s = %d\n", str_device, device );
	CELOG( "%s = 0x%x\n", str_data, (CAM_Int32u)data );

	switch ( cmd )
	{
	case MEDIA_IOC_SETUP_LINK:
		CELOG( "*(struct media_link_desc*)(%s)          = {\n", str_data );
		CELOG( "\t.source.entity                         = %p, \n", (void*)(((struct media_link_desc*)data)->source.entity) );
		CELOG( "\t.source.index                          = %d, \n", ((struct media_link_desc*)data)->source.index           );
		CELOG( "\t.source.flags                          = %d, \n", ((struct media_link_desc*)data)->source.flags           );
		CELOG( "\t.sink.entity                           = %p, \n", (void*)(((struct media_link_desc*)data)->sink.entity)   );
		CELOG( "\t.sink.index                            = %d, \n", ((struct media_link_desc*)data)->sink.index             );
		CELOG( "\t.sink.flags                            = %d, \n", ((struct media_link_desc*)data)->sink.flags             );
		CELOG( "\t.flags                                 = %d, \n", ((struct media_link_desc*)data)->flags                  );
		break;

	case VIDIOC_SUBDEV_S_FMT:
		CELOG( "*(struct v4l2_subdev_format*)(%s)       = {\n", str_data );
		CELOG( "\t.which                                = %d, \n", ((struct v4l2_subdev_format*)data)->which            );
		CELOG( "\t.pad                                  = %d, \n", ((struct v4l2_subdev_format*)data)->pad              );
		CELOG( "\t.format.width                         = %d, \n", ((struct v4l2_subdev_format*)data)->format.width     );
		CELOG( "\t.format.height                        = %d, \n", ((struct v4l2_subdev_format*)data)->format.height    );
		CELOG( "\t.format.code                          = %d, \n", ((struct v4l2_subdev_format*)data)->format.code      );
		CELOG( "\t.format.field                         = %d, \n", ((struct v4l2_subdev_format*)data)->format.field     );
		CELOG( "\t.format.colorspace                    = %d, \n", ((struct v4l2_subdev_format*)data)->format.colorspace);
		break;

	case VIDIOC_PRIVATE_DXOIPC_WAIT_IPC:
		CELOG( "Before VIDIOC_PRIVATE_DXOIPC_WAIT_IPC\n" );
		CELOG( "*(struct v4l2_dxoipc_ipcwait*)(%s)      = {\n", str_data );
		CELOG( "\t.tickinfo.usec                        = %ld,  \n", ((struct v4l2_dxoipc_ipcwait*)data)->tickinfo.sec    );
		CELOG( "\t.tickinfo.delta                       = %ld,  \n", ((struct v4l2_dxoipc_ipcwait*)data)->tickinfo.usec   );
		CELOG( "\t.tickinfo.sec                         = %ld,  \n", ((struct v4l2_dxoipc_ipcwait*)data)->tickinfo.delta  );
		CELOG( "\t.timeout                              = %u,   \n", ((struct v4l2_dxoipc_ipcwait*)data)->timeout         );
		break;

	case VIDIOC_PRIVATE_DXOIPC_SET_STREAM:
		CELOG( "*(struct v4l2_dxoipc_streaming_config*)(%s)  = {\n", str_data );
		CELOG( "\t.enable_in                            = %d, \n", ((struct v4l2_dxoipc_streaming_config*)data)->enable_in   );
		CELOG( "\t.enable_disp                          = %d, \n", ((struct v4l2_dxoipc_streaming_config*)data)->enable_disp );
		CELOG( "\t.enable_codec                         = %d, \n", ((struct v4l2_dxoipc_streaming_config*)data)->enable_codec);
		CELOG( "\t.enable_fbrx                          = %d, \n", ((struct v4l2_dxoipc_streaming_config*)data)->enable_fbrx );
		CELOG( "\t.enable_fbtx                          = %d, \n", ((struct v4l2_dxoipc_streaming_config*)data)->enable_fbtx );
		break;

	case VIDIOC_STREAMOFF:
		CELOG( "*(enum v4l2_buf_type*)(%s)              = %d\n", str_data, *(enum v4l2_buf_type*)data );
		break;

	case VIDIOC_STREAMON:
		CELOG( "*(enum v4l2_buf_type*)(%s)              = %d\n", str_data, *(enum v4l2_buf_type*)data );
		break;

	case VIDIOC_S_FMT:
		CELOG( "*(struct v4l2_format*)(%s)              = {\n", str_data );
		CELOG( "\t.type                                 = %d, \n", ((struct v4l2_format*)data)->type                );
		CELOG( "\t.fmt.pix.width                        = %d, \n", ((struct v4l2_format*)data)->fmt.pix.width       );
		CELOG( "\t.fmt.pix.height                       = %d, \n", ((struct v4l2_format*)data)->fmt.pix.height      );
		CELOG( "\t.fmt.pix.pixelformat                  = %d, \n", ((struct v4l2_format*)data)->fmt.pix.pixelformat );
		CELOG( "\t.fmt.pix.colorspace                   = %d, \n", ((struct v4l2_format*)data)->fmt.pix.colorspace  );
		CELOG( "\t.fmt.pix.bytesperline                 = %d, \n", ((struct v4l2_format*)data)->fmt.pix.bytesperline);
		CELOG( "\t.fmt.pix.sizeimage                    = %d, \n", ((struct v4l2_format*)data)->fmt.pix.sizeimage   );
		break;

	case VIDIOC_PRIVATE_DXOIPC_CONFIG_CODEC:
		CELOG( "*(struct v4l2_dxoipc_config_codec*)(%s) = {\n", str_data );
		CELOG( "\t.dma_burst_size                       = %d, \n", ((struct v4l2_dxoipc_config_codec*)data)->dma_burst_size );
		CELOG( "\t.vbnum                                = %d, \n", ((struct v4l2_dxoipc_config_codec*)data)->vbnum          );
		CELOG( "\t.vbsize                               = %d, \n", ((struct v4l2_dxoipc_config_codec*)data)->vbsize         );
		break;

	case VIDIOC_REQBUFS:
		CELOG( "*(struct v4l2_requestbuffers*)(%s)      = {\n", str_data );
		CELOG( "\t.count                                = %d, \n", ((struct v4l2_requestbuffers*)data)->count   );
		CELOG( "\t.type                                 = %d, \n", ((struct v4l2_requestbuffers*)data)->type    );
		CELOG( "\t.memory                               = %d, \n", ((struct v4l2_requestbuffers*)data)->memory  );
		break;

	case VIDIOC_PRIVATE_DXOIPC_SET_FB:
		CELOG( "*(struct v4l2_dxoipc_set_fb*)(%s)       = {\n", str_data );
		CELOG( "\t.burst_read                           = %d, \n", ((struct v4l2_dxoipc_set_fb*)data)->burst_read   );
		CELOG( "\t.burst_write                          = %d, \n", ((struct v4l2_dxoipc_set_fb*)data)->burst_write  );
		CELOG( "\t.fbcnt                                = %d, \n", ((struct v4l2_dxoipc_set_fb*)data)->fbcnt        );
		for ( i = 0; i < ((struct v4l2_dxoipc_set_fb*)data)->fbcnt; i++ )
		{
			CELOG( "\t.virAddr[%d]                              = %p, \n", i, ((struct v4l2_dxoipc_set_fb*)data)->virAddr[i]   );
			CELOG( "\t.phyAddr[%d]                              = %d, \n", i, ((struct v4l2_dxoipc_set_fb*)data)->phyAddr[i]   );
			CELOG( "\t.size[%d]                                 = %d, \n", i, ((struct v4l2_dxoipc_set_fb*)data)->size[i]      );
		}
		break;

	case VIDIOC_QBUF:
		CELOG( "*(struct v4l2_buffer*)(%s)              = {\n", str_data );
		CELOG( "\t.memory                               = %d,  \n", ((struct v4l2_buffer*)data)->memory     );
		CELOG( "\t.type                                 = %d,  \n", ((struct v4l2_buffer*)data)->type       );
		CELOG( "\t.m.userptr                            = %lu, \n", ((struct v4l2_buffer*)data)->m.userptr  );
		CELOG( "\t.index                                = %d,  \n", ((struct v4l2_buffer*)data)->index      );
		CELOG( "\t.reserved                             = %d,  \n", ((struct v4l2_buffer*)data)->reserved   );
		break;

	case VIDIOC_DQBUF:
		CELOG( "Before VIDIOC_DQBUF\n" );
		CELOG( "*(struct v4l2_buffer*)(%s)      = {\n", str_data );
		CELOG( "\t.memory                               = %d,  \n", ((struct v4l2_buffer*)data)->memory     );
		CELOG( "\t.type                                 = %d,  \n", ((struct v4l2_buffer*)data)->type       );
		CELOG( "\t.m.userptr                            = %lu, \n", ((struct v4l2_buffer*)data)->m.userptr );
		CELOG( "\t.index                                = %d,  \n", ((struct v4l2_buffer*)data)->index      );
		CELOG( "\t.reserved                             = %d,  \n", ((struct v4l2_buffer*)data)->reserved   );
		break;

	default:
		break;
	}
	t1 = -CAM_TimeGetTickCount();


	ret = ioctl( device, cmd, data );

	t1 += CAM_TimeGetTickCount();

	CELOG( "Return value: %d\n", ret );
	if ( ret != 0 )
	{
		CELOG( "Error code: %s\n", strerror( errno ) );
	}
	switch ( cmd )
	{
	case VIDIOC_DQBUF:
		CELOG( "After VIDIOC_DQBUF\n" );
		CELOG( "*(struct v4l2_buffer*)(%s)              = {\n", str_data );
		CELOG( "\t.memory                               = %d,   \n", ((struct v4l2_buffer*)data)->memory    );
		CELOG( "\t.type                                 = %d,   \n", ((struct v4l2_buffer*)data)->type      );
		CELOG( "\t.m.userptr                            = %lu,  \n", ((struct v4l2_buffer*)data)->m.userptr );
		CELOG( "\t.index                                = %d,   \n", ((struct v4l2_buffer*)data)->index     );
		CELOG( "\t.reserved                             = %d,   \n", ((struct v4l2_buffer*)data)->reserved  );
		break;

	case VIDIOC_PRIVATE_DXOIPC_WAIT_IPC:
		CELOG( "After VIDIOC_PRIVATE_DXOIPC_WAIT_IPC\n" );
		CELOG( "*(struct v4l2_dxoipc_ipcwait*)(%s)      = {\n", str_data );
		CELOG( "\t.tickinfo.usec                        = %ld,  \n", ((struct v4l2_dxoipc_ipcwait*)data)->tickinfo.sec    );
		CELOG( "\t.tickinfo.delta                       = %ld,  \n", ((struct v4l2_dxoipc_ipcwait*)data)->tickinfo.usec   );
		CELOG( "\t.tickinfo.sec                         = %ld,  \n", ((struct v4l2_dxoipc_ipcwait*)data)->tickinfo.delta  );
		CELOG( "\t.timeout                              = %u,   \n", ((struct v4l2_dxoipc_ipcwait*)data)->timeout         );
		break;

	default:
		break;
	}
	CELOG( "Perf: ioctl elapse: %.2f ms\n", t1 / 1000.0 );
	CELOG( "\n\n" );

	return ret;
}

CAM_Int32s mc_dbg_open( const char *dev, CAM_Int32s flag, const char *str_dev,
                        const char *str_flag, const char *file, CAM_Int32s line )
{
	CAM_Int32s ret = 0;
	CAM_Tick   t1  = 0;

	CELOG( "%s - %d:\n open(\"%s\", %s)\n", file, line, str_dev, str_flag );
	t1 = -CAM_TimeGetTickCount();

	ret = open( dev, flag );

	t1 += CAM_TimeGetTickCount();
	CELOG( "Return value: %d\n", ret );
	if ( ret != 0 )
	{
		CELOG( "Error code: %s\n", strerror( errno ) );
	}

	CELOG( "Perf: open elapse: %.2f ms\n", t1 / 1000.0 );
	CELOG( "\n\n" );

	return ret;
}

CAM_Int32s mc_dbg_close( CAM_Int32s dev, const char *str_dev, const char *file, CAM_Int32s line )
{
	CAM_Int32s ret = 0;
	CAM_Tick   t1  = 0;

	CELOG( "%s - %d:\n close(%s)\n", file, line, str_dev );
	t1 = -CAM_TimeGetTickCount();

	ret = close( dev );

	t1 += CAM_TimeGetTickCount();
	CELOG( "Return value: %d\n", ret );
	if ( ret != 0 )
	{
		CELOG( "Error code: %s\n", strerror( errno ) );
	}

	CELOG( "Elapse: %.2f ms\n", t1 / 1000.0 );
	CELOG( "\n\n" );

	return ret;
}

int mc_dbg_poll( struct pollfd *ufds, unsigned int nfds, int timeout,
                 const char *str_ufds, const char *str_nfds, const char *str_timeout,
                 const char *file, CAM_Int32s line )
{
	CAM_Int32s ret = 0;
	CAM_Tick   t1  = 0;

	CELOG( "%s - %d:\n poll(%s, %s, %s) \n", file, line, str_ufds, str_nfds, str_timeout );
	t1 = -CAM_TimeGetTickCount();

	ret = poll( ufds, nfds, timeout );

	t1 += CAM_TimeGetTickCount();
	CELOG( "Return value: %d\n", ret );
	if ( ret != 0 )
	{
		CELOG( "Error code: %s\n", strerror( errno ) );
	}
	CELOG( "Elapse: %.2f ms\n", t1 / 1000.0 );
	CELOG( "\n\n" );

	return ret;
}

static inline unsigned int media_entity_type( _CAM_MediaEntity *pstEntity )
{
	return pstEntity->stInfo.type & MEDIA_ENT_TYPE_MASK;
}

_CAM_MediaEntity *media_get_entity_by_name( _CAM_MediaDevice *pstMedia,
                                            const char *sName, size_t stLength )
{
	unsigned int i;

	for ( i = 0; i < pstMedia->uiEntityCount; i++ )
	{
		_CAM_MediaEntity *pstEntity = &pstMedia->astEntities[i];

		if ( strncmp( pstEntity->stInfo.name, sName, stLength ) == 0 )
		{
			return pstEntity;
		}
	}

	return NULL;
}

_CAM_MediaEntity *media_get_entity_by_id( _CAM_MediaDevice *pstMedia,
                                          unsigned int     uiID )
{
	unsigned int i;

	for ( i = 0; i < pstMedia->uiEntityCount; i++ )
	{
		_CAM_MediaEntity *pstEntity = &pstMedia->astEntities[i];

		if ( pstEntity->stInfo.id == uiID )
		{
			return pstEntity;
		}
	}

	return NULL;
}

int media_setup_link( _CAM_MediaDevice *pstMedia,
                      _CAM_MediaPad    *pstSrcPad,
                      _CAM_MediaPad    *pstDstPad,
                      unsigned int     uiFlags )
{
	_CAM_MediaLink          *pstLink;
	struct media_link_desc  stULink;
	unsigned int            i;
	int                     ret;

	for ( i = 0; i < pstSrcPad->pstEntity->uiNumLinks; i++ )
	{
		pstLink = &pstSrcPad->pstEntity->astLinks[i];

		if ( pstLink->pstSrcPad->pstEntity == pstSrcPad->pstEntity &&
		     pstLink->pstSrcPad->uiIndex == pstSrcPad->uiIndex &&
		     pstLink->pstDstPad->pstEntity == pstDstPad->pstEntity &&
		     pstLink->pstDstPad->uiIndex == pstDstPad->uiIndex )
		{
			break;
		}
	}

	if ( i == pstSrcPad->pstEntity->uiNumLinks )
	{
		TRACE( CAM_ERROR, "Error:link not found( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	/* source pad */
	stULink.source.entity = pstSrcPad->pstEntity->stInfo.id;
	stULink.source.index  = pstSrcPad->uiIndex;
	stULink.source.flags  = MEDIA_PAD_FL_SOURCE;

	/* sink pad */
	stULink.sink.entity = pstDstPad->pstEntity->stInfo.id;
	stULink.sink.index  = pstDstPad->uiIndex;
	stULink.sink.flags  = MEDIA_PAD_FL_SINK;

	stULink.flags = uiFlags | ( pstLink->uiFlags & MEDIA_LNK_FL_IMMUTABLE );

	ret = mc_ioctl( pstMedia->fd, MEDIA_IOC_SETUP_LINK, &stULink );
	if ( ret < 0 )
	{
		TRACE( CAM_ERROR, "Error: unable to setup link[%s]( %s, %s, %d )\n",
		       strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	pstLink->uiFlags = uiFlags;
	pstLink->pstTwin->uiFlags = uiFlags;
	return 0;
}

int media_reset_links( _CAM_MediaDevice *pstMedia )
{
	unsigned int i, j;
	int          ret;

	for ( i = 0; i < pstMedia->uiEntityCount; i++ )
	{
		_CAM_MediaEntity *pstEntity = &pstMedia->astEntities[i];

		for ( j = 0; j < pstEntity->uiNumLinks; j++ )
		{
			_CAM_MediaLink *pstLink = &pstEntity->astLinks[j];

			if ( pstLink->uiFlags & MEDIA_LNK_FL_IMMUTABLE ||
			     pstLink->pstSrcPad->pstEntity != pstEntity )
			{
				continue;
			}

			ret = media_setup_link( pstMedia, pstLink->pstSrcPad, pstLink->pstDstPad,
			                        pstLink->uiFlags & ~MEDIA_LNK_FL_ENABLED );
			if ( ret < 0 )
			{
				return ret;
			}
		}
	}

	return 0;
}

static int media_enum_links( _CAM_MediaDevice *pstMedia )
{
	unsigned int            uiID;
	int                     ret = 0;
	struct media_links_enum stLinks;

	for ( uiID = 0; uiID < pstMedia->uiEntityCount; uiID++ )
	{
		_CAM_MediaEntity        *pstEntity = &pstMedia->astEntities[uiID];
		unsigned int            i;

		stLinks.entity    = pstEntity->stInfo.id;
		stLinks.pads      = malloc( pstEntity->stInfo.pads * sizeof( struct media_pad_desc ) );
		stLinks.links     = malloc( pstEntity->stInfo.links * sizeof( struct media_link_desc ) );

		if ( NULL == stLinks.pads || NULL == stLinks.links )
		{
			TRACE( CAM_ERROR, "Error: out of memory( %s, %s, %d )\n",
			       __FILE__, __FUNCTION__, __LINE__ );
			goto exit;
		}

		if ( mc_ioctl( pstMedia->fd, MEDIA_IOC_ENUM_LINKS, &stLinks ) < 0 )
		{
			TRACE( CAM_ERROR, "Error: unable to enumerate pads and links[%s]( %s, %s, %d )\n",
			       strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
			goto exit;
		}

		if ( pstEntity->stInfo.pads >= MAX_PAD_COUNT )
		{
			TRACE( CAM_ERROR, "Error: not enough pad slots [%d] > [%d]( %s, %s, %d )\n",
			       pstEntity->stInfo.pads, MAX_PAD_COUNT, __FILE__, __FUNCTION__, __LINE__ );
			goto exit;
		}

		for ( i = 0; i < pstEntity->stInfo.pads; i++ )
		{
			pstEntity->astPads[i].pstEntity = pstEntity;
			pstEntity->astPads[i].uiIndex   = stLinks.pads[i].index;
			pstEntity->astPads[i].uiFlag    = stLinks.pads[i].flags;
		}

		if ( pstEntity->stInfo.links >= MAX_LINK_COUNT )
		{
			TRACE( CAM_ERROR, "Error: not enough link slots [%d] > [%d]( %s, %s, %d )\n",
			       pstEntity->stInfo.links, MAX_LINK_COUNT, __FILE__, __FUNCTION__, __LINE__ );
			goto exit;
		}

		for ( i = 0; i < pstEntity->stInfo.links; i++ )
		{
			struct media_link_desc *pstLinkDesc = &stLinks.links[i];
			_CAM_MediaLink         *pstFwdLink;
			_CAM_MediaLink         *pstBackLink;
			_CAM_MediaEntity       *pstSrcEntity;
			_CAM_MediaEntity       *pstDstEntity;

			pstSrcEntity    = media_get_entity_by_id( pstMedia, pstLinkDesc->source.entity );
			pstDstEntity    = media_get_entity_by_id( pstMedia, pstLinkDesc->sink.entity );

			if ( pstSrcEntity == NULL || pstDstEntity == NULL )
			{
				TRACE( CAM_WARN, "Warning: entity %u link %u from %u/%u to %u/%u is invalid\n",
				       uiID, i, pstLinkDesc->source.entity, pstLinkDesc->source.index,
				       pstLinkDesc->sink.entity, pstLinkDesc->sink.index );
				ret = -1;
			}
			else
			{
				pstFwdLink              = &pstSrcEntity->astLinks[pstSrcEntity->uiNumLinks++];
				pstFwdLink->pstSrcPad   = &pstSrcEntity->astPads[pstLinkDesc->source.index];
				pstFwdLink->pstDstPad   = &pstDstEntity->astPads[pstLinkDesc->sink.index];
				pstFwdLink->uiFlags     = pstLinkDesc->flags;

				pstBackLink             = &pstDstEntity->astLinks[pstDstEntity->uiNumLinks++];
				pstBackLink->pstSrcPad  = &pstSrcEntity->astPads[pstLinkDesc->source.index];
				pstBackLink->pstDstPad  = &pstDstEntity->astPads[pstLinkDesc->sink.index];
				pstBackLink->uiFlags    = pstLinkDesc->flags;

				pstFwdLink->pstTwin     = pstBackLink;
				pstBackLink->pstTwin    = pstFwdLink;
			}
		}

		free( stLinks.pads );
		free( stLinks.links );
	}

	return ret;

exit:
	if ( stLinks.pads != NULL )
	{
		free( stLinks.pads );
	}
	if ( stLinks.links != NULL )
	{
		free( stLinks.links );
	}
	return -1;
}

void media_close_entities( _CAM_MediaEntity astEntities[MAX_ENTITY_COUNT], unsigned int uiEntityCount )
{
	unsigned int i;

	if ( astEntities == NULL )
	{
		return ;
	}

	for ( i = 0; i < uiEntityCount; i++ )
	{
		_CAM_MediaEntity *pstEntity = &astEntities[i];

		if ( pstEntity->fd != -1 )
		{
			mc_close( pstEntity->fd );
			pstEntity->fd = -1;
		}
	}

	return ;
}

int media_open_entities( int fd, _CAM_MediaEntity astEntities[MAX_ENTITY_COUNT], unsigned int uiMaxEntityCount, unsigned int *puiNumEntity )
{
	_CAM_MediaEntity *pstEntity = NULL;
	struct stat      stDevStat;
	char             sDevName[32];
	char             sSysName[32];
	char             sTargetName[1024];
	char             *p;
	unsigned int     uiID, uiEntityCount;
	int              ret;

	if ( astEntities == NULL || puiNumEntity == NULL || fd < 0 )
	{
		TRACE( CAM_ERROR, "Error: bad argument( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	memset( astEntities, 0, uiMaxEntityCount * sizeof( _CAM_MediaEntity ) );

	uiEntityCount = 0;
	for ( uiID = 0; ; uiID = pstEntity->stInfo.id )
	{
		pstEntity               = &astEntities[uiEntityCount];
		pstEntity->fd           = -1;
		pstEntity->stInfo.id    = uiID | MEDIA_ENT_ID_FLAG_NEXT;

		ret = mc_ioctl( fd, MEDIA_IOC_ENUM_ENTITIES, &pstEntity->stInfo );
		if ( ret < 0 )
		{
			if ( errno == EINVAL )
			{
				break;
			}
			TRACE( CAM_ERROR, "Error: enum entity failed, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
			goto exit;
		}
		uiID = pstEntity->stInfo.id;
		uiEntityCount++;

		// Find the corresponding device name
		if ( media_entity_type( pstEntity ) != MEDIA_ENT_T_DEVNODE &&
		     media_entity_type( pstEntity ) != MEDIA_ENT_T_V4L2_SUBDEV )
		{
			continue;
		}

		// Get the link name from sys directory, and read the link
		sprintf( sSysName, "/sys/dev/char/%u:%u", pstEntity->stInfo.v4l.major,
		         pstEntity->stInfo.v4l.minor );
		ret = readlink( sSysName, sTargetName, sizeof( sTargetName ) );
		if ( ret < 0 )
		{
			continue;
		}

		sTargetName[ret] = '\0';
		p = strrchr( sTargetName, '/' );
		if ( p == NULL )
		{
			continue;
		}

		sprintf( sDevName, "/dev/%s", p + 1 );
		ret = stat( sDevName, &stDevStat );
		if ( ret < 0 )
		{
			continue;
		}

		/* Sanity check: udev might have reordered the device nodes.
		 * Make sure the major/minor match. We should really use
		 * libudev.If your rootfs mismatch, this function
                 *fails here.
		 */
		if ( ((unsigned int)major(stDevStat.st_rdev)) == pstEntity->stInfo.v4l.major &&
		     ((unsigned int)minor(stDevStat.st_rdev)) == pstEntity->stInfo.v4l.minor )
		{
			strcpy( pstEntity->sDevName, sDevName );
		}
	}

	if ( uiEntityCount >= uiMaxEntityCount )
	{
		TRACE( CAM_ERROR, "Error: number of entity exceed maximum[%d]( %s, %s, %d )\n", MAX_ENTITY_COUNT, __FILE__, __FUNCTION__, __LINE__ );
		goto exit;
	}

	*puiNumEntity = uiEntityCount;

	return 0;

exit:
	for ( uiID = 0; uiID < uiEntityCount; uiID++ )
	{
		if ( astEntities[uiID].fd != -1 )
		{
			mc_close( astEntities[uiID].fd );
			astEntities[uiID].fd = -1;
		}
	}
	*puiNumEntity = 0;

	return -1;
}

_CAM_MediaDevice *media_open( const char *sName )
{
	_CAM_MediaDevice *pstMedia = NULL;
	int              ret;
	int              i;

	pstMedia = malloc( sizeof( _CAM_MediaDevice ) );
	if ( pstMedia == NULL )
	{
		TRACE( CAM_ERROR, "Error: unable to allocate memory( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		goto exit;
	}
	memset( pstMedia, 0, sizeof( _CAM_MediaDevice ) );

	pstMedia->fd = mc_open( sName, O_RDWR );
	if ( pstMedia->fd < 0 )
	{
		TRACE( CAM_ERROR, "Error: can't open media device[%s]( %s, %s, %d )\n", sName, __FILE__, __FUNCTION__,__LINE__ );
		goto exit;
	}

	ret = media_open_entities( pstMedia->fd, pstMedia->astEntities, MAX_ENTITY_COUNT, &pstMedia->uiEntityCount );
	if ( ret < 0 )
	{
		TRACE( CAM_ERROR, "Error: unable to enumerate entities for device[%s]( %s, %s, %d )\n",
		       sName, __FILE__, __FUNCTION__, __LINE__ );
		goto exit;
	}

	ret = media_enum_links( pstMedia );
	if ( ret < 0 )
	{
		TRACE( CAM_ERROR, "Error: unable to enumerate pads and links for device[%s]( %s, %s, %d )\n",
		       sName, __FILE__, __FUNCTION__, __LINE__ );
		goto exit;
	}

	return pstMedia;

exit:
	media_close_entities( pstMedia->astEntities, pstMedia->uiEntityCount );

	if ( pstMedia->fd != -1)
	{
		mc_close( pstMedia->fd );
	}

	free( pstMedia );

	return NULL;
}

void media_close( _CAM_MediaDevice *pstMedia )
{
	unsigned int i;

	if ( NULL == pstMedia )
	{
		return;
	}

	media_close_entities( pstMedia->astEntities, pstMedia->uiEntityCount );

	if ( pstMedia->fd != -1)
	{
		mc_close( pstMedia->fd );
	}

	free( pstMedia );

	return;
}
