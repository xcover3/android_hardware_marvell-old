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

#define LOG_TAG "audio_hw_config"
#define LOG_NDEBUG 0

#include <stdio.h>
#include <hardware/hardware.h>
#include <hardware/audio.h>
#include <system/audio.h>
#include <cutils/log.h>
#include <expat.h>

#include "acm_api.h"
#include "audio_path.h"

#define AUDIO_PLATFORM_CONFIG_FILE "/etc/platform_audio_config.xml"
#define DEVICE_NAME_LEN_MAX 32

struct app_cfg_t {
  virtual_mode_t v_mode;
  unsigned int device;  // support multi configurations
  struct app_cfg_t *next;
};

struct android_dev_cfg_t {
  unsigned int android_dev;
  struct app_cfg_t *app_cfg;
  struct android_dev_cfg_t *next;
};

struct board_dev_cfg_t {
  unsigned int hw_dev;
  unsigned int connectivity;  // connectivity type
  unsigned int coupling;      // coupling type
  struct board_dev_cfg_t *next;
};

struct device_name_t {
  char name[DEVICE_NAME_LEN_MAX];
  int len;
};

struct platform_config_t {
  struct board_dev_cfg_t *board_dev_cfg;    // support board device list
  struct android_dev_cfg_t *droid_dev_cfg;  // support android device list

  struct board_dev_cfg_t *current_board_device;
  struct device_name_t current_device_name;
  struct android_dev_cfg_t *current_droid_device;
  struct app_cfg_t *current_app_cfg;
};

typedef enum {
  PARSING_OK = 0,
  PARSING_UNKNOWN_ERROR,
  PARSING_ERROR_IO,
  PARSING_ERROR_MALFORMED,
} parsing_check_t;

typedef enum {
  SECTION_UNKNOWN = -1,
  SECTION_TOP_LEVEL,
  SECTION_BOARD_DEVICE_LIST,
  SECTION_ANDROID_DEVICE,
  SECTION_APPLICATION,
  SECTION_DEVICE_IN_BOARD,
  SECTION_DEVICE_IN_ANDROID,
} section_layout_t;

static struct platform_config_t *mrvl_platform_cfg = NULL;
static section_layout_t current_section = SECTION_UNKNOWN;

// get mic device from platform_audio_config.xml config
unsigned int get_mic_dev(virtual_mode_t v_mode, unsigned int android_dev) {
  struct android_dev_cfg_t *droiddev_cfg = mrvl_platform_cfg->droid_dev_cfg;
  unsigned int default_dev = HWDEV_INVALID;

  while (droiddev_cfg) {
    if (droiddev_cfg->android_dev == android_dev) {
      struct app_cfg_t *app_cfg = droiddev_cfg->app_cfg;
      while (app_cfg) {
        if (app_cfg->v_mode == v_mode) {
          ALOGD("%s: find matched dev 0x%x", __FUNCTION__, app_cfg->device);
          return app_cfg->device;
        } else if (app_cfg->v_mode == V_MODE_DEF) {
          default_dev = app_cfg->device;
        }
        app_cfg = app_cfg->next;
      }

      // if none matched app found, use default device
      ALOGD("%s: cannot find matched app, use default dev 0x%x", __FUNCTION__,
            default_dev);
      return default_dev;
    }

    droiddev_cfg = droiddev_cfg->next;
  }
  // return default device
  return HWDEV_AMIC1;
}

// get mic hw flag for connectivity and coupling
unsigned int get_mic_hw_flag(unsigned int hw_dev) {
  unsigned int flags = 0;
  struct board_dev_cfg_t *dev_cfg = mrvl_platform_cfg->board_dev_cfg;

  // for TTY, use the equivalent device
  switch (hw_dev) {
    case HWDEV_IN_TTY:
      hw_dev = HWDEV_HSMIC;
      break;
    case HWDEV_IN_TTY_VCO_DUAL_AMIC:
      hw_dev = HWDEV_DUAL_AMIC;
      break;
    case HWDEV_IN_TTY_VCO_DUAL_AMIC_SPK_MODE:
      hw_dev = HWDEV_DUAL_AMIC_SPK_MODE;
      break;
    case HWDEV_IN_TTY_VCO_DUAL_DMIC1:
      hw_dev = HWDEV_DUAL_DMIC1;
      break;
    case HWDEV_IN_TTY_VCO_AMIC1:
      hw_dev = HWDEV_AMIC1;
      break;
    case HWDEV_IN_TTY_VCO_AMIC2:
      hw_dev = HWDEV_AMIC2;
      break;
    default:
      // keep the current hw device.
      break;
  }

  while (dev_cfg) {
    if (dev_cfg->hw_dev == hw_dev) {
      flags = (dev_cfg->coupling | dev_cfg->connectivity);
    }
    dev_cfg = dev_cfg->next;
  }
  return flags;
}

