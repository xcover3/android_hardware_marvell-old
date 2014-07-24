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

typedef struct _gpu_rgb2uyvy{
    GLuint programObject;
    GLuint vertShader;
    GLuint fragShader;
    GLuint positionLoc;
    GLuint texCoordLoc;
    GLuint samplerLoc;
    GLuint tcOffsetLoc;
    GLuint rgb2yLoc;
    GLuint rgb2uLoc;
    GLuint rgb2vLoc;

    /*texture coodinate offset value.*/
    GLfloat texHorizontalOffset;

    /*VBO*/
    GLuint vboVert;
    GLuint vboTex;
}gpu_rgb2uyvy_t;

static const char gStrVertexShader[] =
    "attribute vec4 vPosition;    \n"
    "attribute vec2 a_texCoord;   \n"
    "attribute vec2 a_tcOffset;   \n"
    "varying mediump vec2 v_texCoord;  \n"
    "varying mediump vec2 v_tcNeighbor;\n"
    "void main() {                \n"
    "  gl_Position = vPosition;   \n"
    "  v_texCoord = a_texCoord;   \n"
    "  v_tcNeighbor = a_texCoord + a_tcOffset; \n"
    "}                            \n";

static const char gStrFragShader[] =
    "precision lowp float;         \n"
    "uniform lowp sampler2D s_texture;  \n"
    "uniform lowp vec4 rgb2y;      \n"
    "uniform lowp vec4 rgb2u;      \n"
    "uniform lowp vec4 rgb2v;      \n"
    "varying mediump vec2 v_texCoord;   \n"
    "varying mediump vec2 v_tcNeighbor;\n"
    "void main() {                 \n"
    "  vec4 p1rgb;                 \n"
    "  vec4 p2rgb;                 \n"
    "  p1rgb = texture2D(s_texture, v_texCoord);    \n"
    "  p2rgb = texture2D(s_texture, v_tcNeighbor);  \n"
    "  gl_FragColor.x = dot(rgb2u, p1rgb);\n"
    "  gl_FragColor.y = dot(rgb2y, p1rgb);\n"
    "  gl_FragColor.z = dot(rgb2v, p1rgb);\n"
    "  gl_FragColor.w = dot(rgb2y, p2rgb);\n"
    "}                             \n";

static const GLfloat gRgb2Y[] = {
    0.257f,     0.504f,     0.098f,     0.0625f
};

static const GLfloat gRgb2U[] = {
    -0.148,     -0.291f,    0.439f,     0.5f
};

static const GLfloat gRgb2V[] = {
    0.439f,     -0.368f,    -0.071f,    0.5f
};

/*Internal function prototype for rgb2uyvy.*/
ceuSTATUS _rgb2uyvy_set_vertex_data(CEUContext * pContext);

ceuSTATUS gpu_ceu_rgb2uyvy_init(CEUContext* pContext)
{
    gpu_rgb2uyvy_t* pEffectData = NULL;
    if(GPU_VIR_NULL != pContext->effectParam[GPU_RGB2UYVY])
    {
        /*Already Initialized.*/
        pEffectData = (gpu_rgb2uyvy_t*)pContext->effectParam[GPU_RGB2UYVY];

        glUseProgram(pEffectData->programObject);
        _rgb2uyvy_set_vertex_data(pContext);

        return ceuSTATUS_SUCCESS;
    }

    pEffectData = (gpu_rgb2uyvy_t*)gpu_ceu_allocate(sizeof(gpu_rgb2uyvy_t));
    if(GPU_VIR_NULL == (gpu_vir_addr_t)pEffectData)
    {
        /*Fail allocate memory.*/
        return ceuSTATUS_FAILED;
    }

    pContext->effectParam[GPU_RGB2UYVY] = (void*)pEffectData;

    TIME_START();
    pEffectData->vertShader = _ceu_load_shader(GL_VERTEX_SHADER, gStrVertexShader);
    pEffectData->fragShader = _ceu_load_shader(GL_FRAGMENT_SHADER, gStrFragShader);
    pEffectData->programObject = _ceu_create_program(pEffectData->vertShader, pEffectData->fragShader);
    TIME_END("rgb2uyvy Shader Compile");

    pEffectData->positionLoc = glGetAttribLocation(pEffectData->programObject, "vPosition");
    pEffectData->texCoordLoc = glGetAttribLocation(pEffectData->programObject, "a_texCoord");
    pEffectData->tcOffsetLoc = glGetAttribLocation(pEffectData->programObject, "a_tcOffset" );
    pEffectData->samplerLoc = glGetUniformLocation(pEffectData->programObject, "s_texture" );
    pEffectData->rgb2yLoc = glGetUniformLocation(pEffectData->programObject, "rgb2y" );
    pEffectData->rgb2uLoc = glGetUniformLocation(pEffectData->programObject, "rgb2u" );
    pEffectData->rgb2vLoc = glGetUniformLocation(pEffectData->programObject, "rgb2v" );

    glUseProgram(pEffectData->programObject);

    /*Generate VBOs, and upload corresponding data.*/
    glGenBuffers(1, &(pEffectData->vboVert));
    glGenBuffers(1, &(pEffectData->vboTex));

    _rgb2uyvy_set_vertex_data(pContext);

    return ceuSTATUS_SUCCESS;  /*Need to be refine later*/
}

