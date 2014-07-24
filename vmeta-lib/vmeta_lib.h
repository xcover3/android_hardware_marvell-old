#ifndef VDEC_OS_DRIVER_H
#define VDEC_OS_DRIVER_H
#include <sys/poll.h>
#include <semaphore.h>
#include "vdec_os_api.h"
#include "uio_vmeta.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define VDEC_DEBUG_ALL 0x1
#define VDEC_DEBUG_MEM 0x2
#define VDEC_DEBUG_LOCK 0x4
#define VDEC_DEBUG_VER 0x8
#define VDEC_DEBUG_POWER 0x10
#define VDEC_DEBUG_NONE 0x0

#define UIO_DEV "/dev/uio0"
#define UIO_IO_MEM_SIZE "/sys/class/uio/uio0/maps/map0/size"
#define UIO_IO_MEM_ADDR "/sys/class/uio/uio0/maps/map0/addr"
#define UIO_IO_VERSION "/sys/class/uio/uio0/version"

#define UIO_IO_HW_CONTEXT_SIZE "/sys/class/uio/uio0/maps/map1/size"
#define UIO_IO_HW_CONTEXT_ADDR "/sys/class/uio/uio0/maps/map1/addr"

#define UIO_IO_VMETA_OBJ_SIZE "/sys/class/uio/uio0/maps/map2/size"
#define UIO_IO_VMETA_OBJ_ADDR "/sys/class/uio/uio0/maps/map2/addr"
#define UIO_IO_VMETA_OBJ_INDEX 2

#define UIO_IO_KERNEL_SHARE_SIZE "/sys/class/uio/uio0/maps/map3/size"
#define UIO_IO_KERNEL_SHARE_ADDR "/sys/class/uio/uio0/maps/map3/addr"
#define UIO_IO_KERNEL_SHARE_INDEX 3

#define VMETA_SHARED_LOCK_HANDLE "vmeta_shared_lock"

#define VMETA_KERN_MIN_VER	5
#define VMETA_USER_VER		"build-006"
//---------------------------------------------------------------------------
// Macros
//---------------------------------------------------------------------------
#define VMETA_STATUS_BIT_USED		0
#define VMETA_STATUS_BIT_REGISTED	1

#define RESO_VGA_SIZE		(480*640)
#define RESO_WVGA_SIZE		(480*800)
#define RESO_720P_SIZE		(720*1280)
#define RESO_1080P_SIZE		(1080*1920)
//---------------------------------------------------------------------------
// Driver initialization API
//---------------------------------------------------------------------------
SIGN32 vdec_os_driver_version(SIGN8 *ver_str);

//---------------------------------------------------------------------------
// Power operation APIs
//---------------------------------------------------------------------------
typedef enum _VMETA_CLOCK_OP{
	VMETA_CLOCK_L0 = 0,
	VMETA_CLOCK_L1
}VMETA_CLOCK_OP;

SIGN32 vdec_os_api_clock_switch(UNSG32 vco);

typedef enum _VPRO_CODEC_ERROR_CODE_ {
	VDEC_OS_DRIVER_OK = 0,
	VDEC_OS_DRIVER_INIT_FAIL,
	VDEC_OS_DRIVER_OPEN_FAIL,
	VDEC_OS_DRIVER_NO_SYS_MEM_FAIL,
	VDEC_OS_DRIVER_MEM_POOL_INIT_FAIL,
	VDEC_OS_DRIVER_MMAP_FAIL,
	VDEC_OS_DRIVER_SYNC_TIMEOUT_FAIL,
	VDEC_OS_DRIVER_IO_CONTROL_FAIL,
	VDEC_OS_DRIVER_ALREADY_INIT_FAIL,
	VDEC_OS_DRIVER_CLEAN_FAIL,
	VDEC_OS_DRIVER_USER_ID_FAIL,
	VDEC_OS_DRIVER_VER_FAIL,
	VDEC_OS_DRIVER_UPDATE_FAIL
}VPRO_DEC_ERROR_CODE;

/* display debug message */
#define VMETA_LOG_ON 1
#define VMETA_LOG_FILE "/data/vmeta_dbg.log"
int dbg_printf(UNSG32 dbglevel, const char* format, ...);

typedef sem_t lock_t;
//---------------------------------------------------------------------------
// the control block of vdec os driver
//---------------------------------------------------------------------------
typedef struct vdec_os_driver_cb_s
{
	int uiofd;			// the uio file descriptor
	UNSG32 io_mem_phy_addr;		// the physical addr of io register base
	SIGN32 io_mem_virt_addr;	// the reg base addr that maped from kernel
	UNSG32 io_mem_size;		// the size of io mem area
	int refcount;			// reference count in current process
	SIGN32 vdec_obj_va;
	UNSG32 vdec_obj_size;
	UNSG32 hw_context_pa;
	SIGN32 kernel_share_va;
	UNSG32 kernel_share_size;
	int kern_ver;	//vmeta kernel version
	SIGN32 curr_op;
} vdec_os_driver_cb_t;

struct monitor_data{
	pthread_t pt;
	SIGN32 user_id;
};

/* vdec driver get cb */
vdec_os_driver_cb_t *vdec_driver_get_cb(void);


#ifdef __cplusplus
}
#endif
#endif