static void get_android_dev_by_user_selection(char *dev_name) {
  uint32_t mic_mode = get_mic_mode();

  ALOGD("%s mic_mode= %d", __FUNCTION__, mic_mode);

  switch (mic_mode) {
    case MIC_MODE_MIC1:
    case MIC_MODE_MIC2:
    case MIC_MODE_DUALMIC:
      if (strstr(dev_name, "_SPK_MODE")) {
        strcpy(dev_name, mic_mode_to_dev_name[mic_mode]);
        strcat(dev_name, "_SPK_MODE");
      } else
        strcpy(dev_name, mic_mode_to_dev_name[mic_mode]);
      break;

    case MIC_MODE_NONE:
    default:
      break;
  }
}

static unsigned int get_android_dev_byname(char *dev_name) {
  if (!strcmp(dev_name, "AUDIO_DEVICE_IN_BUILTIN_MIC")) {
    return AUDIO_DEVICE_IN_BUILTIN_MIC;
  } else if (!strcmp(dev_name, "AUDIO_DEVICE_IN_BACK_MIC")) {
    return AUDIO_DEVICE_IN_BACK_MIC;
  }
  return 0;
}

static virtual_mode_t get_mode_byname(char *app_name) {
  int i = 0;

  for (i = 0; i < (int)(sizeof(vtrl_mode_name) / sizeof(char *)); i++) {
    if (!strcmp(vtrl_mode_name[i], app_name)) {
      return i;
    }
  }
  return V_MODE_INVALID;
}

static unsigned int get_hwdev_byname(char *dev_name) {
  int i = 0;

  get_android_dev_by_user_selection(dev_name);  // Change the dev_name according
                                                // to the User-Selection (if
                                                // needed)

  for (i = 0; i < (int)(sizeof(input_devname) / sizeof(char *)); i++) {
    if (!strcmp(input_devname[i], dev_name)) {
      return (HWDEV_BIT_IN | (HWDEV_IN_BASE << i));
    }
  }
  return HWDEV_INVALID;
}

static void parseBoardDevice(struct platform_config_t *config_data,
                             const char **attrs) {
  struct board_dev_cfg_t *dev_cfg;
  const char *connectivity = NULL;
  const char *coupling = NULL;
  int i = 0;

  dev_cfg = (struct board_dev_cfg_t *)calloc(1, sizeof(struct board_dev_cfg_t));

  if (dev_cfg == NULL) {
    ALOGE("%s/L%d: out of memory", __FUNCTION__, __LINE__);
    return;
  }

  while (attrs[i] != NULL) {
    if (strcmp("connectivity", attrs[i]) == 0) {
      connectivity = attrs[i + 1];
      ++i;
    } else if (strcmp("coupling", attrs[i]) == 0) {
      coupling = attrs[i + 1];
    }

    ++i;
  }

  if (connectivity != NULL) {
    ALOGD("%s: connectivity = \"%s\"", __FUNCTION__, connectivity);
    if (!strcmp(connectivity, "diff")) {
      dev_cfg->connectivity = CONNECT_DIFF;
    } else if (!strcmp(connectivity, "quasi_diff")) {
      dev_cfg->connectivity = CONNECT_QUASI_DIFF;
    } else if (!strcmp(connectivity, "single_ended")) {
      dev_cfg->connectivity = CONNECT_SINGLE_ENDED;
    } else {
      dev_cfg->connectivity = CONNECT_UNKNOWN;
    }
  } else {
    ALOGE("[device in board] failed to find the attribute connectivity");
    return;
  }

  if (coupling != NULL) {
    ALOGD("%s: coupling = \"%s\"", __FUNCTION__, coupling);
    if (!strcmp(coupling, "ac")) {
      dev_cfg->coupling = COUPLING_AC;
    } else if (!strcmp(coupling, "dc")) {
      dev_cfg->coupling = COUPLING_DC;
    } else {
      dev_cfg->coupling = COUPLING_UNKNOWN;
    }
  } else {
    ALOGE("[device in board] failed to find the attribute coupling");
    return;
  }

  config_data->current_board_device = dev_cfg;
}

