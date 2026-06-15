#include "ring_logic.h"

#include <string.h>

#include "main_config.h"

void ring_context_init(ring_context_t *ctx) {
    if (ctx == NULL) {
        return;
    }

    ctx->state = RING_STATE_NO_LINK;
    ctx->last_rx_ms = -1;
    ctx->wait_until_ms = -1;
    ctx->last_value[0] = '\0';
}

void ring_context_update(ring_context_t *ctx, const char *status, int64_t rx_time_ms) {
    if (ctx == NULL || status == NULL) {
        return;
    }

    ctx->last_rx_ms = rx_time_ms;
    strlcpy(ctx->last_value, status, sizeof(ctx->last_value));
}

const char *ring_context_get_action(const ring_context_t *ctx, int64_t now_ms) {
    if (ctx == NULL) {
        return "stop";
    }

    if (ctx->state == RING_STATE_NO_LINK) {
        return "stop";
    }

    if (ctx->state == RING_STATE_EDGE_BACKING) {
        return "back";
    }

    if (ctx->state == RING_STATE_EDGE_WAIT_TURN) {
        if (ctx->wait_until_ms > 0 && now_ms >= ctx->wait_until_ms) {
            return "rotate_left";
        }
        return "stop";
    }

    if (ctx->state == RING_STATE_EDGE_TURNING) {
        return "rotate_left";
    }

    if (ctx->state == RING_STATE_EDGE_RECOVERY) {
        if (ctx->wait_until_ms > 0 && now_ms >= ctx->wait_until_ms) {
            return "forward";
        }
        return "stop";
    }

    if (ctx->state == RING_STATE_SAFE) {
        return "forward";
    }

    return "stop";
}

bool ring_context_is_timed_out(const ring_context_t *ctx, int64_t now_ms, int64_t timeout_ms) {
    if (ctx == NULL) {
        return true;
    }

    return ctx->last_rx_ms > 0 && (now_ms - ctx->last_rx_ms) > timeout_ms;
}
