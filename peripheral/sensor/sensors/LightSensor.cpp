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

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <linux/input.h>

#include "LightSensor.h"

#define LOG_DBG 1

/*****************************************************************************/

struct sensor_t IntersilLightSensor::sBaseSensorList[] = {
    {"Intersil Light sensor",
     "Intersil ",
     1,
     ISL_SENSORS_LIGHT_HANDLE,
     SENSOR_TYPE_LIGHT,
     4095.0f,
     50.0f,
     0.356f,
     100,
     0,
     0,
     "",
     "",
     0,
     0,
     {}},
};

IntersilLightSensor::IntersilLightSensor()
    : LightSensorSet("ISL_light_sensor") {
  LightSensorSet::mPendingEvent.sensor = ISL_ID_L;
}

/*****************************************************************************/

struct sensor_t AvagoLightSensor::sBaseSensorList[] = {
    {"Avago Light sensor",
     "Avago ",
     1,
     AVAGO_SENSORS_LIGHT_HANDLE,
     SENSOR_TYPE_LIGHT,
     30000.0f,
     1.0f,
     0.5f,
     100000,
     0,
     0,
     "",
     "",
     0,
     0,
     {}},
};

AvagoLightSensor::AvagoLightSensor() : LightSensorSet("APDS_light_sensor") {
  LightSensorSet::mPendingEvent.sensor = AVAGO_ID_L;
}

/**********************************************************************************************/
struct sensor_t TSLLightSensor::sBaseSensorList[] = {
    {"TSL2x7x Light sensor",
     "Taos",
     1,
     TSL_SENSORS_LIGHT_HANDLE,
     SENSOR_TYPE_LIGHT,
     10000.0f,
     1.0f,
     0.5f,
     100,
     0,
     0,
     "",
     "",
     0,
     0,
     {}},
};

TSLLightSensor::TSLLightSensor() : LightSensorSet("TSL_light_sensor") {
  LightSensorSet::mPendingEvent.sensor = TSL_ID_L;
}

/**********************************************************************************************/
struct sensor_t EplLightSensor::sBaseSensorList[] = {
    {"ELAN Light sensor",
     "ELAN",
     1,
     ELAN_SENSORS_LIGHT_HANDLE,
     SENSOR_TYPE_LIGHT,
     30000.0f,
     1.0f,
     0.5f,
     100000,
     0,
     0,
     "",
     "",
     0,
     0,
     {}},
};

EplLightSensor::EplLightSensor() : LightSensorSet("elan_light_sensor") {
  LightSensorSet::mPendingEvent.sensor = ELAN_ID_L;
}

/**********************************************************************************************/
struct sensor_t DynaLightSensor::sBaseSensorList[] = {
    {"DYNA Light sensor",
     "DYNA",
     1,
     DYNA_SENSORS_LIGHT_HANDLE,
     SENSOR_TYPE_LIGHT,
     30000.0f,
     1.0f,
     0.5f,
     100000,
     0,
     0,
     "",
     "",
     0,
     0,
     {}},
};

DynaLightSensor::DynaLightSensor() : LightSensorSet("AP3426_light_sensor") {
  LightSensorSet::mPendingEvent.sensor = DYNA_ID_L;
}

/**********************************************************************************************/

LightSensorSet::LightSensorSet(const char* name)
    : MrvlSensorBase(NULL, name),
      mEnabled(0),
      mInputReader(4),
      mHasPendingEvent(false) {
  mPendingEvent.version = sizeof(sensors_event_t);
  mPendingEvent.type = SENSOR_TYPE_LIGHT;
  memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));
  if (LOG_DBG) SLOGD("LightSensor: sysfs[%s]\n", mClassPath);
}

LightSensorSet::~LightSensorSet() {}

int LightSensorSet::setDelay(int32_t handle, int64_t delay_ns) {
  if (mEnabled) {
    int fd;
    char intervalPath[PATH_MAX];
    sprintf(intervalPath, "%s/%s", mClassPath, "interval");
    fd = open(intervalPath, O_RDWR);
    if (fd >= 0) {
      char buf[80];
      sprintf(buf, "%lld", delay_ns / 1000000);
      write(fd, buf, strlen(buf) + 1);
      close(fd);
      return 0;
    }
  }
  return -1;
}

int LightSensorSet::enable(int32_t handle, int en) {
  int flags = en ? 1 : 0;
  if (flags != mEnabled) {
    int fd;
    char enablePath[PATH_MAX];
    sprintf(enablePath, "%s/%s", mClassPath, "active");
    if (LOG_DBG) SLOGD("LightSensor enable path is %s", enablePath);
    fd = open(enablePath, O_RDWR);
    if (fd >= 0) {
      char buf[2];
      int err;
      buf[1] = '\n';
      if (flags) {
        buf[0] = '1';
      } else {
        buf[0] = '0';
      }
      err = write(fd, buf, sizeof(buf));
      close(fd);
      mEnabled = flags;

      return 0;
    }

    LOGE("Error while open %s", enablePath);
    return -1;
  }
  return 0;
}

bool LightSensorSet::hasPendingEvents() const { return mHasPendingEvent; }

int LightSensorSet::readEvents(sensors_event_t* data, int count) {
  if (count < 1) return -EINVAL;

  if (mHasPendingEvent) {
    mHasPendingEvent = false;
    mPendingEvent.timestamp = getTimestamp();
    *data = mPendingEvent;
    return mEnabled ? 1 : 0;
  }

  ssize_t n = mInputReader.fill(mDevfd);
  if (n < 0) return n;

  int numEventReceived = 0;
  bool valid = false;
  input_event const* event;

  while (count && mInputReader.readEvent(&event)) {
    int type = event->type;
    if ((type == EV_ABS) && (event->value > 0)) {
      if (event->code == ABS_PRESSURE) {
        mPendingEvent.light = event->value;
        valid = true;
        if (LOG_DBG) SLOGD("LightSensor: read value = %f", mPendingEvent.light);
      }
    } else if ((type == EV_SYN) && valid) {
      mPendingEvent.timestamp = timevalToNano(event->time);
      valid = false;
      if (mEnabled) {
        *data++ = mPendingEvent;
        count--;
        numEventReceived++;
      }
    } else {
      LOGE("LightSensor: unknown event (type=%d, code=%d)", type, event->code);
    }
    mInputReader.next();
  }

  return numEventReceived;
}
