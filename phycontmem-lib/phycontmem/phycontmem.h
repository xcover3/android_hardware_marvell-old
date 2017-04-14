/*
 * (C) Copyright 2010 Marvell International Ltd.
 * All Rights Reserved
 */

#ifndef __PHYCONTMEM_H__
#define __PHYCONTMEM_H__

#ifdef __cplusplus
extern "C"
{
#endif

struct pmem_handle_mrvl;
union mem_args {
	struct m_args {
		int fd;
		unsigned int size;
		void *va;
		void *pa;
	} mem;
	struct pmem_args {
		int fd;
		unsigned int size;
		void *va;
		void *pa;
		/* keep above items that are same with mem items */
		struct pmem_handle_mrvl *p;
	} pmem;
	struct ion_args {
		int fd;
		unsigned int size;
		void *va;
		void *pa;
		/* keep above items that are same with mem items */
		struct ion_handle *handle;
		int buf_fd;
		struct ion_args *p;
	} ion;
};
#define MARVELL_IONDEV_NAME    "/dev/ion"

#define PHY_CONT_MEM_ATTR_DEFAULT		0	//cacheable & buffered
#define PHY_CONT_MEM_ATTR_NONCACHED		(1<<1)	//non-cacheable & non-buffered
#define PHY_CONT_MEM_ATTR_WC			(1<<2)	//non-cacheable & buffered
void* phy_cont_malloc(int size, int attr);	//return value is virtual address, attr should be PHY_CONT_MEM_ATTR_DEFAULT or PHY_CONT_MEM_ATTR_NONCACHED or PHY_CONT_MEM_ATTR_WC
void phy_cont_free(void* VA);				//only free the memory allocated by phy_cont_malloc

unsigned int phy_cont_getpa(void* VA);		//return physical address, if the memory isn't allocated from phy_cont_malloc, return NULL
void* phy_cont_getva(unsigned int PA);		//return virtual address, if the memory isn't allocated from phy_cont_malloc, return NULL

#define PHY_CONT_MEM_FLUSH_BIDIRECTION	0
#define PHY_CONT_MEM_FLUSH_TO_DEVICE	1
#define PHY_CONT_MEM_FLUSH_FROM_DEVICE	2
void phy_cont_flush_cache(void* VA, int dir);		//only for the memory allocated by phy_cont_malloc. dir should be PHY_CONT_MEM_FLUSH_BIDIRECTION, PHY_CONT_MEM_FLUSH_TO_DEVICE or PHY_CONT_MEM_FLUSH_FROM_DEVICE
void phy_cont_flush_cache_range(void* VA, unsigned long size, int dir);	//only for the memory allocated by phy_cont_malloc. dir should be PHY_CONT_MEM_FLUSH_BIDIRECTION, PHY_CONT_MEM_FLUSH_TO_DEVICE or PHY_CONT_MEM_FLUSH_FROM_DEVICE


#ifdef __cplusplus
}
#endif

#endif
