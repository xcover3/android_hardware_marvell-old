/*
* All Rights Reserved
*
* MARVELL CONFIDENTIAL
* Copyright 2008 ~ 2010 Marvell International Ltd All Rights Reserved.
* The source code contained or described herein and all documents related to
* the source code ("Material") are owned by Marvell International Ltd or its
* suppliers or licensors. Title to the Material remains with Marvell International Ltd
* or its suppliers and licensors. The Material contains trade secrets and
* proprietary and confidential information of Marvell or its suppliers and
* licensors. The Material is protected by worldwide copyright and trade secret
* laws and treaty provisions. No part of the Material may be used, copied,
* reproduced, modified, published, uploaded, posted, transmitted, distributed,
* or disclosed in any way without Marvell's prior express written permission.
*
* No license under any patent, copyright, trade secret or other intellectual
* property right is granted to or conferred upon you by disclosure or delivery
* of the Materials, either expressly, by implication, inducement, estoppel or
* otherwise. Any license under such intellectual property rights must be
* express and approved by Marvell in writing.
* 
*/
#define LOG_TAG "marvellWirelessDaemon"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <errno.h>
#include <utils/Log.h>

#include <cutils/log.h>
#include <sys/types.h>  
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <stddef.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cutils/properties.h>

#include <sys/prctl.h>
#include <sys/capability.h>
#include <linux/capability.h>
#include <private/android_filesystem_config.h>
#include <dirent.h>

#include "marvell_wireless_daemon.h"


#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef HCI_DEV_ID
#define HCI_DEV_ID 0
#endif

#define HCID_START_DELAY_SEC   1
#define HCID_STOP_DELAY_USEC 500000
#define HCIATTACH_STOP_DELAY_SEC 1
#define FM_ENABLE_DELAY_SEC  3
#define FW_STATE_NORMAL 0
#define FW_STATE_HUNG   1
#define MRVL_WL_RECOVER_DISABLED 0


/** BIT value */
#define MBIT(x)    (1 << (x))
#define DRV_MODE_STA       MBIT(0)
#define DRV_MODE_UAP       MBIT(1)
#define DRV_MODE_WIFIDIRECT       MBIT(2)

#define STA_WEXT_MASK        MBIT(0)
#define UAP_WEXT_MASK        MBIT(1)
#define STA_CFG80211_MASK    MBIT(2)
#define UAP_CFG80211_MASK    MBIT(3)
#define info(fmt, ...)  ALOGI ("%s(L%d): " fmt,__FUNCTION__, __LINE__,  ## __VA_ARGS__)
#define debug(fmt, ...) ALOGD ("%s(L%d): " fmt,__FUNCTION__, __LINE__,  ## __VA_ARGS__)
#define warn(fmt, ...) ALOGW ("## WARNING : %s(L%d): " fmt "##",__FUNCTION__, __LINE__, ## __VA_ARGS__)
#define error(fmt, ...) ALOGE ("## ERROR : %s(L%d): " fmt "##",__FUNCTION__, __LINE__, ## __VA_ARGS__)
#define asrt(s) if(!(s)) ALOGE ("## %s assert %s failed at line:%d ##",__FUNCTION__, #s, __LINE__)

//SD8XXX power state
union POWER_SD8XXX
{
    unsigned int on; // FALSE, means off, others means ON
    struct 
    {
        unsigned int wifi_on:1;  //TRUE means on, FALSE means OFF
        unsigned int uap_on:1;
        unsigned int bt_on:1;
        unsigned int fm_on:1;
    }type;
} power_sd8xxx;

//Static paths and args
// Path define for 8686
static const char* WIFI_DRIVER_MODULE_8686_1_PATH = "/system/lib/modules/libertas.ko";
static const char* WIFI_DRIVER_MODULE_8686_1_NAME = "libertas";
static const char* WIFI_DRIVER_MODULE_8686_1_ARG =  "";

static const char* WIFI_DRIVER_MODULE_8686_2_PATH = "/system/lib/modules/libertas_sdio.ko";
static const char* WIFI_DRIVER_MODULE_8686_2_NAME = "libertas_sdio";
//5: DRV_MODE_STA|DRV_MODE_WIFIDIRECT,  4:STA_CFG80211_MASK
static const char* WIFI_DRIVER_MODULE_8686_2_ARG =  "";

static const char* WIFI_DRIVER_MODULE_1_PATH = "/system/lib/modules/mlan.ko";
static const char* WIFI_DRIVER_MODULE_1_NAME =     "mlan";
static const char* WIFI_DRIVER_MODULE_1_ARG =      "";

static const char* WIFI_DRIVER_MODULE_2_PATH = "/system/lib/modules/sd8787.ko";
static const char* WIFI_DRIVER_MODULE_2_NAME =    "sd8xxx";
//5: DRV_MODE_STA|DRV_MODE_WIFIDIRECT,  4:STA_CFG80211_MASK
static const char* WIFI_DRIVER_MODULE_2_ARG =       "drv_mode=5 cfg80211_wext=12 sta_name=wlan wfd_name=p2p max_vir_bss=1";
//3:DRV_MODE_STA|DRV_MODE_UAP  0x0c:STA_CFG80211_MASK|UAP_CFG80211_MASK
static const char* WIFI_UAP_DRIVER_MODULE_2_ARG =        "drv_mode=3 cfg80211_wext=0xc";
static const char* WIFI_DRIVER_IFAC_NAME =         "/sys/class/net/wlan0";
static const char* WIFI_UAP_DRIVER_IFAC_NAME =         "/sys/class/net/uap0";
static const char* WIFI_DRIVER_MODULE_INIT_ARG = " init_cfg=";
static const char* WIFI_DRIVER_MODULE_INIT_CFG_PATH = "mrvl/wifi_init_cfg.conf";
static const char* WIFI_DRIVER_MODULE_INIT_CFG_STORE_PATH = "/data/misc/wireless/wifi_init_cfg.conf";
static const char* WIFI_DRIVER_MODULE_CAL_DATA_ARG = " cal_data_cfg=";
static const char* WIFI_DRIVER_MODULE_CAL_DATA_CFG_PATH = "mrvl/wifi_cal_data.conf";
static const char* WIFI_DRIVER_MODULE_CAL_DATA_CFG_STORE_PATH = "/system/etc/firmware/mrvl/wifi_cal_data.conf";
static const char* WIFI_DRIVER_MODULE_COUNTRY_ARG = " reg_alpha2=";

