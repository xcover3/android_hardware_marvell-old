/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <fs_mgr.h>
#include <hardware/hardware.h>
#include <hardware/boot_control.h>
#include <cutils/properties.h>

#include "bootinfo.h"


void boot_control_init(struct boot_control_module *module __unused)
{
	return;
}

unsigned get_number_slots(struct boot_control_module *module __unused)
{
	return MAX_SLOTS;
}

unsigned get_current_slot(struct boot_control_module *module __unused)
{
	char propbuf[PROPERTY_VALUE_MAX];
	const char* suffix[MAX_SLOTS] = {"_a", "_b"};
	int i;

	property_get("ro.boot.slot_suffix", propbuf, "");

	if (propbuf[0] != '\0') {
		for (i = 0; i < MAX_SLOTS; i++) {
			if (strncmp(propbuf, suffix[i], 2) == 0)
				return i;
		}

		if (i == MAX_SLOTS)
			fprintf(stderr, "WARNING: androidboot.slot_suffix is invalid\n");
	} else {
		fprintf(stderr, "WARNING: androidboot.slot_suffix is NULL\n");
	}

	return 0;
}

int mark_boot_successful(struct boot_control_module *module __unused)
{
	BrilloBootInfo info;
	unsigned current_slot;
	int i;

	if (!boot_info_load(&info)) {
		fprintf(stderr, "WARNING: Error loading boot-info. Resetting.\n");
		boot_info_reset(&info);
	} else {
		if (!boot_info_validate(&info)) {
			fprintf(stderr, "WARNING: boot-info is invalid. Resetting.\n");
			boot_info_reset(&info);
		}
	}

	current_slot = get_current_slot(module);

	info.slot_info[current_slot].boot_successful = true;
	info.slot_info[current_slot].tries_remaining = 0;

	if (!boot_info_save(&info)) {
		fprintf(stderr, "WARNING: Error saving boot-info.\n");
		return -errno;
	}

	return 0;
}

int set_active_boot_slot(struct boot_control_module *module __unused,
                         unsigned slot)
{
	BrilloBootInfo info;
	unsigned other_slot;
	int i;

	if (slot >= MAX_SLOTS)
		return -EINVAL;

	if (!boot_info_load(&info)) {
		fprintf(stderr, "WARNING: Error loading boot-info. Resetting.\n");
		boot_info_reset(&info);
	} else {
		if (!boot_info_validate(&info)) {
			fprintf(stderr, "WARNING: boot-info is invalid. Resetting.\n");
			boot_info_reset(&info);
		}
	}

	other_slot = 1 - slot;
	if (info.slot_info[other_slot].priority == 15)
		info.slot_info[other_slot].priority = 14;

	info.slot_info[slot].bootable = true;
	info.slot_info[slot].priority = 15;
	info.slot_info[slot].tries_remaining = 7;
	info.slot_info[slot].boot_successful = false;

	if (!boot_info_save(&info)) {
		fprintf(stderr, "WARNING: Error saving boot-info.\n");
		return -errno;
	}

	return 0;
}

int set_slot_as_unbootable(struct boot_control_module *module __unused,
                           unsigned slot)
{
	BrilloBootInfo info;

	if (slot >= MAX_SLOTS)
		return -EINVAL;

	if (!boot_info_load(&info)) {
		fprintf(stderr, "WARNING: Error loading boot-info. Resetting.\n");
		boot_info_reset(&info);
	} else {
		if (!boot_info_validate(&info)) {
			fprintf(stderr, "WARNING: boot-info is invalid. Resetting.\n");
			boot_info_reset(&info);
		}
	}

	info.slot_info[slot].bootable = false;
	info.slot_info[slot].priority = 0;
	info.slot_info[slot].tries_remaining = 0;
	info.slot_info[slot].boot_successful = false;

	if (!boot_info_save(&info)) {
		fprintf(stderr, "WARNING: Error saving boot-info.\n");
		return -errno;
	}

	return 0;
}

int is_slot_bootable(struct boot_control_module *module __unused,
                     unsigned slot)
{
	BrilloBootInfo info;

	if (slot >= MAX_SLOTS)
		return -EINVAL;

	if (!boot_info_load(&info)) {
		fprintf(stderr, "WARNING: Error loading boot-info. Resetting.\n");
		boot_info_reset(&info);
	} else {
		if (!boot_info_validate(&info)) {
			fprintf(stderr, "WARNING: boot-info is invalid. Resetting.\n");
			boot_info_reset(&info);
		}
	}

	return info.slot_info[slot].bootable;
}

const char* get_suffix(struct boot_control_module *module __unused,
                       unsigned slot)
{
	static const char* suffix[MAX_SLOTS] = {"_a", "_b"};

	if (slot >= MAX_SLOTS)
		return NULL;

	return suffix[slot];
}

int is_slot_marked_successful(struct boot_control_module *module __unused,
                              unsigned slot)
{
	BrilloBootInfo info;

	if (slot >= MAX_SLOTS)
		return -EINVAL;

	if (!boot_info_load(&info)) {
		fprintf(stderr, "WARNING: Error loading boot-info. Resetting.\n");
		boot_info_reset(&info);
	} else {
		if (!boot_info_validate(&info)) {
			fprintf(stderr, "WARNING: boot-info is invalid. Resetting.\n");
			boot_info_reset(&info);
		}
	}

	return info.slot_info[slot].boot_successful;
}

static struct hw_module_methods_t boot_control_module_methods = {
	.open = NULL,
};

struct boot_control_module HAL_MODULE_INFO_SYM = {
	.common = {
		.tag = HARDWARE_MODULE_TAG,
		.module_api_version = BOOT_CONTROL_MODULE_API_VERSION_0_1,
		.hal_api_version = HARDWARE_HAL_API_VERSION,
		.id = BOOT_CONTROL_HARDWARE_MODULE_ID,
		.name = "Marvell Boot Control HAL",
		.author = "Marvell SEEDS",
		.methods = &boot_control_module_methods,
	},

	.init = boot_control_init,
	.getNumberSlots = get_number_slots,
	.getCurrentSlot = get_current_slot,
	.markBootSuccessful = mark_boot_successful,
	.setActiveBootSlot = set_active_boot_slot,
	.setSlotAsUnbootable = set_slot_as_unbootable,
	.isSlotBootable = is_slot_bootable,
	.getSuffix = get_suffix,
	.isSlotMarkedSuccessful = is_slot_marked_successful,
};
