/***********************************************************************************
 *
 *    Copyright (c) 2012 - 2013 by Marvell International Ltd. and its affiliates.
 *    All rights reserved.
 *
 *    This software file (the "File") is owned and distributed by Marvell
 *    International Ltd. and/or its affiliates ("Marvell") under Marvell Commercial
 *    License.
 *
 *    If you received this File from Marvell and you have entered into a commercial
 *    license agreement (a "Commercial License") with Marvell, the File is licensed
 *    to you under the terms of the applicable Commercial License.
 *
 ***********************************************************************************/

#include "gpu_ceu.h"
#include "gpu_ceu_internal.h"

#define FRAME_MAX_FRAME_NUM       3
#define FRAME_DEFAULT_FRAME       1

typedef struct _effect_frame{
    GLuint programObject;
    GLuint vertShader;
    GLuint fragShader;
    GLuint positionLoc;
    GLuint texCoordLoc;
    GLuint frameTexCoordLoc;
    GLuint inImageLoc;
    GLuint frameTexLoc;

    /* Texture to store the frame pattern */
    GLuint texFrame;

    CEUint frameNumber;

    /*VBO*/
    GLuint vboVert;
    GLuint vboTex;
    GLuint vboFrameTex;
}gpu_effect_frame_t;

static const char strVertexShader[] =
    "attribute vec4 vPosition;    \n"
    "attribute vec2 a_texCoord;   \n"
    "attribute vec2 frameTexCoord;\n"
    "varying vec2 v_texCoord;     \n"
    "varying vec2 v_frameTexCoord;\n"
    "void main() {                \n"
    "  gl_Position = vPosition;   \n"
    "  v_texCoord = a_texCoord;   \n"
    "  v_frameTexCoord = frameTexCoord;\n"
    "}                            \n";

static const char strFragmentShader[] =
    "precision mediump float;      \n"
    "varying vec2 v_texCoord;      \n"
    "varying vec2 v_frameTexCoord;\n"
    "uniform sampler2D inImage;  \n"
    "uniform sampler2D frameTex;   \n"
    "void main() {                 \n"
    "  vec4 imgColor, frameColor;               \n"
    "  vec3 mixColor;                           \n"
    "  imgColor = texture2D(inImage, v_texCoord);  \n"
    "  frameColor = texture2D(frameTex, v_frameTexCoord);  \n"
    "  mixColor.xyz = frameColor.xyz * frameColor.w + imgColor.xyz * (1.0 - frameColor.w); \n"
    "  gl_FragColor = vec4(mixColor.x, mixColor.y, mixColor.z, 1.0); \n"
    "}                             \n";

/* Internal function prototype for frame effect. */
ceuSTATUS _effect_frame_upload_texture(gpu_effect_frame_t* pEffectData);
ceuSTATUS _effect_frame_set_vertex_data(CEUContext * pContext);

