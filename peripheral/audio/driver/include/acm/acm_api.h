/*
 * Copyright (C)  2005. Marvell International Ltd
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

#ifndef __MARVELL_AUDIO_ACM_H__
#define __MARVELL_AUDIO_ACM_H__

#define COUPLING_DC 0x00200000
#define COUPLING_AC 0x00100000
#define COUPLING_UNKNOWN 0x00000000

#define CONNECT_SINGLE_ENDED 0x00080000
#define CONNECT_QUASI_DIFF 0x00040000
#define CONNECT_DIFF 0x00020000
#define CONNECT_UNKNOWN 0x00000000

#define CONNECT_MONO_HEADSET_L 0x00400000
#define CONNECT_MONO_HEADSET_R 0x00800000
#define CONNECT_STEREO_HEADSET 0x01000000
#define CONNECT_HEADSET_MASK \
  (CONNECT_MONO_HEADSET_L | CONNECT_MONO_HEADSET_R | CONNECT_STEREO_HEADSET)

#define VOLUME_MASK 0x0000ffff
#define PARAM_MASK 0xffff0000
// gain register in low priority path share value with which is in the highest
// priority path
#define SHARED_GAIN 0x80000000

// connectivity and coupling type
typedef enum {
  ACM_MIC_TYPE_AC_DIFF = COUPLING_AC | CONNECT_DIFF,
  ACM_MIC_TYPE_AC_QUASI_DIFF = COUPLING_AC | CONNECT_QUASI_DIFF,
  ACM_MIC_TYPE_AC_SINGLE_ENDED = COUPLING_AC | CONNECT_SINGLE_ENDED,
  ACM_MIC_TYPE_DC_DIFF = COUPLING_DC | CONNECT_DIFF,
  ACM_MIC_TYPE_DC_QUASI_DIFF = COUPLING_DC | CONNECT_QUASI_DIFF,
  ACM_MIC_TYPE_DC_SINGLE_ENDED = COUPLING_DC | CONNECT_SINGLE_ENDED,
  ACM_MIC_TYPE_MASK = COUPLING_AC | COUPLING_DC | CONNECT_DIFF |
                      CONNECT_QUASI_DIFF | CONNECT_SINGLE_ENDED
} ACM_MicType;

typedef enum {
  ACM_RC_OK,
  ACM_RC_INVALID_PATH,
  ACM_RC_PATH_CONFIG_NOT_EXIST,
  ACM_RC_PATH_ALREADY_ENABLED,
  ACM_RC_PATH_ALREADY_DISABLED,
  ACM_RC_PATH_NOT_ENABLED,
  ACM_RC_NO_MUTE_CHANGE_NEEDED,
  ACM_RC_INVALID_VOLUME_CHANGE,
  ACM_RC_INVALID_PARAMETER,
  ACM_RC_INVALID_MIC_TYPE,
  ACM_RC_INVALID_HEADSET_TYPE,
  ACM_RC_FORCE_DELAY
} ACM_ReturnCode;

void ACMInit(void);
void ACMDeInit(void);
ACM_ReturnCode ACMAudioPathEnable(const char *path, unsigned int value);
// deprecate ACMAudioPathDisable API, suggest to use ACMAudioPathHotDisable
// instead
ACM_ReturnCode ACMAudioPathDisable(const char *path);
// if value = 1, will disable the path but not turn off standby components
// if value = 0, will disable the path and turn off standby components, as
// former ACMAudioPathDisable do
ACM_ReturnCode ACMAudioPathHotDisable(const char *path, unsigned int value);
ACM_ReturnCode ACMAudioPathSwitch(const char *path_old, const char *path_new,
                                  unsigned int value);
ACM_ReturnCode ACMAudioPathMute(const char *path, unsigned int value);
ACM_ReturnCode ACMAudioPathVolumeSet(const char *path, unsigned int value);
ACM_ReturnCode ACMGetParameter(unsigned int param_id, void *param,
                               unsigned int *size);
ACM_ReturnCode ACMSetParameter(unsigned int param_id, void *param,
                               unsigned int size);

#endif
