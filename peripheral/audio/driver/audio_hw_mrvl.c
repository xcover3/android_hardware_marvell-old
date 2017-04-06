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

#define LOG_TAG "audio_hw_mrvl"
#define LOG_NDEBUG 0

#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <cutils/log.h>
#include <cutils/str_parms.h>
#include <cutils/list.h>
#include <cutils/properties.h>

#include <hardware/hardware.h>
#include <system/audio.h>

#include "audio_hw_mrvl.h"
#include "acm_api.h"

#include "audio_effect_mrvl.h"

// it should be accessed in protection of madev->lock
static struct mrvl_path_status mrvl_path_manager;

// ----------------------------------------------------------------------
// pcm_config used for tiny alsa device open
// ----------------------------------------------------------------------
// used for low latency output
static struct pcm_config pcm_config_ll = {
    .channels = 2,
    .rate = SAMPLE_RATE_OUT_DEFAULT,
    .format = PCM_FORMAT_S16_LE,
    .period_size = LOW_LATENCY_OUTPUT_PERIOD_SIZE,
    .period_count = LOW_LATENCY_OUTPUT_PERIOD_COUNT,
    .start_threshold =
        LOW_LATENCY_OUTPUT_PERIOD_SIZE * (LOW_LATENCY_OUTPUT_PERIOD_COUNT - 1),
    .stop_threshold =
        LOW_LATENCY_OUTPUT_PERIOD_SIZE * LOW_LATENCY_OUTPUT_PERIOD_COUNT,
};
// used for deep buffer output
static struct pcm_config pcm_config_db = {
    .channels = 2,
    .rate = SAMPLE_RATE_OUT_DEFAULT,
    .format = PCM_FORMAT_S16_LE,
    .period_size = DEEP_BUFFER_LONG_PERIOD_SIZE,
    .period_count = DEEP_BUFFER_LONG_PERIOD_COUNT,
    .start_threshold =
        DEEP_BUFFER_SHORT_PERIOD_SIZE * (DEEP_BUFFER_SHORT_PERIOD_COUNT - 1),
    .avail_min = DEEP_BUFFER_LONG_PERIOD_SIZE,
};
// used for phone output
static struct pcm_config pcm_config_phone = {
    .channels = 2,
    .rate = SAMPLE_RATE_PHONE,
    .format = PCM_FORMAT_S16_LE,
    .period_size = PHONE_OUTPUT_PERIOD_SIZE,
    .period_count = PHONE_OUTPUT_PERIOD_COUNT,
};
// used for fm output
static struct pcm_config pcm_config_fm = {
    .channels = 2,
    .rate = SAMPLE_RATE_OUT_FM,
    .format = PCM_FORMAT_S16_LE,
    .period_size = FM_OUTPUT_PERIOD_SIZE,
    .period_count = FM_OUTPUT_PERIOD_COUNT,
};
// used for default input
static struct pcm_config pcm_config_input = {
    .channels = 2,
    .rate = SAMPLE_RATE_IN_DEFAULT,
    .format = PCM_FORMAT_S16_LE,
    .period_size = LOW_LATENCY_INPUT_PERIOD_SIZE,
    .period_count = LOW_LATENCY_INPUT_PERIOD_COUNT,
    .start_threshold = 1,
    .stop_threshold =
        LOW_LATENCY_INPUT_PERIOD_SIZE * LOW_LATENCY_INPUT_PERIOD_COUNT,
};

// output modes, descending order by
// priority(VC>VOIP/VT>FM>DeepBuffer>LowLatency)
static struct vtrl_mode_priority_cfg priority_modes_output[] = {
    {ID_IPATH_RX_VC, AUDIO_MODE_IN_CALL, V_MODE_VC},
    {ID_IPATH_RX_VC, AUDIO_MODE_NORMAL, V_MODE_VC},
    {ID_IPATH_RX_VC_ST, AUDIO_MODE_NORMAL, V_MODE_VC},
    {ID_IPATH_RX_HIFI_LL, AUDIO_MODE_IN_COMMUNICATION, V_MODE_VOIP},
    {ID_IPATH_RX_HIFI_LL, AUDIO_MODE_IN_VT_CALL, V_MODE_VT},
    {ID_IPATH_RX_FM, AUDIO_MODE_NORMAL, V_MODE_FM},
    {ID_IPATH_RX_HFP, AUDIO_MODE_IN_CALL, V_MODE_HFP},
    {ID_IPATH_RX_HIFI_DB, AUDIO_MODE_NORMAL, V_MODE_HIFI_DB},
    {ID_IPATH_RX_HIFI_LL, AUDIO_MODE_NORMAL, V_MODE_HIFI_LL}};

// input modes, descending order by priority(currently it can not support DB
// recording)
static struct vtrl_mode_priority_cfg priority_modes_input[] = {
    {ID_IPATH_TX_VC, AUDIO_MODE_IN_CALL, V_MODE_VC},
    {ID_IPATH_TX_HIFI, AUDIO_MODE_IN_COMMUNICATION, V_MODE_VOIP},
    {ID_IPATH_TX_HIFI, AUDIO_MODE_IN_VT_CALL, V_MODE_VT},
    {ID_IPATH_TX_HFP, AUDIO_MODE_IN_CALL, V_MODE_HFP},
    {ID_IPATH_TX_FM, AUDIO_MODE_NORMAL, V_MODE_FM},
    {ID_IPATH_TX_HIFI, AUDIO_MODE_NORMAL, V_MODE_HIFI_LL}};

// virtual modes num definition for output and input
#define NUM_VTRL_MODES_OUTPUT \
  (sizeof(priority_modes_output) / sizeof(priority_modes_output[0]))
#define NUM_VTRL_MODES_INPUT \
  (sizeof(priority_modes_input) / sizeof(priority_modes_input[0]))

static enum BT_STATUS_UPDATE {
  BT_UPDATE_NONE = 0x0,
  BT_UPDATE_NREC_MODE = 0x1,
  BT_UPDATE_HEADSET_TYPE = 0x2
}BT_STATUS_UPDATE;

// ----------------------------------------------------------------------
// Internal Helper Functions
// ----------------------------------------------------------------------
static uint32_t out_get_sample_rate(const struct audio_stream *stream);
static audio_channel_mask_t out_get_channels(const struct audio_stream *stream);
static audio_format_t out_get_format(const struct audio_stream *stream);

uint32_t get_mic_mode(void) { return mrvl_path_manager.mic_mode; }

unsigned char get_cpmute_rampdown_level() {
  char prop_value[PROPERTY_VALUE_MAX];
  unsigned char level = 0;
  if (property_get("audio.cpmute.rampdown.level", prop_value, "0")) {
    int value = atoi(prop_value);
    if (value >= 0 && value < 3) {
      level = (unsigned char)value;
    } else {
      ALOGW("%s: Invalid ramp down level %d, current only support 0 ~ 2",
            __FUNCTION__, value);
    }
  }
  return level;
}

unsigned char get_cpmute_rampup_level() {
  char prop_value[PROPERTY_VALUE_MAX];
  unsigned char level = 0;
  if (property_get("audio.cpmute.rampup.level", prop_value, "0")) {
    int value = atoi(prop_value);
    if (value >= 0 && value < 2) {
      level = (unsigned char)value;
    } else {
      ALOGW("%s: Invalid ramp up level %d, current only support 0 ~ 1",
            __FUNCTION__, value);
    }
  }
  return level;
}

static bool is_in_voip_vt(audio_mode_t mode) {
  return (AUDIO_MODE_IN_COMMUNICATION == mode || AUDIO_MODE_IN_VT_CALL == mode);
}

void add_vtrl_mode(struct listnode *mode_list, virtual_mode_t v_mode,
                   bool is_priority_highest) {
  struct virtual_mode *vtrl_mode = calloc(1, sizeof(struct virtual_mode));
  if (vtrl_mode == NULL) {
    return;
  }

  vtrl_mode->v_mode = v_mode;
  vtrl_mode->is_priority_highest = is_priority_highest;
  list_add_tail(mode_list, &vtrl_mode->link);
}

// get current active virtual modes for RX direction
// the priority of virtual modes is VC>VOIP/VT>FM>DeepBuffer>LowLatency
// it supports three modes concurrent (current it supports
// VC/DeepBuffer/LowLatency)
// each one of VC/FM/DeepBuffer modes can be concurrent with LowLatency
// VOIP/VT can only be concurrent with DeepBuffer
void get_out_vrtl_mode(struct mrvl_audio_device *madev,
                       struct listnode *active_mode_list) {
  int i = 0;
  bool is_highest = true;
  virtual_mode_t highest_mode = V_MODE_INVALID;
  // the priorities of modes are sorted by descending order in array
  // priority_modes[]
  for (i = 0; i < (int)NUM_VTRL_MODES_OUTPUT; i++) {
    if (mrvl_path_manager.itf_state[priority_modes_output[i].path_interface]) {
      // VOIP/VT and LowLatency have a common interface, but audio mode is
      // different
      if (priority_modes_output[i].path_interface == ID_IPATH_RX_HIFI_LL) {
        if (((priority_modes_output[i].audio_mode ==
              AUDIO_MODE_IN_COMMUNICATION) ||
             (priority_modes_output[i].audio_mode == AUDIO_MODE_IN_VT_CALL)) &&
            (priority_modes_output[i].audio_mode != madev->mode)) {
          continue;
        }
      }
      // LowLatency could not exist with VOIP/VT simultaneously
      if ((priority_modes_output[i].v_mode == V_MODE_HIFI_LL) &&
          ((highest_mode == V_MODE_VOIP) || (highest_mode == V_MODE_VT))) {
        continue;
      }
      // first mode we get from the array is the highest mode, the left are
      // non-highest
      add_vtrl_mode(active_mode_list, priority_modes_output[i].v_mode,
                    is_highest);
      if (highest_mode == V_MODE_INVALID) {
        highest_mode = priority_modes_output[i].v_mode;
        is_highest = false;
      }
    }
  }
}

// get current active virtual mode for TX direction
void get_in_vrtl_mode(struct mrvl_audio_device *madev, virtual_mode_t *v_mode) {
  int i = 0;
  // the priorities of modes are sorted by descending order in array
  // priority_modes[]
  for (i = 0; i < (int)NUM_VTRL_MODES_INPUT; i++) {
    if (mrvl_path_manager.itf_state[priority_modes_input[i].path_interface]) {
      if ((priority_modes_input[i].path_interface == ID_IPATH_TX_HIFI) &&
          (priority_modes_input[i].audio_mode != madev->mode)) {
        continue;
      }
      *v_mode = priority_modes_input[i].v_mode;
      break;
    }
  }

  if (madev->loopback_param.on) {
    if (madev->loopback_param.type == CP_LOOPBACK) {
      *v_mode = V_MODE_CP_LOOPBACK;
    } else if (madev->loopback_param.type == HARDWARE_LOOPBACK) {
      *v_mode = V_MODE_HW_LOOPBACK;
    } else if (madev->loopback_param.type == APP_LOOPBACK) {
      *v_mode = V_MODE_APP_LOOPBACK;
    }
  }
}

// this function returns the hw device, takes into account the tty mode.
unsigned int get_headset_hw_device(struct mrvl_audio_device *madev,
                                   unsigned int android_dev) {
  unsigned int hardware_dev = HWDEV_INVALID;

  switch (android_dev) {
    case AUDIO_DEVICE_OUT_WIRED_HEADSET:
      switch (madev->tty_mode) {
        case TTY_MODE_FULL:
        case TTY_MODE_VCO:
          hardware_dev = HWDEV_OUT_TTY;
          break;
        case TTY_MODE_HCO:  // HCO output device handled in convert2_hwdev()
        case TTY_MODE_OFF:
        default:
          hardware_dev = HWDEV_HEADPHONE;
          break;
      }
      break;
    case AUDIO_DEVICE_IN_WIRED_HEADSET:
      switch (madev->tty_mode) {
        case TTY_MODE_FULL:
        case TTY_MODE_HCO:
          hardware_dev = HWDEV_IN_TTY;
          break;
        case TTY_MODE_VCO: {
          virtual_mode_t v_mode = V_MODE_INVALID;

          // get current active virtual mode
          get_in_vrtl_mode(madev,
                           &v_mode);  // in TTY mode, it MUST be V_MODE_VC
          switch (get_mic_dev(v_mode, AUDIO_DEVICE_IN_BUILTIN_MIC)) {
            case HWDEV_DUAL_AMIC:
              hardware_dev = HWDEV_IN_TTY_VCO_DUAL_AMIC;
              break;
            case HWDEV_DUAL_AMIC_SPK_MODE:
              hardware_dev = HWDEV_IN_TTY_VCO_DUAL_AMIC_SPK_MODE;
              break;
            case HWDEV_DUAL_DMIC1:
              hardware_dev = HWDEV_IN_TTY_VCO_DUAL_DMIC1;
              break;
            case HWDEV_AMIC1:
              hardware_dev = HWDEV_IN_TTY_VCO_AMIC1;
              break;
            case HWDEV_AMIC2:
              hardware_dev = HWDEV_IN_TTY_VCO_AMIC2;
              break;
            default:
              break;
          }
        } break;
        case TTY_MODE_OFF:
        default:
          hardware_dev = HWDEV_HSMIC;
          break;
      }
      break;
    default:
      // hardware_dev remains HWDEV_INVALID;
      break;
  }

  return hardware_dev;
}

// this function returns the tty param to be used for getting VCM Profile.
static unsigned int get_tty_param(struct mrvl_audio_device *madev) {
  unsigned int tty_param;

  switch (madev->tty_mode) {
    case TTY_MODE_FULL:
      tty_param = VCM_TTY_FULL;
      break;
    case TTY_MODE_HCO:
      tty_param = VCM_TTY_HCO;
      break;
    case TTY_MODE_VCO: {
      virtual_mode_t v_mode;
      // get current active virtual mode
      get_in_vrtl_mode(madev, &v_mode);  // in TTY mode, it MUST be V_MODE_VC
      if ((get_mic_dev(v_mode, AUDIO_DEVICE_IN_BUILTIN_MIC) ==
           HWDEV_DUAL_AMIC) ||
          (get_mic_dev(v_mode, AUDIO_DEVICE_IN_BUILTIN_MIC) ==
           HWDEV_DUAL_AMIC_SPK_MODE) ||
          (get_mic_dev(v_mode, AUDIO_DEVICE_IN_BUILTIN_MIC) ==
           HWDEV_DUAL_DMIC1))
        tty_param = VCM_TTY_VCO_DUALMIC;
      else
        tty_param = VCM_TTY_VCO;
      break;
    }
    case TTY_MODE_OFF:
    default:
      tty_param = 0;
      break;
  }

  return tty_param;
}

// this function returns the call param to be used for getting VCM Profile.
static unsigned int get_call_params(struct mrvl_audio_device *madev) {
  unsigned int params = 0;

  if (madev->use_extra_vol) {
    params |= VCM_EXTRA_VOL;
  }
  if (madev->bt_headset_type == BT_WB) {
    params |= VCM_BT_WB;
  }
  // check whether BT enables NREC or not.
  if (madev->use_sw_nrec) {
    params |= VCM_BT_NREC_OFF;
  }
  if (mrvl_path_manager.enabled_in_hwdev & ~HWDEV_BIT_IN &
      (HWDEV_DUAL_AMIC | HWDEV_DUAL_AMIC_SPK_MODE | HWDEV_DUAL_DMIC1)) {
    params |= VCM_DUAL_MIC;
  }
  params |= get_tty_param(madev);

  return params;
}