static const char* BT_DRIVER_MODULE_PATH =    "/system/lib/modules/mbt8xxx.ko";
static const char* BT_DRIVER_MODULE_NAME =     "mbt8xxx";
static const char* BT_DRIVER_MODULE_INIT_ARG = " init_cfg=";
static const char* BT_DRIVER_MODULE_INIT_CFG_PATH = "mrvl/bt_init_cfg.conf";
static const char* BT_DRIVER_MODULE_INIT_CFG_STORE_PATH = "/data/misc/wireless/bt_init_cfg.conf";

static const char* WIRELESS_UNIX_SOCKET_DIR = "/data/misc/wifi/sockets/socket_daemon";
static const char* WIRELESS_POWER_SET_PATH = "/proc/marvell_wireless";
static const char* MODULE_FILE = "/proc/modules";
static const char DRIVER_PROP_NAME[]    = "wlan.driver.status";
static const char WIFI_COUNTRY_CODE[] = "wifi.country_code";

static const char* RFKILL_SD8X_PATH = "/sys/class/rfkill/rfkill0/state";

static char wifi_drvdbg_arg[MAX_DRV_ARG_SIZE];
static char bt_drvdbg_arg[MAX_DRV_ARG_SIZE];

static const char* BT_DRIVER_DEV_NAME = "/dev/mbtchar0";
static const char* BT_PROCESS_NAME = "droid.bluetooth";
static const char* FM_PROCESS_NAME = "FMRadioServer";
static const char* FM_DRIVER_DEV_NAME = "/dev/mfmchar0";

static const char* MRVL_PROP_WL_RECOVERY = "persist.sys.mrvl_wl_recovery";

static int flag_exit = 0;
static int debug = 1;

void android_set_aid_and_cap() {
    int ret = -1;
    prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0);

    gid_t groups[] = {AID_BLUETOOTH, AID_WIFI, AID_NET_BT_ADMIN, AID_NET_BT, AID_INET, AID_NET_RAW, AID_NET_ADMIN};
    if ((ret = setgroups(sizeof(groups)/sizeof(groups[0]), groups)) != 0){
        ALOGE("setgroups failed, ret:%d, strerror:%s", ret, strerror(errno));
        return;
    }

    if(setgid(AID_SYSTEM) != 0){
        ALOGE("setgid failed, ret:%d, strerror:%s", ret, strerror(errno));
        return;
    }

    if ((ret = setuid(AID_SYSTEM)) != 0){
        ALOGE("setuid failed, ret:%d, strerror:%s", ret, strerror(errno));
        return;
    }

    struct __user_cap_header_struct header;
    struct __user_cap_data_struct cap;
    header.version = _LINUX_CAPABILITY_VERSION;
    header.pid = 0;

    cap.effective = cap.permitted = 1 << CAP_NET_RAW |
    1 << CAP_NET_ADMIN |
    1 << CAP_NET_BIND_SERVICE |
    1 << CAP_SYS_MODULE |
    1 << CAP_IPC_LOCK |
    1 << CAP_KILL;

    cap.inheritable = 0;
    if ((ret = capset(&header, &cap)) != 0){
        ALOGE("capset failed, ret:%d, strerror:%s", ret, strerror(errno));
        return;
    }
    return;
}

//Daemon entry 
int main(void)
{
    int listenfd = -1;
    int clifd = -1;

    power_sd8xxx.on = FALSE;
    //register SIGINT and SIGTERM, set handler as kill_handler
    struct sigaction sa;
    sa.sa_flags = SA_NOCLDSTOP;
    sa.sa_handler = kill_handler;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT,  &sa, NULL);
    /* Sometimes, if client close the socket unexpectedly, */
    /* we may continue to write that socket, then a SIGPIPE */
    /* would be delivered, which would cause the process exit; */
    /* but in fact, this SIGPIPE is harmless, so we should ignore it */
    signal(SIGPIPE,SIG_IGN);

    android_set_aid_and_cap();

    listenfd = serv_listen (WIRELESS_UNIX_SOCKET_DIR);
    if (listenfd < 0) 
    {
        ALOGE("serv_listen error.\n");
        return -1;
    }
    ALOGI("succeed to create socket and listen.\n");
    while (1)
    { 
        clifd = serv_accept (listenfd);
        if (clifd < 0) 
        {
            ALOGE("serv_accept error. \n");
            continue;
        }
        handle_thread(clifd);
        close (clifd);
    }
    close(listenfd);
    return 0; 
}

void handle_thread(int clifd)
{
    int nread;
    char buffer[MAX_BUFFER_SIZE];
    int len = 0;
    int ret = 0;
    nread = read(clifd, buffer, sizeof (buffer));
    if (nread == SOCKERR_IO)
    {
        if (errno == EPIPE) {
            ALOGE("read error on fd [%d]: client close the socket\n", clifd);
        } else {
            ALOGE("read error on fd %d\n", clifd);
        }
    }
    else if (nread == SOCKERR_CLOSED)
    {
        ALOGE("fd %d has been closed.\n", clifd);
    }
    else
    {
        ALOGI("Got that! the data is %s\n", buffer);
        ret = cmd_handler(buffer);
    }
    if(ret == 0)
        strncpy(buffer, "0,OK", sizeof(buffer));
    else
        strncpy(buffer, "1,FAIL", sizeof(buffer));

    nread = write(clifd, buffer, strlen(buffer));

    if (nread == SOCKERR_IO)
    {
        if (errno == EPIPE) {
            ALOGE("write error on fd [%d]: client close the socket\n", clifd);
        } else {
            ALOGE("write error on fd %d\n", clifd);
        }
    }
    else if (nread == SOCKERR_CLOSED)
    {
        ALOGE("fd %d has been closed.\n", clifd);
    }
    else
        ALOGI("Wrote %s to client. \n", buffer);
}

