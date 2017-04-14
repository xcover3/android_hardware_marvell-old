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
#include <math.h>

#define SUNSHINE_TEX_SIZE               256
#define SUNSHINE_DEFAULT_SHINE_COUNT    80
#define SUNSHINE_DEFAULT_SHINE_COLOR    ( (255 << 24) | (60 << 16) | (255 << 8) | (255) )

#define SUNSHINE_MAX_SHINE_RADIUS   1.0
#define SUNSHINE_MIN_SHINE_RADIUS   0.0
#define SUNSHINE_DEFAULT_SHINE_RADIUS   0.1

#define SUNSHINE_MAX_SUN_POSITION       1.0
#define SUNSHINE_MIN_SUN_POSITION       0.0
#define SUNSHINE_DEFAULT_SUN_POSITION   0.1

typedef struct _effect_sunshine_point{
    CEUint x;
    CEUint y;
}effect_sunshine_point_t;

typedef struct _effect_sunshine{
    GLuint programObject;
    GLuint vertShader;
    GLuint fragShader;
    GLuint positionLoc;
    GLuint texCoordLoc;
    GLuint shineTexCoordLoc;
    GLuint inImageLoc;
    GLuint shineTexLoc;

    /* Sunshine texture.*/
    GLuint shineTexId;
    GLint shineTexWidth;
    GLint shineTexHeight;
    CEUbyte* shineTexData;

    effect_sunshine_point_t sunPosition;
    CEUint shineCount;
    CEUint shineRadius;
    CEUfloat* shineSpoke;
    CEUbyte shineColor[4];

    GLuint vboVert;
    GLuint vboTex;
    GLuint vboShineTex;
}gpu_effect_sunshine_t;

static const char strVertexShader[] =
    "attribute vec4 vPosition;    \n"
    "attribute vec2 a_texCoord;   \n"
    "attribute vec2 shineTexCoord;\n"
    "varying vec2 v_texCoord;     \n"
    "varying vec2 v_shineTexCoord;\n"
    "void main() {                \n"
    "  gl_Position = vPosition;   \n"
    "  v_texCoord = a_texCoord;   \n"
    "  v_shineTexCoord = shineTexCoord;\n"
    "}                            \n";

static const char strFragmentShader[] =
    "precision mediump float;      \n"
    "varying vec2 v_texCoord;      \n"
    "varying vec2 v_shineTexCoord;\n"
    "uniform sampler2D inImage;  \n"
    "uniform sampler2D shineTex;   \n"
    "void main() {                 \n"
    "  vec4 imgColor, shineColor;               \n"
    "  vec4 mixColor;                           \n"
    "  imgColor = texture2D(inImage, v_texCoord);  \n"
    "  shineColor = texture2D(shineTex, v_shineTexCoord); \n"
    "  mixColor = imgColor * (1.0 - shineColor.w) + shineColor * shineColor.w;\n"
    "  gl_FragColor = vec4(mixColor.x, mixColor.y, mixColor.z, imgColor.w); \n"
    "}                             \n";

/* Internal function prototype. */
ceuSTATUS _effect_sunshine_init_shine_texture(gpu_effect_sunshine_t* pEffectData);
ceuSTATUS _effect_sunshine_set_shine_pixel(gpu_effect_sunshine_t* pEffectData, CEUint x, CEUint y, CEUbyte * pixelRGBA);
ceuSTATUS _effect_sunshine_set_shine_spoke(CEUfloat* pSpoke);
ceuSTATUS _effect_sunshine_set_vertex_data(CEUContext* pContext);
ceuSTATUS _effect_sunshine_upload_texture(CEUContext* pContext);

