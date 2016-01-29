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

#include <hardware/sensors.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <stdlib.h>

#include <linux/input.h>

#include <utils/Atomic.h>
#include <utils/Log.h>
#include <cutils/properties.h>
#include <utils/KeyedVector.h>

#include "SensorBase.h"
#include "MrvlSensorBase.h"
#include "MrvlSensorWrapper.h"

#include "LightSensor.h"
#include "ProximitySensor.h"

#define SENSOR_OBJS_NUM 16

typedef KeyedVector<int, SensorBase *> HandleMap;
static int sSensorNum;
static struct sensor_t *sSensorList;

static int open_sensors(const struct hw_module_t *module, const char *id,
                        struct hw_device_t **device);

/*This function should fill passed in sensor_t const* list with sensor list
  return value is the list count*/
static int sensors__get_sensors_list(struct sensors_module_t *module,
                                     struct sensor_t const **list) {
  *list = sSensorList;
  return sSensorNum;
}

static struct hw_module_methods_t sensors_module_methods = {
  .open = open_sensors
};

struct sensors_module_t HAL_MODULE_INFO_SYM = {
  .common = {
    .tag = HARDWARE_MODULE_TAG,
    .version_major = 1,
    .version_minor = 0,
    .id = SENSORS_HARDWARE_MODULE_ID,
    .name = "Marvell Sensor module",
    .author = "Marvell SEEDS.",
    .methods = &sensors_module_methods,
    .dso = NULL,
    .reserved = {0}
  },
  .get_sensors_list = sensors__get_sensors_list,
  .set_operation_mode = NULL,
};

struct sensors_poll_context_t {
  sensors_poll_device_1_t device;  // must be first

  sensors_poll_context_t();
  ~sensors_poll_context_t();
  int activate(int handle, int enabled);
  int setDelay(int handle, int64_t ns);
  int pollEvents(sensors_event_t *data, int count);
  int query(int what, int *value);
  int batch(int handle, int flags, int64_t period_ns, int64_t timeout);
  int flush(int handle);

 private:
  enum {
    numSensorDrivers,  // wake pipe goes here
    numFds,
  };

  int add_sensor(MrvlSensorBase *sensorObj);
  int prepare_sensor_list();
  SensorBase *handleToSensorObj(int handle);
  Vector<struct sensor_t> sSensorVector;
  HandleMap mHandleToSensor;
  struct pollfd mPollFds[SENSOR_OBJS_NUM];
  SensorBase *mSensorObjs[SENSOR_OBJS_NUM];  // list all sensor objects so
                                             // sensor object can be easily
                                             // accessed by index
  int mSensorIndex;
  int mSensorObjIndex;
};

/******************************************************************************/

int sensors_poll_context_t::prepare_sensor_list() {
  if (sSensorList == NULL) {
    sSensorNum = sSensorVector.size();
    SLOGD("prepare_sensor_list: sSensorVector size %d\n", sSensorNum);
    sSensorList = new sensor_t[sSensorNum];
    for (int i = 0; i < sSensorNum; i++) {
      SLOGD("prepare_sensor_list: sSensorVector i is %d, name is %s\n", i,
            sSensorVector[i].name);
      memcpy(&sSensorList[i], &sSensorVector[i], sizeof(struct sensor_t));
    }
  }

  return sSensorNum;
}

int sensors_poll_context_t::add_sensor(MrvlSensorBase *sensorObj) {
  int index = mSensorObjIndex;

  SLOGD("add_sensor called for index %d", index);
  if (sensorObj->ifSensorExists() == -1) {
    SLOGD("sensor NOT detected, skip...");
    delete sensorObj;
    return -1;
  }
  // first add to mSensorObjs list for later easy access
  mSensorObjs[index] = sensorObj;

  // Add sensor object's fd to polling list
  mPollFds[index].fd = sensorObj->getFd();
  mPollFds[index].events = POLLIN;
  mPollFds[index].revents = 0;

  mSensorObjIndex++;

  SLOGD("add_sensor call populateSensors");
  // populate sensor list, add all sensors to sSensorVector;
  int sensorNum = sensorObj->populateSensors(sSensorVector);
  // record map between handle and Sensor
  for (int i = mSensorIndex; i < mSensorIndex + sensorNum; i++) {
    // SLOGD("add to mHandleToSensor, i is %d, handle %d, sensor object %x\n",
    // i, sSensorVector[i].handle, (unsigned int)sensorObj);
    mHandleToSensor.add(sSensorVector[i].handle, sensorObj);
  }
  // update sensor index to point to the next start sensor
  mSensorIndex = mSensorIndex + sensorNum;

  return 0;
}