// convert an android device to hardware device to control ACM
unsigned int convert2_hwdev(struct mrvl_audio_device *madev,
                            unsigned int android_dev) {
  unsigned int hardware_dev = HWDEV_INVALID;
  switch (android_dev) {
    case AUDIO_DEVICE_OUT_EARPIECE:
      hardware_dev = HWDEV_EARPIECE;
      break;
    case AUDIO_DEVICE_OUT_SPEAKER:
#ifdef WITH_STEREO_SPKR
      hardware_dev = (madev->tty_mode == TTY_MODE_HCO)
                         ? HWDEV_OUT_TTY_HCO_STEREOSPEAKER
                         : HWDEV_STEREOSPEAKER;
#else
      hardware_dev = (madev->tty_mode == TTY_MODE_HCO)
                         ? HWDEV_OUT_TTY_HCO_SPEAKER
                         : HWDEV_SPEAKER;
#endif
      break;
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO:
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
      hardware_dev = (madev->bt_headset_type == BT_WB)
                         ? ((madev->use_sw_nrec) ? HWDEV_BT_NREC_OFF_WB
                                                 : HWDEV_BLUETOOTH_WB)
                         : ((madev->use_sw_nrec) ? HWDEV_BT_NREC_OFF_NB
                                                 : HWDEV_BLUETOOTH_NB);
      break;
    case AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET:
    case AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET:
    case AUDIO_DEVICE_OUT_WIRED_HEADPHONE:
      hardware_dev = HWDEV_HEADPHONE;
      break;
    case AUDIO_DEVICE_OUT_WIRED_HEADSET:
      hardware_dev = get_headset_hw_device(madev, android_dev);
      break;
    case AUDIO_DEVICE_OUT_SPEAKER | AUDIO_DEVICE_OUT_WIRED_HEADSET:
    case AUDIO_DEVICE_OUT_SPEAKER | AUDIO_DEVICE_OUT_WIRED_HEADPHONE:
#ifdef WITH_STEREO_SPKR
      hardware_dev = HWDEV_STEREOSPEAKER | HWDEV_HEADPHONE;
#else
      hardware_dev = HWDEV_SPEAKER | HWDEV_HEADPHONE;
#endif
      break;
    case AUDIO_DEVICE_IN_BUILTIN_MIC:
    case AUDIO_DEVICE_IN_BACK_MIC: {
      virtual_mode_t v_mode = V_MODE_INVALID;

      // get current active virtual mode
      get_in_vrtl_mode(madev, &v_mode);
      hardware_dev = get_mic_dev(v_mode, android_dev);

      // in case of Headphone (3-pole headset), change the input (mic) hw device
      if (madev->out_device == AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
        switch (hardware_dev) {
          case HWDEV_AMIC1:
          case HWDEV_AMIC1_SPK_MODE:
            hardware_dev = HWDEV_AMIC1_HP_MODE;
            break;
          case HWDEV_AMIC2:
          case HWDEV_AMIC2_SPK_MODE:
            hardware_dev = HWDEV_AMIC2_HP_MODE;
            break;
          case HWDEV_DUAL_AMIC:
          case HWDEV_DUAL_AMIC_SPK_MODE:
            hardware_dev = HWDEV_DUAL_AMIC_HP_MODE;
            break;
          default:
            break;
        }
      }
      break;
    }
    case AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET:
      hardware_dev = (madev->bt_headset_type == BT_WB)
                         ? ((madev->use_sw_nrec) ? HWDEV_BTMIC_NREC_OFF_WB
                                                 : HWDEV_BTMIC_WB)
                         : ((madev->use_sw_nrec) ? HWDEV_BTMIC_NREC_OFF_NB
                                                 : HWDEV_BTMIC_NB);
      break;
    case AUDIO_DEVICE_IN_WIRED_HEADSET:
      hardware_dev = get_headset_hw_device(madev, android_dev);
      break;
    default:
      ALOGI("%s: Invalid input of audio device 0x%x", __FUNCTION__,
            android_dev);
      hardware_dev = HWDEV_INVALID;
      break;
  }
  return hardware_dev;
}

int add_vrtl_path(struct listnode *path_node, struct virtual_mode *vtrl_mode,
                  unsigned device) {
  struct virtual_path *v_path = calloc(1, sizeof(struct virtual_path));
  if (v_path == NULL) {
    return -1;
  }

  v_path->v_mode = vtrl_mode->v_mode;
  v_path->is_priority_highest = vtrl_mode->is_priority_highest;
  v_path->path_device = device;
  list_add_tail(path_node, &v_path->link);

  return 0;
}

// select corresponding input device by output device
unsigned int get_input_dev(unsigned int out_device) {
  unsigned int input_dev = AUDIO_DEVICE_IN_BUILTIN_MIC;

  switch (out_device) {
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO:
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
      input_dev = AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET;
      break;
    case AUDIO_DEVICE_OUT_WIRED_HEADSET:
      input_dev = AUDIO_DEVICE_IN_WIRED_HEADSET;
      break;
    case AUDIO_DEVICE_OUT_SPEAKER:
      input_dev = AUDIO_DEVICE_IN_BACK_MIC;
      break;
    default:
      input_dev = AUDIO_DEVICE_IN_BUILTIN_MIC;
      break;
  }

  return input_dev;
}

// check whether has active output interface
static bool has_active_out_itf(void) {
  int i;
  bool has_active_itf = false;

  for (i = ID_IPATH_RX_MIN; i <= ID_IPATH_RX_MAX; i++) {
    if (mrvl_path_manager.itf_state[i] == true) {
      has_active_itf = true;
      break;
    }
  }
  return has_active_itf;
}

// check whether has active input interface
static bool has_active_in_itf(void) {
  int i;
  bool has_active_itf = false;

  for (i = ID_IPATH_TX_MIN; i <= ID_IPATH_TX_MAX; i++) {
    if (mrvl_path_manager.itf_state[i] == true) {
      has_active_itf = true;
      break;
    }
  }
  return has_active_itf;
}

void get_active_vrtl_path(struct mrvl_audio_device *madev,
                          struct listnode *path_node) {
  ALOGD("%s: out device 0x%x, in device 0x%x", __FUNCTION__,
        mrvl_path_manager.active_out_device,
        mrvl_path_manager.active_in_device);

  // check current active output virtual path, maybe two or more active paths
  if (mrvl_path_manager.active_out_device && !mrvl_path_manager.mute_all_rx) {
    struct listnode out_mode_list;
    list_init(&out_mode_list);
    // get all active modes of output
    get_out_vrtl_mode(madev, &out_mode_list);
    // add output virtual path
    struct listnode *plist = NULL;
    struct listnode *tmp_node = NULL;
    struct virtual_mode *tmp_vmode = NULL;
    list_for_each_safe(plist, tmp_node, &out_mode_list) {
      tmp_vmode = node_to_item(plist, struct virtual_mode, link);
      add_vrtl_path(path_node, tmp_vmode, mrvl_path_manager.active_out_device);
      list_remove(&tmp_vmode->link);
      free(tmp_vmode);
      tmp_vmode = NULL;
    }
  }

  // check current active input virtual path, only support single now
  if (mrvl_path_manager.active_in_device) {
    virtual_mode_t v_mode = V_MODE_INVALID;
    struct virtual_mode temp_vmode;
    get_in_vrtl_mode(madev, &v_mode);
    // add input virtual path
    temp_vmode.v_mode = v_mode;
    temp_vmode.is_priority_highest = true;
    add_vrtl_path(path_node, &temp_vmode, mrvl_path_manager.active_in_device);
  }
}

void select_virtual_path(struct mrvl_audio_device *madev) {
  unsigned int flag = 0;
  unsigned int value = 0;
  struct listnode active_v_path;
  list_init(&active_v_path);

  ALOGD("%s enter", __FUNCTION__);

  // get current active virtual paths
  get_active_vrtl_path(madev, &active_v_path);

  // remove old inactive virtual paths
  {
    struct listnode *plist = NULL;
    struct listnode *tmp_node = NULL;
    struct virtual_path *tmp_vpath = NULL;
    list_for_each_safe(plist, tmp_node, &mrvl_path_manager.out_virtual_path) {
      tmp_vpath = node_to_item(plist, struct virtual_path, link);
      bool hasItem = false;
      flag = 0;
      if (tmp_vpath != NULL) {
        struct listnode *plist_new = NULL;
        struct listnode *tmp_node_new = NULL;
        struct virtual_path *tmp_new_vpath = NULL;
        list_for_each_safe(plist_new, tmp_node_new, &active_v_path) {
          tmp_new_vpath = node_to_item(plist_new, struct virtual_path, link);
          if ((tmp_new_vpath != NULL) &&
              (tmp_new_vpath->v_mode == tmp_vpath->v_mode) &&
              (tmp_new_vpath->path_device == tmp_vpath->path_device)) {
            if (tmp_new_vpath->is_priority_highest &&
                (!tmp_vpath->is_priority_highest)) {
              // the same path, but now its priority asends to the highest,
              // should re-enable the new path
              break;
            } else if ((!tmp_new_vpath->is_priority_highest) &&
                       tmp_vpath->is_priority_highest) {
              // the same path, but now its priority desends to non-highest,
              // just change the old path priority to non-highest
              tmp_vpath->is_priority_highest = false;
            }
            hasItem = true;
            break;
          }
        }

        // can not find same item in new virtual list, then remove
        if (!hasItem) {
          // voice recognition
          flag |= (audio_is_input_device(tmp_vpath->path_device) &&
                   madev->use_voice_recognition)
                      ? RECOGNITION
                      : 0;
          // call path manager to disable this virtual path
          unsigned int hw_dev = convert2_hwdev(madev, tmp_vpath->path_device);
          if (tmp_vpath->path_device & AUDIO_DEVICE_BIT_IN) {
            hw_dev = mrvl_path_manager.enabled_in_hwdev;
          }
          route_vrtl_path(tmp_vpath->v_mode, hw_dev, METHOD_DISABLE, flag,
                          value);
          list_remove(&tmp_vpath->link);
          free(tmp_vpath);
          tmp_vpath = NULL;
        }
      }
    }
  }

  // add new active virtual path to list, and destory the tmp lists
  {
    struct listnode *plist = NULL;
    struct listnode *tmp_node = NULL;
    struct virtual_path *tmp_vpath = NULL;
    list_for_each_safe(plist, tmp_node, &active_v_path) {
      tmp_vpath = node_to_item(plist, struct virtual_path, link);
      bool hasItem = false;
      flag = 0;
      value = 0;
      if (tmp_vpath != NULL) {
        struct listnode *plist_old = NULL;
        struct listnode *tmp_node_old = NULL;
        struct virtual_path *tmp_old_vpath = NULL;
        list_for_each_safe(plist_old, tmp_node_old,
                           &mrvl_path_manager.out_virtual_path) {
          tmp_old_vpath = node_to_item(plist_old, struct virtual_path, link);
          if ((tmp_old_vpath != NULL) &&
              (tmp_old_vpath->v_mode == tmp_vpath->v_mode) &&
              (tmp_old_vpath->path_device == tmp_vpath->path_device)) {
            hasItem = true;
            break;
          }
        }

        list_remove(&tmp_vpath->link);
        // can not find same item in old virtual list, then enable and add it
        if (!hasItem) {
          // voice recognition
          flag |= (audio_is_input_device(tmp_vpath->path_device) &&
                   madev->use_voice_recognition)
                      ? RECOGNITION
                      : 0;
          // call path manager to enable this virtual path
          unsigned int hw_dev = convert2_hwdev(madev, tmp_vpath->path_device);

          // if it is not the highest path ,SHARED_GAIN flag will ignore
          // register
          // configuration which has register misc "shared" in virtual path xml
          if (tmp_vpath->is_priority_highest == false) value |= SHARED_GAIN;

          route_vrtl_path(tmp_vpath->v_mode, hw_dev, METHOD_ENABLE, flag,
                          value);
          list_add_tail(&mrvl_path_manager.out_virtual_path, &tmp_vpath->link);
        } else {
          free(tmp_vpath);
          tmp_vpath = NULL;
        }
      }
    }
  }
}

// audio profile related functions
uint32_t get_profile_flag(struct mrvl_audio_device *madev) {
  uint32_t flag = 0;

  if (mrvl_path_manager.active_out_device & AUDIO_DEVICE_OUT_ALL_SCO) {
    // set correponding flag bits if feature is on
    if (madev->bt_headset_type == BT_WB) {
      flag |= PROFILE_BT_WB;
    }
    // set BT NREC profile:
    //  NREC_OFF: BT headset doesn't enable NREC.
    if (madev->use_sw_nrec) {
      flag |= PROFILE_BT_NREC_OFF;
    }
  }
  if (madev->use_extra_vol) {
    flag |= PROFILE_EXTRA_VOL;
  }
  if (mrvl_path_manager.enabled_in_hwdev & ~HWDEV_BIT_IN &
      (HWDEV_DUAL_AMIC | HWDEV_DUAL_AMIC_SPK_MODE | HWDEV_DUAL_DMIC1)) {
    flag |= PROFILE_DUAL_MIC;
  }

  return flag;
}

uint32_t get_profile(struct mrvl_audio_device *madev) {
  int i = 0;
  uint32_t flag = get_profile_flag(madev);

  // find corresponding profile
  for (i = 0; i < (int)MAX_NUM_PROFILE; i++) {
    if ((gProfile[i].v_mode == madev->mode) &&
        (gProfile[i].out_device == mrvl_path_manager.active_out_device) &&
        ((gProfile[i].in_device == 0) ||
         (gProfile[i].in_device == mrvl_path_manager.active_in_device)) &&
        (gProfile[i].flag == flag)) {
      return STR2ENUM_PROFILE(gProfile[i].profile);
    }
  }

  // the information will be recorded if getting the invalid profile
  ALOGE("%s, the current status is, mode %d, active_out_device 0x%x, flag 0x%x",
        __FUNCTION__, madev->mode, mrvl_path_manager.active_out_device, flag);
  return AUDIO_PROFILE_ID_ENUM_32_BIT;
}

void set_voice_call_volume(struct mrvl_audio_device *madev, float volume,
                           bool use_extra_vol) {
  signed char input_gain = 0;
  signed char input_gain_wb = 0;
  signed char output_gain = 0;
  signed char output_gain_wb = 0;
  unsigned char vc_output_vol = volume * 100;

  // get MSA gain from ACM XML, fix Mic volume to 100
  get_msa_gain(convert2_hwdev(madev, madev->in_device), 100, &input_gain,
               &input_gain_wb, use_extra_vol);
  get_msa_gain(convert2_hwdev(madev, madev->out_device), vc_output_vol,
               &output_gain, &output_gain_wb, use_extra_vol);

#ifdef WITH_TELEPHONY
  vcm_setvolume(input_gain, input_gain_wb, output_gain, output_gain_wb,
                vc_output_vol);
#endif
}

unsigned int get_loopback_headset_flag(struct mrvl_audio_device *madev) {
  unsigned int flag = CONNECT_STEREO_HEADSET;

  ALOGD("%s,headset_flag:%d", __FUNCTION__, madev->loopback_param.headset_flag);

  if (madev->loopback_param.on) {
    if (madev->loopback_param.headset_flag == MONO_HEADSET_L) {
      flag = CONNECT_MONO_HEADSET_L;
    } else if (madev->loopback_param.headset_flag == MONO_HEADSET_R) {
      flag = CONNECT_MONO_HEADSET_R;
    } else {
      flag = CONNECT_STEREO_HEADSET;
    }
  }
  return flag;
}

