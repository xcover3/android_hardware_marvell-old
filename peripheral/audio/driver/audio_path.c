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

#define LOG_TAG "audio_hw_path"
#define LOG_NDEBUG 0

#include <system/audio.h>
#include <cutils/log.h>
#include "audio_path.h"
#include "audio_hw_mrvl.h"
#include "acm_api.h"
#include "acm_param.h"
#include "audio_path.h"

// define the path interface, include playback input and record output path
#define MAX_NUM_ITF (sizeof(gPathInterface) / sizeof(gPathInterface[0]))
static const struct {
  path_interface_t i_path;
  char *path_name;
} gPathInterface[] = {
    // playback path
    {ID_IPATH_RX_HIFI_LL, PATH_NAME(HiFi1Playback)},
    {ID_IPATH_RX_HIFI_DB, PATH_NAME(HiFi2Playback)},
    {ID_IPATH_RX_VC, PATH_NAME(VoicePlayback)},
    {ID_IPATH_RX_HFP, PATH_NAME(HfpPlayback)},
    {ID_IPATH_RX_FM, PATH_NAME(FMPlaybackI2S)},
    {ID_IPATH_RX_VC_ST, PATH_NAME(VoicePlaybackST)},

    // record path
    {ID_IPATH_TX_HIFI, PATH_NAME(HiFi1Record)},
    {ID_IPATH_TX_VC, PATH_NAME(VoiceRecord)},
    {ID_IPATH_TX_HFP, PATH_NAME(HfpRecord)},
    {ID_IPATH_TX_FM, PATH_NAME(FMI2SRecord)},
};

// define the path device, include playback output and record input path
#define MAX_NUM_DEV (sizeof(gPathDevice) / sizeof(gPathDevice[0]))
static const struct {
  unsigned int device;
  unsigned int flag;
  char *apu_path;
  char *codec_path;
} gPathDevice[] = {
    // output devices
    {HWDEV_EARPIECE, 0, PATH_NAME(ToEarpiece), PATH_NAME(Earpiece)},
    {HWDEV_SPEAKER, 0, PATH_NAME(ToSpeaker), PATH_NAME(Speaker)},
    {HWDEV_STEREOSPEAKER, 0, PATH_NAME(ToStereoSpeaker),
     PATH_NAME(StereoSpeaker)},
    {HWDEV_HEADPHONE, 0, PATH_NAME(ToHeadphone), PATH_NAME(Headphone)},
    {HWDEV_BLUETOOTH_NB, 0, PATH_NAME(ToBTNB), NULL},
    {HWDEV_BT_NREC_OFF_NB, 0, PATH_NAME(ToBTNB(NREC Mode)), NULL},
    {HWDEV_BLUETOOTH_WB, 0, PATH_NAME(ToBTWB), NULL},
    {HWDEV_BT_NREC_OFF_WB, 0, PATH_NAME(ToBTWB(NREC Mode)), NULL},
    {HWDEV_OUT_TTY, 0, PATH_NAME(ToHeadphone), PATH_NAME(Headphone)},
    {HWDEV_OUT_TTY_HCO_SPEAKER, 0, PATH_NAME(ToSpeaker), PATH_NAME(Speaker)},
    {HWDEV_OUT_TTY_HCO_STEREOSPEAKER, 0, PATH_NAME(ToStereoSpeaker),
     PATH_NAME(StereoSpeaker)},

    // input devices
    {HWDEV_AMIC1, 0, PATH_NAME(FromAMic1), PATH_NAME(AMic1)},
    {HWDEV_AMIC1_SPK_MODE, 0, PATH_NAME(FromAMic1(Speaker Mode)),
     PATH_NAME(AMic1(Speaker Mode))},
    {HWDEV_AMIC1_HP_MODE, 0, PATH_NAME(FromAMic1(Headphone Mode)),
     PATH_NAME(AMic1(Headphone Mode))},
    {HWDEV_AMIC2, 0, PATH_NAME(FromAMic2), PATH_NAME(AMic2)},
    {HWDEV_AMIC2_SPK_MODE, 0, PATH_NAME(FromAMic2(Speaker Mode)),
     PATH_NAME(AMic2(Speaker Mode))},
    {HWDEV_AMIC2_HP_MODE, 0, PATH_NAME(FromAMic2(Headphone Mode)),
     PATH_NAME(AMic2(Headphone Mode))},
    {HWDEV_DUAL_DMIC1, 0, PATH_NAME(FromDualDMic1), PATH_NAME(DualDMic1)},
    {HWDEV_DMIC1, 0, PATH_NAME(FromDMic1), PATH_NAME(DMic1)},
    {HWDEV_DMIC2, 0, PATH_NAME(FromDMic2), PATH_NAME(DMic2)},
    {HWDEV_HSMIC, 0, PATH_NAME(FromHsMic), PATH_NAME(HsMic)},
    {HWDEV_BTMIC_NB, 0, PATH_NAME(FromBTMicNB), NULL},
    {HWDEV_BTMIC_NREC_OFF_NB, 0, PATH_NAME(FromBTMicNB(NREC Mode)), NULL},
    {HWDEV_BTMIC_WB, 0, PATH_NAME(FromBTMicWB), NULL},
    {HWDEV_BTMIC_NREC_OFF_WB, 0, PATH_NAME(FromBTMicWB(NREC Mode)), NULL},
    {HWDEV_DUAL_AMIC, 0, PATH_NAME(FromDualAMic), PATH_NAME(DualAMic)},
    {HWDEV_DUAL_AMIC_SPK_MODE, 0, PATH_NAME(FromDualAMic(Speaker Mode)),
     PATH_NAME(DualAMic(Speaker Mode))},
    {HWDEV_DUAL_AMIC_HP_MODE, 0, PATH_NAME(FromDualAMic(Headphone Mode)),
     PATH_NAME(DualAMic(Headphone Mode))},
    {HWDEV_IN_TTY, 0, PATH_NAME(FromHsMic), PATH_NAME(HsMic)},
    {HWDEV_IN_TTY_VCO_AMIC1, 0, PATH_NAME(FromAMic1), PATH_NAME(AMic1)},
    {HWDEV_IN_TTY_VCO_AMIC2, 0, PATH_NAME(FromAMic2), PATH_NAME(AMic2)},
    {HWDEV_IN_TTY_VCO_DUAL_AMIC, 0, PATH_NAME(FromDualAMic),
     PATH_NAME(DualAMic)},
    {HWDEV_IN_TTY_VCO_DUAL_AMIC_SPK_MODE, 0,
     PATH_NAME(FromDualAMic(Speaker Mode)), PATH_NAME(DualAMic(Speaker Mode))},
    {HWDEV_IN_TTY_VCO_DUAL_DMIC1, 0, PATH_NAME(FromDualDMic1),
     PATH_NAME(DualDMic1)},
};

