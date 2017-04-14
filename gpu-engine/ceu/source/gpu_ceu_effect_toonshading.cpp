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

#include <stdio.h>
#include <stdlib.h>
#include "gpu_ceu.h"
#include "gpu_ceu_internal.h"

typedef struct _effect_toonshading{
    GLuint programObject;
    GLuint vertShader;
    GLuint fragShader;
    GLuint positionLoc;
    GLuint texCoordLoc;
    GLuint samplerLoc;
    GLuint toonTexLoc;
    /* Texture to store the toon pattern */
    GLuint texToon;

    /*VBO*/
    GLuint vboVert;
    GLuint vboTex;
}gpu_effect_toonshading_t;

static const char strVertexShader[] =
    "attribute vec4 vPosition;    \n"
    "attribute vec2 a_texCoord;   \n"
    "varying vec2 v_texCoord;     \n"
    "void main() {                \n"
    "  gl_Position = vPosition;   \n"
    "  v_texCoord = a_texCoord;   \n"
    "}                            \n";

static const char strFragmentShader[] =
    "precision mediump float;      \n"
    "varying vec2 v_texCoord;      \n"
    "uniform sampler2D s_texture;  \n"
    "uniform sampler2D tex_toon;   \n"
    "void main() {                 \n"
    "  float toonFactor;           \n"
    "  vec4 imgColor;              \n"
    "  imgColor = texture2D(s_texture, v_texCoord);  \n"
    "  toonFactor = (imgColor.r + imgColor.g + imgColor.b) / 3.0 * 1.5;  \n"
    "  toonFactor = max(0.0, min(1.0, toonFactor)); \n"
    "  gl_FragColor = texture2D(tex_toon, vec2(toonFactor, 0.0));\n"
    "}                             \n";

/* Internal function prototype for toonshading effect. */
ceuSTATUS _effect_toonshading_upload_texture(GLuint count, GLuint* textures);
ceuSTATUS _effect_toonshading_set_vertex_data(CEUContext * pContext);

/* Initialization for toonshading effect. */
ceuSTATUS gpu_ceu_effect_toonshading_init(
    CEUContext* pContext
    )
{
    ceuSTATUS ret = ceuSTATUS_SUCCESS;  /*Need to be refine later*/
    gpu_effect_toonshading_t* pEffectData = NULL;

    if(GPU_VIR_NULL != pContext->effectParam[GPU_EFFECT_TOONSHADING])
    {
        /*Already Initialized.*/
        pEffectData = (gpu_effect_toonshading_t*)pContext->effectParam[GPU_EFFECT_TOONSHADING];

        glUseProgram(pEffectData->programObject);
        if(ceuSTATUS_FAILED == _effect_toonshading_set_vertex_data(pContext))
        {
            GPU_CEU_ERROR_LOG(("%s Failed!", __FUNCTION__));
            return ceuSTATUS_FAILED;
        }

        return ceuSTATUS_SUCCESS;
    }

    pEffectData = (gpu_effect_toonshading_t*)gpu_ceu_allocate(sizeof(gpu_effect_toonshading_t));
    if(GPU_VIR_NULL == (gpu_vir_addr_t)pEffectData)
    {
        /*Fail allocate memory.*/
        return ceuSTATUS_FAILED;
    }

    pContext->effectParam[GPU_EFFECT_TOONSHADING] = (void*)pEffectData;

    TIME_START();
    pEffectData->vertShader = _ceu_load_shader(GL_VERTEX_SHADER, strVertexShader);
    pEffectData->fragShader = _ceu_load_shader(GL_FRAGMENT_SHADER, strFragmentShader);
    pEffectData->programObject = _ceu_create_program(pEffectData->vertShader, pEffectData->fragShader);
    TIME_END("ToonShading Shader Compile");

    pEffectData->positionLoc = glGetAttribLocation(pEffectData->programObject, "vPosition");

    pEffectData->texCoordLoc = glGetAttribLocation (pEffectData->programObject, "a_texCoord");

    pEffectData->samplerLoc = glGetUniformLocation (pEffectData->programObject, "s_texture" );

    pEffectData->toonTexLoc = glGetUniformLocation (pEffectData->programObject, "tex_toon" );

    glUseProgram(pEffectData->programObject);

    glGenTextures(1, &(pEffectData->texToon));

    /*Generate VBOs, and upload corresponding data.*/
    glGenBuffers(1, &(pEffectData->vboVert));
    glGenBuffers(1, &(pEffectData->vboTex));

    if(ceuSTATUS_FAILED == _effect_toonshading_upload_texture(1, &(pEffectData->texToon)))
    {
        GPU_CEU_ERROR_LOG(("%s Failed!", __FUNCTION__));
        return ceuSTATUS_FAILED;
    }

    if(ceuSTATUS_FAILED == _effect_toonshading_set_vertex_data(pContext))
    {
        GPU_CEU_ERROR_LOG(("%s Failed!", __FUNCTION__));
        return ceuSTATUS_FAILED;
    }

    return ret;
}