//Command Handler
int cmd_handler(char* buffer)
{ 
    int ret = 0;
    ALOGD("marvell wireless daemon received cmd: %s\n", buffer);
    if(!strncmp(buffer, "WIFI_DISABLE", strlen("WIFI_DISABLE")))
        ret = wifi_disable();
    else if (!strncmp(buffer, "WIFI_ENABLE", strlen("WIFI_ENABLE")))
        ret = wifi_enable();
    if(!strncmp(buffer, "UAP_DISABLE", strlen("UAP_DISABLE")))
        ret = uap_disable();
    else if (!strncmp(buffer, "UAP_ENABLE", strlen("UAP_ENABLE")))
        ret = uap_enable();
    else if (!strncmp(buffer, "BT_DISABLE", strlen("BT_DISABLE")))
        ret = bt_disable();
    else if (!strncmp(buffer, "BT_ENABLE", strlen("BT_ENABLE")))
        ret = bt_enable();
    else if (!strncmp(buffer, "FM_DISABLE", strlen("FM_DISABLE")))
        ret = fm_disable();
    else if (!strncmp(buffer, "FM_ENABLE", strlen("FM_ENABLE")))
        ret = fm_enable();
    else if (!strncmp(buffer, "BT_OFF", strlen("BT_OFF"))) {
        power_sd8xxx.type.bt_on = 0;
        ret = set_power(0);
    } else if (!strncmp(buffer, "BT_ON", strlen("BT_ON"))) {
        power_sd8xxx.type.bt_on = 1;
        ret = set_power(1);
    } else if (!strncmp(buffer, "WIFI_DRV_ARG ", strlen("WIFI_DRV_ARG "))) {
        /* Note: The ' ' before the arg is needed */
        ret = wifi_set_drv_arg(buffer + strlen("WIFI_DRV_ARG"));
    } else if (!strncmp(buffer, "BT_DRV_ARG ", strlen("BT_DRV_ARG"))) {
        /* Note: The ' ' before the arg is needed */
        ret = bt_set_drv_arg(buffer + strlen("BT_DRV_ARG"));
	} else if (!strncmp(buffer, "WIFI_UAP_FORCE_POWER_OFF", strlen("WIFI_UAP_FORCE_POWER_OFF"))) {
        ret = wifi_uap_force_poweroff();
    } else if (!strncmp(buffer, "BT_FM_FORCE_POWER_OFF", strlen("BT_FM_FORCE_POWER_OFF"))) {
        ret = bt_fm_force_poweroff();
    } else if (!strncmp(buffer, "WIFI_GET_FWSTATE", strlen("WIFI_GET_FWSTATE"))) {
        ret = wifi_get_fwstate();
    }
    return ret;
} 



int wifi_enable(void)
{
    int ret = 0;
    power_sd8xxx.type.wifi_on = TRUE;
    ret = wifi_uap_enable(WIFI_DRIVER_MODULE_2_ARG);
    if(ret < 0)goto out;
    ret = wait_interface_ready(WIFI_DRIVER_IFAC_NAME, 1000, 2000);
    if(ret < 0)
    {
        property_set(DRIVER_PROP_NAME, "timeout");
        goto out;
    }
#ifdef SD8787_NEED_CALIBRATE
    ret = wifi_calibrate();
#endif
out:
    if(ret == 0)
    {
        property_set(DRIVER_PROP_NAME, "ok");
    }
    else
    {
        property_set(DRIVER_PROP_NAME, "failed");
    }
    return ret;
}


int wifi_disable(void)
{
    int ret = 0;
    power_sd8xxx.type.wifi_on = FALSE;
    power_sd8xxx.type.uap_on = FALSE;
    ret = wifi_uap_disable();
    if(ret == 0)
    {
        property_set(DRIVER_PROP_NAME, "unloaded");
    }
    else
    {
        property_set(DRIVER_PROP_NAME, "failed");
    }
    return ret;
}

int uap_enable(void)
{
    int ret = 0;
    power_sd8xxx.type.uap_on = TRUE;
    ret = wifi_uap_enable(WIFI_UAP_DRIVER_MODULE_2_ARG);
    if(ret < 0)goto out;
    ret = wait_interface_ready (WIFI_UAP_DRIVER_IFAC_NAME, 1000, 2000);
    if(ret < 0)goto out;
#ifdef SD8787_NEED_CALIBRATE
    ret = wifi_calibrate();        
#endif
out:
    return ret;
}

int wifi_set_drv_arg(char * wifi_drv_arg) {
    if (strlen(wifi_drv_arg) >= MAX_DRV_ARG_SIZE) {
        ALOGE("The wifi driver arg[%s] is too long( >= %d )!", wifi_drv_arg, MAX_DRV_ARG_SIZE);
        return -1;
    }
    memset(wifi_drvdbg_arg, 0, MAX_DRV_ARG_SIZE);
    strcpy(wifi_drvdbg_arg, wifi_drv_arg);

    return 0;
}

int bt_set_drv_arg(char * bt_drv_arg) {
    if (strlen(bt_drv_arg) >= MAX_DRV_ARG_SIZE) {
        ALOGE("The bt driver arg[%s] is too long( >= %d )!", bt_drv_arg, MAX_DRV_ARG_SIZE);
        return -1;
    }
    memset(bt_drvdbg_arg, 0, MAX_DRV_ARG_SIZE);
    strcpy(bt_drvdbg_arg, bt_drv_arg);

    return 0;
}


#ifdef SD8787_NEED_CALIBRATE
int wifi_calibrate(void)
{
    int ret = 0;
    int ret2 = system("mlanconfig wlan0 regrdwr 5 0x26 0x30");
    if(ret2 != 0)
    {
        ALOGD("calibrate:mlanconfig wlan0 regrdwr 5 0x26 0x30 , ret: 0x%x, strerror: %s", ret2, strerror(errno));
    }
    if(ret2 < 0)
    {
        ret = ret2;
    }
    return ret;
}
#endif

int uap_disable(void)
{
    int ret = 0;
    power_sd8xxx.type.uap_on = FALSE;
    ret = wifi_uap_disable();
    return ret;
}

int wifi_get_fwstate()
{
    FILE *fp = fopen("/proc/mwlan/wlan0/debug", "r");
    char *key = "driver_state=";
    int state = FW_STATE_NORMAL;
    char mrvl_wl_recovery_flag[PROPERTY_VALUE_MAX] = {0};
    property_get(MRVL_PROP_WL_RECOVERY, mrvl_wl_recovery_flag, "1");
    if (atoi(mrvl_wl_recovery_flag) == MRVL_WL_RECOVER_DISABLED) {
        ALOGE("The recovery feature has been disabled, ignore the command: wifi get fwstate!"
             "To enable it, please set the property:\n persist.sys.mrvl_wl_recovery");
        return FW_STATE_NORMAL;
    }

    if (fp) {
        char line[1024];
        char *pos = NULL;
        while (fgets(line, sizeof(line), fp)) {
            line[strlen(line)-1] = '\0';
            if (strncmp(line, key, strlen(key)) == 0) {
                pos = line + strlen(key);
                state = atoi(pos);
                break;
            }
        }
        fclose(fp);
    }
    ALOGD("wifi_get_fwstate:[%d]", state);
    return state;
}