static void finishBoardDevice(struct platform_config_t *config_data,
                              char *device_name) {
  ALOGD("%s: find board config device %s", __FUNCTION__, device_name);

  if (config_data->current_board_device != NULL) {
    config_data->current_board_device->hw_dev = get_hwdev_byname(device_name);

    if (config_data->board_dev_cfg == NULL) {
      config_data->board_dev_cfg = config_data->current_board_device;
    } else {
      struct board_dev_cfg_t *last_dev_cfg = config_data->board_dev_cfg;
      while (last_dev_cfg->next) last_dev_cfg = last_dev_cfg->next;
      last_dev_cfg->next = config_data->current_board_device;
    }
    config_data->current_board_device = NULL;
  } else {
    ALOGE("%s:%d current_board_device is null", __FUNCTION__, __LINE__);
  }
}

static void addAndroidDevice(struct platform_config_t *config_data,
                             const char **attrs) {
  // Save Android Device information, support multi devices config
  struct android_dev_cfg_t *droid_dev_cfg;
  const char *android_dev = NULL;

  droid_dev_cfg =
      (struct android_dev_cfg_t *)calloc(1, sizeof(struct android_dev_cfg_t));

  if (droid_dev_cfg == NULL) {
    ALOGE("%s/L%d: out of memory", __FUNCTION__, __LINE__);
    return;
  }

  if (strcmp("identifier", attrs[0]) == 0) {
    android_dev = attrs[1];
  }

  if (android_dev != NULL) {
    droid_dev_cfg->android_dev = get_android_dev_byname((char *)android_dev);
    ALOGD("%s: find android dev identifier %s", __FUNCTION__, android_dev);
  } else {
    ALOGE(
        "Can't find the identifier for AndroidDevice. attrs[0][%s], "
        "attrs[1][%s]",
        attrs[0], attrs[1]);
  }

  // add android device config to list
  if (config_data->droid_dev_cfg == NULL) {
    config_data->droid_dev_cfg = droid_dev_cfg;
  } else {
    struct android_dev_cfg_t *last_path_cfg = config_data->droid_dev_cfg;
    while (last_path_cfg->next) last_path_cfg = last_path_cfg->next;
    last_path_cfg->next = droid_dev_cfg;
  }

  config_data->current_droid_device = droid_dev_cfg;
}

static void addAppToAndroidDevice(struct platform_config_t *config_data,
                                  const char **attrs) {
  const char *app_name = NULL;
  struct app_cfg_t *app_cfg = NULL;

  app_cfg = (struct app_cfg_t *)calloc(1, sizeof(struct app_cfg_t));

  if (app_cfg == NULL) {
    ALOGE("%s/L%d: out of memory", __FUNCTION__, __LINE__);
    return;
  }

  if (strcmp("identifier", attrs[0]) == 0) {
    app_name = attrs[1];
  }

  if (app_name != NULL) {
    ALOGD("%s: find app config, identifier %s", __FUNCTION__, app_name);
    app_cfg->v_mode = get_mode_byname((char *)app_name);
  } else {
    ALOGE(
        "Can't find the identifier for Application. attrs[0][%s], attrs[1][%s]",
        attrs[0], attrs[1]);
    return;
  }

  if (config_data->current_droid_device != NULL) {
    if (config_data->current_droid_device->app_cfg == NULL) {
      config_data->current_droid_device->app_cfg = app_cfg;
    } else {
      struct app_cfg_t *last_app_cfg =
          config_data->current_droid_device->app_cfg;
      while (last_app_cfg->next) last_app_cfg = last_app_cfg->next;
      last_app_cfg->next = app_cfg;
    }
  } else {
    ALOGE("%s:%d current_droid_device is null", __FUNCTION__, __LINE__);
    return;
  }

  config_data->current_app_cfg = app_cfg;
}

