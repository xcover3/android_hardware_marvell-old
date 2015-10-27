/*
 * Copyright (C)  2015. Marvell International Ltd
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

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>

#include <utils/Log.h>
#include <hardware/hardware.h>
#include <hardware/power.h>

#define LOG_TAG "PowerHAL"

#define POWER_HAL_CONF "/etc/powerhal.conf"

#define DEV_MAX 256
#define ERR_MAX 64
#define STATUS_MAX 32

static const char pm_control[] = "control";
static const char pm_status[] = "runtime_status";
static const char pm_state_auto[] = "auto";
static const char pm_state_on[] = "on";

struct node {
    char device[DEV_MAX];
    char status[DEV_MAX];
    int fd;
    struct node *next;
    struct node *prev;
};

struct list {
    struct node *first;
    struct node *last;
};

static struct list devices;

static int add_to_list(const char *device, int fd)
{
    struct node *node = malloc(sizeof(struct node));
    if (node == NULL) {
        ALOGE("Malloc failed for creating new node");
        return -1;
    }

    memset(node, 0, sizeof(struct node));
    if ((strlcpy(node->device, device,
        sizeof(node->device))) >= sizeof(node->device)) {
        ALOGE("Invalid name of the device %s", device);
        free(node);
        return -1;
    }

    int n = snprintf(node->status, sizeof(node->status),
                     "%s/%s", device, pm_status);
    if (n < 0 || n >= sizeof(node->status)) {
        ALOGE("Invalid status of the device %s", device);
        free(node);
        return -1;
    }

    node->fd = fd;

    if (devices.first == NULL) {
        devices.first = devices.last = node;
    } else {
        devices.last->next = node;
        node->prev = devices.last;
        devices.last = node;
    }

    return 0;
}

/**
 * Return 0 if the device is PM supported, or -1 if unsupported.
 */
static int is_pm_supported(const char *control, char *status)
{
    char buf[ERR_MAX] = {0};
    char sbuf[STATUS_MAX] = {0};

    int fd = open(status, O_RDONLY);
    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGW("Error opening %s: %s", status, buf);
        return -1;
    }

    int ret = read(fd, sbuf, sizeof(sbuf) - 1);
    close(fd);

    if (ret < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGW("Error reading %s: %s", status, buf);
        return -1;
    }

    if (strcmp(sbuf, "unsupported") == 0) {
        ALOGW("%s is unsupported", status);
        return -1;
    }

    return 0;
}

static void wait_for_set_complete(struct node *node, int on)
{
    char buf[80] = {0};
    char sbuf[64] = {0};
    int count = 1000;
    const char *status = node->status;
    const char *device = node->device;

    do {
        int fd = open(status, O_RDONLY);
        if (fd < 0) {
            strerror_r(errno, buf, sizeof(buf));
            ALOGE("Error opening %s of %s: %s",
                  status, device, buf);
            return;
        }

        int ret = read(fd, sbuf, sizeof(sbuf) - 1);
        close(fd);

        if (ret < 0) {
            strerror_r(errno, buf, sizeof(buf));
            ALOGE("Error reading %s of %s: %s", status, device, buf);
            continue;
        }

        if (strcmp(sbuf, "unsupported") == 0) {
            ALOGE("%s of %s is unsupported", status, device);
            break;
        }

        if (strcmp(sbuf, on ? "active" : "suspended") == 0) {
            ALOGI("Set %s active/suspend successfully, "
                  "current status is %s", device, sbuf);
            break;
        }

        usleep(1000);
    } while (--count > 0);

    if (count == 0) {
        ALOGE("Device %s suspend/resume failed", device);
        return;
    }
}

static int pm_set_for_device(struct node *node, int on)
{
    char buf[ERR_MAX] = {0};

    const char *pm_value = on ? pm_state_on : pm_state_auto;

    int ret = write(node->fd, pm_value, strlen(pm_value));
    if (ret != strlen(pm_value)) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing %s to %s: %s",
              pm_value, node->device, buf);
        return -1;
    }

    ALOGD("Set %s to %s done", node->device, pm_value);

    return 0;
}

static void power_init(struct power_module *module)
{
    FILE *conf = fopen(POWER_HAL_CONF, "r");

    if (conf == NULL) {
        ALOGE("%s does not exist! No devices for "
              "interactive mode power management", POWER_HAL_CONF);
        return;
    }

    char line[DEV_MAX] = {0};
    char control[DEV_MAX] = {0};
    char status[DEV_MAX] = {0};
    char buf[ERR_MAX] = {0};
    int fd = -1;

    while (fgets(line, DEV_MAX, conf) != NULL) {
        if (line[0] == '#' || line[0] == '\n' ||
            line[0] == ' ' || line[0] == '\0')
            continue;

        size_t line_len = strlen(line);
        if (line[line_len - 1] == '\n') {
            line[line_len - 1] = '\0';
            line_len--;
        }

        int n = snprintf(control, sizeof(control), "%s/%s", line, pm_control);
        if (n < 0 || n >= sizeof(control)) {
            ALOGE("Error copying control of the device %s", line);
            continue;
        }

        n = snprintf(status, sizeof(status), "%s/%s", line, pm_status);
        if (n < 0 || n >= sizeof(status)) {
            ALOGE("Error copying status of the device %s", line);
            continue;
        }

        if (is_pm_supported(control, status) != 0) {
            ALOGE("Device %s does not support PM", line);
            continue;
        }

        fd = open(control, O_RDWR);
        if (fd < 0) {
            strerror_r(errno, buf, sizeof(buf));
            ALOGE("Error opening %s: %s", control, buf);
            continue;
        }

        ALOGD("Add device %s to list", line);
        if (add_to_list(line, fd) != 0) {
            ALOGE("Add device %s to list failed", line);
            close(fd);
        }
    }

    fclose(conf);

    return;
}

static void power_set_interactive(struct power_module *module, int on)
{
    struct node *node;

    ALOGI("power_set_interactive called with on %d", on);
    if (on) {
        for (node = devices.last; node != NULL; node = node->prev)
            pm_set_for_device(node, on);
    } else {
        for (node = devices.first; node != NULL; node = node->next)
            pm_set_for_device(node, on);
    }

    for (node = devices.last; node != NULL; node = node->prev)
        wait_for_set_complete(node, on);

    return;
}

static void power_hint(struct power_module *module,
                       power_hint_t hint, void *data)
{
}

static struct hw_module_methods_t power_module_methods = {
    .open = NULL,
};

struct power_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = POWER_MODULE_API_VERSION_0_2,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = POWER_HARDWARE_MODULE_ID,
        .name = "Marvell Power HAL",
        .author = "Marvell SEEDS",
        .methods = &power_module_methods,
    },

    .init = power_init,
    .setInteractive = power_set_interactive,
    .powerHint = power_hint,
};