void route_devices(struct mrvl_audio_device *madev, unsigned int devices,
                   int enable, unsigned int value) {
  unsigned int hw_dev = 0;
  ALOGI("%s devices 0x%x %s", __FUNCTION__, devices,
        (enable == METHOD_ENABLE) ? "enable" : "disable");

  // if audio HW is set to all Rx sound mute, then ignore out device enable
  if (mrvl_path_manager.mute_all_rx && (enable == METHOD_ENABLE) &&
      !(devices & AUDIO_DEVICE_BIT_IN)) {
    ALOGI("%s in All Rx sound mute status, ignore this operation.",
          __FUNCTION__);
    return;
  }

  // first check speaker device to consider force speaker mode
  // only check output direction, input won't enter
  if (!(devices & AUDIO_DEVICE_BIT_IN) &&
      (devices & AUDIO_DEVICE_OUT_SPEAKER)) {
    // convert android device to HW device and route HW device
    hw_dev = convert2_hwdev(madev, AUDIO_DEVICE_OUT_SPEAKER);
    route_hw_device(hw_dev, enable, value);
    devices &= ~AUDIO_DEVICE_OUT_SPEAKER;
  }

  // check if has other available input/output devices to process
  if (devices & ~AUDIO_DEVICE_BIT_IN) {
    hw_dev = convert2_hwdev(madev, devices);
    // flag for connectivity and coupling
    value |= get_mic_hw_flag(hw_dev);
    if ((devices & AUDIO_DEVICE_BIT_IN) && (enable == METHOD_ENABLE)) {
      mrvl_path_manager.enabled_in_hwdev = hw_dev;
    } else if ((devices & AUDIO_DEVICE_BIT_IN) && (enable == METHOD_DISABLE)) {
      hw_dev = mrvl_path_manager.enabled_in_hwdev;
    } else if (!(devices & AUDIO_DEVICE_BIT_IN) &&
               (devices & (AUDIO_DEVICE_OUT_WIRED_HEADSET |
                           AUDIO_DEVICE_OUT_WIRED_HEADPHONE))) {
      value |= get_loopback_headset_flag(madev);
      ALOGI("choose headset flag: 0x%x", value);
    }
    route_hw_device(hw_dev, enable, value);
  }
}

static void select_input_device(struct mrvl_audio_device *madev) {
  ALOGI("%s in_device 0x%x", __FUNCTION__, madev->in_device);

  // check active input interface to make sure device is configured later than
  // interface
  if (!has_active_in_itf()) {
    ALOGI("%s no active input interface, return.", __FUNCTION__);
    return;
  }

  // check input parameter
  if (((mrvl_path_manager.active_in_device == madev->in_device) &&
       (mrvl_path_manager.enabled_in_hwdev ==
        convert2_hwdev(madev, madev->in_device))) ||
      ((madev->in_device & ~AUDIO_DEVICE_BIT_IN) == AUDIO_DEVICE_NONE)) {
    ALOGI("%s same or null device, return.", __FUNCTION__);
    return;
  }

  // disable old device if exists
  if (mrvl_path_manager.active_in_device) {
    // hot disable device, which means don't shutdown codec
    route_devices(madev, mrvl_path_manager.active_in_device, METHOD_DISABLE, 1);
    mrvl_path_manager.active_in_device = AUDIO_DEVICE_NONE;
    select_virtual_path(madev);
  }

  // enable the new device
  route_devices(madev, madev->in_device, METHOD_ENABLE, 0);
  mrvl_path_manager.active_in_device = madev->in_device;

  // process new virtual path
  select_virtual_path(madev);
}

static struct pcm *open_tiny_alsa_device(unsigned int device,
                                         unsigned int direction,
                                         struct pcm_config *config) {
  ALOGI(
      "%s: device %u, direction %u, channels %u, sample_rate %u, period_size "
      "%u",
      __FUNCTION__, device, direction, config->channels, config->rate,
      config->period_size);

  struct pcm *pcm = NULL;
  unsigned int req_period_size = config->period_size;
  unsigned int req_period_count = config->period_count;
  pcm = pcm_open(ALSA_CARD_DEFAULT, device, direction, config);
  if (!pcm_is_ready(pcm)) {
    ALOGE("%s: unable to open PCM device%u (%s)", __FUNCTION__, device,
          pcm_get_error(pcm));
    pcm_close(pcm);
    pcm = NULL;
  } else {
    if (((device == ALSA_DEVICE_DEFAULT) ||
         (device == ALSA_DEVICE_DEEP_BUFFER)) &&
        ((config->period_size != req_period_size) ||
         (config->period_count != req_period_count))) {
      ALOGE("%s: Warning: period size changed error!!!", __FUNCTION__);
    }
  }
  return pcm;
}

static void select_output_device(struct mrvl_audio_device *madev) {
  ALOGI("%s switch device from old device 0x%x to new device 0x%x ",
        __FUNCTION__, mrvl_path_manager.active_out_device, madev->out_device);

  bool cp_mute = false;
  // check active output interface to make sure device is configured later than
  // interface
  if (!has_active_out_itf()) {
    ALOGI("%s no active output interface, return.", __FUNCTION__);
    return;
  }

  if (madev->in_fm && popcount(madev->out_device) == 2) {
    ALOGI("%s Mute FM when playing force speaker stream", __FUNCTION__);
    set_hw_volume(V_MODE_FM, convert2_hwdev(madev, madev->fm_device), 0);
  }

  if (madev->mode == AUDIO_MODE_IN_CALL) {
    // select input devices and route input device
    madev->in_device = get_input_dev(madev->out_device);
// Mute CP before switch Tx path.
#ifdef WITH_TELEPHONY
    vcm_mute_all(true, get_cpmute_rampdown_level());
    cp_mute = true;
#endif
    select_input_device(madev);

    if ((madev->tty_mode == TTY_MODE_HCO) &&
        (madev->in_device == AUDIO_DEVICE_IN_WIRED_HEADSET)) {
      ALOGI(
          "%s tty_mode= %d, in_device= 0x%x ==> Change out_device from 0x%x to "
          "AUDIO_DEVICE_OUT_SPEAKER",
          __FUNCTION__, madev->tty_mode, madev->in_device, madev->out_device);
      madev->out_device = AUDIO_DEVICE_OUT_SPEAKER;
    }

    // notify CP to update device and re-set volume
    if ((!madev->in_call) ||
        (mrvl_path_manager.active_out_device != madev->out_device)) {
      unsigned int params = get_call_params(madev);
#ifdef WITH_TELEPHONY
      vcm_select_path(madev->out_device, madev->in_device, params);
      set_voice_call_volume(madev, madev->voice_volume, madev->use_extra_vol);
#endif
      ALOGD("%s CP enter In Call state.", __FUNCTION__);
      madev->in_call = true;  // update IN_CALL status
    }
  } else {
    if (madev->in_call) {
      ALOGD("%s CP leave In Call state.", __FUNCTION__);
      madev->in_call = false;  // update IN_CALL status

      // If call ended, and user's choice is TTY_HCO, and the headset was pulled
      // out during
      // the call, the next call while using loud-speaker should NOT be in
      // TTY_HCO mode
      if ((madev->tty_mode != TTY_MODE_OFF) &&
          (madev->in_device != AUDIO_DEVICE_IN_WIRED_HEADSET)) {
        ALOGD("%s: Call Ended and HS is out ==> set TTY Mode to TTY_OFF",
              __FUNCTION__);
        madev->tty_mode = TTY_MODE_OFF;
      }
    }
  }

  if ((is_in_voip_vt(madev->mode)) && (!madev->loopback_param.on)) {
    // select input devices and route input device
    madev->in_device = get_input_dev(madev->out_device);
    if (madev->active_input) {
      ramp_down_start(SAMPLE_RATE_IN_DEFAULT);
      select_input_device(madev);
    }
  }

  // check duplicate devices for smoothly switch
  unsigned int unchange_dev =
      mrvl_path_manager.active_out_device & madev->out_device;

  // first check device to disable
  unsigned int dev_todisable =
      mrvl_path_manager.active_out_device & ~unchange_dev;
  if (dev_todisable) {
    // hot disable device, which means don't shutdown codec
    route_devices(madev, dev_todisable, METHOD_DISABLE, 1);
    mrvl_path_manager.active_out_device &= ~dev_todisable;
  }

  // then check device to enable
  unsigned int dev_toenable = madev->out_device & ~unchange_dev;
  if (dev_toenable) {
    route_devices(madev, dev_toenable, METHOD_ENABLE, 0);
    mrvl_path_manager.active_out_device |= dev_toenable;
  }

  // process new virtual path
  select_virtual_path(madev);

// Unmute CP if needed after switch Rx path.
#ifdef WITH_TELEPHONY
  if (cp_mute) {
    vcm_mute_all(false, get_cpmute_rampup_level());
    cp_mute = false;
  }
#endif

  // use saved fm_volume value to update fm volume if fm virtual path exists
  if (madev->fm_handle && (madev->fm_volume != 0)) {
    set_hw_volume(V_MODE_FM, convert2_hwdev(madev, madev->out_device),
                  madev->fm_volume);
  }
}

static void select_interface(struct mrvl_audio_device *madev,
                             path_interface_t ipath, bool enable) {
  int i = 0;
  ALOGI("%s path %d %s", __FUNCTION__, ipath, enable ? "enable" : "disable");

  if ((ipath < ID_IPATH_RX_MIN) || (ipath > ID_IPATH_TX_MAX)) {
    ALOGE("%s path invalid %d", __FUNCTION__, ipath);
    return;
  }

  // ignore duplicate commands. interface is disabled in some concurrency
  // and may disabled again when enter standby
  if (mrvl_path_manager.itf_state[ipath] == enable) {
    ALOGI("%s ignore duplicated control for %d", __FUNCTION__, ipath);
    return;
  }

  // update path interface status
  mrvl_path_manager.itf_state[ipath] = enable;

  if (enable) {
    // enable path interface before enabling device
    route_interface(ipath, METHOD_ENABLE);
  } else {
    // if all playback input paths disabled, then disable output path
    bool all_standby = true;
    for (i = ID_IPATH_RX_MIN; i <= ID_IPATH_RX_MAX; i++) {
      if (mrvl_path_manager.itf_state[i] == true) {
        all_standby = false;
        break;
      }
    }

    // disable all devices
    if (all_standby &&
        (mrvl_path_manager.active_out_device != AUDIO_DEVICE_NONE)) {
      route_devices(madev, mrvl_path_manager.active_out_device, METHOD_DISABLE,
                    0);
      mrvl_path_manager.active_out_device = AUDIO_DEVICE_NONE;
    }

    // if all record output paths disabled, then disable input path
    all_standby = true;
    for (i = ID_IPATH_TX_MIN; i <= ID_IPATH_TX_MAX; i++) {
      if (mrvl_path_manager.itf_state[i] == true) {
        all_standby = false;
        break;
      }
    }
    if (all_standby &&
        (mrvl_path_manager.active_in_device != AUDIO_DEVICE_NONE)) {
      route_devices(madev, mrvl_path_manager.active_in_device, METHOD_DISABLE,
                    0);
      mrvl_path_manager.active_in_device = AUDIO_DEVICE_NONE;
    }

    // disable path interface after disabling device
    route_interface(ipath, METHOD_DISABLE);
  }

  // process new virtual path
  select_virtual_path(madev);
}

static int start_output_stream_low_latency(struct mrvl_stream_out *out) {
  int ret = 0;
  struct mrvl_audio_device *madev = out->dev;
  ALOGD("%s", __FUNCTION__);

  if (out->handle) {
    pcm_close(out->handle);
    out->handle = NULL;
  }
  out->handle = open_tiny_alsa_device(ALSA_DEVICE_DEFAULT,
                                      PCM_OUT | PCM_NORESTART | PCM_MONOTONIC,
                                      &pcm_config_ll);
  if (!out->handle) {
    ALOGE("%s open tiny alsa device error", __FUNCTION__);
    return -1;
  }
  // configure playback input path
  select_interface(madev, ID_IPATH_RX_HIFI_LL, true);

  // configure playback output path if necessary
  // only when not in VC/FM mode, using fm_handle to distinguish whether FM path
  // is enabled
  if ((madev->mode != AUDIO_MODE_IN_CALL) && (!madev->fm_handle)) {
    select_output_device(madev);
  }

  return 0;
}

static int start_output_stream_deep_buffer(struct mrvl_stream_out *out) {
  int ret = 0;
  struct mrvl_audio_device *madev = out->dev;
  ALOGD("%s", __FUNCTION__);

  if (out->handle) {
    pcm_close(out->handle);
    out->handle = NULL;
  }
  out->handle =
      open_tiny_alsa_device(ALSA_DEVICE_DEEP_BUFFER,
                            PCM_OUT | PCM_MMAP | PCM_MONOTONIC, &pcm_config_db);
  if (!out->handle) {
    ALOGE("%s open tiny alsa device error", __FUNCTION__);
    return -1;
  }

  out->write_threshold =
      DEEP_BUFFER_LONG_PERIOD_SIZE * DEEP_BUFFER_LONG_PERIOD_COUNT;
  out->use_long_periods = true;

  // configure playback input path
  select_interface(madev, ID_IPATH_RX_HIFI_DB, true);

  // configure Music playback output and input path
  // only when not in VC/FM mode, using fm_handle to distinguish whether FM path
  // is enabled
  if ((madev->mode != AUDIO_MODE_IN_CALL) && (!madev->fm_handle)) {
    select_output_device(madev);
  }

  return 0;
}

// must be called with hw device and output stream mutexes locked
static int start_input_stream(struct mrvl_stream_in *in) {
  int ret = 0;
  struct mrvl_audio_device *madev = in->dev;
  ALOGD("%s in->source %d", __FUNCTION__, in->source);
  if (in->handle) {
    pcm_close(in->handle);
    in->handle = NULL;
  }

  if (in->source == AUDIO_SOURCE_FMRADIO) {
    if (madev->mode != AUDIO_MODE_IN_CALL) {
      // FIXME, open alsa device
      in->handle = open_tiny_alsa_device(
          ALSA_DEVICE_DEFAULT, PCM_IN | PCM_NORESTART, &pcm_config_input);
      if (!in->handle) {
        ALOGE("%s open tiny alsa device error", __FUNCTION__);
        return -1;
      }
      // configure FM record output path
      select_interface(madev, ID_IPATH_TX_FM, true);
    } else {
      ALOGW("%s forbid FM record during VC", __FUNCTION__);
      return -1;
    }
  } else {
    // open default hifi input alsa device
    in->handle = open_tiny_alsa_device(ALSA_DEVICE_DEFAULT,
                                       PCM_IN | PCM_NORESTART | PCM_MONOTONIC,
                                       &pcm_config_input);
    if (!in->handle) {
      ALOGE("%s open tiny alsa device error", __FUNCTION__);
      return -1;
    }

    // enable voice recognition
    if (in->source == AUDIO_SOURCE_VOICE_RECOGNITION) {
      madev->use_voice_recognition = true;
    }

    // configure hifi record output path
    select_interface(madev, ID_IPATH_TX_HIFI, true);

    // configure record input path if necessary
    if (madev->mode != AUDIO_MODE_IN_CALL) {
      ramp_up_start(SAMPLE_RATE_IN_DEFAULT);
      select_input_device(madev);
    }
  }

  madev->active_input = in;

  return 0;
}
// must be called with hw device and input stream mutexes locked
static int do_input_standby(struct mrvl_stream_in *in) {
  ALOGI("%s in %p", __FUNCTION__, in);
  struct mrvl_audio_device *madev = in->dev;

  if (!in->standby) {
    in->standby = true;

    // stop pcm read/write before close audio path.
    if (in->handle) {
      pcm_stop(in->handle);
    }

    if (in->source == AUDIO_SOURCE_FMRADIO) {
      select_interface(in->dev, ID_IPATH_TX_FM, false);
    } else {
      select_interface(in->dev, ID_IPATH_TX_HIFI, false);
    }

    // disable voice recognition
    if (in->source == AUDIO_SOURCE_VOICE_RECOGNITION) {
      madev->use_voice_recognition = false;
    }

    if (in->handle) {
      pcm_close(in->handle);
      in->handle = NULL;
    }
    madev->active_input = NULL;
  }
  return 0;
}

