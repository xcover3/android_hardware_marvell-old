/*
 * Copyright (C)  2016. Marvell International Ltd
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

/*
 *********************************************
 *       Andromeda Box GPIO Mapping          *
 *********************************************
 * Extension Board Phsical Pin Layout:
 *      +-+-+-+-+-+......+-+-+-+--+--+
 *      |2|4|6|8| |......| | | |38|40|
 *      +-+-+-+-+-+......+-+-+-+--+--+
 *      |1|3|5|7| |......| | | |37|39|
 *      +----------      ------------+
 *
 * PHY_PIN     NAME        DESCRIPTION             DIRECTION
 * 3           IO1         GPIO_45(UART1_CTSn)     IN/OUT
 * 5           IO2         GPIO_44(UART1_TXD)      IN/OUT
 * 7           IO3         GPIO_43(UART1_RXD)      IN/OUT
 * 9           IO4         GPIO_46(UART1_RTSn)     IN/OUT
 * 23          IO5         GPIO_72(TP_INT)         IN/OUT
 * 25          IO6         GPIO_09(GPIO_09)        IN/OUT
 * 27          IO7         GPIO_12(GPIO_12)        IN/OUT
 * 29          IO8         GPIO_51(PWM_GPIO51)     IN/OUT
 * 31          IO9         GPIO_67(CAM_RST_MAIN)   IN/OUT
 * 33          IO10        GPIO_69(CAM_RST_SUB)    IN/OUT
 * 16          IO11        GPIO_01(EXP_I2S_SYNC)   OUT Only
 * 18          IO12        GPIO_00(EXP_I2S_CLK)    OUT Only
 * 20          IO13        GPIO_02(EXP_I2S_DO)     OUT Only
 * 22          IO14        GPIO_03(EXP_I2S_DI)     OUT Only
 * 24          IO15        GPIO_75(TP_RESET)       IN/OUT
 * 26          IO16        GPIO_18(GPIO_18)        IN/OUT
 * 28          IO17        GPIO_100(LCD_CTRL)      IN/OUT
 * 30          IO18        GPIO_04(LCD_RESET_N)    IN/OUT
 * 32          IO19        GPIO_68(CAM_PD_MAIN)    IN/OUT
 * 34          IO20        GPIO_70(CAM_PD_SUB)     IN/OUT
 */

#include <cutils/log.h>
#include <string.h>
#include <stdlib.h>
#include <hardware/hardware.h>
#include <hardware/peripheral_io.h>

#define PIN_NUMBER 20
#define UNUSED(x) (void)(x)

int phy_pin_index[PIN_NUMBER] = {45,44,43,46,72,9,12,51,67,69,1,0,2,3,75,18,100,4,68,70};

const char IO[] = "IO";

static int io_register_devices(const peripheral_io_module_t* dev,
                               const peripheral_registration_cb_t* callbacks);

struct peripheral_io_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .version_major = 1,
        .version_minor = 0,
        .id = PERIPHERAL_IO_HARDWARE_MODULE_ID,
        .name = "peripheral_io module",
        .author = "Marvell SEEDS",
    },
    .register_devices = io_register_devices
};

static int io_register_devices(const peripheral_io_module_t* dev,
                               const peripheral_registration_cb_t* callbacks)
{
    UNUSED(dev);

    int i = 0;
    char io_name[16] = {0};

    for(i = 1; i <= PIN_NUMBER; i++)
    {
        memset(io_name,0,sizeof(io_name));
        sprintf(io_name, "%s%d", IO, i);
        callbacks->register_gpio_sysfs(io_name, phy_pin_index[i - 1]);
    }
    return true;
}
