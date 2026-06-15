#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define MIC_MAG_THRESHOLD       5000000.0f
#define MIC_PEAK_TO_NOISE_RATIO 12.0f
#define MIC_FREQ_TOL            50.0f
#define MIC_CONFIRM_FRAMES      3
#define MIC_RELEASE_FRAMES      3

typedef enum {
    MIC_CMD_NONE = 0,
    MIC_CMD_FULL_FORWARD,
    MIC_CMD_BACKWARD,
} mic_command_t;

typedef struct {
    mic_command_t cmd;
    bool active;
} mic_command_event_t;

typedef struct {
    float freq_hz;
    mic_command_t cmd;
} freq_cmd_entry_t;

static const freq_cmd_entry_t FREQ_CMD_TABLE[] = {
    { .freq_hz =  678.0f, .cmd = MIC_CMD_FULL_FORWARD },
    { .freq_hz =  300.0f, .cmd = MIC_CMD_BACKWARD     },
};

#define FREQ_CMD_TABLE_COUNT (sizeof(FREQ_CMD_TABLE) / sizeof(FREQ_CMD_TABLE[0]))

static inline mic_command_t match_freq(float detected_hz, float magnitude) {
    if (magnitude < MIC_MAG_THRESHOLD) {
        return MIC_CMD_NONE;
    }

    for (int c = 0; c < FREQ_CMD_TABLE_COUNT; c++) {
        float diff = detected_hz - FREQ_CMD_TABLE[c].freq_hz;
        if (diff < 0.0f) diff = -diff;
        if (diff <= MIC_FREQ_TOL) {
            return FREQ_CMD_TABLE[c].cmd;
        }
    }

    return MIC_CMD_NONE;
}