// must be called with hw device and output stream mutexes locked
static int do_output_standby(struct mrvl_stream_out *out) {
  struct mrvl_audio_device *madev = out->dev;
  ALOGI("%s out %p standby %s", __FUNCTION__, out,
        out->standby ? "true" : "false");

  if (!out->standby) {
    out->standby = true;

    // stop pcm read/write before close audio path.
    if (out->handle) {
      pcm_stop(out->handle);
    }

    if (out == madev->outputs[OUTPUT_LOW_LATENCY]) {
      select_interface(out->dev, ID_IPATH_RX_HIFI_LL, false);
    } else if (out == madev->outputs[OUTPUT_DEEP_BUF]) {
      select_interface(out->dev, ID_IPATH_RX_HIFI_DB, false);
    } else {
      ALOGE("%s unrecognized stream out", __FUNCTION__);
    }

    if (out->handle) {
      pcm_close(out->handle);
      out->handle = NULL;
    }
  }

  return 0;
}

// must be called with hw device mutex locked
static void force_all_standby(struct mrvl_audio_device *madev) {
  struct mrvl_stream_out *out = NULL;
  struct mrvl_stream_in *in = NULL;
  // standby both low latency and deep buffer output and input.
  // it can used to device switch or deep buffer output resume
  out = madev->outputs[OUTPUT_LOW_LATENCY];
  if (out != NULL && !out->standby) {
    pthread_mutex_lock(&out->lock);
    do_output_standby(out);
    pthread_mutex_unlock(&out->lock);
  }

  out = madev->outputs[OUTPUT_DEEP_BUF];
  if (out != NULL && !out->standby) {
    pthread_mutex_lock(&out->lock);
    do_output_standby(out);
    pthread_mutex_unlock(&out->lock);
  }

  if (madev->active_input) {
    in = madev->active_input;
    pthread_mutex_lock(&in->lock);
    do_input_standby(in);
    pthread_mutex_unlock(&in->lock);
  }
}

// FM handling function, for internal use
static void set_fm_parameters(struct mrvl_audio_device *madev,
                              struct str_parms *parms) {
  int fm_status = 0;
  int fm_device = 0;
  int fm_volume = 0;
  pthread_mutex_lock(&madev->lock);

  // set fm volume
  if (str_parms_get_int(parms, AUDIO_PARAMETER_FM_VOLUME, &fm_volume) >= 0) {
    ALOGI("%s: set FM volume %d", __FUNCTION__, fm_volume);
    madev->fm_volume = fm_volume;
    set_hw_volume(V_MODE_FM, convert2_hwdev(madev, madev->out_device),
                  fm_volume);
    str_parms_del(parms, AUDIO_PARAMETER_FM_VOLUME);
  }

  // set fm status
  if (str_parms_get_int(parms, AUDIO_PARAMETER_FM_STATUS, &fm_status) >= 0) {
    switch (fm_status) {
      case FM_DISABLE:  // disable
        if (str_parms_get_int(parms, AUDIO_PARAMETER_FM_DEVICE, &fm_device) >=
            0) {
          // disable FM playback input path
          ALOGI("%s: disable FM", __FUNCTION__);
          if (madev->in_fm) {
            select_interface(madev, ID_IPATH_RX_FM, false);
            madev->in_fm = false;
            madev->fm_device = 0;
            madev->fm_volume = 0;

            // close FE when disable FM
            if (madev->fm_handle) {
              pcm_stop(madev->fm_handle);
              pcm_close(madev->fm_handle);
              madev->fm_handle = NULL;
            }
          }
        }
        break;
      case FM_ENABLE:  // enable
        if (str_parms_get_int(parms, AUDIO_PARAMETER_FM_DEVICE, &fm_device) >=
            0) {
          ALOGI("%s: enable FM", __FUNCTION__);
          if (!madev->in_fm) {
            // open FE when enable FM
            if (!madev->fm_handle) {
              madev->fm_handle = open_tiny_alsa_device(ALSA_DEVICE_FM, PCM_OUT,
                                                       &pcm_config_fm);
              if (!madev->fm_handle) {
                ALOGE("%s open tiny alsa device error", __FUNCTION__);
                break;
              }
              pcm_start(madev->fm_handle);
            }

            // enable FM playback input path
            select_interface(madev, ID_IPATH_RX_FM, true);
            madev->in_fm = true;
          }
          // enable or switch FM output path
          madev->out_device = fm_device;
          madev->fm_device = fm_device;
          select_output_device(madev);
        }
        break;
    }
    str_parms_del(parms, AUDIO_PARAMETER_FM_DEVICE);
    str_parms_del(parms, AUDIO_PARAMETER_FM_STATUS);
  }
  pthread_mutex_unlock(&madev->lock);
}

static bool is_phone_call(int mode) { return (mode == AUDIO_MODE_IN_CALL); }

static void select_mode(struct mrvl_audio_device *madev, int mode) {
  int old_mode = madev->mode;

  // close FM when change mode from NORMAL to others
  if ((old_mode == AUDIO_MODE_NORMAL) && (mode != AUDIO_MODE_NORMAL)) {
    if (madev->in_fm) {
      select_interface(madev, ID_IPATH_RX_FM, false);
      if (madev->fm_handle) {
        pcm_stop(madev->fm_handle);
        pcm_close(madev->fm_handle);
        madev->fm_handle = NULL;
      }
    }
  }

  // case 1: change mode from non-phone call to phone call
  if (!is_phone_call(old_mode) && is_phone_call(mode)) {
    ALOGI("%s Entering IN_CALL state: %d -> %d", __FUNCTION__, old_mode, mode);

    // force earpiece route for in call state if speaker is the
    // only currently selected route. This prevents having to tear
    // down the modem PCMs to change route from speaker to earpiece
    // after the ringtone is played, but doesn't cause a route
    // change if a headset or bt device is already connected. If
    // speaker is not the only thing active, just remove it from
    // the route. We'll assume it'll never be used initially during
    // a call. This works because we're sure that the audio policy
    // manager will update the output device after the audio mode
    // change, even if the device selection did not change.
    if (madev->out_device == AUDIO_DEVICE_OUT_SPEAKER) {
      madev->out_device = AUDIO_DEVICE_OUT_EARPIECE;
    } else {
      madev->out_device &= ~AUDIO_DEVICE_OUT_SPEAKER;
    }

    // open FE when enter IN_CALL mode
    if (!madev->phone_dl_handle) {
      madev->phone_dl_handle =
          open_tiny_alsa_device(ALSA_DEVICE_VOICE, PCM_OUT, &pcm_config_phone);
      if (!madev->phone_dl_handle) {
        ALOGE("%s open tiny alsa device error", __FUNCTION__);
        return;
      }
      pcm_start(madev->phone_dl_handle);
    }
    if (!madev->phone_ul_handle) {
      madev->phone_ul_handle =
          open_tiny_alsa_device(ALSA_DEVICE_VOICE, PCM_IN, &pcm_config_phone);
      if (!madev->phone_ul_handle) {
        ALOGE("%s open tiny alsa device error", __FUNCTION__);
        pcm_stop(madev->phone_dl_handle);
        pcm_close(madev->phone_dl_handle);
        madev->phone_dl_handle = NULL;
        return;
      }
      pcm_start(madev->phone_ul_handle);
    }

    // configure VC interface, both Playback and Record
    select_interface(madev, ID_IPATH_RX_VC, true);
    select_interface(madev, ID_IPATH_TX_VC, true);

    // case 2: change mode from phone call to non-phone call
  } else if (is_phone_call(old_mode) && !is_phone_call(mode)) {
    ALOGI("%s Leaving IN_CALL state: %d -> %d", __FUNCTION__, old_mode, mode);

    // configure VC interface, both Playback and Record
    select_interface(madev, ID_IPATH_RX_VC, false);
    select_interface(madev, ID_IPATH_TX_VC, false);

    // close FE when leave IN_CALL mode
    if (madev->phone_dl_handle) {
      pcm_stop(madev->phone_dl_handle);
      pcm_close(madev->phone_dl_handle);
      madev->phone_dl_handle = NULL;
    }
    if (madev->phone_ul_handle) {
      pcm_stop(madev->phone_ul_handle);
      pcm_close(madev->phone_ul_handle);
      madev->phone_ul_handle = NULL;
    }

    // case 3: change mode from non-phone call to non-phone call
  } else {
    ALOGI("%s Nothing change when switching between non-phone calls",
          __FUNCTION__);
  }

  // resume FM when change mode from others to NORMAL
  if ((old_mode != AUDIO_MODE_NORMAL) && (mode == AUDIO_MODE_NORMAL)) {
    if (madev->in_fm) {
      if (!madev->fm_handle) {
        madev->fm_handle =
            open_tiny_alsa_device(ALSA_DEVICE_FM, PCM_OUT, &pcm_config_fm);
        if (!madev->fm_handle) {
          ALOGE("%s open tiny alsa device error", __FUNCTION__);
          return;
        }
        pcm_start(madev->fm_handle);
      }
      select_interface(madev, ID_IPATH_RX_FM, true);
    }
  }

  madev->mode = mode;
  select_output_device(madev);
  if (madev->active_input) {
    select_input_device(madev);
  }
}

static void enable_loopback(struct mrvl_audio_device *madev) {
  ALOGI("%s: out_device 0x%x, in_device 0x%x", __FUNCTION__, madev->out_device,
        madev->in_device);

  float loopback_volume = 0.8;

  if (madev->loopback_param.type == APP_LOOPBACK) {
    select_input_device(madev);
    select_output_device(madev);
    return;
  }

  if (!madev->phone_dl_handle) {
    madev->phone_dl_handle =
        open_tiny_alsa_device(ALSA_DEVICE_VOICE, PCM_OUT, &pcm_config_phone);
    if (!madev->phone_dl_handle) {
      ALOGE("%s open tiny alsa device error", __FUNCTION__);
      return;
    }
    pcm_start(madev->phone_dl_handle);
  }
  if (!madev->phone_ul_handle) {
    madev->phone_ul_handle =
        open_tiny_alsa_device(ALSA_DEVICE_VOICE, PCM_IN, &pcm_config_phone);
    if (!madev->phone_ul_handle) {
      ALOGE("%s open tiny alsa device error", __FUNCTION__);
      pcm_stop(madev->phone_dl_handle);
      pcm_close(madev->phone_dl_handle);
      madev->phone_dl_handle = NULL;
      return;
    }
    pcm_start(madev->phone_ul_handle);
  }

  // configure VC interface, both Playback and Record
  if (madev->loopback_param.type == CP_LOOPBACK) {
    select_interface(madev, ID_IPATH_RX_VC, true);
    select_interface(madev, ID_IPATH_TX_VC, true);

#ifdef WITH_TELEPHONY
    vcm_mute_all(true, get_cpmute_rampdown_level());
    vcm_set_loopback(madev->out_device, true);
    set_voice_call_volume(madev, loopback_volume, 0);
    set_hw_volume(V_MODE_VC, convert2_hwdev(madev, madev->out_device),
                  (unsigned char)(loopback_volume * 100));
    vcm_mute_all(false, get_cpmute_rampup_level());
#endif

  } else if (madev->loopback_param.type == HARDWARE_LOOPBACK) {
    select_interface(madev, ID_IPATH_RX_VC_ST, true);
    select_interface(madev, ID_IPATH_TX_VC, true);
  } else {
    ALOGE("%s: Invalid loop type", __FUNCTION__);
  }

  select_input_device(madev);
  select_output_device(madev);
}

static void disable_loopback(struct mrvl_audio_device *madev) {
  ALOGI("%s: out_device 0x%x, in_device 0x%x, loopback_mode:%d", __FUNCTION__,
        madev->out_device, madev->in_device, madev->loopback_param.type);

  if (madev->loopback_param.type == APP_LOOPBACK) {
    return;
  }
  // configure VC interface, both Playback and Record
  if (madev->loopback_param.type == CP_LOOPBACK) {
    select_interface(madev, ID_IPATH_RX_VC, false);
    select_interface(madev, ID_IPATH_TX_VC, false);

#ifdef WITH_TELEPHONY
    vcm_mute_all(true, get_cpmute_rampdown_level());
    set_voice_call_volume(madev, 0, 0);
    vcm_set_loopback(madev->out_device, false);
    set_hw_volume(V_MODE_VC, convert2_hwdev(madev, madev->out_device), 0);
    vcm_mute_all(false, get_cpmute_rampup_level());
#endif

  } else if (madev->loopback_param.type == HARDWARE_LOOPBACK) {
    select_interface(madev, ID_IPATH_RX_VC_ST, false);
    select_interface(madev, ID_IPATH_TX_VC, false);
  } else {
    ALOGE("%s: Invalid loop type", __FUNCTION__);
  }

  if (madev->phone_dl_handle) {
    pcm_stop(madev->phone_dl_handle);
    pcm_close(madev->phone_dl_handle);
    madev->phone_dl_handle = NULL;
  }
  if (madev->phone_ul_handle) {
    pcm_stop(madev->phone_ul_handle);
    pcm_close(madev->phone_ul_handle);
    madev->phone_ul_handle = NULL;
  }
}

static void select_loopback(struct mrvl_audio_device *madev, bool value) {
  ALOGD("loopback_out_device:0x%x, loopback_in_device:0x%x",
        madev->loopback_param.out_device, madev->loopback_param.in_device);
  if (value) {
    madev->out_device = madev->loopback_param.out_device;
    madev->in_device = madev->loopback_param.in_device;
    madev->loopback_param.on = true;
    enable_loopback(madev);
  } else {
    madev->out_device = AUDIO_DEVICE_OUT_SPEAKER;
    madev->in_device = AUDIO_DEVICE_IN_BUILTIN_MIC;
    madev->loopback_param.on = false;
    disable_loopback(madev);
  }
}

// ----------------------------------------------------------------------
// External Interfaces
// ----------------------------------------------------------------------
static uint32_t out_get_sample_rate(const struct audio_stream *stream) {
  struct mrvl_stream_out *out = (struct mrvl_stream_out *)stream;
  return out->sample_rate;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate) {
  struct mrvl_stream_out *out = (struct mrvl_stream_out *)stream;
  ALOGD("%s sample_rate %d", __FUNCTION__, rate);
  out->sample_rate = rate;
  return 0;
}

static size_t out_get_buffer_size_low_latency(
    const struct audio_stream *stream) {
  struct mrvl_stream_out *out = (struct mrvl_stream_out *)stream;

  // take resampling into account and return the closest majoring
  // multiple of 16 frames, as audioflinger expects audio buffers to
  // be a multiple of 16 frames.
  int buffer_size = ((out->period_size + 15) / 16) * 16;
  ALOGD("%s buffer_size %d", __FUNCTION__, buffer_size);
  return buffer_size * audio_stream_frame_size((struct audio_stream *)stream);
}

static size_t out_get_buffer_size_deep_buffer(
    const struct audio_stream *stream) {
  struct mrvl_stream_out *out = (struct mrvl_stream_out *)stream;

  // take resampling into account and return the closest majoring
  // multiple of 16 frames, as audioflinger expects audio buffers to
  // be a multiple of 16 frames.
  int buffer_size = ((DEEP_BUFFER_SHORT_PERIOD_SIZE + 15) / 16) * 16;
  ALOGD("%s buffer_size %d", __FUNCTION__, buffer_size);
  return buffer_size * audio_stream_frame_size((struct audio_stream *)stream);
}

static audio_channel_mask_t out_get_channels(
    const struct audio_stream *stream) {
  struct mrvl_stream_out *out = (struct mrvl_stream_out *)stream;
  return out->channel_mask;
}

static audio_format_t out_get_format(const struct audio_stream *stream) {
  struct mrvl_stream_out *out = (struct mrvl_stream_out *)stream;
  return (audio_format_t)out->format;
}

static int out_set_format(struct audio_stream *stream, audio_format_t format) {
  struct mrvl_stream_out *out = (struct mrvl_stream_out *)stream;
  ALOGD("%s", __FUNCTION__);
  out->format = format;
  return 0;
}

