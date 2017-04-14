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


#ifndef __gc_hal_user_shader_h_
#define __gc_hal_user_shader_h_

#if gcdENABLE_3D

#ifdef __cplusplus
extern "C" {
#endif


gceSTATUS gcQueryShaderCompilerHwCfg(
    IN  gcoHAL Hal,
    OUT PVSC_HW_CONFIG pVscHwCfg);

/*******************************************************************************
**                                gcLoadShaders
********************************************************************************
**
**    Load a pre-compiled and pre-linked shader program into the hardware.
**
**    INPUT:
**
**        gcoHAL Hal
**            Pointer to a gcoHAL object.
**
**        gctSIZE_T StateBufferSize
**            The number of bytes in the 'StateBuffer'.
**
**        gctPOINTER StateBuffer
**            Pointer to the states that make up the shader program.
**
**        gcsHINT_PTR Hints
**            Pointer to a gcsHINT structure that contains information required
**            when loading the shader states.
*/
gceSTATUS
gcLoadShaders(
    IN gcoHAL Hal,
    IN gctSIZE_T StateBufferSize,
    IN gctPOINTER StateBuffer,
    IN gcsHINT_PTR Hints
    );

/*******************************************************************************
**                                gcLoadKernel
********************************************************************************
**
**  Load a pre-compiled and pre-linked kernel program into the hardware.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctSIZE_T StateBufferSize
**          The number of bytes in the 'StateBuffer'.
**
**      gctPOINTER StateBuffer
**          Pointer to the states that make up the shader program.
**
**      gcsHINT_PTR Hints
**          Pointer to a gcsHINT structure that contains information required
**          when loading the shader states.
*/
gceSTATUS
gcLoadKernel(
    IN gcoHARDWARE Hardware,
    IN gctSIZE_T StateBufferSize,
    IN gctPOINTER StateBuffer,
    IN gcsHINT_PTR Hints
    );

gceSTATUS
gcInvokeThreadWalker(
    IN gcoHARDWARE Hardware,
    IN gcsTHREAD_WALKER_INFO_PTR Info
    );

gceSTATUS
gcSHADER_SpecialHint(
    IN  gcePATCH_ID patchId,
    OUT   gctUINT32_PTR hint
    );

gceSTATUS
gcoSHADER_ProgramUniform(
    IN gcoHAL Hal,
    IN gctUINT32 Address,
    IN gctUINT Columns,
    IN gctUINT Rows,
    IN gctCONST_POINTER Values,
    IN gctBOOL FixedPoint,
    IN gctBOOL ConvertToFloat,
    IN gcSHADER_KIND Type
    );

gceSTATUS
gcoSHADER_ProgramUniformEx(
    IN gcoHAL Hal,
    IN gctUINT32 Address,
    IN gctSIZE_T Columns,
    IN gctSIZE_T Rows,
    IN gctSIZE_T Arrays,
    IN gctBOOL   IsRowMajor,
    IN gctSIZE_T MatrixStride,
    IN gctSIZE_T ArrayStride,
    IN gctCONST_POINTER Values,
    IN gceUNIFORMCVT Convert,
    IN gcSHADER_KIND Type
    );

gceSTATUS
gcoSHADER_BindUniformBlock(
    IN gcoHAL Hal,
    IN gctUINT32 Address,
    IN gctUINT32 Base,
    IN gctSIZE_T Offset,
    IN gctSIZE_T Size,
    IN gcSHADER_KIND Type
    );

#ifdef __cplusplus
}
#endif

#endif /* gcdENABLE_3D */
#endif /* __gc_hal_user_shader_h_ */
