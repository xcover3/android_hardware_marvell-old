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

#ifndef __AUDIO_PATH_MRVL_H__
#define __AUDIO_PATH_MRVL_H__

/*********************************************************
 * definition of Audio Path info for ACM
 *********************************************************/
#define PATH_NAME(e) #e

// Audio path operation methods
#define INVALID_METHOD_ID 0xFFFFFFFF
#define METHOD_ENABLE 0x00000001
#define METHOD_DISABLE 0x00000002
#define METHOD_MUTE 0x00000003
#define METHOD_VOLUME_SET 0x00000004
#define METHOD_SWITCH 0x00000005

// Audio Hardware devices, relative with APU and codec paths
// Output Device must be defined by order, 1 bit 1 step
#define HWDEV_INVALID 0xFFFFFFFF
#define HWDEV_OUT_BASE 0x00000001
#define HWDEV_EARPIECE (HWDEV_OUT_BASE << 0)
#define HWDEV_SPEAKER (HWDEV_OUT_BASE << 1)
#define HWDEV_STEREOSPEAKER (HWDEV_OUT_BASE << 2)
#define HWDEV_HEADPHONE (HWDEV_OUT_BASE << 3)
#define HWDEV_BLUETOOTH_NB (HWDEV_OUT_BASE << 4)
#define HWDEV_BT_NREC_OFF_NB (HWDEV_OUT_BASE << 5)
#define HWDEV_BLUETOOTH_WB (HWDEV_OUT_BASE << 6)
#define HWDEV_BT_NREC_OFF_WB (HWDEV_OUT_BASE << 7)
#define HWDEV_OUT_TTY (HWDEV_OUT_BASE << 8)
#define HWDEV_OUT_TTY_HCO_SPEAKER (HWDEV_OUT_BASE << 9)
#define HWDEV_OUT_TTY_HCO_STEREOSPEAKER (HWDEV_OUT_BASE << 10)

// Input Device must be defined by order, 1 bit 1 step
#define HWDEV_BIT_IN 0x80000000
#define HWDEV_IN_BASE 0x00000001
#define HWDEV_AMIC1 (HWDEV_BIT_IN | (HWDEV_IN_BASE << 0))
#define HWDEV_AMIC2 (HWDEV_BIT_IN | (HWDEV_IN_BASE << 1))
#define HWDEV_DMIC1 (HWDEV_BIT_IN | (HWDEV_IN_BASE << 2))
#define HWDEV_DMIC2 (HWDEV_BIT_IN | (HWDEV_IN_BASE << 3))
#define HWDEV_AMIC1_SPK_MODE (HWDEV_BIT_IN | (HWDEV_IN_BASE << 4))
#define HWDEV_AMIC1_HP_MODE (HWDEV_BIT_IN | (HWDEV_IN_BASE << 5))
#define HWDEV_AMIC2_SPK_MODE (HWDEV_BIT_IN | (HWDEV_IN_BASE << 6))
#define HWDEV_AMIC2_HP_MODE (HWDEV_BIT_IN | (HWDEV_IN_BASE << 7))
#define HWDEV_DUAL_DMIC1 (HWDEV_BIT_IN | (HWDEV_IN_BASE << 8))
#define HWDEV_HSMIC (HWDEV_BIT_IN | (HWDEV_IN_BASE << 9))
#define HWDEV_BTMIC_NB (HWDEV_BIT_IN | (HWDEV_IN_BASE << 10))
#define HWDEV_BTMIC_NREC_OFF_NB (HWDEV_BIT_IN | (HWDEV_IN_BASE << 11))
#define HWDEV_BTMIC_WB (HWDEV_BIT_IN | (HWDEV_IN_BASE << 12))
#define HWDEV_BTMIC_NREC_OFF_WB (HWDEV_BIT_IN | (HWDEV_IN_BASE << 13))
#define HWDEV_DUAL_AMIC (HWDEV_BIT_IN | (HWDEV_IN_BASE << 14))
#define HWDEV_DUAL_AMIC_SPK_MODE (HWDEV_BIT_IN | (HWDEV_IN_BASE << 15))
#define HWDEV_DUAL_AMIC_HP_MODE (HWDEV_BIT_IN | (HWDEV_IN_BASE << 16))
#define HWDEV_IN_TTY (HWDEV_BIT_IN | (HWDEV_IN_BASE << 17))
#define HWDEV_IN_TTY_VCO (HWDEV_BIT_IN | (HWDEV_IN_BASE << 18))
#define HWDEV_IN_TTY_VCO_AMIC1 (HWDEV_BIT_IN | (HWDEV_IN_BASE << 19))
#define HWDEV_IN_TTY_VCO_AMIC2 (HWDEV_BIT_IN | (HWDEV_IN_BASE << 20))
#define HWDEV_IN_TTY_VCO_DUAL_AMIC (HWDEV_BIT_IN | (HWDEV_IN_BASE << 21))
#define HWDEV_IN_TTY_VCO_DUAL_AMIC_SPK_MODE \
  (HWDEV_BIT_IN | (HWDEV_IN_BASE << 22))
#define HWDEV_IN_TTY_VCO_DUAL_DMIC1 (HWDEV_BIT_IN | (HWDEV_IN_BASE << 23))