static int out_standby(struct audio_stream *stream) {
  struct mrvl_stream_out *out = (struct mrvl_stream_out *)stream;
  struct mrvl_audio_device *madev = out->dev;
  ALOGI("%s out %p", __FUNCTION__, out);

#ifdef MRVL_AEC
  if (out == madev->outputs[OUTPUT_LOW_LATENCY]) {
    out_release_effect((struct audio_stream *)out,
                       (effect_uuid_t *)FX_IID_VOIPRX);
  }
#endif

  pthread_mutex_lock(&out->dev->lock);
  pthread_mutex_lock(&out->lock);

#ifdef WITH_ACOUSTIC
  acoustic_manager_reset(out->acoustic_manager);
#endif

  do_output_standby(out);

  pthread_mutex_unlock(&out->lock);
  pthread_mutex_unlock(&out->dev->lock);
  return 0;
}

static int out_dump(const struct audio_stream *stream, int fd) {
  const size_t SIZE = 256;
  char buffer[SIZE];
  char result[SIZE];
  snprintf(buffer, SIZE, "out_dump()");
  strcpy(result, buffer);
  snprintf(buffer, SIZE, "\tsample rate: %d", out_get_sample_rate(stream));
  strcat(result, buffer);
  snprintf(buffer, SIZE, "\tchannel count: %d", out_get_channels(stream));
  strcat(result, buffer);
  snprintf(buffer, SIZE, "\tformat: %d", out_get_format(stream));
  strcat(result, buffer);
  write(fd, result, strlen(result));
  return 0;
}

static int out_set_parameters(struct audio_stream *stream,
                              const char *kvpairs) {
  struct mrvl_stream_out *out = (struct mrvl_stream_out *)stream;
  struct mrvl_audio_device *madev = out->dev;
  struct str_parms *parms;
  char value[PARAM_SIZE];
  int ret = 0;
  int val = 0;
  int hw_vol = 0;
  ALOGI("%s: %s", __FUNCTION__, kvpairs);

  parms = str_parms_create_str(kvpairs);

  // for FM handling
  set_fm_parameters(madev, parms);

  ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING, value,
                          sizeof(value));
  if (ret >= 0) {
    val = atoi(value);
    if (val != 0) {
      pthread_mutex_lock(&madev->lock);
      pthread_mutex_lock(&out->lock);
      madev->out_device = val;
      select_output_device(madev);
      // set volume value to ACM for volume calibration for voice call.
      if (madev->mode == AUDIO_MODE_IN_CALL) {
        set_hw_volume(V_MODE_VC, convert2_hwdev(madev, madev->out_device),
                      madev->voice_volume * 100);
      }
      pthread_mutex_unlock(&out->lock);
      pthread_mutex_unlock(&madev->lock);
    }
#ifdef MRVL_AEC
    if (out == madev->outputs[OUTPUT_LOW_LATENCY] &&
        is_in_voip_vt(madev->mode)) {
      uint32_t audio_profile = get_profile(madev);
      if (audio_profile != AUDIO_PROFILE_ID_ENUM_32_BIT) {
        effect_set_profile(audio_profile, madev);
      }
    }
#endif
    str_parms_del(parms, AUDIO_PARAMETER_STREAM_ROUTING);
  }

  // set hw volume that used for force speaker
  if (str_parms_get_int(parms, AUDIO_PARAMETER_STREAM_HW_VOLUME, &hw_vol) >=
      0) {
    ALOGI("%s: set HW volume %d", __FUNCTION__, hw_vol);
    set_hw_volume(V_MODE_HIFI_LL, convert2_hwdev(madev, madev->out_device),
                  hw_vol);
    str_parms_del(parms, AUDIO_PARAMETER_STREAM_HW_VOLUME);
  }

  str_parms_destroy(parms);

#ifdef WITH_ACOUSTIC
  if (madev->out_device) {
    acoustic_manager_init(&(out->acoustic_manager), out->sample_rate,
                          popcount(out->channel_mask), madev->out_device);
  }
#endif

  return ret;
}

static char *out_get_parameters(const struct audio_stream *stream,
                                const char *keys) {
  struct mrvl_stream_out *out = (struct mrvl_stream_out *)stream;
  struct mrvl_audio_device *madev = out->dev;
  struct str_parms *param;
  char value[PARAM_SIZE];
  const char *key;
  char *ret_val = NULL;
  ALOGI("%s keys %s", __FUNCTION__, keys);

  param = str_parms_create_str(keys);
  if (!param) return ret_val;
  key = AUDIO_PARAMETER_STREAM_ROUTING;

  if (str_parms_get_str(param, key, value, sizeof(value)) >= 0) {
    str_parms_add_int(param, key, (int)madev->out_device);
    ret_val = str_parms_to_str(param);
    ALOGV("%s: %s", __FUNCTION__, ret_val);
    str_parms_del(param, key);
  }

  str_parms_destroy(param);

  return ret_val;
}

static uint32_t out_get_latency_low_latency(
    const struct audio_stream_out *stream) {
  struct mrvl_stream_out *out = (struct mrvl_stream_out *)stream;
  ALOGD("%s period_size %d period_count %d", __FUNCTION__, out->period_size,
        out->period_count);
  return 1000 * out->period_size * out->period_count / out->sample_rate;
}

static uint32_t out_get_latency_deep_buffer(
    const struct audio_stream_out *stream) {
  struct mrvl_stream_out *out = (struct mrvl_stream_out *)stream;
  ALOGD("%s period_size %d period_count %d", __FUNCTION__,
        DEEP_BUFFER_LONG_PERIOD_SIZE, DEEP_BUFFER_LONG_PERIOD_COUNT);
  return 1000 * DEEP_BUFFER_LONG_PERIOD_SIZE * DEEP_BUFFER_LONG_PERIOD_COUNT /
         out->sample_rate;
}

static int out_set_volume(struct audio_stream_out *stream, float left,
                          float right) {
  return -ENOSYS;
}

static ssize_t out_write_low_latency(struct audio_stream_out *stream,
                                     const void *buffer, size_t bytes) {
  int ret = -1;
  struct mrvl_stream_out *out = (struct mrvl_stream_out *)stream;
  struct mrvl_audio_device *madev = out->dev;
  bool force_input_standby = false;
  size_t out_frames = bytes / audio_stream_frame_size(&out->stream.common);

#ifdef AUDIO_DEBUG
  audio_debug_stream_dump(AH_RX_LL, buffer, bytes);
#endif

#ifdef MRVL_AEC
  if (out == madev->outputs[OUTPUT_LOW_LATENCY] && is_in_voip_vt(madev->mode)) {
    uint32_t audio_profile = get_profile(madev);
    if (audio_profile != AUDIO_PROFILE_ID_ENUM_32_BIT) {
      out_load_effect((struct audio_stream *)out,
                      (effect_uuid_t *)FX_IID_VOIPRX, audio_profile);
    }
  }
#endif

  // path operations should under protection of madev->lock
  // request madev->lock first to avoid deadlock
  pthread_mutex_lock(&madev->lock);
  pthread_mutex_lock(&out->lock);
  if (out->standby) {
    ret = start_output_stream_low_latency(out);
    if (ret != 0) {
      ALOGE("%s start output error. ret %d", __FUNCTION__, ret);
      pthread_mutex_unlock(&madev->lock);
      goto exit;
    }
    out->standby = false;
  }
  pthread_mutex_unlock(&madev->lock);

#ifdef MRVL_AEC
  if (is_in_voip_vt(madev->mode)) {
    effect_rx_process(out, buffer);
  }
#endif

#ifdef WITH_ACOUSTIC
  if (!is_in_voip_vt(madev->mode)) {
    acoustic_manager_process(out->acoustic_manager, (signed short *)buffer,
                             out_frames);
  }
#endif

#ifdef AUDIO_DEBUG
  audio_debug_stream_dump(AH_RX_AF_ACOUSTIC_LL, buffer, bytes);
#endif

  ret = pcm_write(out->handle, buffer, bytes);
  if (ret == -EPIPE) {
    ALOGW("%s: [UNDERRUN] failed to write, reason: broken pipe", __FUNCTION__);
    ret = pcm_write(out->handle, buffer, bytes);
  }

  if (ret == 0) {
    out->written += bytes / (pcm_config_ll.channels * sizeof(int16_t));
  } else if (ret < 0) {
    ALOGE("%s: failed to write, reason: %s", __FUNCTION__,
          pcm_get_error(out->handle));
    goto exit;
  }

#ifdef MRVL_AEC
  if (is_in_voip_vt(madev->mode)) {
    echo_ref_rx_write(out, buffer, bytes);
  }
#endif

exit:
  pthread_mutex_unlock(&out->lock);

  // delay when error happened except enter underrun
  if ((ret < 0) && (ret != -EPIPE)) {
    usleep(bytes * 1000000 / audio_stream_frame_size(&stream->common) /
           out_get_sample_rate(&stream->common));
  }
  return bytes;
}

static ssize_t out_write_deep_buffer(struct audio_stream_out *stream,
                                     const void *buffer, size_t bytes) {
  int ret = -1;
  struct mrvl_stream_out *out = (struct mrvl_stream_out *)stream;
  struct mrvl_audio_device *madev = out->dev;
  size_t out_frames = bytes / audio_stream_frame_size(&out->stream.common);
  bool use_long_periods = true;
  int avail_frames = 0;
  int kernel_frames = 0;

#ifdef AUDIO_DEBUG
  audio_debug_stream_dump(AH_RX_DB, buffer, bytes);
#endif

  // path operations should under protection of madev->lock
  // request madev->lock first to avoid deadlock
  pthread_mutex_lock(&madev->lock);
  pthread_mutex_lock(&out->lock);
  if (out->standby) {
    ret = start_output_stream_deep_buffer(out);
    if (ret != 0) {
      ALOGE("%s start output error. ret %d", __FUNCTION__, ret);
      pthread_mutex_unlock(&madev->lock);
      goto exit;
    }
    out->standby = false;
  }
  use_long_periods = madev->screen_off && !madev->active_input;
  pthread_mutex_unlock(&madev->lock);

#ifdef WITH_ACOUSTIC
  if (!is_in_voip_vt(madev->mode)) {
    acoustic_manager_process(out->acoustic_manager, (signed short *)buffer,
                             out_frames);
  }
#endif

#ifdef AUDIO_DEBUG
  audio_debug_stream_dump(AH_RX_AF_ACOUSTIC_DB, buffer, bytes);
#endif

  if (use_long_periods != out->use_long_periods) {
    if (use_long_periods) {
      out->write_threshold =
          DEEP_BUFFER_LONG_PERIOD_SIZE * DEEP_BUFFER_LONG_PERIOD_COUNT;
    } else {
      out->write_threshold =
          DEEP_BUFFER_SHORT_PERIOD_SIZE * DEEP_BUFFER_SHORT_PERIOD_COUNT;
    }
    // ALOGV("%s: write_threshold is %d.", __FUNCTION__, out->write_threshold);
    out->use_long_periods = use_long_periods;
  }

  // do not allow more than out->write_threshold frames in kernel pcm driver
  // buffer
  do {
    struct timespec time_stamp;

    // query the available frames in audio driver buffer
    if (pcm_get_htimestamp(out->handle, (unsigned int *)&avail_frames,
                           &time_stamp) < 0)
      break;

    kernel_frames = pcm_get_buffer_size(out->handle) - avail_frames;
    // ALOGV("%s: write_threshold is %d, kernel_frames is %d.", __FUNCTION__,
    // out->write_threshold, kernel_frames);
    if (kernel_frames > out->write_threshold) {
      unsigned long time =
          (unsigned long)(((int64_t)(kernel_frames - out->write_threshold) *
                           1000000) /
                          SAMPLE_RATE_OUT_DEFAULT);
      if (time < MIN_WRITE_SLEEP_US) time = MIN_WRITE_SLEEP_US;
      usleep(time);
    }
  } while (kernel_frames > out->write_threshold);

  ret = pcm_mmap_write(out->handle, buffer, bytes);
  if (ret == 0) {
    out->written += bytes / (pcm_config_db.channels * sizeof(int16_t));
  } else if (ret == -EPIPE) {
    ALOGW("%s: [UNDERRUN] failed to write, reason: broken pipe", __FUNCTION__);
  } else if (ret < 0) {
    ALOGE("%s: failed to write, reason: %s", __FUNCTION__,
          pcm_get_error(out->handle));
  }

exit:
  pthread_mutex_unlock(&out->lock);

  if (ret < 0) {
    usleep(bytes * 1000000 / audio_stream_frame_size(&stream->common) /
           out_get_sample_rate(&stream->common));
  }
  return bytes;
}

static int out_get_render_position(const struct audio_stream_out *stream,
                                   uint32_t *dsp_frames) {
  return -EINVAL;
}

static int out_get_presentation_position(const struct audio_stream_out *stream,
                                         uint64_t *frames,
                                         struct timespec *timestamp) {
  struct mrvl_stream_out *out = (struct mrvl_stream_out *)stream;
  int ret = -1;

  pthread_mutex_lock(&out->lock);

  size_t avail = 0;
  if (out->handle) {
    if (pcm_get_htimestamp(out->handle, &avail, timestamp) == 0) {
      int64_t kernel_buffer_size =
          (int64_t)out->period_size * (int64_t)out->period_count;
      int64_t signed_frames = out->written - kernel_buffer_size + avail;
      if (signed_frames >= 0) {
        *frames = signed_frames;
        ret = 0;
      }
    }
  }
  pthread_mutex_unlock(&out->lock);

  return ret;
}

static int out_add_audio_effect_fake(const struct audio_stream *stream,
                                     effect_handle_t effect) {
  return 0;
}

static int out_remove_audio_effect_fake(const struct audio_stream *stream,
                                        effect_handle_t effect) {
  return 0;
}

// audio_stream_in implementation
static uint32_t in_get_sample_rate(const struct audio_stream *stream) {
  struct mrvl_stream_in *in = (struct mrvl_stream_in *)stream;
  return in->sample_rate;
}

static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate) {
  return 0;
}

static size_t in_get_buffer_size(const struct audio_stream *stream) {
  struct mrvl_stream_in *in = (struct mrvl_stream_in *)stream;

  // take resampling into account and return the closest majoring
  // multiple of 16 frames, as audioflinger expects audio buffers to
  // be a multiple of 16 frames.
  int buffer_size = ((in->period_size + 15) / 16) * 16;
  ALOGD("%s buffer_size %d", __FUNCTION__, buffer_size);
  return buffer_size * audio_stream_frame_size((struct audio_stream *)stream);
}

static uint32_t in_get_channels(const struct audio_stream *stream) {
  struct mrvl_stream_in *in = (struct mrvl_stream_in *)stream;
  return in->channel_mask;
}

static audio_format_t in_get_format(const struct audio_stream *stream) {
  struct mrvl_stream_in *in = (struct mrvl_stream_in *)stream;
  return in->format;
}

static int in_set_format(struct audio_stream *stream, audio_format_t format) {
  return 0;
}

static int in_standby_vc_rec(struct audio_stream *stream) {
  struct mrvl_stream_in *in = (struct mrvl_stream_in *)stream;
  ALOGI("%s in %p", __FUNCTION__, in);

  pthread_mutex_lock(&in->lock);
  if (!in->standby) {
#ifdef WITH_TELEPHONY
    vcm_recording_stop();
#endif
    in->standby = true;
  }
  pthread_mutex_unlock(&in->lock);
  return 0;
}

