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
//---------------------------------------------------------------------------
//  Description:    VDEC OS API
//---------------------------------------------------------------------------

#ifndef VDEC_OS_API_H
#define VDEC_OS_API_H

#ifdef __cplusplus
extern "C"
{
#endif


typedef struct _vmeta_user_info {
    /*in parameters*/
    int usertype;            /*0:dec, 1:enc*/
    int strm_fmt;            /*0:mpeg1, 1:mpeg2, 2:mpeg4, 3:h261, 4:h263, 5:h264, 6:vc1 ap, 7:jpeg, 8:mjpeg, 10:vc1 sp&mp*/
    int width;
    int height;
    int perf_req;            /*-99: expect lowest perf, -1: expect lower perf, 0: default perf, 1: expect higher perf, 99: expect highest perf*/
    /*out parameters*/
    int curr_op;             /*filled by driver, inform high-level user current operation point after user info update*/
}vmeta_user_info;

#ifndef VMETA_OP_MAX
#define VMETA_OP_MAX		15
#define VMETA_OP_MIN		0
#define VMETA_OP_VGA		1
#define VMETA_OP_720P		8
#define VMETA_OP_1080P		14
#define VMETA_OP_VGA_MAX	(VMETA_OP_720P-1)
#define VMETA_OP_720P_MAX	(VMETA_OP_1080P-1)
#define VMETA_OP_1080P_MAX	VMETA_OP_1080P
#define VMETA_OP_VGA_ENC	VMETA_OP_720P
#define VMETA_OP_VGA_ENC_MAX	VMETA_OP_720P_MAX
#define VMETA_OP_INVALID -1
#endif

#ifndef __KERNEL__
//---------------------------------------------------------------------------
// Macros
//---------------------------------------------------------------------------

#define VMETA_MAX_OP	VMETA_OP_MAX
#define VMETA_MIN_OP	VMETA_OP_MIN

#ifndef UNSG32
#define UNSG32 unsigned int
#endif

#ifndef SIGN32
#define SIGN32 int
#endif

#ifndef UNSG16
#define UNSG16 unsigned short
#endif

#ifndef SIGN16
#define SIGN16 short
#endif


#ifndef UNSG8
#define UNSG8 unsigned char
#endif

#ifndef SIGN8
#define SIGN8 char
#endif

#ifndef _ASM_LINUX_DMA_MAPPING_H
#define _ASM_LINUX_DMA_MAPPING_H
enum dma_data_direction {
    DMA_BIDIRECTIONAL   = 0,
    DMA_TO_DEVICE       = 1,
    DMA_FROM_DEVICE     = 2,
    DMA_NONE            = 3,
};
#endif

typedef enum _LOCK_RET_CODE {
	LOCK_RET_ERROR_TIMEOUT = -9999,
	LOCK_RET_ERROR_UNKNOWN,
	LOCK_RET_OHTERS_NORM = 0,
	LOCK_RET_NULL,
	LOCK_RET_ME,
	LOCK_RET_FORCE_INIT,
	LOCK_RET_FORCE_TO_OTHERS,
}LOCK_RET_CODE;

//---------------------------------------------------------------------------
// Driver initialization API
//---------------------------------------------------------------------------
SIGN32 vdec_os_driver_init(void);
SIGN32 vdec_os_driver_clean(void);

//---------------------------------------------------------------------------
// Memory operation API
//---------------------------------------------------------------------------
void * vdec_os_api_dma_alloc(UNSG32 size, UNSG32 align, UNSG32 * pPhysical);
void * vdec_os_api_dma_alloc_writecombine(UNSG32 size, UNSG32 align, UNSG32 * pPhysical);
void * vdec_os_api_dma_alloc_cached(UNSG32 size, UNSG32 align, UNSG32 * pPhysical);
void vdec_os_api_dma_free(void *ptr);
void *vdec_os_api_vmalloc(UNSG32 size, UNSG32 align);		// always return VA and can't be translated to PA
void vdec_os_api_vfree(void *ptr);
UNSG32 vdec_os_api_get_va(UNSG32 paddr);
UNSG32 vdec_os_api_get_pa(UNSG32 vaddr);
UNSG32 vdec_os_api_flush_cache(UNSG32 vaddr, UNSG32 size, enum dma_data_direction direction);

//---------------------------------------------------------------------------
// Mem/IO R/W API
//---------------------------------------------------------------------------
UNSG8 vdec_os_api_rd8(UNSG32 addr);
UNSG16 vdec_os_api_rd16(UNSG32 addr);
UNSG32 vdec_os_api_rd32(UNSG32 addr);
void vdec_os_api_wr8(UNSG32 addr, UNSG8 data);
void vdec_os_api_wr16(UNSG32 addr, UNSG16 data);
void vdec_os_api_wr32(UNSG32 addr, UNSG32 data);
UNSG32 vdec_os_api_get_regbase_addr(void);			// return VA

//---------------------------------------------------------------------------
// Interrupt register API
//---------------------------------------------------------------------------
SIGN32 vdec_os_api_set_sync_timeout_isr(UNSG32 timeout);
SIGN32 vdec_os_api_sync_event(void);

/*return value of vdec_os_api_update_user_info_ext()*/
#define SOCKET_CANNOT_OPEN 1
#define SOCKET_CANNOT_SENDMSG 2
#define VMETA_INSTANCE_OVERANGE 3
#define UNSUPPORTED_OPERATION 4

//---------------------------------------------------------------------------
// Power Management API
//---------------------------------------------------------------------------
SIGN32 vdec_os_api_power_on(void);
SIGN32 vdec_os_api_power_off(void);
SIGN32 vdec_os_api_suspend_check(void);
void vdec_os_api_suspend_ready(void);
SIGN32 vdec_os_api_clock_on(void);
SIGN32 vdec_os_api_clock_off(void);
SIGN32 vdec_os_api_update_user_info(SIGN32 user_id, vmeta_user_info *info);

typedef struct{
    int app_type;    //0:fullspeed, 1:not-fullspeed. This fullspeed flag is prior to all other information.
    int perf_req;    //0: default, 1: expect higher perf, -1: expect lower perf
    int width;
    int height;
    int codec_type;  //0:dec, 1:enc
    int strm_fmt;    //0:mpeg1, 1:mpeg2, 2:mpeg4, 3:h261, 4:h263, 5:h264, 6:vc1 ap, 7:jpeg, 8:mjpeg, 10:vc1 sp&mp
    void* reserved[16];
}vmeta_user_info_ext;

//return 0 means old poweropt solution, return 1 means new poweropt solution
SIGN32 vdec_os_api_get_poweropt_solution(void);

//option 0: create; option 1: update; option 2: close
SIGN32 vdec_os_api_update_user_info_ext(SIGN32 user_id, vmeta_user_info_ext *info, SIGN32 option);

//---------------------------------------------------------------------------
// Multi-instance API
//---------------------------------------------------------------------------
SIGN32 vdec_os_api_get_user_id(void);
SIGN32 vdec_os_api_free_user_id(SIGN32 user_id);
SIGN32 vdec_os_api_register_user_id(SIGN32 user_id);
SIGN32 vdec_os_api_unregister_user_id(SIGN32 user_id);
SIGN32 vdec_os_api_get_hw_obj_addr(UNSG32 *vaddr, UNSG32 size);
SIGN32 vdec_os_api_get_hw_context_addr(UNSG32 *paddr, UNSG32 *vaddr, UNSG32 size, SIGN32 flag);
SIGN32 vdec_os_api_lock(SIGN32 user_id, UNSG32 to_ms);
SIGN32 vdec_os_api_unlock(SIGN32 user_id);
SIGN32 vdec_os_api_get_user_count(void);
SIGN32 vdec_os_api_force_ini(void);

#endif // end of #ifndef __KERNEL__

#ifdef __cplusplus
}
#endif

#endif
