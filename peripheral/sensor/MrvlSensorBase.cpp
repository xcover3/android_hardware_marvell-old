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
#include <stdlib.h>
#include <dirent.h>
#include <sys/select.h>
#include <cutils/log.h>
#include <linux/input.h>

#include <cutils/properties.h>

#include "MrvlSensorBase.h"

/*****************************************************************************/

MrvlSensorBase::MrvlSensorBase(const char *devPath, const char *devName)
    : SensorBase(NULL, NULL), mDevPath(devPath), mDevName(devName), mDevfd(-1) {
  memset(mClassPath, 0, PATH_MAX);
  openInputDevice();
}

MrvlSensorBase::~MrvlSensorBase() {
  if (mDevfd) {
    close(mDevfd);
  }
}

int MrvlSensorBase::setDelay(int32_t handle, int64_t ns) { return 0; }

bool MrvlSensorBase::hasPendingEvents() const { return false; }

int MrvlSensorBase::enable(int32_t handle, int enabled) { return 0; }

int MrvlSensorBase::query(int what, int *value) { return 0; }

int MrvlSensorBase::batch(int handle, int flags, int64_t period_ns,
                          int64_t timeout) {
  return 0;
}

int MrvlSensorBase::flush(int handle) { return 0; }

int MrvlSensorBase::getFd() const { return mDevfd; }

int MrvlSensorBase::populateSensors(Vector<struct sensor_t> &sensorVector) {
  return 0;
}
int MrvlSensorBase::ifSensorExists() {
  if (mDevfd > 0) return 0;
  return -1;
}

/*-------------------------------------------------------------------------------*/

int MrvlSensorBase::openInputDevice() {
  // first try open directly by input device path
  if (mDevPath) {
    mDevfd = openDeviceByPath();
  }

  // then try by device name
  if (mDevfd <= 0 && mDevName) {
    mDevfd = openDeviceByName();
  }

  return 0;
}
/*This function searches through /sys/class/input/inputX to
 find matching name. If name is matched, corresponding /dev/input/eventX
 will be opened*/
int MrvlSensorBase::openDeviceByName() {
  const char *dirname = "/dev/input";
  char *filename;
  DIR *dir;
  struct dirent *de;
  char sysNamePath[PATH_MAX];
  char devName[PATH_MAX];
  int sysfd = -1;
  int devfd = -1;
  int ret;

  strcpy(mCalculatedDevPath, dirname);
  filename = mCalculatedDevPath + strlen(mCalculatedDevPath);
  *filename++ = '/';

  if (!(dir = opendir(INPUT_CLASS_PATH))) {
    LOGE("Directory %s open failed.", INPUT_CLASS_PATH);
    return -1;
  }

  while ((de = readdir(dir))) {
    if (strncmp(de->d_name, "input", strlen("input")) != 0) {
      continue;
    }

    snprintf(sysNamePath, sizeof(sysNamePath), "%s/%s/name", INPUT_CLASS_PATH,
             de->d_name);

    if ((sysfd = open(sysNamePath, O_RDONLY)) < 0) {
      LOGE("File %s open failed.", sysNamePath);
      continue;
    }

    if ((ret = read(sysfd, devName, sizeof(devName))) <= 0) {
      close(sysfd);
      continue;
    }
    close(sysfd);

    devName[ret - 1] = '\0';
    if (strcmp(devName, mDevName) == 0) {
      /*
       ** found the device name under
       ** "SYS_CLASS_PATH/inputxx/name"
       */
      // first save the sys path
      snprintf(mClassPath, sizeof(mClassPath), "%s/%s", INPUT_CLASS_PATH,
               de->d_name);

      // then find and open corresponding dev path
      char eventName[PATH_MAX];
      if (strlen(de->d_name) > 5) {
        sprintf(eventName, "event%s", de->d_name + 5);
        strcpy(filename, eventName);
        devfd = open(mCalculatedDevPath, O_RDONLY);
        LOGI("path open %s", mCalculatedDevPath);
      }
      break;
    }
  }
  closedir(dir);
  return devfd;
}

int MrvlSensorBase::openDeviceByPath() {
  mDevfd = open(mDevPath, O_RDONLY);
  LOGE_IF(mDevfd < 0, "Couldn't open %s (%s)", mDevPath, strerror(errno));

  return mDevfd;
}

int MrvlSensorBase::closeInputDevice() {
  if (mDevfd >= 0) {
    close(mDevfd);
    mDevfd = -1;
  }
  return 0;
}
