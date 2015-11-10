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

#include "ProximitySensor.h"
#include "sensors_hal.h"

/*****************************************************************************/

#define LOG_DBG 1

struct sensor_t IntersilProximitySensor::sBaseSensorList[] = {
    {"Intersil tech Proximity",
     "Intersial",
     1,
     ISL_SENSORS_PROXIMITY_HANDLE,
     SENSOR_TYPE_PROXIMITY,
     10.0f,
     1.0f,
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

IntersilProximitySensor::IntersilProximitySensor()
    : ProximitySensor("ISL_proximity_sensor") {
  ProximitySensor::mPendingEvent.sensor = ISL_ID_PRO;
}

/************************************************************************************************************/
struct sensor_t AvagoProximitySensor::sBaseSensorList[] = {
    {"Avago tech Proximity",
     "Avago",
     1,
     AVAGO_SENSORS_PROXIMITY_HANDLE,
     SENSOR_TYPE_PROXIMITY,
     10.0f,
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

AvagoProximitySensor::AvagoProximitySensor()
    : ProximitySensor("APDS_proximity_sensor") {
  ProximitySensor::mPendingEvent.sensor = AVAGO_ID_PRO;
}

/*****************************************************************************************************************/
struct sensor_t TSLProximitySensor::sBaseSensorList[] = {
    {"TSL2x7x tech Proximity",
     "Taos",
     1,
     TSL_SENSORS_PROXIMITY_HANDLE,
     SENSOR_TYPE_PROXIMITY,
     5.0f,
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

TSLProximitySensor::TSLProximitySensor()
    : ProximitySensor("TSL_proximity_sensor") {
  ProximitySensor::mPendingEvent.sensor = TSL_ID_PRO;
}

/*****************************************************************************************************************/
struct sensor_t EplProximitySensor::sBaseSensorList[] = {
    {"ELAN Proximity sensor",
     "ELAN",
     1,
     ELAN_SENSORS_PROXIMITY_HANDLE,
     SENSOR_TYPE_PROXIMITY,
     1.0f,
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

EplProximitySensor::EplProximitySensor()
    : ProximitySensor("elan_proximity_sensor") {
  ProximitySensor::mPendingEvent.sensor = ELAN_ID_PRO;
}

/*****************************************************************************************************************/
struct sensor_t DynaProximitySensor::sBaseSensorList[] = {
    {"DYNA Proximity sensor",
     "DYNA",
     1,
     DYNA_SENSORS_PROXIMITY_HANDLE,
     SENSOR_TYPE_PROXIMITY,
     10.0f,
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

DynaProximitySensor::DynaProximitySensor()
    : ProximitySensor("AP3426_proximity_sensor") {
  ProximitySensor::mPendingEvent.sensor = DYNA_ID_PRO;
}

/*****************************************************************************************************************/

ProximitySensor::ProximitySensor(const char* name)
    : MrvlSensorBase(NULL, name),
      mEnabled(0),
      mInputReader(4),
      mHasPendingEvent(false) {
  mPendingEvent.version = sizeof(sensors_event_t);
  mPendingEvent.type = SENSOR_TYPE_PROXIMITY;
  memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));

  if (LOG_DBG) SLOGD("ProximitySensor: sysfs[%s]\n", mClassPath);
}

ProximitySensor::~ProximitySensor() {}

int ProximitySensor::setDelay(int32_t handle, int64_t ns) {
  if (mEnabled) {
    int fd;
    char intervalPath[PATH_MAX];
    sprintf(intervalPath, "%s/%s", mClassPath, "interval");
    fd = open(intervalPath, O_RDWR);
    if (fd >= 0) {
      char buf[80];
      sprintf(buf, "%lld", ns);
      write(fd, buf, strlen(buf) + 1);
      close(fd);
      return 0;
    }
  }
  return -1;
}

int ProximitySensor::enable(int32_t, int en) {
  int flags = en ? 1 : 0;
  if (flags != mEnabled) {
    int fd;
    char enablePath[PATH_MAX];
    sprintf(enablePath, "%s/%s", mClassPath, "active");
    if (LOG_DBG) SLOGD("ProximitySensor enable path is %s", enablePath);
    fd = open(enablePath, O_RDWR);
    if (fd >= 0) {
      char buf[2];
      buf[1] = '\n';
      if (flags) {
        buf[0] = '1';
      } else {
        buf[0] = '0';
      }
      write(fd, buf, sizeof(buf));
      close(fd);
      mEnabled = flags;
      return 0;
    }

    LOGE("Error while open %s", enablePath);
    return -1;
  }
  return 0;
}

bool ProximitySensor::hasPendingEvents() const { return mHasPendingEvent; }

int ProximitySensor::readEvents(sensors_event_t* data, int count) {
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
  input_event const* event;

  while (count && mInputReader.readEvent(&event)) {
    int type = event->type;
    if (type == EV_ABS) {
      if (event->code == ABS_DISTANCE) {
        mPendingEvent.distance = event->value;
        if (LOG_DBG)
          SLOGD("ProximitySensor: read value = %f", mPendingEvent.distance);
      }
    } else if (type == EV_SYN) {
      mPendingEvent.timestamp = timevalToNano(event->time);
      if (mEnabled) {
        *data++ = mPendingEvent;
        count--;
        numEventReceived++;
      }
    } else {
      LOGE("ProximitySensor: unknown event (type=%d, code=%d)", type,
           event->code);
    }
    mInputReader.next();
  }

  return numEventReceived;
}
