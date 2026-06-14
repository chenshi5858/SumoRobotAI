#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define MIC_MAG_THRESHOLD 100000.0f
#define MIC_FREQ_TOL      100.0f

typedef enum {
    MIC_CMD_NONE = 0,
    MIC_CMD_FULL_FORWARD,
    MIC_CMD_STOP,
    MIC_CMD_ROTATE_LEFT,
    MIC_CMD_ROTATE_RIGHT,
    MIC_CMD_BOOST,
} mic_command_t;

typedef struct {
    float freq_hz;
    mic_command_t cmd;
} freq_cmd_entry_t;

static const freq_cmd_entry_t FREQ_CMD_TABLE[] = {
    { .freq_hz =  678.0f, .cmd = MIC_CMD_FULL_FORWARD },
    { .freq_hz = 1870.0f, .cmd = MIC_CMD_STOP          },
    { .freq_hz = 1234.0f, .cmd = MIC_CMD_ROTATE_LEFT   },
    { .freq_hz = 2456.0f, .cmd = MIC_CMD_ROTATE_RIGHT  },
    { .freq_hz = 3120.0f, .cmd = MIC_CMD_BOOST         },
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
