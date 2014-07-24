// ============================================================================
// DxO Labs proprietary and confidential information
// Copyright (C) DxO Labs 1999-2010 - (All rights reserved)
// ============================================================================
/******************************************************************************
//(C) Copyright [2010 - 2011] Marvell International Ltd.
//All Rights Reserved
******************************************************************************/

#include <stdio.h>
#include <stdarg.h>

#if !defined(NDEBUG)
void DxOISP_Debug(char* format,...) {
#if defined(i386)
	extern FILE* dbgTraceFd ;
	va_list args ;
	va_start (args, format) ;
	vfprintf (dbgTraceFd,format, args) ;
	va_end (args) ;
#elif defined(__sparclet__)
	extern void cprintf(char* format, ...) ;
	va_list args ;
	va_start (args, format) ;
	cprintf (format, args) ;
	va_end (args) ;
#elif defined(LINUX)
	FILE *dbgTraceFd = stdout; 
	va_list args;
	va_start( args, format );
	fprintf( dbgTraceFd, format,args );
	va_end( args );
#endif
}
//#else
//#error "file DxOISP_Debug.c must not be compiled if NDEBUG is not defined!!!"
#endif

void cprintf(char* format, ...)
{
	va_list args ;
	va_start (args, format) ;
	printf( format, args );
	va_end (args) ;
}

void armprintf(char* format, ...)
{
	va_list args ;
	va_start (args, format) ;
	printf( format, args );
	va_end (args) ;
}


