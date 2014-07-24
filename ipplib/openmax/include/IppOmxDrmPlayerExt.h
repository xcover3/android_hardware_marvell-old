
#ifndef _IppOmxDrmPlayerExt_H_
#define _IppOmxDrmPlayerExt_H_

#define DRM_INDEX_CONFIG_LICENSE_CHALLENGE "DRM.index.config.licensechallenge"
#define DRM_INDEX_CONFIG_LICENSE_RESPONSE "DRM.index.config.licenseresponse"
#define DRM_INDEX_CONFIG_DELETE_LICENSE "DRM.index.config.deletelicense"
#define DRM_BUFFER_HEADER_EXTRADATA_INITIALIZATION_VECTOR "DRM.buffer.header.extradata.initializationvector"
#define DRM_BUFFER_HEADER_EXTRADATA_ENCRYPTION_OFFSET "DRM.buffer.header.extradata.encryptionoffset"
#define DRM_BUFFER_HEADER_EXTRADATA_ENCRYPTION_METADATA "DRM.buffer.header.extradata.encryptionmetadata"
#define DRM_INDEX_SET_VIDEO_RENDERER "DRM.index.set.video.renderer"
#define DRM_INDEX_SET_SURFACE "DRM.index.config.setsurface"

typedef enum OMX_DRMPLAYEXT_INDEXTYPE
{
    //other vender extension start at 0x7f000000
    OMX_DRMPLAYEXT_IndexChallenge = 0x7f100001,
    OMX_DRMPLAYEXT_IndexResponse = 0x7f100002,
    OMX_DRMPLAYEXT_IndexDelete = 0x7f100003,

    //extra data type
    OMX_DRMPLAYEXT_ExtraDataIVData = 0x7f100004,
    OMX_DRMPLAYEXT_ExtraDataEncryptionOffset = 0x7f00005,
    OMX_DRMPLAYEXT_ExtraDataEncryptionMetadata = 0x7f00006,

    OMX_DRMPLAYEXT_IndexSetVideoRenderer = 0x7f000007,
    OMX_DRMPLAYEXT_IndexSetSurface = 0x7f000008,
} OMX_DRMPLAYEXT_INDEXTYPE;


typedef struct OMX_DRMPLAYEXT_SETVIDEORENDERERTYPE
{
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;
    OMX_PTR         pVideoRenderer;
} OMX_DRMPLAYEXT_SETVIDEORENDERERTYPE;

typedef struct DRM_CONFIG_SET_SURFACE{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 displayWidth;               // display width coordinate of top left corner of display area
    OMX_U32 displayHeight;              // display height coordinate of top left corner of display area
    OMX_U32 displayX;                   // display X coordinate of top left corner of display area
    OMX_U32 displayY;                   // display Y coordinate of top left corner of display area
    OMX_U32 componentNameSize;          // (actual data size send from IL component to IL Client)
    OMX_U8  componentName[1];           // Variable length array of component name(send from IL client to IL component)
}DRM_CONFIG_SET_SURFACE;
int DrmPlaySetVideoRenderer(void *pHandle);

#endif /* _IppOmxDrmPlayerExt_H_ */
