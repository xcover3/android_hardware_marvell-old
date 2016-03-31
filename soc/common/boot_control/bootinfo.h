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

#ifndef BOOTINFO_H_
#define BOOTINFO_H_

#include <stdint.h>
#include <stdbool.h>

#define MAX_SLOTS	2

typedef struct brillo_slot_info {
	// Flag mean that the slot can bootable or not.
	uint8_t bootable :1;

	// Set to true when the OS has successfully booted.
	uint8_t boot_successful :1;

	// Priorty number range [0:15], with 15 meaning highest priortiy,
	// 1 meaning lowest priority and 0 mean that the slot is unbootable.
	uint8_t priority;

	// Boot times left range [0:7].
	uint8_t tries_remaining;

	uint8_t reserved[1];
} BrilloSlotInfo;

typedef struct BrilloBootInfo {
	// Magic for identification
	uint8_t magic[3];

	// Version of BrilloBootInfo struct, must be 0 or larger.
	uint8_t version;

	// Information about each slot.
	BrilloSlotInfo slot_info[2];

	uint8_t reserved[20];
} BrilloBootInfo;

// Loading and saving BrillBootInfo instances.
bool boot_info_load(BrilloBootInfo *out_info);
bool boot_info_save(BrilloBootInfo *info);

// Returns non-zero if valid.
bool boot_info_validate(BrilloBootInfo* info);
void boot_info_reset(BrilloBootInfo* info);

#if defined(__GNUC__) && __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
_Static_assert(sizeof(BrilloBootInfo) == 32, "BrilloBootInfo has wrong size");
#endif

#endif  // BOOTINFO_H