static int in_standby_primary(struct audio_stream *stream) {
  struct mrvl_stream_in *in = (struct mrvl_stream_in *)stream;
  struct mrvl_audio_device *madev = in->dev;
  ALOGI("%s in %p", __FUNCTION__, in);

#ifdef MRVL_AEC
  in_release_effect((struct audio_stream *)in, (effect_uuid_t *)FX_IID_VOIPTX);
  pthread_mutex_lock(&madev->lock);
  pthread_mutex_lock(&in->lock);
  remove_echo_ref(in, madev->outputs[OUTPUT_LOW_LATENCY]);
  pthread_mutex_unlock(&in->lock);
  pthread_mutex_unlock(&madev->lock);
#endif

  pthread_mutex_lock(&madev->lock);
  pthread_mutex_lock(&in->lock);

#ifdef WITH_ACOUSTIC
  acoustic_manager_reset(in->acoustic_manager);
#endif

  do_input_standby(in);

  pthread_mutex_unlock(&in->lock);
  pthread_mutex_unlock(&madev->lock);
  return 0;
}

static int in_dump(const struct audio_stream *stream, int fd) { return 0; }

static int in_set_parameters(struct audio_stream *stream, const char *kvpairs) {
  struct mrvl_stream_in *in = (struct mrvl_stream_in *)stream;
  struct mrvl_audio_device *madev = in->dev;
  struct str_parms *parms;
  char *str;
  char value[32];
  int ret, val = 0;
  bool do_standby = false;
  ALOGI("%s: %s", __FUNCTION__, kvpairs);

  parms = str_parms_create_str(kvpairs);

  pthread_mutex_lock(&madev->lock);
  pthread_mutex_lock(&in->lock);
  ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_INPUT_SOURCE, value,
                          sizeof(value));
  if (ret >= 0) {
    val = atoi(value);
    // no audio source uses val == 0
    if ((in->source != val) && (val != 0)) {
      in->source = val;
    }
  }

  ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING, value,
                          sizeof(value));
  if (ret >= 0) {
    val = atoi(value);
    if ((val != 0) && ((unsigned int)val != AUDIO_DEVICE_IN_VOICE_CALL) &&
        (madev->in_device != (unsigned int)val) &&
        (!is_in_voip_vt(madev->mode))) {
      madev->in_device = val;
      select_input_device(madev);
    }
  }

  pthread_mutex_unlock(&in->lock);
  pthread_mutex_unlock(&madev->lock);

#ifdef WITH_ACOUSTIC
  if (madev->in_device & ~AUDIO_DEVICE_BIT_IN) {
    acoustic_manager_init(&(in->acoustic_manager), in->sample_rate,
                          popcount(in->channel_mask), madev->in_device);
  }
#endif

  str_parms_destroy(parms);
  return ret;
}

static char *in_get_parameters(const struct audio_stream *stream,
                               const char *keys) {
  struct mrvl_stream_in *in = (struct mrvl_stream_in *)stream;
  struct mrvl_audio_device *madev = (struct mrvl_audio_device *)in->dev;
  ALOGV("%s: %s", __FUNCTION__, keys);

  struct str_parms *param;
  char value[32];
  char *key;
  int ret = 0;
  char *ret_val = NULL;

  param = str_parms_create_str(keys);
  if (!param) return ret_val;

  key = AUDIO_PARAMETER_STREAM_ROUTING;
  if ((ret = str_parms_get_str(param, key, value, sizeof(value))) >= 0) {
    str_parms_add_int(param, key, (int)madev->in_device);
    ret_val = str_parms_to_str(param);
    ALOGV("%s: %s", __FUNCTION__, ret_val);
    str_parms_del(param, key);
  }

  str_parms_destroy(param);
  return ret_val;
}

static int in_add_audio_effect_fake(const struct audio_stream *stream,
                                    effect_handle_t effect) {
  return 0;
}

static int in_remove_audio_effect_fake(const struct audio_stream *stream,
                                       effect_handle_t effect) {
  return 0;
}

static int in_set_gain(struct audio_stream_in *stream, float gain) { return 0; }

static ssize_t in_read_vc_rec(struct audio_stream_in *stream, void *buffer,
                              size_t bytes) {
  int ret = 0;
  struct mrvl_stream_in *in = (struct mrvl_stream_in *)stream;

  // path operations should under protection of madev->lock
  // request madev->lock first to avoid deadlock
  pthread_mutex_lock(&in->lock);
  if (in->standby) {
#ifdef WITH_TELEPHONY
    ret = vcm_recording_start();
#endif
    if (ret != 0) {
      ALOGE("%s start input error. ret %d", __FUNCTION__, ret);
      goto exit;
    }
    in->standby = false;
  }

// read stream data from VCM
#ifdef WITH_TELEPHONY
  ret = vcm_recording_read(buffer, bytes);
#endif
  if (ret < 0) {
    ALOGE("%s read VC recording stream error, ret %d", __FUNCTION__, ret);
    goto exit;
  }

exit:
  pthread_mutex_unlock(&in->lock);
  if (ret < 0) {
    memset((unsigned char *)buffer, 0, bytes);
    usleep(bytes * 1000000 / audio_stream_frame_size(&stream->common) /
           in_get_sample_rate(&stream->common));
  }
  return bytes;
}

static ssize_t in_read_primary(struct audio_stream_in *stream, void *buffer,
                               size_t bytes) {
  int ret = 0;
  struct mrvl_stream_in *in = (struct mrvl_stream_in *)stream;
  struct mrvl_audio_device *madev = in->dev;

#ifdef MRVL_AEC
  if (is_in_voip_vt(madev->mode)) {
    uint32_t audio_profile = get_profile(madev);
    if (audio_profile != AUDIO_PROFILE_ID_ENUM_32_BIT) {
      in_load_effect((struct audio_stream *)stream,
                     (effect_uuid_t *)FX_IID_VOIPTX, audio_profile, true);
    }
  }
#endif

  // path operations should under protection of madev->lock
  // request madev->lock first to avoid deadlock
  pthread_mutex_lock(&madev->lock);
  pthread_mutex_lock(&in->lock);
  if (in->standby) {
    ret = start_input_stream(in);
    if (ret != 0) {
      ALOGE("%s start input error. ret %d", __FUNCTION__, ret);
      pthread_mutex_unlock(&madev->lock);
      goto exit;
    }
    in->standby = false;
  }

#ifdef MRVL_AEC
  if (madev->echo_reference == NULL) {
    ALOGI("%s: create echo reference for input stream %p", __FUNCTION__, in);
    create_echo_ref(in, madev->outputs[OUTPUT_LOW_LATENCY]);
  }
#endif
  pthread_mutex_unlock(&madev->lock);

  ret = pcm_read(in->handle, buffer, bytes);
  if (ret == -EPIPE) {
    ALOGW("%s: [OVERRUN] failed to read, reason: broken pipe", __FUNCTION__);
    ret = pcm_read(in->handle, buffer, bytes);
  }

  if (ret < 0) {
    ALOGE("%s: failed to read, reason: %s", __FUNCTION__,
          pcm_get_error(in->handle));
    goto exit;
  }

#ifdef AUDIO_DEBUG
  audio_debug_stream_dump(AH_TX, buffer, bytes);
#endif

  // ramp down the data before disable TX and ramp up the data after enable TX
  ramp_process(buffer, bytes);

#ifdef WITH_ACOUSTIC
  if ((!is_in_voip_vt(madev->mode)) &&
      (in->source != AUDIO_SOURCE_VOICE_RECOGNITION)) {
    int16_t *p = (int16_t *)buffer;
    size_t in_frames = bytes / audio_stream_frame_size(&in->stream.common);
    acoustic_manager_process(in->acoustic_manager, p, in_frames);
  }
#endif

#ifdef AUDIO_DEBUG
  audio_debug_stream_dump(AH_TX_AF_ACOUSTIC, buffer, bytes);
#endif

#ifdef MRVL_AEC
  if (is_in_voip_vt(madev->mode)) {
    in_pre_process(in, buffer);
  }
#endif

exit:
  pthread_mutex_unlock(&in->lock);
  if ((ret < 0) && (ret != -EPIPE)) {
    memset((unsigned char *)buffer, 0, bytes);
    usleep(bytes * 1000000 / audio_stream_frame_size(&stream->common) /
           in_get_sample_rate(&stream->common));
  }

  // clean the buffer if mic_mute is on and input is not for fm.
  if ((madev->mic_mute) && (madev->in_device != AUDIO_DEVICE_IN_FMRADIO)) {
    memset((unsigned char *)buffer, 0, bytes);
  }

  return bytes;
}

static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream) {
  return 0;
}

static int mrvl_hw_dev_open_output_stream(
    struct audio_hw_device *dev, audio_io_handle_t handle,
    audio_devices_t devices, audio_output_flags_t flags,
    struct audio_config *config, struct audio_stream_out **stream_out,
    const char *address) {
  struct mrvl_audio_device *madev = (struct mrvl_audio_device *)dev;
  struct mrvl_stream_out *out = NULL;
  int ret = 0;
  int output_type = 0;
  ALOGI("%s: device 0x%x", __FUNCTION__, devices);

  *stream_out = NULL;

  out = (struct mrvl_stream_out *)calloc(1, sizeof(struct mrvl_stream_out));
  if (!out) return -ENOMEM;

  out->channel_mask = AUDIO_CHANNEL_OUT_STEREO;
  out->sample_rate = SAMPLE_RATE_OUT_DEFAULT;
  out->format = AUDIO_FORMAT_PCM_16_BIT;

  if (flags & AUDIO_OUTPUT_FLAG_DEEP_BUFFER) {
    ALOGV("%s deep buffer", __FUNCTION__);
    if (madev->outputs[OUTPUT_DEEP_BUF] != NULL) {
      ret = -ENOSYS;
      goto err_open;
    }
    output_type = OUTPUT_DEEP_BUF;
    out->period_size = DEEP_BUFFER_LONG_PERIOD_SIZE;
    out->period_count = DEEP_BUFFER_LONG_PERIOD_COUNT;
    out->channel_mask = AUDIO_CHANNEL_OUT_STEREO;
    out->stream.common.get_buffer_size = out_get_buffer_size_deep_buffer;
    out->stream.get_latency = out_get_latency_deep_buffer;
    out->stream.write = out_write_deep_buffer;
  } else {
    ALOGV("%s low latency", __FUNCTION__);
    if (madev->outputs[OUTPUT_LOW_LATENCY] != NULL) {
      ret = -ENOSYS;
      goto err_open;
    }
    output_type = OUTPUT_LOW_LATENCY;
    out->period_size = LOW_LATENCY_OUTPUT_PERIOD_SIZE;
    out->period_count = LOW_LATENCY_OUTPUT_PERIOD_COUNT;
    out->stream.common.get_buffer_size = out_get_buffer_size_low_latency;
    out->stream.get_latency = out_get_latency_low_latency;
    out->stream.write = out_write_low_latency;
  }

  out->stream.common.standby = out_standby;
  out->stream.common.get_sample_rate = out_get_sample_rate;
  out->stream.common.set_sample_rate = out_set_sample_rate;
  out->stream.common.get_channels = out_get_channels;
  out->stream.common.get_format = out_get_format;
  out->stream.common.set_format = out_set_format;
  out->stream.common.dump = out_dump;
  out->stream.common.set_parameters = out_set_parameters;
  out->stream.common.get_parameters = out_get_parameters;
  out->stream.common.add_audio_effect = out_add_audio_effect_fake;
  out->stream.common.remove_audio_effect = out_remove_audio_effect_fake;
  out->stream.get_render_position = out_get_render_position;
  out->stream.get_presentation_position = out_get_presentation_position;
  out->stream.set_volume = out_set_volume;
  out->handle = NULL;
  out->standby = true;
  out->io_handle = handle;
  out->written = 0;

  config->format = out->stream.common.get_format(&out->stream.common);
  config->channel_mask = out->stream.common.get_channels(&out->stream.common);
  config->sample_rate = out->stream.common.get_sample_rate(&out->stream.common);

  pthread_mutex_init(&out->lock, NULL);
  out->dev = madev;
  *stream_out = &out->stream;

  pthread_mutex_lock(&madev->lock);
  madev->outputs[output_type] = out;
#ifdef ROUTE_SPEAKER_TO_HEADSET
  if (devices == AUDIO_DEVICE_OUT_SPEAKER)
    madev->out_device = AUDIO_DEVICE_OUT_WIRED_HEADSET;
  else
    madev->out_device = devices;
#else
  madev->out_device = devices;
#endif
  pthread_mutex_unlock(&madev->lock);

  list_init(&out->effect_interfaces);

#ifdef WITH_ACOUSTIC
  out->acoustic_manager = 0;
#endif

  ALOGI("%s: success. out %p", __FUNCTION__, out);

  return 0;

err_open:
  free(out);
  return ret;
}

static void mrvl_hw_dev_close_output_stream(struct audio_hw_device *dev,
                                            struct audio_stream_out *stream) {
  struct mrvl_audio_device *madev = (struct mrvl_audio_device *)dev;
  struct mrvl_stream_out *out = (struct mrvl_stream_out *)stream;

  ALOGI("%s: out %p", __FUNCTION__, out);

  // standby current outputs
  pthread_mutex_lock(&madev->lock);
  pthread_mutex_lock(&out->lock);

  do_output_standby(out);

#ifdef WITH_ACOUSTIC
  acoustic_manager_destroy(out->acoustic_manager);
  out->acoustic_manager = 0;
#endif

  pthread_mutex_unlock(&out->lock);
  pthread_mutex_unlock(&madev->lock);

  // clear mrvl dev outputs array
  if (out == madev->outputs[OUTPUT_LOW_LATENCY]) {
    madev->outputs[OUTPUT_LOW_LATENCY] = NULL;
  } else if (out == madev->outputs[OUTPUT_DEEP_BUF]) {
    madev->outputs[OUTPUT_DEEP_BUF] = NULL;
  }

  pthread_mutex_destroy(&out->lock);
  free(out);
}