// Mic Name, used to parse platform config xml
static char *input_devname[] = {
    "AMIC1", "AMIC2", "DMIC1", "DMIC2", "AMIC1_SPK_MODE", "AMIC1_HP_MODE",
    "AMIC2_SPK_MODE", "AMIC2_HP_MODE", "DUAL_DMIC1", "HSMIC", "BTMIC_NB",
    "BTMIC_NREC_OFF_NB", "BTMIC_WB", "BTMIC_NREC_OFF_WB", "DUAL_AMIC",
    "DUAL_AMIC_SPK_MODE", "DUAL_AMIC_HP_MODE"};

/* mic mode in marvell settings */
enum mic_modes {
  MIC_MODE_NONE = 0,
  MIC_MODE_MIC1,
  MIC_MODE_MIC2,
  MIC_MODE_DUALMIC
};

/* mic mode to dev name conversion table */
static const char *mic_mode_to_dev_name[] = {
    "\0",       /*MIC_MODE_NONE=0*/
    "AMIC1",    /*MIC_MODE_MIC1=1*/
    "AMIC2",    /*MIC_MODE_MIC2=2*/
    "DUAL_AMIC" /*MIC_MODE_DUALMIC=3*/
};

#define RECOGNITION 0x00010000

#define MSA_GAIN_DEFAULT 0

// path type definition
typedef enum ID_AUDIO_PATH_TYPE {
  ID_PTYPE_INVALID,
  ID_PTYPE_INTERFACE = 1,  // Playback input path and Record output path
  ID_PTYPE_DEVICE,         // Playback output path and Record input path
  ID_PTYPE_VIRTUAL,        // Virtual path
  ID_PTYPE_MAX
} path_type_t;

// define both playback input path and record output path
typedef enum ID_INTERFACE_PATH {
  ID_IPATH_INVALID = -1,
  ID_IPATH_RX_MIN,
  ID_IPATH_RX_HIFI_LL = ID_IPATH_RX_MIN,  // low latency
  ID_IPATH_RX_HIFI_DB,                    // deep buffer
  ID_IPATH_RX_VC,                         // voice call
  ID_IPATH_RX_VC_ST,                      // sidetone
  ID_IPATH_RX_HFP,                        // HFP
  ID_IPATH_RX_FM,                         // FM
  ID_IPATH_RX_MAX = ID_IPATH_RX_FM,
  ID_IPATH_TX_MIN,
  ID_IPATH_TX_HIFI = ID_IPATH_TX_MIN,  // hifi recording
  ID_IPATH_TX_VC,                      // voice call
  ID_IPATH_TX_HFP,                     // HFP
  ID_IPATH_TX_FM,                      // FM recording
  ID_IPATH_TX_MAX = ID_IPATH_TX_FM,
} path_interface_t;

// virtual mode definition, only used for virtual path
typedef enum ID_VIRTUAL_MODE {
  V_MODE_INVALID = 0,
  V_MODE_HIFI_LL,       // both input and output
  V_MODE_HIFI_DB,       // only have one direction, output
  V_MODE_VC,            // phone mode in AUDIO_MODE_IN_CALL
  V_MODE_VOIP,          // AUDIO_MODE_IN_COMMUNICATION
  V_MODE_VT,            // AUDIO_MODE_IN_VT_CALL
  V_MODE_HFP,           // HFP mode in AUDIO_MODE_IN_CALL
  V_MODE_FM,            // when FM_STATUS=on
  V_MODE_CP_LOOPBACK,   // test cp loopback
  V_MODE_HW_LOOPBACK,   // test hw loopback
  V_MODE_APP_LOOPBACK,  // test app loopback
  V_MODE_DEF            // add for default config to choose HW device
} virtual_mode_t;

// the priorities of modes configuration
struct vtrl_mode_priority_cfg {
  path_interface_t path_interface;
  audio_mode_t audio_mode;
  virtual_mode_t v_mode;
};

// used to parse config xml, must align with ID_VIRTUAL_MODE
// add a new type Default, will be used when nothing matched
static char *vtrl_mode_name[] = {"INVALID",     "HIFI_LL",      "HIFI_DB",
                                 "VoiceCall",   "VoIP",         "VT",
                                 "HFP",         "FM",           "CP_LOOPBACK",
                                 "HW_LOOPBACK", "APP_LOOPBACK", "Default"};

///////////////////////////////////////////////////////////
// definition of Audio Platform config interface
///////////////////////////////////////////////////////////
unsigned int get_mic_hw_flag(unsigned int hw_dev);
unsigned int get_mic_dev(virtual_mode_t v_mode, unsigned int android_dev);
int init_platform_config();
void deinit_platform_config();
uint32_t get_mic_mode(void);

///////////////////////////////////////////////////////////
// definition of Audio path interface
///////////////////////////////////////////////////////////
char *get_vrtl_path(virtual_mode_t mode, unsigned int device,
                    unsigned int value);

void route_vrtl_path(virtual_mode_t v_mode, int hw_dev, int enable,
                     unsigned int flag, unsigned int value);
void route_hw_device(unsigned int hw_dev, int enable, unsigned int value);
void route_interface(path_interface_t path_itf, int enable);
void set_hw_volume(virtual_mode_t v_mode, int hw_dev, unsigned int value);

void get_msa_gain(unsigned int hw_dev, unsigned char volume,
                  signed char *msa_gain, signed char *msa_gain_wb,
                  bool use_extra_vol);

#endif /* __AUDIO_PATH_MRVL_H__ */