int bt_enable(void)
{
    int ret = 0;
    power_sd8xxx.type.bt_on = TRUE;

    ret = bt_fm_enable();
    if (ret < 0) {
        ALOGE("Fail to enable bt_fm!");
        goto out;
    }
    ret = wait_interface_ready(BT_DRIVER_DEV_NAME, 200000, 10);
    if(ret < 0)
    {
        ALOGE("Timeout to wait /dev/mbtchar0!");
        goto out;
    }
out:
    return ret;
}


int bt_disable() 
{
    int ret = 0;

    power_sd8xxx.type.bt_on = FALSE;
    if(power_sd8xxx.type.fm_on == FALSE)
    {
        ret = bt_fm_disable();
    }
out:
    return ret;
}

int fm_enable(void) 
{
    int ret = 0;
    power_sd8xxx.type.fm_on = TRUE;

    ret = bt_fm_enable();
    if(ret < 0) {
        ALOGE("Fail to enable bt_fm!");
        goto out;
    }
    ret = wait_interface_ready(FM_DRIVER_DEV_NAME, 200000, 10);
    if(ret < 0)
    {
        ALOGE("Timeout to wait /dev/mfmchar0!");
        goto out;
    }
out:
    return ret;
}

int fm_disable() 
{
    int ret = 0;
    power_sd8xxx.type.fm_on = FALSE;
    if(power_sd8xxx.type.bt_on == FALSE)
    {
        ret = bt_fm_disable();
    }
    return ret;
}

int check_cfg_file(const char* cfg_path)
{
    int ret = 0;

    if (access(cfg_path, F_OK) == 0)
    {
        ret = 1;
    }
    else
    {
        system("tcmd-subcase.sh copy-wifi-bt-cfg");
        if (access(cfg_path, F_OK) == 0)
        {
            ret = 1;
        }
    }

    return ret;
}

int wifi_uap_enable(const char* driver_module_arg)
{
    int ret = 0;
    char arg_buf[MAX_BUFFER_SIZE];
    char country_code[MAX_BUFFER_SIZE];

    ALOGD("wifi_uap_enable");
    memset(arg_buf, 0, MAX_BUFFER_SIZE);
    strcpy(arg_buf, driver_module_arg);
    strcat(arg_buf, wifi_drvdbg_arg);

    country_code[0] = '\0';
    property_get(WIFI_COUNTRY_CODE, country_code, "");
    if(strcmp("", country_code) != 0)
    {
        strcat(arg_buf, WIFI_DRIVER_MODULE_COUNTRY_ARG);
        strcat(arg_buf, country_code);
    }

#ifdef SD8787_CAL_ON_BOARD
    if (check_cfg_file(WIFI_DRIVER_MODULE_INIT_CFG_STORE_PATH))
    {
        ALOGD("The wifi config file exists");
        check_wifi_bt_mac_addr();
    }
    else
    {
        ALOGD("The wifi config file doesn't exist");
        if(create_wifi_bt_init_cfg() != 0) ALOGD("create wifi bt init cfg file failed");
    }
    if (access(WIFI_DRIVER_MODULE_INIT_CFG_STORE_PATH, F_OK) == 0)
    {
        strcat(arg_buf, WIFI_DRIVER_MODULE_INIT_ARG);
        strcat(arg_buf, WIFI_DRIVER_MODULE_INIT_CFG_PATH);
    }

    if (check_cfg_file(WIFI_DRIVER_MODULE_CAL_DATA_CFG_STORE_PATH))
    {
        strcat(arg_buf, WIFI_DRIVER_MODULE_CAL_DATA_ARG);
        strcat(arg_buf, WIFI_DRIVER_MODULE_CAL_DATA_CFG_PATH);
    }
    else
    {
        ALOGD("The wifi calibrate file not exist");
    }
#endif
    ret = set_power(TRUE);
    if (ret < 0)
    {
        ALOGD("wifi_uap_enable, set_power fail");
        goto out;
    }
    if(check_driver_loaded(WIFI_DRIVER_MODULE_1_NAME) == FALSE)
    {
        ret = insmod(WIFI_DRIVER_MODULE_1_PATH, WIFI_DRIVER_MODULE_1_ARG);
        if (ret < 0)
        {
            ALOGD("wifi_uap_enable, insmod: %s %s fail", WIFI_DRIVER_MODULE_1_PATH, WIFI_DRIVER_MODULE_1_ARG);
            goto out;
        }
    }

    if(check_driver_loaded(WIFI_DRIVER_MODULE_2_NAME) == FALSE)
    {
        ret = insmod(WIFI_DRIVER_MODULE_2_PATH, arg_buf);
        if (ret < 0)
        {
            ALOGD("wifi_uap_enable, insmod: %s %s fail", WIFI_DRIVER_MODULE_2_PATH, arg_buf);
            goto out;
        }
    }

    if(check_driver_loaded(WIFI_DRIVER_MODULE_8686_1_NAME) == FALSE)
    {
        ret = insmod(WIFI_DRIVER_MODULE_8686_1_PATH, WIFI_DRIVER_MODULE_8686_1_ARG);
        if (ret < 0)
        {
            ALOGD("wifi_uap_enable, insmod: %s %s fail", WIFI_DRIVER_MODULE_8686_1_PATH, WIFI_DRIVER_MODULE_8686_1_ARG);
            goto out;
        }
    }

    if(check_driver_loaded(WIFI_DRIVER_MODULE_8686_2_NAME) == FALSE)
    {
        ret = insmod(WIFI_DRIVER_MODULE_8686_2_PATH, WIFI_DRIVER_MODULE_8686_2_ARG);
        if (ret < 0)
        {
            ALOGD("wifi_uap_enable, insmod: %s %s fail", WIFI_DRIVER_MODULE_8686_2_PATH, arg_buf);
            goto out;
        }
    }

out:
    return ret;
}


