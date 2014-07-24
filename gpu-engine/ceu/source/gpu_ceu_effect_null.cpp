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

/* Effect specific data structure. */
typedef struct _effect_null{
    GLuint programObject;
    GLuint vertShader;
    GLuint fragShader;
    GLuint positionLoc;
    GLuint texCoordLoc;
    GLuint samplerLoc;
    GLuint vboVert;
    GLuint vboTex;
}gpu_effect_null_t;

/******************************************************************\
*****************effect vertex shader and fragment shader****************
\******************************************************************/

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
    "void main() {                 \n"
    "  gl_FragColor = texture2D(s_texture, v_texCoord);  \n"
    "}                             \n";

/* Prototype for effect internal functions. */
ceuSTATUS _effect_null_set_vertex_data(CEUContext * pContext);

/*Initialization for null effect.*/
ceuSTATUS gpu_ceu_effect_null_init(
    CEUContext* pContext
    )
{
    gpu_effect_null_t* pEffectData = NULL;
    if(GPU_VIR_NULL != pContext->effectParam[GPU_EFFECT_NULL])
    {
        /*Already Initialized.*/
        pEffectData = (gpu_effect_null_t*)pContext->effectParam[GPU_EFFECT_NULL];

        glUseProgram(pEffectData->programObject);
        _effect_null_set_vertex_data(pContext);

        return ceuSTATUS_SUCCESS;
    }

    pEffectData = (gpu_effect_null_t*)gpu_ceu_allocate(sizeof(gpu_effect_null_t));
    if(GPU_VIR_NULL == (gpu_vir_addr_t)pEffectData)
    {
        /*Fail allocate memory.*/
        return ceuSTATUS_FAILED;
    }

    pContext->effectParam[GPU_EFFECT_NULL] = (void*)pEffectData;

    TIME_START();
    pEffectData->vertShader = _ceu_load_shader(GL_VERTEX_SHADER, strVertexShader);
    pEffectData->fragShader = _ceu_load_shader(GL_FRAGMENT_SHADER, strFragmentShader);
    pEffectData->programObject = _ceu_create_program(pEffectData->vertShader, pEffectData->fragShader);
    TIME_END("NULL Shader Compile");

    pEffectData->positionLoc = glGetAttribLocation(pEffectData->programObject, "vPosition");

    pEffectData->texCoordLoc = glGetAttribLocation (pEffectData->programObject, "a_texCoord");

    pEffectData->samplerLoc = glGetUniformLocation (pEffectData->programObject, "s_texture" );

    glUseProgram(pEffectData->programObject);

    /*Generate VBOs, and upload corresponding data.*/
    glGenBuffers(1, &(pEffectData->vboVert));
    glGenBuffers(1, &(pEffectData->vboTex));

    _effect_null_set_vertex_data(pContext);

    return ceuSTATUS_SUCCESS;  /*Need to be refine later*/
}

/* Do clean up job for null effect */
ceuSTATUS gpu_ceu_effect_null_deinit(
    CEUContext* pContext
    )
{
    if(GPU_VIR_NULL == pContext->effectParam[GPU_EFFECT_NULL])
    {
        /*Already De-Initialized.*/
        return ceuSTATUS_SUCCESS;
    }

    gpu_effect_null_t* pEffectData = (gpu_effect_null_t*)pContext->effectParam[GPU_EFFECT_NULL];

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

    gpu_ceu_free(pContext->effectParam[GPU_EFFECT_NULL]);
    pContext->effectParam[GPU_EFFECT_NULL] = GPU_VIR_NULL;

    return ceuSTATUS_SUCCESS;
}

ceuSTATUS _effect_null_set_vertex_data(
    CEUContext * pContext
)
{
    gpu_effect_null_t* pEffectData = (gpu_effect_null_t*)pContext->effectParam[GPU_EFFECT_NULL];

    glUniform1i ( pEffectData->samplerLoc, 0 );

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

