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

#define HATCHING_TEXTURE_COUNT 5

typedef struct _effect_hatching{
    GLuint programObject;
    GLuint vertShader;
    GLuint fragShader;
    GLuint positionLoc;
    GLuint texCoordLoc;
    GLuint samplerLoc;
    GLuint hatchTexLoc[HATCHING_TEXTURE_COUNT];

    /* Texture to store the hatching pattern */
    GLuint texHatch[HATCHING_TEXTURE_COUNT];

    /*VBO*/
    GLuint vboVert;
    GLuint vboTex;
}gpu_effect_hatching_t;

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
    "uniform sampler2D hatch0;     \n"
    "uniform sampler2D hatch1;     \n"
    "uniform sampler2D hatch2;     \n"
    "uniform sampler2D hatch3;     \n"
    "uniform sampler2D hatch4;     \n"
    "void main() {                 \n"
    "  vec3 weight0 = vec3(0.0);   \n"
    "  vec3 weight1 = vec3(0.0);   \n"
    "  vec4 imgColor;              \n"
    "  float hatchFactor;          \n"
    "  vec4 hatchColor0;           \n"
    "  vec4 hatchColor1;           \n"
    "  vec4 hatchColor2;           \n"
    "  vec4 hatchColor3;           \n"
    "  vec4 hatchColor4;           \n"
    "  imgColor = texture2D(s_texture, v_texCoord);  \n"
    "  hatchFactor = (imgColor.r + imgColor.g + imgColor.b) / 3.0; \n"
    "  hatchFactor = max(0.0, min(1.0, hatchFactor)); \n"
    "  hatchFactor = pow(hatchFactor, 0.6) * 5.0;  \n"
    "  hatchFactor = max(0.0, min(5.0, hatchFactor)); \n"
    "  if (hatchFactor>4.0)        \n"
    "  {                           \n"
    "    weight0.x = 1.0;          \n"
    "  } // End if                 \n"
    "  else if (hatchFactor>3.0)   \n"
    "  {                           \n"
    "    weight0.x = 1.0 - (4.0 - hatchFactor);\n"
    "    weight0.y = 1.0 - weight0.x;\n"
    "  } // End else if              \n"
    "  else if (hatchFactor>2.0)     \n"
    "  {                             \n"
    "    weight0.y = 1.0 - (3.0 - hatchFactor);\n"
    "    weight0.z = 1.0 - weight0.y;\n"
    "  } // End else if              \n"
    "  else if (hatchFactor>1.0)     \n"
    "  {                             \n"
    "    weight0.z = 1.0 - (2.0 - hatchFactor);\n"
    "    weight1.x = 1.0 - weight0.z;\n"
    "  } // End else if              \n"
    "  else     \n"
    "  {                             \n"
    "    weight1.x = 1.0 - (1.0 - hatchFactor);\n"
    "    weight1.y = 1.0 - weight1.x;\n"
    "  } // End else if              \n"
    "  hatchColor0 = texture2D(hatch0, v_texCoord) * weight0.x;\n"
    "  hatchColor1 = texture2D(hatch1, v_texCoord) * weight0.y;\n"
    "  hatchColor2 = texture2D(hatch2, v_texCoord) * weight0.z;\n"
    "  hatchColor3 = texture2D(hatch3, v_texCoord) * weight1.x;\n"
    "  hatchColor4 = texture2D(hatch4, v_texCoord) * weight1.y;\n"
    "  gl_FragColor = hatchColor0 + hatchColor1 + hatchColor2 + hatchColor3 + hatchColor4;\n"
    "}                             \n";

/* Prototype for hatching effect internal function. */
ceuSTATUS _effect_hatching_upload_texture(GLuint count, GLuint* textures);
ceuSTATUS _effect_hatching_set_vertex_data(CEUContext* pContext);

