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

#define PENCILSKETCH_DEFAULT_CONTRAST       50.0f
#define PENCILSKETCH_DEFAULT_BRIGHTNESS     -50.0f

#define PENCILSKETCH_MIN_CONTRAST           -100.0f
#define PENCILSKETCH_MAX_CONTRAST           100.0f

#define PENCILSKETCH_MIN_BRIGHTNESS         -100.0f
#define PENCILSKETCH_MAX_BRIGHTNESS         100.0f

#define PENCILSKETCH_MAX_BLUR_PASS          3
#define PENCILSKETCH_MIN_BLUR_PASS          1
#define PENCILSKETCH_DEFAULT_BLUR_PASS      1

#define PENCILSKETCH_MAX_PENCIL_SIZE        5
#define PENCILSKETCH_MIN_PENCIL_SIZE        0

typedef struct _PENCILSKETCH_TEXTURE_WRAPPER{
    GLuint      id;
    CEUuint     width;
    CEUuint     height;
}pencilsketch_texture_wrapper_t;

typedef struct _effect_pencilsketch{
    GLuint pass0ProgObj;
    GLuint pass0VertShader;
    GLuint pass0FragShader;

    GLuint pass1ProgObj;
    GLuint pass1VertShader;
    GLuint pass1FragShader;

    GLuint pass0PosLoc;
    GLuint pass0TexCoordLoc;
    GLuint pass0InImageLoc;
    GLuint pass0Rgb2GrayLoc;

    GLuint pass1PosLoc;
    GLuint pass1TexCoordLoc;
    GLuint pass1InImageLoc;
    GLuint pass1TexBiasLoc;
    GLuint pass1GradFactorLoc;

    /* Contrast-Brightness Texture. */
    //GLuint CBTex;
    CEUfloat contrast;
    CEUfloat brightness;
    CEUint blurPassNum;

    /* Blur Texture. */
    pencilsketch_texture_wrapper_t blurTex[PENCILSKETCH_MAX_BLUR_PASS];

    GLuint vboVert;
    GLuint vboTex;
}gpu_effect_pencilsketch_t;

static const char gBlurGrayVertShader[] =
    "attribute vec4 vPosition;    \n"
    "attribute vec2 a_texCoord;   \n"
    "varying vec2 texCoord;       \n"
    "void main() {                \n"
    "  gl_Position = vPosition;   \n"
    "  texCoord = a_texCoord;     \n"
    "}                            \n";

static const char gBlurGrayFragShader[] =
    "precision mediump float; \n"
    "uniform sampler2D inImage; \n"
    "uniform vec3 rgb2Gray; \n"
    "varying vec2 texCoord;  \n"
    "void main(void) \n"
    "{\n"
    "  vec3 inColor = texture2D(inImage, texCoord).rgb;\n"
    "  float gray = dot(rgb2Gray, inColor);\n"
    "  gl_FragColor = vec4(gray, gray, gray, 1.0); \n"
    "}";

static const char gEdgeVertShader[] =
    "attribute vec4 vPosition;    \n"
    "attribute vec2 a_texCoord;   \n"
    "uniform vec2 texBias;        \n"
    "varying vec4 texCoord0;      \n"
    "varying vec4 texCoord1;      \n"
    "void main() {                \n"
    "  gl_Position = vPosition;   \n"
    "  texCoord0.xy = vec2(a_texCoord.x, a_texCoord.y - texBias.y); \n"
    "  texCoord0.zw = vec2(a_texCoord.x, a_texCoord.y + texBias.y); \n"
    "  texCoord1.xy = vec2(a_texCoord.x - texBias.x, a_texCoord.y); \n"
    "  texCoord1.zw = vec2(a_texCoord.x + texBias.x, a_texCoord.y); \n"
    "}                            \n";

static const char gEdgeFragShader[] =
    "precision mediump float; \n"
    "uniform sampler2D blurImage; \n"
    "uniform vec2 gradFactor;    \n"
    "varying vec4 texCoord0;      \n"
    "varying vec4 texCoord1;      \n"
    "void main(void) \n"
    "{\n"
    "  vec4 color; \n"
    "  color.x = texture2D(blurImage, texCoord0.xy).r; \n"
    "  color.y = texture2D(blurImage, texCoord0.zw).r; \n"
    "  color.z = texture2D(blurImage, texCoord1.xy).r; \n"
    "  color.w = texture2D(blurImage, texCoord1.zw).r; \n"
    "  float grad_x = dot(gradFactor, color.xy);"
    "  float grad_y = dot(gradFactor, color.zw);"
    "  float grad = 1.0 - clamp(abs(grad_x) + abs(grad_y), 0.0, 1.0); \n"
    "  gl_FragColor = vec4(grad, grad, grad, 1.0); \n"
    "}";

static const GLfloat gRgb2Gray[] = {
    0.30f,      0.59f,      0.11f
};

static const GLfloat gradFactor[] = {
    -3.0f,      3.0f
};

