/*
 * SPDX-FileCopyrightText: 2021-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sdkconfig.h>
#include <inttypes.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/timers.h>
#include <esp_log.h>
#include <mqtt_client.h>
#include <esp_event.h>
#include <esp_rmaker_common_events.h>
#include <esp_rmaker_mqtt_glue.h>
#include <esp_idf_version.h>
#include <esp_rmaker_utils.h>
#include <esp_rmaker_work_queue.h>
#ifdef CONFIG_ESP_RMAKER_MQTT_PORT_443
#define ESP_RMAKER_MQTT_USE_PORT_443
#endif

#ifdef CONFIG_ESP_RMAKER_MQTT_USE_CERT_BUNDLE
#define ESP_RMAKER_MQTT_USE_CERT_BUNDLE
#include <esp_crt_bundle.h>
#endif

static const char *TAG = "esp_mqtt_glue";

#define MAX_MQTT_SUBSCRIPTIONS      CONFIG_ESP_RMAKER_MAX_MQTT_SUBSCRIPTIONS

/* A SUBSCRIBE that the broker rejects (SUBACK failure code, e.g. broker-side throttling), that could
 * not be sent, or that esp-mqtt dropped from its outbox is re-sent until it is acknowledged. The
 * delay between attempts starts at BASE, doubles on every attempt and is capped at MAX; retries
 * never stop while the connection is up. A lost connection is handled separately: the timer is
 * stopped on disconnect and every subscription is re-sent on the next connect.
 */
#define MQTT_SUB_RETRY_BASE_MS      (5 * 1000)
#define MQTT_SUB_RETRY_MAX_MS       (60 * 1000)
/* esp-mqtt silently deletes an un-acked SUBSCRIBE from its outbox after this long */
#ifdef CONFIG_MQTT_OUTBOX_EXPIRED_TIMEOUT_MS
#define MQTT_SUB_REQUEST_EXPIRY_MS  CONFIG_MQTT_OUTBOX_EXPIRED_TIMEOUT_MS
#else
#define MQTT_SUB_REQUEST_EXPIRY_MS  (30 * 1000)
#endif
/* Block time for timer commands; they are issued without the glue lock held (see s_sub_retry_timer) */
#define MQTT_SUB_TIMER_CMD_WAIT_MS  100
/* SUBACKs that arrive before their request is recorded are parked for this long */
#define MQTT_SUB_EARLY_ACK_SLOTS    4
#define MQTT_SUB_EARLY_ACK_TTL_MS   5000

/* Subscription states for tracking subscription lifecycle */
typedef enum {
    MQTT_SUB_STATE_NONE = 0,        /* Not subscribed */
    MQTT_SUB_STATE_REQUESTED,       /* Subscription request sent, waiting for SUBACK */
    MQTT_SUB_STATE_ACKNOWLEDGED,    /* SUBACK received, subscription active */
    MQTT_SUB_STATE_FAILED           /* Subscription failed */
} mqtt_subscription_state_t;

typedef struct {
    char *topic;
    esp_rmaker_mqtt_subscribe_cb_t cb;
    void *priv;
    mqtt_subscription_state_t state;
    int msg_id;                     /* Message ID from last subscribe request */
    uint8_t qos;                    /* QoS level for this subscription */
    TickType_t request_tick;        /* When the last subscribe request was sent */
} esp_mqtt_glue_subscription_t;

/* A SUBACK that arrived before its request was recorded, see esp_mqtt_glue_send_subscribe() */
typedef struct {
    int msg_id;                     /* 0: slot free */
    bool rejected;
    TickType_t tick;
} esp_mqtt_glue_early_ack_t;

typedef struct {
    esp_mqtt_client_handle_t mqtt_client;
    esp_rmaker_mqtt_conn_params_t *conn_params;
    esp_mqtt_glue_subscription_t *subscriptions[MAX_MQTT_SUBSCRIPTIONS];
    /* The fields below are guarded by s_glue_lock, like subscriptions[] */
    bool connected;
    uint32_t sub_retry_delay_ms;
    bool timer_arm_pending;         /* A timer command was dropped; re-issue it on the next MQTT event */
    esp_mqtt_glue_early_ack_t early_acks[MQTT_SUB_EARLY_ACK_SLOTS];
    bool deleting;                  /* esp_mqtt_glue_deinit() has started; no new operation may begin */
    int busy;                       /* Operations using mqtt_client outside the lock, see glue_begin_op() */
} esp_mqtt_glue_data_t;
esp_mqtt_glue_data_t *mqtt_data;

/* Created on first init and never deleted, so that a subscribe or a retry round racing
 * esp_mqtt_glue_deinit() always finds valid objects to synchronise on.
 *
 * Never call into esp-mqtt while holding s_glue_lock: the MQTT task holds its own API lock while
 * delivering events into mqtt_event_handler(), which takes this lock.
 */
static SemaphoreHandle_t s_glue_lock;
static TimerHandle_t s_sub_retry_timer;

static inline void glue_lock(void)
{
    xSemaphoreTakeRecursive(s_glue_lock, portMAX_DELAY);
}

static inline void glue_unlock(void)
{
    xSemaphoreGiveRecursive(s_glue_lock);
}

/* Enter an operation that uses mqtt_data or mqtt_client outside the lock. Fails once deinit has
 * started; deinit in turn waits for every operation entered before that to call glue_end_op().
 */
static bool glue_begin_op(void)
{
    if (!s_glue_lock) {
        return false;
    }
    glue_lock();
    if (!mqtt_data || mqtt_data->deleting) {
        glue_unlock();
        return false;
    }
    mqtt_data->busy++;
    glue_unlock();
    return true;
}

/* Leave an operation entered through glue_begin_op(). No NULL check on purpose: deinit() waits for
 * busy to reach zero before it frees mqtt_data, so an operation that was let in always finds it
 * valid here, and a NULL would be a paired-call bug that a silent check would only hide.
 */
static void glue_end_op(void)
{
    glue_lock();
    mqtt_data->busy--;
    glue_unlock();
}

typedef struct {
    char *data;
    char *topic;
} esp_mqtt_glue_long_data_t;

