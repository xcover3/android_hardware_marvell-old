/*
 * (C) Copyright 2011 Marvell International Ltd.
 * All Rights Reserved
 */

#ifndef __CAM_OSAL_H__
#define __CAM_OSAL_H__

#ifdef __cplusplus
extern "C"
{
#endif

// thread
#define CAM_THREAD_CREATE_JOINABLE 0
#define CAM_THREAD_CREATE_DETACHED 1

#define CAM_THREAD_POLICY_NORMAL   0
#define CAM_THREAD_POLICY_FIFO     1
#define CAM_THREAD_POLICY_RR       2

#define CAM_THREAD_PRIORITY_LOW    0
#define CAM_THREAD_PRIORITY_NORMAL 1
#define CAM_THREAD_PRIORITY_HIGH   2

typedef struct
{
	int          iDetachState;
	int          iPolicy;
	int          iPriority;
	void         *hEventWakeup;    // event to activate thread
	void         *hEventExitAck;   // event to notify manager thread that the work thread has exited
} CAM_ThreadAttribute;

typedef struct
{
	long                iThreadID;         // thread ID
	CAM_ThreadAttribute stThreadAttribute;
	int                 bExitThread;       // flag to notify the work thread to exit
	void                *pPrivateData;
} CAM_Thread;

int CAM_ThreadCreate( CAM_Thread *pThread, void *pFunc, void *pParam );

int CAM_ThreadDestroy( CAM_Thread *pThread );

int CAM_ThreadSetAttribute( CAM_Thread *pThread, CAM_ThreadAttribute *pThreadAttribute );

int CAM_ThreadGetAttribute( CAM_Thread *pThread, CAM_ThreadAttribute *pThreadAttribute );

int CAM_Sleep( unsigned long long uSec );

// mutex
int CAM_MutexCreate( void **phMutex );

int CAM_MutexDestroy( void **phMutex );

int CAM_MutexLock( void *hMutex, unsigned int mSec, int *pbTimedOut );

int CAM_MutexUnlock( void *hMutex );

// event
#define CAM_INFINITE_WAIT 0xFFFFFFFF

int CAM_EventCreate( void **phEvent );

int CAM_EventDestroy( void **phEvent );

int CAM_EventWait( void *hEvent, unsigned int mSec, int *pbTimedOut );

int CAM_EventReset( void *hEvent );

int CAM_EventSet( void *hEvent );

int CAM_EventBroadcast( void *hEvent );

// profiling
long long CAM_TimeGetTickCount();

// physical continuous memory
#if 1
#include "phycontmem.h"

#define OSALBMM_ATTR_DEFAULT                    PHY_CONT_MEM_ATTR_DEFAULT
#define OSALBMM_ATTR_NONCACHED                  PHY_CONT_MEM_ATTR_NONCACHED

#define OSALBMM_DMA_BIDIRECTION                 PHY_CONT_MEM_FLUSH_BIDIRECTION
#define OSALBMM_DMA_TO_DEVICE                   PHY_CONT_MEM_FLUSH_TO_DEVICE
#define OSALBMM_DMA_FROM_DEVICE                 PHY_CONT_MEM_FLUSH_FROM_DEVICE

#define osalbmm_malloc                          phy_cont_malloc
#define osalbmm_free                            phy_cont_free
#define osalbmm_get_paddr                       phy_cont_getpa
#define osalbmm_get_vaddr                       phy_cont_getva

#define osalbmm_flush_cache                     phy_cont_flush_cache
#define osalbmm_flush_cache_range               phy_cont_flush_cache_range

#else
#include "bmm_lib.h"

#define OSALBMM_ATTR_DEFAULT                    BMM_ATTR_DEFAULT
#define OSALBMM_ATTR_NONCACHED                  BMM_ATTR_NONCACHED

#define OSALBMM_DMA_BIDIRECTION                 BMM_DMA_BIDIRECTION
#define OSALBMM_DMA_TO_DEVICE                   BMM_DMA_TO_DEVICE
#define OSALBMM_DMA_FROM_DEVICE                 BMM_DMA_FROM_DEVICE

#define osalbmm_malloc                          bmm_malloc
#define osalbmm_free                            bmm_free
#define osalbmm_get_paddr                       bmm_get_paddr
#define osalbmm_get_vaddr                       bmm_get_vaddr

#define osalbmm_flush_cache                     bmm_flush_cache
#define osalbmm_flush_cache_range               bmm_flush_cache_range

#endif

#ifdef __cplusplus
}
#endif

#endif  // __CAM_OSAL_H__
