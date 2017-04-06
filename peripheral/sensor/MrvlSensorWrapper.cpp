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

#include "MrvlSensorWrapper.h"

/*****************************************************************************/

MrvlSensorWrapper::MrvlSensorWrapper(SensorBase* sensorObj,
                                     struct sensor_t* sensors_list, int num)
    : MrvlSensorBase(NULL, NULL) {
  mDevName = NULL;
  mDevPath = NULL;
  mDevfd = 0;
  sensor_info_list = new sensor_t[num];
  sensor_object = sensorObj;
  memcpy(sensor_info_list, sensors_list, num * sizeof(sensor_t));
  sensor_nums = num;
}

MrvlSensorWrapper::~MrvlSensorWrapper() {
  delete sensor_info_list;
  delete sensor_object;
}

int MrvlSensorWrapper::setDelay(int32_t handle, int64_t ns) {
  return sensor_object->setDelay(handle, ns);
}

int MrvlSensorWrapper::readEvents(sensors_event_t* data, int count) {
  return sensor_object->readEvents(data, count);
}

bool MrvlSensorWrapper::hasPendingEvents() const {
  return sensor_object->hasPendingEvents();
}

int MrvlSensorWrapper::enable(int32_t handle, int enabled) {
  return sensor_object->enable(handle, enabled);
}

int MrvlSensorWrapper::query(int what, int* value) {
  return sensor_object->query(what, value);
}

int MrvlSensorWrapper::batch(int handle, int flags, int64_t period_ns,
                             int64_t timeout) {
  return sensor_object->batch(handle, flags, period_ns, timeout);
}

int MrvlSensorWrapper::flush(int handle) {
  return sensor_object->flush(handle);
}

int MrvlSensorWrapper::getFd() const { return sensor_object->getFd(); }

int MrvlSensorWrapper::populateSensors(Vector<struct sensor_t>& sensorVector) {
  for (int i = 0; i < sensor_nums; i++) {
    SLOGD("populateSensors i is %d, name is %s\n", i, sensor_info_list[i].name);
    sensorVector.push_back(sensor_info_list[i]);
  }

  return sensor_nums;
}
int MrvlSensorWrapper::ifSensorExists() {
  if (sensor_object->getFd() > 0) return 0;
  return -1;
}

/*-------------------------------------------------------------------------------*/

int MrvlSensorWrapper::openInputDevice() { return 0; }

int MrvlSensorWrapper::closeInputDevice() { return 0; }