/* A (callback, priv) pair matched for an incoming message */
typedef struct {
    esp_rmaker_mqtt_subscribe_cb_t cb;
    void *priv;
} esp_mqtt_glue_cb_match_t;

/* A topic collected for (re-)subscribe while the lock is held */
typedef struct {
    char *topic;
    uint8_t qos;
    bool no_suback;     /* Was requested but never acknowledged */
} esp_mqtt_glue_pending_sub_t;

static void esp_mqtt_glue_deinit(void);
static void esp_mqtt_glue_schedule_sub_retry(void);
static void esp_mqtt_glue_sub_retry_work(void *arg);

/**
 * @brief Check if an MQTT topic matches a subscription pattern with wildcards
 * 
 * Supports MQTT single-level wildcard:
 * - '+' matches a single level (e.g., "node/+/params" matches "node/device1/params")
 * 
 * @param topic_filter The subscription pattern (may contain '+' wildcards)
 * @param topic_name The actual topic name to match
 * @param topic_len Length of the topic name
 * @return true if the topic matches the filter, false otherwise
 */
static bool mqtt_topic_matches(const char *topic_filter, const char *topic_name, int topic_len)
{
    const char *filter_pos = topic_filter;
    const char *topic_pos = topic_name;
    int topic_consumed = 0;

    while (*filter_pos && topic_consumed < topic_len) {
        if (*filter_pos == '+') {
            // Single-level wildcard - skip to next '/' or end of topic
            while (topic_consumed < topic_len && *topic_pos != '/') {
                topic_pos++;
                topic_consumed++;
            }
            filter_pos++;
        } else if (*filter_pos == *topic_pos) {
            // Characters match, advance both
            filter_pos++;
            topic_pos++;
            topic_consumed++;
        } else {
            // Characters don't match
            return false;
        }
    }

    // Both strings must be fully consumed
    return (*filter_pos == '\0' && topic_consumed == topic_len);
}

/* Reset all subscription states. Caller holds the lock. */
static void esp_mqtt_glue_reset_subscription_states_locked(void)
{
    for (int i = 0; i < MAX_MQTT_SUBSCRIPTIONS; i++) {
        if (mqtt_data->subscriptions[i]) {
            mqtt_data->subscriptions[i]->state = MQTT_SUB_STATE_NONE;
        }
    }
}

static void esp_mqtt_glue_subscribe_callback(const char *topic, int topic_len, const char *data, int data_len)
{
    /* Collect the matching callbacks under the lock and invoke them with it released, so that a
     * callback may itself subscribe or unsubscribe.
     */
    esp_mqtt_glue_cb_match_t matches[MAX_MQTT_SUBSCRIPTIONS];
    int count = 0;

    /* topic is a length-delimited slice of the esp-mqtt receive buffer (the payload follows it), not
     * a C string, so the callbacks get a NUL-terminated copy of the actual topic (the subscription
     * they registered may be a wildcard).
     */
    char *actual_topic = strndup(topic, topic_len);
    if (!actual_topic) {
        ESP_LOGE(TAG, "Failed to allocate memory for actual topic");
        return;
    }

    glue_lock();
    for (int i = 0; i < MAX_MQTT_SUBSCRIPTIONS; i++) {
        esp_mqtt_glue_subscription_t *sub = mqtt_data->subscriptions[i];
        if (sub && mqtt_topic_matches(sub->topic, topic, topic_len)) {
            matches[count].cb = sub->cb;
            matches[count].priv = sub->priv;
            count++;
        }
    }
    glue_unlock();

    for (int i = 0; i < count; i++) {
        matches[i].cb(actual_topic, (void *)data, data_len, matches[i].priv);
    }
    free(actual_topic);
}

/*
 * Compatibility wrapper for ESP-IDF v5.1.2+ esp_mqtt_client_subscribe macro issue
 * The _Generic macro doesn't handle const char* topic type properly in older versions
 * esp_mqtt_client_subscribe_single was introduced in ESP-IDF v5.1.2 to fix this
 * See: https://github.com/espressif/esp-idf/issues/13414
 */
static inline int _esp_mqtt_client_subscribe(esp_mqtt_client_handle_t client, const char *topic, int qos)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 2)
    return esp_mqtt_client_subscribe_single(client, topic, qos);
#else
    return esp_mqtt_client_subscribe(client, topic, qos);
#endif
}

/* Park a SUBACK whose request has not been recorded yet. Caller holds the lock. */
static void esp_mqtt_glue_park_early_ack_locked(int msg_id, bool rejected)
{
    TickType_t now = xTaskGetTickCount();
    esp_mqtt_glue_early_ack_t *slot = &mqtt_data->early_acks[0];
    for (int i = 0; i < MQTT_SUB_EARLY_ACK_SLOTS; i++) {
        esp_mqtt_glue_early_ack_t *e = &mqtt_data->early_acks[i];
        if (e->msg_id == 0 || (now - e->tick) > pdMS_TO_TICKS(MQTT_SUB_EARLY_ACK_TTL_MS)) {
            slot = e;
            break;
        }
        if ((now - e->tick) > (now - slot->tick)) {
            slot = e;   /* No free slot: evict the oldest */
        }
    }
    slot->msg_id = msg_id;
    slot->rejected = rejected;
    slot->tick = now;
}

/* Take a parked SUBACK for msg_id, if there is one. Caller holds the lock. */
static bool esp_mqtt_glue_take_early_ack_locked(int msg_id, bool *rejected)
{
    TickType_t now = xTaskGetTickCount();
    for (int i = 0; i < MQTT_SUB_EARLY_ACK_SLOTS; i++) {
        esp_mqtt_glue_early_ack_t *e = &mqtt_data->early_acks[i];
        if (e->msg_id == msg_id && (now - e->tick) <= pdMS_TO_TICKS(MQTT_SUB_EARLY_ACK_TTL_MS)) {
            *rejected = e->rejected;
            e->msg_id = 0;
            return true;
        }
    }
    return false;
}

