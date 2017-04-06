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

#include <time.h>
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/mman.h>
#include <pthread.h>
#include <cutils/properties.h>
#include <hardware/bluetooth.h>

#include "bt_vendor_lib.h"
#include "hci_audio.h"
#include "marvell_wireless.h"
#include "bt_hci_bdroid.h"

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* define log functions here */
#define LOG_TAG "libbt-vendor-mrvl"
#include <cutils/log.h>
#define info(fmt, ...)   ALOGI ("%s(L%d): " fmt,__FUNCTION__, __LINE__,  ## __VA_ARGS__)
#define debug(fmt, ...)  ALOGD ("%s(L%d): " fmt,__FUNCTION__, __LINE__,  ## __VA_ARGS__)
#define warn(fmt, ...)   ALOGW ("## WARNING : %s(L%d): " fmt "##",__FUNCTION__, __LINE__, ## __VA_ARGS__)
#define error(fmt, ...)  ALOGE ("## ERROR : %s(L%d): " fmt "##",__FUNCTION__, __LINE__, ## __VA_ARGS__)
#define asrt(s) if(!(s)) ALOGE ("## %s assert %s failed at line:%d ##",__FUNCTION__, #s, __LINE__)

/*[NK] @Marvell - Driver FIX
   ioctl command to release the read thread before driver close */
#define MBTCHAR_IOCTL_RELEASE _IO  ('M', 1)

/*Mrvl HCI commands for PCM settings*/
#define HCI_GRP_VENDOR_SPECIFIC (0x3F << 10)            /* 0xFC00 */
#define HCI_MRVL_PCM_VOICE_PATH (0x001D | HCI_GRP_VENDOR_SPECIFIC)
#define HCI_MRVL_PCM_SYNC       (0x0028 | HCI_GRP_VENDOR_SPECIFIC)
#define HCI_MRVL_PCM_MODE       (0x0007 | HCI_GRP_VENDOR_SPECIFIC)
#define HCI_MRVL_PCM_LINK       (0x0029 | HCI_GRP_VENDOR_SPECIFIC)
#define HCI_RESET                0x0C03

#define HCIC_PREAMBLE_SIZE               3
#define HCIC_PARAM_SIZE_WRITE_PARAM1     1
#define HCIC_PARAM_SIZE_WRITE_PARAM2     2
#define HCIC_PARAM_SIZE_WRITE_PARAM3     3

#define HCI_EVT_CMD_CMPL_STATUS_RET_BYTE 5
#define HCI_EVT_CMD_CMPL_OPCODE          3

#define HCI_MARVELL_WBS_EXTENSION_OGF    0x3f
#define HCI_MARVELL_WBS_EXTENSION_OCF    0x0073

#define UINT8_TO_STREAM(p, u8)   {*(p)++ = (uint8_t)(u8);}
#define UINT16_TO_STREAM(p, u16) {*(p)++ = (uint8_t)(u16); *(p)++ = (uint8_t)((u16) >> 8);}
#define STREAM_TO_UINT16(u16, p) {u16 = ((uint16_t)(*(p)) + (((uint16_t)(*((p) + 1))) << 8)); (p) += 2;}

void bt_set_sco_codec_cmd(void *param);

static const bt_vendor_callbacks_t *vnd_cb = NULL;

static char bdaddr[17];

static char mchar_port[] = "/dev/mbtchar0";

static int mchar_fd = 0;

int btsnd_hcic_set_pcm_voice_path(void);  /* Set PCM Voice Path*/
int btsnd_hcic_set_pcm_sync(void);        /* Set PCM Sync*/
int btsnd_hcic_set_pcm_mode(void);        /* Set PCM Mode Master*/
int btsnd_hcic_set_pcm_link(void);        /* Set PCM Link */
void hw_epilog_process(void);

#define MAX_RETRY 2
int bt_vnd_mrvl_if_init(const bt_vendor_callbacks_t* p_cb, unsigned char *local_bdaddr)
{
    vnd_cb = p_cb;
    memcpy(bdaddr, local_bdaddr, sizeof(bdaddr));
    return 0;
}

