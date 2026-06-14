#pragma once

#include <stdbool.h>
#include <stdint.h>

#define ROBOT_STATUS_TEXT_LEN 16

typedef struct {
    char ring_status[ROBOT_STATUS_TEXT_LEN];
    char motion[ROBOT_STATUS_TEXT_LEN];
    char beacon_pos[ROBOT_STATUS_TEXT_LEN];
    uint8_t source_mac[6];
    bool have_source_mac;
    int64_t last_update_ms;
    int64_t last_beacon_update_ms;
    uint8_t beacon_class_id;
} robot_status_snapshot_t;

void robot_state_init(void);
void robot_state_set_ring_status(const char *status, const uint8_t *source_mac);
void robot_state_set_beacon(uint8_t class_id);
void robot_state_set_motion(const char *motion);
void robot_state_get_snapshot(robot_status_snapshot_t *out);