/* Log and publish the outcome of a SUBACK, then re-evaluate the retry timer. Call without the lock. */
static void esp_mqtt_glue_report_suback(const char *topic, bool rejected)
{
    if (rejected) {
        ESP_LOGW(TAG, "Broker rejected subscription to %s. Will retry.", topic);
    } else {
        ESP_LOGD(TAG, "Subscription acknowledged for topic: %s", topic);
    }
    esp_event_post(RMAKER_COMMON_EVENT, rejected ? RMAKER_MQTT_EVENT_SUBSCRIBE_FAILED : RMAKER_MQTT_EVENT_SUBSCRIBED,
                   topic, strlen(topic) + 1, portMAX_DELAY);
    esp_mqtt_glue_schedule_sub_retry();
}

/* Send one SUBSCRIBE for a topic and record the outcome on every table entry for that topic.
 * Call without the lock held and from inside a glue operation (see glue_begin_op()).
 *
 * The msg_id is only known once esp-mqtt has sent the packet, and the MQTT task can deliver the
 * SUBACK before this task gets to record it. So the entries are reserved first (REQUESTED with
 * msg_id -1); a SUBACK that matches no recorded request is parked by the event handler and picked
 * up here as soon as the msg_id is known.
 */
static void esp_mqtt_glue_send_subscribe(const char *topic, uint8_t qos)
{
    glue_lock();
    bool connected = mqtt_data->connected;
    TickType_t now = xTaskGetTickCount();
    for (int i = 0; i < MAX_MQTT_SUBSCRIPTIONS; i++) {
        esp_mqtt_glue_subscription_t *sub = mqtt_data->subscriptions[i];
        if (sub && strcmp(sub->topic, topic) == 0) {
            sub->msg_id = -1;
            sub->state = MQTT_SUB_STATE_REQUESTED;
            sub->request_tick = now;
        }
    }
    glue_unlock();

    int ret = -1;
    if (connected) {
        ret = _esp_mqtt_client_subscribe(mqtt_data->mqtt_client, topic, qos);
    }

    bool early_ack = false, early_rejected = false;
    glue_lock();
    if (ret >= 0) {
        early_ack = esp_mqtt_glue_take_early_ack_locked(ret, &early_rejected);
    }
    for (int i = 0; i < MAX_MQTT_SUBSCRIPTIONS; i++) {
        esp_mqtt_glue_subscription_t *sub = mqtt_data->subscriptions[i];
        if (!sub || strcmp(sub->topic, topic) != 0 || sub->state != MQTT_SUB_STATE_REQUESTED || sub->msg_id != -1) {
            continue;   /* Not our reservation any more, e.g. a disconnect reset it in between */
        }
        if (ret < 0) {
            sub->state = MQTT_SUB_STATE_FAILED;
        } else {
            sub->msg_id = ret;
            if (early_ack) {
                sub->state = early_rejected ? MQTT_SUB_STATE_FAILED : MQTT_SUB_STATE_ACKNOWLEDGED;
            }
        }
    }
    glue_unlock();

    if (ret < 0) {
        if (connected) {
            ESP_LOGW(TAG, "Failed to send subscribe for %s. Will retry.", topic);
        }
    } else if (early_ack) {
        esp_mqtt_glue_report_suback(topic, early_rejected);
    } else {
        ESP_LOGD(TAG, "Subscribing to %s (msg_id: %d, QoS: %d)", topic, ret, qos);
    }
}

/* Whether an entry needs a (re-)subscribe. Caller holds the lock. */
static bool esp_mqtt_glue_sub_needs_send_locked(const esp_mqtt_glue_subscription_t *sub, TickType_t now)
{
    switch (sub->state) {
        case MQTT_SUB_STATE_ACKNOWLEDGED:
            return false;
        case MQTT_SUB_STATE_REQUESTED:
            /* No SUBACK for this long means esp-mqtt has dropped the request from its outbox */
            return (now - sub->request_tick) > pdMS_TO_TICKS(MQTT_SUB_REQUEST_EXPIRY_MS);
        default:
            return true;
    }
}

/* Send a SUBSCRIBE for every unique topic that needs one, at the highest QoS requested for it.
 * A request that got no SUBACK within the outbox expiry is reported as failed before it is re-sent,
 * so that a silently dropped subscribe is visible exactly like a rejected one.
 * Returns true if at least one previously failed (as opposed to merely expired) entry was re-sent.
 */
static bool esp_mqtt_glue_resubscribe_pending(void)
{
    esp_mqtt_glue_pending_sub_t pending[MAX_MQTT_SUBSCRIPTIONS];
    int count = 0;
    bool resent_failed = false;
    TickType_t now = xTaskGetTickCount();

    glue_lock();
    for (int i = 0; i < MAX_MQTT_SUBSCRIPTIONS; i++) {
        esp_mqtt_glue_subscription_t *sub = mqtt_data->subscriptions[i];
        if (!sub || !esp_mqtt_glue_sub_needs_send_locked(sub, now)) {
            continue;
        }
        int j;
        for (j = 0; j < count; j++) {
            if (strcmp(pending[j].topic, sub->topic) == 0) {
                if (sub->qos > pending[j].qos) {
                    pending[j].qos = sub->qos;
                }
                break;
            }
        }
        if (j < count) {
            continue;
        }
        /* Copy the topic: the entry may be unsubscribed once the lock is released */
        pending[count].topic = strdup(sub->topic);
        if (!pending[count].topic) {
            ESP_LOGE(TAG, "Failed to allocate memory for topic string");
            break;
        }
        pending[count].qos = sub->qos;
        pending[count].no_suback = (sub->state == MQTT_SUB_STATE_REQUESTED);
        count++;
    }
    glue_unlock();

    for (int i = 0; i < count; i++) {
        if (pending[i].no_suback) {
            ESP_LOGW(TAG, "No SUBACK for %s within %d ms. Re-subscribing.", pending[i].topic, MQTT_SUB_REQUEST_EXPIRY_MS);
            esp_event_post(RMAKER_COMMON_EVENT, RMAKER_MQTT_EVENT_SUBSCRIBE_FAILED, pending[i].topic,
                           strlen(pending[i].topic) + 1, portMAX_DELAY);
        } else {
            resent_failed = true;
        }
        esp_mqtt_glue_send_subscribe(pending[i].topic, pending[i].qos);
        free(pending[i].topic);
    }
    return resent_failed;
}