// define virtual path
#define MAX_NUM_VRTL (sizeof(gPathVirtual) / sizeof(gPathVirtual[0]))
static const struct {
  virtual_mode_t v_mode;
  unsigned int device;
  unsigned int flag;
  char *path_name;
} gPathVirtual[] = {
    // HiFi Playback
    {V_MODE_HIFI_LL, HWDEV_EARPIECE, 0, PATH_NAME(HiFi1PlaybackToEarpiece)},
    {V_MODE_HIFI_LL, HWDEV_SPEAKER, 0, PATH_NAME(HiFi1PlaybackToSpeaker)},
    {V_MODE_HIFI_LL, HWDEV_STEREOSPEAKER, 0,
     PATH_NAME(HiFi1PlaybackToStereoSpeaker)},
    {V_MODE_HIFI_LL, HWDEV_BLUETOOTH_NB, 0, PATH_NAME(HiFi1PlaybackToBTNB)},
    {V_MODE_HIFI_LL, HWDEV_BLUETOOTH_WB, 0, PATH_NAME(HiFi1PlaybackToBTWB)},
    {V_MODE_HIFI_LL, HWDEV_HEADPHONE, 0, PATH_NAME(HiFi1PlaybackToHeadphone)},
    {V_MODE_HIFI_LL, HWDEV_BT_NREC_OFF_NB, 0,
     PATH_NAME(HiFi1PlaybackToBTNB(NREC Mode))},
    {V_MODE_HIFI_LL, HWDEV_BT_NREC_OFF_WB, 0,
     PATH_NAME(HiFi1PlaybackToBTWB(NREC Mode))},

    // force speaker mode
    {V_MODE_HIFI_LL, HWDEV_SPEAKER | HWDEV_HEADPHONE, 0,
     PATH_NAME(HiFi1PlaybackToSpeakerAndHeadphone)},
    {V_MODE_HIFI_LL, HWDEV_STEREOSPEAKER | HWDEV_HEADPHONE, 0,
     PATH_NAME(HiFi1PlaybackToStereoSpeakerAndHeadphone)},

    {V_MODE_HIFI_DB, HWDEV_EARPIECE, 0, PATH_NAME(HiFi2PlaybackToEarpiece)},
    {V_MODE_HIFI_DB, HWDEV_SPEAKER, 0, PATH_NAME(HiFi2PlaybackToSpeaker)},
    {V_MODE_HIFI_DB, HWDEV_STEREOSPEAKER, 0,
     PATH_NAME(HiFi2PlaybackToStereoSpeaker)},
    {V_MODE_HIFI_DB, HWDEV_HEADPHONE, 0, PATH_NAME(HiFi2PlaybackToHeadphone)},
    {V_MODE_HIFI_DB, HWDEV_BLUETOOTH_NB, 0, PATH_NAME(HiFi2PlaybackToBTNB)},
    {V_MODE_HIFI_DB, HWDEV_BLUETOOTH_WB, 0, PATH_NAME(HiFi2PlaybackToBTWB)},
    {V_MODE_HIFI_DB, HWDEV_BT_NREC_OFF_NB, 0,
     PATH_NAME(HiFi2PlaybackToBTNB(NREC Mode))},
    {V_MODE_HIFI_DB, HWDEV_BT_NREC_OFF_WB, 0,
     PATH_NAME(HiFi2PlaybackToBTWB(NREC Mode))},

    // HiFi Record
    {V_MODE_HIFI_LL, HWDEV_AMIC1, 0, PATH_NAME(HiFi1RecordFromAMic1)},
    {V_MODE_HIFI_LL, HWDEV_AMIC2, 0, PATH_NAME(HiFi1RecordFromAMic2)},
    {V_MODE_HIFI_LL, HWDEV_DMIC1, 0, PATH_NAME(HiFi1RecordFromDMic1)},
    {V_MODE_HIFI_LL, HWDEV_DMIC2, 0, PATH_NAME(HiFi1RecordFromDMic2)},
    {V_MODE_HIFI_LL, HWDEV_DUAL_DMIC1, 0, PATH_NAME(HiFi1RecordFromDualDMic1)},
    {V_MODE_HIFI_LL, HWDEV_HSMIC, 0, PATH_NAME(HiFi1RecordFromHsMic)},
    {V_MODE_HIFI_LL, HWDEV_BTMIC_NB, 0, PATH_NAME(HiFi1RecordFromBTMicNB)},
    {V_MODE_HIFI_LL, HWDEV_BTMIC_WB, 0, PATH_NAME(HiFi1RecordFromBTMicWB)},
    {V_MODE_HIFI_LL, HWDEV_DUAL_AMIC, 0, PATH_NAME(HiFi1RecordFromDualAMic)},
    {V_MODE_HIFI_LL, HWDEV_BTMIC_NREC_OFF_NB, 0,
     PATH_NAME(HiFi1RecordFromBTMicNB(NREC Mode))},
    {V_MODE_HIFI_LL, HWDEV_BTMIC_NREC_OFF_WB, 0,
     PATH_NAME(HiFi1RecordFromBTMicWB(NREC Mode))},

    // HiFi Record Recognition
    {V_MODE_HIFI_LL, HWDEV_AMIC1, RECOGNITION,
     PATH_NAME(HiFi1RecognitionRecordFromAMic1)},
    {V_MODE_HIFI_LL, HWDEV_AMIC2, RECOGNITION,
     PATH_NAME(HiFi1RecognitionRecordFromAMic2)},
    {V_MODE_HIFI_LL, HWDEV_DMIC1, RECOGNITION,
     PATH_NAME(HiFi1RecognitionRecordFromDMic1)},
    {V_MODE_HIFI_LL, HWDEV_DMIC2, RECOGNITION,
     PATH_NAME(HiFi1RecognitionRecordFromDMic2)},
    {V_MODE_HIFI_LL, HWDEV_DUAL_DMIC1, RECOGNITION,
     PATH_NAME(HiFi1RecognitionRecordFromDualDMic1)},
    {V_MODE_HIFI_LL, HWDEV_HSMIC, RECOGNITION,
     PATH_NAME(HiFi1RecognitionRecordFromHsMic)},
    {V_MODE_HIFI_LL, HWDEV_BTMIC_NB, RECOGNITION,
     PATH_NAME(HiFi1RecognitionRecordFromBTMicNB)},
    {V_MODE_HIFI_LL, HWDEV_BTMIC_WB, RECOGNITION,
     PATH_NAME(HiFi1RecognitionRecordFromBTMicWB)},
    {V_MODE_HIFI_LL, HWDEV_DUAL_AMIC, RECOGNITION,
     PATH_NAME(HiFi1RecognitionRecordFromDualAMic)},
    {V_MODE_HIFI_LL, HWDEV_BTMIC_NREC_OFF_NB, RECOGNITION,
     PATH_NAME(HiFi1RecognitionRecordFromBTMicNB(NREC Mode))},
    {V_MODE_HIFI_LL, HWDEV_BTMIC_NREC_OFF_WB, RECOGNITION,
     PATH_NAME(HiFi1RecognitionRecordFromBTMicWB(NREC Mode))},

    // Voice Call Playback
    {V_MODE_VC, HWDEV_EARPIECE, 0, PATH_NAME(VoicePlaybackToEarpiece)},
    {V_MODE_VC, HWDEV_SPEAKER, 0, PATH_NAME(VoicePlaybackToSpeaker)},
    {V_MODE_VC, HWDEV_STEREOSPEAKER, 0,
     PATH_NAME(VoicePlaybackToStereoSpeaker)},
    {V_MODE_VC, HWDEV_HEADPHONE, 0, PATH_NAME(VoicePlaybackToHeadphone)},
    {V_MODE_VC, HWDEV_BLUETOOTH_NB, 0, PATH_NAME(VoicePlaybackToBTNB)},
    {V_MODE_VC, HWDEV_BLUETOOTH_WB, 0, PATH_NAME(VoicePlaybackToBTWB)},
    {V_MODE_VC, HWDEV_OUT_TTY, 0, PATH_NAME(VoicePlaybackToTty)},
    {V_MODE_VC, HWDEV_OUT_TTY_HCO_SPEAKER, 0,
     PATH_NAME(VoicePlaybackToTtyHcoSpeaker)},
    {V_MODE_VC, HWDEV_OUT_TTY_HCO_STEREOSPEAKER, 0,
     PATH_NAME(VoicePlaybackToTtyHcoStereoSpeaker)},
    {V_MODE_VC, HWDEV_BT_NREC_OFF_NB, 0,
     PATH_NAME(VoicePlaybackToBTNB(NREC Mode))},
    {V_MODE_VC, HWDEV_BT_NREC_OFF_WB, 0,
     PATH_NAME(VoicePlaybackToBTWB(NREC Mode))},

    // Voice Call Record
    {V_MODE_VC, HWDEV_AMIC1, 0, PATH_NAME(VoiceRecordFromAMic1)},
    {V_MODE_VC, HWDEV_AMIC1_SPK_MODE, 0,
     PATH_NAME(VoiceRecordFromAMic1(Speaker Mode))},
    {V_MODE_VC, HWDEV_AMIC1_HP_MODE, 0,
     PATH_NAME(VoiceRecordFromAMic1(Headphone Mode))},
    {V_MODE_VC, HWDEV_AMIC2, 0, PATH_NAME(VoiceRecordFromAMic2)},
    {V_MODE_VC, HWDEV_AMIC2_SPK_MODE, 0,
     PATH_NAME(VoiceRecordFromAMic2(Speaker Mode))},
    {V_MODE_VC, HWDEV_AMIC2_HP_MODE, 0,
     PATH_NAME(VoiceRecordFromAMic2(Headphone Mode))},
    {V_MODE_VC, HWDEV_DMIC1, 0, PATH_NAME(VoiceRecordFromDMic1)},
    {V_MODE_VC, HWDEV_DMIC2, 0, PATH_NAME(VoiceRecordFromDMic2)},
    {V_MODE_VC, HWDEV_HSMIC, 0, PATH_NAME(VoiceRecordFromHsMic)},
    {V_MODE_VC, HWDEV_BTMIC_NB, 0, PATH_NAME(VoiceRecordFromBTMicNB)},
    {V_MODE_VC, HWDEV_BTMIC_WB, 0, PATH_NAME(VoiceRecordFromBTMicWB)},
    {V_MODE_VC, HWDEV_DUAL_AMIC, 0, PATH_NAME(VoiceRecordFromDualAMic)},
    {V_MODE_VC, HWDEV_DUAL_AMIC_SPK_MODE, 0,
     PATH_NAME(VoiceRecordFromDualAMic(Speaker Mode))},
    {V_MODE_VC, HWDEV_DUAL_AMIC_HP_MODE, 0,
     PATH_NAME(VoiceRecordFromDualAMic(Headphone Mode))},
    {V_MODE_VC, HWDEV_IN_TTY, 0, PATH_NAME(VoiceRecordFromTty)},
    {V_MODE_VC, HWDEV_IN_TTY_VCO_AMIC1, 0,
     PATH_NAME(VoiceRecordFromTtyVcoAMic1)},
    {V_MODE_VC, HWDEV_IN_TTY_VCO_AMIC2, 0,
     PATH_NAME(VoiceRecordFromTtyVcoAMic2)},
    {V_MODE_VC, HWDEV_IN_TTY_VCO_DUAL_AMIC, 0,
     PATH_NAME(VoiceRecordFromTtyVcoDualAMic)},
    {V_MODE_VC, HWDEV_IN_TTY_VCO_DUAL_AMIC_SPK_MODE, 0,
     PATH_NAME(VoiceRecordFromTtyVcoDualAMic(Speaker Mode))},
    {V_MODE_VC, HWDEV_IN_TTY_VCO_DUAL_DMIC1, 0,
     PATH_NAME(VoiceRecordFromTtyVcoDualDMic1)},
    {V_MODE_VC, HWDEV_BTMIC_NREC_OFF_NB, 0,
     PATH_NAME(VoiceRecordFromBTMicNB(NREC Mode))},
    {V_MODE_VC, HWDEV_BTMIC_NREC_OFF_WB, 0,
     PATH_NAME(VoiceRecordFromBTMicWB(NREC Mode))},

    // VOIP Playback
    {V_MODE_VOIP, HWDEV_SPEAKER, 0, PATH_NAME(VoipPlaybackToSpeaker)},
    {V_MODE_VOIP, HWDEV_STEREOSPEAKER, 0,
     PATH_NAME(VoipPlaybackToStereoSpeaker)},
    {V_MODE_VOIP, HWDEV_HEADPHONE, 0, PATH_NAME(VoipPlaybackToHeadphone)},
    {V_MODE_VOIP, HWDEV_EARPIECE, 0, PATH_NAME(VoipPlaybackToEarpiece)},
    {V_MODE_VOIP, HWDEV_BLUETOOTH_NB, 0, PATH_NAME(VoipPlaybackToBTNB)},
    {V_MODE_VOIP, HWDEV_BLUETOOTH_WB, 0, PATH_NAME(VoipPlaybackToBTWB)},
    {V_MODE_VOIP, HWDEV_BT_NREC_OFF_NB, 0,
     PATH_NAME(VoipPlaybackToBTNB(NREC Mode))},
    {V_MODE_VOIP, HWDEV_BT_NREC_OFF_WB, 0,
     PATH_NAME(VoipPlaybackToBTWB(NREC Mode))},

    // VOIP Record
    {V_MODE_VOIP, HWDEV_AMIC1, 0, PATH_NAME(VoipRecordFromAMic1)},
    {V_MODE_VOIP, HWDEV_AMIC1_SPK_MODE, 0,
     PATH_NAME(VoipRecordFromAMic1(Speaker Mode))},
    {V_MODE_VOIP, HWDEV_AMIC1_HP_MODE, 0,
     PATH_NAME(VoipRecordFromAMic1(Headhone Mode))},
    {V_MODE_VOIP, HWDEV_AMIC2, 0, PATH_NAME(VoipRecordFromAMic2)},
    {V_MODE_VOIP, HWDEV_AMIC2_SPK_MODE, 0,
     PATH_NAME(VoipRecordFromAMic2(Speaker Mode))},
    {V_MODE_VOIP, HWDEV_AMIC2_HP_MODE, 0,
     PATH_NAME(VoipRecordFromAMic2(Headphone Mode))},
    {V_MODE_VOIP, HWDEV_DMIC1, 0, PATH_NAME(VoipRecordFromDMic1)},
    {V_MODE_VOIP, HWDEV_DMIC2, 0, PATH_NAME(VoipRecordFromDMic2)},
    {V_MODE_VOIP, HWDEV_HSMIC, 0, PATH_NAME(VoipRecordFromHsMic)},
    {V_MODE_VOIP, HWDEV_BTMIC_NB, 0, PATH_NAME(VoipRecordFromBTMicNB)},
    {V_MODE_VOIP, HWDEV_BTMIC_WB, 0, PATH_NAME(VoipRecordFromBTMicWB)},
    {V_MODE_VOIP, HWDEV_DUAL_AMIC, 0, PATH_NAME(VoipRecordFromDualAMic)},
    {V_MODE_VOIP, HWDEV_DUAL_AMIC_SPK_MODE, 0,
     PATH_NAME(VoipRecordFromDualAMic(Speaker Mode))},
    {V_MODE_VOIP, HWDEV_DUAL_AMIC_HP_MODE, 0,
     PATH_NAME(VoipRecordFromDualAMic(Headphone Mode))},
    {V_MODE_VOIP, HWDEV_BTMIC_NREC_OFF_NB, 0,
     PATH_NAME(VoipRecordFromBTMicNB(NREC Mode))},
    {V_MODE_VOIP, HWDEV_BTMIC_NREC_OFF_WB, 0,
     PATH_NAME(VoipRecordFromBTMicWB(NREC Mode))},

    // VT Playback
    {V_MODE_VT, HWDEV_SPEAKER, 0, PATH_NAME(VTPlaybackToSpeaker)},
    {V_MODE_VT, HWDEV_STEREOSPEAKER, 0, PATH_NAME(VTPlaybackToStereoSpeaker)},
    {V_MODE_VT, HWDEV_HEADPHONE, 0, PATH_NAME(VTPlaybackToHeadphone)},
    {V_MODE_VT, HWDEV_EARPIECE, 0, PATH_NAME(VTPlaybackToEarpiece)},
    {V_MODE_VT, HWDEV_BLUETOOTH_NB, 0, PATH_NAME(VTPlaybackToBTNB)},
    {V_MODE_VT, HWDEV_BLUETOOTH_WB, 0, PATH_NAME(VTPlaybackToBTWB)},
    {V_MODE_VT, HWDEV_BT_NREC_OFF_NB, 0,
     PATH_NAME(VTPlaybackToBTNB(NREC Mode))},
    {V_MODE_VT, HWDEV_BT_NREC_OFF_WB, 0,
     PATH_NAME(VTPlaybackToBTWB(NREC Mode))},

    // VT Record
    {V_MODE_VT, HWDEV_AMIC1, 0, PATH_NAME(VTRecordFromAMic1)},
    {V_MODE_VT, HWDEV_AMIC1_SPK_MODE, 0,
     PATH_NAME(VTRecordFromAMic1(Speaker Mode))},
    {V_MODE_VT, HWDEV_AMIC1_HP_MODE, 0,
     PATH_NAME(VTRecordFromAMic1(Headphone Mode))},
    {V_MODE_VT, HWDEV_AMIC2, 0, PATH_NAME(VTRecordFromAMic2)},
    {V_MODE_VT, HWDEV_AMIC2_SPK_MODE, 0,
     PATH_NAME(VTRecordFromAMic2(Speaker Mode))},
    {V_MODE_VT, HWDEV_AMIC2_HP_MODE, 0,
     PATH_NAME(VTRecordFromAMic2(Headphone Mode))},
    {V_MODE_VT, HWDEV_DMIC1, 0, PATH_NAME(VTRecordFromDMic1)},
    {V_MODE_VT, HWDEV_DMIC2, 0, PATH_NAME(VTRecordFromDMic2)},
    {V_MODE_VT, HWDEV_HSMIC, 0, PATH_NAME(VTRecordFromHsMic)},
    {V_MODE_VT, HWDEV_BTMIC_NB, 0, PATH_NAME(VTRecordFromBTMicNB)},
    {V_MODE_VT, HWDEV_BTMIC_WB, 0, PATH_NAME(VTRecordFromBTMicWB)},
    {V_MODE_VT, HWDEV_DUAL_AMIC, 0, PATH_NAME(VTRecordFromDualAMic)},
    {V_MODE_VT, HWDEV_DUAL_AMIC_SPK_MODE, 0,
     PATH_NAME(VTRecordFromDualAMic(Speaker Mode))},
    {V_MODE_VT, HWDEV_DUAL_AMIC_HP_MODE, 0,
     PATH_NAME(VTRecordFromDualAMic(Headphone Mode))},
    {V_MODE_VT, HWDEV_BTMIC_NREC_OFF_NB, 0,
     PATH_NAME(VTRecordFromBTMicNB(NREC Mode))},
    {V_MODE_VT, HWDEV_BTMIC_NREC_OFF_WB, 0,
     PATH_NAME(VTRecordFromBTMicWB(NREC Mode))},

    // HFP
    {V_MODE_HFP, HWDEV_SPEAKER, 0, PATH_NAME(HfpPlaybackToSpeaker)},
    {V_MODE_HFP, HWDEV_AMIC2, 0, PATH_NAME(HfpRecordFromAMic2)},
    {V_MODE_HFP, HWDEV_AMIC1, 0, PATH_NAME(HfpRecordFromAMic1)},

    // FM Playback
    {V_MODE_FM, HWDEV_SPEAKER, 0, PATH_NAME(FMPlaybackI2SToSpeaker)},
    {V_MODE_FM, HWDEV_STEREOSPEAKER, 0,
     PATH_NAME(FMPlaybackI2SToStereoSpeaker)},
    {V_MODE_FM, HWDEV_HEADPHONE, 0, PATH_NAME(FMPlaybackI2SToHeadphone)},

    // FM Record
    {V_MODE_FM, HWDEV_INVALID, 0, PATH_NAME(FMI2SRecordFromFM)},

    // CP Loopback
    {V_MODE_CP_LOOPBACK, HWDEV_AMIC1, 0, PATH_NAME(VoiceRecordFromAMic1)},
    {V_MODE_CP_LOOPBACK, HWDEV_AMIC2, 0, PATH_NAME(VoiceRecordFromAMic2)},
    {V_MODE_CP_LOOPBACK, HWDEV_DMIC1, 0, PATH_NAME(VoiceRecordFromDMic1)},
    {V_MODE_CP_LOOPBACK, HWDEV_HSMIC, 0, PATH_NAME(VoiceRecordFromHsMic)},

    // HW Loopback
    {V_MODE_HW_LOOPBACK, HWDEV_AMIC1, 0, PATH_NAME(VoiceRecordFromAMic1)},
    {V_MODE_HW_LOOPBACK, HWDEV_AMIC2, 0, PATH_NAME(VoiceRecordFromAMic2)},
    {V_MODE_HW_LOOPBACK, HWDEV_DMIC1, 0, PATH_NAME(VoiceRecordFromDMic1)},
    {V_MODE_HW_LOOPBACK, HWDEV_HSMIC, 0, PATH_NAME(VoiceRecordFromHsMic)},

    // APP Loopback
    {V_MODE_APP_LOOPBACK, HWDEV_AMIC1, 0, PATH_NAME(VoipRecordFromAMic1)},
    {V_MODE_APP_LOOPBACK, HWDEV_AMIC2, 0, PATH_NAME(VoipRecordFromAMic2)},
    {V_MODE_APP_LOOPBACK, HWDEV_DMIC1, 0, PATH_NAME(VoipRecordFromDMic1)},
    {V_MODE_APP_LOOPBACK, HWDEV_HSMIC, 0, PATH_NAME(VoipRecordFromHsMic)},
};

