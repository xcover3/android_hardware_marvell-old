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

typedef struct _effect_oldmovie{
    GLuint programObject;
    GLuint vertShader;
    GLuint fragShader;
    GLuint positionLoc;
    GLuint texCoordLoc;
    GLuint samplerLoc;
    GLuint yuv2RgbMtxLoc;
    GLuint rgb2yLoc;

    GLuint vboVert;
    GLuint vboTex;
}gpu_effect_oldmovie_t;

static const char gOldMovieVertexShader[] =
    "attribute vec4 vPosition;    \n"
    "attribute vec2 a_texCoord;   \n"
    "varying vec2 v_texCoord;     \n"
    "void main() {                \n"
    "  gl_Position = vPosition;   \n"
    "  v_texCoord = a_texCoord;   \n"
    "}                            \n";

static const char gOldMovieFragmentShader[] =
    "precision mediump float;           \n"
    "varying mediump vec2 v_texCoord;   \n"
    "uniform lowp sampler2D s_texture;  \n"
    "uniform lowp vec4 rgb2y;           \n"
    "uniform lowp mat4 yuv2RgbMtx;      \n"
    "void main() {                      \n"
    "  lowp vec4 imgColor;              \n"
    "  lowp vec4 yuvColor = vec4(1.0, 0.457, 0.543, 1.0);\n"
    "  imgColor = texture2D(s_texture, v_texCoord);  \n"
    "  yuvColor.x = dot(rgb2y, imgColor);\n"
    "  gl_FragColor = yuv2RgbMtx * yuvColor;    \n"
    "}                             \n";

/* Prototype of old movie effect internal function. */
ceuSTATUS _effect_oldmovie_set_vertex_data(CEUContext* pContext);

static const GLfloat gYuv2RgbMtx[] = {
    1.0f,       1.0f,       1.0f,       0.0f,
    0.0f,       -0.344f,    1.770f,     0.0f,
    1.403f,     -0.714f,    0.0f,       0.0f,
    -0.7015f,   0.529f,     -0.885f,    1.0f
};

static const GLfloat gRgb2Y[] = {
    0.299f,     0.587f,     0.114f,     0.0f
};

/* Initialization for oldmovie effect. */
ceuSTATUS gpu_ceu_effect_oldmovie_init(
    CEUContext* pContext
    )
{
    gpu_effect_oldmovie_t* pEffectData = NULL;
    if(GPU_VIR_NULL != pContext->effectParam[GPU_EFFECT_OLDMOVIE])
    {
        /*Already Initialized.*/
        pEffectData = (gpu_effect_oldmovie_t*)pContext->effectParam[GPU_EFFECT_OLDMOVIE];

        glUseProgram(pEffectData->programObject);
        _effect_oldmovie_set_vertex_data(pContext);

        return ceuSTATUS_SUCCESS;
    }

    pEffectData = (gpu_effect_oldmovie_t*)gpu_ceu_allocate(sizeof(gpu_effect_oldmovie_t));
    if(GPU_VIR_NULL == (gpu_vir_addr_t)pEffectData)
    {
        /*Fail allocate memory.*/
        return ceuSTATUS_FAILED;
    }

    pContext->effectParam[GPU_EFFECT_OLDMOVIE] = (void*)pEffectData;

    TIME_START();
    pEffectData->vertShader = _ceu_load_shader(GL_VERTEX_SHADER, gOldMovieVertexShader);
    pEffectData->fragShader = _ceu_load_shader(GL_FRAGMENT_SHADER, gOldMovieFragmentShader);
    pEffectData->programObject = _ceu_create_program(pEffectData->vertShader, pEffectData->fragShader);
    TIME_END("Oldmovie Shader Compile");

    pEffectData->positionLoc = glGetAttribLocation(pEffectData->programObject, "vPosition");

    pEffectData->texCoordLoc = glGetAttribLocation (pEffectData->programObject, "a_texCoord");

    pEffectData->samplerLoc = glGetUniformLocation (pEffectData->programObject, "s_texture" );

    pEffectData->rgb2yLoc = glGetUniformLocation (pEffectData->programObject, "rgb2y" );

    pEffectData->yuv2RgbMtxLoc = glGetUniformLocation (pEffectData->programObject, "yuv2RgbMtx" );

    glUseProgram(pEffectData->programObject);

    /*Generate VBOs, and upload corresponding data.*/
    glGenBuffers(1, &(pEffectData->vboVert));
    glGenBuffers(1, &(pEffectData->vboTex));

    _effect_oldmovie_set_vertex_data(pContext);

    return ceuSTATUS_SUCCESS;  /*Need to be refine later*/
}

/* Clean up for oldmovie effect. */
ceuSTATUS gpu_ceu_effect_oldmovie_deinit(
    CEUContext* pContext
    )
{
    if(GPU_VIR_NULL == pContext->effectParam[GPU_EFFECT_OLDMOVIE])
    {
        /*Already De-Initialized.*/
        return ceuSTATUS_SUCCESS;
    }

    gpu_effect_oldmovie_t* pEffectData = (gpu_effect_oldmovie_t*)pContext->effectParam[GPU_EFFECT_OLDMOVIE];

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

    gpu_ceu_free(pContext->effectParam[GPU_EFFECT_OLDMOVIE]);
    pContext->effectParam[GPU_EFFECT_OLDMOVIE] = GPU_VIR_NULL;

    return ceuSTATUS_SUCCESS;
}

ceuSTATUS _effect_oldmovie_set_vertex_data(CEUContext* pContext)
{
    gpu_effect_oldmovie_t* pEffectData = (gpu_effect_oldmovie_t*)pContext->effectParam[GPU_EFFECT_OLDMOVIE];

    glUniform1i ( pEffectData->samplerLoc, 0 );
    glUniformMatrix4fv(pEffectData->yuv2RgbMtxLoc, 1, GL_FALSE, gYuv2RgbMtx);
    glUniform4fv(pEffectData->rgb2yLoc, 1, gRgb2Y);

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