/* One retry round. Runs on the RainMaker work queue when there is one (see the timer callback), so
 * that the blocking calls in here (our lock, esp-mqtt's API lock, esp_event_post) do not stall the
 * timer service task.
 */
static void esp_mqtt_glue_sub_retry_work(void *arg)
{
    if (!glue_begin_op()) {
        return;
    }
    ESP_LOGI(TAG, "Retrying pending MQTT subscriptions");
    /* Back off only when a failed entry was actually retried; an expiry check alone does not count */
    if (esp_mqtt_glue_resubscribe_pending()) {
        glue_lock();
        mqtt_data->sub_retry_delay_ms *= 2;
        if (mqtt_data->sub_retry_delay_ms > MQTT_SUB_RETRY_MAX_MS) {
            mqtt_data->sub_retry_delay_ms = MQTT_SUB_RETRY_MAX_MS;
        }
        glue_unlock();
    }
    esp_mqtt_glue_schedule_sub_retry();
    glue_end_op();
}

static void esp_mqtt_glue_sub_retry_timer_cb(TimerHandle_t timer)
{
    /* Hand the round to the work queue (a non-blocking post); run it here only if there is none */
    if (esp_rmaker_work_queue_add_task(esp_mqtt_glue_sub_retry_work, NULL) != ESP_OK) {
        esp_mqtt_glue_sub_retry_work(NULL);
    }
}

/* Arm the retry timer if any subscription is not acknowledged, stop it once all are. */
static void esp_mqtt_glue_schedule_sub_retry(void)
{
    bool failed = false, requested = false;
    uint32_t delay_ms = 0;

    glue_lock();
    for (int i = 0; i < MAX_MQTT_SUBSCRIPTIONS; i++) {
        esp_mqtt_glue_subscription_t *sub = mqtt_data->subscriptions[i];
        if (!sub || sub->state == MQTT_SUB_STATE_ACKNOWLEDGED) {
            continue;
        }
        if (sub->state == MQTT_SUB_STATE_REQUESTED) {
            requested = true;
        } else {
            failed = true;
        }
    }
    if (!mqtt_data->connected) {
        /* Everything is re-sent on the next MQTT_EVENT_CONNECTED */
        mqtt_data->sub_retry_delay_ms = MQTT_SUB_RETRY_BASE_MS;
    } else if (failed) {
        /* Jitter from the tick count, so that a fleet reconnecting together does not retry in lockstep */
        delay_ms = mqtt_data->sub_retry_delay_ms + (xTaskGetTickCount() * portTICK_PERIOD_MS) % MQTT_SUB_RETRY_BASE_MS;
    } else if (requested) {
        /* Nothing failed yet. Check back once esp-mqtt would have dropped an un-acked request. */
        delay_ms = MQTT_SUB_REQUEST_EXPIRY_MS + 1000;
    } else {
        mqtt_data->sub_retry_delay_ms = MQTT_SUB_RETRY_BASE_MS;
    }
    glue_unlock();

    if (!s_sub_retry_timer) {
        return;
    }
    /* The timer handle lives for the whole process, so it can be used outside the lock, and not
     * holding the lock is what makes a non-zero block time safe here: the timer daemon may be inside
     * a retry round waiting for our lock, and it must be able to get it to drain its command queue.
     */
    BaseType_t posted;
    if (delay_ms) {
        ESP_LOGD(TAG, "Subscribe retry check in %" PRIu32 " ms", delay_ms);
        posted = xTimerChangePeriod(s_sub_retry_timer, pdMS_TO_TICKS(delay_ms), pdMS_TO_TICKS(MQTT_SUB_TIMER_CMD_WAIT_MS));
    } else {
        posted = xTimerStop(s_sub_retry_timer, pdMS_TO_TICKS(MQTT_SUB_TIMER_CMD_WAIT_MS));
    }
    if (posted != pdPASS) {
        /* Timer command queue full. Do not leave pending subscriptions waiting for the next
         * reconnect: mqtt_event_handler() re-issues the command on the next MQTT event. */
        ESP_LOGE(TAG, "Could not %s the subscribe retry timer, will retry on the next MQTT event", delay_ms ? "arm" : "stop");
        glue_lock();
        if (mqtt_data) {
            mqtt_data->timer_arm_pending = true;
        }
        glue_unlock();
    }
}

/* Connection is gone: forget broker-side state and stop retrying until the next connect. */
static void esp_mqtt_glue_mark_disconnected(void)
{
    glue_lock();
    mqtt_data->connected = false;
    esp_mqtt_glue_reset_subscription_states_locked();
    mqtt_data->sub_retry_delay_ms = MQTT_SUB_RETRY_BASE_MS;
    glue_unlock();
    if (s_sub_retry_timer && xTimerStop(s_sub_retry_timer, pdMS_TO_TICKS(MQTT_SUB_TIMER_CMD_WAIT_MS)) != pdPASS) {
        /* Harmless: a round that fires while disconnected finds nothing to send */
        ESP_LOGD(TAG, "Could not stop the subscribe retry timer");
    }
}