static void handle_ctl_info(char *path_name, int method, char *old_path_name,
                            int val) {
  ACM_ReturnCode ret = ACM_RC_OK;
  if (path_name == NULL) {
    ALOGI("%s NULL path, return directly", __FUNCTION__);
    return;
  }

  switch (method) {
    case METHOD_ENABLE:
      ALOGI("Enable path %s, value is 0x%08x", path_name, val);
      ret = ACMAudioPathEnable(path_name, val);
      break;
    case METHOD_DISABLE:
      ALOGI("Disable path %s, value is 0x%08x", path_name, val);
      ret = ACMAudioPathHotDisable(path_name, val);
      break;
    case METHOD_MUTE:
      ALOGI("Mute path %s, value is 0x%08x", path_name, val);
      ret = ACMAudioPathMute(path_name, val);
      break;
    case METHOD_VOLUME_SET:
      ALOGI("Set Volume for path %s, value = 0x%08x", path_name, val);
      ret = ACMAudioPathVolumeSet(path_name, val);
      break;
    case METHOD_SWITCH:
      ALOGI("Switch from old path %s to path %s, value is 0x%08x",
            old_path_name, path_name, val);
      ret = ACMAudioPathSwitch(old_path_name, path_name, val);
      break;
    default:
      ALOGI("not support method %d\n", method);
      break;
  }
  if (ret != ACM_RC_OK) {
    ALOGW("%s: Audio path handling error, method: %d error code: %d",
          __FUNCTION__, method, ret);
  }
}