/* Initialization for frame effect. */
ceuSTATUS gpu_ceu_effect_frame_init(
    CEUContext* pContext
    )
{
    ceuSTATUS ret = ceuSTATUS_SUCCESS;  /*Need to be refine later*/
    gpu_effect_frame_t* pEffectData = NULL;

    if(GPU_VIR_NULL != pContext->effectParam[GPU_EFFECT_FRAME])
    {
        /*Already Initialized.*/
        pEffectData = (gpu_effect_frame_t*)pContext->effectParam[GPU_EFFECT_FRAME];

        glUseProgram(pEffectData->programObject);
        if(ceuSTATUS_FAILED == _effect_frame_set_vertex_data(pContext))
        {
            GPU_CEU_ERROR_LOG(("%s Failed!", __FUNCTION__));
            return ceuSTATUS_FAILED;
        }

        return ceuSTATUS_SUCCESS;
    }

    pEffectData = (gpu_effect_frame_t*)gpu_ceu_allocate(sizeof(gpu_effect_frame_t));
    if(GPU_VIR_NULL == (gpu_vir_addr_t)pEffectData)
    {
        /*Fail allocate memory.*/
        return ceuSTATUS_FAILED;
    }

    pContext->effectParam[GPU_EFFECT_FRAME] = (void*)pEffectData;
    gpu_ceu_memset((void*)pEffectData, 0, sizeof(gpu_effect_frame_t));

    TIME_START();
    pEffectData->vertShader = _ceu_load_shader(GL_VERTEX_SHADER, strVertexShader);
    pEffectData->fragShader = _ceu_load_shader(GL_FRAGMENT_SHADER, strFragmentShader);
    pEffectData->programObject = _ceu_create_program(pEffectData->vertShader, pEffectData->fragShader);
    TIME_END("Frame Shader Compile");

    pEffectData->positionLoc = glGetAttribLocation(pEffectData->programObject, "vPosition");

    pEffectData->texCoordLoc = glGetAttribLocation (pEffectData->programObject, "a_texCoord");

    pEffectData->frameTexCoordLoc = glGetAttribLocation (pEffectData->programObject, "frameTexCoord");

    pEffectData->inImageLoc = glGetUniformLocation (pEffectData->programObject, "inImage" );

    pEffectData->frameTexLoc = glGetUniformLocation (pEffectData->programObject, "frameTex" );

    glUseProgram(pEffectData->programObject);

    glGenTextures(1, &(pEffectData->texFrame));

    /*Generate VBOs, and upload corresponding data.*/
    glGenBuffers(1, &(pEffectData->vboVert));
    glGenBuffers(1, &(pEffectData->vboTex));
    glGenBuffers(1, &(pEffectData->vboFrameTex));

    if(ceuSTATUS_FAILED == _effect_frame_set_vertex_data(pContext))
    {
        GPU_CEU_ERROR_LOG(("%s Failed!", __FUNCTION__));
        return ceuSTATUS_FAILED;
    }

    return ret;
}

/* Clean up for frame effect. */
ceuSTATUS gpu_ceu_effect_frame_deinit(CEUContext* pContext)
{
    if(GPU_VIR_NULL == pContext->effectParam[GPU_EFFECT_FRAME])
    {
        /*Already De-Initialized.*/
        return ceuSTATUS_SUCCESS;
    }

    gpu_effect_frame_t* pEffectData = (gpu_effect_frame_t*)pContext->effectParam[GPU_EFFECT_FRAME];

    glDisableVertexAttribArray ( pEffectData->positionLoc );
    glDisableVertexAttribArray ( pEffectData->texCoordLoc );
    glDisableVertexAttribArray ( pEffectData->frameTexCoordLoc );

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDeleteBuffers(1, &(pEffectData->vboVert));
    glDeleteBuffers(1, &(pEffectData->vboTex));
    glDeleteBuffers(1, &(pEffectData->vboFrameTex));

    glDetachShader(pEffectData->programObject, pEffectData->vertShader);
    glDetachShader(pEffectData->programObject, pEffectData->fragShader);

    glDeleteShader(pEffectData->vertShader);
    glDeleteShader(pEffectData->fragShader);
    glDeleteProgram(pEffectData->programObject);

    glDeleteTextures(1, &(pEffectData->texFrame));

    gpu_ceu_free(pContext->effectParam[GPU_EFFECT_FRAME]);
    pContext->effectParam[GPU_EFFECT_FRAME] = GPU_VIR_NULL;

    return ceuSTATUS_SUCCESS;
}

/* Upload necessary texture. */
ceuSTATUS _effect_frame_upload_texture(
    gpu_effect_frame_t* pEffectData
    )
{
    GLubyte* toonTexData = NULL;
    CEUint toonWidth, toonHeight;

    char str[50];

    /* Load resource image from apk Package. */
    sprintf(str, "images/frame%d.png", pEffectData->frameNumber);
    if(CEU_FALSE == _ceu_load_image_as_rgba8888((void**)&toonTexData, str, &toonWidth, &toonHeight))
    {
        if(toonTexData != NULL)
        {
            gpu_ceu_free((void*)toonTexData);
        }

        GPU_CEU_ERROR_LOG(("Can't open frame source file!"));

        return ceuSTATUS_FAILED;
    }

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, pEffectData->texFrame);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, toonWidth, toonHeight, 0 ,
                GL_RGBA, GL_UNSIGNED_BYTE, toonTexData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    gpu_ceu_free((void*)toonTexData);

    return ceuSTATUS_SUCCESS;
}

