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

#ifndef __AUDIO_HW_MRVL_H__
#define __AUDIO_HW_MRVL_H__

#include <hardware/audio.h>
#include <system/audio.h>
#include <echo_reference.h>
#include <cutils/list.h>
#include <cutils/atomic.h>
#include <pthread.h>
#include <tinyalsa/asoundlib.h>

#include "audio_path.h"
#include "audio_profile.h"

enum bt_headset_type {
  BT_NB,
  BT_WB,
};

enum loopback_type {
  CP_LOOPBACK = 1,
  HARDWARE_LOOPBACK,
  APP_LOOPBACK,
};

enum headset_type {
  STEREO_HEADSET,
  MONO_HEADSET_L,
  MONO_HEADSET_R,
};

//#define STREAM_DUMP_DEBUG
#define PARAM_SIZE 20

#define ALSA_CARD_DEFAULT 0

#define ALSA_DEVICE_DEFAULT 0
#ifdef WITH_MAP_LITE
#define ALSA_DEVICE_VOICE 1
#define ALSA_DEVICE_FM 2
#define ALSA_DEVICE_HFP 3
// no this device on MAP-Lite,
// keep it as invalid value to compile successfully.
#define ALSA_DEVICE_DEEP_BUFFER 0xFF
#else
#define ALSA_DEVICE_DEEP_BUFFER 1
#define ALSA_DEVICE_VOICE 2
#define ALSA_DEVICE_FM 3
#define ALSA_DEVICE_HFP 4
#endif

#define SAMPLE_RATE_OUT_DEFAULT 44100
#define SAMPLE_RATE_IN_DEFAULT 44100
#define SAMPLE_RATE_PHONE 16000
#define SAMPLE_RATE_VC_RECORDING 16000
#define SAMPLE_RATE_OUT_FM 48000
#define SAMPLE_RATE_HFP_WB 16000
#define SAMPLE_RATE_HFP_NB 8000

// definition of output/input period and buffer size in frame counts
// must be multiple of 16 !!!
#ifdef WITH_MAP_LITE
#define LOW_LATENCY_OUTPUT_PERIOD_SIZE 1024
#define LOW_LATENCY_OUTPUT_PERIOD_COUNT 3
#else
#define LOW_LATENCY_OUTPUT_PERIOD_SIZE 512
#define LOW_LATENCY_OUTPUT_PERIOD_COUNT 3
#endif

#define DEEP_BUFFER_SHORT_PERIOD_SIZE 1024
#define DEEP_BUFFER_SHORT_PERIOD_COUNT 3

#define DEEP_BUFFER_LONG_PERIOD_SIZE 4096
#define DEEP_BUFFER_LONG_PERIOD_COUNT 2

#define LOW_LATENCY_INPUT_PERIOD_SIZE 512
#define LOW_LATENCY_INPUT_PERIOD_COUNT 4

#define PHONE_OUTPUT_PERIOD_SIZE 320
#define PHONE_OUTPUT_PERIOD_COUNT 2

#define FM_OUTPUT_PERIOD_SIZE 160
#define FM_OUTPUT_PERIOD_COUNT 2

#define HFP_OUTPUT_PERIOD_SIZE 160
#define HFP_OUTPUT_PERIOD_COUNT 2

#define VC_RECORDING_INPUT_PERIOD_SIZE 320

// minimum sleep time in out_write() when write threshold is not reached
#define MIN_WRITE_SLEEP_US 5000

#ifndef WITH_ADVANCED_AUDIO
#define AUDIO_PARAMETER_STREAM_HW_VOLUME "hardware_volume"  // uint32_t
#define AUDIO_PARAMETER_FM_STATUS "FM_STATUS"
#define AUDIO_PARAMETER_FM_DEVICE "FM_DEVICE"
#define AUDIO_PARAMETER_FM_VOLUME "FM_VOLUME"
typedef enum { FM_DISABLE = 0, FM_ENABLE, FM_SETVOLUME } fm_status;

#define AUDIO_STREAM_FM AUDIO_STREAM_DEFAULT
#define AUDIO_SOURCE_FMRADIO AUDIO_SOURCE_DEFAULT
#define AUDIO_SOURCE_VOICE_VT_CALL AUDIO_SOURCE_DEFAULT
#define AUDIO_DEVICE_OUT_FM_HEADPHONE AUDIO_DEVICE_OUT_DEFAULT
#define AUDIO_DEVICE_OUT_FM_SPEAKER (AUDIO_DEVICE_OUT_DEFAULT + 1)
#define AUDIO_DEVICE_IN_FMRADIO AUDIO_DEVICE_IN_DEFAULT
#define AUDIO_DEVICE_IN_VT_MIC AUDIO_DEVICE_IN_DEFAULT
#define AUDIO_MODE_IN_VT_CALL AUDIO_MODE_CURRENT
#endif

#define VCM_EXTRA_VOL       0x00000001
#define VCM_BT_NREC_OFF     0x00000002
#define VCM_BT_WB           0x00000004
#define VCM_TTY_FULL        0x00000008
#define VCM_TTY_HCO         0x00000010
#define VCM_TTY_VCO         0x00000020
#define VCM_TTY_VCO_DUALMIC 0x00000040
#define VCM_DUAL_MIC        0x00000080