char *get_vrtl_path(virtual_mode_t v_mode, unsigned int hw_dev,
                    unsigned int flag) {
  int i = 0;

  // find ACM path and enable/disable it through ACM
  for (i = 0; i < (int)MAX_NUM_VRTL; i++) {
    if ((gPathVirtual[i].v_mode == v_mode) &&
        (gPathVirtual[i].device == hw_dev) && (gPathVirtual[i].flag == flag)) {
      ALOGD("%s find path %s", __FUNCTION__, gPathVirtual[i].path_name);
      return gPathVirtual[i].path_name;
    }
  }

  return NULL;
}

// parameter "flag" is used to search virtual path in audio HAL
// parameter "value" is used for register value setting in ACM
void route_vrtl_path(virtual_mode_t v_mode, int hw_dev, int enable,
                     unsigned int flag, unsigned int value) {
  char *path = NULL;
  ALOGI("%s mode %d hw_dev 0x%x %s", __FUNCTION__, v_mode, hw_dev,
        (enable == METHOD_ENABLE) ? "enable" : "disable");

  path = get_vrtl_path(v_mode, hw_dev, flag);
  if (path != NULL) {
    // set hardware volume to 0 as default.
    // for normal Hifi/Voice call path, gain still use fix value
    // for force speaker/FM, we set path mute first, set_volume will set correct
    // volume
    handle_ctl_info(path, enable, NULL, value);
  }
}

