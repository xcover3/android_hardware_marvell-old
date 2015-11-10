/*
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef ANDROID_SENSORS_H
#define ANDROID_SENSORS_H

#include <stdint.h>
#include <errno.h>
#include <sys/cdefs.h>
#include <sys/types.h>

__BEGIN_DECLS

/* Physical parameters of the sensors supported by misc vendor */

enum {
  ISL_ID_L = 40,
  ISL_ID_PRO,
  AVAGO_ID_L,
  AVAGO_ID_PRO,
  TSL_ID_L,
  TSL_ID_PRO,
  ELAN_ID_PRO,
  ELAN_ID_L,
  DYNA_ID_PRO,
  DYNA_ID_L
};

#define ISL_SENSORS_LIGHT_HANDLE (ISL_ID_L)
#define ISL_SENSORS_PROXIMITY_HANDLE (ISL_ID_PRO)
#define AVAGO_SENSORS_LIGHT_HANDLE (AVAGO_ID_L)
#define AVAGO_SENSORS_PROXIMITY_HANDLE (AVAGO_ID_PRO)
#define TSL_SENSORS_LIGHT_HANDLE (TSL_ID_L)
#define TSL_SENSORS_PROXIMITY_HANDLE (TSL_ID_PRO)
#define ELAN_SENSORS_PROXIMITY_HANDLE (ELAN_ID_PRO)
#define ELAN_SENSORS_LIGHT_HANDLE (ELAN_ID_L)
#define DYNA_SENSORS_PROXIMITY_HANDLE (DYNA_ID_PRO)
#define DYNA_SENSORS_LIGHT_HANDLE (DYNA_ID_L)

__END_DECLS

#endif  // ANDROID_SENSORS_H