/* TTY mode selection */
enum tty_modes { TTY_MODE_OFF = 0, TTY_MODE_FULL, TTY_MODE_HCO, TTY_MODE_VCO };

static char *const EXTRA_VOL = "Extra_volume";
static char *const MUTE_ALL_RX = "mute_voice_Rx";
static char *const VOICE_USER_EQ = "dha";
static char *const MICROPHONE_MODE = "microphone_mode";
static char *const LOOPBACK = "Loopback";
static char *const LOOPBACK_INPUT_DEVICE = "input_dev";
static char *const LOOPBACK_OUTPUT_DEVICE = "output_dev";
static char *const LOOPBACK_HEADSET_FLAG = "hs_flag";
static char *const LOOPBACK_MODE_SETTTING = "loopback_mode";

enum output_type {
  OUTPUT_LOW_LATENCY,  // low latency output stream
  OUTPUT_DEEP_BUF,  // deep PCM buffers output stream
  OUTPUT_HDMI,
  OUTPUT_TOTAL
};

enum input_type {
  INPUT_PRIMARY,  // primary input stream
  INPUT_VC,  // voice call recording input stream
  INPUT_TOTAL
};

struct mrvl_audio_patch {
  audio_patch_handle_t patch_handle;
  unsigned int num_sources;
  struct audio_port_config sources[AUDIO_PATCH_PORTS_MAX];
  unsigned int num_sinks;
  struct audio_port_config sinks[AUDIO_PATCH_PORTS_MAX];
  struct listnode link;
};

struct virtual_mode {
  virtual_mode_t v_mode;
  bool is_priority_highest;  // whether virtual mode priority is the highest
  struct listnode link;
};

struct virtual_path {
  virtual_mode_t v_mode;
  bool is_priority_highest;  // whether virtual path priority is the highest
  int path_device;           // can be input(without mask) or output device
  struct listnode link;
};

struct mrvl_path_status {
  bool itf_state[ID_IPATH_TX_MAX + 1];  // output&input interface state
  bool mute_all_rx;   // all Rx sound should be muted
  uint32_t mic_mode;  // mic mode in marvell settings
  uint32_t active_out_device;
  uint32_t active_in_device;
  uint32_t enabled_in_hwdev;
  struct listnode out_virtual_path;
  struct listnode in_virtual_path;
};

struct mrvl_stream_out {
  struct audio_stream_out stream;
  struct mrvl_audio_device *dev;
  audio_format_t format;
  audio_channel_mask_t channel_mask;
  uint32_t sample_rate;
  pthread_mutex_t lock;
  struct pcm *handle;
  unsigned int period_size;  // frame counts
  unsigned int period_count;  // counts of period
  unsigned int written;  // total frames written
  bool standby;
  int write_threshold;
  bool use_long_periods;
  struct listnode effect_interfaces;
  audio_io_handle_t io_handle;

#ifdef WITH_ACOUSTIC
  int acoustic_manager;
#endif
};

struct mrvl_stream_in {
  struct audio_stream_in stream;
  struct mrvl_audio_device *dev;
  uint32_t format;
  audio_channel_mask_t channel_mask;
  uint32_t sample_rate;
  int source;
  pthread_mutex_t lock;
  struct pcm *handle;
  unsigned int period_size;  // frame counts
  unsigned int period_count;  // counts of period
  bool standby;
  struct listnode effect_interfaces;
  struct echo_reference_buffer ref_buffer;
  audio_io_handle_t io_handle;

#ifdef WITH_ACOUSTIC
  int acoustic_manager;
#endif
};

struct mrvl_loopback_param {
  uint32_t headset_flag;  // record headset type of  mono and stereo
  uint32_t in_device;
  uint32_t out_device;
  uint32_t type;
  bool on;
};

struct mrvl_audio_device {
  struct audio_hw_device device;
  struct mrvl_stream_in *active_input;
  struct mrvl_stream_in *inputs[INPUT_TOTAL];
  struct mrvl_stream_out *outputs[OUTPUT_TOTAL];
  uint32_t out_device;
  uint32_t in_device;
  uint32_t fm_device;
  struct mrvl_loopback_param loopback_param;
  bool mic_mute;
  int tty_mode;
  pthread_mutex_t lock;
  float voice_volume;
  int fm_volume;
  int hfp_volume;
  audio_mode_t mode;
  char plug_type[256];
  bool in_call;  // record voice call status
  bool in_fm;    // record FM status
  bool in_hfp;   // record HFP status
  bool use_sw_nrec;
  bool screen_off;
  int bt_headset_type;
  int unique_id;
  bool use_extra_vol;
  bool use_voice_recognition;
  struct echo_reference_itfe *echo_reference;
  struct pcm *phone_dl_handle;
  struct pcm *phone_ul_handle;
  struct pcm *fm_handle;
  struct pcm *hfp_out_handle;
  struct pcm *hfp_in_handle;
  struct listnode audio_patches;
};

#endif /* __AUDIO_HW_MRVL_H__ */