void route_hw_device(unsigned int hw_dev, int enable, unsigned int value) {
  int i = 0;
  ALOGI("%s hw_dev 0x%x %s", __FUNCTION__, hw_dev,
        (enable == METHOD_ENABLE) ? "enable" : "disable");

  // find ACM path and enable/disable it through ACM
  for (i = 0; i < (int)MAX_NUM_DEV; i++) {
    if ((gPathDevice[i].device == hw_dev) &&
        ((gPathDevice[i].flag & value) == gPathDevice[i].flag)) {
      ALOGD("%s find path %s and %s", __FUNCTION__, gPathDevice[i].apu_path,
            gPathDevice[i].codec_path);
      if (enable == METHOD_ENABLE) {
        handle_ctl_info(gPathDevice[i].apu_path, enable, NULL, value);
        handle_ctl_info(gPathDevice[i].codec_path, enable, NULL, value);
      } else {
        handle_ctl_info(gPathDevice[i].codec_path, enable, NULL, value);
        handle_ctl_info(gPathDevice[i].apu_path, enable, NULL, value);
      }
    }
  }
}

void route_interface(path_interface_t path_itf, int enable) {
  unsigned int value = 0;
  int i = 0;
  ALOGI("%s path_interface %d %s", __FUNCTION__, path_itf,
        (enable == METHOD_ENABLE) ? "enable" : "disable");

  if ((path_itf < ID_IPATH_RX_MIN) || (path_itf > ID_IPATH_TX_MAX)) {
    ALOGE("%s Invalid path error %d", __FUNCTION__, path_itf);
    return;
  }

  // find ACM path and enable/disable it through ACM
  for (i = 0; i < (int)MAX_NUM_ITF; i++) {
    if (gPathInterface[i].i_path == path_itf) {
      ALOGI("%s find path %s", __FUNCTION__, gPathInterface[i].path_name);
      handle_ctl_info(gPathInterface[i].path_name, enable, NULL, value);
    }
  }
}