int bt_vnd_mrvl_if_op(bt_vendor_opcode_t opcode, void *param)
{
    int ret = 0;
    int local_st = 0;

    debug("opcode = %d\n", opcode);
    switch (opcode) {
    case BT_VND_OP_POWER_CTRL:
        {
            int state = *(int *) param;
            if (state == BT_VND_PWR_OFF) {
                debug("power off ***************************************\n");
                ret = bluetooth_disable();
                debug("bluetooth_disable, ret: 0x%x", ret);
                if (ret) {
                    /* Sometimes, driver has not detected the FW hung yet (driver need 9s to get this);
                     * and so MarvellWirelessDaemon did not call mrvl_sd8xxx_force_poweroff
                     * to recover the chip, which will lead to failure of bluetooth_disable.
                     * Then we need to do it here
                     */
                    debug("Fail to disable BT, force power off");
                    if (!mrvl_sd8xxx_force_poweroff()) ret = 0;
                }
            } else if (state == BT_VND_PWR_ON) {
                debug("power on --------------------------------------\n");
                int retry = MAX_RETRY;
                while (retry-- > 0) {
                    ret = bluetooth_enable();
                    debug("bluetooth_enable, ret: 0x%x", ret);
                    if (!ret) break;
                    debug("Fail to enable BT the [%d] time, force power off", MAX_RETRY - retry);

                    /* bluetooth_enable failed, assume FW has hung */
                    if (mrvl_sd8xxx_force_poweroff()) break;
                }
                if (ret) {
                    bluetooth_disable();
                }
            }
        }
        break;
    case BT_VND_OP_FW_CFG:
        {
            // TODO: Perform any vendor specific initialization or configuration on the BT Controller
            // ret = xxx
            if (vnd_cb) {
                vnd_cb->fwcfg_cb(ret);
            }
        }
        break;
    case BT_VND_OP_SCO_CFG:
        {
            ret = btsnd_hcic_set_pcm_voice_path();

            if (ret == BT_VND_OP_RESULT_SUCCESS)
                ret = btsnd_hcic_set_pcm_sync();

            if (ret == BT_VND_OP_RESULT_SUCCESS)
                ret = btsnd_hcic_set_pcm_mode();

            if (ret == BT_VND_OP_RESULT_SUCCESS)
                ret = btsnd_hcic_set_pcm_link();

            if (vnd_cb) {
                vnd_cb->scocfg_cb(ret);
            }
        }
        break;
    case BT_VND_OP_USERIAL_OPEN:
        {
            int (*fd_array)[] = (int (*)[]) param;
            int idx;

            mchar_fd = open(mchar_port, O_RDWR);
            if (mchar_fd > 0) {
                debug("open %s successfully\n", mchar_port);
                for (idx=0; idx < CH_MAX; idx++) {
                    (*fd_array)[idx] = mchar_fd;
                        ret = 1;
                }
            } else {
                error("open %s failed fd = %d error = %s\n", mchar_port, mchar_fd, strerror(errno));
                ret = -1;
            }
        }
        break;
    case BT_VND_OP_SET_AUDIO_STATE:
        bt_set_sco_codec_cmd(param);
        break;
    case BT_VND_OP_USERIAL_CLOSE:
        {
            /* mBtChar port is blocked on read. Release the port before we close it */
            ioctl(mchar_fd, MBTCHAR_IOCTL_RELEASE, &local_st);
            /* Give it sometime before we close the mbtchar */
            usleep(1000);
            if (mchar_fd) {
                if (close(mchar_fd) < 0) {
                    error("failed to close fd %d", mchar_fd);
                    ret = -1;
                }
            }
        }
        break;
    case BT_VND_OP_GET_LPM_IDLE_TIMEOUT:
        break;
    case BT_VND_OP_LPM_SET_MODE:
        {
            // TODO: Enable or disable LPM mode on BT Controller.
            // ret = xx;
            if (vnd_cb) {
                vnd_cb->lpm_cb(ret);
            }
        }
        break;
    case BT_VND_OP_LPM_WAKE_SET_STATE:
        break;
    case BT_VND_OP_EPILOG:
        {
#if (HW_END_WITH_HCI_RESET == TRUE)
            hw_epilog_process();
#else
            if (vnd_cb){
                vnd_cb->epilog_cb(BT_VND_OP_RESULT_SUCCESS);
            }
#endif
        }
        break;
    default:
        ret = -1;
        break;
    }
    debug("END!\n");
    return ret;
}