static void finishAppDroidDevice(struct platform_config_t *config_data,
                                 char *device_name) {
  if (device_name != NULL) {
    ALOGD("%s: find android device  %s", __FUNCTION__, device_name);
    config_data->current_app_cfg->device |= get_hwdev_byname(device_name);
  } else {
    ALOGE("%s: device name is NULL", __FUNCTION__);
  }
}

static void XMLCALL
startElementHandler(void *userData, const char *name, const char **attr) {
  struct platform_config_t *config_data;

  config_data = (struct platform_config_t *)userData;

  if (strcmp("MarvellPlatformAudioConfiguration", name) == 0) {
    current_section = SECTION_TOP_LEVEL;
  } else if (strcmp("BoardDeviceList", name) == 0) {
    if (current_section == SECTION_TOP_LEVEL) {
      current_section = SECTION_BOARD_DEVICE_LIST;
    } else {
      ALOGE("Wrong section(%d) in parsing to section: BoardDeviceList",
            current_section);
      current_section = SECTION_UNKNOWN;
    }
  } else if (strcmp("AndroidDevice", name) == 0) {
    if (current_section == SECTION_TOP_LEVEL) {
      addAndroidDevice(config_data, attr);
      current_section = SECTION_ANDROID_DEVICE;
    } else {
      ALOGE("Wrong section(%d) in parsing to section: AndroidDevice",
            current_section);
      current_section = SECTION_UNKNOWN;
    }
  } else if (strcmp("Application", name) == 0) {
    if (current_section == SECTION_ANDROID_DEVICE) {
      addAppToAndroidDevice(config_data, attr);
      current_section = SECTION_APPLICATION;
    } else {
      ALOGE("Wrong section(%d) in parsing to section: Application",
            current_section);
      current_section = SECTION_UNKNOWN;
    }
  } else if (strcmp("Device", name) == 0) {
    if (current_section == SECTION_BOARD_DEVICE_LIST) {
      current_section = SECTION_DEVICE_IN_BOARD;
      parseBoardDevice(config_data, attr);
    } else if (current_section == SECTION_APPLICATION) {
      current_section = SECTION_DEVICE_IN_ANDROID;
    }
  } else {
    ALOGE("Wrong element(%s), current section is %d", name, current_section);
  }
}

static void XMLCALL endElementHandler(void *userData, const char *name) {
  struct platform_config_t *config_data;

  config_data = (struct platform_config_t *)userData;
  if (strcmp("Application", name) == 0) {
    current_section = SECTION_ANDROID_DEVICE;
    config_data->current_device_name
        .name[config_data->current_device_name.len] = '\0';
    finishAppDroidDevice(config_data, config_data->current_device_name.name);
    config_data->current_device_name.len = 0;
    config_data->current_app_cfg = NULL;
  } else if (strcmp("AndroidDevice", name) == 0) {
    current_section = SECTION_TOP_LEVEL;
    config_data->current_droid_device = NULL;
  } else if (strcmp("BoardDeviceList", name) == 0) {
    current_section = SECTION_TOP_LEVEL;
  } else if (strcmp("Device", name) == 0) {
    if (current_section == SECTION_DEVICE_IN_BOARD) {
      current_section = SECTION_BOARD_DEVICE_LIST;
      config_data->current_device_name
          .name[config_data->current_device_name.len] = '\0';
      finishBoardDevice(config_data, config_data->current_device_name.name);
      config_data->current_device_name.len = 0;
    } else if (current_section == SECTION_DEVICE_IN_ANDROID) {
      current_section = SECTION_APPLICATION;
    }
  }
}

static void XMLCALL
characterDataHandler(void *userData, const XML_Char *s, int len) {
  struct platform_config_t *config_data;

  config_data = (struct platform_config_t *)userData;

  if (current_section == SECTION_DEVICE_IN_BOARD ||
      current_section == SECTION_DEVICE_IN_ANDROID) {
    strncpy(config_data->current_device_name.name +
                config_data->current_device_name.len,
            s, len);
    config_data->current_device_name.len += len;
  }
}