/* Initialization for sunshine effect. */
ceuSTATUS gpu_ceu_effect_sunshine_init(
    CEUContext* pContext
    )
{
    ceuSTATUS ret = ceuSTATUS_SUCCESS;  /*Need to be refine later*/
    gpu_effect_sunshine_t* pEffectData = NULL;

    if(GPU_VIR_NULL != pContext->effectParam[GPU_EFFECT_SUNSHINE])
    {
        /*Already Initialized.*/
        pEffectData = (gpu_effect_sunshine_t*)pContext->effectParam[GPU_EFFECT_SUNSHINE];

        glUseProgram(pEffectData->programObject);
        if(ceuSTATUS_FAILED == _effect_sunshine_set_vertex_data(pContext))
        {
            GPU_CEU_ERROR_LOG(("%s Failed!", __FUNCTION__));
            return ceuSTATUS_FAILED;
        }

        return ceuSTATUS_SUCCESS;
    }

    pEffectData = (gpu_effect_sunshine_t*)gpu_ceu_allocate(sizeof(gpu_effect_sunshine_t));
    if(GPU_VIR_NULL == (gpu_vir_addr_t)pEffectData)
    {
        /*Fail allocate memory.*/
        return ceuSTATUS_FAILED;
    }

    gpu_ceu_memset((void*)pEffectData, 0, sizeof(gpu_effect_sunshine_t));

    pContext->effectParam[GPU_EFFECT_SUNSHINE] = (void*)pEffectData;

    TIME_START();
    pEffectData->vertShader = _ceu_load_shader(GL_VERTEX_SHADER, strVertexShader);
    pEffectData->fragShader = _ceu_load_shader(GL_FRAGMENT_SHADER, strFragmentShader);
    pEffectData->programObject = _ceu_create_program(pEffectData->vertShader, pEffectData->fragShader);
    TIME_END("SunShine Shader Compile");

    pEffectData->positionLoc = glGetAttribLocation(pEffectData->programObject, "vPosition");

    pEffectData->texCoordLoc = glGetAttribLocation (pEffectData->programObject, "a_texCoord");

    pEffectData->shineTexCoordLoc = glGetAttribLocation (pEffectData->programObject, "shineTexCoord");

    pEffectData->inImageLoc = glGetUniformLocation (pEffectData->programObject, "inImage" );

    pEffectData->shineTexLoc = glGetUniformLocation (pEffectData->programObject, "shineTex" );

    glUseProgram(pEffectData->programObject);

    glGenTextures(1, &(pEffectData->shineTexId));

    /* Init shine tex data. */
    pEffectData->shineTexWidth = SUNSHINE_TEX_SIZE;
    pEffectData->shineTexHeight = SUNSHINE_TEX_SIZE;

    pEffectData->shineColor[0] = SUNSHINE_DEFAULT_SHINE_COLOR & 0xFF;
    pEffectData->shineColor[1] = (SUNSHINE_DEFAULT_SHINE_COLOR >> 8) & 0xFF;
    pEffectData->shineColor[2] = (SUNSHINE_DEFAULT_SHINE_COLOR >> 16) & 0xFF;
    pEffectData->shineColor[3] = (SUNSHINE_DEFAULT_SHINE_COLOR >> 24) & 0xFF;

    pEffectData->shineCount = SUNSHINE_DEFAULT_SHINE_COUNT;

    /*Generate VBOs, and upload corresponding data.*/
    glGenBuffers(1, &(pEffectData->vboVert));
    glGenBuffers(1, &(pEffectData->vboTex));
    glGenBuffers(1, &(pEffectData->vboShineTex));

    if(ceuSTATUS_FAILED == _effect_sunshine_set_vertex_data(pContext))
    {
        GPU_CEU_ERROR_LOG(("%s Failed!", __FUNCTION__));
        return ceuSTATUS_FAILED;
    }

    return ret;
}