void  bt_vnd_mrvl_if_cleanup(void)
{
    return;
}

/*******************************************************************************
+**
+** Function         bt_set_sco_codec_cback
+**
+** Description      Callback function for sending Wide band support enable and disable to the controller.
+**
+** Returns          None
+**
+*******************************************************************************/
static void bt_set_sco_codec_cback(void *p_mem)
{
    uint8_t     *p, status;
    uint16_t    opcode, len = 0;
    HC_BT_HDR   *p_buf = (HC_BT_HDR *) p_mem;
    int ret = 0;

    p = (uint8_t *)(p_buf + 1) + 3;
    STREAM_TO_UINT16(opcode, p);
    status = *p++;
    if (status == 0) { /* Success */
        STREAM_TO_UINT16(len, p);
    } else {
      error("bt_set_sco_codec_cback : Setting Codec Failed %d", status);
      return;
    }

    debug("bt_set_sco_codec_cback : OpCode 0x%04X Status %d\n", opcode, status);
    vnd_cb->dealloc((void*)p_buf);
}

/*******************************************************************************
+**
+** Function        bt_set_sco_codec_cmd
+**
+** Description     Issue PCM Interface Initialization Commands.
+** Returns         None
+**
+*******************************************************************************/
void bt_set_sco_codec_cmd(void *param)
{
    HC_BT_HDR  *p_buf = NULL;
    uint8_t    *p;
    uint8_t    ret = 0x00;
    uint16_t   opCode;
    char       wbs_enable = FALSE;

    bt_vendor_op_audio_state_t *sco_state_parms = (bt_vendor_op_audio_state_t *)param;

    debug("Handle %d, codec %d, state %d\n", sco_state_parms->handle,
                                             sco_state_parms->peer_codec,
                                             sco_state_parms->state);

    wbs_enable = sco_state_parms->peer_codec == SCO_CODEC_MSBC;

    if (vnd_cb) {
        p_buf = (HC_BT_HDR *) vnd_cb->alloc(BT_HC_HDR_SIZE + HCIC_PREAMBLE_SIZE + sizeof(char));
    }

    opCode = (uint16_t)(HCI_MARVELL_WBS_EXTENSION_OGF << 10 | (HCI_MARVELL_WBS_EXTENSION_OCF & 0x03FF));

    if (p_buf) {
        p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        p_buf->offset = 0;
        p_buf->layer_specific = 0;
        p_buf->len = HCIC_PREAMBLE_SIZE + sizeof(char);

        p = (uint8_t *) (p_buf + 1);
        UINT16_TO_STREAM(p, opCode);
        UINT8_TO_STREAM(p, sizeof(char));

        UINT8_TO_STREAM(p, wbs_enable);

        ret = vnd_cb->xmit_cb(opCode, p_buf, bt_set_sco_codec_cback);
        if (ret == FALSE) {
            vnd_cb->dealloc((void*)p_buf);
        }
        else
            return;
    }

    if (vnd_cb) {
        debug("vendor lib postload aborted");
        vnd_cb->scocfg_cb(ret);
    }

}

