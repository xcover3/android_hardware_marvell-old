/******************************************************************************
//(C) Copyright [2009 - 2011] Marvell International Ltd.
//All Rights Reserved
******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/syscall.h>

#if defined ( ANDROID )
#include <linux/capability.h>
#include <utils/threads.h>
#include <cutils/sched_policy.h>
#else
#include <sys/unistd.h>
#endif

#include <sys/resource.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include "CameraOSAL.h"
#include "cam_log.h"


/**********************************************************************
 * Thread
 ***********************************************************************/
static int _osal2posix_policy( int *pPosixPolicy, int iOsalPolicy )
{
	int iPolicy;

	switch ( iOsalPolicy )
	{
	case CAM_THREAD_POLICY_RR:
		iPolicy = SCHED_RR;
		break;

	case CAM_THREAD_POLICY_FIFO:
		iPolicy = SCHED_FIFO;
		break;

	case CAM_THREAD_POLICY_NORMAL:
		iPolicy = SCHED_OTHER;
		break;

	default:
		TRACE( CAM_ERROR, "Error: unsupported OSAL policy[%d]( %s, %s, %d )\n", iOsalPolicy, __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	*pPosixPolicy = iPolicy;

	return 0;
}

static int _posix2osal_policy( int *pOsalPolicy, int iPosixPolicy )
{
	int iPolicy;

	switch ( iPosixPolicy )
	{
	case SCHED_RR:
		iPolicy = CAM_THREAD_POLICY_RR;
		break;

	case SCHED_FIFO:
		iPolicy = CAM_THREAD_POLICY_FIFO;
		break;

	case SCHED_OTHER:
		iPolicy = CAM_THREAD_POLICY_NORMAL;
		break;

	default:
		TRACE( CAM_ERROR, "Error: unsupported Posix policy[%d]( %s, %s, %d )\n", iPosixPolicy, __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	*pOsalPolicy = iPolicy;

	return 0;
}

static int _osal2posix_priority( int *pPosixPriority, int iOsalPriority, int iOsalPolicy )
{
	int iPriority;
	int iPriorityL, iPriorityH;
	int iPosixPolicy;
	int ret;

	ret = _osal2posix_policy( &iPosixPolicy, iOsalPolicy );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to mapping policy from osal to posix( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	iPriorityL = sched_get_priority_min( iPosixPolicy );
	iPriorityH = sched_get_priority_max( iPosixPolicy );

	switch ( iOsalPriority )
	{
	case CAM_THREAD_PRIORITY_LOW:
		iPriority = iPriorityL;
		break;

	case CAM_THREAD_PRIORITY_NORMAL:
		iPriority = ( iPriorityH + iPriorityL ) >> 1;
		break;

	case CAM_THREAD_PRIORITY_HIGH:
		iPriority = iPriorityH;
		break;

	default:
		TRACE( CAM_ERROR, "Error: invalide osal prority[%d]( %s, %s, %d )\n", iOsalPriority, __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	*pPosixPriority = iPriority;

	return 0;
}

static int _posix2osal_priority( int *pOsalPriority, int iPosixPriority, int iPosixPolicy )
{
	int iPriority;
	int iPriorityL, iPriorityH;

	iPriorityL = sched_get_priority_min( iPosixPolicy );
	if ( iPriorityL < 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to get min priority( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	iPriorityH = sched_get_priority_max( iPosixPolicy );
	if ( iPriorityH < 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to get max priority( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	if ( iPosixPriority == ( iPriorityL + iPriorityH ) / 2 )
	{
		iPriority = CAM_THREAD_PRIORITY_NORMAL;
	}
	else if ( iPosixPriority == iPriorityL )
	{
		iPriority = CAM_THREAD_PRIORITY_LOW;
	}
	else if ( iPosixPriority == iPriorityH )
	{
		iPriority = CAM_THREAD_PRIORITY_HIGH;
	}
	else
	{
		TRACE( CAM_ERROR, "Error: invalid posix priority[%d]( %s, %s, %d )\n", iPosixPriority, __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	*pOsalPriority = iPriority;

	return 0;
}

static int _osal2linux_normal_priority( int *pLinuxPriority, int iOsalPriority, int iOsalPolicy )
{
	int iPriority;

	if ( iOsalPolicy != CAM_THREAD_POLICY_NORMAL )
	{
		TRACE( CAM_ERROR, "Error: only support CAM_THREAD_POLICY_NORMAL to use this function( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	switch ( iOsalPriority )
	{
	case CAM_THREAD_PRIORITY_HIGH:
		iPriority = -18;
		break;

	case CAM_THREAD_PRIORITY_NORMAL:
		iPriority = 0;
		break;

	case CAM_THREAD_PRIORITY_LOW:
		iPriority = 19;
		break;

	default:
		TRACE( CAM_ERROR, "Error: unsupported osal priority[%d]( %s, %s, %d )\n", iOsalPriority, __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	*pLinuxPriority = iPriority;

	return 0;
}

static int _linux2osal_normal_priority( int *pOsalPriority, int iLinuxPriority, int iLinuxPolicy )
{
	int iPriority;

	if ( iLinuxPolicy != SCHED_OTHER )
	{
		TRACE( CAM_ERROR, "Error: only support SCHED_OTHER to use this function( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	if ( iLinuxPriority >= -18 && iLinuxPriority < -5 )
	{
		iPriority = CAM_THREAD_PRIORITY_HIGH;
	}
	else if ( iLinuxPriority >= -5 && iLinuxPriority < 7 )
	{
		iPriority = CAM_THREAD_PRIORITY_NORMAL;
	}
	else if ( iLinuxPriority >= 7 && iLinuxPriority <= 20 )
	{
		iPriority = CAM_THREAD_PRIORITY_LOW;
	}
	else
	{
		TRACE( CAM_ERROR, "unsupported linux priority[%d]( %s, %s, %d )\n", iLinuxPriority, __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	*pOsalPriority = iPriority;

	return 0;
}

typedef void* (*_CAM_ThreadFunc)( void* );

typedef struct
{
	_CAM_ThreadFunc      fnEntryFunc;
	void                 *pParam;
	int                  iPriority;
	int                  iPolicy;
} _CAM_ThreadData;

static void* _trampoline( void *pUsrData )
{
	_CAM_ThreadData     *pstThreadData = (_CAM_ThreadData*)pUsrData;
	struct sched_param  stParam;
	int                 ret;

	// NOTE: set thread policy to RR/FIFO need root permisson
	stParam.sched_priority =( pstThreadData->iPolicy == SCHED_OTHER ? 0 : pstThreadData->iPriority );
	ret = sched_setscheduler( 0, pstThreadData->iPolicy, &stParam );
	if ( ret != 0 )
	{
		TRACE( CAM_WARN, "Warning: sched_setscheduler failed, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
	}

	// NOTE: since pthread don't support adjust priority in NOEMAL policy, we need use linux call directly to attain this
	if ( pstThreadData->iPolicy == SCHED_OTHER )
	{
		ret = setpriority( PRIO_PROCESS, 0, pstThreadData->iPriority );
		if ( ret != 0 )
		{
			TRACE( CAM_WARN, "Warning: setpriority failed, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
		}
	}

	return pstThreadData->fnEntryFunc( pstThreadData->pParam );
}

int CAM_ThreadCreate( CAM_Thread *pThread, void *pFunc, void *pParam )
{
	int                   ret;
	_CAM_ThreadData       *pThreadPrivate   = NULL;
	CAM_ThreadAttribute   *pThreadAttribute = NULL;
	pthread_attr_t        stAttr;
	struct sched_param    stParam;
	int                   iPolicy;
	int                   iPriority;
	int                   iDetachState;

	if ( !pFunc || !pThread )
	{
		TRACE( CAM_ERROR, "Error: input parametes should not be NULL( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	pThreadAttribute = &pThread->stThreadAttribute;

	pThreadPrivate = malloc( sizeof(_CAM_ThreadData) );
	if ( !pThreadPrivate )
	{
		TRACE( CAM_ERROR, "Error: out of memory to allcate private data( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	if ( pThread->stThreadAttribute.hEventWakeup )
	{
		ret = CAM_EventReset( pThread->stThreadAttribute.hEventWakeup );
		if ( ret != 0 )
		{
			TRACE( CAM_ERROR, "Error: failed to reset wakeup event, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
			return -1;
		}
	}

	if ( pThread->stThreadAttribute.hEventExitAck )
	{
		ret = CAM_EventReset( pThread->stThreadAttribute.hEventExitAck );
		if ( ret != 0 )
		{
			TRACE( CAM_ERROR, "Error: failed to reset exitack event, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
			return -1;
		}
	}
	pThread->bExitThread = CAM_FALSE;

	pthread_attr_init( &stAttr );

	// set inherit schdule if needed
#if !defined( ANDROID )
	if ( pThreadAttribute->iPolicy != CAM_THREAD_POLICY_NORMAL )
	{
		ret = pthread_attr_setinheritsched( &stAttr, PTHREAD_EXPLICIT_SCHED );
		if ( ret != 0 )
		{
			TRACE( CAM_ERROR, "Error: set inherit schedule failed, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
			return -1;
		}
	}
#endif

	// set policy
	ret = _osal2posix_policy( &iPolicy, pThreadAttribute->iPolicy );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to mapping policy( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		pthread_attr_destroy( &stAttr );
		return -1;
	}
	pthread_attr_setschedpolicy( &stAttr, iPolicy );

	// set priority
	ret = _osal2posix_priority( &iPriority, pThreadAttribute->iPriority, pThreadAttribute->iPolicy );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to mapping priority( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		pthread_attr_destroy( &stAttr );
		return -1;
	}
	pthread_attr_getschedparam( &stAttr, &stParam );
	stParam.sched_priority = iPriority;
	pthread_attr_setschedparam( &stAttr, &stParam );

	/* NOTE: When using normal policy, pthread can only accept 0 as static
	 * priority in order to set the dynamic priority as well, we will
	 * mapping the proirity for normal policy again
	 */
	if ( pThreadAttribute->iPolicy == CAM_THREAD_POLICY_NORMAL )
	{
		ret = _osal2linux_normal_priority( &iPriority, pThreadAttribute->iPriority, pThreadAttribute->iPolicy );
		if ( ret != 0 )
		{
			TRACE( CAM_ERROR, "Error: failed to mapping priority( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
			pthread_attr_destroy( &stAttr );
			return -1;
		}
	}

	// set detatch state
	if ( pThreadAttribute->iDetachState == CAM_THREAD_CREATE_DETACHED )
	{
		iDetachState = PTHREAD_CREATE_DETACHED;
	}
	else if ( pThreadAttribute->iDetachState == CAM_THREAD_CREATE_JOINABLE )
	{
		iDetachState = PTHREAD_CREATE_JOINABLE;
	}
	else
	{
		TRACE( CAM_ERROR, "Error: unsupported osal detach state[%d]( %s, %s, %d )\n", pThreadAttribute->iDetachState, __FILE__, __FUNCTION__, __LINE__ );
		pthread_attr_destroy( &stAttr );
		return -1;
	}
	pthread_attr_setdetachstate( &stAttr, iDetachState );

	pThreadPrivate->fnEntryFunc = pFunc;
	pThreadPrivate->pParam      = pParam;
	pThreadPrivate->iPolicy     = iPolicy;
	pThreadPrivate->iPriority   = iPriority;

	ret = pthread_create( (pthread_t*)&pThread->iThreadID, &stAttr, _trampoline, pThreadPrivate );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: pthread create thread failed, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
		pthread_attr_destroy( &stAttr );
		return -1;
	}

	pthread_attr_destroy( &stAttr );

	pThread->pPrivateData = (void*)pThreadPrivate;

	return 0;
}

int CAM_ThreadDestroy( CAM_Thread *pThread )
{
	int ret   = 0;

	if ( !pThread )
	{
		TRACE( CAM_ERROR, "Error: input parameter should not be NULL( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	if ( pThread->stThreadAttribute.hEventExitAck )
	{
		// reset the ack before notify
		ret = CAM_EventReset( pThread->stThreadAttribute.hEventExitAck );
		if ( ret != 0 )
		{
			TRACE( CAM_ERROR, "Error: failed to reset exitack event,error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
			return -1;
		}
	}

	pThread->bExitThread = CAM_TRUE;

	if ( pThread->stThreadAttribute.hEventWakeup )
	{
		ret = CAM_EventSet( pThread->stThreadAttribute.hEventWakeup );
		if ( ret != 0 )
		{
			TRACE( CAM_ERROR, "Error: failed to set wakeup event, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
			return -1;
		}
	}

	if ( pThread->stThreadAttribute.hEventExitAck )
	{
		// wait for thread exit ack
		ret = CAM_EventWait( pThread->stThreadAttribute.hEventExitAck, CAM_INFINITE_WAIT, NULL );
		if ( ret != 0 )
		{
			TRACE( CAM_ERROR, "Error: failed to wait exitack event, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
			return -1;
		}
	}

	if ( CAM_THREAD_CREATE_JOINABLE == pThread->stThreadAttribute.iDetachState )
	{
		ret = pthread_join( (pthread_t)pThread->iThreadID, NULL );
		if ( ret != 0 )
		{
			TRACE( CAM_ERROR, "Error: pthread failed to join thread, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
			return -1;
		}
	}

	if ( pThread->pPrivateData )
	{
		free( pThread->pPrivateData );
		pThread->pPrivateData = NULL;
	}

	pThread->iThreadID = -1;

	return 0;
}

int CAM_ThreadSetAttribute( CAM_Thread *pThread, CAM_ThreadAttribute *pThreadAttribute )
{
	struct sched_param stParam;
	int                iPolicy, iPriority;
	int                ret;

	if ( !pThread || !pThreadAttribute )
	{
		TRACE( CAM_ERROR, "Error: input parameters should not be NULL( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	// set params
	ret = _osal2posix_policy( &iPolicy, pThreadAttribute->iPolicy );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to mapping policy( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	ret = _osal2posix_priority( &stParam.sched_priority, pThreadAttribute->iPriority, pThreadAttribute->iPolicy );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to mapping priority( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	if ( pThread->stThreadAttribute.iPolicy != pThreadAttribute->iPolicy ||
	     pThread->stThreadAttribute.iPriority != pThreadAttribute->iPriority )
	{
		ret = sched_setscheduler( 0, iPolicy, &stParam );
		if ( ret != 0 )
		{
			TRACE( CAM_ERROR, "Error: failed to set secheduler( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
			return -1;
		}
	}

	if ( iPolicy == SCHED_OTHER && pThread->stThreadAttribute.iPriority != pThreadAttribute->iPriority )
	{
		ret = _osal2linux_normal_priority( &iPriority, pThreadAttribute->iPriority, pThreadAttribute->iPolicy );
		if ( ret != 0 )
		{
			TRACE( CAM_ERROR, "Error: failed to mapping priority of ormal policy( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
			return -1;
		}

		ret = setpriority( PRIO_PROCESS, 0, iPriority );
		if ( ret != 0 )
		{
			TRACE( CAM_ERROR, "Error: failed to set priority, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
			return -1;
		}
	}

	memmove( &pThread->stThreadAttribute, pThreadAttribute, sizeof( CAM_ThreadAttribute ) );

	return 0;
}

int CAM_ThreadGetAttribute( CAM_Thread *pThread, CAM_ThreadAttribute *pThreadAttribute )
{
	int iPriority;
	int ret = 0;

	if ( !pThread || !pThreadAttribute )
	{
		TRACE( CAM_ERROR, "Error: input parameters should not be NULL( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	*pThreadAttribute = pThread->stThreadAttribute;

	if ( pThread->stThreadAttribute.iPolicy == CAM_THREAD_POLICY_NORMAL )
	{
		iPriority = getpriority( PRIO_PROCESS, 0 );
		ret = _linux2osal_normal_priority( &pThreadAttribute->iPriority, iPriority, SCHED_OTHER );
		if ( ret != 0 )
		{
			TRACE( CAM_ERROR, "Error: failed to mapping normal policy priority( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
			return -1;
		}
	}

	return 0;
}

int CAM_Sleep( unsigned long long uSec )
{
	struct timespec t, t_remain;

	t.tv_sec = (int)( uSec / 1000000 );
	t.tv_nsec = ( uSec % 1000000 ) * 1000;

	nanosleep( &t, &t_remain );

	return 0;
}


/**********************************************************************
 * Mutex
 **********************************************************************/
int CAM_MutexCreate( void **phMutex )
{
	int                 ret;
	pthread_mutexattr_t stAttr;
	pthread_mutex_t     *pMutex = NULL;

	*phMutex = NULL;

	pMutex = malloc( sizeof( pthread_mutex_t ) );
	if ( pMutex == NULL )
	{
		TRACE( CAM_ERROR, "Error: out of memory for create a mutex( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	ret = pthread_mutexattr_init( &stAttr );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: pthread init mutex attribute failed, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
		free( pMutex );
		pMutex = NULL;
		return -1;
	}

	ret = pthread_mutexattr_settype( &stAttr, PTHREAD_MUTEX_RECURSIVE );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: pthread init mutex set type failed, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
		free( pMutex );
		pMutex = NULL;
		return -1;
	}

	ret = pthread_mutex_init( pMutex, &stAttr );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: pthread init mutex create failed, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
		free( pMutex );
		pMutex = NULL;
		return -1;
	}

	ret = pthread_mutexattr_destroy( &stAttr );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: pthread init mutex attribute failed, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
		free( pMutex );
		pMutex = NULL;
		return -1;
	}

	*phMutex = (void*)pMutex;

	return 0;
}


int CAM_MutexDestroy( void **phMutex )
{
	pthread_mutex_t *pMutex = NULL;
	int             ret;

	if ( !phMutex )
	{
		TRACE( CAM_ERROR, "Error: input parameter should not be NULL( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	pMutex = (pthread_mutex_t*)(*phMutex);

	if ( !pMutex )
	{
		TRACE( CAM_ERROR, "Error: mutex should not be NULL( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	ret = pthread_mutex_destroy( pMutex );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: pthread mutex destroy failed, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	free( pMutex );
	*phMutex = NULL;

	return 0;
}

int CAM_MutexLock( void *hMutex, unsigned int mSec, int *pbTimedOut )
{
	int             ret;
	pthread_mutex_t *pMutex = (pthread_mutex_t*)hMutex;

	if ( !pMutex )
	{
		TRACE( CAM_ERROR, "Error: input parameter should not be NULL( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	if ( pbTimedOut )
	{
		*pbTimedOut = 0;
	}

	if ( mSec == 0 )
	{
		if ( pbTimedOut != NULL )
		{
			*pbTimedOut = 1;
		}
		return 0;
	}
	else if ( mSec == CAM_INFINITE_WAIT )
	{
		ret = pthread_mutex_lock( pMutex );
		if ( ret != 0 )
		{
			TRACE( CAM_ERROR, "Error: pthread mutex lock failed, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
			return -1;
		}
	}
	else
	{
#if 0
		unsigned long long timeout_us;
		struct timespec    timeout;
		struct timeval     now;

		gettimeofday( &now, NULL );

		timeout_us      = now.tv_usec + 1000 * mSec;
		timeout.tv_sec  = now.tv_sec + timeout_us / 1000000;
		timeout.tv_nsec = (timeout_us % 1000000) * 1000;

		ret = pthread_mutex_timedlock( pMutex, &timeout );
		switch ( ret )
		{
		case 0:
			return 0;

		case ETIMEDOUT:
			if ( pbTimedOut != NULL )
			{
				*pbTimedOut = 1;
			}
			return 0;

		default:
			TRACE( CAM_ERROR, "Error: pthread mutex timed lock failed, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
			return -1;
		}
#else
		TRACE( CAM_ERROR, "Error: don't support timed lock( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
#endif
	}

	return 0;
}

int CAM_MutexUnlock( void *hMutex )
{
	int             ret;
	pthread_mutex_t *pMutex = (pthread_mutex_t*)hMutex;

	if ( !pMutex )
	{
		TRACE( CAM_ERROR, "Error: input parameter should not be NULL( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	ret = pthread_mutex_unlock( pMutex );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: pthread mutex unlock failed, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	return 0;
}

/**********************************************************************
 * Event
 **********************************************************************/
typedef struct
{
	int             bSignaled;
	pthread_mutex_t hMutex;
	pthread_cond_t  stCondition;
} _CAM_ThreadEvent;

int CAM_EventCreate( void **phEvent )
{
	_CAM_ThreadEvent *pEvent = NULL;
	int              ret;

	pEvent = malloc( sizeof( _CAM_ThreadEvent ) );
	if ( !pEvent )
	{
		TRACE( CAM_ERROR, "Error: out of memory for create a event( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	pEvent->bSignaled = 0;

	ret = pthread_mutex_init( &pEvent->hMutex, NULL );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: pthread mutex init failed, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
		free( pEvent );
		pEvent = NULL;
		return -1;
	}

	ret = pthread_cond_init( &pEvent->stCondition, NULL );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: pthread condition init failed, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
		pthread_mutex_destroy( &pEvent->hMutex );
		free( pEvent );
		pEvent = NULL;
		return -1;
	}

	*phEvent = (void*)pEvent;

	return 0;
}

int CAM_EventDestroy( void **phEvent )
{
	_CAM_ThreadEvent *pEvent = NULL;
	int              ret;

	if ( !phEvent )
	{
		TRACE( CAM_ERROR, "Error: input parameter should not be NULL( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	pEvent = (_CAM_ThreadEvent*)(*phEvent);
	if ( !pEvent )
	{
		TRACE( CAM_ERROR, "Error: event should not be NULL( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	ret = pthread_mutex_lock( &pEvent->hMutex );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: pthread mutex lock failed, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}
	
	ret = pthread_cond_destroy( &pEvent->stCondition );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: pthread condition destroy failed, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	ret = pthread_mutex_unlock( &pEvent->hMutex );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: pthread mutex unlock failed, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	ret = pthread_mutex_destroy( &pEvent->hMutex );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: pthread mutex destroy failed, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	free( pEvent );
	*phEvent = NULL;

	return 0;
}

int CAM_EventSet( void *hEvent )
{
	_CAM_ThreadEvent *pEvent = (_CAM_ThreadEvent*)hEvent;
	int              ret;

	if ( !pEvent )
	{
		TRACE( CAM_ERROR, "Error: input parameter should not be NULL( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	ret = pthread_mutex_lock( &pEvent->hMutex );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: pthread mutex lock failed, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	pEvent->bSignaled = 1;

	ret = pthread_cond_signal( &pEvent->stCondition );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: pthread signal condition failed, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	ret = pthread_mutex_unlock( &pEvent->hMutex );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: pthread mutex unlock failed, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	return 0;
}

int CAM_EventReset( void *hEvent )
{
	_CAM_ThreadEvent *pEvent = (_CAM_ThreadEvent*)hEvent;
	int              ret;

	if ( !pEvent )
	{
		TRACE( CAM_ERROR, "Error: input parameter should not be NULL( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	ret = pthread_mutex_lock( &pEvent->hMutex );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: pthread mutex lock failed, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	pEvent->bSignaled = 0;

	ret = pthread_mutex_unlock( &pEvent->hMutex );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: pthread mutex unlock failed, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	return 0;
}

// wake up all threads which are waitting on this condition
int CAM_EventBroadcast( void *hEvent )
{
	_CAM_ThreadEvent *pEvent = (_CAM_ThreadEvent*)hEvent;
	int              ret;

	if ( !pEvent )
	{
		TRACE( CAM_ERROR, "Error: input parameter should not be NULL( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	ret = pthread_mutex_lock( &pEvent->hMutex );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: pthread mutex lock failed, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	pEvent->bSignaled = 1;

	ret = pthread_cond_broadcast( &pEvent->stCondition );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: pthread broadcast condition failed, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	ret = pthread_mutex_unlock( &pEvent->hMutex );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: pthread mutex unlock failed, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	return 0;
}

int CAM_EventWait( void *hEvent, unsigned int mSec, int *pbTimedOut )
{
	_CAM_ThreadEvent   *pEvent = (_CAM_ThreadEvent*)hEvent;
	unsigned long long timeout_us;
	struct timespec    timeout;
	struct timeval     now;
	int                ret;

	if ( !pEvent )
	{
		TRACE( CAM_ERROR, "Error: input parameter should not be NULL( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	gettimeofday( &now, NULL );

	if ( NULL != pbTimedOut )
	{
		*pbTimedOut = 0;
	}

	timeout_us      = now.tv_usec + 1000 * mSec;
	timeout.tv_sec  = now.tv_sec + timeout_us / 1000000;
	timeout.tv_nsec = ( timeout_us % 1000000 ) * 1000;

	ret = pthread_mutex_lock( &pEvent->hMutex );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: pthread mutex lock failed, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	if ( 0 == mSec )
	{
		if ( !pEvent->bSignaled )
		{
			if ( NULL != pbTimedOut )
			{
				*pbTimedOut = 1;
			}
		}
	}
	else if ( CAM_INFINITE_WAIT == mSec )
	{
		while ( !pEvent->bSignaled )
		{
			pthread_cond_wait( &pEvent->stCondition, &pEvent->hMutex );
		}
	}
	else
	{
		while ( !pEvent->bSignaled )
		{
			ret = pthread_cond_timedwait( &pEvent->stCondition, &pEvent->hMutex, &timeout );
			if ( ( ret == ETIMEDOUT ) && !pEvent->bSignaled )
			{
				if ( pbTimedOut != NULL )
				{
					*pbTimedOut = 1;
				}
				break;
			}
		}
	}

	pEvent->bSignaled = 0;

	ret = pthread_mutex_unlock( &pEvent->hMutex );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: pthread mutex unlock failed, error code[%d]( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	return 0;
}

/**********************************************************************
 * Profiling, granularity: us
 **********************************************************************/
long long CAM_TimeGetTickCount()
{
	struct timespec t;

	t.tv_sec = t.tv_nsec = 0;

	clock_gettime( CLOCK_MONOTONIC, &t );
	return ( (long long)t.tv_sec * 1000000000LL + t.tv_nsec ) / 1000;
}

/* EOF */
