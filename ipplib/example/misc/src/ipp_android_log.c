/*****************************************************************************************
Copyright (c) 2009, Marvell International Ltd.
All Rights Reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Marvell nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY MARVELL ''AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL MARVELL BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************************/

//this source code is only for android
#ifdef ANDROID
#include <stdarg.h>
#define LOG_TAG "Mrvl_IPP"
#include <cutils/log.h>

#define TMPBUF_MAXSZ	1024

void Ipp_android_logV(const char* format, ...)
{
	char buf[TMPBUF_MAXSZ];
	va_list args;
	buf[TMPBUF_MAXSZ-1] = '\0';
	va_start(args, format);
	vsprintf(buf, format, args);
	va_end(args);
	ALOGV("%s", buf);
	return;
}

void Ipp_android_logD(const char* format, ...)
{
	char buf[TMPBUF_MAXSZ];
	va_list args;
	buf[TMPBUF_MAXSZ-1] = '\0';
	va_start(args, format);
	vsprintf(buf, format, args);
	va_end(args);
	ALOGD("%s", buf);
	return;
}

void Ipp_android_logI(const char* format, ...)
{
	char buf[TMPBUF_MAXSZ];
	va_list args;
	buf[TMPBUF_MAXSZ-1] = '\0';
	va_start(args, format);
	vsprintf(buf, format, args);
	va_end(args);
	ALOGI("%s", buf);
	return;
}

void Ipp_android_logW(const char* format, ...)
{
	char buf[TMPBUF_MAXSZ];
	va_list args;
	buf[TMPBUF_MAXSZ-1] = '\0';
	va_start(args, format);
	vsprintf(buf, format, args);
	va_end(args);
	ALOGW("%s", buf);
	return;
}

void Ipp_android_logE(const char* format, ...)
{
	char buf[TMPBUF_MAXSZ];
	va_list args;
	buf[TMPBUF_MAXSZ-1] = '\0';
	va_start(args, format);
	vsprintf(buf, format, args);
	va_end(args);
	ALOGE("%s", buf);
	return;
}

int Ipp_android_ObjLogV(void* dummyobj, const char* format, ...)
{
	char buf[TMPBUF_MAXSZ];
	va_list args;
	buf[TMPBUF_MAXSZ-1] = '\0';
	va_start(args, format);
	vsprintf(buf, format, args);
	va_end(args);
	ALOGV("%s", buf);
	return 0;
}

int Ipp_android_ObjLogD(void* dummyobj, const char* format, ...)
{
	char buf[TMPBUF_MAXSZ];
	va_list args;
	buf[TMPBUF_MAXSZ-1] = '\0';
	va_start(args, format);
	vsprintf(buf, format, args);
	va_end(args);
	ALOGD("%s", buf);
	return 0;
}

int Ipp_android_ObjLogI(void* dummyobj, const char* format, ...)
{
	char buf[TMPBUF_MAXSZ];
	va_list args;
	buf[TMPBUF_MAXSZ-1] = '\0';
	va_start(args, format);
	vsprintf(buf, format, args);
	va_end(args);
	ALOGI("%s", buf);
	return 0;
}

int Ipp_android_ObjLogW(void* dummyobj, const char* format, ...)
{
	char buf[TMPBUF_MAXSZ];
	va_list args;
	buf[TMPBUF_MAXSZ-1] = '\0';
	va_start(args, format);
	vsprintf(buf, format, args);
	va_end(args);
	ALOGW("%s", buf);
	return 0;
}

int Ipp_android_ObjLogE(void* dummyobj, const char* format, ...)
{
	char buf[TMPBUF_MAXSZ];
	va_list args;
	buf[TMPBUF_MAXSZ-1] = '\0';
	va_start(args, format);
	vsprintf(buf, format, args);
	va_end(args);
	ALOGE("%s", buf);
	return 0;
}

#endif