/**** config_mrvl_sd8787_voice Commands ****/
int btsnd_hcic_set_pcm_voice_path(void) {
    HC_BT_HDR *p;
    uint8_t *pp;
    uint8_t voice_path = 0x01;

    p = (HC_BT_HDR *) vnd_cb->alloc(BT_HC_HDR_SIZE + HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_WRITE_PARAM1);
    if (p == NULL)
        return BT_VND_OP_RESULT_FAIL;

    pp = (uint8_t *)(p + 1);

    p->event = MSG_STACK_TO_HC_HCI_CMD;
    p->offset = 0;
    p->layer_specific = 0;
    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_WRITE_PARAM3;

    UINT16_TO_STREAM(pp, HCI_MRVL_PCM_VOICE_PATH);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_WRITE_PARAM1);

    UINT8_TO_STREAM(pp, voice_path);

    if(vnd_cb->xmit_cb(HCI_MRVL_PCM_VOICE_PATH, p, NULL)){
        return BT_VND_OP_RESULT_SUCCESS;
    } else {
        vnd_cb->dealloc(p);
        return BT_VND_OP_RESULT_FAIL;
    }
}

int btsnd_hcic_set_pcm_sync(void) {
    HC_BT_HDR *p;
    uint8_t *pp;
    char pcmmaster[PROPERTY_VALUE_MAX];
    uint8_t pcm_sync[3] = {0x03, 0x00, 0x00};

    memset(pcmmaster, 0, PROPERTY_VALUE_MAX);
    property_get("persist.bt.pcmmaster", pcmmaster, "1");
    if (strcmp(pcmmaster, "0") == 0) {
        pcm_sync[2] = 0x0;
    } else {
        pcm_sync[2] = 0x3;
    }

    p = (HC_BT_HDR *) vnd_cb->alloc(BT_HC_HDR_SIZE + HCIC_PREAMBLE_SIZE +HCIC_PARAM_SIZE_WRITE_PARAM3);
    if (p == NULL)
        return BT_VND_OP_RESULT_FAIL;

    pp = (uint8_t *)(p + 1);

    p->event = MSG_STACK_TO_HC_HCI_CMD;
    p->offset = 0;
    p->layer_specific = 0;
    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_WRITE_PARAM3;

    UINT16_TO_STREAM(pp, HCI_MRVL_PCM_SYNC);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_WRITE_PARAM3);

    UINT8_TO_STREAM(pp, pcm_sync[0]);
    UINT8_TO_STREAM(pp, pcm_sync[1]);
    UINT8_TO_STREAM(pp, pcm_sync[2]);

    if(vnd_cb->xmit_cb(HCI_MRVL_PCM_SYNC, p, NULL)){
        return BT_VND_OP_RESULT_SUCCESS;
    } else {
        vnd_cb->dealloc(p);
        return BT_VND_OP_RESULT_FAIL;
    }
}

int btsnd_hcic_set_pcm_mode(void) {
    HC_BT_HDR *p;
    uint8_t *pp;
    uint8_t pcm_mode;

    char pcmmaster[PROPERTY_VALUE_MAX];
    memset(pcmmaster, 0, PROPERTY_VALUE_MAX);
    property_get("persist.bt.pcmmaster", pcmmaster, "1");
    if (strcmp(pcmmaster, "0") == 0){
        pcm_mode = 0x0;
    } else {
        pcm_mode = 0x2;
    }

    p = (HC_BT_HDR *) vnd_cb->alloc(BT_HC_HDR_SIZE + HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_WRITE_PARAM1);
    if (p == NULL)
        return BT_VND_OP_RESULT_FAIL;

    pp = (uint8_t *)(p + 1);

    p->event = MSG_STACK_TO_HC_HCI_CMD;
    p->offset = 0;
    p->layer_specific = 0;
    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_WRITE_PARAM1;

    UINT16_TO_STREAM(pp, HCI_MRVL_PCM_MODE);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_WRITE_PARAM1);

    UINT8_TO_STREAM(pp, pcm_mode);

    if (vnd_cb->xmit_cb(HCI_MRVL_PCM_MODE, p, NULL)) {
        return BT_VND_OP_RESULT_SUCCESS;
    } else {
        vnd_cb->dealloc(p);
        return BT_VND_OP_RESULT_FAIL;
    }

}

