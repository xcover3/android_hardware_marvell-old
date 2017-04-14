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

#define GLOW_DEFAULT_CONTRAST       10.0f
#define GLOW_DEFAULT_BRIGHTNESS     20.0f

#define GLOW_MIN_CONTRAST           -100.0f
#define GLOW_MAX_CONTRAST           100.0f

#define GLOW_MIN_BRIGHTNESS         -100.0f
#define GLOW_MAX_BRIGHTNESS         100.0f

#define GLOW_BLUR_PASS_NUM               2

typedef struct _GLOW_TEXTURE_WRAPPER{
    GLuint      id;
    CEUuint     width;
    CEUuint     height;
}glow_texture_wrapper_t;

typedef struct _effect_glow{
    GLuint pass0ProgObj;
    GLuint pass0VertShader;
    GLuint pass0FragShader;

    GLuint pass1ProgObj;
    GLuint pass1VertShader;
    GLuint pass1FragShader;

    GLuint pass0PosLoc;
    GLuint pass0TexCoordLoc;
    GLuint pass0InImageLoc;

    GLuint pass1PosLoc;
    GLuint pass1TexCoordLoc;
    GLuint pass1InImageLoc;
    GLuint pass1BlurImageLoc;
    GLuint pass1CBLoc;  /*Contrast-Brightness*/

    /* Blur Texture. */
    glow_texture_wrapper_t blurTex[GLOW_BLUR_PASS_NUM];

    GLuint vboVert;
    GLuint vboTex;
}gpu_effect_glow_t;

static const char gPass0VertShader[] =
    "attribute vec4 vPosition;    \n"
    "attribute vec2 a_texCoord;   \n"
    "varying vec2 texCoord;       \n"
    "void main() {                \n"
    "  gl_Position = vPosition;   \n"
    "  texCoord = a_texCoord;     \n"
    "}                            \n";

static const char gPass1VertShader[] =
    "attribute vec4 vPosition;    \n"
    "attribute vec2 a_texCoord;   \n"
    "varying vec2 texCoord;       \n"
    "void main() {                \n"
    "  gl_Position = vPosition;   \n"
    "  texCoord = a_texCoord;     \n"
    "}                            \n";

static const char gPass0FragShader[] =
    "precision mediump float; \n"
    "uniform sampler2D inImage; \n"
    "varying vec2 texCoord;  \n"
    "void main(void) \n"
    "{\n"
    "  gl_FragColor = texture2D(inImage, texCoord);"
    "}";

static const char gPass1FragShader[] =
    "precision mediump float; \n"
    "uniform sampler2D inImage; \n"
    "uniform sampler2D blurImage; \n"
    "uniform vec2 ConBright; \n"
    "varying vec2 texCoord;  \n"
    "void main(void) \n"
    "{\n"
    "  vec4 origColor = texture2D( inImage, texCoord ); \n"
    "  vec4 procColor = texture2D(blurImage, texCoord); \n"
    "  procColor.rgb = (procColor.rgb - 0.5) * ConBright.x + ConBright.y; \n"
    "  procColor.rgb = 1.0 - (1.0 - origColor.rgb) * (1.0 - procColor.rgb); \n"
    "  procColor.rgb = clamp(procColor.rgb, 0.0, 1.0); \n"
    "  gl_FragColor = vec4(procColor.r, procColor.g, procColor.b, origColor.a); \n"
    "}";

/* Internal function prototype for glow effect. */
ceuSTATUS _effect_glow_render_blur_texture(CEUContext* pContext);
ceuSTATUS _effect_glow_set_pass1_data(CEUContext* pContext);

