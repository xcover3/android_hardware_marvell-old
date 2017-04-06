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

#ifndef __MRVL_AUDIO_EFFECT_H__
#define __MRVL_AUDIO_EFFECT_H__

void ramp_up_start(unsigned int sample_rate);
void ramp_down_start(unsigned int sample_rate);
void ramp_process(void *buffer, size_t bytes);

#endif  /* __MRVL_AUDIO_EFFECT_H__ */
