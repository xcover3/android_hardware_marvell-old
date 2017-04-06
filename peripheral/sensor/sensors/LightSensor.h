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

#ifndef ANDROID_LIGHT_SENSOR_H
#define ANDROID_LIGHT_SENSOR_H

#include <stdint.h>
#include <errno.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <hardware/sensors.h>
#include "MrvlSensorBase.h"
#include "sensors_hal.h"
#include "InputEventReader.h"

/*****************************************************************************/

struct inputnevent;

class LightSensorSet : public MrvlSensorBase {
  int mEnabled;
  InputEventCircularReader mInputReader;
  bool mHasPendingEvent;
  // sensors_event_t mPendingEvent;
  int setInitialState();

 public:
  sensors_event_t mPendingEvent;
  LightSensorSet(const char* name);
  virtual ~LightSensorSet();
  virtual int readEvents(sensors_event_t* data, int count);
  virtual bool hasPendingEvents() const;
  virtual int setDelay(int32_t handle, int64_t ns);
  virtual int enable(int32_t handle, int enabled);
};

class IntersilLightSensor : public LightSensorSet {
 public:
  static struct sensor_t sBaseSensorList[];
  IntersilLightSensor();
};

class AvagoLightSensor : public LightSensorSet {
 public:
  static struct sensor_t sBaseSensorList[];
  AvagoLightSensor();
};

class TSLLightSensor : public LightSensorSet {
 public:
  static struct sensor_t sBaseSensorList[];
  TSLLightSensor();
};

class EplLightSensor : public LightSensorSet {
 public:
  static struct sensor_t sBaseSensorList[];
  EplLightSensor();
};
class DynaLightSensor : public LightSensorSet {
 public:
  static struct sensor_t sBaseSensorList[];
  DynaLightSensor();
};
/*****************************************************************************/

#endif  // ANDROID_LIGHT_SENSOR_H