int btsnd_hcic_set_pcm_link(void) {
    HC_BT_HDR *p;
    uint8_t *pp;
    uint8_t pcm_link[2] = {0x04, 0x00};

    p = (HC_BT_HDR *) vnd_cb->alloc(BT_HC_HDR_SIZE + HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_WRITE_PARAM2);
    if (p == NULL)
        return BT_VND_OP_RESULT_FAIL;

    pp = (uint8_t *)(p + 1);

    p->event = MSG_STACK_TO_HC_HCI_CMD;
    p->offset = 0;
    p->layer_specific = 0;
    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_WRITE_PARAM2;

    UINT16_TO_STREAM(pp, HCI_MRVL_PCM_LINK);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_WRITE_PARAM2);

    UINT8_TO_STREAM(pp, pcm_link[0]);
    UINT8_TO_STREAM(pp, pcm_link[1]);

    if (vnd_cb->xmit_cb(HCI_MRVL_PCM_LINK, p, NULL)) {
        return BT_VND_OP_RESULT_SUCCESS;
    } else {
        vnd_cb->dealloc(p);
        return BT_VND_OP_RESULT_FAIL;
    }
}

#if (HW_END_WITH_HCI_RESET == TRUE)
/*******************************************************************************
**
** Function         hw_epilog_cback
**
** Description      Callback function for Command Complete Events from HCI
**                  commands sent in epilog process.
**
** Returns          None
**
*******************************************************************************/
void hw_epilog_cback(void *p_mem)
{
    HC_BT_HDR   *p_evt_buf = (HC_BT_HDR *) p_mem;
    uint8_t     *p, status;
    uint16_t    opcode;

    status = *((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_STATUS_RET_BYTE);
    p = (uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_OPCODE;
    STREAM_TO_UINT16(opcode,p);

    debug("%s Opcode:0x%04X Status: %d", __FUNCTION__, opcode, status);

    if (vnd_cb) {
        /* Must free the RX event buffer */
        vnd_cb->dealloc(p_evt_buf);

        /* Once epilog process is done, must call epilog_cb callback
           to notify caller */
        vnd_cb->epilog_cb(BT_VND_OP_RESULT_SUCCESS);
    }
}

/*******************************************************************************
**
** Function         hw_epilog_process
**
** Description      implementation of epilog process
**                  Before shut down bluetooth,  need to send some command to
**                  bluetooth driver: such as reset command. Then can clean up safely
**
** Returns          None
**
*******************************************************************************/
void hw_epilog_process(void)
{
    HC_BT_HDR  *p_buf = NULL;
    uint8_t     *p;

    debug("hw_epilog_process");

    /* Sending a HCI_RESET */
    if (vnd_cb) {
        /* Must allocate command buffer via HC's alloc API */
        p_buf = (HC_BT_HDR *) vnd_cb->alloc(BT_HC_HDR_SIZE + HCIC_PREAMBLE_SIZE);
    }

    if (p_buf) {
        p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        p_buf->offset = 0;
        p_buf->layer_specific = 0;
        p_buf->len = HCIC_PREAMBLE_SIZE;

        p = (uint8_t *) (p_buf + 1);
        UINT16_TO_STREAM(p, HCI_RESET);
        *p = 0; /* parameter length */

        /* Send command via HC's xmit_cb API */
        vnd_cb->xmit_cb(HCI_RESET, p_buf, hw_epilog_cback);
    } else {
        if (vnd_cb) {
            error("vendor lib epilog process aborted [no buffer]");
            vnd_cb->epilog_cb(BT_VND_OP_RESULT_FAIL);
        }
    }
}
#endif // (HW_END_WITH_HCI_RESET == TRUE)

const bt_vendor_interface_t BLUETOOTH_VENDOR_LIB_INTERFACE = {
    sizeof(bt_vendor_interface_t),
    bt_vnd_mrvl_if_init,
    bt_vnd_mrvl_if_op,
    bt_vnd_mrvl_if_cleanup,
};

