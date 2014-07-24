/******************************************************************************
//(C) Copyright [2010 - 2011] Marvell International Ltd.
//All Rights Reserved
******************************************************************************/


#ifndef __CAM_SOCISP_MEDIA_H__
#define __CAM_SOCISP_MEDIA_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/media.h>

#if defined( BUILD_OPTION_DEBUG_DUMP_V4L2_CALLING )
#include <sys/poll.h>

int mc_dbg_ioctl( int device, int cmd, void* data,
                  const char *str_device, const char *str_cmd, const char *str_data,
                  const char *file, int line );
int mc_dbg_open( const char *dev, int flag, const char *str_dev, const char *str_flag, const char *file, int line );
int mc_dbg_close( int dev, const char *str_dev, const char *file, int line );
int mc_dbg_poll( struct pollfd *ufds, unsigned int nfds, int timeout,
                 const char *str_ufds, const char *str_nfds, const char *str_timeout, const char *file, int line );

#define mc_open(a, b)          mc_dbg_open( a, b, a, #b, __FILE__, __LINE__ )
#define mc_close(a)            mc_dbg_close( a, #a, __FILE__, __LINE__ )
#define mc_ioctl(a, b, c)      mc_dbg_ioctl( a, b, c, #a, #b, #c, __FILE__, __LINE__ )
#define mc_poll(a, b, c)       mc_dbg_poll( a, b, c, #a, #b, #c, __FILE__, __LINE__ )

#else
#define mc_open                open
#define mc_close               close
#define mc_ioctl               ioctl
#define mc_poll                poll
#endif

#define MAX_ENTITY_COUNT        15
#define MAX_LINK_COUNT          4
#define MAX_PAD_COUNT           5


typedef struct _cam_mediapad
{
        struct _cam_mediaentity  *pstEntity;
        unsigned int             uiIndex;
        unsigned int             uiFlag;
        unsigned int             uiPadding[3];
} _CAM_MediaPad;

typedef struct _cam_medialink
{
	_CAM_MediaPad            *pstSrcPad;
	_CAM_MediaPad            *pstDstPad;
	struct _cam_medialink    *pstTwin;
	unsigned int             uiFlags;
	unsigned int             uiPadding[3];
} _CAM_MediaLink;

typedef struct _cam_mediaentity
{
	struct media_entity_desc stInfo;
	_CAM_MediaPad            astPads[MAX_PAD_COUNT];
	_CAM_MediaLink           astLinks[MAX_LINK_COUNT];
	unsigned int             uiMaxLinks;
	unsigned int             uiNumLinks;

	char                     sDevName[32];
	int                      fd;
	unsigned int             uiPadding[6];
} _CAM_MediaEntity;

typedef struct _cam_mediadevice
{
	int                      fd;
	_CAM_MediaEntity         astEntities[MAX_ENTITY_COUNT];
	unsigned int             uiEntityCount;
	unsigned int             uiPadding[6];
} _CAM_MediaDevice;

/**
 * @brief Open a media device.
 * @param name - name (including path) of the device node.
 * @param verbose - whether to print verbose information on the standard output.
 *
 * Open the media device referenced by @a name and enumerate entities, pads and
 * links.
 *
 * @return A pointer to a newly allocated _CAM_MediaDevice structure instance on
 * success and NULL on failure. The returned pointer must be freed with
 * media_close when the device isn't needed anymore.
 */
_CAM_MediaDevice *media_open( const char *sName );

/**
 * @brief Close a media device.
 * @param media - device instance.
 *
 * Close the @a media device instance and free allocated resources. Access to the
 * device instance is forbidden after this function returns.
 */
void media_close( _CAM_MediaDevice *pstMedia );

/**
 * @brief Find an entity by its name.
 * @param media - media device.
 * @param name - entity name.
 * @param length - size of @a name.
 *
 * Search for an entity with a name equal to @a name.
 *
 * @return A pointer to the entity if found, or NULL otherwise.
 */
_CAM_MediaEntity *media_get_entity_by_name( _CAM_MediaDevice *pstMedia,
                                            const char *sName, size_t stLength );

/**
 * @brief Find an entity by its ID.
 * @param media - media device.
 * @param id - entity ID.
 *
 * Search for an entity with an ID equal to @a id.
 *
 * @return A pointer to the entity if found, or NULL otherwise.
 */
_CAM_MediaEntity *media_get_entity_by_id( _CAM_MediaDevice *pstMedia,
                                          unsigned int     uiID );

/**
 * @brief Configure a link.
 * @param media - media device.
 * @param source - source pad at the link origin.
 * @param sink - sink pad at the link target.
 * @param flags - configuration flags.
 *
 * Locate the link between @a source and @a sink, and configure it by applying
 * the new @a flags.
 *
 * Only the MEDIA_LINK_FLAG_ENABLED flag is writable.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int media_setup_link( _CAM_MediaDevice *pstMedia,
                      _CAM_MediaPad    *pstSrcPad,
                      _CAM_MediaPad    *pstDstPad,
                      unsigned int     uiFlag );

/**
 * @brief Reset all links to the disabled state.
 * @param media - media device.
 *
 * Disable all links in the media device. This function is usually used after
 * opening a media device to reset all links to a known state.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int media_reset_links( _CAM_MediaDevice *pstMedia );

/**
 * @brief open all entities.
 *
 * open all entities controlled by media controller
 *
 * @return 0 on success, or a negative error code on failure.
 */
int media_open_entities( int fd, _CAM_MediaEntity astEntities[MAX_ENTITY_COUNT], unsigned int uiMaxEntityCount, unsigned int *puiNumEntity );

/**
 * @brief close all entities.
 *
 * close all entities opened by media controller
 */
void media_close_entities( _CAM_MediaEntity astEntities[MAX_ENTITY_COUNT], unsigned int uiEntityCount );

#ifdef __cplusplus
}
#endif

#endif
