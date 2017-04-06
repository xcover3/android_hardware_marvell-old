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

#ifndef ANDROID_MRVL_SENSOR_WRAPPER_H
#define ANDROID_MRVL_SENSOR_WRAPPER_H

#include <stdint.h>
#include <errno.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <utils/Vector.h>
#include "MrvlSensorBase.h"

using namespace android;

class MrvlSensorWrapper : public MrvlSensorBase {
 protected:
  char mCalculatedDevPath[PATH_MAX];
  char mClassPath[PATH_MAX];
  const char* mDevPath;
  const char* mDevName;
  int mDevfd;
  SensorBase* sensor_object;
  struct sensor_t* sensor_info_list;
  int sensor_nums;

  int openInputDevice();
  int openDeviceByPath();
  int openDeviceByName();
  int closeInputDevice();

 public:
  MrvlSensorWrapper(SensorBase* sensorObj, struct sensor_t* sensors_list,
                    int num);
  ~MrvlSensorWrapper();

  virtual int readEvents(sensors_event_t* data, int count);
  virtual bool hasPendingEvents() const;
  virtual int getFd() const;
  virtual int setDelay(int32_t handle, int64_t ns);
  virtual int enable(int32_t handle, int enabled);
  virtual int query(int what, int* value);
  virtual int batch(int handle, int flags, int64_t period_ns, int64_t timeout);
  virtual int flush(int handle);

  virtual int populateSensors(Vector<struct sensor_t>& sensorVector);
  virtual int ifSensorExists();
};

/*****************************************************************************/

#endif  // ANDROID_MRVL_SENSOR_WRAPPER_H