static esp_err_t esp_mqtt_glue_subscribe(const char *topic, esp_rmaker_mqtt_subscribe_cb_t cb, uint8_t qos, void *priv_data)
{
    if (!topic || !cb || !glue_begin_op()) {
        return ESP_FAIL;
    }
    esp_err_t err = ESP_OK;
    esp_mqtt_glue_subscription_t *existing_entry = NULL;
    bool topic_has_active_subscription = false;
    bool need_send = false;
    int empty_slot = -1;

    glue_lock();
    /* Single pass: gather all the info we need */
    for (int i = 0; i < MAX_MQTT_SUBSCRIPTIONS; i++) {
        esp_mqtt_glue_subscription_t *sub = mqtt_data->subscriptions[i];
        if (sub) {
            if (strcmp(topic, sub->topic) == 0) {
                if (cb == sub->cb) {
                    /* Same topic and callback: this is an update */
                    existing_entry = sub;
                }
                if (sub->state == MQTT_SUB_STATE_ACKNOWLEDGED) {
                    topic_has_active_subscription = true;
                }
            }
        } else if (empty_slot == -1) {
            empty_slot = i;
        }
    }

    if (existing_entry) {
        existing_entry->priv = priv_data;
        if (existing_entry->qos < qos) {
            ESP_LOGD(TAG, "QoS upgrade requested for topic: %s (%d->%d)", topic, existing_entry->qos, qos);
            existing_entry->qos = qos;
            need_send = true;
        } else if (existing_entry->state == MQTT_SUB_STATE_NONE || existing_entry->state == MQTT_SUB_STATE_FAILED) {
            /* Not acknowledged and not in flight: send now rather than waiting for the retry timer */
            need_send = true;
        }
        qos = existing_entry->qos;
    } else {
        if (empty_slot == -1) {
            glue_unlock();
            ESP_LOGE(TAG, "No space for new subscription to topic: %s", topic);
            err = ESP_FAIL;
            goto done;
        }
        esp_mqtt_glue_subscription_t *subscription = calloc(1, sizeof(esp_mqtt_glue_subscription_t));
        if (!subscription) {
            glue_unlock();
            ESP_LOGE(TAG, "Failed to allocate memory for subscription");
            err = ESP_FAIL;
            goto done;
        }
        subscription->topic = strdup(topic);
        if (!subscription->topic) {
            glue_unlock();
            free(subscription);
            ESP_LOGE(TAG, "Failed to allocate memory for topic string");
            err = ESP_FAIL;
            goto done;
        }
        subscription->priv = priv_data;
        subscription->cb = cb;
        subscription->qos = qos;
        subscription->state = topic_has_active_subscription ? MQTT_SUB_STATE_ACKNOWLEDGED : MQTT_SUB_STATE_NONE;
        mqtt_data->subscriptions[empty_slot] = subscription;
        need_send = !topic_has_active_subscription;
    }
    glue_unlock();

    if (need_send) {
        esp_mqtt_glue_send_subscribe(topic, qos);
        esp_mqtt_glue_schedule_sub_retry();
    } else {
        ESP_LOGD(TAG, "Added callback for already-subscribed topic: %s", topic);
    }
done:
    glue_end_op();
    return err;
}

/* Detach the entry at *slot from the table. Caller holds the lock. *send_unsubscribe is set when
 * the broker-side subscription should go too: connected, and no other entry uses the same topic.
 */
static esp_mqtt_glue_subscription_t *esp_mqtt_glue_detach_locked(esp_mqtt_glue_subscription_t **slot, bool *send_unsubscribe)
{
    esp_mqtt_glue_subscription_t *sub = *slot;
    *slot = NULL;
    *send_unsubscribe = mqtt_data->connected;
    for (int i = 0; i < MAX_MQTT_SUBSCRIPTIONS; i++) {
        if (mqtt_data->subscriptions[i] && strcmp(mqtt_data->subscriptions[i]->topic, sub->topic) == 0) {
            *send_unsubscribe = false;
            break;
        }
    }
    return sub;
}

/* Send UNSUBSCRIBE if requested and free a detached entry. Call without the lock held. */
static void esp_mqtt_glue_release(esp_mqtt_glue_subscription_t *sub, bool send_unsubscribe)
{
    if (!send_unsubscribe) {
        ESP_LOGD(TAG, "Not sending UNSUBSCRIBE for %s (not connected, or other callbacks still use it)", sub->topic);
    } else if (esp_mqtt_client_unsubscribe(mqtt_data->mqtt_client, sub->topic) < 0) {
        ESP_LOGW(TAG, "Could not unsubscribe from topic: %s", sub->topic);
    } else {
        ESP_LOGD(TAG, "Unsubscribed from topic: %s", sub->topic);
    }
    free(sub->topic);
    free(sub);
}

static esp_err_t esp_mqtt_glue_unsubscribe(const char *topic)
{
    if (!topic || !glue_begin_op()) {
        return ESP_FAIL;
    }
    esp_mqtt_glue_subscription_t *sub = NULL;
    bool send_unsubscribe = false;
    glue_lock();
    for (int i = 0; i < MAX_MQTT_SUBSCRIPTIONS; i++) {
        if (mqtt_data->subscriptions[i] && strncmp(topic, mqtt_data->subscriptions[i]->topic, strlen(topic)) == 0) {
            sub = esp_mqtt_glue_detach_locked(&mqtt_data->subscriptions[i], &send_unsubscribe);
            break;
        }
    }
    glue_unlock();
    if (sub) {
        esp_mqtt_glue_release(sub, send_unsubscribe);
    }
    glue_end_op();
    return sub ? ESP_OK : ESP_FAIL;
}

static esp_err_t esp_mqtt_glue_publish(const char *topic, void *data, size_t data_len, uint8_t qos, int *msg_id)
{
    if (!mqtt_data || !topic || !data) {
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "Publishing to %s", topic);
    int ret = esp_mqtt_client_publish(mqtt_data->mqtt_client, topic, data, data_len, qos, 0);
    if (ret < 0) {
        ESP_LOGE(TAG, "MQTT Publish failed");
        return ESP_FAIL;
    }
    if (msg_id) {
        *msg_id = ret;
    }
    return ESP_OK;
}

static esp_mqtt_glue_long_data_t *esp_mqtt_glue_free_long_data(esp_mqtt_glue_long_data_t *long_data)
{
    if (long_data) {
        if (long_data->topic) {
            free(long_data->topic);
        }
        if (long_data->data) {
            free(long_data->data);
        }
        free(long_data);
    }
    return NULL;
}