int wifi_uap_disable()
{
    int ret = 0;

    /* To speed up the recovery, detect the FW status here */
    if (wifi_get_fwstate() == FW_STATE_HUNG)
    {
        if (wifi_uap_force_poweroff() == 0)return ret;
    }
    if(check_driver_loaded(WIFI_DRIVER_MODULE_8686_2_NAME) == TRUE)
    {
        ret = rmmod (WIFI_DRIVER_MODULE_8686_2_NAME);
        if (ret < 0) goto out;
    }
    if(check_driver_loaded(WIFI_DRIVER_MODULE_8686_1_NAME) == TRUE)
    {
        ret = rmmod (WIFI_DRIVER_MODULE_8686_1_NAME);
        if (ret < 0) goto out;
    }
     if(check_driver_loaded(WIFI_DRIVER_MODULE_2_NAME) == TRUE)
    {
        ret = rmmod (WIFI_DRIVER_MODULE_2_NAME);
        if (ret < 0) goto out;
    }
    if(check_driver_loaded(WIFI_DRIVER_MODULE_1_NAME) == TRUE)
    {
        ret = rmmod (WIFI_DRIVER_MODULE_1_NAME);
        if (ret < 0) goto out;
    }
    ret = set_power(FALSE);
out:
    return ret;
}


int bt_fm_enable(void)
{
    int ret = 0;
    char arg_buf[MAX_BUFFER_SIZE];

    debug("");
    ALOGD("bt_fm_enable");
    memset(arg_buf, 0, MAX_BUFFER_SIZE);

#ifdef SD8787_CAL_ON_BOARD
    if (check_cfg_file(BT_DRIVER_MODULE_INIT_CFG_STORE_PATH))
    {
        ALOGD("The bluetooth config file exists");
        check_wifi_bt_mac_addr();
    }
    else
    {
        ALOGD("The bluetooth config file doesn't exist");
        if(create_wifi_bt_init_cfg() != 0) ALOGD("create wifi bt init cfg file failed");
    }
    if (access(BT_DRIVER_MODULE_INIT_CFG_STORE_PATH, F_OK) == 0)
    {
        strcat(arg_buf, BT_DRIVER_MODULE_INIT_ARG);
        strcat(arg_buf, BT_DRIVER_MODULE_INIT_CFG_PATH);
    }
#endif

    strcat(arg_buf, bt_drvdbg_arg);

    if(check_driver_loaded(BT_DRIVER_MODULE_NAME) == FALSE)
    {
        ret = insmod(BT_DRIVER_MODULE_PATH, arg_buf);
        if (ret < 0) {
            error("insmod %s failed\n", BT_DRIVER_MODULE_PATH);
            goto out;
        }
    }

    ret = set_power(TRUE);
    if (ret < 0)
    {
        ALOGD("bt_fm_enable, set_power fail: errno:%d, %s", errno, strerror(errno));
        goto out;
    }

out:
    return ret;
}

int kill_process_by_name(const char* ProcName) {
    DIR             *dir = NULL;
    struct dirent   *d = NULL;
    int             pid = 0;
    char comm[PATH_MAX+1];
    /* Open the /proc directory. */
    dir = opendir("/proc");
    if (!dir)
    {
        printf("cannot open /proc");
        return -1;
    }
    /* Walk through the directory. */
    while ((d = readdir(dir)) != NULL) {
        /* See if this is a process */
        if ((pid = atoi(d->d_name)) == 0) continue;
        snprintf(comm, sizeof(comm), "/proc/%s/comm", d->d_name);
        FILE *fp = fopen(comm, "r");
        if (fp) {
            char line[1024];
            char *pos = NULL;
            while (fgets(line, sizeof(line), fp)) {
                line[strlen(line)-1] = '\0';
                if (strncmp(line, ProcName, strlen(ProcName)) == 0) {
                    ALOGI("Try to kill pid[%d][%s]\n", pid, ProcName);
                    if (kill(pid, SIGKILL) != 0) {
                        ALOGE("Fail to kill pid[%d][%s], error[%s]\n", pid, ProcName, strerror(errno));
                    }
                }
            }
            fclose(fp);
        }
    }
    closedir(dir);
    return  0;
}

int wifi_uap_force_poweroff() {
    int ret = 0;
    char mrvl_wl_recovery_flag[PROPERTY_VALUE_MAX] = {0};
    property_get(MRVL_PROP_WL_RECOVERY, mrvl_wl_recovery_flag, "1");
    if (atoi(mrvl_wl_recovery_flag) == MRVL_WL_RECOVER_DISABLED) {
        ALOGE("The recovery feature has been disabled, ignore the command: force power off!"
             "To enable it, please set the property:\n persist.sys.mrvl_wl_recovery");
        return -1;
    }
    ALOGE("wifi/uap force power off");
    ret = system("/system/bin/rfkill block all");
    if(ret != 0) {
        ALOGE("---------system /system/bin/rfkill block all, ret: 0x%x, strerror: %s", ret, strerror(errno));
    }
    if(check_driver_loaded(WIFI_DRIVER_MODULE_8686_2_NAME) == TRUE)
    {
        ret = rmmod (WIFI_DRIVER_MODULE_8686_2_NAME);
        if (ret < 0)
            ALOGE("Fail to rmmod[%s], ret = %d", WIFI_DRIVER_MODULE_8686_2_NAME, ret);
    }
    if(check_driver_loaded(WIFI_DRIVER_MODULE_8686_1_NAME) == TRUE)
    {
        ret = rmmod (WIFI_DRIVER_MODULE_8686_1_NAME);
        if (ret < 0)
            ALOGE("Fail to rmmod[%s], ret = %d", WIFI_DRIVER_MODULE_8686_1_NAME, ret);
    }

    if(check_driver_loaded(WIFI_DRIVER_MODULE_2_NAME) == TRUE)
    {
        ret = rmmod (WIFI_DRIVER_MODULE_2_NAME);
        if (ret < 0)
            ALOGE("Fail to rmmod[%s], ret = %d", WIFI_DRIVER_MODULE_2_NAME, ret);
    }
    if(check_driver_loaded(WIFI_DRIVER_MODULE_1_NAME) == TRUE)
    {
        ret = rmmod (WIFI_DRIVER_MODULE_1_NAME);
        if (ret < 0)
            ALOGE("Fail to rmmod[%s], ret = %d", WIFI_DRIVER_MODULE_1_NAME, ret);
    }

    /* We need to kill 'BT_PROCESS_NAME', for BT wouldn't recover in time after 8787 is forced power off */
    if (power_sd8xxx.type.bt_on) {
        ALOGE("wifi_uap_force_poweroff: kill BT");
        kill_process_by_name(BT_PROCESS_NAME);
    }

    if (power_sd8xxx.type.fm_on) {
        ALOGE("wifi_uap_force_poweroff: kill FM");
        kill_process_by_name(FM_PROCESS_NAME);
    }

    return 0;
}

