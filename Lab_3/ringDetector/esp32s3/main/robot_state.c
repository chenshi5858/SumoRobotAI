#include "robot_state.h"

#include <string.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

static robot_status_snapshot_t s_state;
static portMUX_TYPE s_state_lock = portMUX_INITIALIZER_UNLOCKED;

void robot_state_init(void) {
    portENTER_CRITICAL(&s_state_lock);
    memset(&s_state, 0, sizeof(s_state));
    strlcpy(s_state.ring_status, "NO_LINK", sizeof(s_state.ring_status));
    strlcpy(s_state.motion, "stop", sizeof(s_state.motion));
    s_state.last_update_ms = -1;
    portEXIT_CRITICAL(&s_state_lock);
}

void robot_state_set_ring_status(const char *status, const uint8_t *source_mac) {
    if (status == NULL) {
        return;
    }

    portENTER_CRITICAL(&s_state_lock);
    strlcpy(s_state.ring_status, status, sizeof(s_state.ring_status));
    if (source_mac != NULL) {
        memcpy(s_state.source_mac, source_mac, sizeof(s_state.source_mac));
        s_state.have_source_mac = true;
    }
    s_state.last_update_ms = esp_timer_get_time() / 1000;
    portEXIT_CRITICAL(&s_state_lock);
}

void robot_state_set_motion(const char *motion) {
    if (motion == NULL) {
        return;
    }

    portENTER_CRITICAL(&s_state_lock);
    strlcpy(s_state.motion, motion, sizeof(s_state.motion));
    portEXIT_CRITICAL(&s_state_lock);
}

void robot_state_get_snapshot(robot_status_snapshot_t *out) {
    if (out == NULL) {
        return;
    }

    portENTER_CRITICAL(&s_state_lock);
    *out = s_state;
    portEXIT_CRITICAL(&s_state_lock);
}