static esp_mqtt_glue_long_data_t *esp_mqtt_glue_manage_long_data(esp_mqtt_glue_long_data_t *long_data,
        esp_mqtt_event_handle_t event)
{
    if (event->topic) {
        /* This is new data. Free any earlier data, if present. */
        esp_mqtt_glue_free_long_data(long_data);
        long_data = calloc(1, sizeof(esp_mqtt_glue_long_data_t));
        if (!long_data) {
            ESP_LOGE(TAG, "Could not allocate memory for esp_mqtt_glue_long_data_t");
            return NULL;
        }
        long_data->data = MEM_CALLOC_EXTRAM(1, event->total_data_len);
        if (!long_data->data) {
            ESP_LOGE(TAG, "Could not allocate %d bytes for received data.", event->total_data_len);
            return esp_mqtt_glue_free_long_data(long_data);
        }
        long_data->topic = strndup(event->topic, event->topic_len);
        if (!long_data->topic) {
            ESP_LOGE(TAG, "Could not allocate %d bytes for received topic.", event->topic_len);
            return esp_mqtt_glue_free_long_data(long_data);
        }
    }
    if (long_data) {
        memcpy(long_data->data + event->current_data_offset, event->data, event->data_len);

        if ((event->current_data_offset + event->data_len) == event->total_data_len) {
            esp_mqtt_glue_subscribe_callback(long_data->topic, strlen(long_data->topic),
                        long_data->data, event->total_data_len);
            return esp_mqtt_glue_free_long_data(long_data);
        }
    }
    return long_data;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    /* A timer command that found the timer queue full is re-issued on the next event of any kind */
    bool rearm = false;
    glue_lock();
    if (mqtt_data && mqtt_data->timer_arm_pending) {
        mqtt_data->timer_arm_pending = false;
        rearm = true;
    }
    glue_unlock();
    if (rearm) {
        esp_mqtt_glue_schedule_sub_retry();
    }

    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Connected");
            glue_lock();
            mqtt_data->connected = true;
            /* Broker-side subscriptions did not survive the reconnect; re-send all of them */
            esp_mqtt_glue_reset_subscription_states_locked();
            mqtt_data->sub_retry_delay_ms = MQTT_SUB_RETRY_BASE_MS;
            glue_unlock();
            esp_mqtt_glue_resubscribe_pending();
            esp_mqtt_glue_schedule_sub_retry();
            esp_event_post(RMAKER_COMMON_EVENT, RMAKER_MQTT_EVENT_CONNECTED, NULL, 0, portMAX_DELAY);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT Disconnected. Will try reconnecting in a while...");
            esp_mqtt_glue_mark_disconnected();
            esp_event_post(RMAKER_COMMON_EVENT, RMAKER_MQTT_EVENT_DISCONNECTED, NULL, 0, portMAX_DELAY);
            break;
        case MQTT_EVENT_SUBSCRIBED: {
            /* esp-mqtt reports a SUBACK failure code (e.g. broker throttling) as a normal
             * SUBSCRIBED event with the error type set, so check it before trusting the ack.
             */
            bool rejected = event->error_handle &&
                            event->error_handle->error_type == MQTT_ERROR_TYPE_SUBSCRIBE_FAILED;
            char *topic = NULL;
            glue_lock();
            for (int i = 0; i < MAX_MQTT_SUBSCRIPTIONS; i++) {
                esp_mqtt_glue_subscription_t *sub = mqtt_data->subscriptions[i];
                if (sub && sub->state == MQTT_SUB_STATE_REQUESTED && sub->msg_id == event->msg_id) {
                    sub->state = rejected ? MQTT_SUB_STATE_FAILED : MQTT_SUB_STATE_ACKNOWLEDGED;
                    if (!topic) {
                        topic = strdup(sub->topic);
                    }
                }
            }
            if (!topic) {
                /* Either the sending task has not recorded this msg_id yet (see
                 * esp_mqtt_glue_send_subscribe()), or the topic was unsubscribed meanwhile.
                 * Park the result for the former case; it expires harmlessly in the latter. */
                esp_mqtt_glue_park_early_ack_locked(event->msg_id, rejected);
            }
            glue_unlock();
            if (topic) {
                esp_mqtt_glue_report_suback(topic, rejected);
                free(topic);
            } else {
                ESP_LOGD(TAG, "SUBACK for msg_id %d has no recorded request yet", event->msg_id);
            }
            break;
        }
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGD(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            esp_event_post(RMAKER_COMMON_EVENT, RMAKER_MQTT_EVENT_PUBLISHED, &event->msg_id, sizeof(event->msg_id), portMAX_DELAY);
            break;
#ifdef CONFIG_MQTT_REPORT_DELETED_MESSAGES
        case MQTT_EVENT_DELETED: {
            ESP_LOGD(TAG, "MQTT_EVENT_DELETED, msg_id=%d", event->msg_id);
            /* An un-acked SUBSCRIBE dropped from the esp-mqtt outbox counts as a failure.
             * esp-mqtt msg_ids are shared by PUBLISH, SUBSCRIBE and UNSUBSCRIBE and this event
             * carries no packet type, so a dropped QoS1 publish whose id collides with a still
             * outstanding subscribe is misread as a dropped subscribe. That needs the 16-bit id
             * space to wrap while the subscribe is unacknowledged, and costs one redundant
             * re-subscribe, so it is tolerated. */
            char *topic = NULL;
            glue_lock();
            for (int i = 0; i < MAX_MQTT_SUBSCRIPTIONS; i++) {
                esp_mqtt_glue_subscription_t *sub = mqtt_data->subscriptions[i];
                if (sub && sub->state == MQTT_SUB_STATE_REQUESTED && sub->msg_id == event->msg_id) {
                    sub->state = MQTT_SUB_STATE_FAILED;
                    if (!topic) {
                        topic = strdup(sub->topic);
                    }
                }
            }
            glue_unlock();
            esp_event_post(RMAKER_COMMON_EVENT, RMAKER_MQTT_EVENT_MSG_DELETED, &event->msg_id, sizeof(event->msg_id), portMAX_DELAY);
            if (topic) {
                ESP_LOGW(TAG, "Subscribe request for %s was dropped without a SUBACK. Will retry.", topic);
                esp_event_post(RMAKER_COMMON_EVENT, RMAKER_MQTT_EVENT_SUBSCRIBE_FAILED, topic, strlen(topic) + 1, portMAX_DELAY);
                free(topic);
                esp_mqtt_glue_schedule_sub_retry();
            }
            break;
        }
#endif /* CONFIG_MQTT_REPORT_DELETED_MESSAGES */
        case MQTT_EVENT_DATA: {
            ESP_LOGD(TAG, "MQTT_EVENT_DATA");
            static esp_mqtt_glue_long_data_t *long_data;
            /* Topic can be NULL, for data longer than the MQTT buffer */
            if (event->topic) {
                ESP_LOGD(TAG, "TOPIC=%.*s\r\n", event->topic_len, event->topic);
            }
            ESP_LOGD(TAG, "DATA=%.*s\r\n", event->data_len, event->data);
            if (event->data_len == event->total_data_len) {
                /* If long_data still exists, it means there was some issue getting the
                 * long data, and so, it needs to be freed up.
                 */
                if (long_data) {
                    long_data = esp_mqtt_glue_free_long_data(long_data);
                }
                esp_mqtt_glue_subscribe_callback(event->topic, event->topic_len, event->data, event->data_len);
            } else {
                long_data = esp_mqtt_glue_manage_long_data(long_data, event);
            }
            break;
        }
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGD(TAG, "Other event id:%d", event->event_id);
            break;
    }
}

