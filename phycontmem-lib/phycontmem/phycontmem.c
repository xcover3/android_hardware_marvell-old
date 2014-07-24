/*
 * (C) Copyright 2010 Marvell International Ltd.
 * All Rights Reserved
 */

#include <pthread.h>
#include <linux/pxa_ion.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <cutils/log.h>

#include "ion_helper_lib.h"
#include "phycontmem.h"

//for bmm like
typedef struct phycontmem_node{
	union mem_args args;
	struct phycontmem_node* next;
}PHYCONTMEM_NODE;

static PHYCONTMEM_NODE* g_phycontmemlist = NULL;

static pthread_mutex_t g_phycontmemlist_mutex = PTHREAD_MUTEX_INITIALIZER;

static __inline void list_add_node(PHYCONTMEM_NODE* pnode)
{
	pnode->next = g_phycontmemlist;
	g_phycontmemlist = pnode;
	return;
}

static PHYCONTMEM_NODE* list_find_by_va_range(void* VA)
{
	PHYCONTMEM_NODE* pnode;

	pnode = g_phycontmemlist;
	while(pnode) {
		if((unsigned int)VA >= (unsigned int)pnode->args.mem.va
			&& (unsigned int)VA
			< (unsigned int)pnode->args.mem.va
			+ pnode->args.mem.size) {
			return pnode;
		}
		pnode = pnode->next;
	}
	return NULL;
}


static PHYCONTMEM_NODE* list_find_by_pa_range(unsigned int PA)
{
	PHYCONTMEM_NODE* pnode;

	pnode = g_phycontmemlist;
	while(pnode) {
		if(PA >= (unsigned int)pnode->args.mem.pa
			&& PA < (unsigned int)pnode->args.mem.pa
			+ (unsigned int)pnode->args.mem.size) {
			return pnode;
		}
		pnode = pnode->next;
	}
	return NULL;
}

static PHYCONTMEM_NODE* list_remove_by_va(void* VA)
{
	PHYCONTMEM_NODE* pPrev = NULL;
	PHYCONTMEM_NODE* pnode;

	pnode = g_phycontmemlist;
	while(pnode) {
		if(VA == pnode->args.mem.va) {
			break;
		}
		pPrev = pnode;
		pnode = pnode->next;
	}
	if(pnode != NULL) {
		if(pPrev) {
			pPrev->next = pnode->next;
		}else{
			g_phycontmemlist = pnode->next;
		}
	}
	return pnode;
}

void* phy_cont_malloc(int size, int attr)
{
	PHYCONTMEM_NODE* pnode;
	void* VA = NULL;

	struct ion_args *args;
	pnode = (PHYCONTMEM_NODE*)malloc(sizeof(PHYCONTMEM_NODE));

	if(pnode == NULL) {
		return NULL;
	}
	attr = (attr == PHY_CONT_MEM_ATTR_DEFAULT) ? ION_FLAG_CACHED : 0;
	args = ion_malloc(size, attr);
	if (!args) {
		free(pnode);
		return NULL;
	}
	memcpy(&pnode->args.ion, args, sizeof(struct ion_args));
	pnode->args.ion.p = args;
        VA = pnode->args.ion.va;

	pthread_mutex_lock(&g_phycontmemlist_mutex);
	list_add_node(pnode);
	pthread_mutex_unlock(&g_phycontmemlist_mutex);

	return VA;
}

void phy_cont_free(void* VA)
{
	PHYCONTMEM_NODE* pnode;
	pthread_mutex_lock(&g_phycontmemlist_mutex);
	pnode = list_remove_by_va(VA);
	pthread_mutex_unlock(&g_phycontmemlist_mutex);
	if(pnode) {
		ion_free(pnode->args.ion.p);
		free(pnode);
	}
	return;
}

unsigned int phy_cont_getpa(void* VA)
{
	PHYCONTMEM_NODE* pnode;
	unsigned int PA = 0;
	pthread_mutex_lock(&g_phycontmemlist_mutex);
	pnode = list_find_by_va_range(VA);
	if(pnode) {
		PA = ((unsigned int)VA - (unsigned int)pnode->args.mem.va)
			+ (unsigned int)pnode->args.mem.pa;
	}
	pthread_mutex_unlock(&g_phycontmemlist_mutex);
	return PA;
}


void* phy_cont_getva(unsigned int PA)
{
	PHYCONTMEM_NODE* pnode;
	void* VA = NULL;
	pthread_mutex_lock(&g_phycontmemlist_mutex);
	pnode = list_find_by_pa_range(PA);
	if(pnode) {
		VA = (void*)((PA-(unsigned int)pnode->args.mem.pa)
			+ (unsigned int)pnode->args.mem.va);
	}
	pthread_mutex_unlock(&g_phycontmemlist_mutex);
	return VA;
}

void phy_cont_flush_cache(void* VA, int dir)
{
	PHYCONTMEM_NODE* pnode;
	int ion_dir;

	if (dir == PHY_CONT_MEM_FLUSH_BIDIRECTION) {
		ion_dir = PXA_DMA_BIDIRECTIONAL;
	} else if (dir == PHY_CONT_MEM_FLUSH_TO_DEVICE) {
		ion_dir = PXA_DMA_TO_DEVICE;
	} else if (dir == PHY_CONT_MEM_FLUSH_FROM_DEVICE) {
		ion_dir = PXA_DMA_FROM_DEVICE;
	} else {
		return;
	}
	pthread_mutex_lock(&g_phycontmemlist_mutex);
	pnode = list_find_by_va_range(VA);
	if (pnode == NULL) {
		pthread_mutex_unlock(&g_phycontmemlist_mutex);
		return;
	}
	ion_flush_cache(&pnode->args.ion, 0, pnode->args.ion.size, ion_dir);
	pthread_mutex_unlock(&g_phycontmemlist_mutex);

	return;
}

void phy_cont_flush_cache_range(void* VA, unsigned long size, int dir)
{
	PHYCONTMEM_NODE* pnode;
	int ion_dir;

	if (dir == PHY_CONT_MEM_FLUSH_BIDIRECTION) {
		ion_dir = PXA_DMA_BIDIRECTIONAL;
	} else if (dir == PHY_CONT_MEM_FLUSH_TO_DEVICE) {
		ion_dir = PXA_DMA_TO_DEVICE;
	} else if (dir == PHY_CONT_MEM_FLUSH_FROM_DEVICE) {
		ion_dir = PXA_DMA_FROM_DEVICE;
	} else {
		return;
	}
	pthread_mutex_lock(&g_phycontmemlist_mutex);
	pnode = list_find_by_va_range(VA);
	if (pnode == NULL) {
		pthread_mutex_unlock(&g_phycontmemlist_mutex);
		return;
	}
	if ((unsigned long)VA + size > (unsigned long)(pnode->args.ion.va) +
		pnode->args.ion.size)
		size = pnode->args.ion.size + (unsigned long)(pnode->args.ion.va)
			- (unsigned long)VA;
	ion_flush_cache(&pnode->args.ion,
			(unsigned long)VA - (unsigned long)pnode->args.ion.va,
			size, ion_dir);
	pthread_mutex_unlock(&g_phycontmemlist_mutex);
	return;
}