/* Clean up for toonshading effect. */
ceuSTATUS gpu_ceu_effect_toonshading_deinit(CEUContext* pContext)
{
    if(GPU_VIR_NULL == pContext->effectParam[GPU_EFFECT_TOONSHADING])
    {
        /*Already De-Initialized.*/
        return ceuSTATUS_SUCCESS;
    }

    gpu_effect_toonshading_t* pEffectData = (gpu_effect_toonshading_t*)pContext->effectParam[GPU_EFFECT_TOONSHADING];

    glDisableVertexAttribArray ( pEffectData->positionLoc );
    glDisableVertexAttribArray ( pEffectData->texCoordLoc );

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDeleteBuffers(1, &(pEffectData->vboVert));
    glDeleteBuffers(1, &(pEffectData->vboTex));

    glDetachShader(pEffectData->programObject, pEffectData->vertShader);
    glDetachShader(pEffectData->programObject, pEffectData->fragShader);

    glDeleteShader(pEffectData->vertShader);
    glDeleteShader(pEffectData->fragShader);
    glDeleteProgram(pEffectData->programObject);

    glDeleteTextures(1, &(pEffectData->texToon));

    gpu_ceu_free(pContext->effectParam[GPU_EFFECT_TOONSHADING]);
    pContext->effectParam[GPU_EFFECT_TOONSHADING] = GPU_VIR_NULL;

    return ceuSTATUS_SUCCESS;
}

/* Upload necessary texture. */
ceuSTATUS _effect_toonshading_upload_texture(
    GLuint count,
    GLuint* textures
    )
{
    GLubyte* toonTexData = NULL;
    CEUint toonWidth, toonHeight;

    count = count;  /*Get rid of compiler warning*/

    char str[50];

    /* Load resource image from apk Package. */
    sprintf(str, "images/toon.jpg");
    if(CEU_FALSE == _ceu_load_image_as_rgb565((void**)&toonTexData, str, &toonWidth, &toonHeight))
    {
        if(toonTexData != NULL)
        {
            gpu_ceu_free((void*)toonTexData);
        }

        GPU_CEU_ERROR_LOG(("%s Failed", __FUNCTION__));

        return ceuSTATUS_FAILED;
    }

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, toonWidth, toonHeight, 0 ,
                GL_RGB, GL_UNSIGNED_SHORT_5_6_5, toonTexData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    gpu_ceu_free((void*)toonTexData);

    return ceuSTATUS_SUCCESS;
}

ceuSTATUS _effect_toonshading_set_vertex_data(
    CEUContext * pContext
    )
{
    gpu_effect_toonshading_t* pEffectData = (gpu_effect_toonshading_t*)pContext->effectParam[GPU_EFFECT_TOONSHADING];

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, pEffectData->texToon);

    glUniform1i ( pEffectData->samplerLoc, 0 );
    glUniform1i ( pEffectData->toonTexLoc, 1 );

    glEnableVertexAttribArray ( pEffectData->positionLoc );
    glEnableVertexAttribArray ( pEffectData->texCoordLoc );

    glBindBuffer(GL_ARRAY_BUFFER, pEffectData->vboVert);
    glBufferData(GL_ARRAY_BUFFER, pContext->nTotalBlocks * 4 * 2 * sizeof(GLfloat),
                    pContext->blockVertexPos, GL_STREAM_DRAW);
    glVertexAttribPointer ( pEffectData->positionLoc, 2, GL_FLOAT, GL_FALSE, 0, 0 );

    glBindBuffer(GL_ARRAY_BUFFER, pEffectData->vboTex);
    glBufferData(GL_ARRAY_BUFFER, pContext->nTotalBlocks * 4 * 2 * sizeof(GLfloat),
                    pContext->blockTexCoords, GL_STREAM_DRAW);
    glVertexAttribPointer ( pEffectData->texCoordLoc, 2, GL_FLOAT, GL_FALSE, 0, 0 );

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    return ceuSTATUS_SUCCESS;
}