static esp_err_t esp_mqtt_glue_connect(void)
{
    if (!mqtt_data) {
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Connecting to %s", mqtt_data->conn_params->mqtt_host);
    esp_err_t ret = esp_mqtt_client_start(mqtt_data->mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_mqtt_client_start() failed with err = %d", ret);
        return ret;
    }
    return ESP_OK;
}

static void esp_mqtt_glue_unsubscribe_all(void)
{
    if (!mqtt_data) {
        return;
    }
    esp_mqtt_glue_subscription_t *subs[MAX_MQTT_SUBSCRIPTIONS];
    bool send_unsubscribe[MAX_MQTT_SUBSCRIPTIONS];
    int count = 0;
    glue_lock();
    for (int i = 0; i < MAX_MQTT_SUBSCRIPTIONS; i++) {
        if (mqtt_data->subscriptions[i]) {
            subs[count] = esp_mqtt_glue_detach_locked(&mqtt_data->subscriptions[i], &send_unsubscribe[count]);
            count++;
        }
    }
    glue_unlock();
    for (int i = 0; i < count; i++) {
        esp_mqtt_glue_release(subs[i], send_unsubscribe[i]);
    }
}

static esp_err_t esp_mqtt_glue_disconnect(void)
{
    if (!mqtt_data) {
        return ESP_FAIL;
    }
    esp_mqtt_glue_unsubscribe_all();
    esp_err_t err = esp_mqtt_client_stop(mqtt_data->mqtt_client);
    esp_mqtt_glue_mark_disconnected();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disconnect from MQTT");
    } else {
        ESP_LOGI(TAG, "MQTT Disconnected.");
    }
    return err;
}
#ifdef ESP_RMAKER_MQTT_USE_PORT_443
static const char *alpn_protocols[] = { "x-amzn-mqtt-ca", NULL };
#endif /* ESP_RMAKER_MQTT_USE_PORT_443 */

/* Static helper to create MQTT client config from connection params */
static esp_mqtt_client_config_t esp_mqtt_glue_create_client_config(esp_rmaker_mqtt_conn_params_t *conn_params)
{
#ifdef CONFIG_ESP_RMAKER_MQTT_SEND_USERNAME
    const char *username = esp_get_aws_ppi();
#endif
    esp_mqtt_client_config_t mqtt_client_cfg = {
        .broker = {
            .address = {
                .hostname = conn_params->mqtt_host,
#ifdef ESP_RMAKER_MQTT_USE_PORT_443
                .port = 443,
#else
                .port = 8883,
#endif
                .transport = MQTT_TRANSPORT_OVER_SSL,
            },
            .verification = {
#ifdef ESP_RMAKER_MQTT_USE_PORT_443
                .alpn_protos = alpn_protocols,
#endif
#ifdef ESP_RMAKER_MQTT_USE_CERT_BUNDLE
                .crt_bundle_attach = esp_crt_bundle_attach,
#else
                .certificate = (const char *)conn_params->server_cert,
                .certificate_len = conn_params->server_cert_len,
#endif
            }
        },
        .credentials = {
#ifdef CONFIG_ESP_RMAKER_MQTT_SEND_USERNAME
            .username = username,
#endif
            .client_id = (const char *)conn_params->client_id,
            .authentication = {
                .certificate = (const char *)conn_params->client_cert,
                .certificate_len = conn_params->client_cert_len,
                .key = (const char *)conn_params->client_key,
                .key_len = conn_params->client_key_len,
                .ds_data = conn_params->ds_data
            },
        },
        .session = {
            .keepalive = CONFIG_ESP_RMAKER_MQTT_KEEP_ALIVE_INTERVAL,
            .last_will = {
                .topic = (const char *)conn_params->mqtt_last_will_topic,
                .msg = (const char *)conn_params->mqtt_last_will_message,
                .msg_len = conn_params->mqtt_last_will_message_len,
                .qos = RMAKER_MQTT_QOS1,
                .retain = false
            },
#ifdef CONFIG_ESP_RMAKER_MQTT_PERSISTENT_SESSION
            .disable_clean_session = 1,
#endif /* CONFIG_ESP_RMAKER_MQTT_PERSISTENT_SESSION */
        },
    };
    if (conn_params->use_ecdsa_peripheral) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 1)
        mqtt_client_cfg.credentials.authentication.use_ecdsa_peripheral = conn_params->use_ecdsa_peripheral;
        mqtt_client_cfg.credentials.authentication.ecdsa_key_efuse_blk = conn_params->ecdsa_key_efuse_blk;
#else
        ESP_LOGW(TAG, "MQTT ECDSA peripheral is supported only on ESP-IDF >= v5.5.1");
#endif
    }
    return mqtt_client_cfg;
}

/* Static helper to log LWT configuration */
static void esp_mqtt_glue_log_lwt(esp_rmaker_mqtt_conn_params_t *conn_params)
{
    if (conn_params->mqtt_last_will_topic) {
        ESP_LOGI(TAG, "MQTT LWT topic: %s", conn_params->mqtt_last_will_topic);
        if (conn_params->mqtt_last_will_message && conn_params->mqtt_last_will_message_len > 0) {
            ESP_LOGI(TAG, "MQTT LWT message: %.*s", (int)conn_params->mqtt_last_will_message_len,
                     conn_params->mqtt_last_will_message);
        }
    } else {
        ESP_LOGI(TAG, "MQTT LWT not configured");
    }
}