static int mrvl_hw_dev_set_parameters(struct audio_hw_device *dev,
                                      const char *kvpairs) {
  struct mrvl_audio_device *madev = (struct mrvl_audio_device *)dev;
  struct str_parms *param = NULL;
  char value[32] = {0};
  int intvalue = 0;
  const char *key = NULL;
  int ret = 0;
  enum BT_STATUS_UPDATE bt_status_update = BT_UPDATE_NONE;
  bool new_sw_nrec_mode = true;
  int new_bt_headset_type = BT_NB;
  int is_reinit_audio = 0;

  ALOGI("%s: %s", __FUNCTION__, kvpairs);

  if (strlen(kvpairs) == 0) return -1;
  param = str_parms_create_str(kvpairs);
  if (!param) {
    ALOGW("%s: param create str is null!", __FUNCTION__);
    return -1;
  }
  pthread_mutex_lock(&madev->lock);

  // set TTY mode
  key = AUDIO_PARAMETER_KEY_TTY_MODE;
  if ((ret = str_parms_get_str(param, key, value, sizeof(value))) >= 0) {
    int tty_mode = TTY_MODE_OFF;

    if (strcmp(value, AUDIO_PARAMETER_VALUE_TTY_OFF) == 0)
      tty_mode = TTY_MODE_OFF;
    else if (strcmp(value, AUDIO_PARAMETER_VALUE_TTY_FULL) == 0)
      tty_mode = TTY_MODE_FULL;
    else if (strcmp(value, AUDIO_PARAMETER_VALUE_TTY_HCO) == 0)
      tty_mode = TTY_MODE_HCO;
    else if (strcmp(value, AUDIO_PARAMETER_VALUE_TTY_VCO) == 0)
      tty_mode = TTY_MODE_VCO;

    if (tty_mode != madev->tty_mode) {
      if (madev->mode != AUDIO_MODE_IN_CALL) {
        // Do NOT update TTY mode during voice call; user can't do it,
        // and it could occur only due to headset in/out. In case of headset
        // out/in during TTY call, the tty mode change should be ignored
        madev->tty_mode = tty_mode;
      } else {
        select_output_device(madev);
      }
    }
    str_parms_del(param, key);
  }

  // set BT nrec
  key = AUDIO_PARAMETER_KEY_BT_NREC;
  if ((ret = str_parms_get_str(param, key, value, sizeof(value))) >= 0) {
    if (strcmp(value, AUDIO_PARAMETER_VALUE_ON) == 0) {
      new_sw_nrec_mode = true;
      ALOGI("%s: SW_NREC on board is ON. NREC on BT Headset is unsupported",
            __FUNCTION__);
    } else {
      new_sw_nrec_mode = false;
      ALOGI("%s: SW_NREC on board is OFF. NREC on BT Headset is supported",
            __FUNCTION__);
    }
    if (madev->use_sw_nrec != new_sw_nrec_mode) {
      bt_status_update |= BT_UPDATE_NREC_MODE;
    }
    str_parms_del(param, key);
  }

  // set BT WB/NB mode
  key = AUDIO_PARAMETER_KEY_BT_SCO_WB;
  if ((ret = str_parms_get_str(param, key, value, sizeof(value))) >= 0) {
    new_bt_headset_type =
        strcmp(value, AUDIO_PARAMETER_VALUE_ON) == 0 ? BT_WB : BT_NB;
    if (new_bt_headset_type != madev->bt_headset_type) {
      bt_status_update |= BT_UPDATE_HEADSET_TYPE;
    }
  }

  if (bt_status_update != BT_UPDATE_NONE) {
    ALOGI(
        "%s: disable old sco device and virtual path before enabling new one.",
        __FUNCTION__);
    audio_devices_t bt_sco_out_device =
        madev->out_device & AUDIO_DEVICE_OUT_ALL_SCO;
    if (bt_sco_out_device) {
      madev->out_device &= ~bt_sco_out_device;
      select_output_device(madev);
    }
    if (bt_status_update & BT_UPDATE_NREC_MODE) {
      madev->use_sw_nrec = new_sw_nrec_mode;
      ALOGI("%s: update BT routing for NREC mode: %d", __FUNCTION__,
            new_sw_nrec_mode);
    }
    if (bt_status_update & BT_UPDATE_HEADSET_TYPE) {
      madev->bt_headset_type = new_bt_headset_type;
      ALOGI("%s: update BT routing for dev type: %s", __FUNCTION__,
            new_bt_headset_type == BT_WB ? "BT_WB" : "BT_NB");
    }
    ALOGI("%s: enable new sco device and virtual path", __FUNCTION__);
    madev->out_device |= bt_sco_out_device;
    select_output_device(madev);
  }

  // set extra volume
  key = EXTRA_VOL;
  if ((ret = str_parms_get_str(param, key, value, sizeof(value))) >= 0) {
    bool status = (strcmp(value, "true") == 0 ? true : false);
    if (madev->use_extra_vol != status) {
      ALOGI("Extra volume mode is %s.", status ? "on" : "off");
      if (madev->mode == AUDIO_MODE_IN_CALL) {
        unsigned int params = get_call_params(madev);
#ifdef WITH_TELEPHONY
        vcm_mute_all(true, get_cpmute_rampdown_level());
        vcm_select_path(madev->out_device, madev->in_device, params);
        set_voice_call_volume(madev, madev->voice_volume, status);
        vcm_mute_all(false, get_cpmute_rampup_level());
#endif
      }
      madev->use_extra_vol = status;
    }
  }

  // mute all rx sound
  key = MUTE_ALL_RX;
  if (str_parms_get_int(param, key, &intvalue) == 0) {
    if (mrvl_path_manager.mute_all_rx != (bool)intvalue) {
      // mute all RX: 1, unmute all Rx: 0
      mrvl_path_manager.mute_all_rx = (bool)intvalue;
      ALOGI("%s mute all Rx sound is %s.", __FUNCTION__,
            intvalue ? "true" : "false");
      if (mrvl_path_manager.mute_all_rx) {
        route_devices(madev, mrvl_path_manager.active_out_device,
                      METHOD_DISABLE, 0);
        select_virtual_path(madev);
      } else {
        if (mrvl_path_manager.active_out_device) {
          route_devices(madev, mrvl_path_manager.active_out_device,
                        METHOD_ENABLE, 0);
          select_virtual_path(madev);
        }
      }
    }
  }

  // set screen state
  key = AUDIO_PARAMETER_KEY_SCREEN_STATE;
  if ((ret = str_parms_get_str(param, key, value, sizeof(value))) >= 0) {
    ALOGI("%s: set screen state [%s].", __FUNCTION__, value);
    if (strcmp(value, AUDIO_PARAMETER_VALUE_ON) == 0) {
      madev->screen_off = false;
    } else {
      madev->screen_off = true;
    }
  }

  // set loopback paramters,loopback mode setting select cp/app/hardware
  // loopback type;
  // Input/output device setting select specific input/output device during
  // loopback;
  // headset flag paramters used for selecting left-headset,right-heaset or
  // stereo headset.
  if ((ret = str_parms_get_str(param, LOOPBACK, value, sizeof(value))) >= 0) {
    if (strcmp(value, "on") == 0) {
      if (str_parms_get_int(param, LOOPBACK_INPUT_DEVICE, &intvalue) >= 0) {
        intvalue = intvalue | AUDIO_DEVICE_BIT_IN;
        ALOGI("%s: input device 0x%x", __FUNCTION__, intvalue);
        madev->loopback_param.in_device = intvalue;
        str_parms_del(param, LOOPBACK_INPUT_DEVICE);
      }
      if (str_parms_get_int(param, LOOPBACK_OUTPUT_DEVICE, &intvalue) >= 0) {
        ALOGI("%s: output device 0x%x", __FUNCTION__, intvalue);
        madev->loopback_param.out_device = intvalue;
        str_parms_del(param, LOOPBACK_OUTPUT_DEVICE);
      }
      if (str_parms_get_int(param, LOOPBACK_HEADSET_FLAG, &intvalue) >= 0) {
        ALOGI("%s: headset flag %d", __FUNCTION__, intvalue);
        madev->loopback_param.headset_flag = intvalue;
        str_parms_del(param, LOOPBACK_HEADSET_FLAG);
      }
      if (str_parms_get_int(param, LOOPBACK_MODE_SETTTING, &intvalue) >= 0) {
        ALOGI("%s: loopback mode %d", __FUNCTION__, intvalue);
        madev->loopback_param.type = intvalue;
        str_parms_del(param, LOOPBACK_MODE_SETTTING);
      }
      select_loopback(madev, true);
    } else {
      select_loopback(madev, false);
    }
    str_parms_del(param, LOOPBACK);
  }

  // set voice call EQ parameters
  char eq_value[100];
  key = VOICE_USER_EQ;
  if ((str_parms_get_str(param, key, eq_value, sizeof(eq_value))) >= 0) {
#ifdef WITH_TELEPHONY
    vcm_set_user_eq(eq_value, sizeof(eq_value));
#endif
    str_parms_del(param, key);
  }

  // set microphone mode
  key = MICROPHONE_MODE;
  if (str_parms_get_int(param, key, &intvalue) == 0) {
    if ((unsigned int)intvalue != mrvl_path_manager.mic_mode) {
      ALOGD("%s: MIC User-Selection changed MIC to %d", __FUNCTION__, intvalue);
      mrvl_path_manager.mic_mode = intvalue;
      is_reinit_audio = 1;
    }
    str_parms_del(param, key);
  }

  pthread_mutex_unlock(&madev->lock);
  str_parms_destroy(param);

  if (is_reinit_audio) {
    // reinit the Audio platform config parser
    ALOGD(
        "%s: User-Selection changed MIC ==> reinit the Audio platform config "
        "parser",
        __FUNCTION__);
    init_platform_config();

    // during voice-call, if mic selection changed, switch the input device, and
    // update the VCM profile
    if (madev->mode == AUDIO_MODE_IN_CALL) {
      select_input_device(madev);  // update the input device
#ifdef WITH_TELEPHONY
      unsigned int params = get_call_params(madev);
      vcm_select_path(madev->out_device, madev->in_device,
                      params);  // update the VCM profile
#endif
    }
  }

  return 0;
}

static char *mrvl_hw_dev_get_parameters(const struct audio_hw_device *dev,
                                        const char *keys) {
  struct mrvl_audio_device *madev = (struct mrvl_audio_device *)dev;
  char *ret_val = NULL;
  char val_str[32];
  struct str_parms *param;
  const char *key;
  ALOGI("%s keys %s", __FUNCTION__, keys);

  param = str_parms_create_str(keys);
  if (!param) return ret_val;
  pthread_mutex_lock(&madev->lock);

  key = EXTRA_VOL;
  if (str_parms_get_str(param, key, val_str, sizeof(val_str)) >= 0) {
    if (snprintf(val_str, sizeof(val_str), "%s",
                 madev->use_extra_vol ? "true" : "false") >= 0) {
      ret_val = strdup(val_str);
    }
  }
  pthread_mutex_unlock(&madev->lock);
  str_parms_destroy(param);
  return ret_val;
}

static int mrvl_hw_dev_init_check(const struct audio_hw_device *dev) {
  ALOGI("%s", __FUNCTION__);
  return 0;
}

static int mrvl_hw_dev_set_voice_volume(struct audio_hw_device *dev,
                                        float volume) {
  struct mrvl_audio_device *madev = (struct mrvl_audio_device *)dev;
  ALOGI("%s volume %f", __FUNCTION__, volume);
  madev->voice_volume = volume;
  if (madev->mode == AUDIO_MODE_IN_CALL) {
    set_voice_call_volume(madev, volume, madev->use_extra_vol);
    // set volume value to ACM for volume calibration for voice call.
    set_hw_volume(V_MODE_VC, convert2_hwdev(madev, madev->out_device),
                  (unsigned char)(volume * 100));
  }
  return 0;
}

static int mrvl_hw_dev_set_master_volume(struct audio_hw_device *dev,
                                         float volume) {
  return -ENOSYS;
}

static int mrvl_hw_dev_set_mode(struct audio_hw_device *dev, int mode) {
  struct mrvl_audio_device *madev = (struct mrvl_audio_device *)dev;
  ALOGI("%s mode %d", __FUNCTION__, mode);

  if (mode < 0 || mode >= AUDIO_MODE_CNT) return -1;
  pthread_mutex_lock(&madev->lock);
  if (madev->mode != mode) {
    select_mode(madev, mode);
  }
  pthread_mutex_unlock(&madev->lock);
  return 0;
}

static int mrvl_hw_dev_set_mic_mute(struct audio_hw_device *dev, bool state) {
  struct mrvl_audio_device *madev = (struct mrvl_audio_device *)dev;
  ALOGI("%s: state %s, mic_mute %s", __FUNCTION__, state ? "mute" : "unmute",
        madev->mic_mute ? "mute" : "unmute");
  if (state != madev->mic_mute) {
#ifdef WITH_TELEPHONY
    unsigned char ramp_level = 0;
    if (state) {
      ramp_level = get_cpmute_rampdown_level();
    } else {
      ramp_level = get_cpmute_rampup_level();
    }
    vcm_mute_mic(state, ramp_level);
#endif
    madev->mic_mute = state;
  }
  return 0;
}

static int mrvl_hw_dev_get_mic_mute(const struct audio_hw_device *dev,
                                    bool *state) {
  struct mrvl_audio_device *madev = (struct mrvl_audio_device *)dev;
  ALOGD("%s: %s", __FUNCTION__, madev->mic_mute ? "mute" : "unmute");

  *state = madev->mic_mute;
  return 0;
}

static size_t mrvl_hw_dev_get_input_buffer_size(
    const struct audio_hw_device *dev, const struct audio_config *config) {
  int buffersize = 0;
  if (config->format != AUDIO_FORMAT_PCM_16_BIT) {
    ALOGW("%s: bad format: %d", __FUNCTION__, config->format);
    return 0;
  }

  if (config->sample_rate == SAMPLE_RATE_VC_RECORDING) {
    buffersize = VC_RECORDING_INPUT_PERIOD_SIZE;
  } else {
    buffersize = LOW_LATENCY_INPUT_PERIOD_SIZE;
  }

  return buffersize * sizeof(uint16_t) * popcount(config->channel_mask);
}

static int mrvl_hw_dev_open_input_stream(struct audio_hw_device *dev,
                                         audio_io_handle_t handle,
                                         audio_devices_t devices,
                                         struct audio_config *config,
                                         struct audio_stream_in **stream_in,
                                         audio_input_flags_t flags,
                                         const char *address,
                                         audio_source_t source) {
  struct mrvl_audio_device *madev = (struct mrvl_audio_device *)dev;
  struct mrvl_stream_in *in;
  int input_type = 0;

  if (config != NULL) {
    ALOGI("%s req sample rate %d channel mask 0x%x devices 0x%x", __FUNCTION__,
          config->sample_rate, config->channel_mask, devices);
  }

  *stream_in = NULL;

  in = (struct mrvl_stream_in *)calloc(1, sizeof(struct mrvl_stream_in));
  if (!in) {
    ALOGE("%s calloc error", __FUNCTION__);
    return -ENOMEM;
  }
  in->stream.common.get_sample_rate = in_get_sample_rate;
  in->stream.common.set_sample_rate = in_set_sample_rate;
  in->stream.common.get_buffer_size = in_get_buffer_size;
  in->stream.common.get_channels = in_get_channels;
  in->stream.common.get_format = in_get_format;
  in->stream.common.set_format = in_set_format;
  in->stream.common.dump = in_dump;
  in->stream.common.set_parameters = in_set_parameters;
  in->stream.common.get_parameters = in_get_parameters;
  in->stream.common.add_audio_effect = in_add_audio_effect_fake;
  in->stream.common.remove_audio_effect = in_remove_audio_effect_fake;
  in->stream.set_gain = in_set_gain;
  in->stream.get_input_frames_lost = in_get_input_frames_lost;

  in->format = ((config != NULL) ? config->format : AUDIO_FORMAT_PCM_16_BIT);
  if (devices & AUDIO_DEVICE_IN_VOICE_CALL & ~AUDIO_DEVICE_BIT_IN) {
    // phone plug-in only output 16K Mono stream
    in->sample_rate = SAMPLE_RATE_VC_RECORDING;
    in->period_size = VC_RECORDING_INPUT_PERIOD_SIZE;
    in->channel_mask = AUDIO_CHANNEL_IN_MONO;
    in->stream.read = in_read_vc_rec;
    in->stream.common.standby = in_standby_vc_rec;
    input_type = INPUT_VC;
  } else {
    in->sample_rate = SAMPLE_RATE_IN_DEFAULT;
    in->period_size = LOW_LATENCY_INPUT_PERIOD_SIZE;
    in->period_count = LOW_LATENCY_INPUT_PERIOD_COUNT;
    in->channel_mask = AUDIO_CHANNEL_IN_STEREO;
    in->stream.read = in_read_primary;
    in->stream.common.standby = in_standby_primary;
    input_type = INPUT_PRIMARY;
  }
  in->handle = NULL;
  in->standby = true;
  in->io_handle = handle;

  pthread_mutex_init(&in->lock, NULL);
  madev->in_device = devices;

  in->dev = madev;
  *stream_in = &in->stream;
  madev->inputs[input_type] = in;

  list_init(&in->effect_interfaces);

#ifdef WITH_ACOUSTIC
  in->acoustic_manager = 0;
#endif

  if (config != NULL) {
    config->format = in->stream.common.get_format(&in->stream.common);
    config->channel_mask = in->stream.common.get_channels(&in->stream.common);
    config->sample_rate = in->stream.common.get_sample_rate(&in->stream.common);
  }

  ALOGI("%s: success. in %p", __FUNCTION__, in);
  return 0;
}