/*Internal function prototype for pencilsketch.*/
ceuSTATUS _effect_pencilsketch_set_blurgray_data(CEUContext* pContext);
ceuSTATUS _effect_pencilsketch_render_blurgray_texture(CEUContext* pContext);
ceuSTATUS _effect_pencilsketch_set_edge_data(CEUContext* pContext);

/* Initialization for pencilsketch effect. */
ceuSTATUS gpu_ceu_effect_pencilsketch_init(
    CEUContext* pContext
    )
{
    gpu_effect_pencilsketch_t* pEffectData = NULL;
    if(GPU_VIR_NULL != pContext->effectParam[GPU_EFFECT_PENCILSKETCH])
    {
        /*Already Initialized.*/
        return ceuSTATUS_SUCCESS;
    }

    pEffectData = (gpu_effect_pencilsketch_t*)gpu_ceu_allocate(sizeof(gpu_effect_pencilsketch_t));
    if(GPU_VIR_NULL == (gpu_vir_addr_t)pEffectData)
    {
        /*Fail allocate memory.*/
        return ceuSTATUS_FAILED;
    }
    gpu_ceu_memset((void*)pEffectData, 0, sizeof(gpu_effect_pencilsketch_t));

    pContext->effectParam[GPU_EFFECT_PENCILSKETCH] = (void*)pEffectData;

    TIME_START();
    pEffectData->pass0VertShader = _ceu_load_shader(GL_VERTEX_SHADER, gBlurGrayVertShader);
    pEffectData->pass0FragShader = _ceu_load_shader(GL_FRAGMENT_SHADER, gBlurGrayFragShader);
    pEffectData->pass0ProgObj = _ceu_create_program(pEffectData->pass0VertShader, pEffectData->pass0FragShader);

    pEffectData->pass1VertShader = _ceu_load_shader(GL_VERTEX_SHADER, gEdgeVertShader);
    pEffectData->pass1FragShader = _ceu_load_shader(GL_FRAGMENT_SHADER, gEdgeFragShader);
    pEffectData->pass1ProgObj = _ceu_create_program(pEffectData->pass1VertShader, pEffectData->pass1FragShader);

    TIME_END("pencilsketch Shader Compile");

    pEffectData->pass0PosLoc = glGetAttribLocation(pEffectData->pass0ProgObj, "vPosition");
    pEffectData->pass0TexCoordLoc = glGetAttribLocation(pEffectData->pass0ProgObj, "a_texCoord");
    pEffectData->pass0InImageLoc = glGetUniformLocation(pEffectData->pass0ProgObj, "inImage");
    pEffectData->pass0Rgb2GrayLoc = glGetUniformLocation(pEffectData->pass0ProgObj, "rgb2Gray");

    pEffectData->pass1PosLoc = glGetAttribLocation(pEffectData->pass1ProgObj, "vPosition");
    pEffectData->pass1TexCoordLoc = glGetAttribLocation(pEffectData->pass1ProgObj, "a_texCoord");
    pEffectData->pass1InImageLoc = glGetUniformLocation(pEffectData->pass1ProgObj, "blurImage");
    pEffectData->pass1TexBiasLoc = glGetUniformLocation(pEffectData->pass1ProgObj, "texBias");
    pEffectData->pass1GradFactorLoc = glGetUniformLocation(pEffectData->pass1ProgObj, "gradFactor");

    for(int i = 0; i < PENCILSKETCH_MAX_BLUR_PASS; i++)
    {
        glGenTextures(1, &(pEffectData->blurTex[i].id));
    }

    /*Generate VBOs, and upload corresponding data.*/
    glGenBuffers(1, &(pEffectData->vboVert));
    glGenBuffers(1, &(pEffectData->vboTex));

    return ceuSTATUS_SUCCESS;  /*Need to be refine later*/
}

/* Clean up for pencilsketch effect. */
ceuSTATUS gpu_ceu_effect_pencilsketch_deinit(
    CEUContext* pContext
    )
{
    if(GPU_VIR_NULL == pContext->effectParam[GPU_EFFECT_PENCILSKETCH])
    {
        /*Already De-Initialized.*/
        return ceuSTATUS_SUCCESS;
    }

    gpu_effect_pencilsketch_t* pEffectData = (gpu_effect_pencilsketch_t*)pContext->effectParam[GPU_EFFECT_PENCILSKETCH];

    glUseProgram(0);

    glDisableVertexAttribArray ( pEffectData->pass1PosLoc);
    glDisableVertexAttribArray ( pEffectData->pass1TexCoordLoc);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDeleteBuffers(1, &(pEffectData->vboVert));
    glDeleteBuffers(1, &(pEffectData->vboTex));

    for(int i = 0; i < PENCILSKETCH_MAX_BLUR_PASS; i++)
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

    gpu_ceu_free(pContext->effectParam[GPU_EFFECT_PENCILSKETCH]);
    pContext->effectParam[GPU_EFFECT_PENCILSKETCH] = GPU_VIR_NULL;

    return ceuSTATUS_SUCCESS;
}

