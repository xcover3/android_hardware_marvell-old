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

#ifndef ANDROID_PROXIMITY_SENSOR_H
#define ANDROID_PROXIMITY_SENSOR_H

#include <stdint.h>
#include <errno.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <hardware/sensors.h>
#include "MrvlSensorBase.h"
#include "InputEventReader.h"

/*****************************************************************************/

struct input_event;

class ProximitySensor : public MrvlSensorBase {
  int mEnabled;
  InputEventCircularReader mInputReader;
  bool mHasPendingEvent;

 public:
  sensors_event_t mPendingEvent;
  ProximitySensor(const char* name);
  virtual ~ProximitySensor();
  virtual int readEvents(sensors_event_t* data, int count);
  virtual bool hasPendingEvents() const;
  virtual int enable(int32_t handle, int enabled);
  virtual int setDelay(int32_t handle, int64_t ns);
};

class IntersilProximitySensor : public ProximitySensor {
 public:
  static struct sensor_t sBaseSensorList[];
  IntersilProximitySensor();
};

class AvagoProximitySensor : public ProximitySensor {
 public:
  static struct sensor_t sBaseSensorList[];
  AvagoProximitySensor();
};

class TSLProximitySensor : public ProximitySensor {
 public:
  static struct sensor_t sBaseSensorList[];
  TSLProximitySensor();
};

class EplProximitySensor : public ProximitySensor {
 public:
  static struct sensor_t sBaseSensorList[];
  EplProximitySensor();
};

class DynaProximitySensor : public ProximitySensor {
 public:
  static struct sensor_t sBaseSensorList[];
  DynaProximitySensor();
};
/*****************************************************************************/

#endif  // ANDROID_PROXIMITY_SENSOR_H