// setting hardware volume accroding to virtual mode and hw device
void set_hw_volume(virtual_mode_t v_mode, int hw_dev, unsigned int value) {
  char *path = NULL;

  path = get_vrtl_path(v_mode, hw_dev, 0);

  if (path != NULL) {
    ALOGI("%s path %s volume set to 0x%x\n", __FUNCTION__, path, value);
    handle_ctl_info(path, METHOD_VOLUME_SET, NULL, value);
  }
}

// get MSA gain from ACM xml according to special device
void get_msa_gain(unsigned int hw_dev, unsigned char volume,
                  signed char *ret_gain, signed char *ret_gain_wb,
                  bool use_extra_vol) {
  int extra_volume =
      (use_extra_vol ? MSA_GAIN_EXTRA_MODE : MSA_GAIN_NORMAL_MODE);
  signed char tmp_msa_gain = MSA_GAIN_DEFAULT;
  signed char tmp_msa_gainwb = MSA_GAIN_DEFAULT;
  ACM_MsaGain msa_gain = {0};
  unsigned int size = sizeof(ACM_MsaGain);

  msa_gain.volume = volume;
  msa_gain.gain = 0;
  msa_gain.wbGain = 0;
  msa_gain.path_direction = 0;

  // fix to VC mode here. only VC have msa gain
  msa_gain.path_name =
      (const unsigned char *)get_vrtl_path(V_MODE_VC, hw_dev, 0);

  if (ACMSetParameter(ACM_MSA_GAIN_MODE, NULL, extra_volume) != ACM_RC_OK) {
    ALOGI("set output msa gain mode failed, use the default normal mode ");
  }

  if (ACMGetParameter(ACM_MSA_GAIN, &msa_gain, &size) != ACM_RC_OK) {
    ALOGE("get output msa gain failed, use default gain.");
  } else {
    tmp_msa_gain = msa_gain.gain;
    tmp_msa_gainwb = msa_gain.wbGain;
  }

  *ret_gain = tmp_msa_gain;
  *ret_gain_wb = tmp_msa_gainwb;
}