/* Clean up for sunshine effect. */
ceuSTATUS gpu_ceu_effect_sunshine_deinit(CEUContext* pContext)
{
    if(GPU_VIR_NULL == pContext->effectParam[GPU_EFFECT_SUNSHINE])
    {
        /*Already De-Initialized.*/
        return ceuSTATUS_SUCCESS;
    }

    gpu_effect_sunshine_t* pEffectData = (gpu_effect_sunshine_t*)pContext->effectParam[GPU_EFFECT_SUNSHINE];

    if(NULL != pEffectData->shineTexData)
    {
        gpu_ceu_free((void*)pEffectData->shineTexData);
        pEffectData->shineTexData = NULL;
    }

    if(NULL != pEffectData->shineSpoke)
    {
        gpu_ceu_free((void*)pEffectData->shineSpoke);
        pEffectData->shineSpoke = NULL;
    }

    glDisableVertexAttribArray ( pEffectData->positionLoc );
    glDisableVertexAttribArray ( pEffectData->texCoordLoc );
    glDisableVertexAttribArray ( pEffectData->shineTexCoordLoc );

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDeleteBuffers(1, &(pEffectData->vboVert));
    glDeleteBuffers(1, &(pEffectData->vboTex));
    glDeleteBuffers(1, &(pEffectData->vboShineTex));

    glDetachShader(pEffectData->programObject, pEffectData->vertShader);
    glDetachShader(pEffectData->programObject, pEffectData->fragShader);

    glDeleteShader(pEffectData->vertShader);
    glDeleteShader(pEffectData->fragShader);
    glDeleteProgram(pEffectData->programObject);

    glDeleteTextures(1, &(pEffectData->shineTexId));

    gpu_ceu_free(pContext->effectParam[GPU_EFFECT_SUNSHINE]);
    pContext->effectParam[GPU_EFFECT_SUNSHINE] = GPU_VIR_NULL;

    return ceuSTATUS_SUCCESS;
}

ceuSTATUS _effect_sunshine_init_shine_texture(gpu_effect_sunshine_t* pEffectData)
{
    ceuSTATUS status = ceuSTATUS_OK;

    effect_sunshine_point_t sunPosition = pEffectData->sunPosition;
    CEUuint shineCount = pEffectData->shineCount;
    CEUuint texWidth = pEffectData->shineTexWidth;
    CEUuint texHeight = pEffectData->shineTexHeight;

    unsigned int i, j;

    if(pEffectData->shineSpoke != NULL)
    {
        gpu_ceu_free((void*)(pEffectData->shineSpoke));
    }

    pEffectData->shineSpoke = (CEUfloat*)gpu_ceu_allocate(shineCount * sizeof(CEUfloat));

    for(i = 0; i < shineCount; i++)
    {
        _effect_sunshine_set_shine_spoke(pEffectData->shineSpoke + i);
    }

    if(pEffectData->shineTexData != NULL)
    {
        gpu_ceu_free((void*)pEffectData->shineTexData);
    }

    pEffectData->shineTexData = (CEUbyte*)gpu_ceu_allocate(texWidth * texHeight * 4);
    gpu_ceu_memset((void*)pEffectData->shineTexData, 0, texWidth * texHeight * 4);

    for(i = 0; i < texHeight; i++)
        for(j = 0; j < texWidth; j++)
        {
            _effect_sunshine_set_shine_pixel(pEffectData, j, i, pEffectData->shineTexData + i * texWidth * 4 + j * 4);
        }

    return status;
}

ceuSTATUS _effect_sunshine_set_shine_pixel(gpu_effect_sunshine_t* pEffectData, CEUint x, CEUint y, CEUbyte * pixelRGBA)
{
    ceuSTATUS status = ceuSTATUS_OK;
    effect_sunshine_point_t sunPosition = pEffectData->sunPosition;
    CEUint shineCount = pEffectData->shineCount;
    CEUint shineRadius = pEffectData->shineRadius;
    CEUfloat* shineSpoke = pEffectData->shineSpoke;
    CEUbyte* shineColor = pEffectData->shineColor;

    double u = (double)(x - sunPosition.x + 0.001) / shineRadius;
    double v = (double)(y - sunPosition.y + 0.001) / shineRadius;

    double t = (atan2 (u, v) / (2 * CEU_PI) + 0.51) * shineCount;
    int i = (int)floor(t) ;
    t -= i ;
    i %= shineCount;

    double w1 = shineSpoke[i] * (1-t) + shineSpoke[(i+1) % shineCount] * t ;
    w1 = w1 * w1;

    double w = 1.0f / sqrt (u*u + v*v) * 0.9;
    double fRatio = CEU_CLAMP(w, 0.0f, 1.0);

    double ws = CEU_CLAMP (w1 * w, 0.0, 1.0);

    static int tempCounter = 0;
    for (int m = 0 ; m < 4 ; m++)
    {
        double spokecol = (double)shineColor[m] / 255.0;

        double r;
        r = CEU_CLAMP(spokecol * w, 0.0, 1.0);
        r += ws ;

        pixelRGBA[m] = (CEUbyte)(CEU_CLAMP(r * 255.0, 0.0, 255.0));
    }

    return status;
}

