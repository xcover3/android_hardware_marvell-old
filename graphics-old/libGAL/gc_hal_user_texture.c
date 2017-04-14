/****************************************************************************
*
*    Copyright (c) 2005 - 2015 by Vivante Corp.  All rights reserved.
*
*    The material in this file is confidential and contains trade secrets
*    of Vivante Corporation. This is proprietary information owned by
*    Vivante Corporation. No part of this work may be disclosed,
*    reproduced, copied, transmitted, or used in any way for any purpose,
*    without the express written permission of Vivante Corporation.
*
*****************************************************************************/


#include "gc_hal_user_precomp.h"

#if gcdENABLE_3D

#if gcdNULL_DRIVER < 2

/* Zone used for header/footer. */
#define _GC_OBJ_ZONE    gcvZONE_TEXTURE

/******************************************************************************\
|********************************* Structures *********************************|
\******************************************************************************/

typedef struct _gcsMIPMAP *     gcsMIPMAP_PTR;

/* The internal gcsMIPMAP structure. */
struct _gcsMIPMAP
{
    /* User-specified Internal format of texture */
    gctINT                      internalFormat;

    /* Surface format of texture. */
    gceSURF_FORMAT              format;

    /* Width of the mipmap. */
    gctUINT                     width;

    /* Height of the mipmap.  Only used for 2D and 3D textures. */
    gctUINT                     height;

    /* Depth of the mipmap.  Only used for 3D textures. */
    gctUINT                     depth;

    /* Number of faces.  6 for cubic maps. */
    gctUINT                     faces;

    /* Size per slice. */
    gctSIZE_T                   sliceSize;

    /* Horizontal alignment in pixels of the surface. */
    gceSURF_ALIGNMENT           hAlignment;

    /* Surface allocated for mipmap. */
    gcePOOL                     pool;
    gcoSURF                     surface;
    gctPOINTER                  locked;
    gctUINT32                   address;

    /* Next mipmap level. */
    gcsMIPMAP_PTR               next;
};

/* The gcoTEXTURE object. */
struct _gcoTEXTURE
{
    /* The object. */
    gcsOBJECT                   object;

    /* Surface format of texture. */
    gceSURF_FORMAT              format;

    /* Endian hint of texture. */
    gceENDIAN_HINT              endianHint;

    /* Block size for compressed and YUV textures. */
    gctUINT                     blockWidth;
    gctUINT                     blockHeight;

    /* Pointer to head of mipmap chain. */
    gcsMIPMAP_PTR               maps;
    gcsMIPMAP_PTR               tail;
    gcsMIPMAP_PTR               baseLevelMap;

    /* Texture info. */
    gcsTEXTURE                  Info;

    gctINT                      levels;

    gctBOOL                     unsizedDepthTexture;

    /* Texture object type */
    gceTEXTURE_TYPE             type;

    /* Texture validation. */
    gctBOOL                     complete;
    gctINT                      completeMax;
    gctINT                      completeBase;
    gctINT                      completeLevels;

    gctBOOL                     filterable;
};


/******************************************************************************\
|******************************** Support Code ********************************|
\******************************************************************************/