int bt_fm_force_poweroff() {
    int ret = 0;
    char mrvl_wl_recovery_flag[PROPERTY_VALUE_MAX] = {0};
    property_get(MRVL_PROP_WL_RECOVERY, mrvl_wl_recovery_flag, "1");
    if (atoi(mrvl_wl_recovery_flag) == MRVL_WL_RECOVER_DISABLED) {
        ALOGE("The recovery feature has been disabled, ignore the command: force power off!"
             "To enable it, please set the property:\n persist.sys.mrvl_wl_recovery");
        return -1;
    }
    ALOGE("bt/fm force power off");
    ret = system("/system/bin/rfkill block all");
    if(ret != 0) {
        ALOGE("---------system /system/bin/rfkill block all, ret: 0x%x, strerror: %s", ret, strerror(errno));
    }
    if(!(power_sd8xxx.type.bt_on | power_sd8xxx.type.fm_on) &&
       check_driver_loaded(BT_DRIVER_MODULE_NAME) == TRUE)
    {
        ret = rmmod(BT_DRIVER_MODULE_NAME);
        if (ret < 0)
            ALOGE("Fail to rmmod[%s], ret = %d", BT_DRIVER_MODULE_NAME, ret);
    }

    if (power_sd8xxx.type.bt_on) {
        ALOGE("bt_fm_force_poweroff: kill BT");
        kill_process_by_name(BT_PROCESS_NAME);
    }

    if (power_sd8xxx.type.fm_on) {
        ALOGE("bt_fm_force_poweroff: kill FM");
        kill_process_by_name(FM_PROCESS_NAME);
    }
    return 0;
}

int bt_fm_disable(void)
{
    int ret = 0;

    /* To speed up the recovery, detect the FW status here */
    if (wifi_get_fwstate() == FW_STATE_HUNG)
    {
        if (!bt_fm_force_poweroff())return ret;
    }
    if(check_driver_loaded(BT_DRIVER_MODULE_NAME) == TRUE)
    {
        ret = rmmod(BT_DRIVER_MODULE_NAME);
        if (ret < 0) goto out;
    }

    if (!power_sd8xxx.type.wifi_on) {
        ret = set_power(FALSE);
    }
out:
    return ret;
}

int do_set_power(int on)
{
    int fd;
    int retry = 5;
    char ret = -1;
    char sw = on ? '1' : '0';

    ALOGI("rfkill utils:%s: on=%d", __FUNCTION__, on);
    while (retry--) {
        fd = open(RFKILL_SD8X_PATH, O_RDWR);
        /*marvell mobile platform usr rfkill to control wifi/bt/fm chip power before, all will use pm runtime in the future.
        but switch schedule of different platform is different, for some time, we should make rfkill and pm runtime
        can coexist, don't break wif/bt/fm features.
        Fix:before echo rfkill state file, check whether this file exists, if not exists, assume pm runtime is used, let driver to control power, rfkill is
        removed form kernel, just return OK.*/
        if(errno == ENOENT)return 0;
        if (fd <= 0) {
            ALOGE("open %s fail", RFKILL_SD8X_PATH);
            return -1;
        }
        if (write(fd, &sw, sizeof(char)) < 0) {
            ALOGE("write error %s\n", strerror(errno));
        }
        close(fd);
        usleep(200000);

        fd = open(RFKILL_SD8X_PATH, O_RDWR);
        if (fd <= 0) {
            ALOGE("open %s fail", RFKILL_SD8X_PATH);
            return -1;
        }
        read(fd, &ret, sizeof(char));
        close(fd);

        if (ret == sw) {
            return 0;
        }
    }
    return -1;
}

/*
int set_power(int on)
{
    ALOGI("rfkill utils:%s: on=%d", __FUNCTION__, on);

    if((!on) && (!(power_sd8787.on))) {
        // off
        return do_set_power(on);
    } else if (on) {
        // on
        return do_set_power(on);
    }
    return 0;
}*/

int set_power(int on)
{
    const char data = (on ? '1' : '0');
    int fd = 0;
    int sz = 0;
    int ret = 0;
    ALOGI("rfkill utils:%s: on=%d", __FUNCTION__, on);
    if((!on) && (!(power_sd8xxx.on)))
    {
        ret = system("/system/bin/rfkill block all");
        if(ret != 0)
            ALOGE("---------system /system/bin/rfkill block all, ret: 0x%x, strerror: %s", ret, strerror(errno));
    }
    else if(on)
    {
        ret = system("/system/bin/rfkill unblock all");
        if(ret != 0)
            ALOGE("---------system /system/bin/rfkill unblock all, ret: 0x%x, strerror: %s", ret, strerror(errno));
    }
    else{}
out:
    return 0;
}

int insmod(const char *filename, const char *args)
{
    void *module = NULL;
    unsigned int size = 0;
    int ret = 0;
    module = load_file(filename, &size);
    if (!module)
    {
        ALOGD("loadfile:%s, error: %s", filename, strerror(errno));
        ret = -1;
        goto out;
    }
    ret = init_module(module, size, args);
    free(module);
out:
    ALOGD("insmod:%s,args %s, size %d, ret: %d", filename, args, size, ret);
    return ret;
}

int rmmod(const char *modname)
{
    int ret = -1;
    int maxtry = 10;

    while (maxtry-- > 0) {
        ret = delete_module(modname, O_NONBLOCK | O_EXCL);
        if (ret < 0 && errno == EAGAIN)
            usleep(500000);
        else
            break;
    }

    if (ret != 0)
        ALOGD("Unable to unload driver module \"%s\": %s\n",
             modname, strerror(errno));
    return ret;
}