/* Drawing for pencilsketch effect. */
ceuSTATUS gpu_ceu_render_pencilsketch(
    CEUContext* pContext
    )
{
    gpu_effect_pencilsketch_t* pEffectData = (gpu_effect_pencilsketch_t*)pContext->effectParam[GPU_EFFECT_PENCILSKETCH];

    _effect_pencilsketch_set_blurgray_data(pContext);
    _effect_pencilsketch_render_blurgray_texture(pContext);

    _effect_pencilsketch_set_edge_data(pContext);
    gpu_ceu_render_common(pContext);

    return ceuSTATUS_SUCCESS;
}

ceuSTATUS _effect_pencilsketch_set_blurgray_data(CEUContext* pContext)
{
    gpu_effect_pencilsketch_t* pEffectData = (gpu_effect_pencilsketch_t*)pContext->effectParam[GPU_EFFECT_PENCILSKETCH];

    /* Check if need to update the contrast and brightness texture.*/
    if(pContext->pEffectUserParam == NULL)
    {
        pEffectData->blurPassNum = PENCILSKETCH_DEFAULT_BLUR_PASS;
    }
    else
    {
        CEU_PARAM_PENCILSKETCH* pUserParam = (CEU_PARAM_PENCILSKETCH*)pContext->pEffectUserParam;

        int sizePerPass = (PENCILSKETCH_MAX_PENCIL_SIZE - PENCILSKETCH_MIN_PENCIL_SIZE + 1) /
                            (PENCILSKETCH_MAX_BLUR_PASS - PENCILSKETCH_MIN_BLUR_PASS + 1);
        pEffectData->blurPassNum = CEU_CLAMP(pUserParam->pencilSize, PENCILSKETCH_MIN_PENCIL_SIZE, PENCILSKETCH_MAX_PENCIL_SIZE) / sizePerPass
                                   + PENCILSKETCH_MIN_BLUR_PASS;
    }

    return ceuSTATUS_SUCCESS;
}

ceuSTATUS _effect_pencilsketch_render_blurgray_texture(CEUContext* pContext)
{
    gpu_effect_pencilsketch_t* pEffectData = (gpu_effect_pencilsketch_t*)pContext->effectParam[GPU_EFFECT_PENCILSKETCH];

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
    for(int i = 0; i < pEffectData->blurPassNum; i++)
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
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, pEffectData->blurTex[i].width,
                pEffectData->blurTex[i].height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
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
        glUniform3fv(pEffectData->pass0Rgb2GrayLoc, 1, gRgb2Gray);

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

ceuSTATUS _effect_pencilsketch_set_edge_data(CEUContext* pContext)
{
    gpu_effect_pencilsketch_t* pEffectData = (gpu_effect_pencilsketch_t*)pContext->effectParam[GPU_EFFECT_PENCILSKETCH];

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

    CEUuint inWidth = pContext->srcWidth;
    CEUuint inHeight = pContext->srcHeight;

    glActiveTexture(GL_TEXTURE0 + pEffectData->blurPassNum);
    glBindTexture(GL_TEXTURE_2D, pEffectData->blurTex[pEffectData->blurPassNum - 1].id);

    /* Set Uniform data. */
    glUniform1i(pEffectData->pass1InImageLoc, pEffectData->blurPassNum);
    glUniform2f(pEffectData->pass1TexBiasLoc, 1.0f / inWidth, 1.0f / inHeight);

    /*Contrast and brightness to control line brightness.*/
    if(pContext->pEffectUserParam == NULL)
    {
        pEffectData->contrast = PENCILSKETCH_DEFAULT_CONTRAST;
        pEffectData->brightness = PENCILSKETCH_DEFAULT_BRIGHTNESS;
    }
    else
    {
        CEU_PARAM_PENCILSKETCH* pUserParam = (CEU_PARAM_PENCILSKETCH*)pContext->pEffectUserParam;

        pEffectData->contrast = CEU_CLAMP(pUserParam->contrast,
                                          PENCILSKETCH_MIN_CONTRAST,
                                          PENCILSKETCH_MAX_CONTRAST);
        pEffectData->brightness = CEU_CLAMP(pUserParam->brightness,
                                            PENCILSKETCH_MIN_BRIGHTNESS,
                                            PENCILSKETCH_MAX_BRIGHTNESS);
    }
    float contrast = (pEffectData->contrast - PENCILSKETCH_MIN_CONTRAST) / PENCILSKETCH_MAX_CONTRAST;
    float brightness = (pEffectData->brightness - PENCILSKETCH_MIN_BRIGHTNESS) / PENCILSKETCH_MAX_BRIGHTNESS;

    glUniform2f(pEffectData->pass1GradFactorLoc,
                gradFactor[0] * contrast + brightness,
                gradFactor[1] * contrast - brightness);

    if(glGetError() != GL_NO_ERROR)
    {
        GPU_CEU_ERROR_LOG(("%s: Failed!", __FUNCTION__));
        return ceuSTATUS_FAILED;
    }

    return ceuSTATUS_SUCCESS;
}