/* Initialization for glow effect. */
ceuSTATUS gpu_ceu_effect_glow_init(
    CEUContext* pContext
    )
{
    gpu_effect_glow_t* pEffectData = NULL;
    if(GPU_VIR_NULL != pContext->effectParam[GPU_EFFECT_GLOW])
    {
        /*Already Initialized.*/
        return ceuSTATUS_SUCCESS;
    }

    pEffectData = (gpu_effect_glow_t*)gpu_ceu_allocate(sizeof(gpu_effect_glow_t));
    if(GPU_VIR_NULL == (gpu_vir_addr_t)pEffectData)
    {
        /*Fail allocate memory.*/
        return ceuSTATUS_FAILED;
    }
    gpu_ceu_memset((void*)pEffectData, 0, sizeof(gpu_effect_glow_t));

    pContext->effectParam[GPU_EFFECT_GLOW] = (void*)pEffectData;

    TIME_START();
    pEffectData->pass0VertShader = _ceu_load_shader(GL_VERTEX_SHADER, gPass0VertShader);
    pEffectData->pass0FragShader = _ceu_load_shader(GL_FRAGMENT_SHADER, gPass0FragShader);
    pEffectData->pass1VertShader = _ceu_load_shader(GL_VERTEX_SHADER, gPass1VertShader);
    pEffectData->pass1FragShader = _ceu_load_shader(GL_FRAGMENT_SHADER, gPass1FragShader);
    pEffectData->pass0ProgObj = _ceu_create_program(pEffectData->pass0VertShader, pEffectData->pass0FragShader);
    pEffectData->pass1ProgObj = _ceu_create_program(pEffectData->pass1VertShader, pEffectData->pass1FragShader);
    TIME_END("glow Shader Compile");

    pEffectData->pass0PosLoc = glGetAttribLocation(pEffectData->pass0ProgObj, "vPosition");
    pEffectData->pass0TexCoordLoc = glGetAttribLocation(pEffectData->pass0ProgObj, "a_texCoord");
    pEffectData->pass0InImageLoc = glGetUniformLocation(pEffectData->pass0ProgObj, "inImage");

    pEffectData->pass1PosLoc = glGetAttribLocation(pEffectData->pass1ProgObj, "vPosition");
    pEffectData->pass1TexCoordLoc = glGetAttribLocation(pEffectData->pass1ProgObj, "a_texCoord");
    pEffectData->pass1InImageLoc = glGetUniformLocation(pEffectData->pass1ProgObj, "inImage");
    pEffectData->pass1BlurImageLoc = glGetUniformLocation(pEffectData->pass1ProgObj, "blurImage");
    pEffectData->pass1CBLoc = glGetUniformLocation(pEffectData->pass1ProgObj, "ConBright");

    for(int i = 0; i < GLOW_BLUR_PASS_NUM; i++)
    {
        glGenTextures(1, &(pEffectData->blurTex[i].id));
    }

    /*Generate VBOs, and upload corresponding data.*/
    glGenBuffers(1, &(pEffectData->vboVert));
    glGenBuffers(1, &(pEffectData->vboTex));

    return ceuSTATUS_SUCCESS;  /*Need to be refine later*/
}

/* Clean up for glow effect. */
ceuSTATUS gpu_ceu_effect_glow_deinit(
    CEUContext* pContext
    )
{
    if(GPU_VIR_NULL == pContext->effectParam[GPU_EFFECT_GLOW])
    {
        /*Already De-Initialized.*/
        return ceuSTATUS_SUCCESS;
    }

    gpu_effect_glow_t* pEffectData = (gpu_effect_glow_t*)pContext->effectParam[GPU_EFFECT_GLOW];

    glUseProgram(0);

    glDisableVertexAttribArray ( pEffectData->pass1PosLoc);
    glDisableVertexAttribArray ( pEffectData->pass1TexCoordLoc);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDeleteBuffers(1, &(pEffectData->vboVert));
    glDeleteBuffers(1, &(pEffectData->vboTex));


    for(int i = 0; i < GLOW_BLUR_PASS_NUM; i++)
    {
        glDeleteTextures(1, &(pEffectData->blurTex[i].id));
    }

    glDetachShader(pEffectData->pass0ProgObj, pEffectData->pass0VertShader);
    glDetachShader(pEffectData->pass0ProgObj, pEffectData->pass0FragShader);

    glDeleteShader(pEffectData->pass0VertShader);
    glDeleteShader(pEffectData->pass0FragShader);
    glDeleteProgram(pEffectData->pass0ProgObj);

    glDetachShader(pEffectData->pass1ProgObj, pEffectData->pass1VertShader);
    glDetachShader(pEffectData->pass1ProgObj, pEffectData->pass1FragShader);

    glDeleteShader(pEffectData->pass1VertShader);
    glDeleteShader(pEffectData->pass1FragShader);
    glDeleteProgram(pEffectData->pass1ProgObj);

    gpu_ceu_free(pContext->effectParam[GPU_EFFECT_GLOW]);
    pContext->effectParam[GPU_EFFECT_GLOW] = GPU_VIR_NULL;

    return ceuSTATUS_SUCCESS;
}

/* Drawing for glow effect. */
ceuSTATUS gpu_ceu_render_glow(
    CEUContext* pContext
    )
{
    gpu_effect_glow_t* pEffectData = (gpu_effect_glow_t*)pContext->effectParam[GPU_EFFECT_GLOW];

    _effect_glow_render_blur_texture(pContext);

    _effect_glow_set_pass1_data(pContext);

    gpu_ceu_render_common(pContext);

    return ceuSTATUS_SUCCESS;
}