/*******************************************************************************
**
**  _DestroyMaps
**
**  Destroy all gcsMIPMAP structures attached to an gcoTEXTURE object.
**
**  INPUT:
**
**      gcsMIPMAP_PTR MipMap
**          Pointer to the gcsMIPMAP structure that is the head of the linked
**          list.
**
**      gcoOS Os
**          Pointer to an gcoOS object.
**
**  OUTPUT:
**
**      Nothing.
**
**  RETURNS:
**
**      Nothing.
*/
static gceSTATUS _DestroyMaps(
    IN gcsMIPMAP_PTR MipMap,
    IN gcoOS Os
    )
{
    gcsMIPMAP_PTR next;
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("MipMap=0x%x Os=0x%x", MipMap, Os);

    /* Loop while we have valid gcsMIPMAP structures. */
    while (MipMap != gcvNULL)
    {
        /* Get pointer to next gcsMIPMAP structure. */
        next = MipMap->next;

        if (MipMap->locked != gcvNULL)
        {
            /* Unlock MipMap. */
            gcmONERROR(
                gcoSURF_Unlock(MipMap->surface, MipMap->locked));
        }

        if (MipMap->surface != gcvNULL)
        {
            /* Destroy the attached gcoSURF object. */
            gcmONERROR(gcoSURF_Destroy(MipMap->surface));
        }

        /* Free the gcsMIPMAP structure. */
        gcmONERROR(gcmOS_SAFE_FREE(Os, MipMap));

        /* Continue with next mip map. */
        MipMap = next;
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}


/******************************************************************************\
|**************************** gcoTEXTURE Object Code ***************************|
\******************************************************************************/

/*******************************************************************************
**
**  gcoTEXTURE_ConstructEx
**
**  Construct a new gcoTEXTURE object.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object.
**
**      gcvTEXTURE_TYPE Type
**          Texture object type
**
**  OUTPUT:
**
**      gcoTEXTURE * Texture
**          Pointer to a variable receiving the gcoTEXTURE object pointer.
*/
gceSTATUS
gcoTEXTURE_ConstructEx(
    IN gcoHAL Hal,
    IN gceTEXTURE_TYPE Type,
    OUT gcoTEXTURE * Texture
    )
{
    gcoTEXTURE texture;
    gceSTATUS status;
    gctPOINTER pointer = gcvNULL;

    gcmHEADER();

    /* Veriffy the arguments. */
    gcmDEBUG_VERIFY_ARGUMENT(Texture != gcvNULL);

    /* Allocate memory for the gcoTEXTURE object. */
    status = gcoOS_Allocate(gcvNULL,
                            sizeof(struct _gcoTEXTURE),
                            &pointer);

    if (gcmIS_ERROR(status))
    {
        /* Error. */
        gcmFOOTER();
        return status;
    }

    texture = pointer;

    /* Initialize the gcoTEXTURE object. */
    texture->object.type = gcvOBJ_TEXTURE;

    /* Unknown texture format. */
    texture->format = gcvSURF_UNKNOWN;

    /* Default endian hint */
    texture->endianHint  = gcvENDIAN_NO_SWAP;

    /* No mip map allocated yet. */
    texture->maps           = gcvNULL;
    texture->tail           = gcvNULL;
    texture->baseLevelMap   = gcvNULL;
    texture->levels         = 0;
    texture->complete       = gcvFALSE;
    texture->completeMax    = -1;
    texture->completeBase   = 0x7fffffff;
    texture->completeLevels = 0;
    texture->unsizedDepthTexture = gcvFALSE;
    texture->type           = Type;

    /* Return the gcoTEXTURE object pointer. */
    *Texture = texture;

    gcmPROFILE_GC(GLTEXTURE_OBJECT, 1);

    /* Success. */
    gcmFOOTER_ARG("*Texture=0x%x", *Texture);
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoTEXTURE_Construct [Legacy]
**
**  Construct a new gcoTEXTURE object with unknown type
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object.
**
**
**  OUTPUT:
**
**      gcoTEXTURE * Texture
**          Pointer to a variable receiving the gcoTEXTURE object pointer.
*/
gceSTATUS
gcoTEXTURE_Construct(
    IN gcoHAL Hal,
    OUT gcoTEXTURE * Texture
    )
{
    return gcoTEXTURE_ConstructEx(Hal, gcvTEXTURE_UNKNOWN, Texture);
}


/*******************************************************************************
**
**  gcoTEXTURE_ConstructSized
**
**  Construct a new sized gcoTEXTURE object.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object.
**
**      gceSURF_FORMAT Format
**          Format of the requested texture.
**
**      gctUINT Width
**          Requested width of the texture.
**
**      gctUINT Height
**          Requested height of the texture.
**
**      gctUINT Depth
**          Requested depth of the texture.  If 'Depth' is not 0, the texture
**          is a volume map.
**
**      gctUINT Faces
**          Requested faces of the texture.  If 'Faces' is not 0, the texture
**          is a cubic map.
**
**      gctUINT MipMapCount
**          Number of requested mip maps for the texture.  'MipMapCount' must be
**          at least 1.
**
**      gcePOOL Pool
**          Pool to allocate tetxure surfaces from.
**
**  OUTPUT:
**
**      gcoTEXTURE * Texture
**          Pointer to a variable receiving the gcoTEXTURE object pointer.
*/
gceSTATUS
gcoTEXTURE_ConstructSized(
    IN gcoHAL Hal,
    IN gceSURF_FORMAT Format,
 /* IN gceTILING Tiling, */
    IN gctUINT Width,
    IN gctUINT Height,
    IN gctUINT Depth,
    IN gctUINT Faces,
    IN gctUINT MipMapCount,
    IN gcePOOL Pool,
    OUT gcoTEXTURE * Texture
    )
{
    gcoTEXTURE texture;
    gceSTATUS status;
    gcsMIPMAP_PTR map = gcvNULL;
    gctUINT level;
    gctPOINTER pointer = gcvNULL;

    gcmHEADER_ARG("Format=%d Width=%d Height=%d Depth=%d "
                    "Faces=%d MipMapCount=%d Pool=%d",
                    Format, Width, Height, Depth,
                    Faces, MipMapCount, Pool);

    /* Veriffy the arguments. */
    gcmDEBUG_VERIFY_ARGUMENT(MipMapCount > 0);
    gcmDEBUG_VERIFY_ARGUMENT(Texture != gcvNULL);

    /* Allocate memory for the gcoTEXTURE object. */
    status = gcoOS_Allocate(gcvNULL,
                            sizeof(struct _gcoTEXTURE),
                            &pointer);

    if (gcmIS_ERROR(status))
    {
        /* Error. */
        gcmFOOTER();
        return status;
    }

    texture = pointer;

    /* Initialize the gcoTEXTURE object. */
    texture->object.type = gcvOBJ_TEXTURE;

    /* Format of texture. */
    texture->format = Format;
    texture->type   = gcvTEXTURE_2D;

    /* Default endian hint */
    texture->endianHint = gcvENDIAN_NO_SWAP;

    /* No mip map allocated yet. */
    texture->maps           = gcvNULL;
    texture->tail           = gcvNULL;
    texture->baseLevelMap   = gcvNULL;
    texture->levels         = 0;
    texture->complete       = gcvFALSE;
    texture->completeMax    = -1;
    texture->completeBase   = 0x7fffffff;
    texture->completeLevels = 0;

    /* Client driver may sent depth = 0 */
    Depth = Depth > 0 ? Depth : 1;

    /* Loop through all mip map. */
    for (level = 0; MipMapCount-- > 0; ++level)
    {
        /* Query texture support */
        status = gcoHARDWARE_QueryTexture(gcvNULL,
                                          Format,
                                          gcvTILED,
                                          level,
                                          Width,
                                          Height,
                                          Depth,
                                          Faces,
                                          &texture->blockWidth,
                                          &texture->blockHeight);

        if (status < 0)
        {
            /* Error. */
            break;
        }

        if (status == gcvSTATUS_OK)
        {
            /* Create an gcsMIPMAP structure. */
            status = gcoOS_Allocate(gcvNULL,
                                   sizeof(struct _gcsMIPMAP),
                                   &pointer);

            if (status < 0)
            {
                /* Error. */
                break;
            }

            map = pointer;

            /* Initialize the gcsMIPMAP structure. */
            map->format     = Format;
            map->width      = Width;
            map->height     = Height;
            map->depth      = Depth;
            map->faces      = Faces;
            map->pool       = Pool;
            map->locked     = gcvNULL;
            map->next       = gcvNULL;

            /* Construct the surface. */
            status = gcoSURF_Construct(gcvNULL,
                                      gcmALIGN_NP2(Width, texture->blockWidth),
                                      gcmALIGN_NP2(Height, texture->blockHeight),
                                      gcmMAX(gcmMAX(Depth, Faces), 1),
                                      gcvSURF_TEXTURE,
                                      Format,
                                      Pool,
                                      &map->surface);

            if (status < 0)
            {
                /* Roll back. */
                gcmVERIFY_OK(gcmOS_SAFE_FREE(gcvNULL, map));

                /* Error. */
                break;
            }

            map->sliceSize = map->surface->info.sliceSize;

            /* Query texture horizontal alignment. */
            status = gcoHARDWARE_QueryTileAlignment(gcvNULL,
                                                    gcvSURF_TEXTURE,
                                                    map->format,
                                                    &map->hAlignment,
                                                    gcvNULL);
            if (status < 0)
            {
                /* Roll back. */
                gcmVERIFY_OK(gcmOS_SAFE_FREE(gcvNULL, map));

                /* Error. */
                break;
            }

            /* Append the gcsMIPMAP structure to the end of the chain. */
            if (texture->maps == gcvNULL)
            {
                texture->maps = map;
                texture->tail = map;
            }
            else
            {
                texture->tail->next = map;
                texture->tail       = map;
            }
        }

        /* Increment number of levels. */
        texture->levels++;
        texture->completeLevels++;

        /* Move to next mip map level. */
        Width  = gcmMAX(Width  / 2, 1);
        Height = gcmMAX(Height / 2, 1);
        Depth  = gcmMAX(Depth  / 2, 1);
    }

    if ( (status == gcvSTATUS_OK) && (texture->maps == gcvNULL) )
    {
        /* No maps created, so texture is not supported. */
        status = gcvSTATUS_NOT_SUPPORTED;
    }

    if (status < 0)
    {
        /* Roll back. */
        gcmVERIFY_OK(_DestroyMaps(texture->maps, gcvNULL));

        texture->object.type = gcvOBJ_UNKNOWN;

        gcmVERIFY_OK(gcmOS_SAFE_FREE(gcvNULL, texture));

        /* Error. */
        gcmFOOTER();
        return status;
    }

    /* Set texture completeness. */
    gcmASSERT(texture->levels > 0);
    gcmASSERT(texture->levels == texture->completeLevels);
    texture->complete    = gcvTRUE;
    texture->completeMax = texture->completeLevels - 1;
    texture->completeBase= 0;
    texture->baseLevelMap = texture->maps;

    /* Set texture filterable property. */
    texture->filterable = !map->surface->info.formatInfo.fakedFormat ||
                          map->surface->info.paddingFormat;

    /* Return the gcoTEXTURE object pointer. */
    *Texture = texture;
    gcmPROFILE_GC(GLTEXTURE_OBJECT, 1);

    /* Success. */
    gcmFOOTER_ARG("*Texture=0x%x", *Texture);
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoTEXTURE_Destroy
**
**  Destroy an gcoTEXTURE object.
**
**  INPUT:
**
**      gcoTEXTURE Texture
**          Pointer to an gcoTEXTURE object.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoTEXTURE_Destroy(
    IN gcoTEXTURE Texture
    )
{
    gcmHEADER_ARG("Texture=0x%x", Texture);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Texture, gcvOBJ_TEXTURE);

    gcmPROFILE_GC(GLTEXTURE_OBJECT, -1);

    /* Free all maps. */
    gcmVERIFY_OK(_DestroyMaps(Texture->maps, gcvNULL));

    /* Mark the gcoTEXTURE object as unknown. */
    Texture->object.type = gcvOBJ_UNKNOWN;

    /* Free the gcoTEXTURE object. */
    gcmVERIFY_OK(gcmOS_SAFE_FREE(gcvNULL, Texture));

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

static gctBOOL
_UseAccurateUpload(
    gceSURF_FORMAT SrcFmt, gcsSURF_INFO_PTR DstSurfInfo
    )
{
    gcsSURF_FORMAT_INFO_PTR srcFmtInfo = gcvNULL;
    gcsSURF_FORMAT_INFO_PTR DstFmtInfo = &DstSurfInfo->formatInfo;

    gcoSURF_QueryFormat(SrcFmt, &srcFmtInfo);

    if (srcFmtInfo && DstFmtInfo)
    {
        /*
        ** 1: Fake format case
        */
        if (srcFmtInfo->fakedFormat || DstFmtInfo->fakedFormat)
        {
            return gcvTRUE;
        }
        /*
        * 2: If tiling of DstSurf is gcvMULTI_SUPERTILED, use accurateUpload to upload data.
        *    Because gcvMULTI_SUPERTILED not supported by gcoHARDWARE_UploadTexture.
        */
        if (DstSurfInfo->tiling == gcvMULTI_SUPERTILED)
        {
            return gcvTRUE;
        }

        /* 3: If one of them is not NORMALIZED, go accurate path. */
        if ((srcFmtInfo->fmtDataType != gcvFORMAT_DATATYPE_UNSIGNED_NORMALIZED) ||
            (DstFmtInfo->fmtDataType != gcvFORMAT_DATATYPE_UNSIGNED_NORMALIZED))
        {
            return gcvTRUE;
        }

        /* 4: If they are both NORMALIZED. */
        if (!DstSurfInfo->superTiled)
        {
            /* Check 2.1: If to upscale, go accurate path. */
            if (gcvFORMAT_CLASS_RGBA == srcFmtInfo->fmtClass &&
                gcvFORMAT_CLASS_RGBA == DstFmtInfo->fmtClass)
            {
                /* Upscale. */
                if ((srcFmtInfo->u.rgba.red.width != 0 && srcFmtInfo->u.rgba.red.width < DstFmtInfo->u.rgba.red.width) ||
                    (srcFmtInfo->u.rgba.green.width != 0 && srcFmtInfo->u.rgba.green.width < DstFmtInfo->u.rgba.green.width) ||
                    (srcFmtInfo->u.rgba.blue.width != 0 && srcFmtInfo->u.rgba.blue.width < DstFmtInfo->u.rgba.blue.width) ||
                    (srcFmtInfo->u.rgba.alpha.width != 0 && srcFmtInfo->u.rgba.alpha.width < DstFmtInfo->u.rgba.alpha.width))
                {
                    return gcvTRUE;
                }
            }
        }
    }

    /* Otherwise, go non-accurate. */
    return gcvFALSE;
}

/*******************************************************************************
**
**  gcoTEXTURE_Upload
**
**  Upload data for a mip map to an gcoTEXTURE object.
**
**  INPUT:
**
**      gcoTEXTURE Texture
**          Pointer to an gcoTEXTURE object.
**
**      gceTEXTURE_FACE Face
**          Face of mip map to upload.  If 'Face' is gcvFACE_NONE, the data does
**          not represend a face of a cubic map.
**
**      gctUINT Width
**          Width of mip map to upload.
**
**      gctUINT Height
**          Height of mip map to upload.
**
**      gctUINT Slice
**          Slice of mip map to upload (only valid for volume maps).
**
**      gctPOINTER Memory
**          Pointer to mip map data to upload.
**
**      gctINT Stride
**          Stride of mip map data to upload.  If 'Stride' is 0, the stride will
**          be computed.
**
**      gceSURF_FORMAT Format
**          Format of the mip map data to upload.  Color conversion might be
**          required if tehe texture format and mip map data format are
**          different (24-bit RGB would be one example).
**
**      gceSURF_COLOR_SPACE SrcColorSpace
**          Color Space of source data
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoTEXTURE_Upload(
    IN gcoTEXTURE Texture,
    IN gctINT MipMap,
    IN gceTEXTURE_FACE Face,
    IN gctSIZE_T Width,
    IN gctSIZE_T Height,
    IN gctUINT Slice,
    IN gctCONST_POINTER Memory,
    IN gctSIZE_T Stride,
    IN gceSURF_FORMAT Format,
    IN gceSURF_COLOR_SPACE SrcColorSpace
    )
{
    gcsMIPMAP_PTR map;
    gctUINT index;
    gctUINT32 address[3] = {0};
    gctPOINTER memory[3] = {gcvNULL};
    gcoSURF srcSurf = gcvNULL;
    gceSTATUS status;
    gctUINT32 offset, sliceSize, width, height, stride;

    gcmHEADER_ARG("Texture=0x%x MipMap=%d Face=%d Width=%d Height=%d "
                  "Slice=%d Memory=0x%x Stride=%d Format=%d, SrcColorSpace=%u",
                  Texture, MipMap, Face, Width, Height,
                  Slice, Memory, Stride, Format, SrcColorSpace);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Texture, gcvOBJ_TEXTURE);
    gcmDEBUG_VERIFY_ARGUMENT(Memory != gcvNULL);

    gcmSAFECASTSIZET(width, Width);
    gcmSAFECASTSIZET(height, Height);
    gcmSAFECASTSIZET(stride, Stride);

    /* Walk to the proper mip map level. */
    for (map = Texture->maps; map != gcvNULL; map = map->next)
    {
        /* See if we have reached the proper level. */
        if (MipMap-- == 0)
        {
            break;
        }
    }

    if (map == gcvNULL)
    {
        /* Requested map might be too large. */
        gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    /* Convert face into index. */
    switch (Face)
    {
    case gcvFACE_NONE:
        /* Use slice for volume maps or 2D arrays. */
        index = Slice;

        switch(Texture->type)
        {
        case gcvTEXTURE_3D:
        case gcvTEXTURE_2D_ARRAY:
            gcmASSERT(map->faces == 1);
            if (index >= map->depth)
            {
                /* The specified slice is outside the allocated texture. */
                gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
            }
            break;

        case gcvTEXTURE_2D:
            gcmASSERT(map->faces == 1);
            gcmASSERT(map->depth == 1);
            if (index != 0)
            {
                /* The specified slice is outside the allocated texture. */
                gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
            }
            break;

         default:
            break;
        }
        break;

    case gcvFACE_POSITIVE_X:
    case gcvFACE_NEGATIVE_X:
    case gcvFACE_POSITIVE_Y:
    case gcvFACE_NEGATIVE_Y:
    case gcvFACE_POSITIVE_Z:
    case gcvFACE_NEGATIVE_Z:
        /* Get index from Face. */
        index = Face - gcvFACE_POSITIVE_X;
        if (index >= map->faces)
        {
            /* The specified face is outside the allocated texture faces. */
            gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
        }
        break;

    default:
        index = 0;
        break;
    }

    /* Lock the surface. */
    gcmONERROR(gcoSURF_Lock(map->surface, address, memory));

    if (map->surface->info.hasStencilComponent)
    {
        map->surface->info.canDropStencilPlane = gcvFALSE;
    }

    if (map->surface->info.hzNode.valid)
    {
        map->surface->info.hzDisabled = gcvTRUE;
    }

    gcmSAFECASTSIZET(sliceSize, map->sliceSize);
    /* Compute offset. */
    offset = index * sliceSize;

#if gcdSYNC
    gcmONERROR(gcoSURF_WaitFence(map->surface));
#endif

    if (((Format & gcvSURF_FORMAT_OCL) != 0) ||
        !_UseAccurateUpload(Format, &map->surface->info))
    {
        /* Copy the data. */
        gcmONERROR(gcoHARDWARE_UploadTexture(&map->surface->info, offset, 0, 0,
                                             width, height, Memory, stride, Format));

        /* Flush the CPU cache. */
        gcmONERROR(gcoSURF_NODE_Cache(&map->surface->info.node,
                                      memory[0],
                                      map->surface->info.node.size,
                                      gcvCACHE_CLEAN));
    }
    else
    {
        gcsSURF_BLIT_ARGS arg;

        /* Create the wrapper surface. */
        gcmONERROR(gcoSURF_Construct(gcvNULL, width, height, 1, gcvSURF_BITMAP,
                                     Format, gcvPOOL_USER, &srcSurf));

        /* If user specified stride(alignment in fact), it must be no less than calculated one. */
        gcmASSERT((gctUINT)Stride >= srcSurf->info.stride);
        gcmONERROR(gcoSURF_WrapSurface(srcSurf, stride, (gctPOINTER) Memory, gcvINVALID_ADDRESS));
        gcmONERROR(gcoSURF_SetColorSpace(srcSurf, SrcColorSpace));

        /* Propagate canDropStencilPlane */
        srcSurf->info.canDropStencilPlane = map->surface->info.canDropStencilPlane;

        gcoOS_ZeroMemory(&arg, sizeof(arg));
        arg.srcSurface = srcSurf;
        arg.dstZ = (gctINT)index;
        arg.dstSurface = map->surface;
        arg.srcWidth   = arg.dstWidth  = (gctINT)width;
        arg.srcHeight  = arg.dstHeight = (gctINT)height;
        arg.srcDepth   = arg.dstDepth  = 1;
        gcmONERROR(gcoSURF_BlitCPU(&arg));
    }

    /* Dump the buffer. */
    gcmDUMP_BUFFER(gcvNULL, "texture", address[0], memory[0], offset, map->sliceSize);

    if (gcPLS.hal->dump != gcvNULL)
    {
        /* Dump the texture data. */
        gcmVERIFY_OK(gcoDUMP_DumpData(gcPLS.hal->dump,
                                      gcvTAG_TEXTURE,
                                      address[0] + offset,
                                      map->sliceSize,
                                      (char *) memory[0] + offset));
    }

    gcmPROFILE_GC(GLTEXTURE_OBJECT_BYTES, sliceSize);

OnError:
    if (srcSurf != gcvNULL)
        gcoSURF_Destroy(srcSurf);

    if (memory[0] != gcvNULL)
        gcmVERIFY_OK(gcoSURF_Unlock(map->surface, memory[0]));

    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoTEXTURE_UploadSub
**
**  Upload data for a mip map to an gcoTEXTURE object.
**
**  INPUT:
**
**      gcoTEXTURE Texture
**          Pointer to an gcoTEXTURE object.
**
**      gctUINT MipMap
**          Mip map level to upload.
**
**      gceTEXTURE_FACE Face
**          Face of mip map to upload.  If 'Face' is gcvFACE_NONE, the data does
**          not represend a face of a cubic map.
**
**      gctUINT XOffset
**          XOffset offset into mip map to upload.
**
**      gctUINT YOffset
**          YOffset offset into mip map to upload.
**
**      gctUINT Width
**          Width of mip map to upload.
**
**      gctUINT Height
**          Height of mip map to upload.
**
**      gctUINT Slice
**          Slice of mip map to upload (only valid for volume maps).
**
**      gctPOINTER Memory
**          Pointer to mip map data to upload.
**
**      gctINT Stride
**          Stride of mip map data to upload.  If 'Stride' is 0, the stride will
**          be computed.
**
**      gceSURF_FORMAT Format
**          Format of the mip map data to upload.  Color conversion might be
**          required if tehe texture format and mip map data format are
**          different (24-bit RGB would be one example).
**
**      gceSURF_COLOR_SPACE SrcColorSpace
**          Color Space of source data
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoTEXTURE_UploadSub(
    IN gcoTEXTURE Texture,
    IN gctINT MipMap,
    IN gceTEXTURE_FACE Face,
    IN gctSIZE_T XOffset,
    IN gctSIZE_T YOffset,
    IN gctSIZE_T Width,
    IN gctSIZE_T Height,
    IN gctUINT Slice,
    IN gctCONST_POINTER Memory,
    IN gctSIZE_T Stride,
    IN gceSURF_FORMAT Format,
    IN gceSURF_COLOR_SPACE SrcColorSpace,
    IN gctUINT32 PhysicalAddress
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctUINT32 address[3] = {0};
    gctPOINTER memory[3] = {gcvNULL};
    gcsMIPMAP_PTR map;
    gctUINT index;
    gctUINT32 offset, sliceSize, width, height, stride, xOffset, yOffset;

    gcmHEADER_ARG("Texture=0x%x MipMap=%d Face=%d XOffset=%d YOffset=%d Width=%d Height=%d "
                  "Slice=%d Memory=0x%x Stride=%d Format=%d, SrcColorSpace=%u",
                  Texture, MipMap, Face, XOffset, YOffset, Width, Height,
                  Slice, Memory, Stride, Format, SrcColorSpace);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Texture, gcvOBJ_TEXTURE);
    gcmDEBUG_VERIFY_ARGUMENT(Memory != gcvNULL);

    gcmSAFECASTSIZET(width, Width);
    gcmSAFECASTSIZET(height, Height);
    gcmSAFECASTSIZET(stride, Stride);
    gcmSAFECASTSIZET(xOffset, XOffset);
    gcmSAFECASTSIZET(yOffset, YOffset);

    /* Walk to the proper mip map level. */
    for (map = Texture->maps; map != gcvNULL; map = map->next)
    {
        /* See if we have reached the proper level. */
        if (MipMap-- == 0)
        {
            break;
        }
    }

    if (!map || !map->surface)
    {
        /* Requested map might be too large. */
        status = gcvSTATUS_MIPMAP_TOO_LARGE;
        goto OnError;
    }

    if ((XOffset + Width > map->width) || (YOffset + Height > map->height))
    {
        /* Requested upload does not match the mip map. */
        gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    /* Convert face into index. */
    switch (Face)
    {
    case gcvFACE_NONE:
        /* Use slice for volume maps or 2D arrays. */
        index = Slice;

        switch(Texture->type)
        {
        case gcvTEXTURE_3D:
        case gcvTEXTURE_2D_ARRAY:
            gcmASSERT(map->faces == 1);
            if (index >= map->depth)
            {
                /* The specified slice is outside the allocated texture. */
                gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
            }
            break;
        case gcvTEXTURE_2D:
            gcmASSERT(map->faces == 1);
            gcmASSERT(map->depth == 1);
            if (index != 0)
            {
                /* The specified slice is outside the allocated texture. */
                gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
            }
            break;

         default:
            break;
        }
        break;

    case gcvFACE_POSITIVE_X:
    case gcvFACE_NEGATIVE_X:
    case gcvFACE_POSITIVE_Y:
    case gcvFACE_NEGATIVE_Y:
    case gcvFACE_POSITIVE_Z:
    case gcvFACE_NEGATIVE_Z:
        /* Get index from Face. */
        index = Face - gcvFACE_POSITIVE_X;
        if (index >= map->faces)
        {
            /* The specified face is outside the allocated texture faces. */
            gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
        }
        break;

    default:
        index = 0;
        break;
    }

    gcmSAFECASTSIZET(sliceSize, map->sliceSize);
    offset = index * sliceSize;

    /* Lock the surface. */
    gcmONERROR(gcoSURF_Lock(map->surface, address, memory));

    if (map->surface->info.hasStencilComponent)
    {
        map->surface->info.canDropStencilPlane = gcvFALSE;
    }

    if (map->surface->info.hzNode.valid)
    {
        map->surface->info.hzDisabled = gcvTRUE;
    }

    /* Currently limit for GLBM3.0 only, will remove the limitation later. */
    if (PhysicalAddress != gcvINVALID_ADDRESS && Format == gcvSURF_A8B8G8R8 &&
        XOffset == 0 && YOffset == 0 && Width == 128 && Height == 128)
    {
        gcoSURF srcSurf = gcvNULL;

        do
        {
            /* Create the wrapper surface. */
            gcmERR_BREAK(gcoSURF_Construct(gcvNULL, width, height, 1, gcvSURF_BITMAP,
                                         Format, gcvPOOL_USER, &srcSurf));

            /* If user specified stride(alignment in fact), it must be no less than calculated one. */
            gcmASSERT((gctUINT)Stride >= srcSurf->info.stride);
            gcmERR_BREAK(gcoSURF_WrapSurface(srcSurf, (gctUINT)Stride, (gctPOINTER) Memory, PhysicalAddress));
            gcmERR_BREAK(gcoSURF_SetColorSpace(srcSurf, SrcColorSpace));

            /* just to right slice */
            gcmERR_BREAK(gcoSURF_SetOffset(map->surface, offset));

            /* Propagate canDropStencilPlane */
            srcSurf->info.canDropStencilPlane = map->surface->info.canDropStencilPlane;

            gcmERR_BREAK(gcoSURF_Resolve(srcSurf, map->surface));

            gcmDUMP_BUFFER(gcvNULL,
                           "memory",
                           srcSurf->info.node.physical,
                           srcSurf->info.node.logical,
                           0,
                           srcSurf->info.size);


        } while (gcvFALSE);

        /* always jump back to slice 0 */
        gcmVERIFY_OK(gcoSURF_SetOffset(map->surface, 0));

        if (srcSurf)
        {
            gcoSURF_Destroy(srcSurf);
        }
    }
    else
    {
        /* For client memory, not support GPU copy now */
        status = gcvSTATUS_NOT_SUPPORTED;
    }

    if (gcmIS_ERROR(status))
    {

#if gcdSYNC
        gcmONERROR(gcoSURF_WaitFence(map->surface));
#endif

        if (_UseAccurateUpload(Format, &map->surface->info) == gcvFALSE)
        {
            /* Copy the data. */
            gcmONERROR(gcoHARDWARE_UploadTexture(&map->surface->info,
                                                 offset,
                                                 xOffset,
                                                 yOffset,
                                                 width,
                                                 height,
                                                 Memory,
                                                 stride,
                                                 Format));

            /* Flush the CPU cache. */
            gcmONERROR(gcoSURF_NODE_Cache(&map->surface->info.node,
                                          memory[0],
                                          map->surface->info.node.size,
                                          gcvCACHE_CLEAN));
        }
        else
        {
            gcoSURF srcSurf = gcvNULL;
            gcsSURF_BLIT_ARGS arg;

            do
            {
                /* Create the wrapper surface. */
                gcmERR_BREAK(gcoSURF_Construct(gcvNULL, width, height, 1, gcvSURF_BITMAP,
                                             Format, gcvPOOL_USER, &srcSurf));

                /* If user specified stride(alignment in fact), it must be no less than calculated one. */
                gcmASSERT((gctUINT)Stride >= srcSurf->info.stride);
                gcmERR_BREAK(gcoSURF_WrapSurface(srcSurf, (gctUINT)Stride, (gctPOINTER)Memory, gcvINVALID_ADDRESS));
                gcmERR_BREAK(gcoSURF_SetColorSpace(srcSurf, SrcColorSpace));

                gcoOS_ZeroMemory(&arg, sizeof(arg));
                arg.srcSurface = srcSurf;
                arg.dstX = (gctINT)XOffset;
                arg.dstY = (gctINT)YOffset;
                arg.dstZ = (gctINT)index;
                arg.dstSurface = map->surface;
                arg.srcWidth   = arg.dstWidth  = (gctINT)Width;
                arg.srcHeight  = arg.dstHeight = (gctINT)Height;
                arg.srcDepth   = arg.dstDepth  = 1;
                gcmERR_BREAK(gcoSURF_BlitCPU(&arg));
            } while (gcvFALSE);

            if (srcSurf)
            {
                gcmONERROR(gcoSURF_Destroy(srcSurf));
            }
        }
    }

    /* Dump the buffer. */
    gcmDUMP_BUFFER(gcvNULL,
                   "texture",
                   address[0],
                   memory[0],
                   offset,
                   map->sliceSize);

    gcmPROFILE_GC(GLTEXTURE_OBJECT_BYTES, sliceSize);

OnError:
    /* Unlock the surface. */
    if (memory[0])
    {
        gcmVERIFY_OK(gcoSURF_Unlock(map->surface, memory[0]));
    }

    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoTEXTURE_GetMipMap
**
**  Get the gcoSURF object pointer for a certain mip map level.
**
**  INPUT:
**
**      gcoTEXTURE Texture
**          Pointer to an gcoTEXTURE object.
**
**      gctUINT MipMap
**          Mip map level to get the gcoSURF object pointer for.
**
**  OUTPUT:
**
**      gcoSURF * Surface
**          Pointer to a variable that receives the gcoSURF object pointer.
*/
gceSTATUS
gcoTEXTURE_GetMipMap(
    IN gcoTEXTURE Texture,
    IN gctUINT MipMap,
    OUT gcoSURF * Surface
    )
{
    gcsMIPMAP_PTR map;
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Texture=0x%x MipMap=%d", Texture, MipMap);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Texture, gcvOBJ_TEXTURE);
    gcmDEBUG_VERIFY_ARGUMENT(Surface != gcvNULL);

    /* Get the pointer to the first gcsMIPMAP. */
    map = Texture->maps;

    /* Loop until we reached the desired mip map. */
    while (MipMap-- != 0)
    {
        /* Bail out if we are beyond the mip map chain. */
        if (map == gcvNULL)
        {
            break;
        }

        /* Get next gcsMIPMAP_PTR pointer. */
        map = map->next;
    }

    if ((map == gcvNULL) || (map->surface == gcvNULL))
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        /* Could not find requested mip map. */
        return status;
    }

    /* Return pointer to gcoSURF object tied to the gcsMIPMAP. */
    *Surface = map->surface;

    /* Success. */
    gcmFOOTER_ARG("*Surface=0x%x", *Surface);
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoTEXTURE_GetMipMapFace
**
**  Get the gcoSURF object pointer an offset into the memory for a certain face
**  inside a certain mip map level.
**
**  INPUT:
**
**      gcoTEXTURE Texture
**          Pointer to an gcoTEXTURE object.
**
**      gctUINT MipMap
**          Mip map level to get the gcoSURF object pointer for.
**
**      gceTEXTURE_FACE
**          Face to compute offset for.
**
**  OUTPUT:
**
**      gcoSURF * Surface
**          Pointer to a variable that receives the gcoSURF object pointer.
**
**      gctUINT32_PTR Offset
**          Pointer to a variable that receives the offset for the face.
*/
gceSTATUS
gcoTEXTURE_GetMipMapFace(
    IN gcoTEXTURE Texture,
    IN gctUINT MipMap,
    IN gceTEXTURE_FACE Face,
    OUT gcoSURF * Surface,
    OUT gctSIZE_T_PTR Offset
    )
{
    gcsMIPMAP_PTR map;
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Texture=0x%x MipMap=%d Face=%d", Texture, MipMap, Face);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Texture, gcvOBJ_TEXTURE);
    gcmDEBUG_VERIFY_ARGUMENT(Surface != gcvNULL);
    gcmDEBUG_VERIFY_ARGUMENT(Offset != gcvNULL);

    /* Get the pointer to the first gcsMIPMAP. */
    map = Texture->maps;

    /* Loop until we reached the desired mip map. */
    while (MipMap-- != 0)
    {
        /* Bail out if we are beyond the mip map chain. */
        if (map == gcvNULL)
        {
            break;
        }

        /* Get next gcsMIPMAP_PTR pointer. */
        map = map->next;
    }

    if ((map == gcvNULL) || (map->surface == gcvNULL))
    {
        /* Could not find requested mip map. */
        gcmFOOTER();
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    if ( (Face != gcvFACE_NONE) && (map->faces != 6) )
    {
        /* Could not find requested mip map. */
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }

    /* Return pointer to gcoSURF object tied to the gcsMIPMAP. */
    *Surface = map->surface;

    /* Return offset to face. */
    *Offset = (Face == gcvFACE_NONE)
            ? 0
            : ((Face - 1) * map->sliceSize);

    /* Success. */
    gcmFOOTER_ARG("*Surface=0x%x *Offset=%d", *Surface, *Offset);
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoTEXTURE_GetMipMapSlice
**
**  Get the gcoSURF object pointer an offset into the memory for a certain slice
**  inside a certain mip map level.
**
**  INPUT:
**
**      gcoTEXTURE Texture
**          Pointer to an gcoTEXTURE object.
**
**      gctUINT MipMap
**          Mip map level to get the gcoSURF object pointer for.
**
**      gctUINT Slice
**          Slice to compute offset for.
**
**  OUTPUT:
**
**      gcoSURF * Surface
**          Pointer to a variable that receives the gcoSURF object pointer.
**
**      gctUINT32_PTR Offset
**          Pointer to a variable that receives the offset for the face.
*/
gceSTATUS
gcoTEXTURE_GetMipMapSlice(
    IN gcoTEXTURE Texture,
    IN gctUINT MipMap,
    IN gctUINT Slice,
    OUT gcoSURF * Surface,
    OUT gctSIZE_T_PTR Offset
    )
{
    gcsMIPMAP_PTR map;
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Texture=0x%x MipMap=%d Slice=%d", Texture, MipMap, Slice);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Texture, gcvOBJ_TEXTURE);
    gcmDEBUG_VERIFY_ARGUMENT(Surface != gcvNULL);
    gcmDEBUG_VERIFY_ARGUMENT(Offset != gcvNULL);

    /* Get the pointer to the first gcsMIPMAP. */
    map = Texture->maps;

    /* Loop until we reached the desired mip map. */
    while (MipMap-- != 0)
    {
        /* Bail out if we are beyond the mip map chain. */
        if (map == gcvNULL)
        {
            break;
        }

        /* Get next gcsMIPMAP_PTR pointer. */
        map = map->next;
    }

    if ((map == gcvNULL) || (map->surface == gcvNULL))
    {
        /* Could not find requested mip map. */
        gcmFOOTER();
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    if (Slice >= map->depth)
    {
        /* Could not find requested mip map. */
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }

    /* Return pointer to gcoSURF object tied to the gcsMIPMAP. */
    *Surface = map->surface;

    /* Return offset to face. */
    *Offset = Slice * map->sliceSize;

    /* Success. */
    gcmFOOTER_ARG("*Surface=0x%x *Offset=%d", *Surface, *Offset);
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoTEXTURE_AddMipMap
**
**  Add a new mip map to the gcoTEXTURE object.
**
**  INPUT:
**
**      gcoTEXTURE Texture
**          Pointer to an gcoTEXTURE object.
**
**      gctINT Level
**          Mip map level to add.
**
**      gctINT InternalFormat
**          InternalFormat of the level requested by app, used for complete check.
**
**      gceSURF_FORMAT Format
**          Format of mip map level to add.
**
**      gctUINT Width
**          Width of mip map to add.  Should be larger or equal than 1.
**
**      gctUINT Height
**          Height of mip map to add.  Can be 0 for 1D texture maps, or larger
**          or equal than 1 for 2D, 3D, or cubic maps.
**
**      gctUINT Depth
**          Depth of mip map to add.  Can be 0 for 1D, 2D, or cubic maps, or
**          larger or equal than 1 for 3D texture maps.
**
**      gctUINT Faces
**          Number of faces of the mip map to add.  Can be 0 for 1D, 2D, or 3D
**          texture maps, or 6 for cubic maps.
**
**      gctBOOL unsizedDepthTexture
**          Indicate whehter dpeth texture is specified by sized or unsized internal format
**          to support different behavior betwee GL_OES_depth_texture and OES3.0 core spec.
**
**      gcePOOL Pool
**          Pool to allocate memory from for the mip map.
**
**  OUTPUT:
**
**      gcoSURF * Surface
**          Pointer to a variable that receives the gcoSURF object pointer.
**          'Surface' can be gcvNULL, in which case no surface will be returned.
*/
gceSTATUS
gcoTEXTURE_AddMipMap(
    IN gcoTEXTURE Texture,
    IN gctINT Level,
    IN gctINT InternalFormat,
    IN gceSURF_FORMAT Format,
    IN gctSIZE_T Width,
    IN gctSIZE_T Height,
    IN gctSIZE_T Depth,
    IN gctUINT Faces,
    IN gcePOOL Pool,
    OUT gcoSURF * Surface
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Texture=0x%x Level=%d Format=%d Width=%d Height=%d "
                    "Depth=%d Faces=0x%x Pool=%d",
                    Texture, Level, Format, Width, Height,
                    Depth, Faces, Pool);

    gcmONERROR(gcoTEXTURE_AddMipMapWithFlag(Texture, Level, InternalFormat, Format, Width, Height,
                                            Depth, Faces, Pool, gcvFALSE, Surface));
OnError:
    /* Return the status. */
    gcmFOOTER_ARG("*Surface=0x%08x status=%d", gcmOPT_VALUE(Surface), status);
    return status;
}

/*******************************************************************************
**
**  gcoTEXTURE_AddMipMapWithFlag
**
**  Add a new mip map to the gcoTEXTURE object.
**
**  INPUT:
**
**      gcoTEXTURE Texture
**          Pointer to an gcoTEXTURE object.
**
**      gctINT Level
**          Mip map level to add.
**
**      gctINT InternalFormat
**          InternalFormat of the level requested by app, used for complete check.
**
**      gceSURF_FORMAT Format
**          Format of mip map level to add.
**
**      gctUINT Width
**          Width of mip map to add.  Should be larger or equal than 1.
**
**      gctUINT Height
**          Height of mip map to add.  Can be 0 for 1D texture maps, or larger
**          or equal than 1 for 2D, 3D, or cubic maps.
**
**      gctUINT Depth
**          Depth of mip map to add.  Can be 0 for 1D, 2D, or cubic maps, or
**          larger or equal than 1 for 3D texture maps.
**
**      gctUINT Faces
**          Number of faces of the mip map to add.  Can be 0 for 1D, 2D, or 3D
**          texture maps, or 6 for cubic maps.
**
**      gctBOOL unsizedDepthTexture
**          Indicate whehter dpeth texture is specified by sized or unsized internal format
**          to support different behavior betwee GL_OES_depth_texture and OES3.0 core spec.
**
**      gcePOOL Pool
**          Pool to allocate memory from for the mip map.
**
**      gctBOOL Protected
**          Indicate whether will allocate with secure flag when construct surface.
**
**  OUTPUT:
**
**      gcoSURF * Surface
**          Pointer to a variable that receives the gcoSURF object pointer.
**          'Surface' can be gcvNULL, in which case no surface will be returned.
*/
gceSTATUS
gcoTEXTURE_AddMipMapWithFlag(
    IN gcoTEXTURE Texture,
    IN gctINT Level,
    IN gctINT InternalFormat,
    IN gceSURF_FORMAT Format,
    IN gctSIZE_T Width,
    IN gctSIZE_T Height,
    IN gctSIZE_T Depth,
    IN gctUINT Faces,
    IN gcePOOL Pool,
    IN gctBOOL Protected,
    OUT gcoSURF * Surface
    )
{
    gctINT level;
    gctINT internalFormat = gcvUNKNOWN_MIPMAP_IMAGE_FORMAT;
    gcsMIPMAP_PTR map;
    gcsMIPMAP_PTR next;
    gceSTATUS status;
    gceSURF_FORMAT format;
    gctUINT width, height, depth;

    gcmHEADER_ARG("Texture=0x%x Level=%d Format=%d Width=%d Height=%d "
                    "Depth=%d Faces=0x%x Pool=%d",
                    Texture, Level, Format, Width, Height,
                    Depth, Faces, Pool);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Texture, gcvOBJ_TEXTURE);

    gcmSAFECASTSIZET(width, Width);
    gcmSAFECASTSIZET(height, Height);
    gcmSAFECASTSIZET(depth, Depth);

    if (Level < 0)
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }

    /* Remove format modifiers. */
    format = (gceSURF_FORMAT) (Format & ~gcvSURF_FORMAT_OCL);

    /* Set initial level. */
    map  = gcvNULL;
    next = Texture->maps;
    /* Find the correct mip level. */
    for (level = 0; level <= Level; level += 1)
    {
        /* Create gcsMIPMAP structure if doesn't yet exist. */
        if (next == gcvNULL)
        {
            gctPOINTER pointer = gcvNULL;

            gcmONERROR(
                gcoOS_Allocate(gcvNULL,
                               sizeof(struct _gcsMIPMAP),
                               &pointer));

            next = pointer;

            /* Update the texture object format which will be used for _UploadSub. */
            Texture->format = Format;

            /* Initialize the gcsMIPMAP structure. */
            next->internalFormat = gcvUNKNOWN_MIPMAP_IMAGE_FORMAT;
            next->format     = gcvSURF_UNKNOWN;
            next->width      = ~0U;
            next->height     = ~0U;
            next->depth      = ~0U;
            next->faces      = ~0U;
            next->sliceSize  = ~0U;
            next->pool       = gcvPOOL_UNKNOWN;
            next->surface    = gcvNULL;
            next->locked     = gcvNULL;
            next->next       = gcvNULL;
            next->hAlignment = gcvSURF_FOUR;

            /* Append the gcsMIPMAP structure to the end of the chain. */
            if (Texture->maps == gcvNULL)
            {
                /* Save texture format. */
                Texture->format = format;

                /* Update map pointers. */
                Texture->maps = next;
                Texture->tail = next;
            }
            else
            {
                /* Update map pointers. */
                Texture->tail->next = next;
                Texture->tail       = next;
            }

            /* Increment the number of levels. */
            Texture->levels ++;
        }
        else
        {
            internalFormat = next->internalFormat;
        }

        /* Move to the next level. */
        map  = next;
        next = next->next;
    }

    if (map == gcvNULL)
    {
        status = gcvSTATUS_HEAP_CORRUPTED;
        gcmFOOTER();
        return status;
    }

    /* If caller specified it, use the specified one, otherwise the inherited one */
    if (InternalFormat != gcvUNKNOWN_MIPMAP_IMAGE_FORMAT)
    {
        internalFormat = InternalFormat;
    }

    /* Client driver may sent depth = 0 */
    depth = depth > 0 ? depth : 1;

    /* Query texture support */
    gcmONERROR(
        gcoHARDWARE_QueryTexture(gcvNULL,
                                 format,
                                 gcvTILED,
                                 Level,
                                 width, height, depth,
                                 Faces,
                                 &Texture->blockWidth,
                                 &Texture->blockHeight));

    if (gcmIS_SUCCESS(status))
    {
        /* Free the surface (and all the texture's mipmaps) if it exists and is different. */
        if
        (
            (map != gcvNULL) &&
            (map->surface != gcvNULL)   &&
            (
                (map->format != format) ||
                (map->width  != width)  ||
                (map->height != height) ||
                (map->depth  != depth)  ||
                (map->faces  != Faces)  ||
                (map->pool   != Pool)
            )
        )
        {
            /* Unlock the surface. */
            if (map->locked != gcvNULL)
            {
                status = gcoSURF_Unlock(map->surface, map->locked);

                if (gcmIS_ERROR(status))
                {
                    /* Error. */
                    gcmFOOTER();
                    return status;
                }

                map->locked = gcvNULL;
            }

            /* Destroy the surface. */
            if (map->surface != gcvNULL)
            {
                status = gcoSURF_Destroy(map->surface);

                if (gcmIS_ERROR(status))
                {
                    /* Error. */
                    gcmFOOTER();
                    return status;
                }
            }

            /* Reset the surface pointer. */
            map->surface = gcvNULL;

            /* A surface has been removed. */
            gcmASSERT(Texture->completeLevels > 0);
            Texture->completeLevels --;
        }

        if (map != gcvNULL && map->surface == gcvNULL)
        {
            /* Construct the surface. */
            gcmONERROR(
                gcoSURF_Construct(gcvNULL,
                                  gcmALIGN_NP2(width, Texture->blockWidth),
                                  gcmALIGN_NP2(height, Texture->blockHeight),
                                  gcmMAX(gcmMAX(depth, Faces), 1),
                                  Protected ? gcvSURF_TEXTURE | gcvSURF_PROTECTED_CONTENT : gcvSURF_TEXTURE,
                                  Format,
                                  Pool,
                                  &map->surface));

            /* When replace mipmap, the tex format could change */
            Texture->format = Format;

            /* Initialize the gcsMIPMAP structure. */
            map->format    = format;
            map->width     = width;
            map->height    = height;
            map->depth     = depth;
            map->faces     = Faces;
            map->sliceSize = map->surface->info.sliceSize;
            map->pool      = Pool;
            gcmONERROR(
                gcoHARDWARE_QueryTileAlignment(gcvNULL,
                                               gcvSURF_TEXTURE,
                                               map->format,
                                               &map->hAlignment,
                                               gcvNULL));

            /* Update the valid surface count. */
            gcmASSERT(Texture->completeLevels < Texture->levels);
            Texture->completeLevels ++;

            /* Invalidate completeness status. */
            Texture->completeMax = -1;
            Texture->completeBase = 0x7fffffff;
            Texture->baseLevelMap = gcvNULL;;
        }

        /* Set texture filterable property. */
        Texture->filterable = !map->surface->info.formatInfo.fakedFormat ||
                              map->surface->info.paddingFormat;

        /* Update internal format no matter surface was reallocated or not */
        map->internalFormat = internalFormat;

        /* Return the gcoSURF pointer. */
        if (Surface != gcvNULL)
        {
            *Surface = map->surface;
        }
    }

OnError:
    /* Return the status. */
    gcmFOOTER_ARG("*Surface=0x%08x status=%d", gcmOPT_VALUE(Surface), status);
    return status;
}

/*******************************************************************************
**
**  gcoTEXTURE_AddMipMapFromClient
**
**  Add a new mip map by wrapping a client surface to the gcoTEXTURE object.
**
**  INPUT:
**
**      gcoTEXTURE Texture
**          Pointer to an gcoTEXTURE object.
**
**      gctINT Level
**          Mip map level to add.
**
**     gcoSURF Surface
**          Client surface to be wrapped
**
**  OUTPUT:
**
**      Nothing.
*/

gceSTATUS
gcoTEXTURE_AddMipMapFromClient(
    IN gcoTEXTURE Texture,
    IN gctINT Level,
    IN gcoSURF Surface
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Texture=0x%x Level=%d Surface=0x%x", Texture, Level, Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Texture, gcvOBJ_TEXTURE);
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    if (Texture->maps != gcvNULL)
    {
        _DestroyMaps(Texture->maps, gcvNULL);
        Texture->maps = gcvNULL;
    }

    /* Test for VAA surface. */
    if (Surface->info.vaa)
    {
        if (Level != 0)
        {
            /* Only supports level 0. */
            gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
        }

        gcmONERROR(
            gcoTEXTURE_AddMipMap(Texture,
                                 Level,
                                 gcvUNKNOWN_MIPMAP_IMAGE_FORMAT,
                                 Surface->info.format,
                                 Surface->info.rect.right / 2,
                                 Surface->info.rect.bottom,
                                 Surface->depth,
                                 1,
                                 gcvPOOL_DEFAULT,
                                 gcvNULL));

        /* Set texture completeness. */
        gcmASSERT(Texture->levels > 0);
        gcmASSERT(Texture->levels == Texture->completeLevels);
        Texture->complete    = gcvTRUE;
        Texture->completeMax = 0;
        Texture->completeBase= 0;
        Texture->baseLevelMap = Texture->maps;
    }
    else
    {
        gcmONERROR(
            gcoTEXTURE_AddMipMapFromSurface(Texture,
                                            Level,
                                            Surface));

        /* Reference client surface. */
        gcmONERROR(
            gcoSURF_ReferenceSurface(Surface));
    }

    /* Copy surface format to texture. */
    Texture->format = Surface->info.format;

    /* Set texture filterable property. */
    Texture->filterable = !Surface->info.formatInfo.fakedFormat ||
                          Surface->info.paddingFormat;

    /* Success. */
    status = gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoTEXTURE_AddMipMapFromSurface
**
**  Add a new mip map from an existing surface to the gcoTEXTURE object.
**
**  INPUT:
**
**      gcoTEXTURE Texture
**          Pointer to an gcoTEXTURE object.
**
**      gctINT Level
**          Mip map level to add.
**
**     gcoSURF    Surface
**          Surface which will be added to mipmap
**
**  OUTPUT:
**
**      Nothing.
*/

gceSTATUS
gcoTEXTURE_AddMipMapFromSurface(
    IN gcoTEXTURE Texture,
    IN gctINT     Level,
    IN gcoSURF    Surface
    )
{
    gcsMIPMAP_PTR map;
    gceSTATUS status;
    gctUINT width, height, face;
    gceSURF_FORMAT format;
    gceTILING tiling;
    gctPOINTER pointer = gcvNULL;

    gcmHEADER_ARG("Texture=0x%x Level=%d Surface=0x%x", Texture, Level, Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Texture, gcvOBJ_TEXTURE);

    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    if (Level != 0)
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }

    if (Texture->maps != gcvNULL)
    {
        _DestroyMaps(Texture->maps, gcvNULL);
        Texture->maps = gcvNULL;
    }

    /* Currently only support image sourced from 2D,
    ** otherwise the face or slice index cannot be known.
    */

    format = Surface->info.format;
    tiling = Surface->info.tiling;
    width  = Surface->info.rect.right;
    height = Surface->info.rect.bottom;
    face   = Surface->depth;

    /* Query texture support */
    status = gcoHARDWARE_QueryTexture(gcvNULL,
                                      format,
                                      tiling,
                                      Level,
                                      width, height, 0,
                                      face,
                                      &Texture->blockWidth,
                                      &Texture->blockHeight);

    if (gcmIS_ERROR(status))
    {
        /* Error. */
        gcmFOOTER();
        return status;
    }

    /* Create an gcsMIPMAP structure. */
    status = gcoOS_Allocate(gcvNULL,
                               gcmSIZEOF(struct _gcsMIPMAP),
                               &pointer);

    if (gcmIS_ERROR(status))
    {
        /* Error. */
        gcmFOOTER();
        return status;
    }

    gcoOS_MemFill(pointer, 0, gcmSIZEOF(struct _gcsMIPMAP));

    /* Save texture format. */
    Texture->format = format;

    map = pointer;

    /* Initialize the gcsMIPMAP structure. */
    map->width      = width;
    map->height     = height;
    map->depth      = 1;
    map->faces      = 1;
    map->sliceSize  = Surface->info.sliceSize;
    map->pool       = Surface->info.node.pool;
    map->surface    = Surface;
    map->locked     = gcvNULL;
    map->next       = gcvNULL;
    map->format     = format;
    gcoHARDWARE_QueryTileAlignment(gcvNULL,
                                   gcvSURF_TEXTURE,
                                   format,
                                   &map->hAlignment,
                                   gcvNULL);

    /* Append the gcsMIPMAP structure to the end of the chain. */
    Texture->maps = map;
    Texture->tail = map;

    /* Increment the number of levels. */
    Texture->levels ++;
    Texture->completeLevels ++;

    /* Set texture completeness. */
    gcmASSERT(Texture->levels > 0);
    gcmASSERT(Texture->levels == Texture->completeLevels);
    Texture->complete    = gcvTRUE;
    Texture->completeMax = 0;
    Texture->completeBase= 0;
    Texture->baseLevelMap = Texture->maps;

    /* Set texture filterable property. */
    Texture->filterable = !Surface->info.formatInfo.fakedFormat ||
                          Surface->info.paddingFormat;

    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoTEXTURE_SetEndianHint
**
**  Set the endian hint.
**
**  INPUT:
**
**      gcoTEXTURE Texture
**          Pointer to an gcoTEXTURE object.
**
**      gctUINT Red, Green, Blue, Alpha
**          The border color for the respective color channel.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoTEXTURE_SetEndianHint(
    IN gcoTEXTURE Texture,
    IN gceENDIAN_HINT EndianHint
    )
{
    gcmHEADER_ARG("Texture=0x%x EndianHint=%d", Texture, EndianHint);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Texture, gcvOBJ_TEXTURE);

    /* Save the endian hint. */
    Texture->endianHint = EndianHint;

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoTEXTURE_Disable
**
**  Disable a texture sampler.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object.
**
**      gctINT Sampler
**          The physical sampler to disable.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoTEXTURE_Disable(
    IN gcoHAL Hal,
    IN gctINT Sampler
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Sampler=%d", Sampler);

    /* Verify the arguments. */
    gcmDEBUG_VERIFY_ARGUMENT(Sampler >= 0);

    /* Disable the texture. */
    status = gcoHARDWARE_DisableTextureSampler(gcvNULL, Sampler);

    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoTEXTURE_Flush
**
**  Flush the texture cache, when used in the pixel shader.
**
**  INPUT:
**
**      gcoTEXTURE Texture
**          Pointer to an gcoTEXTURE object.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoTEXTURE_Flush(
    IN gcoTEXTURE Texture
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Texture=0x%x", Texture);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Texture, gcvOBJ_TEXTURE);

    /* Flush the texture cache. */
    status = gcoHARDWARE_FlushTexture(gcvNULL, gcvFALSE);

    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoTEXTURE_FlushVS
**
**  Flush the texture cache, when used in the vertex shader.
**
**  INPUT:
**
**      gcoTEXTURE Texture
**          Pointer to an gcoTEXTURE object.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoTEXTURE_FlushVS(
    IN gcoTEXTURE Texture
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Texture=0x%x", Texture);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Texture, gcvOBJ_TEXTURE);

    /* Flush the texture cache. */
    status = gcoHARDWARE_FlushTexture(gcvNULL, gcvTRUE);

    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoTEXTURE_QueryCaps
**
**  Query the texture capabilities.
**
**  INPUT:
**
**      Nothing.
**
**  OUTPUT:
**
**      gctUINT * MaxWidth
**          Pointer to a variable receiving the maximum width of a texture.
**
**      gctUINT * MaxHeight
**          Pointer to a variable receiving the maximum height of a texture.
**
**      gctUINT * MaxDepth
**          Pointer to a variable receiving the maximum depth of a texture.  If
**          volume textures are not supported, 0 will be returned.
**
**      gctBOOL * Cubic
**          Pointer to a variable receiving a flag whether the hardware supports
**          cubic textures or not.
**
**      gctBOOL * NonPowerOfTwo
**          Pointer to a variable receiving a flag whether the hardware supports
**          non-power-of-two textures or not.
**
**      gctUINT * VertexSamplers
**          Pointer to a variable receiving the number of sampler units in the
**          vertex shader.
**
**      gctUINT * PixelSamplers
**          Pointer to a variable receiving the number of sampler units in the
**          pixel shader.
*/
gceSTATUS
gcoTEXTURE_QueryCaps(
    IN  gcoHAL    Hal,
    OUT gctUINT * MaxWidth,
    OUT gctUINT * MaxHeight,
    OUT gctUINT * MaxDepth,
    OUT gctBOOL * Cubic,
    OUT gctBOOL * NonPowerOfTwo,
    OUT gctUINT * VertexSamplers,
    OUT gctUINT * PixelSamplers
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("MaxWidth=0x%x MaxHeight=0x%x MaxDepth=0x%x Cubic=0x%x "
                    "NonPowerOfTwo=0x%x VertexSamplers=0x%x PixelSamplers=0x%x",
                    MaxWidth, MaxHeight, MaxDepth, Cubic,
                    NonPowerOfTwo, VertexSamplers, PixelSamplers);

    /* Route to gcoHARDWARE function. */
    status = gcoHARDWARE_QueryTextureCaps(gcvNULL,
                                          MaxWidth,
                                          MaxHeight,
                                          MaxDepth,
                                          Cubic,
                                          NonPowerOfTwo,
                                          VertexSamplers,
                                          PixelSamplers,
                                          gcvNULL);

    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoTEXTURE_GetClosestFormat
**
**  Returns the closest supported format to the one specified in the call.
**
**  INPUT:
**
**      gceSURF_FORMAT APIValue
**          API value.
**
**  OUTPUT:
**
**      gctUINT32 * HwValue
**          Closest supported API value.
*/
gceSTATUS
gcoTEXTURE_GetClosestFormat(
    IN gcoHAL Hal,
    IN gceSURF_FORMAT InFormat,
    OUT gceSURF_FORMAT* OutFormat
    )
{
    return gcoTEXTURE_GetClosestFormatEx(Hal,
                                         InFormat,
                                         gcvTEXTURE_UNKNOWN,
                                         OutFormat);
}


/*******************************************************************************
**
**  gcoTEXTURE_GetClosestFormatEx
**
**  Returns the closest supported format to the one specified in the call.
**
**  INPUT:
**
**      gceSURF_FORMAT APIValue
**          API value.
**
**  INPUT
**      gceTEXTURE_TYPE TextureType
**          Texture type
**
**  OUTPUT:
**
**      gctUINT32 * HwValue
**          Closest supported API value.
*/
gceSTATUS
gcoTEXTURE_GetClosestFormatEx(
    IN gcoHAL Hal,
    IN gceSURF_FORMAT InFormat,
    IN gceTEXTURE_TYPE TextureType,
    OUT gceSURF_FORMAT* OutFormat
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("InFormat=%d TextureType=%d", InFormat, TextureType);

    /* Route to gcoHARDWARE function. */
    status = gcoHARDWARE_GetClosestTextureFormat(gcvNULL,
                                                 InFormat,
                                                 TextureType,
                                                 OutFormat);

    /* Return status. */
    gcmFOOTER();
    return status;
}



/*******************************************************************************
**
**  gcoTEXTURE_GetFormatInfo
**
**  Returns the format info structure from the texture mipmap.
**
**  INPUT:
**
**      gcoTEXTURE Texture
**          Pointer to an gcoTEXTURE object.
**
**  OUTPUT:
**
**      gcsSURF_FORMAT_INFO_PTR * TxFormatInfo
**          Format Info structure.
*/
gceSTATUS
gcoTEXTURE_GetFormatInfo(
    IN gcoTEXTURE Texture,
    IN gctINT preferLevel,
    OUT gcsSURF_FORMAT_INFO_PTR * TxFormatInfo
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Texture=0x%x TxFormatInfo=0x%x",
                    Texture, TxFormatInfo);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Texture, gcvOBJ_TEXTURE);
    gcmASSERT(preferLevel >= 0 && preferLevel < Texture->levels);

    if (TxFormatInfo != gcvNULL)
    {
        gcsMIPMAP_PTR prefMipMap = Texture->maps;
        while (preferLevel --)
        {
            prefMipMap = prefMipMap->next;
        }

        if (prefMipMap->surface)
        {
            *TxFormatInfo = &prefMipMap->surface->info.formatInfo;
        }
        else
        {
            status = gcvSTATUS_INVALID_ARGUMENT;
        }
    }

    gcmFOOTER_NO();

    return status;
}

/*******************************************************************************
**
**  gcoTEXTURE_GetTextureFormatName
**
**  Returns the format name string for the hardware texture format.
**
**  INPUT:
**
**      gcsSURF_FORMAT_INFO_PTR TxFormatInfo
**          Pointer to a Texture format info structure.
**
**  OUTPUT:
**
**      gctCONST_STRING * TxName
**          Name string for this format.
*/
gceSTATUS
gcoTEXTURE_GetTextureFormatName(
    IN gcsSURF_FORMAT_INFO_PTR TxFormatInfo,
    OUT gctCONST_STRING * TxName
    )
{
    static gctCONST_STRING txNames[] =
    {
        "INVALID",
        "A8",
        "L8",
        "I8",
        "A8L8",
        "ARGB4",
        "XRGB4",
        "ARGB8",
        "XRGB8",
        "ABGR8",
        "XBGR8",
        "R5G6B5",
        "A1RGB5",
        "X1RGB5",
        "YUY2",
        "UYVY",
        "D16",
        "D24X8",
        "A8_OES",
        "DXT1",
        "DXT3",
        "DXT5",
        "HDR7E3",
        "HDR6E4",
        "HDR5E5",
        "HDR6E5",
        "RGBE8",
        "RGBE8F",
        "RGB9E5",
        "RGB9E5F",
        "ETC1"
    };

    static gctCONST_STRING extendedTxNames[] =
    {
        "ETC2_RGB8",
        "ETC2_RGB8A1",
        "ETC2_RGB8A8",
        "EAC__R11_UNSIGNED",
        "EAC__RG11_UNSIGNED",
        "EAC__RG11_SIGNED",
        "R8G8",
        "RF16",
        "RF16GF16",
        "RF16GF16BGF16AF16",
        "RF32",
        "RF32GF32",
        "R10G10B10A2",
        "EAC__R11_SIGNED",
        "R8__SNORM",
        "RG8__SNORM",
        "RGBX8__SNORM",
        "RGBA8__SNORM",
        "RGB8",
        "YUV_ASSEMBLY",
        "ASTC",
        "RI8",
        "RI8GI8",
        "RI8GI8BI8AI8",
        "RI16",
        "RI16GI16",
        "RI16GI16BI16AI16",
        "RF11GF11BF10",
        "RI10GI10BI10AI2",
        "R8G8B8G8",
        "G8R8G8B8",
        "RI8GI8BI8GI8",
        "GI8RI8GI8BI8"
    };

    gcmHEADER_ARG("TxFormatInfo=0x%x TxName=0x%x",
                   TxFormatInfo, TxName);

    if ((TxFormatInfo != gcvNULL)
     && (TxName != gcvNULL))
    {
        if (TxFormatInfo->txFormat & 0x1F)
        {
            *TxName = txNames[TxFormatInfo->txFormat & 0x1F];
        }
        else
        {
            *TxName = extendedTxNames[(TxFormatInfo->txFormat >> 12) & 0x3F];
        }
    }

    gcmFOOTER_NO();

    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoTEXTURE_UploadYUV
**
**  Upload YUV data for a mip map to an gcoTEXTURE object.
**
**  INPUT:
**
**      gcoTEXTURE Texture
**          Pointer to an gcoTEXTURE object. Texture format must be
**          gcvSURF_YUY2.
**
**      gceTEXTURE_FACE Face
**          Face of mip map to upload.  If 'Face' is gcvFACE_NONE, the data does
**          not represend a face of a cubic map.
**
**      gctUINT Width
**          Width of mip map to upload.
**
**      gctUINT Height
**          Height of mip map to upload.
**
**      gctUINT Slice
**          Slice of mip map to upload (only valid for volume maps).
**
**      gctPOINTER Memory[3]
**          Pointer to mip map data to upload. YUV data may at most have 3
**          planes, 3 addresses are specified. Same for 'Stride[3]' below.
**
**      gctINT Stride[3]
**          Stride of mip map data to upload.  If 'Stride' is 0, the stride will
**          be computed.
**
**      gceSURF_FORMAT Format
**          Format of the mip map data to upload.  Color conversion might be
**          required if the texture format and mip map data format are
**          different.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS gcoTEXTURE_UploadYUV(
    IN gcoTEXTURE Texture,
    IN gceTEXTURE_FACE Face,
    IN gctUINT Width,
    IN gctUINT Height,
    IN gctUINT Slice,
    IN gctPOINTER Memory[3],
    IN gctINT Stride[3],
    IN gceSURF_FORMAT Format
    )
{
    gcsMIPMAP_PTR map;
    gctUINT index;
    gctUINT32 address[3] = {0};
    gctPOINTER memory[3] = {gcvNULL};
    gceSTATUS status;
    gctUINT32 offset, sliceSize;

    gcmHEADER_ARG("Texture=0x%x Face=%d Width=%d Height=%d "
                    "Slice=%d Memory=0x%x Stride=%d Format=%d",
                    Texture, Face, Width, Height,
                    Slice, Memory, Stride, Format);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Texture, gcvOBJ_TEXTURE);
    gcmDEBUG_VERIFY_ARGUMENT(Memory != gcvNULL);

    /* Walk to the proper mip map level. */
    for (map = Texture->maps; map != gcvNULL; map = map->next)
    {
        /* See if the map matches the requested size. */
        if ( (Width == map->width) && (Height == map->height) )
        {
            break;
        }
    }

    if (map == gcvNULL)
    {
        status = gcvSTATUS_MIPMAP_TOO_LARGE;
        gcmFOOTER();
        /* Requested map might be too large. */
        return status;
    }

    if (map->format != gcvSURF_YUY2)
    {
        /*
         * Only support YUY2 texture format.
         * It makes no sense for UYVY or other YUV formats.
         */
        status = gcvSTATUS_NOT_SUPPORTED;
        gcmFOOTER();

        return status;
    }

    /* Convert face into index. */
    switch (Face)
    {
    case gcvFACE_NONE:
        /* Use slice for volume maps. */
        index = Slice;
        switch(Texture->type)
        {
        case gcvTEXTURE_3D:
        case gcvTEXTURE_2D_ARRAY:
            gcmASSERT(map->faces == 1);
            if (index >= map->depth)
            {
                /* The specified slice is outside the allocated texture. */
                gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
            }
            break;

        case gcvTEXTURE_2D:
            gcmASSERT(map->faces == 1);
            gcmASSERT(map->depth == 1);
            if (index != 0)
            {
                /* The specified slice is outside the allocated texture. */
                gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
            }
            break;

         default:
            break;
        }
        break;

    case gcvFACE_POSITIVE_X:
    case gcvFACE_NEGATIVE_X:
    case gcvFACE_POSITIVE_Y:
    case gcvFACE_NEGATIVE_Y:
    case gcvFACE_POSITIVE_Z:
    case gcvFACE_NEGATIVE_Z:
        /* Get index from Face. */
        index = Face - gcvFACE_POSITIVE_X;
        if (index > map->faces)
        {
            /* The specified face is outside the allocated texture faces. */
            status = gcvSTATUS_INVALID_ARGUMENT;
            gcmFOOTER();
            return status;
        }
        break;

    default:
        index = 0;
        break;
    }

    /* Lock the surface. */
    gcmONERROR(gcoSURF_Lock(map->surface, address, memory));

    if (map->surface->info.hasStencilComponent)
    {
        map->surface->info.canDropStencilPlane = gcvFALSE;
    }

    gcmSAFECASTSIZET(sliceSize, map->sliceSize);
    /* Compute offset. */
    offset = index * sliceSize;

#if gcdSYNC
    gcmONERROR(gcoSURF_WaitFence(map->surface));
#endif

    /* Copy the data. */
    gcmONERROR(gcoHARDWARE_UploadTextureYUV(map->format,
                                            address[0],
                                            memory[0],
                                            offset,
                                            map->surface->info.stride,
                                            0,
                                            0,
                                            Width,
                                            Height,
                                            Memory,
                                            Stride,
                                            Format));

    /* Flush the CPU cache. */
    gcmONERROR(gcoSURF_NODE_Cache(&map->surface->info.node,
                                  memory[0],
                                  map->surface->info.node.size,
                                  gcvCACHE_CLEAN));

    /* Dump the buffer. */
    gcmDUMP_BUFFER(gcvNULL,
                   "texture",
                   address[0],
                   memory[0],
                   offset,
                   map->sliceSize);

    if (gcPLS.hal->dump != gcvNULL)
    {
        /* Dump the texture data. */
        gcmVERIFY_OK(gcoDUMP_DumpData(gcPLS.hal->dump,
                                      gcvTAG_TEXTURE,
                                      address[0] + offset,
                                      map->sliceSize,
                                      (char *) memory[0] + offset));
    }

    /* Unlock the surface. */
    gcmVERIFY_OK(gcoSURF_Unlock(map->surface, memory[0]));

    gcmPROFILE_GC(GLTEXTURE_OBJECT_BYTES, sliceSize);

    /* Return the status. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoTEXTURE_UploadCompressed
**
**  Upload compressed data for a mip map to an gcoTEXTURE object.
**
**  INPUT:
**
**      gcoTEXTURE Texture
**          Pointer to an gcoTEXTURE object.
**
**      gceTEXTURE_FACE Face
**          Face of mip map to upload.  If 'Face' is gcvFACE_NONE, the data does
**          not represent a face of a cubic map.
**
**      gctUINT Width
**          Width of mip map to upload.
**
**      gctUINT Height
**          Height of mip map to upload.
**
**      gctUINT Slice
**          Slice of mip map to upload (only valid for volume maps).
**
**      gctPOINTER Memory
**          Pointer to mip map data to upload.
**
**      gctSIZE_T Size
**          Size of the compressed data to upload.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoTEXTURE_UploadCompressed(
    IN gcoTEXTURE Texture,
    IN gctINT MipMap,
    IN gceTEXTURE_FACE Face,
    IN gctSIZE_T Width,
    IN gctSIZE_T Height,
    IN gctUINT Slice,
    IN gctCONST_POINTER Memory,
    IN gctSIZE_T Size
    )
{
    gcsMIPMAP_PTR map;
    gctUINT index;
    gctUINT32 address[3] = {0};
    gctPOINTER memory[3] = {gcvNULL};
    gceSTATUS status;
    gctUINT32 offset, width, height, size;

    gcmHEADER_ARG("Texture=0x%x Mipmap=%d, Face=%d Width=%d Height=%d Slice=%d Memory=0x%x Size=%d",
                    Texture, MipMap, Face, Width, Height, Slice, Memory, Size);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Texture, gcvOBJ_TEXTURE);
    gcmDEBUG_VERIFY_ARGUMENT(Memory != gcvNULL);

    gcmSAFECASTSIZET(width, Width);
    gcmSAFECASTSIZET(height, Height);
    gcmSAFECASTSIZET(size, Size);

    /* Walk to the proper mip map level. */
    for (map = Texture->maps; map != gcvNULL; map = map->next)
    {
        /* See if we have reached the proper level. */
        if (MipMap-- == 0)
        {
            break;
        }
    }

    if (map == gcvNULL)
    {
        /* Requested map might be too large. */
        gcmONERROR(gcvSTATUS_MIPMAP_TOO_LARGE);
    }

    /* Convert face into index. */
    switch (Face)
    {
    case gcvFACE_NONE:
        /* Use slice for volume maps or 2D arrays. */
        index = Slice;

        switch(Texture->type)
        {
        case gcvTEXTURE_3D:
        case gcvTEXTURE_2D_ARRAY:
            gcmASSERT(map->faces == 1);
            if (index >= map->depth)
            {
                /* The specified slice is outside the allocated texture. */
                gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
            }
            break;

        case gcvTEXTURE_2D:
            gcmASSERT(map->faces == 1);
            gcmASSERT(map->depth == 1);
            if (index != 0)
            {
                /* The specified slice is outside the allocated texture. */
                gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
            }
            break;

         default:
            break;
        }
        break;

    case gcvFACE_POSITIVE_X:
    case gcvFACE_NEGATIVE_X:
    case gcvFACE_POSITIVE_Y:
    case gcvFACE_NEGATIVE_Y:
    case gcvFACE_POSITIVE_Z:
    case gcvFACE_NEGATIVE_Z:
        /* Get index from Face. */
        index = Face - gcvFACE_POSITIVE_X;
        if (index >= map->faces)
        {
            /* The specified face is outside the allocated texture faces. */
            gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
        }
        break;

    default:
        index = 0;
        break;
    }

    /* Lock the surface. */
    gcmONERROR(gcoSURF_Lock(map->surface, address, memory));

    /* Compute offset. */
    gcmSAFECASTSIZET(offset, index * map->sliceSize);

    /* Copy the data. */
    gcmONERROR(gcoHARDWARE_UploadCompressedTexture(&map->surface->info,
                                                   Memory,
                                                   offset,
                                                   0,
                                                   0,
                                                   width,
                                                   height,
                                                   size));

    /* Dump the buffer. */
    gcmDUMP_BUFFER(gcvNULL, "texture", address[0], memory[0], offset, map->sliceSize);

    if (gcPLS.hal->dump != gcvNULL)
    {
        /* Dump the texture data. */
        gcmONERROR(gcoDUMP_DumpData(gcPLS.hal->dump,
                                    gcvTAG_TEXTURE,
                                    address[0] + offset,
                                    Size,
                                    (char *) memory[0] + offset));
    }

    gcmPROFILE_GC(GLTEXTURE_OBJECT_BYTES, size);

OnError:
    if (memory[0])
    {
        /* Unlock the surface. */
        gcmVERIFY_OK(gcoSURF_Unlock(map->surface, memory[0]));
    }

    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoTEXTURE_UploadCompressedSub
**
**  Upload compressed data for a mip map to an gcoTEXTURE object.
**
**  INPUT:
**
**      gcoTEXTURE Texture
**          Pointer to an gcoTEXTURE object.
**
**      gceTEXTURE_FACE Face
**          Face of mip map to upload.  If 'Face' is gcvFACE_NONE, the data does
**          not represend a face of a cubic map.
**
**      gctUINT Width
**          Width of mip map to upload.
**
**      gctUINT Height
**          Height of mip map to upload.
**
**      gctUINT Slice
**          Slice of mip map to upload (only valid for volume maps).
**
**      gctPOINTER Memory
**          Pointer to mip map data to upload.
**
**      gctSIZE_T Size
**          Size of the compressed data to upload.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoTEXTURE_UploadCompressedSub(
    IN gcoTEXTURE Texture,
    IN gctINT MipMap,
    IN gceTEXTURE_FACE Face,
    IN gctSIZE_T XOffset,
    IN gctSIZE_T YOffset,
    IN gctSIZE_T Width,
    IN gctSIZE_T Height,
    IN gctUINT Slice,
    IN gctCONST_POINTER Memory,
    IN gctSIZE_T Size
    )
{
    gcsMIPMAP_PTR map;
    gctUINT index;
    gctUINT32 address[3] = {0};
    gctPOINTER memory[3] = {gcvNULL};
    gceSTATUS status;
    gctUINT32 offset, width, height, size, xOffset, yOffset;

    gcmHEADER_ARG("Texture=0x%x Face=%d Width=%d Height=%d Slice=%d Memory=0x%x Size=%d",
                   Texture, Face, Width, Height, Slice, Memory, Size);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Texture, gcvOBJ_TEXTURE);
    gcmDEBUG_VERIFY_ARGUMENT(Memory != gcvNULL);

    gcmSAFECASTSIZET(width, Width);
    gcmSAFECASTSIZET(height, Height);
    gcmSAFECASTSIZET(size, Size);
    gcmSAFECASTSIZET(xOffset, XOffset);
    gcmSAFECASTSIZET(yOffset, YOffset);

    /* Walk to the proper mip map level. */
    for (map = Texture->maps; map != gcvNULL; map = map->next)
    {
        /* See if we have reached the proper level. */
        if (MipMap-- == 0)
        {
            break;
        }
    }

    if (!map || !map->surface)
    {
        /* Requested map might be too large, or not been created yet. */
        gcmONERROR(gcvSTATUS_MIPMAP_TOO_LARGE);
    }

    if ((XOffset + Width > map->width) || (YOffset + Height > map->height))
    {
        /* Requested upload does not match the mip map. */
        gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    /* Convert face into index. */
    switch (Face)
    {
    case gcvFACE_NONE:
        /* Use slice for volume maps or 2D arrays. */
        index = Slice;

        switch(Texture->type)
        {
        case gcvTEXTURE_3D:
        case gcvTEXTURE_2D_ARRAY:
            gcmASSERT(map->faces == 1);
            if (index >= map->depth)
            {
                /* The specified slice is outside the allocated texture. */
                gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
            }
            break;

        case gcvTEXTURE_2D:
            gcmASSERT(map->faces == 1);
            gcmASSERT(map->depth == 1);
            if (index != 0)
            {
                /* The specified slice is outside the allocated texture. */
                gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
            }
            break;

         default:
            break;
        }
        break;

    case gcvFACE_POSITIVE_X:
    case gcvFACE_NEGATIVE_X:
    case gcvFACE_POSITIVE_Y:
    case gcvFACE_NEGATIVE_Y:
    case gcvFACE_POSITIVE_Z:
    case gcvFACE_NEGATIVE_Z:
        /* Get index from Face. */
        index = Face - gcvFACE_POSITIVE_X;
        if (index >= map->faces)
        {
            /* The specified face is outside the allocated texture faces. */
            gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
        }
        break;

    default:
        index = 0;
        break;
    }

    /* Lock the surface. */
    gcmONERROR(gcoSURF_Lock(map->surface, address, memory));


    /* Compute offset. */
    gcmSAFECASTSIZET(offset, index*map->sliceSize);

    /* Copy the data. */
    gcmONERROR(gcoHARDWARE_UploadCompressedTexture(&map->surface->info,
                                                   Memory,
                                                   offset,
                                                   xOffset,
                                                   yOffset,
                                                   width,
                                                   height,
                                                   size));

    /* Dump the buffer. */
    gcmDUMP_BUFFER(gcvNULL, "texture", address[0], memory[0], offset, map->sliceSize);

    if (gcPLS.hal->dump != gcvNULL)
    {
        /* Dump the texture data. */
        gcmONERROR(gcoDUMP_DumpData(gcPLS.hal->dump,
                                    gcvTAG_TEXTURE,
                                    address[0] + offset,
                                    Size,
                                    (char *) memory[0] + offset));
    }

    gcmPROFILE_GC(GLTEXTURE_OBJECT_BYTES, size);

OnError:
    /* Unlock the surface. */
    if (memory[0])
    {
        gcmVERIFY_OK(gcoSURF_Unlock(map->surface, memory[0]));
    }

    /* Return the status. */
    gcmFOOTER();
    return status;
}


/*
** This function is only working for ES11 now.
** You needn't put any ES20 app patch here.
** All ES20 app can have RTT tile status buffer by default withouth patch
*/
gceSTATUS
gcoTEXTURE_RenderIntoMipMap(
    IN gcoTEXTURE Texture,
    IN gctINT Level
    )
{
    gcsMIPMAP_PTR map   = gcvNULL;
    gceSURF_TYPE type   = gcvSURF_TYPE_UNKNOWN;
    gceSTATUS status    = gcvSTATUS_OK;
    gcoSURF surface     = gcvNULL;
    gctBOOL hasTileFiller;
    gcePATCH_ID patchId = gcvPATCH_INVALID;

    gcmHEADER_ARG("Texture=0x%x Level=%d", Texture, Level);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Texture, gcvOBJ_TEXTURE);

    /* Get the pointer to the first gcsMIPMAP. */
    map = Texture->maps;

    /* Loop until we reached the desired mip map. */
    while (Level-- != 0)
    {
        /* Bail out if we are beyond the mip map chain. */
        if (map == gcvNULL)
        {
            break;
        }

        /* Get next gcsMIPMAP_PTR pointer. */
        map = map->next;
    }

    if ((map == gcvNULL) || (map->surface == gcvNULL))
    {
        /* Could not find requested mip map. */
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }

    if (gcoHAL_IsFeatureAvailable(gcvNULL, gcvFEATURE_SINGLE_BUFFER)
                          == gcvSTATUS_TRUE)
    {

        status = gcoSURF_DisableTileStatus(map->surface, gcvTRUE);

        /* Surface is already renderable!. */
        /*status = gcvSTATUS_OK;*/
        gcmFOOTER();
        return status;
    }

    if (gcoHARDWARE_QuerySurfaceRenderable(gcvNULL, &map->surface->info) == gcvSTATUS_OK)
    {
        gcmFOOTER();
        return gcvSTATUS_OK;
    }

    gcoHARDWARE_GetPatchID(gcvNULL, &patchId);
    if (patchId == gcvPATCH_GLBM21 || patchId == gcvPATCH_GLBM25 || patchId == gcvPATCH_GLBM27 || patchId == gcvPATCH_GFXBENCH)
    {
        hasTileFiller = gcoHAL_IsFeatureAvailable(gcvNULL, gcvFEATURE_TILE_FILLER) == gcvSTATUS_TRUE;
    }
    else
    {
        hasTileFiller = gcvFALSE;
    }

    switch (map->format)
    {
    case gcvSURF_D16:
        /* fall through */
    case gcvSURF_D24S8:
        /* fall through */
    case gcvSURF_D24X8:
        /* fall through */
    case gcvSURF_D32:
        /* TODO: just because tile filler cannot work for depth now. */
        type = gcvSURF_DEPTH_NO_TILE_STATUS;
        break;

    default:
        type = hasTileFiller ? gcvSURF_RENDER_TARGET : gcvSURF_RENDER_TARGET_NO_TILE_STATUS;
        break;
    }

    if (map->surface->info.type == gcvSURF_TEXTURE)
    {
        if (map->locked != gcvNULL)
        {
                status = gcoSURF_Unlock(map->surface, map->locked);

                if (gcmIS_ERROR(status))
                {
                    /* Error. */
                    gcmFOOTER();
                    return status;
                }

                map->locked = gcvNULL;
        }

        /* Construct a new surface. */
        status = gcoSURF_Construct(gcvNULL,
                                   gcmALIGN_NP2(map->width, Texture->blockWidth),
                                   gcmALIGN_NP2(map->height, Texture->blockHeight),
                                   gcmMAX(gcmMAX(map->depth, map->faces), 1),
                                   type,
                                   map->format,
                                   gcvPOOL_DEFAULT,
                                   &surface);

        if (gcmIS_SUCCESS(status))
        {
            /* Copy the data. */
            status = gcoSURF_Resolve(map->surface, surface);
            if (gcmIS_ERROR(status))
            {
                gcmVERIFY_OK(gcoSURF_Destroy(surface));
                gcmFOOTER();
                return status;
            }

            /* Destroy the old surface. */
            gcmVERIFY_OK(gcoSURF_Destroy(map->surface));

            /* Set the new surface. */
            map->surface = surface;

            /* Mark the mipmap as not resolvable. */
            gcmVERIFY_OK(
                gcoSURF_SetResolvability(map->surface, gcvFALSE));
        }
    }

    /* Success. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoTEXTURE_IsRenderable(
    IN gcoTEXTURE Texture,
    IN gctUINT Level
    )
{
    gcsMIPMAP_PTR map;
    gcoSURF surface;
    gceSTATUS status;

    gcmHEADER_ARG("Texture=0x%x Level=%d", Texture, Level);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Texture, gcvOBJ_TEXTURE);

    /* Get the pointer to the first gcsMIPMAP. */
    map = Texture->maps;

    /* Loop until we reached the desired mip map. */
    while (Level-- != 0)
    {
        /* Bail out if we are beyond the mip map chain. */
        if (map == gcvNULL)
        {
            break;
        }

        /* Get next gcsMIPMAP_PTR pointer. */
        map = map->next;
    }

    if ((map == gcvNULL) || (map->surface == gcvNULL))
    {
        /* Could not find requested mip map. */
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }

    /* Get mipmap surface. */
    surface = map->surface;

    /* Check whether the surface is renderable. */
    status = gcoHARDWARE_QuerySurfaceRenderable(gcvNULL, &surface->info);

    /* Return status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoTEXTURE_RenderIntoMipMap2(
    IN gcoTEXTURE Texture,
    IN gctINT Level,
    IN gctBOOL Sync
    )
{
    gcsMIPMAP_PTR map;
    gceSURF_TYPE type;
    gceSTATUS status = gcvSTATUS_OK;
    gcoSURF surface;

    gcmHEADER_ARG("Texture=0x%x Level=%d", Texture, Level);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Texture, gcvOBJ_TEXTURE);

    /* Get the pointer to the first gcsMIPMAP. */
    map = Texture->maps;

    /* Loop until we reached the desired mip map. */
    while (Level-- != 0)
    {
        /* Bail out if we are beyond the mip map chain. */
        if (map == gcvNULL)
        {
            break;
        }

        /* Get next gcsMIPMAP_PTR pointer. */
        map = map->next;
    }

    if ((map == gcvNULL) || (map->surface == gcvNULL))
    {
        /* Could not find requested mip map. */
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }

    surface = map->surface;

    /*
    ** The surface can be directly rendered.
    */
    if (gcoHARDWARE_QuerySurfaceRenderable(gcvNULL, &surface->info) == gcvSTATUS_OK)
    {
        /* Change correct type to make disable tile status buffer work as expected. */
        switch (surface->info.formatInfo.fmtClass)
        {
        case gcvFORMAT_CLASS_DEPTH:
            surface->info.type = gcvSURF_DEPTH;
            break;

        default:
            gcmASSERT(surface->info.formatInfo.fmtClass == gcvFORMAT_CLASS_RGBA);
            surface->info.type = gcvSURF_RENDER_TARGET;
            break;
        }
#if gcdRTT_DISABLE_FC
        gcmONERROR(gcoSURF_DisableTileStatus(surface, gcvTRUE));
#else
        if ((gcoHAL_IsFeatureAvailable(gcvNULL, gcvFEATURE_TEXTURE_TILE_STATUS_READ) == gcvFALSE) &&
            (gcoHAL_IsFeatureAvailable(gcvNULL, gcvFEATURE_TILE_FILLER) == gcvFALSE))
        {
            gcmONERROR(gcoSURF_DisableTileStatus(surface, gcvTRUE));
        }
        else
        {
            surface->info.TSDirty = Sync;
            /* Try to allocate HZ buffer */
            gcmONERROR(gcoSURF_AllocateHzBuffer(surface));

            /* Try to allocate tile status buffer. */
            gcmONERROR(gcoSURF_AllocateTileStatus(surface));

            /* Lock HZ buffer */
            gcmONERROR(gcoSURF_LockHzBuffer(surface));

            /* Lock Tile status buffer */
            gcmONERROR(gcoSURF_LockTileStatus(surface));
        }
#endif
        gcmFOOTER();
        return gcvSTATUS_OK;
    }

    /* re-create surface for rendering, which can be sampled later too. */
    switch (surface->info.formatInfo.fmtClass)
    {
    case gcvFORMAT_CLASS_DEPTH:
        type = gcvSURF_DEPTH;
        break;

    default:
        gcmASSERT(surface->info.formatInfo.fmtClass == gcvFORMAT_CLASS_RGBA);
        type = gcvSURF_RENDER_TARGET;
        break;
    }

#if gcdRTT_DISABLE_FC
    type |= gcvSURF_NO_TILE_STATUS;
#endif

    /* Renderable surface serve as texture later */
    type |= gcvSURF_CREATE_AS_TEXTURE;

    if (map->surface->info.type == gcvSURF_TEXTURE)
    {
        if (map->locked != gcvNULL)
        {
                status = gcoSURF_Unlock(map->surface, map->locked);

                if (gcmIS_ERROR(status))
                {
                    /* Error. */
                    gcmFOOTER();
                    return status;
                }

                map->locked = gcvNULL;
        }

        /* Construct a new surface. */
        status = gcoSURF_Construct(gcvNULL,
                                   gcmALIGN_NP2(map->width, Texture->blockWidth),
                                   gcmALIGN_NP2(map->height, Texture->blockHeight),
                                   gcmMAX(gcmMAX(map->depth, map->faces), 1),
                                   type,
                                   map->format,
                                   gcvPOOL_DEFAULT,
                                   &surface);

        if (gcmIS_SUCCESS(status))
        {
            if (Sync)
            {
                /* Copy the data. */
                status = gcoSURF_Resolve(map->surface, surface);
            }
            if (gcmIS_ERROR(status))
            {
                gcmVERIFY_OK(gcoSURF_Destroy(surface));
                gcmFOOTER();
                return status;
            }

            /* Destroy the old surface. */
            gcmVERIFY_OK(gcoSURF_Destroy(map->surface));

            /* Set the new surface. */
            map->surface = surface;
        }
    }

OnError:

    /* Success. */
    gcmFOOTER();
    return status;
}


gceSTATUS
gcoTEXTURE_IsComplete(
    IN gcoTEXTURE Texture,
    IN gcsTEXTURE_PTR Info,
    IN gctINT BaseLevel,
    IN gctINT MaxLevel
    )
{
    gceSTATUS status;
    gcsTEXTURE_PTR info = gcvNULL;

    gcmHEADER_ARG("Texture=0x%x", Texture);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Texture, gcvOBJ_TEXTURE);

    /* If no mipmap was specified */
    if (MaxLevel < 0 || BaseLevel < 0 || BaseLevel > MaxLevel)
    {
        Texture->complete = gcvFALSE;
    }

    info = (Info == gcvNULL) ? &Texture->Info : Info;

    info->baseLevel = BaseLevel;
    info->maxLevel = MaxLevel;

    /* Determine the completeness. */
    if (MaxLevel > Texture->completeMax || BaseLevel < Texture->completeBase)
    {
        gctINT level;
        gctINT internalFormat = gcvUNKNOWN_MIPMAP_IMAGE_FORMAT;
        gceSURF_FORMAT format = gcvSURF_UNKNOWN;
        gctUINT width  = ~0U;
        gctUINT height = ~0U;
        gctUINT depth  = ~0U;
        gctUINT faces  = ~0U;
        gcsMIPMAP_PTR prev;
        gcsMIPMAP_PTR curr;

        /* Assume complete texture. */
        Texture->complete = gcvTRUE;

        /* Set initial level. */
        prev = gcvNULL;
        curr = Texture->maps;
        Texture->baseLevelMap = gcvNULL;

        for (level = 0; level <= MaxLevel; level += 1)
        {
            if (level < BaseLevel)
            {
                /* Move to the next level. */
                curr = curr->next;
                continue;
            }

            /* Does the level exist? */
            if (curr == gcvNULL)
            {
                /* Incomplete. */
                Texture->complete = gcvFALSE;
                break;
            }

            /* Is there a surface attached? */
            if (curr->surface == gcvNULL)
            {
                /* Incomplete. */
                Texture->complete = gcvFALSE;
                break;
            }

            /* Set texture parameters if we are at level 0. */
            if (prev == gcvNULL)
            {
                internalFormat = curr->internalFormat;
                format = curr->format;
                width  = curr->width;
                height = curr->height;
                depth  = curr->depth;
                faces  = curr->faces;
            }
            else
            {
                /* Does the level match the expected? */
                if ((internalFormat != curr->internalFormat) ||
                    (format != curr->format) ||
                    (width  != curr->width)  ||
                    (height != curr->height) ||
                    (depth  != curr->depth)  ||
                    (faces  != curr->faces))
                {
                    /* Incomplete/invalid. */
                    Texture->complete = gcvFALSE;
                    break;
                }
            }

            if (prev == gcvNULL)
            {
                Texture->baseLevelMap = curr;
            }

            /* Compute the size of the next level. */
            width  = gcmMAX(width  / 2, 1);
            height = gcmMAX(height / 2, 1);
            if (Texture->type == gcvTEXTURE_3D)
            {
                depth  = gcmMAX(depth  / 2, 1);
            }

            /* Move to the next level. */
            prev = curr;
            curr = curr->next;
        }

        if (Texture->complete)
        {
            /* Set texture format. */
            Texture->format = format;

            /* Validate completeness. */
            Texture->completeMax = MaxLevel;
            Texture->completeBase = BaseLevel;
        }
        else
        {
            /* Reset completeMax to initial value. */
            Texture->completeMax  = -1;
            Texture->completeBase = 0x7fffffff;
            Texture->baseLevelMap = gcvNULL;
        }
    }
    else if (Texture->complete)
    {
        /* Need redirect to base map */

        gctINT skipLevels = BaseLevel;
        Texture->baseLevelMap = Texture->maps;
        while (skipLevels --)
        {
            Texture->baseLevelMap = Texture->baseLevelMap->next;
        }
    }

    /* Determine the status. */
    status = Texture->complete
        ? gcvSTATUS_OK
        : gcvSTATUS_INVALID_MIPMAP;

    if (status == gcvSTATUS_OK)
    {
        switch (Texture->format)
        {
        case gcvSURF_A32F:
            /* Fall through */
        case gcvSURF_L32F:
            /* Fall through */
        case gcvSURF_A32L32F:
            if ((info->magFilter != gcvTEXTURE_POINT)
             || (info->minFilter != gcvTEXTURE_POINT)
             || (
                    (info->mipFilter != gcvTEXTURE_NONE)
                 && (info->mipFilter != gcvTEXTURE_POINT)
                )
               )
            {
                Texture->complete = gcvFALSE;

                status = gcvSTATUS_NOT_SUPPORTED;
            }
            break;

        default:
            break;
        }
    }

    /* Return status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoTEXTURE_BindTextureEx(
    IN gcoTEXTURE Texture,
    IN gctINT Target,
    IN gctINT Sampler,
    IN gcsTEXTURE_PTR Info,
    IN gctINT textureLayer
    )
{
    gceSTATUS status;
    gcsMIPMAP_PTR map;
    gctINT lod;
    gctINT baseLevel;
    gctFLOAT maxLevel;
    gcsSAMPLER samplerInfo;
    gcsMIPMAP_PTR baseMipMap = gcvNULL;
    gcsMIPMAP_PTR textureMap = gcvNULL;
    gctUINT32 address[3] = {0};
    gctPOINTER memory[3] = {gcvNULL};
    gcsTEXTURE_PTR pTexParams = gcvNULL;
    gcsTEXTURE tmpInfo;
#if gcdUSE_NPOT_PATCH
    gceTEXTURE_ADDRESSING r = gcvTEXTURE_INVALID, s = gcvTEXTURE_INVALID, t = gcvTEXTURE_INVALID;
    gceTEXTURE_FILTER mip = gcvTEXTURE_NONE;
    gctBOOL np2_forced = gcvFALSE;
#endif
    gcmHEADER_ARG("Texture=0x%x Sampler=%d", Texture, Sampler);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Texture, gcvOBJ_TEXTURE);

    /* Use the local struct Info for every thread.*/
    tmpInfo = *Info;
    pTexParams = &tmpInfo;

    gcoOS_ZeroMemory(&samplerInfo, gcmSIZEOF(samplerInfo));

    baseLevel  = gcmMAX(0, pTexParams->baseLevel);
#if gcdUSE_NPOT_PATCH
    if (gcPLS.bNeedSupportNP2Texture)
    {
        if ((Texture->maps->width & (Texture->maps->width - 1)) ||
            (Texture->maps->height & (Texture->maps->height - 1))
           )
        {
            r = pTexParams->r;
            s = pTexParams->s;
            t = pTexParams->t;
            mip = pTexParams->mipFilter;
            np2_forced = gcvTRUE;

            pTexParams->r = pTexParams->s = pTexParams->t = gcvTEXTURE_CLAMP;
            pTexParams->mipFilter = gcvTEXTURE_NONE;
        }
    }
#endif
    textureMap = Texture->maps;


    if (textureMap && textureMap->surface)
    {
        gcsSURF_INFO_PTR info = &textureMap->surface->info;

        /*Not support multi_tile/multi_supertile mipmap sampler */
        if (info->tiling == gcvMULTI_TILED || info->tiling == gcvMULTI_SUPERTILED)
        {
            pTexParams->mipFilter = gcvTEXTURE_NONE;
            pTexParams->lodBias = (gctFLOAT)baseLevel;
        }
    }


    maxLevel = (gcvTEXTURE_NONE == pTexParams->mipFilter)
             ? (baseLevel + 0.3f)    /* Add <0.5 bias to let HW min/mag determination correct*/
             : (gctFLOAT)gcmMIN(pTexParams->maxLevel, Texture->levels - 1);

    /* If HALT > 1 and bug18 fixed. The HW will clamp lodMax with maxLevel, no need SW doing it.*/
    if(gcoHAL_IsFeatureAvailable(gcvNULL,gcvFEATURE_HALTI2)       &&
       gcoHAL_IsFeatureAvailable(gcvNULL, gcvFEATURE_BUG_FIXES18)
       )
    {
        pTexParams->lodMax = Info->lodMax;
    }
    else
    {
        pTexParams->lodMax = gcmMIN(maxLevel, Info->lodMax);
    }

    pTexParams->lodMin = gcmMAX(pTexParams->lodMin, 0.0f);
    gcmASSERT(baseLevel <= (gctINT)maxLevel);

    /* Make sure we have maps defined in range [baseLevel, maxLevel].
    ** Parameter "Info" is the local variable, all updating in gcoTEXTURE_IsComplete
    ** should on it.
    */
    status = gcoTEXTURE_IsComplete(Texture, &tmpInfo, baseLevel, (gctINT)maxLevel);

    /* Special case for external texture. */
    if ((status == gcvSTATUS_INVALID_MIPMAP)
     && (Texture->levels == 0))
    {
        /* For external textures, allow sampler to be
        still bound when no texture is bound. */
        status = gcvSTATUS_OK;
        gcmASSERT(Texture->maps == gcvNULL);
    }

    if (gcmIS_ERROR(status))
    {
#if gcdUSE_NPOT_PATCH
        if (np2_forced)
        {
            pTexParams->r = r;
            pTexParams->s = s;
            pTexParams->t = t;
            pTexParams->mipFilter = mip;
        }
#endif
        gcmFOOTER_ARG("status=%d", status);
        return status;
    }

    baseMipMap = Texture->baseLevelMap;

    /* Check if the texture object is resident. */
    if (Sampler >= 0)
    {
        /* Set format and dimension info. */
        samplerInfo.endianHint  = Texture->endianHint;

        if (baseMipMap != gcvNULL)
        {
            samplerInfo.format      = baseMipMap->surface->info.format;
            samplerInfo.unsizedDepthTexture = Texture->unsizedDepthTexture;
            samplerInfo.formatInfo  = &baseMipMap->surface->info.formatInfo;
            samplerInfo.tiling      = baseMipMap->surface->info.tiling;
            samplerInfo.width       = baseMipMap->width;
            samplerInfo.height      = baseMipMap->height;
            samplerInfo.depth       = baseMipMap->depth;
            samplerInfo.faces       = baseMipMap->faces;
            samplerInfo.texType     = Texture->type;
            samplerInfo.hAlignment  = baseMipMap->hAlignment;
            samplerInfo.addressing  = gcvSURF_NO_STRIDE_TILED;
            samplerInfo.hasTileStatus = 0;
            samplerInfo.tileStatusAddr = 0;
            samplerInfo.tileStatusClearValue = 0;
        }
        else
        {
            /* Set dummy values, to sample black (0,0,0,1). */
            samplerInfo.format      = gcvSURF_A8R8G8B8;
            samplerInfo.tiling      = gcvTILED;
            samplerInfo.width       = 0;
            samplerInfo.height      = 0;
            samplerInfo.depth       = 1;
            samplerInfo.faces       = 1;
            samplerInfo.texType     = gcvTEXTURE_UNKNOWN;
            samplerInfo.hAlignment  = gcvSURF_FOUR;
            samplerInfo.addressing  = gcvSURF_NO_STRIDE_TILED;
            samplerInfo.lodNum      = 0;
            samplerInfo.hasTileStatus = 0;
            samplerInfo.tileStatusAddr = 0;
            samplerInfo.tileStatusClearValue = 0;

            gcoSURF_QueryFormat(gcvSURF_A8R8G8B8, &samplerInfo.formatInfo);
        }

        samplerInfo.baseLod = 0;

        /* Does this texture map have a valid tilestatus node attached to it?. */
        if ((baseMipMap != gcvNULL)
         && (baseMipMap->surface != gcvNULL))
        {
            gcsSURF_INFO_PTR info = &baseMipMap->surface->info;

            /* Update horizontal alignment to the surface's alignment. */
            switch(info->tiling)
            {
            case gcvLINEAR:
                samplerInfo.hAlignment = gcvSURF_SIXTEEN;
                /* Linear texture with stride. */
                samplerInfo.addressing = gcvSURF_STRIDE_LINEAR;
                switch (samplerInfo.format)
                {
                case gcvSURF_YV12:
                case gcvSURF_I420:
                case gcvSURF_NV12:
                case gcvSURF_NV21:
                case gcvSURF_NV16:
                case gcvSURF_NV61:
                    /* Need YUV assembler. */
                    samplerInfo.lodNum = 3;
                    break;

                case gcvSURF_YUY2:
                case gcvSURF_UYVY:
                case gcvSURF_YVYU:
                case gcvSURF_VYUY:
                default:
                    /* Need linear texture. */
                    samplerInfo.lodNum = 1;
                    break;
                }
                break;

            case gcvTILED:
                samplerInfo.hAlignment = baseMipMap->hAlignment;
                samplerInfo.lodNum = 1;
                break;

            case gcvSUPERTILED:
                samplerInfo.hAlignment = gcvSURF_SUPER_TILED;
                samplerInfo.lodNum = 1;
                break;

            case gcvMULTI_TILED:
                samplerInfo.hAlignment = gcvSURF_SPLIT_TILED;
                samplerInfo.lodNum = 2;
                break;

            case gcvMULTI_SUPERTILED:
                samplerInfo.hAlignment = gcvSURF_SPLIT_SUPER_TILED;
                samplerInfo.lodNum = 2;
                break;

            default:
                gcmASSERT(gcvFALSE);
                status = gcvSTATUS_NOT_SUPPORTED;
#if gcdUSE_NPOT_PATCH
                if (np2_forced)
                {
                    pTexParams->r = r;
                    pTexParams->s = s;
                    pTexParams->t = t;
                    pTexParams->mipFilter = mip;
                }
#endif
                gcmFOOTER();
                return status;
            }

            /* Set all the LOD levels. */
            samplerInfo.lodAddr[0] = info->node.physical + textureLayer * info->layerSize;

            if (samplerInfo.lodNum == 3)
            {
                /* YUV-assembler needs 3 lods. */
                samplerInfo.lodAddr[1]   = info->node.physical + info->uOffset;
                samplerInfo.lodAddr[2]   = info->node.physical + info->vOffset;

                /* Save strides. */
                samplerInfo.lodStride[0] = info->stride;
                samplerInfo.lodStride[1] = info->uStride;
                samplerInfo.lodStride[2] = info->vStride;
            }
            else if (samplerInfo.lodNum == 2)
            {
                samplerInfo.lodAddr[1] = info->node.physicalBottom + textureLayer * info->layerSize;
            }
            else
            {
                samplerInfo.baseLod = baseLevel;

                /* Normal textures(include linear RGB) can have up to 14 lods. */
                for (map = baseMipMap, lod = baseLevel; map && (lod <= (gctINT)maxLevel); map = map->next, lod++)
                {
                    info = &map->surface->info;
                    if (map->locked == gcvNULL)
                    {
                        /* Lock the texture surface. */
                        status = gcoSURF_Lock(map->surface, address, memory);
                        map->address = address[0];
                        map->locked = memory[0];

                        if (gcmIS_ERROR(status))
                        {
#if gcdUSE_NPOT_PATCH
                            if (np2_forced)
                            {
                                pTexParams->r = r;
                                pTexParams->s = s;
                                pTexParams->t = t;
                                pTexParams->mipFilter = mip;
                            }
#endif
                            /* Error. */
                            gcmFOOTER();
                            return status;
                        }
                    }

                    /*
                    ** For mipmap texture, we have to disable all mipmap's tile status buffer if it has.
                    ** Later we may can squeeze all mipmap surface into a big to use a big tile status buffer
                    ** Then we can support render into mipmap/multi-slice texture.
                    */
                    if (((gctINT)maxLevel > baseLevel) &&
                        (info->tileStatusNode.pool != gcvPOOL_UNKNOWN) &&
                        (!info->tileStatusDisabled))
                    {
                        /*
                        ** We have no way to sample MSAA surface for now as we can't resolve MSAA into itself.
                        ** MSAA texture has been put into shadow rendering path.
                        */
                        gcmASSERT(info->samples.x == 1 && info->samples.y == 1);

                        gcoSURF_DisableTileStatus(map->surface, gcvTRUE);

                    }

                    samplerInfo.lodAddr[lod]   = map->address + textureLayer * map->surface->info.layerSize;
                    samplerInfo.lodStride[lod] = map->surface->info.stride;

#if defined(ANDROID) && gcdANDROID_UNALIGNED_LINEAR_COMPOSITION_ADJUST
                    /* Adjust linear texture if from flip bitmap. */
                    samplerInfo.lodAddr[lod] += map->surface->info.flipBitmapOffset;
#endif

                    if (info->formatInfo.fmtClass == gcvFORMAT_CLASS_ASTC)
                    {
                        samplerInfo.astcSize[lod] = info->format -
                                                    (info->formatInfo.sRGB ? gcvSURF_ASTC4x4_SRGB : gcvSURF_ASTC4x4);
                        samplerInfo.astcSRGB[lod] = info->formatInfo.sRGB;
                    }
                }

                /* Set correct lodNum. */
                samplerInfo.lodNum = lod;
            }

            info = &baseMipMap->surface->info;

            /* Program tile status.
            ** Current texture FC only support the simplest case (single mipmap).
            ** It can't support render to mipmap/CUBE/layer texture.
            ** We also don't have decompressor for TX except GC4000(HISI).
            ** Later we need disable compression when rendering to texture.
            ** Even on GC4000, it has bugs if TX enable tile status, so we always
            ** disable it for existing chips.
            */
            if ((info->tileStatusNode.pool != gcvPOOL_UNKNOWN) && (!info->tileStatusDisabled))
            {
                /*
                ** We have no way to sample MSAA surface for now as we can't resolve MSAA into itself.
                ** MSAA texture has been put into shadow rendering path.
                */
                gcmASSERT(info->samples.x == 1 && info->samples.y == 1);
                if ((gcoHAL_IsFeatureAvailable(gcvNULL, gcvFEATURE_TEXTURE_TILE_STATUS_READ) == gcvFALSE) ||
                    (info->compressed && gcoHAL_IsFeatureAvailable(gcvNULL, gcvFEATURE_TX_DECOMPRESSOR) == gcvFALSE) ||
                    (Sampler >= 8) ||
                    (gcoHAL_IsSwwaNeeded(gcvNULL, gcvSWWA_1165)))
                {
                    gcoSURF_DisableTileStatus(baseMipMap->surface, gcvTRUE);
                    samplerInfo.hasTileStatus = gcvFALSE;
                }
                else
                {
                    samplerInfo.hasTileStatus = !info->tileStatusDisabled;
                    samplerInfo.tileStatusAddr = info->tileStatusNode.physical;
                    samplerInfo.tileStatusClearValue = info->fcValue;
                    samplerInfo.tileStatusClearValueUpper = info->fcValueUpper;
                    samplerInfo.compressed = info->compressed;
                    samplerInfo.compressedFormat = info->compressFormat;
                }
            }
            else
            {
                samplerInfo.hasTileStatus = gcvFALSE;
            }
        }

        /* Copy the texture info. */
        samplerInfo.textureInfo = pTexParams;

        samplerInfo.filterable  = Texture->filterable;

        /* Bind to hardware. */
        status = gcoHARDWARE_BindTexture(gcvNULL, Sampler, &samplerInfo);

        if (gcmIS_ERROR(status) && (status != gcvSTATUS_NOT_SUPPORTED))
        {
#if gcdUSE_NPOT_PATCH
            if (np2_forced)
            {
                pTexParams->r = r;
                pTexParams->s = s;
                pTexParams->t = t;
                pTexParams->mipFilter = mip;
            }
#endif
            /* Error. */
            gcmFOOTER();
            return status;
        }
    }
    else
    {
        /* Unlock the texture if locked. */
        for (map = Texture->maps; map != gcvNULL; map = map->next)
        {
            if (map->locked != gcvNULL)
            {
                /* Unlock the texture surface. */
                status = gcoSURF_Unlock(map->surface, memory[0]);

                if (gcmIS_ERROR(status))
                {
#if gcdUSE_NPOT_PATCH
                    if (np2_forced)
                    {
                        pTexParams->r = r;
                        pTexParams->s = s;
                        pTexParams->t = t;
                        pTexParams->mipFilter = mip;
                    }
#endif
                    /* Error. */
                    gcmFOOTER();
                    return status;
                }

                /* Mark the surface as unlocked. */
                map->locked = gcvNULL;
            }
        }
    }

#if gcdUSE_NPOT_PATCH
    if (np2_forced)
    {
        pTexParams->r = r;
        pTexParams->s = s;
        pTexParams->t = t;
        pTexParams->mipFilter = mip;
    }
#endif
    /* If success, update info in share object. */
    Texture->Info = tmpInfo;

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

gceSTATUS
gcoTEXTURE_BindTexture(
    IN gcoTEXTURE Texture,
    IN gctINT Target,
    IN gctINT Sampler,
    IN gcsTEXTURE_PTR Info
    )
{
    return gcoTEXTURE_BindTextureEx(Texture, Target, Sampler, Info, 0);
}

/*******************************************************************************
**
**  gcoTEXTURE_InitParams
**
**  Init the client side texture parameters to default value.
**  Some clients only know part of HAL define texture parameters, it doesn't make
**  sense for client to initial its unknown parameters.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object.
**
**      gcsTEXTURE_PTR TexParams
**          Pointer to gcsTEXTURE, which contains HAL tex params but allocated by clients.
*/

gceSTATUS
gcoTEXTURE_InitParams(
    IN gcoHAL Hal,
    IN gcsTEXTURE_PTR TexParams
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hal=0x%x TexParams=0x%x", Hal, TexParams);

    if (TexParams)
    {
        gcoOS_ZeroMemory(TexParams, sizeof(gcsTEXTURE));
        TexParams->s                                = gcvTEXTURE_WRAP;
        TexParams->t                                = gcvTEXTURE_WRAP;
        TexParams->r                                = gcvTEXTURE_WRAP;
        TexParams->swizzle[gcvTEXTURE_COMPONENT_R]  = gcvTEXTURE_SWIZZLE_R;
        TexParams->swizzle[gcvTEXTURE_COMPONENT_G]  = gcvTEXTURE_SWIZZLE_G;
        TexParams->swizzle[gcvTEXTURE_COMPONENT_B]  = gcvTEXTURE_SWIZZLE_B;
        TexParams->swizzle[gcvTEXTURE_COMPONENT_A]  = gcvTEXTURE_SWIZZLE_A;
        TexParams->border[gcvTEXTURE_COMPONENT_R]   = 0;
        TexParams->border[gcvTEXTURE_COMPONENT_G]   = 0;
        TexParams->border[gcvTEXTURE_COMPONENT_B]   = 0;
        TexParams->border[gcvTEXTURE_COMPONENT_A]   = 0;
        TexParams->minFilter                        = gcvTEXTURE_POINT;
        TexParams->mipFilter                        = gcvTEXTURE_LINEAR;
        TexParams->magFilter                        = gcvTEXTURE_LINEAR;
        TexParams->anisoFilter                      = 1;
        TexParams->lodBias                          = 0.0f;
        TexParams->lodMin                           = -1000.0f;
        TexParams->lodMax                           = 1000.0f;
        TexParams->compareMode                      = gcvTEXTURE_COMPARE_MODE_NONE;
        TexParams->compareFunc                      = gcvCOMPARE_LESS_OR_EQUAL;
    }

    /* Success. */
    gcmFOOTER_NO();
    return status;
}

gceSTATUS
gcoTEXTURE_SetDepthTextureFlag(
    IN gcoTEXTURE Texture,
    IN gctBOOL  unsized
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcmHEADER_ARG("Texture=0x%x unsized=%d", Texture, unsized);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Texture, gcvOBJ_TEXTURE);

    Texture->unsizedDepthTexture = unsized;

    gcmFOOTER_NO();
    return status;

}
#else /* gcdNULL_DRIVER < 2 */


/*******************************************************************************
** NULL DRIVER STUBS
*/

gceSTATUS
gcoTEXTURE_ConstructEx(
    IN gcoHAL Hal,
    IN gceTEXTURE_TYPE Type,
    OUT gcoTEXTURE * Texture
    )
{
    *Texture = gcvNULL;
    return gcvSTATUS_OK;
}


gceSTATUS gcoTEXTURE_Construct(
    IN gcoHAL Hal,
    OUT gcoTEXTURE * Texture
    )
{
    *Texture = gcvNULL;
    return gcvSTATUS_OK;
}

gceSTATUS gcoTEXTURE_ConstructSized(
    IN gcoHAL Hal,
    IN gceSURF_FORMAT Format,
    IN gctUINT Width,
    IN gctUINT Height,
    IN gctUINT Depth,
    IN gctUINT Faces,
    IN gctUINT MipMapCount,
    IN gcePOOL Pool,
    OUT gcoTEXTURE * Texture
    )
{
    *Texture = gcvNULL;
    return gcvSTATUS_OK;
}

gceSTATUS gcoTEXTURE_Destroy(
    IN gcoTEXTURE Texture
    )
{
    return gcvSTATUS_OK;
}

gceSTATUS gcoTEXTURE_Upload(
    IN gcoTEXTURE Texture,
    IN gctINT MipMap,
    IN gceTEXTURE_FACE Face,
    IN gctSIZE_T Width,
    IN gctSIZE_T Height,
    IN gctUINT Slice,
    IN gctCONST_POINTER Memory,
    IN gctSIZE_T Stride,
    IN gceSURF_FORMAT Format,
    IN gceSURF_COLOR_SPACE SrcColorSpace
    )
{
    return gcvSTATUS_OK;
}

gceSTATUS
gcoTEXTURE_UploadSub(
    IN gcoTEXTURE Texture,
    IN gctINT MipMap,
    IN gceTEXTURE_FACE Face,
    IN gctSIZE_T XOffset,
    IN gctSIZE_T YOffset,
    IN gctSIZE_T Width,
    IN gctSIZE_T Height,
    IN gctUINT Slice,
    IN gctCONST_POINTER Memory,
    IN gctSIZE_T Stride,
    IN gceSURF_FORMAT Format,
    IN gceSURF_COLOR_SPACE SrcColorSpace,
    IN gctUINT32 PhysicalAddress
    )
{
    return gcvSTATUS_OK;
}

gceSTATUS gcoTEXTURE_GetMipMap(
    IN gcoTEXTURE Texture,
    IN gctUINT MipMap,
    OUT gcoSURF * Surface
    )
{
    *Surface = gcvNULL;
    return gcvSTATUS_OK;
}

gceSTATUS gcoTEXTURE_GetMipMapFace(
    IN gcoTEXTURE Texture,
    IN gctUINT MipMap,
    IN gceTEXTURE_FACE Face,
    OUT gcoSURF * Surface,
    OUT gctSIZE_T_PTR Offset
    )
{
    *Surface = gcvNULL;
    *Offset = 0;
    return gcvSTATUS_OK;
}

gceSTATUS
gcoTEXTURE_GetMipMapSlice(
    IN gcoTEXTURE Texture,
    IN gctUINT MipMap,
    IN gctUINT Slice,
    OUT gcoSURF * Surface,
    OUT gctSIZE_T_PTR Offset
    )
{
    *Surface = gcvNULL;
    *Offset = 0;
    return gcvSTATUS_OK;
}

gceSTATUS gcoTEXTURE_AddMipMap(
    IN gcoTEXTURE Texture,
    IN gctINT Level,
    IN gctINT InternalFormat,
    IN gceSURF_FORMAT Format,
    IN gctSIZE_T Width,
    IN gctSIZE_T Height,
    IN gctSIZE_T Depth,
    IN gctUINT Faces,
    IN gcePOOL Pool,
    OUT gcoSURF * Surface
    )
{
    if (Surface)
    {
        *Surface = gcvNULL;
    }
    return gcvSTATUS_OK;
}

gceSTATUS gcoTEXTURE_AddMipMapWithFlag(
    IN gcoTEXTURE Texture,
    IN gctINT Level,
    IN gctINT InternalFormat,
    IN gceSURF_FORMAT Format,
    IN gctSIZE_T Width,
    IN gctSIZE_T Height,
    IN gctSIZE_T Depth,
    IN gctUINT Faces,
    IN gcePOOL Pool,
    IN gctBOOL Protected,
    OUT gcoSURF * Surface
    )
{
    if (Surface)
    {
        *Surface = gcvNULL;
    }
    return gcvSTATUS_OK;
}

gceSTATUS gcoTEXTURE_AddMipMapFromClient(
    IN gcoTEXTURE Texture,
    IN gctINT Level,
    IN gcoSURF Surface
    )
{
    return gcvSTATUS_OK;
}

gceSTATUS gcoTEXTURE_AddMipMapFromSurface(
    IN gcoTEXTURE Texture,
    IN gctINT     Level,
    IN gcoSURF    Surface
    )
{
    return gcvSTATUS_OK;
}

gceSTATUS gcoTEXTURE_SetEndianHint(
    IN gcoTEXTURE Texture,
    IN gceENDIAN_HINT EndianHint
    )
{
    return gcvSTATUS_OK;
}

gceSTATUS gcoTEXTURE_Disable(
    IN gcoHAL Hal,
    IN gctINT Sampler
    )
{
    return gcvSTATUS_OK;
}

gceSTATUS gcoTEXTURE_Flush(
    IN gcoTEXTURE Texture
    )
{
    return gcvSTATUS_OK;
}

gceSTATUS gcoTEXTURE_QueryCaps(
    IN  gcoHAL    Hal,
    OUT gctUINT * MaxWidth,
    OUT gctUINT * MaxHeight,
    OUT gctUINT * MaxDepth,
    OUT gctBOOL * Cubic,
    OUT gctBOOL * NonPowerOfTwo,
    OUT gctUINT * VertexSamplers,
    OUT gctUINT * PixelSamplers
    )
{
    *MaxWidth = 8192;
    *MaxHeight = 8192;
    *MaxDepth = 8192;
    *Cubic = gcvTRUE;
    *NonPowerOfTwo = gcvTRUE;
    if (VertexSamplers != gcvNULL)
    {
        *VertexSamplers = 4;
    }
    *PixelSamplers = 8;
    return gcvSTATUS_OK;
}

gceSTATUS gcoTEXTURE_GetClosestFormat(
    IN gcoHAL Hal,
    IN gceSURF_FORMAT InFormat,
    OUT gceSURF_FORMAT* OutFormat
    )
{
    *OutFormat = InFormat;
    return gcvSTATUS_OK;
}

gceSTATUS gcoTEXTURE_GetClosestFormatEx(
    IN gcoHAL Hal,
    IN gceSURF_FORMAT InFormat,
    IN gceTEXTURE_TYPE TextureType,
    OUT gceSURF_FORMAT* OutFormat
    )
{
    *OutFormat = InFormat;
    return gcvSTATUS_OK;
}

gceSTATUS gcoTEXTURE_UploadCompressed(
    IN gcoTEXTURE Texture,
    IN gctINT MipMap,
    IN gceTEXTURE_FACE Face,
    IN gctSIZE_T Width,
    IN gctSIZE_T Height,
    IN gctUINT Slice,
    IN gctCONST_POINTER Memory,
    IN gctSIZE_T Size
    )
{
    return gcvSTATUS_OK;
}

gceSTATUS
gcoTEXTURE_UploadCompressedSub(
    IN gcoTEXTURE Texture,
    IN gctINT MipMap,
    IN gceTEXTURE_FACE Face,
    IN gctSIZE_T XOffset,
    IN gctSIZE_T YOffset,
    IN gctSIZE_T Width,
    IN gctSIZE_T Height,
    IN gctUINT Slice,
    IN gctCONST_POINTER Memory,
    IN gctSIZE_T Size
    )
{
    return gcvSTATUS_OK;
}

gceSTATUS gcoTEXTURE_RenderIntoMipMap(
    IN gcoTEXTURE Texture,
    IN gctINT Level
    )
{
    return gcvSTATUS_OK;
}

gceSTATUS gcoTEXTURE_RenderIntoMipMap2(
    IN gcoTEXTURE Texture,
    IN gctINT Level,
    IN gctBOOL Sync
    )
{
    return gcvSTATUS_OK;
}


gceSTATUS gcoTEXTURE_IsRenderable(
    IN gcoTEXTURE Texture,
    IN gctUINT Level
    )
{
    return gcvSTATUS_OK;
}

gceSTATUS gcoTEXTURE_IsComplete(
    IN gcoTEXTURE Texture,
    IN gcsTEXTURE_PTR Info,
    IN gctINT BaseLevel,
    IN gctINT MaxLevel
    )
{
    return gcvSTATUS_OK;
}

gceSTATUS gcoTEXTURE_BindTexture(
    IN gcoTEXTURE Texture,
    IN gctINT Target,
    IN gctINT Sampler,
    IN gcsTEXTURE_PTR Info
    )
{
    return gcvSTATUS_OK;
}

gceSTATUS
gcoTEXTURE_InitParams(
    IN gcoHAL Hal,
    IN gcsTEXTURE_PTR TexParams
    )
{
    return gcvSTATUS_OK;
}

gceSTATUS
gcoTEXTURE_SetDepthTextureFlag(
    IN gcoTEXTURE Texture,
    IN gctBOOL  unsized
    )
{
    return gcvSTATUS_OK;
}


#endif /* gcdNULL_DRIVER < 2 */
#endif

