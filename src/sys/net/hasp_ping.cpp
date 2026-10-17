/* MIT License - Copyright (c) 2019-2024 Francis Van Roie
   For full license information read the LICENSE file in the project folder */

#if defined(ARDUINO_ARCH_ESP32)

#include "hasplib.h"
#include "hasp_ping.h"

#include "lwip/ip_addr.h"
#include "ping/ping_sock.h"

#ifndef HASP_PING_COUNT
#define HASP_PING_COUNT 3
#endif

static esp_ping_handle_t ping_handle = NULL;
static volatile bool ping_done       = false;
static uint32_t ping_replies         = 0;
static uint32_t ping_time_ms         = 0; // sum of the reply times
static char ping_host[48]            = "";
static char ping_object[8]           = "";

/* The callbacks run on the lwIP ping task: only collect numbers, the LVGL and MQTT work is done in pingLoop() */
static void ping_on_success(esp_ping_handle_t hdl, void* args)
{
    uint32_t elapsed = 0;
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed, sizeof(elapsed));
    ping_time_ms += elapsed;
    ping_replies++;
}

static void ping_on_end(esp_ping_handle_t hdl, void* args)
{
    ping_done = true;
}

void dispatch_ping(const char*, const char* payload, uint8_t source)
{
    char host[sizeof(ping_host)] = "";
    char object[sizeof(ping_object)] = "";

    if(!payload || sscanf(payload, "%47s %7s", host, object) < 1) {
        LOG_WARNING(TAG_MSGR, F("ping: missing ip address"));
        return;
    }
    if(ping_handle) {
        LOG_WARNING(TAG_MSGR, F("ping: busy"));
        return;
    }

    ip_addr_t target;
    if(!ipaddr_aton(host, &target)) {
        LOG_WARNING(TAG_MSGR, F("ping: invalid ip address %s"), host);
        return;
    }

    esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
    config.target_addr       = target;
    config.count             = HASP_PING_COUNT;
    config.interval_ms       = 500;
    config.timeout_ms        = 1000;
    config.task_stack_size   = 4096;

    esp_ping_callbacks_t cbs = {};
    cbs.on_ping_success      = ping_on_success;
    cbs.on_ping_end          = ping_on_end;

    ping_done    = false;
    ping_replies = 0;
    ping_time_ms = 0;
    strncpy(ping_host, host, sizeof(ping_host));
    strncpy(ping_object, object, sizeof(ping_object));

    if(esp_ping_new_session(&config, &cbs, &ping_handle) != ESP_OK || esp_ping_start(ping_handle) != ESP_OK) {
        LOG_ERROR(TAG_MSGR, F("ping: could not start"));
        if(ping_handle) esp_ping_delete_session(ping_handle);
        ping_handle = NULL;
        return;
    }
    LOG_VERBOSE(TAG_MSGR, F("ping %s"), host);
}

void pingLoop(void)
{
    if(!ping_handle || !ping_done) return;

    esp_ping_delete_session(ping_handle);
    ping_handle = NULL;

    uint32_t avg = ping_replies ? ping_time_ms / ping_replies : 0;
    char buffer[128];

    snprintf_P(buffer, sizeof(buffer), PSTR("{\"host\":\"%s\",\"sent\":%u,\"received\":%u,\"time\":%u}"), ping_host,
               HASP_PING_COUNT, ping_replies, avg);
    dispatch_state_subtopic("ping", buffer);

    if(ping_object[0]) {
        if(ping_replies)
            snprintf_P(buffer, sizeof(buffer), PSTR("%s.text=%s: %u ms"), ping_object, ping_host, avg);
        else
            snprintf_P(buffer, sizeof(buffer), PSTR("%s.text=%s: timeout"), ping_object, ping_host);
        dispatch_text_line(buffer, TAG_MSGR);
    }
}

#endif // ARDUINO_ARCH_ESP32