int check_driver_loaded(const char *modname) 
{
    FILE *proc = NULL;
    char line[64];
    int ret = FALSE;

    if ((proc = fopen(MODULE_FILE, "r")) == NULL) 
    {
        ALOGW("Could not open %s: %s", MODULE_FILE, strerror(errno));
        ret = FALSE;
        goto out;
    }
    while ((fgets(line, sizeof(line), proc)) != NULL) 
    {
        if (strncmp(line, modname, strlen(modname)) == 0) 
        {
            fclose(proc);
            ret = TRUE;    
            goto out;
        }
    }
    fclose(proc);
out:
    ALOGD("check_driver_loaded %s: ret:%d", modname, ret);
    return ret;
}

//to do: don’t use polling mode, use uevent, listen interface added uevent from driver
int wait_interface_ready (const char* interface_path, int us_interval, int retry)
{
    int fd, count = retry;
    int i = 0;

    while(count--) {
        fd = open(interface_path, O_RDONLY);
        if (fd >= 0)
        {
            close(fd);
            return 0;
        }
        usleep(us_interval);
    }

    ALOGE("timeout(%dms) to wait %s", (retry * us_interval/1000), interface_path);
    return -1;
}

static void kill_handler(int sig)
{
    ALOGI("Received signal %d.", sig);
    power_sd8xxx.on = FALSE;

    if (set_power(FALSE) < 0) 
    {
        ALOGE("set_fm_power failed.");
    }
    flag_exit = 1;
}

int serv_listen (const char* name)
{
    int fd,len;
    struct sockaddr_un unix_addr;

    /* Create a Unix domain stream socket */
    if ( (fd = socket (AF_UNIX, SOCK_STREAM, 0)) < 0)
        return (-1);
    unlink (name);
    /* Fill in socket address structure */
    memset (&unix_addr, 0, sizeof (unix_addr));
    unix_addr.sun_family = AF_UNIX;
    strcpy (unix_addr.sun_path, name);
    snprintf(unix_addr.sun_path, sizeof(unix_addr.sun_path), "%s", name);
    len = sizeof (unix_addr.sun_family) + strlen (unix_addr.sun_path);

    /* Bind the name to the descriptor */
    if (bind (fd, (struct sockaddr*)&unix_addr, len) < 0)
    {
        ALOGE("bind fd:%d and address:%s error: %s", fd, unix_addr.sun_path, strerror(errno));
        goto error;
    }
    if (chmod (name, 0666) < 0)
    {
        ALOGE("change %s mode error: %s", name, strerror(errno));
        goto error;
    }
    if (listen (fd, 5) < 0)
    {
        ALOGE("listen fd %d error: %s", fd, strerror(errno));
        goto error;
    }
    return (fd);

error:
    close (fd);
    return (-1); 
}

#define    STALE    30    /* client's name can't be older than this (sec) */


/* returns new fd if all OK, < 0 on error */
int serv_accept (int listenfd)
{
    int                clifd, len;
    time_t             staletime;
    struct sockaddr_un unix_addr;
    struct stat        statbuf;
    const char*        pid_str;

    len = sizeof (unix_addr);
    if ( (clifd = accept (listenfd, (struct sockaddr *) &unix_addr, &len)) < 0)
    {
        ALOGE("listenfd %d, accept error: %s", listenfd, strerror(errno));
        return (-1);        /* often errno=EINTR, if signal caught */
    }
    return (clifd);
}

#define RANDOM(x) (rand()%x)
#define MAC_ADDR_LENGTH 12
#define VENDOR_PREFIX_LENGTH 6
#define FMT_MAC_ADDR_LEN (MAC_ADDR_LENGTH+5)

unsigned char hex_char[16]={'0','1','2','3','4','5','6','7','8','9','A','B','C',
'D','E','F'};
//Marvell MAC prefix assigned by IEEE: 00:50:43
const char *vendor_prefix = "00:50:43:00:00:00";
char fmt_mac_addr[FMT_MAC_ADDR_LEN+1];

void format_mac_addr(void)
{
    unsigned short r = 0;
    unsigned short n = 0;

    strncpy(fmt_mac_addr, vendor_prefix, strlen(vendor_prefix));
    for(n = VENDOR_PREFIX_LENGTH * 3 /2; n < FMT_MAC_ADDR_LEN; n++ )
    {
        if( fmt_mac_addr[n] != ':' )
        {
            r = RANDOM(16);
            fmt_mac_addr[n] = hex_char[r];
        }
        else
        {
            n++;
        }
    }
}

void MSRAND(void)
{
	struct timeval tv;
	unsigned int seed;
	gettimeofday(&tv, NULL);
	seed = tv.tv_sec * 1000000 + tv.tv_usec;
	srand(seed);
}

int read_mac_from_file(char* mac_addr, const char *file_path)
{
    int ret = 0;
    FILE* fp = NULL;
    int sz;
    int len = 17;

    fp = fopen(file_path, "r");
    if (!fp)
    {
        ALOGE("open(%s) failed: %s (%d)", file_path, strerror(errno), errno);
        goto out;
    }

    sz = fread(mac_addr, 1, len, fp);
    mac_addr[len] = '\0';
    if (sz < 0)
    {
        ALOGE("read(%s) failed: %s (%d)", file_path, strerror(errno), errno);
        goto out;
    }
    else if (sz != len)
    {
        ALOGE("read(%s)unexpected MAC size %d", file_path, sz);
        goto out;
    }

    ret = 1;

    out:
    if (fp != NULL)
    {
        fclose(fp);
    }

    return ret;
}

int read_mac_from_cfg(char* mac_addr, const char *cfg_path)
{
    FILE* fp = NULL;
    char buf[1024];
    char* pos = NULL;

    fp = fopen(cfg_path, "r");
    if (!fp)
    {
        ALOGE("open(%s) failed: %s (%d)", cfg_path, strerror(errno), errno);
        goto out;
    }

    memset(buf, 0, 1024);
    fgets(buf, 1024, fp);
    pos = buf;
    if (strncmp(pos, "mac_addr", 8) == 0)
    {
        pos = strchr(pos, ':');
        if (pos != NULL)
        {
            strncpy(mac_addr, pos+2, FMT_MAC_ADDR_LEN);
        }
    }

    out:
    if (fp != NULL)
    {
        fclose(fp);
    }
    return 0;
}