sensors_poll_context_t::sensors_poll_context_t() {
  for (int i = 0; i < SENSOR_OBJS_NUM; i++) {
    mSensorObjs[i] = NULL;
    mPollFds[i].fd = -1;
    mPollFds[i].events = POLLIN;
    mPollFds[i].revents = 0;
  }
  memset(&device, 0, sizeof(sensors_poll_device_1));

  mSensorIndex = 0;
  mSensorObjIndex = numSensorDrivers;

  MrvlSensorWrapper *lightSensor = NULL;

#ifdef BOARD_HAVE_AVAGO
  lightSensor = new MrvlSensorWrapper(new AvagoLightSensor(),
                                      AvagoLightSensor::sBaseSensorList, 1);
  add_sensor(lightSensor);
#endif
#ifdef BOARD_HAVE_TSL
  lightSensor = new MrvlSensorWrapper(new TSLLightSensor(),
                                      TSLLightSensor::sBaseSensorList, 1);
  add_sensor(lightSensor);
#endif
#ifdef BOARD_HAVE_ELAN
  lightSensor = new MrvlSensorWrapper(new EplLightSensor(),
                                      EplLightSensor::sBaseSensorList, 1);
  add_sensor(lightSensor);
#endif
#ifdef BOARD_HAVE_DYNA
  lightSensor = new MrvlSensorWrapper(new DynaLightSensor(),
                                      DynaLightSensor::sBaseSensorList, 1);
  add_sensor(lightSensor);
#endif

  MrvlSensorWrapper *proximitySensor = NULL;
#ifdef BOARD_HAVE_INTERSIL
  proximitySensor =
      new MrvlSensorWrapper(new IntersilProximitySensor(),
                            IntersilProximitySensor::sBaseSensorList, 1);
  add_sensor(proximitySensor);
#endif
#ifdef BOARD_HAVE_AVAGO
  proximitySensor = new MrvlSensorWrapper(
      new AvagoProximitySensor(), AvagoProximitySensor::sBaseSensorList, 1);
  add_sensor(proximitySensor);
#endif
#ifdef BOARD_HAVE_TSL
  proximitySensor = new MrvlSensorWrapper(
      new TSLProximitySensor(), TSLProximitySensor::sBaseSensorList, 1);
  add_sensor(proximitySensor);
#endif
#ifdef BOARD_HAVE_ELAN
  proximitySensor = new MrvlSensorWrapper(
      new EplProximitySensor(), EplProximitySensor::sBaseSensorList, 1);
  add_sensor(proximitySensor);
#endif
#ifdef BOARD_HAVE_DYNA
  proximitySensor = new MrvlSensorWrapper(
      new DynaProximitySensor(), DynaProximitySensor::sBaseSensorList, 1);
  add_sensor(proximitySensor);
#endif

  for (unsigned int i = 0; i < sSensorVector.size(); i++) {
    SLOGD("Final sSensorVector: i is %d, name is %s\n", i,
          sSensorVector[i].name);
  }
  for (int i = 0; i < mSensorObjIndex; i++) {
    SLOGD("mSensorObjs: i is %d, obj is %lx\n", i,
          (unsigned long)mSensorObjs[i]);
  }

  prepare_sensor_list();
}

sensors_poll_context_t::~sensors_poll_context_t() {
  for (int i = 0; i < mSensorObjIndex; i++) {
    if (mSensorObjs[i] != NULL) {
      delete mSensorObjs[i];
    }
  }
  for (int i = 0; i < mSensorObjIndex; i++) {
    if (mPollFds[i].fd > 0) {
      close(mPollFds[i].fd);
    }
  }
}

SensorBase *sensors_poll_context_t::handleToSensorObj(int handle) {
  int index = mHandleToSensor.indexOfKey(handle);
  if (index < 0) {
    return NULL;
  }
  return mHandleToSensor.valueAt(index);
}