ceuSTATUS gpu_ceu_effect_hatching_init(CEUContext* pContext)
{
    ceuSTATUS ret = ceuSTATUS_SUCCESS;
    gpu_effect_hatching_t* pEffectData = NULL;
    char strName[64];

    if(GPU_VIR_NULL != pContext->effectParam[GPU_EFFECT_HATCHING])
    {
        /*Already Initialized.*/
        pEffectData = (gpu_effect_hatching_t*)pContext->effectParam[GPU_EFFECT_HATCHING];

        glUseProgram(pEffectData->programObject);
        if(ceuSTATUS_FAILED == _effect_hatching_set_vertex_data(pContext))
        {
            GPU_CEU_ERROR_LOG(("%s Failed!", __FUNCTION__));
            return ceuSTATUS_FAILED;
        }

        return ceuSTATUS_SUCCESS;
    }

    pEffectData = (gpu_effect_hatching_t*)gpu_ceu_allocate(sizeof(gpu_effect_hatching_t));
    if(GPU_VIR_NULL == (gpu_vir_addr_t)pEffectData)
    {
        /*Fail allocate memory.*/
        return ceuSTATUS_FAILED;
    }

    pContext->effectParam[GPU_EFFECT_HATCHING] = (void*)pEffectData;

    TIME_START();

    pEffectData->vertShader = _ceu_load_shader(GL_VERTEX_SHADER, strVertexShader);
    pEffectData->fragShader = _ceu_load_shader(GL_FRAGMENT_SHADER, strFragmentShader);
    pEffectData->programObject = _ceu_create_program(pEffectData->vertShader, pEffectData->fragShader);

    TIME_END("Hatching Shader Compile");

    pEffectData->positionLoc = glGetAttribLocation(pEffectData->programObject, "vPosition");

    pEffectData->texCoordLoc = glGetAttribLocation (pEffectData->programObject, "a_texCoord");

    pEffectData->samplerLoc = glGetUniformLocation (pEffectData->programObject, "s_texture" );

    for(int i = 0; i < HATCHING_TEXTURE_COUNT; i++)
    {
        sprintf(strName, "hatch%d", i);
        pEffectData->hatchTexLoc[i]= glGetUniformLocation (pEffectData->programObject, strName );
    }

    glUseProgram(pEffectData->programObject);

    glGenTextures(HATCHING_TEXTURE_COUNT, pEffectData->texHatch);

    /*Generate VBOs, and upload corresponding data.*/
    glGenBuffers(1, &(pEffectData->vboVert));
    glGenBuffers(1, &(pEffectData->vboTex));

    if(ceuSTATUS_FAILED == _effect_hatching_set_vertex_data(pContext))
    {
        GPU_CEU_ERROR_LOG(("%s Failed!", __FUNCTION__));
        return ceuSTATUS_FAILED;
    }

    return ret;
}

ceuSTATUS gpu_ceu_effect_hatching_deinit(CEUContext* pContext)
{
    if(GPU_VIR_NULL == pContext->effectParam[GPU_EFFECT_HATCHING])
    {
        /*Already De-Initialized.*/
        return ceuSTATUS_SUCCESS;
    }

    gpu_effect_hatching_t* pEffectData = (gpu_effect_hatching_t*)pContext->effectParam[GPU_EFFECT_HATCHING];

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

    glDeleteTextures(HATCHING_TEXTURE_COUNT, pEffectData->texHatch);

    gpu_ceu_free(pContext->effectParam[GPU_EFFECT_HATCHING]);
    pContext->effectParam[GPU_EFFECT_HATCHING] = GPU_VIR_NULL;

    return ceuSTATUS_SUCCESS;
}

/* Upload necessary texture. */
ceuSTATUS _effect_hatching_upload_texture(GLuint count, GLuint* textures)
{
    GLubyte* hatchTexData = NULL;
    CEUint hatchWidth, hatchHeight;

    char str[50];

    /*6 hatch texture*/
    for(unsigned int i = 0; i < count; i++)
    {
        /* Load res files from apk Package. */
        sprintf(str, "images/hatch%d.jpg", i);
        if(CEU_FALSE == _ceu_load_image_as_rgb565((void**)&hatchTexData, str, &hatchWidth, &hatchHeight))
        {
            if(hatchTexData != NULL)
            {
                gpu_ceu_free((void*)hatchTexData);
            }

            GPU_CEU_ERROR_LOG(("%s Failed", __FUNCTION__));

            return ceuSTATUS_FAILED;
        }

        glActiveTexture(GL_TEXTURE1 + i);
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, hatchWidth, hatchHeight, 0 ,
                    GL_RGB, GL_UNSIGNED_SHORT_5_6_5, hatchTexData);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glFinish();

        gpu_ceu_free((void*)hatchTexData);
        hatchTexData = NULL;
    }

    return ceuSTATUS_SUCCESS;
}

ceuSTATUS _effect_hatching_set_vertex_data(
    CEUContext* pContext
)
{
    gpu_effect_hatching_t* pEffectData = (gpu_effect_hatching_t*)pContext->effectParam[GPU_EFFECT_HATCHING];

    glUniform1i ( pEffectData->samplerLoc, 0 );
    for(int i = 0; i < HATCHING_TEXTURE_COUNT; i++)
    {
        glActiveTexture(GL_TEXTURE1 + i);
        glBindTexture(GL_TEXTURE_2D, pEffectData->texHatch[i]);
        glUniform1i ( pEffectData->hatchTexLoc[i], i + 1 );
    }

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

    if(ceuSTATUS_FAILED == _effect_hatching_upload_texture(HATCHING_TEXTURE_COUNT, pEffectData->texHatch))
    {
        GPU_CEU_ERROR_LOG(("%s Failed!", __FUNCTION__));
        return ceuSTATUS_FAILED;
    }

    return ceuSTATUS_SUCCESS;
}