const char* wifi_mac_path = "/NVM/wifi_addr";
const char* bt_mac_path = "/NVM/bt_addr";

int create_wifi_bt_init_cfg()
{
    unsigned short i = 0;
    FILE *fp_bt = NULL;
    FILE *fp_wifi = NULL;
    int ret = -1;
    int size = 0;

    fp_bt = fopen(BT_DRIVER_MODULE_INIT_CFG_STORE_PATH, "w" );
    if( !fp_bt )
    {
        ALOGE("create the file %s failed, error:%s\n", BT_DRIVER_MODULE_INIT_CFG_STORE_PATH, strerror(errno));
        goto err;
    }
    fp_wifi = fopen(WIFI_DRIVER_MODULE_INIT_CFG_STORE_PATH, "w" );

    if( !fp_wifi )
    {
        ALOGE("create the file %s failed, error:%s\n", WIFI_DRIVER_MODULE_INIT_CFG_STORE_PATH, strerror(errno));
        goto err;
    }

    MSRAND();

    if (access(bt_mac_path, F_OK) == 0)
    {
        read_mac_from_file(fmt_mac_addr, bt_mac_path);
        ALOGD("read bt mac address from file %s: %s\n", bt_mac_path, fmt_mac_addr);
    }
    else
    {
        format_mac_addr();
        ALOGD("generate bt mac address from random generator: %s\n", fmt_mac_addr);
    }
    size = fprintf( fp_bt, "mac_addr = mbtchar0: %s\n",fmt_mac_addr);
    if(debug) ALOGD("mac_addr = mbtchar0: %s\n",fmt_mac_addr);
    if(size <= 0)
    {
        ALOGE("write the file %s failed, error:%s\n", BT_DRIVER_MODULE_INIT_CFG_STORE_PATH, strerror(errno));
        goto err;
    }

    if (access(wifi_mac_path, F_OK) == 0)
    {
        read_mac_from_file(fmt_mac_addr, wifi_mac_path);
        ALOGD("read wifi mac address from file %s: %s\n", wifi_mac_path, fmt_mac_addr);
    }
    else
    {
        format_mac_addr();
        ALOGD("generate wifi mac address from random generator: %s\n", fmt_mac_addr);
    }
    size = fprintf( fp_wifi, "mac_addr = wlan0: %s\n",fmt_mac_addr);
    if(debug) ALOGD("mac_addr = wlan0: %s\n",fmt_mac_addr);
    if(size <= 0)
    {
        ALOGE("write the file %s failed, error:%s\n", WIFI_DRIVER_MODULE_INIT_CFG_STORE_PATH, strerror(errno));
        goto err;
    }

    fmt_mac_addr[1] = '2';
    size  = fprintf( fp_wifi, "mac_addr = p2p0: %s\n",fmt_mac_addr);
    if(debug) ALOGD("mac_addr = p2p0: %s\n",fmt_mac_addr);
    if(size <= 0)
    {
        ALOGE("write the file %s failed, error:%s\n", WIFI_DRIVER_MODULE_INIT_CFG_STORE_PATH, strerror(errno));
        goto err;
    }

    ret = 0;
err:
    if(fp_bt != NULL)fclose(fp_bt);
    if(fp_wifi != NULL)fclose(fp_wifi);
    if(ret != 0)
    {
        unlink(BT_DRIVER_MODULE_INIT_CFG_STORE_PATH);
        unlink(WIFI_DRIVER_MODULE_INIT_CFG_STORE_PATH);
    }
    return ret;
}

void check_wifi_bt_mac_addr()
{
    FILE *fp_bt = NULL;
    FILE *fp_wifi = NULL;
    char cfg_mac_addr[FMT_MAC_ADDR_LEN+1];
    char file_mac_addr[FMT_MAC_ADDR_LEN+1];

    if (access(wifi_mac_path, F_OK) == 0)
    {
        memset(cfg_mac_addr, 0, FMT_MAC_ADDR_LEN+1);
        memset(file_mac_addr, 0, FMT_MAC_ADDR_LEN+1);
        read_mac_from_file(file_mac_addr, wifi_mac_path);
        ALOGD("file wifi mac address: %s\n", file_mac_addr);
        read_mac_from_cfg(cfg_mac_addr, WIFI_DRIVER_MODULE_INIT_CFG_STORE_PATH);
        ALOGD("cfg wifi mac address: %s\n", cfg_mac_addr);
        if (memcmp(file_mac_addr, cfg_mac_addr, FMT_MAC_ADDR_LEN) != 0)
        {
            ALOGD("wifi mac address not consistent, update the wifi cfg file");
            fp_wifi = fopen(WIFI_DRIVER_MODULE_INIT_CFG_STORE_PATH, "w");
            if (fp_wifi)
            {
                fprintf(fp_wifi, "mac_addr = wlan0: %s\n", file_mac_addr);
                if(debug) ALOGD("update wifi mac_addr: %s\n", file_mac_addr);
                file_mac_addr[1] = '2';
                fprintf(fp_wifi, "mac_addr = p2p0: %s\n", file_mac_addr);
            }
            fclose(fp_wifi);
        }
    }

    if (access(bt_mac_path, F_OK) == 0)
    {
        memset(cfg_mac_addr, 0, FMT_MAC_ADDR_LEN+1);
        memset(file_mac_addr, 0, FMT_MAC_ADDR_LEN+1);
        read_mac_from_file(file_mac_addr, bt_mac_path);
        ALOGD("file bt mac address: %s\n", file_mac_addr);
        read_mac_from_cfg(cfg_mac_addr, BT_DRIVER_MODULE_INIT_CFG_STORE_PATH);
        ALOGD("cfg bt mac address: %s\n", cfg_mac_addr);
        if (memcmp(file_mac_addr, cfg_mac_addr, FMT_MAC_ADDR_LEN) != 0)
        {
            ALOGD("bt mac address not consistent, update the bt cfg file");
            fp_bt = fopen(BT_DRIVER_MODULE_INIT_CFG_STORE_PATH, "w");
            if (fp_bt)
            {
                fprintf(fp_bt, "mac_addr = mbtchar0: %s\n", file_mac_addr);
                if(debug) ALOGD("update bt mac_addr: %s\n", file_mac_addr);
            }
            fclose(fp_bt);
        }
    }
}
