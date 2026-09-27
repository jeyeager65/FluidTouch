// Copyright 2015-2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License

#ifndef _ESP_GSC_H_
#define _ESP_GSC_H_

#include <stdint.h>

#include "gsc_core_types.h"

/* Memory placement:
 * - On ESP32-P4 the GSC core's internal buffers are allocated in PSRAM by
 *   default (auto-enabled for esp32p4 builds). To place them in internal RAM
 *   instead, define GSC_P4_INTERNAL_RAM as a compile definition of the
 *   esp_audio_processor component.
 * - ESP_GSC_DISABLE_PSRAM (define before including this header) only moves
 *   the small wrapper buffers (handle and frame conversion buffers) to
 *   internal RAM; it does NOT affect the GSC core buffers. */
#ifndef ESP_GSC_DISABLE_PSRAM
#define ESP_GSC_PSRAM_DEFAULT
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gsc_handle_t gsc_handle_t;

/**
 * @brief Initialize GSC beamformer for an arbitrary microphone array.
 *
 * @param mic_coord  Array of microphone coordinates (unit: meters),
 *                   right-hand coordinate system, one entry per microphone.
 *                   Any array shape is supported. The entries may be in any
 *                   order, but audio channel i passed to esp_gsc_process()
 *                   must always come from the microphone at mic_coord[i].
 * @param mic_num    Number of microphones (>= 2).
 *
 * @note When chaining DOA (esp_doa_capon_embedded) with GSC, the coordinate
 *       order passed here must match the channel order assumed by the DOA
 *       module, otherwise the estimated angle refers to the wrong channels.
 *
 * @return Initialized gsc_handle_t object pointer, NULL on invalid parameters
 *         or allocation failure
 */
gsc_handle_t *esp_gsc_create(PlaneCoord *mic_coord, int mic_num);

/**
 * @brief Release all allocated resources
 * @param gsc gsc_handle_t instance pointer to be freed
 */
void esp_gsc_destroy(gsc_handle_t *gsc);

/**
 * @brief Process audio frame with GSC beamforming
 *
 * @param gsc       gsc_handle_t instance pointer
 * @param mic_data  mic_num-channel 16-bit PCM data, planar layout.
 *                  Each channel has 128 samples (frame_len).
 *                  Layout: [ch0_0..ch0_127, ch1_0..ch1_127, ...].
 *                  Channel i must correspond to mic_coord[i] of esp_gsc_create().
 * @param loc_phi   Direction of arrival in degrees, range 0-360.
 *                  Standard spherical coordinate (phi from positive x-axis,
 *                  counter-clockwise in the x-y plane). It is defined in the
 *                  absolute array coordinate system and is independent of the
 *                  microphone ordering in mic_coord.
 * @param out_data  Output beamformed signal, 128 samples of 16-bit signed PCM
 */
void esp_gsc_process(gsc_handle_t *gsc, int16_t *mic_data, float loc_phi, int16_t *out_data);

#ifdef __cplusplus
}
#endif

#endif /* _ESP_GSC_H_ */