static esp_err_t esp_mqtt_glue_init(esp_rmaker_mqtt_conn_params_t *conn_params)
{
#ifdef CONFIG_ESP_RMAKER_MQTT_SEND_USERNAME
    const char *username = esp_get_aws_ppi();
    if (!username) {
        ESP_LOGE(TAG, "username received is NULL");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "AWS PPI: %s", username);
#endif
    if (!s_glue_lock) {
        s_glue_lock = xSemaphoreCreateRecursiveMutex();
        if (!s_glue_lock) {
            ESP_LOGE(TAG, "Failed to create MQTT glue lock");
            return ESP_ERR_NO_MEM;
        }
    }
    if (!s_sub_retry_timer) {
        s_sub_retry_timer = xTimerCreate("mqtt_sub_retry", pdMS_TO_TICKS(MQTT_SUB_RETRY_BASE_MS), pdFALSE, NULL,
                                         esp_mqtt_glue_sub_retry_timer_cb);
        if (!s_sub_retry_timer) {
            ESP_LOGW(TAG, "Could not create subscribe retry timer. Failed subscriptions will be retried only on reconnect.");
        }
    }
    if (mqtt_data) {
        ESP_LOGE(TAG, "MQTT already initialized");
        return ESP_OK;
    }
    if (!conn_params) {
        ESP_LOGE(TAG, "Connection params are mandatory for esp_mqtt_glue_init");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Initialising MQTT");
    esp_mqtt_glue_data_t *data = calloc(1, sizeof(esp_mqtt_glue_data_t));
    if (!data) {
        ESP_LOGE(TAG, "Failed to allocate memory for esp_mqtt_glue_data_t");
        return ESP_ERR_NO_MEM;
    }
    data->conn_params = conn_params;
    data->sub_retry_delay_ms = MQTT_SUB_RETRY_BASE_MS;

    esp_mqtt_client_config_t mqtt_client_cfg = esp_mqtt_glue_create_client_config(conn_params);
    esp_mqtt_glue_log_lwt(conn_params);

    data->mqtt_client = esp_mqtt_client_init(&mqtt_client_cfg);
    if (!data->mqtt_client) {
        ESP_LOGE(TAG, "esp_mqtt_client_init failed");
        free(data);
        return ESP_FAIL;
    }
    /* Publish the instance only once it is complete; glue_begin_op() checks it under the lock */
    glue_lock();
    mqtt_data = data;
    glue_unlock();
    esp_mqtt_client_register_event(data->mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    return ESP_OK;
}

static void esp_mqtt_glue_deinit(void)
{
    if (!s_glue_lock) {
        return;
    }
    glue_lock();
    if (!mqtt_data || mqtt_data->deleting) {
        glue_unlock();
        return;
    }
    mqtt_data->deleting = true;
    glue_unlock();

    if (s_sub_retry_timer) {
        xTimerStop(s_sub_retry_timer, portMAX_DELAY);
    }
    /* Wait for a retry round or a subscribe/unsubscribe that is already past glue_begin_op().
     * Anything that starts after this point is turned away by the deleting flag. */
    for (;;) {
        glue_lock();
        int busy = mqtt_data->busy;
        glue_unlock();
        if (busy == 0) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    esp_mqtt_glue_unsubscribe_all();
    if (mqtt_data->mqtt_client) {
        esp_mqtt_client_destroy(mqtt_data->mqtt_client);
    }
    glue_lock();
    free(mqtt_data);
    mqtt_data = NULL;
    glue_unlock();
}

/* Update MQTT config (including LWT) and reconnect.
 * Subscriptions are preserved across the reconnection.
 */
static esp_err_t esp_mqtt_glue_update_config(esp_rmaker_mqtt_conn_params_t *conn_params)
{
    if (!mqtt_data || !mqtt_data->mqtt_client) {
        ESP_LOGE(TAG, "MQTT not initialized, cannot update config");
        return ESP_ERR_INVALID_STATE;
    }
    if (!conn_params) {
        ESP_LOGE(TAG, "Connection params are mandatory for update_config");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Updating MQTT config and reconnecting");

    /* Stop the MQTT client (disconnect) */
    esp_err_t err = esp_mqtt_client_stop(mqtt_data->mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to stop MQTT client: %d", err);
        /* Continue anyway - try to update config */
    }
    esp_mqtt_glue_mark_disconnected();

    /* Update the stored conn_params */
    mqtt_data->conn_params = conn_params;

    /* Create new config with updated params */
    esp_mqtt_client_config_t mqtt_client_cfg = esp_mqtt_glue_create_client_config(conn_params);
    esp_mqtt_glue_log_lwt(conn_params);

    /* Update the client config using esp_mqtt_set_config */
    err = esp_mqtt_set_config(mqtt_data->mqtt_client, &mqtt_client_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update MQTT config: %d", err);
        return err;
    }

    /* Start the client again (connect) */
    err = esp_mqtt_client_start(mqtt_data->mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %d", err);
        return err;
    }

    ESP_LOGI(TAG, "MQTT config updated, reconnecting...");
    return ESP_OK;
}

esp_err_t esp_rmaker_mqtt_glue_setup(esp_rmaker_mqtt_config_t *mqtt_config)
{
    mqtt_config->init           = esp_mqtt_glue_init;
    mqtt_config->deinit         = esp_mqtt_glue_deinit;
    mqtt_config->connect        = esp_mqtt_glue_connect;
    mqtt_config->disconnect     = esp_mqtt_glue_disconnect;
    mqtt_config->publish        = esp_mqtt_glue_publish;
    mqtt_config->subscribe      = esp_mqtt_glue_subscribe;
    mqtt_config->unsubscribe    = esp_mqtt_glue_unsubscribe;
    mqtt_config->update_config  = esp_mqtt_glue_update_config;
    mqtt_config->setup_done     = true;
    return ESP_OK;
}
