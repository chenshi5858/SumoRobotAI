#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef enum {
    RING_STATE_NO_LINK,
    RING_STATE_SAFE,
    RING_STATE_EDGE_WAIT_TURN,
    RING_STATE_EDGE_TURNING,
    RING_STATE_EDGE_RECOVERY,
} ring_state_t;

typedef struct {
    ring_state_t state;
    int64_t last_rx_ms;
    int64_t wait_until_ms;
    char last_value[4];
} ring_context_t;

void ring_context_init(ring_context_t *ctx);
void ring_context_update(ring_context_t *ctx, const char *status, int64_t rx_time_ms);
const char *ring_context_get_action(const ring_context_t *ctx, int64_t now_ms);
bool ring_context_is_timed_out(const ring_context_t *ctx, int64_t now_ms, int64_t timeout_ms);
