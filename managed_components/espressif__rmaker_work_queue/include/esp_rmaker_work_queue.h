/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <stdint.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C"
{
#endif
/** Prototype for ESP RainMaker Work Queue Function
 *
 * @param[in] priv_data The private data associated with the work function.
 */
typedef void (*esp_rmaker_work_fn_t)(void *priv_data);

/** Initializes the Work Queue
 *
 * This initializes the work queue, which is basically a mechanism to run
 * tasks in the context of a dedicated thread. You can start queueing tasks
 * after this, but they will get executed only after calling
 * esp_rmaker_work_queue_start().
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_work_queue_init(void);

/** De-initialize the Work Queue
 *
 * This de-initializes the work queue. Note that the work queue needs to
 * be stopped using esp_rmaker_work_queue_stop() before calling this.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_work_queue_deinit(void);

/** Start the Work Queue
 *
 * This starts the Work Queue thread which then starts executing the tasks queued.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_work_queue_start(void);

/** Stop the Work Queue
 *
 * This stops a running Work Queue.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_work_queue_stop(void);

/** Queue execution of a function in the Work Queue's context
 *
 * This API queues a work function for execution in the Work Queue Task's context.
 *
 * @param[in] work_fn The Work function to be queued.
 * @param[in] priv_data Private data to be passed to the work function.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_work_queue_add_task(esp_rmaker_work_fn_t work_fn, void *priv_data);

#ifdef __cplusplus
}
#endif
