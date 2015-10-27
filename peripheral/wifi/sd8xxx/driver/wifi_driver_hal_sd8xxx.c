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

#define LOG_TAG "wifi_driver_sd8xxx"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cutils/log.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <stddef.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include <hardware_brillo/wifi_driver_hal.h>
#include "marvell_wireless.h"

#define SOCKERR_IO          -1
#define SOCKERR_CLOSED      -2
#define SOCKERR_INVARG      -3
#define SOCKERR_TIMEOUT     -4
#define SOCKERR_OK          0
#define MAX_BUFFER_SIZE    256
#define MAX_DRV_ARG_SIZE   128

static int wireless_send_command(const char *cmd, char *reply, size_t reply_len);
static int cli_conn(const char *name);
static int ap_mode = 0;

static const char *local_socket_dir = "/data/misc/wireless/socket_daemon";
const char kApDeviceName[] = "wlan0";
const char kStationDeviceName[] = "wlan0";
const char kDevicePathPrefix[] = "/sys/class/net";

static wifi_driver_error wifi_driver_initialize_sd8xxx() {
    ALOGI("Do wifi_driver_initialize_sd8xxx()\n");
    if(wifi_enable())
        return WIFI_ERROR_NOT_AVAILABLE;

    return WIFI_SUCCESS;
}

static int interface_exists(const char *interface_name) {
    char interface_path[PATH_MAX];
    sprintf(interface_path, "%s/%s", kDevicePathPrefix, interface_name);
    ALOGI("To test the %s accessible", interface_path);
    return access(interface_path, F_OK) == 0;
}

static wifi_driver_error wifi_driver_set_mode_sd8xxx(wifi_driver_mode mode,
                                                     char *wifi_device_name,
                                                     size_t wifi_device_name_size) {
    const char *device_name = NULL;

    switch (mode) {
    case WIFI_MODE_AP:
        ALOGI("In %s mode and will set to AP mode\n", ap_mode ? "AP" : "STATION");
        device_name = kApDeviceName;
        break;

    case WIFI_MODE_STATION:
        ALOGI("In %s mode and will set to STATION mode\n", ap_mode ? "AP" : "STATION");
        device_name = kStationDeviceName;
        break;

    default:
        ALOGE("Unkonwn WiFi driver mode %d", mode);
        return WIFI_ERROR_INVALID_ARGS;
    }

    strlcpy(wifi_device_name, device_name, wifi_device_name_size);

    if(interface_exists(device_name))
        return WIFI_SUCCESS;

    return WIFI_ERROR_TIMED_OUT;
}

static int close_sd8xxx_driver(struct hw_device_t *device) {
    wifi_driver_device_t *dev = (wifi_driver_device_t *)(device);

    ALOGI("Do close_sd8xxx_driver()\n");
    if(dev)
        free(dev);

    return 0;
}

static int open_sd8xxx_driver(const struct hw_module_t *module,
                              const char *id,
                              struct hw_device_t **device) {
    wifi_driver_device_t *dev = (wifi_driver_device_t *)(
    calloc(1, sizeof(wifi_driver_device_t)));

    ALOGI("Do open_sd8xxx_driver()\n");

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = WIFI_DRIVER_DEVICE_API_VERSION_0_1;
    dev->common.module = (hw_module_t *)(module);
    dev->common.close = close_sd8xxx_driver;
    dev->wifi_driver_initialize = wifi_driver_initialize_sd8xxx;
    dev->wifi_driver_set_mode = wifi_driver_set_mode_sd8xxx;

    *device = &dev->common;
    ap_mode = 0;

    return 0;
}

static struct hw_module_methods_t sd8xxx_driver_module_methods = {
    open: open_sd8xxx_driver,
};

hw_module_t HAL_MODULE_INFO_SYM = {
    tag: HARDWARE_MODULE_TAG,
    version_major: 1,
    version_minor: 0,
    id: WIFI_DRIVER_HARDWARE_MODULE_ID,
    name: "sd8xxx module",
    author: "marvell",
    methods: &sd8xxx_driver_module_methods,
    dso: NULL,
    reserved: {0},
};