ceuSTATUS _effect_sunshine_set_shine_spoke(CEUfloat* pSpoke)
{
    ceuSTATUS status = ceuSTATUS_OK;
    float s = 0.0f;

    for (int i=0 ; i < 6 ; i++)
    {
        s += rand() / (float)(RAND_MAX + 1.0f) ;
    }

    *pSpoke = s / 6.0 ;

    return status;
}

ceuSTATUS _effect_sunshine_set_vertex_data(CEUContext* pContext)
{
    gpu_effect_sunshine_t* pEffectData = (gpu_effect_sunshine_t*)pContext->effectParam[GPU_EFFECT_SUNSHINE];

    CEUbool needUpdateTexture = CEU_FALSE;
    if(pEffectData->shineTexData == NULL) /* sunshine texture not inited. */
    {
        if(NULL != pContext->pEffectUserParam)
        {
            CEU_PARAM_SUNSHINE* pUserParam = (CEU_PARAM_SUNSHINE*)pContext->pEffectUserParam;
            CEUint sunX = (CEUint)(CEU_CLAMP(pUserParam->sunPositionX, SUNSHINE_MIN_SUN_POSITION, SUNSHINE_MAX_SUN_POSITION)
                            * SUNSHINE_TEX_SIZE);
            CEUint sunY = (CEUint)(CEU_CLAMP(pUserParam->sunPositionY, SUNSHINE_MIN_SUN_POSITION, SUNSHINE_MAX_SUN_POSITION)
                            * SUNSHINE_TEX_SIZE);
            CEUint sunRadius = (CEUint)(CEU_CLAMP(pUserParam->sunRadius, SUNSHINE_MIN_SHINE_RADIUS, SUNSHINE_MAX_SHINE_RADIUS)
                            * SUNSHINE_TEX_SIZE);

            pEffectData->sunPosition.x = sunX;
            pEffectData->sunPosition.y = sunY;
            pEffectData->shineRadius = sunRadius;
        }
        else
        {
            /* use default parameter. */
            pEffectData->shineRadius = (CEUint)(SUNSHINE_DEFAULT_SHINE_RADIUS * SUNSHINE_TEX_SIZE);
            pEffectData->sunPosition.x = (CEUint)(SUNSHINE_DEFAULT_SUN_POSITION * SUNSHINE_TEX_SIZE);
            pEffectData->sunPosition.y = (CEUint)(SUNSHINE_DEFAULT_SUN_POSITION * SUNSHINE_TEX_SIZE);
        }

        needUpdateTexture = CEU_TRUE;
    }
    else
    {
        if(NULL != pContext->pEffectUserParam)
        {
            CEU_PARAM_SUNSHINE* pUserParam = (CEU_PARAM_SUNSHINE*)pContext->pEffectUserParam;
            CEUint sunX = (CEUint)(CEU_CLAMP(pUserParam->sunPositionX, SUNSHINE_MIN_SUN_POSITION, SUNSHINE_MAX_SUN_POSITION)
                            * SUNSHINE_TEX_SIZE);
            CEUint sunY = (CEUint)(CEU_CLAMP(pUserParam->sunPositionY, SUNSHINE_MIN_SUN_POSITION, SUNSHINE_MAX_SUN_POSITION)
                            * SUNSHINE_TEX_SIZE);
            CEUint sunRadius = (CEUint)(CEU_CLAMP(pUserParam->sunRadius, SUNSHINE_MIN_SHINE_RADIUS, SUNSHINE_MAX_SHINE_RADIUS)
                            * SUNSHINE_TEX_SIZE);

            if((sunX != pEffectData->sunPosition.x) ||
                (sunY != pEffectData->sunPosition.y) ||
                (sunRadius != pEffectData->shineRadius))
            {
                pEffectData->sunPosition.x = sunX;
                pEffectData->sunPosition.y = sunY;
                pEffectData->shineRadius = sunRadius;

                needUpdateTexture = CEU_TRUE;
            }
        }
        else
        {
            CEUint defSunRad = (CEUint)(SUNSHINE_DEFAULT_SHINE_RADIUS * SUNSHINE_TEX_SIZE);
            CEUint defSunX = (CEUint)(SUNSHINE_DEFAULT_SUN_POSITION * SUNSHINE_TEX_SIZE);
            CEUint defSunY = (CEUint)(SUNSHINE_DEFAULT_SUN_POSITION * SUNSHINE_TEX_SIZE);

            if((defSunX != pEffectData->sunPosition.x) ||
                (defSunY != pEffectData->sunPosition.y) ||
                (defSunRad != pEffectData->shineRadius))
            {
                pEffectData->sunPosition.x = defSunX;
                pEffectData->sunPosition.y = defSunY;
                pEffectData->shineRadius = defSunRad;

                needUpdateTexture = CEU_TRUE;
            }
        }
    }

    if(CEU_TRUE == needUpdateTexture)
    {
         _effect_sunshine_init_shine_texture(pEffectData);
         if(ceuSTATUS_FAILED == _effect_sunshine_upload_texture(pContext))
        {
            GPU_CEU_ERROR_LOG(("%s Failed!", __FUNCTION__));
            return ceuSTATUS_FAILED;
        }
    }

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, pEffectData->shineTexId);

    glUniform1i ( pEffectData->inImageLoc, 0 );
    glUniform1i ( pEffectData->shineTexLoc, 1 );

    glEnableVertexAttribArray ( pEffectData->positionLoc );
    glEnableVertexAttribArray ( pEffectData->texCoordLoc );
    glEnableVertexAttribArray ( pEffectData->shineTexCoordLoc );

    glBindBuffer(GL_ARRAY_BUFFER, pEffectData->vboVert);
    glBufferData(GL_ARRAY_BUFFER, pContext->nTotalBlocks * 4 * 2 * sizeof(GLfloat),
                    pContext->blockVertexPos, GL_STREAM_DRAW);
    glVertexAttribPointer ( pEffectData->positionLoc, 2, GL_FLOAT, GL_FALSE, 0, 0 );

    glBindBuffer(GL_ARRAY_BUFFER, pEffectData->vboTex);
    glBufferData(GL_ARRAY_BUFFER, pContext->nTotalBlocks * 4 * 2 * sizeof(GLfloat),
                    pContext->blockTexCoords, GL_STREAM_DRAW);
    glVertexAttribPointer ( pEffectData->texCoordLoc, 2, GL_FLOAT, GL_FALSE, 0, 0 );

    glBindBuffer(GL_ARRAY_BUFFER, pEffectData->vboShineTex);
    glBufferData(GL_ARRAY_BUFFER, pContext->nTotalBlocks * 4 * 2 * sizeof(GLfloat),
                    pContext->blockNormalizedTexCoords, GL_STREAM_DRAW);
    glVertexAttribPointer ( pEffectData->shineTexCoordLoc, 2, GL_FLOAT, GL_FALSE, 0, 0 );

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    return ceuSTATUS_SUCCESS;
}

ceuSTATUS _effect_sunshine_upload_texture(CEUContext* pContext)
{
    gpu_effect_sunshine_t* pEffectData = (gpu_effect_sunshine_t*)pContext->effectParam[GPU_EFFECT_SUNSHINE];

    CEUbyte* texData = pEffectData->shineTexData;
    CEUint texWidth = pEffectData->shineTexWidth;
    CEUint texHeight = pEffectData->shineTexHeight;

    if(NULL == texData)
    {
        GPU_CEU_ERROR_LOG(("%s Failed", __FUNCTION__));

        return ceuSTATUS_FAILED;
    }

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, pEffectData->shineTexId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texWidth, texHeight, 0 ,
                GL_RGBA, GL_UNSIGNED_BYTE, texData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    return ceuSTATUS_SUCCESS;
}