ceuSTATUS _effect_glow_render_blur_texture(CEUContext* pContext)
{
    gpu_effect_glow_t* pEffectData = (gpu_effect_glow_t*)pContext->effectParam[GPU_EFFECT_GLOW];

    glUseProgram(pEffectData->pass0ProgObj);

    /* Pass blur don't do block dividing. */
    const static GLfloat vertPos[] = {
        -1.0f,  -1.0f,
        -1.0f,  1.0f,
        1.0f,   1.0f,
        1.0f,   -1.0f,
    };

    const static GLfloat texCoord[] = {
        0.0f,   0.0f,
        0.0f,   1.0f,
        1.0f,   1.0f,
        1.0f,   0.0f,
    };

    glEnableVertexAttribArray(pEffectData->pass0PosLoc);
    glEnableVertexAttribArray(pEffectData->pass0TexCoordLoc);

    /* No VBO needed when render the blur texture. */
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glVertexAttribPointer(pEffectData->pass0PosLoc, 2, GL_FLOAT, GL_FALSE, 0, vertPos);
    glVertexAttribPointer(pEffectData->pass0TexCoordLoc, 2, GL_FLOAT, GL_FALSE, 0, texCoord);

    CEUuint inWidth = pContext->srcWidth;
    CEUuint inHeight = pContext->srcHeight;
    for(int i = 0; i < GLOW_BLUR_PASS_NUM; i++)
    {
        inWidth /= 2;
        inHeight /= 2;

        glActiveTexture(GL_TEXTURE1 + i);
        glBindTexture(GL_TEXTURE_2D, pEffectData->blurTex[i].id);
        /* Gen blank Texture to bind to FBO. */
        if((pEffectData->blurTex[i].width != inWidth) ||
           (pEffectData->blurTex[i].height != inHeight))
        {
            pEffectData->blurTex[i].width = inWidth;
            pEffectData->blurTex[i].height = inHeight;
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pEffectData->blurTex[i].width,
                pEffectData->blurTex[i].height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
            pEffectData->blurTex[i].id, 0);
        glViewport(0, 0, pEffectData->blurTex[i].width, pEffectData->blurTex[i].height);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if(status != GL_FRAMEBUFFER_COMPLETE)
        {
            GPU_CEU_ERROR_LOG(("%s: Bind Framebuffer Failed!", __FUNCTION__));
            return ceuSTATUS_FAILED;
        }

        /* Set Uniform data. */
        glUniform1i(pEffectData->pass0InImageLoc, i);

        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glFinish();
    }

    glDisableVertexAttribArray(pEffectData->pass0PosLoc);
    glDisableVertexAttribArray(pEffectData->pass0TexCoordLoc);

    if(glGetError() != GL_NO_ERROR)
    {
        GPU_CEU_ERROR_LOG(("%s: Failed!", __FUNCTION__));
        return ceuSTATUS_FAILED;
    }

    return ceuSTATUS_SUCCESS;
}

ceuSTATUS _effect_glow_set_pass1_data(CEUContext* pContext)
{
    gpu_effect_glow_t* pEffectData = (gpu_effect_glow_t*)pContext->effectParam[GPU_EFFECT_GLOW];

    glUseProgram(pEffectData->pass1ProgObj);

    /* attach render buffer to frame buffer */
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                              GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER,
                              pContext->renderBuffer);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(status != GL_FRAMEBUFFER_COMPLETE)
    {
        GPU_CEU_ERROR_LOG(("%s: Bind Framebuffer Failed!", __FUNCTION__));
        return ceuSTATUS_FAILED;
    }

    glViewport(0, 0, pContext->renderBufferWidth, pContext->renderBufferHeight);

    /* Set the vertex data. */
    glEnableVertexAttribArray(pEffectData->pass1PosLoc);
    glEnableVertexAttribArray(pEffectData->pass1TexCoordLoc);

    glBindBuffer(GL_ARRAY_BUFFER, pEffectData->vboVert);
    glBufferData(GL_ARRAY_BUFFER, pContext->nTotalBlocks * 4 * 2 * sizeof(GLfloat),
                    pContext->blockVertexPos, GL_STREAM_DRAW);
    glVertexAttribPointer(pEffectData->pass1PosLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);

    glBindBuffer(GL_ARRAY_BUFFER, pEffectData->vboTex);
    glBufferData(GL_ARRAY_BUFFER, pContext->nTotalBlocks * 4 * 2 * sizeof(GLfloat),
                    pContext->blockTexCoords, GL_STREAM_DRAW);
    glVertexAttribPointer(pEffectData->pass1TexCoordLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);

    /*Update the contrast and brightness.*/
    float contrast, brightness;
    if(pContext->pEffectUserParam == NULL)
    {
        contrast = GLOW_DEFAULT_CONTRAST;
        brightness = GLOW_DEFAULT_BRIGHTNESS;
    }
    else
    {
        CEU_PARAM_GLOW* pUserParam = (CEU_PARAM_GLOW*)pContext->pEffectUserParam;

        contrast = CEU_CLAMP(pUserParam->contrast, GLOW_MIN_CONTRAST, GLOW_MAX_CONTRAST);
        brightness = CEU_CLAMP(pUserParam->brightness, GLOW_MIN_BRIGHTNESS, GLOW_MAX_BRIGHTNESS);
    }

    glActiveTexture(GL_TEXTURE0 + GLOW_BLUR_PASS_NUM);
    glBindTexture(GL_TEXTURE_2D, pEffectData->blurTex[GLOW_BLUR_PASS_NUM - 1].id);

    /* Set Uniform data. */
    glUniform1i(pEffectData->pass1InImageLoc, 0);
    glUniform1i(pEffectData->pass1BlurImageLoc, GLOW_BLUR_PASS_NUM);
    glUniform2f(pEffectData->pass1CBLoc,
                (contrast + 100.0f) / 100.0f,
                brightness / 255.0f + 0.5f);

    if(glGetError() != GL_NO_ERROR)
    {
        GPU_CEU_ERROR_LOG(("%s: Failed!", __FUNCTION__));
        return ceuSTATUS_FAILED;
    }

    return ceuSTATUS_SUCCESS;
}

