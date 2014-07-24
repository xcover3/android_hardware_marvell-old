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

#define TWIST_DEFAULT_TWIST         3.0f
#define TWIST_DEFAULT_RADIUS        0.9f

#define TWIST_MIN_TWIST             -10.0f
#define TWIST_MAX_TWIST             10.0f

#define TWIST_MIN_RADIUS            0.0f
#define TWIST_MAX_RADIUS            1.0f

typedef struct _effect_twist{
    GLuint programObject;
    GLuint vertShader;
    GLuint fragShader;
    GLuint positionLoc;
    GLuint texCoordLoc;
    GLuint samplerLoc;
    GLuint twistLoc;
    GLuint radiusLoc;

    GLuint vboVert;
    GLuint vboTex;
}gpu_effect_twist_t;

static const char gTwistVertexShader[] =
    "attribute vec4 vPosition;    \n"
    "attribute vec2 a_texCoord;   \n"
    "varying vec2 v_texCoord;     \n"
    "varying vec2 centerDiff;     \n"
    "void main() {                \n"
    "  gl_Position = vPosition;   \n"
    "  v_texCoord = a_texCoord;   \n"
    "  centerDiff = v_texCoord - vec2(0.5); \n"
    "}                            \n";

static const char gTwistFragmentShader[] =
    "precision lowp float;              \n"
    "varying lowp vec2 v_texCoord;      \n"
    "varying vec2 centerDiff;           \n"
    "uniform lowp sampler2D s_texture;  \n"
    "uniform float twist;               \n"
    "uniform float radius;              \n"
    "void main() {                      \n"
    "  vec2 uv = centerDiff;            \n"
    "  float r = length(uv);            \n"
    "  float theta;                     \n"
    "  float abs_y = abs(uv.y);         \n"
    "  float t = 1.0 - r * radius;      \n"
    "  t = (t < 0.0) ? 0.0 : (t * t * t) ; \n"
    "  if(uv.x >= 0.0) {                \n"
    "    theta = (uv.x - abs_y) / (uv.x + abs_y); \n"
    "    theta = 0.7854 - 0.7854 * theta; \n"
    "  }                                \n"
    "  else {                           \n"
    "    theta = (abs_y + uv.x) / (abs_y - uv.x); \n"
    "    theta = 2.3562 - 0.7854 * theta; \n"
    "  }                                \n"
    "  if(uv.y < 0.0) {                 \n"
    "    theta = -theta;                \n"
    "  }                                \n"
    "  theta += t * twist;              \n"
    "  uv.x = 0.5 + r * cos(theta);     \n"
    "  uv.y = 0.5 + r * sin(theta);     \n"
    "  gl_FragColor = texture2D(s_texture, uv); \n"
    "}                                  \n";

/* Internal function prototype for twist effect. */
ceuSTATUS _effect_twist_set_vertex_data(CEUContext* pContext);

/* Initialization for twist effect. */
ceuSTATUS gpu_ceu_effect_twist_init(
    CEUContext* pContext
    )
{
    gpu_effect_twist_t* pEffectData = NULL;
    if(GPU_VIR_NULL != pContext->effectParam[GPU_EFFECT_TWIST])
    {
        /*Already Initialized.*/
        pEffectData = (gpu_effect_twist_t*)pContext->effectParam[GPU_EFFECT_TWIST];

        glUseProgram(pEffectData->programObject);
        _effect_twist_set_vertex_data(pContext);

        return ceuSTATUS_SUCCESS;
    }

    pEffectData = (gpu_effect_twist_t*)gpu_ceu_allocate(sizeof(gpu_effect_twist_t));
    if(GPU_VIR_NULL == (gpu_vir_addr_t)pEffectData)
    {
        /*Fail allocate memory.*/
        return ceuSTATUS_FAILED;
    }

    pContext->effectParam[GPU_EFFECT_TWIST] = (void*)pEffectData;

    TIME_START();
    pEffectData->vertShader = _ceu_load_shader(GL_VERTEX_SHADER, gTwistVertexShader);
    pEffectData->fragShader = _ceu_load_shader(GL_FRAGMENT_SHADER, gTwistFragmentShader);
    pEffectData->programObject = _ceu_create_program(pEffectData->vertShader, pEffectData->fragShader);
    TIME_END("Twist Shader Compile");

    pEffectData->positionLoc = glGetAttribLocation(pEffectData->programObject, "vPosition");

    pEffectData->texCoordLoc = glGetAttribLocation (pEffectData->programObject, "a_texCoord");

    pEffectData->samplerLoc = glGetUniformLocation (pEffectData->programObject, "s_texture" );

    pEffectData->twistLoc = glGetUniformLocation (pEffectData->programObject, "twist" );

    pEffectData->radiusLoc = glGetUniformLocation (pEffectData->programObject, "radius" );

    glUseProgram(pEffectData->programObject);

    /*Generate VBOs, and upload corresponding data.*/
    glGenBuffers(1, &(pEffectData->vboVert));
    glGenBuffers(1, &(pEffectData->vboTex));

    _effect_twist_set_vertex_data(pContext);

    return ceuSTATUS_SUCCESS;  /*Need to be refine later*/
}

/* Clean up for twist effect. */
ceuSTATUS gpu_ceu_effect_twist_deinit(
    CEUContext* pContext
    )
{
    if(GPU_VIR_NULL == pContext->effectParam[GPU_EFFECT_TWIST])
    {
        /*Already De-Initialized.*/
        return ceuSTATUS_SUCCESS;
    }

    gpu_effect_twist_t* pEffectData = (gpu_effect_twist_t*)pContext->effectParam[GPU_EFFECT_TWIST];

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

    gpu_ceu_free(pContext->effectParam[GPU_EFFECT_TWIST]);
    pContext->effectParam[GPU_EFFECT_TWIST] = GPU_VIR_NULL;

    return ceuSTATUS_SUCCESS;
}

ceuSTATUS _effect_twist_set_vertex_data(CEUContext* pContext)
{
    gpu_effect_twist_t* pEffectData = (gpu_effect_twist_t*)pContext->effectParam[GPU_EFFECT_TWIST];

    float twist, radius;

    if(pContext->pEffectUserParam == NULL)
    {
        twist = TWIST_DEFAULT_TWIST;
        radius = TWIST_DEFAULT_RADIUS;
    }
    else
    {
        CEU_PARAM_TWIST* pTwistParam = (CEU_PARAM_TWIST*)pContext->pEffectUserParam;
        twist = CEU_CLAMP(pTwistParam->twist, TWIST_MIN_TWIST, TWIST_MAX_TWIST);
        radius = CEU_CLAMP(pTwistParam->radius, TWIST_MIN_RADIUS, TWIST_MAX_RADIUS);
    }

    glUniform1i(pEffectData->samplerLoc, 0);
    glUniform1f(pEffectData->twistLoc, twist);
    glUniform1f(pEffectData->radiusLoc, 2.0f / radius);

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

    glFinish();
    return ceuSTATUS_SUCCESS;
}

