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

#define LOG_TAG "audio_effect_mrvl"
#define LOG_NDEBUG 0

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <math.h>

#include <cutils/log.h>
#include <cutils/str_parms.h>
#include <cutils/list.h>
#include <cutils/properties.h>

#include <hardware/hardware.h>
#include <hardware/audio.h>
#include <system/audio.h>

#include "audio_effect_mrvl.h"

#define RAMP_DOWN_GAIN_INITIAL_VALUE 1
#define RAMP_UP_GAIN_INITIAL_VALUE 0.0001

static bool ramp_up_begin = false;
static bool ramp_down_begin = false;
static double ramp_gain_value = RAMP_UP_GAIN_INITIAL_VALUE;
// the ramp up gain will increase from 0.0001 to 1 in 80ms(80*44.1*4 bytes),
// 120ms(120*44.1*4 bytes)
static int ramp_up_level_table[] = {80, 120};
// the ramp down gain will decrease from 1 to 0.0001 in 5ms(5*44.1*4 bytes),
// 10ms(10*44.1*4 bytes), 15ms(15*44.1*4 bytes)
static int ramp_down_level_table[] = {5, 10, 15};
// currently, we can only set the ramp level once, if we want to change the ramp
// level by setting it manually, we need kill
// mediaserver after setting the property or reboot then reset the ramp level
// according to property. ramp ratio is
// calculated by g(n)=g(1)*q^(n-1), q=pow(g(n)/g(1), 1.0/(n-1)), n is num of the
// samples, for example(n = 10ms * 44.1)
static unsigned char ramp_up_level = 0xFF;
static unsigned char ramp_down_level = 0xFF;
static double ramp_up_ratio = 0.0;
static double ramp_down_ratio = 0.0;

#define RAMP_UP_LEVEL_NUM \
  (sizeof(ramp_up_level_table) / sizeof(ramp_up_level_table[0]))
#define RAMP_DOWN_LEVEL_NUM \
  (sizeof(ramp_down_level_table) / sizeof(ramp_down_level_table[0]))

unsigned char get_ap_rampdown_level() {
  char prop_value[PROPERTY_VALUE_MAX];
  unsigned char level = 0;
  if (property_get("audio.approcess.rampdown.level", prop_value, "1")) {
    int value = atoi(prop_value);
    if (value >= 0 && (unsigned int)value < RAMP_DOWN_LEVEL_NUM) {
      level = (unsigned char)value;
    } else {
      ALOGW("%s: Invalid ramp down level %d, current only support 0 ~ 2",
            __FUNCTION__, value);
    }
  }
  return level;
}

unsigned char get_ap_rampup_level() {
  char prop_value[PROPERTY_VALUE_MAX];
  unsigned char level = 0;
  if (property_get("audio.approcess.rampup.level", prop_value, "0")) {
    int value = atoi(prop_value);
    if (value >= 0 && (unsigned int)value < RAMP_UP_LEVEL_NUM) {
      level = (unsigned char)value;
    } else {
      ALOGW("%s: Invalid ramp up level %d, current only support 0 ~ 1",
            __FUNCTION__, value);
    }
  }
  return level;
}

// force set the ramp_up_gain value to RAMP_UP_GAIN_INITIAL_VALUE if ramp up is
// triggered
void ramp_up_start(unsigned int sample_rate) {
  ramp_down_begin = false;
  ramp_up_begin = true;
  ramp_gain_value = RAMP_UP_GAIN_INITIAL_VALUE;
  if (ramp_up_level == 0xFF) {
    ramp_up_level = get_ap_rampup_level();
    ramp_up_ratio =
        pow((double)(RAMP_DOWN_GAIN_INITIAL_VALUE / RAMP_UP_GAIN_INITIAL_VALUE),
            (1.0 * 1000) /
                (ramp_up_level_table[ramp_up_level] * sample_rate - 1000));
  }
}

// ramp up has priority to ramp down, if ramp up has been triggered and not
// finished, then ignore the ramp down action
// ignore latter ramp down trigger action if the previous ramp down is still
// on-going
void ramp_down_start(unsigned int sample_rate) {
  if (!ramp_down_begin && !ramp_up_begin) {
    ramp_down_begin = true;
    ramp_up_begin = false;
    ramp_gain_value = RAMP_DOWN_GAIN_INITIAL_VALUE;
  }
  if (ramp_down_level == 0xFF) {
    ramp_down_level = get_ap_rampdown_level();
    ramp_down_ratio =
        pow((double)(RAMP_UP_GAIN_INITIAL_VALUE / RAMP_DOWN_GAIN_INITIAL_VALUE),
            (1.0 * 1000) /
                (ramp_down_level_table[ramp_down_level] * sample_rate - 1000));
  }
}

void ramp_process(void *buffer, size_t bytes) {
  unsigned int i = 0;
  short *buf = (short *)buffer;

  if (!ramp_down_begin && !ramp_up_begin) {
    return;
  }

  if (ramp_down_begin) {
    for (i = 0; i < bytes / (sizeof(short) * 2); i++) {
      buf[2 * i] = (short)((float)buf[2 * i] * ramp_gain_value);
      buf[2 * i + 1] = (short)((float)buf[2 * i + 1] * ramp_gain_value);
      ramp_gain_value *= ramp_down_ratio;
      if (RAMP_UP_GAIN_INITIAL_VALUE > ramp_gain_value) {
        memset(buf + 2 * (i + 1), 0, bytes - 2 * (i + 1) * 2);
        ramp_down_begin = false;
        // FIXME because there is a lock between path operation and in read
        // thread,
        // we can only trigger one ramp action, if ramp down is triggered, ramp
        // up
        // will be triggered automatically after the ramp down finished.
        ramp_up_begin = true;
        ramp_gain_value = RAMP_UP_GAIN_INITIAL_VALUE;
        return;
      }
    }
  }
  if (ramp_up_begin) {
    for (i = 0; i < bytes / (sizeof(short) * 2); i++) {
      buf[2 * i] = (short)((float)buf[2 * i] * ramp_gain_value);
      buf[2 * i + 1] = (short)((float)buf[2 * i + 1] * ramp_gain_value);
      ramp_gain_value *= ramp_up_ratio;
      if (ramp_gain_value > RAMP_DOWN_GAIN_INITIAL_VALUE) {
        ramp_up_begin = false;
        return;
      }
    }
  }
}
