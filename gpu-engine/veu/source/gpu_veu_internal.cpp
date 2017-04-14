/***********************************************************************************
 *
 *    Copyright (c) 2011 - 2013 by Marvell International Ltd. and its affiliates.
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

/*!
*******************************************************************************
*  \file gpu_veu_internal.cpp
*  \brief
*      Implementation of VEU internal interface
******************************************************************************/

#include "gpu_veu_internal.h"
#include <android/log.h>
static const size_t kBufferSize = 256;

#define LOG_TAG "veu"

void _veuDebugf(const char format[], ...) {
    va_list args;
    va_start(args, format);
    __android_log_vprint(ANDROID_LOG_DEBUG, LOG_TAG, format, args);
    va_end(args);
}