static void mrvl_hw_dev_close_input_stream(struct audio_hw_device *dev,
                                           struct audio_stream_in *stream) {
  struct mrvl_audio_device *madev = (struct mrvl_audio_device *)dev;
  struct mrvl_stream_in *in = (struct mrvl_stream_in *)stream;
  ALOGI("%s", __FUNCTION__);

  pthread_mutex_lock(&madev->lock);
  pthread_mutex_lock(&in->lock);

  do_input_standby(in);

#ifdef WITH_ACOUSTIC
  acoustic_manager_destroy(in->acoustic_manager);
  in->acoustic_manager = 0;
#endif

  pthread_mutex_unlock(&in->lock);
  pthread_mutex_unlock(&madev->lock);

  // clear mrvl dev inputs array
  if (in == madev->inputs[INPUT_PRIMARY]) {
    madev->inputs[INPUT_PRIMARY] = NULL;
  } else if (in == madev->inputs[INPUT_VC]) {
    madev->inputs[INPUT_VC] = NULL;
  }

  pthread_mutex_destroy(&in->lock);
  free(stream);
}

static int mrvl_hw_dev_dump(const audio_hw_device_t *device, int fd) {
  const size_t SIZE = 256;
  char buffer[SIZE];
  char result[SIZE];
  snprintf(buffer, SIZE, "dump()");
  strcpy(result, buffer);
  write(fd, result, strlen(result));
  return 0;
}

static int mrvl_hw_dev_create_audio_patch(
    struct audio_hw_device *dev, unsigned int num_sources,
    const struct audio_port_config *sources, unsigned int num_sinks,
    const struct audio_port_config *sinks, audio_patch_handle_t *handle) {
  ALOGI("%s: num_sources %d num_sinks %d handle %d", __FUNCTION__, num_sources,
        num_sinks, *handle);
  struct mrvl_audio_device *madev = (struct mrvl_audio_device *)dev;
  bool do_standby = false;
  unsigned int i = 0;
  audio_devices_t in_dev = 0;
  audio_devices_t out_dev = 0;
  audio_source_t in_src = 0;

  // limit number of sources to 1 for now
  if (num_sources == 0 || num_sources > 1 || num_sinks == 0 ||
      num_sinks > AUDIO_PATCH_PORTS_MAX) {
    return -1;
  }

  pthread_mutex_lock(&madev->lock);

  switch (sources[0].type) {
    case AUDIO_PORT_TYPE_DEVICE: {
      struct mrvl_stream_in *in;
      // limit number of sinks to 1 for now
      if (num_sinks > 1) {
        pthread_mutex_unlock(&madev->lock);
        return -1;
      }
      if (sinks[0].type == AUDIO_PORT_TYPE_MIX) {
        for (i = 0; i < INPUT_TOTAL; i++) {
          in = madev->inputs[i];
          if (in && sinks[0].ext.mix.handle == in->io_handle) {
            in_dev = sources[0].ext.device.type;
            in_src = sinks[0].ext.mix.usecase.source;
            // no audio source uses value 0
            if ((in->source != (int)in_src) && (in_src != 0)) {
              in->source = in_src;
            }
            if ((madev->in_device != in_dev) && (in_dev != 0) &&
                (in_dev != AUDIO_DEVICE_IN_VOICE_CALL) &&
                (!is_in_voip_vt(madev->mode))) {
              madev->in_device = in_dev;
              select_input_device(madev);
            }

#ifdef WITH_ACOUSTIC
            if ((madev->in_device & ~AUDIO_DEVICE_BIT_IN) &&
                (in->source != AUDIO_SOURCE_VOICE_RECOGNITION)) {
              acoustic_manager_init(&(in->acoustic_manager), in->sample_rate,
                                    popcount(in->channel_mask),
                                    madev->in_device);
            }
#endif
          }
        }
      } else {
        // TODO: device to device patch
      }
    } break;
    case AUDIO_PORT_TYPE_MIX: {
      struct mrvl_stream_out *out;
      // limit to connections between devices and output streams
      for (i = 0; i < num_sinks; i++) {
        if (sinks[i].type != AUDIO_PORT_TYPE_DEVICE) {
          ALOGW("%s: invalid sink type %d for bus source", __FUNCTION__,
                sinks[i].type);
          pthread_mutex_unlock(&madev->lock);
          return -1;
        }
      }
      for (i = 0; i < OUTPUT_TOTAL; i++) {
        out = madev->outputs[i];
        if (out && sources[0].ext.mix.handle == out->io_handle) {
          for (i = 0; i < num_sinks; i++) {
            out_dev |= sinks[i].ext.device.type;
          }
#ifdef ROUTE_SPEAKER_TO_HEADSET
          if (out_dev == AUDIO_DEVICE_OUT_SPEAKER){
            ALOGI("%s: convert Speaker audio source to Headset audio source", __FUNCTION__);
            out_dev = AUDIO_DEVICE_OUT_WIRED_HEADSET;
          }
#endif
          madev->out_device = out_dev;

          select_output_device(madev);
          // set volume value to ACM for volume calibration for voice call.
          if (madev->mode == AUDIO_MODE_IN_CALL) {
            set_hw_volume(V_MODE_VC, convert2_hwdev(madev, madev->out_device),
                          madev->voice_volume * 100);
          }
#ifdef MRVL_AEC
          if (out == madev->outputs[OUTPUT_LOW_LATENCY] &&
              is_in_voip_vt(madev->mode)) {
            uint32_t audio_profile = get_profile(madev);
            if (audio_profile != AUDIO_PROFILE_ID_ENUM_32_BIT) {
              effect_set_profile(audio_profile, madev);
            }
          }
#endif
#ifdef WITH_ACOUSTIC
          if (madev->out_device) {
            acoustic_manager_init(&(out->acoustic_manager), out->sample_rate,
                                  popcount(out->channel_mask),
                                  madev->out_device);
          }
#endif
        }
      }
    } break;
    default:
      ALOGW("%s: invalid source type %d ", __FUNCTION__, sources[0].type);
      pthread_mutex_unlock(&madev->lock);
      return 0;
  }

  struct mrvl_audio_patch *m_patch = calloc(1, sizeof(struct mrvl_audio_patch));
  if (m_patch == NULL) {
    ALOGE("%s, memory allocation fail!", __FUNCTION__);
    pthread_mutex_unlock(&madev->lock);
    return -1;
  }
  *handle = (int)android_atomic_inc(&madev->unique_id);
  m_patch->patch_handle = *handle;
  m_patch->num_sources = num_sources;
  for (i = 0; i < num_sources; i++) {
    m_patch->sources[i] = sources[i];
  }
  m_patch->num_sinks = num_sinks;
  for (i = 0; i < num_sinks; i++) {
    m_patch->sinks[i] = sinks[i];
  }
  list_add_tail(&madev->audio_patches, &m_patch->link);

  pthread_mutex_unlock(&madev->lock);
  return 0;
}

static int mrvl_hw_dev_release_audio_patch(struct audio_hw_device *dev,
                                           audio_patch_handle_t handle) {
  ALOGI("%s: audio patch handle is %d", __FUNCTION__, handle);
  struct mrvl_audio_device *madev = (struct mrvl_audio_device *)dev;
  bool do_standby = false;
  unsigned int i = 0;
  audio_devices_t in_dev = 0;
  audio_devices_t out_dev = 0;

  struct listnode *plist = NULL;
  struct listnode *tmp_node = NULL;
  struct mrvl_audio_patch *tmp_patch = NULL;

  pthread_mutex_lock(&madev->lock);

  list_for_each_safe(plist, tmp_node, &madev->audio_patches) {
    tmp_patch = node_to_item(plist, struct mrvl_audio_patch, link);
    if (tmp_patch != NULL && tmp_patch->patch_handle == handle) {
      switch (tmp_patch->sources[0].type) {
        case AUDIO_PORT_TYPE_DEVICE: {
          struct mrvl_stream_in *in;
          if (tmp_patch->sinks[0].type == AUDIO_PORT_TYPE_MIX) {
            for (i = 0; i < INPUT_TOTAL; i++) {
              in = madev->inputs[i];
              if (in && tmp_patch->sinks[0].ext.mix.handle == in->io_handle) {
                // current do nothing and let in_standby to disable path
              }
            }
          } else {
            // TODO: device to device patch
          }
        } break;
        case AUDIO_PORT_TYPE_MIX: {
          struct mrvl_stream_out *out;
          for (i = 0; i < OUTPUT_TOTAL; i++) {
            out = madev->outputs[i];
            if (out && tmp_patch->sources[0].ext.mix.handle == out->io_handle) {
              // current do nothing and let out_standby to disable path
            }
          }
        } break;
        default:
          ALOGW("%s: invalid source type %d ", __FUNCTION__,
                tmp_patch->sources[0].type);
          pthread_mutex_unlock(&madev->lock);
          return 0;
      }
      list_remove(&tmp_patch->link);
      free(tmp_patch);
      tmp_patch = NULL;
    }
  }

  pthread_mutex_unlock(&madev->lock);
  return 0;
}

static int mrvl_hw_dev_get_audio_port(struct audio_hw_device *dev,
                                      struct audio_port *port) {
  ALOGI("%s", __FUNCTION__);
  return 0;
}

static int mrvl_hw_dev_set_audio_port_config(
    struct audio_hw_device *dev, const struct audio_port_config *config) {
  ALOGI("%s", __FUNCTION__);
  return 0;
}

static int mrvl_hw_dev_close(hw_device_t *device) {
  struct mrvl_audio_device *madev = (struct mrvl_audio_device *)device;

  // Deinit acm module
  ACMDeInit();

  // clean path manager
  struct listnode *plist = NULL;
  struct listnode *tmp_node = NULL;
  struct virtual_path *tmp_vpath = NULL;
  list_for_each_safe(plist, tmp_node, &mrvl_path_manager.out_virtual_path) {
    tmp_vpath = node_to_item(plist, struct virtual_path, link);
    if (tmp_vpath != NULL) {
      list_remove(&tmp_vpath->link);
      free(tmp_vpath);
      tmp_vpath = NULL;
    }
  }
  list_for_each_safe(plist, tmp_node, &mrvl_path_manager.in_virtual_path) {
    tmp_vpath = node_to_item(plist, struct virtual_path, link);
    if (tmp_vpath != NULL) {
      list_remove(&tmp_vpath->link);
      free(tmp_vpath);
      tmp_vpath = NULL;
    }
  }

  deinit_platform_config();

  pthread_mutex_destroy(&madev->lock);
  free(madev);

  return 0;
}

static uint32_t mrvl_hw_dev_get_supported_devices(
    const struct audio_hw_device *dev) {
  return (  // OUT
      AUDIO_DEVICE_OUT_EARPIECE | AUDIO_DEVICE_OUT_SPEAKER |
      AUDIO_DEVICE_OUT_WIRED_HEADSET | AUDIO_DEVICE_OUT_WIRED_HEADPHONE |
      AUDIO_DEVICE_OUT_AUX_DIGITAL | AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET |
      AUDIO_DEVICE_OUT_ALL_SCO | AUDIO_DEVICE_OUT_DEFAULT |
      // IN
      AUDIO_DEVICE_IN_COMMUNICATION | AUDIO_DEVICE_IN_AMBIENT |
      AUDIO_DEVICE_IN_BUILTIN_MIC | AUDIO_DEVICE_IN_WIRED_HEADSET |
      AUDIO_DEVICE_IN_AUX_DIGITAL | AUDIO_DEVICE_IN_VOICE_CALL |
      AUDIO_DEVICE_IN_BACK_MIC | AUDIO_DEVICE_IN_VT_MIC |
      AUDIO_DEVICE_IN_FMRADIO | AUDIO_DEVICE_IN_ALL_SCO |
      AUDIO_DEVICE_IN_DEFAULT);
}

static int mrvl_hw_dev_open(const hw_module_t *module, const char *name,
                            hw_device_t **device) {
  struct mrvl_audio_device *madev = NULL;
  int ap_mode = 0;
  int mDev = 0;
  char value[256];
  ALOGI("%s name %s", __FUNCTION__, name);

  if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0) return -EINVAL;

  madev =
      (struct mrvl_audio_device *)calloc(1, sizeof(struct mrvl_audio_device));
  if (!madev) return -ENOMEM;

  madev->device.common.tag = HARDWARE_DEVICE_TAG;
  madev->device.common.version = AUDIO_DEVICE_API_VERSION_CURRENT;
  madev->device.common.module = (struct hw_module_t *)module;
  madev->device.common.close = mrvl_hw_dev_close;

  madev->device.get_supported_devices = mrvl_hw_dev_get_supported_devices;
  madev->device.init_check = mrvl_hw_dev_init_check;
  madev->device.set_voice_volume = mrvl_hw_dev_set_voice_volume;
  madev->device.set_master_volume = mrvl_hw_dev_set_master_volume;
  madev->device.set_mode = mrvl_hw_dev_set_mode;
  madev->device.set_mic_mute = mrvl_hw_dev_set_mic_mute;
  madev->device.get_mic_mute = mrvl_hw_dev_get_mic_mute;
  madev->device.set_parameters = mrvl_hw_dev_set_parameters;
  madev->device.get_parameters = mrvl_hw_dev_get_parameters;
  madev->device.get_input_buffer_size = mrvl_hw_dev_get_input_buffer_size;
  madev->device.open_output_stream = mrvl_hw_dev_open_output_stream;
  madev->device.close_output_stream = mrvl_hw_dev_close_output_stream;
  madev->device.open_input_stream = mrvl_hw_dev_open_input_stream;
  madev->device.close_input_stream = mrvl_hw_dev_close_input_stream;
  madev->device.dump = mrvl_hw_dev_dump;
  madev->device.create_audio_patch = mrvl_hw_dev_create_audio_patch;
  madev->device.release_audio_patch = mrvl_hw_dev_release_audio_patch;
  madev->device.get_audio_port = mrvl_hw_dev_get_audio_port;
  madev->device.set_audio_port_config = mrvl_hw_dev_set_audio_port_config;

  pthread_mutex_init(&madev->lock, NULL);
  madev->mode = AUDIO_MODE_NORMAL;
  madev->voice_volume = 0.9f;
  madev->fm_volume = 0;
  madev->out_device = AUDIO_DEVICE_OUT_SPEAKER;
  madev->in_device = AUDIO_DEVICE_IN_BUILTIN_MIC;
  madev->fm_device = 0;
  strcpy(madev->plug_type, "plug:phone");
  madev->in_call = false;
  madev->in_fm = false;
  madev->in_hfp = false;
  madev->mic_mute = false;
  madev->tty_mode = (int)TTY_MODE_OFF;

  madev->use_sw_nrec = true;
  madev->screen_off = true;

  madev->phone_dl_handle = NULL;
  madev->phone_ul_handle = NULL;

  madev->hfp_out_handle = NULL;
  madev->hfp_in_handle = NULL;

  madev->bt_headset_type = BT_NB;
  madev->use_extra_vol = false;
  madev->use_voice_recognition = false;
  madev->unique_id = 1;

  madev->loopback_param.on = false;
  madev->loopback_param.in_device = AUDIO_DEVICE_IN_BUILTIN_MIC;
  madev->loopback_param.out_device = AUDIO_DEVICE_OUT_EARPIECE;
  madev->loopback_param.headset_flag = STEREO_HEADSET;
  madev->loopback_param.type = CP_LOOPBACK;

  // initialize mrvl path manager
  list_init(&mrvl_path_manager.in_virtual_path);
  list_init(&mrvl_path_manager.out_virtual_path);

  mrvl_path_manager.mic_mode = MIC_MODE_NONE;

  list_init(&madev->audio_patches);

#ifdef WITH_ACOUSTIC
  acoustic_manager_load_config();
#endif

  // Init acm module
  ACMInit();
  init_platform_config();

  *device = &madev->device.common;

  return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = mrvl_hw_dev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common =
        {
         .tag = HARDWARE_MODULE_TAG,
         .version_major = 1,
         .version_minor = 0,
         .id = AUDIO_HARDWARE_MODULE_ID,
         .name = "Marvll audio HW HAL",
         .author = "Marvell APSE/SE1-Audio",
         .methods = &hal_module_methods,
        },
};