ceuSTATUS _effect_frame_set_vertex_data(
    CEUContext * pContext
    )
{
    ceuSTATUS status = ceuSTATUS_SUCCESS;
    gpu_effect_frame_t* pEffectData = (gpu_effect_frame_t*)pContext->effectParam[GPU_EFFECT_FRAME];

    CEUbool needUpdateTexture = CEU_FALSE;
    if(0 == pEffectData->frameNumber) /* frame texture not inited. */
    {
        if(NULL != pContext->pEffectUserParam)
        {
            CEU_PARAM_FRAME* pUserParam = (CEU_PARAM_FRAME*)pContext->pEffectUserParam;
            CEUint userFrame = CEU_CLAMP(pUserParam->frameNumber, 1, FRAME_MAX_FRAME_NUM);

            pEffectData->frameNumber = userFrame;
        }
        else
        {
            pEffectData->frameNumber = FRAME_DEFAULT_FRAME;
        }
        needUpdateTexture = CEU_TRUE;
    }
    else
    {
        if(NULL != pContext->pEffectUserParam)
        {
            CEU_PARAM_FRAME* pUserParam = (CEU_PARAM_FRAME*)pContext->pEffectUserParam;
            CEUint userFrame = CEU_CLAMP(pUserParam->frameNumber, 1, FRAME_MAX_FRAME_NUM);

            if(pEffectData->frameNumber != userFrame)
            {
                pEffectData->frameNumber = userFrame;
                needUpdateTexture = CEU_TRUE;
            }
        }
        else if(FRAME_DEFAULT_FRAME != pEffectData->frameNumber)
        {
            pEffectData->frameNumber = FRAME_DEFAULT_FRAME;
            needUpdateTexture = CEU_TRUE;
        }
    }

    if(CEU_TRUE == needUpdateTexture)
    {
        status = _effect_frame_upload_texture(pEffectData);
    }

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, pEffectData->texFrame);

    glUniform1i ( pEffectData->inImageLoc, 0 );
    glUniform1i ( pEffectData->frameTexLoc, 1 );

    glEnableVertexAttribArray ( pEffectData->positionLoc );
    glEnableVertexAttribArray ( pEffectData->texCoordLoc );
    glEnableVertexAttribArray ( pEffectData->frameTexCoordLoc );

    glBindBuffer(GL_ARRAY_BUFFER, pEffectData->vboVert);
    glBufferData(GL_ARRAY_BUFFER, pContext->nTotalBlocks * 4 * 2 * sizeof(GLfloat),
                    pContext->blockVertexPos, GL_STREAM_DRAW);
    glVertexAttribPointer ( pEffectData->positionLoc, 2, GL_FLOAT, GL_FALSE, 0, 0 );

    glBindBuffer(GL_ARRAY_BUFFER, pEffectData->vboTex);
    glBufferData(GL_ARRAY_BUFFER, pContext->nTotalBlocks * 4 * 2 * sizeof(GLfloat),
                    pContext->blockTexCoords, GL_STREAM_DRAW);
    glVertexAttribPointer ( pEffectData->texCoordLoc, 2, GL_FLOAT, GL_FALSE, 0, 0 );

    glBindBuffer(GL_ARRAY_BUFFER, pEffectData->vboFrameTex);
    glBufferData(GL_ARRAY_BUFFER, pContext->nTotalBlocks * 4 * 2 * sizeof(GLfloat),
                    pContext->blockNormalizedTexCoords, GL_STREAM_DRAW);
    glVertexAttribPointer ( pEffectData->frameTexCoordLoc, 2, GL_FLOAT, GL_FALSE, 0, 0 );

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    return status;
}