int sensors_poll_context_t::activate(int handle, int enabled) {
  SensorBase *sensor = handleToSensorObj(handle);
  if (sensor == NULL) {
    LOGE("Cannot find sensor obj for handle %d\n", handle);
    return -1;
  }
  return sensor->enable(handle, enabled);
}

int sensors_poll_context_t::setDelay(int handle, int64_t ns) {
  SensorBase *sensor = handleToSensorObj(handle);
  if (sensor == NULL) return -1;
  return sensor->setDelay(handle, ns);
}

int sensors_poll_context_t::pollEvents(sensors_event_t *data, int count) {
  int nbEvents = 0;
  int nb, polltime = -1;

  nb = poll(mPollFds, mSensorObjIndex, polltime);
  LOGI_IF(0, "poll nb=%d, count=%d, pt=%d", nb, count, polltime);
  if (nb > 0) {
    for (int i = 0; count && i < mSensorObjIndex; i++) {
      if (mPollFds[i].revents & (POLLIN | POLLPRI)) {
        nb = 0;
        if (mSensorObjs[i] != NULL) {
          nb = mSensorObjs[i]->readEvents(data, count);
          if (nb < count) {
            mPollFds[i].revents = 0;
          }
          count -= nb;
          nbEvents += nb;
          data += nb;
          return nbEvents;
        }
        // }
      }
    }

  } else if (nb == 0) {
    LOGE("poll returned with count 0");
  }
  return nbEvents;
}

int sensors_poll_context_t::query(int what, int *value) { return -1; }

int sensors_poll_context_t::batch(int handle, int flags, int64_t period_ns,
                                  int64_t timeout) {
  SensorBase *sensor = handleToSensorObj(handle);
  if (sensor == NULL) {
    LOGE("Cannot find sensor obj for handle %d\n", handle);
    return -1;
  }
  return 0;
}

int sensors_poll_context_t::flush(int handle) {
  SensorBase *sensor = handleToSensorObj(handle);
  if (sensor == NULL) {
    LOGE("Cannot find sensor obj for handle %d\n", handle);
    return -1;
  }
  return -1;
}

/******************************************************************************/

static int poll__close(struct hw_device_t *dev) {
  sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
  if (ctx) {
    delete ctx;
  }
  return 0;
}

static int poll__activate(struct sensors_poll_device_t *dev, int handle,
                          int enabled) {
  sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
  return ctx->activate(handle, enabled);
}

static int poll__setDelay(struct sensors_poll_device_t *dev, int handle,
                          int64_t ns) {
  sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
  int s = ctx->setDelay(handle, ns);
  return s;
}

static int poll__poll(struct sensors_poll_device_t *dev, sensors_event_t *data,
                      int count) {
  sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
  return ctx->pollEvents(data, count);
}

static int poll__query(struct sensors_poll_device_1 *dev, int what,
                       int *value) {
  sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
  return ctx->query(what, value);
}

static int poll__batch(struct sensors_poll_device_1 *dev, int handle, int flags,
                       int64_t period_ns, int64_t timeout) {
  sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
  return ctx->batch(handle, flags, period_ns, timeout);
}

static int poll__flush(struct sensors_poll_device_1 *dev, int handle) {
  sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
  return ctx->flush(handle);
}

/******************************************************************************/

/** Open a new instance of a sensor device using name
 This function should fill passed in sensors_poll_device_1_t* mSensorDevice*/
static int open_sensors(const struct hw_module_t *module, const char *id,
                        struct hw_device_t **device) {
  ALOGV("open_sensors begin...");

  sensors_poll_context_t *dev = new sensors_poll_context_t();

  dev->device.common.tag = HARDWARE_DEVICE_TAG;
  dev->device.common.version = SENSORS_DEVICE_API_VERSION_1_3;
  dev->device.common.module = const_cast<hw_module_t *>(module);
  dev->device.common.close = poll__close;
  dev->device.activate = poll__activate;
  dev->device.setDelay = poll__setDelay;
  dev->device.poll = poll__poll;
  dev->device.batch = poll__batch;
  dev->device.flush = poll__flush;

  *device = &dev->device.common;
  ALOGV("...open_sensors end");

  return 0;
}