ceuSTATUS gpu_ceu_rgb2uyvy_deinit(CEUContext* pContext)
{
    if(GPU_VIR_NULL == pContext->effectParam[GPU_RGB2UYVY])
    {
        /*Already De-Initialized.*/
        return ceuSTATUS_SUCCESS;
    }

    gpu_rgb2uyvy_t* pEffectData = (gpu_rgb2uyvy_t*)pContext->effectParam[GPU_RGB2UYVY];

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

    gpu_ceu_free(pContext->effectParam[GPU_RGB2UYVY]);
    pContext->effectParam[GPU_RGB2UYVY] = GPU_VIR_NULL;

    return ceuSTATUS_SUCCESS;
}

ceuSTATUS _rgb2uyvy_set_vertex_data(
    CEUContext * pContext
)
{
    gpu_rgb2uyvy_t* pEffectData = (gpu_rgb2uyvy_t*)pContext->effectParam[GPU_RGB2UYVY];

    switch(pContext->dstRotation)
    {
    case GPU_ROTATION_0:
        pEffectData->texHorizontalOffset = 1.0f / pContext->srcWidth;
        glVertexAttrib2f(pEffectData->tcOffsetLoc, pEffectData->texHorizontalOffset, 0.0f);
        break;

    case GPU_ROTATION_90:
        pEffectData->texHorizontalOffset = 1.0f / pContext->srcHeight;
        glVertexAttrib2f(pEffectData->tcOffsetLoc, 0.0f, pEffectData->texHorizontalOffset);
        break;

    case GPU_ROTATION_180:
        pEffectData->texHorizontalOffset = -1.0f / pContext->srcWidth;
        glVertexAttrib2f(pEffectData->tcOffsetLoc, pEffectData->texHorizontalOffset, 0.0f);
        break;

    case GPU_ROTATION_270:
        pEffectData->texHorizontalOffset = -1.0f / pContext->srcHeight;
        glVertexAttrib2f(pEffectData->tcOffsetLoc, 0.0f, pEffectData->texHorizontalOffset);
        break;

    case GPU_ROTATION_FLIP_H:
        pEffectData->texHorizontalOffset = -1.0f / pContext->srcWidth;
        glVertexAttrib2f(pEffectData->tcOffsetLoc, pEffectData->texHorizontalOffset, 0.0f);
        break;

    case GPU_ROTATION_FLIP_V:
        pEffectData->texHorizontalOffset = 1.0f / pContext->srcWidth;
        glVertexAttrib2f(pEffectData->tcOffsetLoc, pEffectData->texHorizontalOffset, 0.0f);
        break;

    default:
        GPU_CEU_ERROR_LOG(("Unsupported Rotation!"));
        return ceuSTATUS_FAILED;
    }

    glUniform1i ( pEffectData->samplerLoc, 0 );
    glUniform4fv(pEffectData->rgb2yLoc, 1, gRgb2Y);
    glUniform4fv(pEffectData->rgb2uLoc, 1, gRgb2U);
    glUniform4fv(pEffectData->rgb2vLoc, 1, gRgb2V);

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

