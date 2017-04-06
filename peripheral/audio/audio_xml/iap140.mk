# Copyright (C) 2015 The Android Open Source Project
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

XML_LOCAL_PATH := hardware/bsp/marvell/peripheral/audio/audio_xml
XML_TARGET_PATH := system/etc

define copy-path
$(foreach file,$(subst \
    $(XML_LOCAL_PATH)/$(1)/,,$(wildcard \
        $(addprefix $(XML_LOCAL_PATH)/$(1)/,$(2)))), \
            $(XML_LOCAL_PATH)/$(1)/$(file):$(XML_TARGET_PATH)/$(file))
endef

# APU #
APU_DIR := apu/map-lite
PRODUCT_COPY_FILES += $(call copy-path,$(APU_DIR),*)

# CODEC #
CODEC_DIR := codec/ibiza2plus
PRODUCT_COPY_FILES += $(call copy-path,$(CODEC_DIR),*)

# PLATFORM #
PLATFORM_DIR := platform/ulc
PRODUCT_COPY_FILES += $(call copy-path,$(PLATFORM_DIR),*)
