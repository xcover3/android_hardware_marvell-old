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

# Audio Component Config
BOARD_AUDIO_COMPONENT_APU   := MAP-LITE
BOARD_ENABLE_ADVANCED_AUDIO := false

# Audio Config
BOARD_WITH_STEREO_SPKR := false

# Route audio speaker to headset output
BOARD_WITH_HEADSET_OUTPUT_ONLY := true

# Audio modules
PRODUCT_COPY_FILES += \
     $(TOP)/hardware/bsp/marvell/peripheral/audio/audio_policy.conf:/system/etc/audio_policy.conf \
     $(TOP)/hardware/bsp/marvell/peripheral/audio/platform_audio_config.xml:/system/etc/platform_audio_config.xml

PRODUCT_PACKAGES += \
     libaudioutils

DEVICE_PACKAGES += \
     audio.primary.mrvl

-include hardware/bsp/marvell/peripheral/audio/audio_xml/iap140.mk
