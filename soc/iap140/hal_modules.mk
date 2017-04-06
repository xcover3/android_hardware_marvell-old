#
# Copyright 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

-include hardware/bsp/marvell/soc/iap140/modules/audio_hal_module.mk
-include hardware/bsp/marvell/soc/iap140/modules/sensor_hal_module.mk
-include hardware/bsp/marvell/soc/iap140/modules/lights_hal_module.mk
-include hardware/bsp/marvell/soc/iap140/modules/power_hal_module.mk
-include hardware/bsp/marvell/soc/iap140/modules/wifi_hal_module.mk
-include hardware/bsp/marvell/soc/iap140/modules/bluetooth_hal_moduel.mk
-include hardware/bsp/marvell/soc/iap140/modules/keystore_hal_module.mk
-include hardware/bsp/marvell/soc/iap140/modules/peripheral_io_hal_module.mk

# the client and server of the wireless daemon
-include hardware/bsp/marvell/soc/iap140/modules/libwireless.mk