int init_platform_config() {
  char *config_file_name = AUDIO_PLATFORM_CONFIG_FILE;
  XML_Parser parser;
  FILE *file;
  parsing_check_t parsing_check = PARSING_OK;

  file = fopen(config_file_name, "r");

  if (file == NULL) {
    ALOGE("unable to open platform audio configuration xml file: %s",
          config_file_name);
    return -1;
  } else {
    ALOGI("%s: config file %s", __FUNCTION__, config_file_name);
  }

  mrvl_platform_cfg =
      (struct platform_config_t *)calloc(1, sizeof(struct platform_config_t));

  if (mrvl_platform_cfg == NULL) {
    ALOGE("%s/L%d: out of memory", __FUNCTION__, __LINE__);
    fclose(file);
    return -1;
  }
  ALOGI(
      "current_board_device = %p, current_droid_device=%p, device name len=%d",
      mrvl_platform_cfg->current_board_device,
      mrvl_platform_cfg->current_droid_device,
      mrvl_platform_cfg->current_device_name.len);

  parser = XML_ParserCreate(NULL);
  if (!parser) {
    ALOGE("%s: Couldn't allocate memory for parser\n", __FUNCTION__);
    return -1;
  }

  XML_SetUserData(parser, mrvl_platform_cfg);
  XML_SetElementHandler(parser, startElementHandler, endElementHandler);
  XML_SetCharacterDataHandler(parser, characterDataHandler);

  mrvl_platform_cfg->current_device_name.len = 0;

  const int BUFF_SIZE = 512;
  while (parsing_check == PARSING_OK) {
    void *buff = XML_GetBuffer(parser, BUFF_SIZE);
    if (buff == NULL) {
      ALOGE("failed in call to XML_GetBuffer()");
      parsing_check = PARSING_UNKNOWN_ERROR;
      break;
    }

    int bytes_read = fread(buff, 1, BUFF_SIZE, file);
    if (bytes_read < 0) {
      ALOGE("failed in call to read");
      parsing_check = PARSING_ERROR_IO;
      break;
    }

    enum XML_Status status =
        XML_ParseBuffer(parser, bytes_read, bytes_read == 0);
    if (status != XML_STATUS_OK) {
      ALOGE("malformed (%s)", XML_ErrorString(XML_GetErrorCode(parser)));
      parsing_check = PARSING_ERROR_MALFORMED;
      break;
    }

    if (bytes_read == 0) {
      break;
    }
  }

  XML_ParserFree(parser);

  fclose(file);
  file = NULL;

  ALOGI("%s: config file %s finished parsing", __FUNCTION__, config_file_name);

  return 0;
}

// free global platform configuration
void deinit_platform_config() {
  struct android_dev_cfg_t *tmp_droiddev_cfg = NULL;
  struct board_dev_cfg_t *tmp_board_dev_cfg = NULL;

  if (mrvl_platform_cfg == NULL) {
    ALOGE("%s: mrvl_platform_cfg is not initialized.", __FUNCTION__);
    return;
  }

  tmp_board_dev_cfg = mrvl_platform_cfg->board_dev_cfg;
  while (tmp_board_dev_cfg) {
    mrvl_platform_cfg->board_dev_cfg = mrvl_platform_cfg->board_dev_cfg->next;
    free(tmp_board_dev_cfg);
    tmp_board_dev_cfg = mrvl_platform_cfg->board_dev_cfg;
  }

  tmp_droiddev_cfg = mrvl_platform_cfg->droid_dev_cfg;
  while (tmp_droiddev_cfg) {
    struct app_cfg_t *tmp_app_cfg = tmp_droiddev_cfg->app_cfg;
    while (tmp_app_cfg) {
      tmp_droiddev_cfg->app_cfg = tmp_droiddev_cfg->app_cfg->next;
      free(tmp_app_cfg);
      tmp_app_cfg = tmp_droiddev_cfg->app_cfg;
    }
    mrvl_platform_cfg->droid_dev_cfg = mrvl_platform_cfg->droid_dev_cfg->next;
    free(tmp_droiddev_cfg);
    tmp_droiddev_cfg = mrvl_platform_cfg->droid_dev_cfg;
  }

  free(mrvl_platform_cfg);
  mrvl_platform_cfg = NULL;
}
