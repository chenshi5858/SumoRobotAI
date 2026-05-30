/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "detection_responder.h"
#include "tensorflow/lite/micro/micro_log.h"

#include "esp_main.h"
#include "driver/gpio.h"
#if DISPLAY_SUPPORT
#include "image_provider.h"
#include "bsp/esp-bsp.h"

// Camera definition is always initialized to match the trained detection model: 96x96 pix
// That is too small for LCD displays, so we extrapolate the image to 192x192 pix
#define IMG_WD (96 * 2)
#define IMG_HT (96 * 2)

static lv_obj_t *camera_canvas = NULL;
static lv_obj_t *person_indicator = NULL;
static lv_obj_t *label = NULL;

void create_gui(void)
{
  bsp_display_cfg_t cfg = {
    .lvgl_port_cfg = {
      .task_priority = CONFIG_BSP_DISPLAY_LVGL_TASK_PRIORITY,
      .task_stack = 6144,
      .task_affinity = 1,
      .task_max_sleep_ms = CONFIG_BSP_DISPLAY_LVGL_MAX_SLEEP,
      .timer_period_ms = CONFIG_BSP_DISPLAY_LVGL_TICK,
    },
    .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
    .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
    .flags = {
      .buff_dma = false,
      .buff_spiram = true,
    }
  };
  bsp_display_start_with_config(&cfg);
  bsp_display_backlight_on();

  bsp_display_lock(0);
  camera_canvas = lv_canvas_create(lv_scr_act());
  assert(camera_canvas);
  lv_obj_align(camera_canvas, LV_ALIGN_TOP_MID, 0, 0);

  person_indicator = lv_led_create(lv_scr_act());
  assert(person_indicator);
  lv_obj_align(person_indicator, LV_ALIGN_BOTTOM_MID, -70, 0);
  lv_led_set_color(person_indicator, lv_palette_main(LV_PALETTE_GREEN));

  label = lv_label_create(lv_scr_act());
  assert(label);
  lv_label_set_text_static(label, "Person detected");
  lv_obj_align_to(label, person_indicator, LV_ALIGN_OUT_RIGHT_MID, 20, 0);
  bsp_display_unlock();
}
#endif // DISPLAY_SUPPORT

#if CONFIG_CAMERA_MODULE_AI_THINKER
#define DETECTION_LED_GPIO GPIO_NUM_4
static bool detection_led_initialized = false;

static void detection_led_init(void)
{
  if (detection_led_initialized) {
    return;
  }

  gpio_config_t io_conf = {};
  io_conf.pin_bit_mask = 1ULL << DETECTION_LED_GPIO;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&io_conf);
  gpio_set_level(DETECTION_LED_GPIO, 0);
  detection_led_initialized = true;
}
#endif

void RespondToDetection(float person_score, float no_person_score) {
  int person_score_int = (person_score) * 100 + 0.5;
  int no_person_score_int = (no_person_score) * 100 + 0.5;
  bool person_detected = person_score_int >= 60;
#if DISPLAY_SUPPORT
  if (!camera_canvas) {
    create_gui();
  }

  uint16_t *buf = (uint16_t *) image_provider_get_display_buf();

  bsp_display_lock(0);
  if (!person_detected) { // treat score less than 60% as no person
    lv_led_off(person_indicator);
  } else {
    lv_led_on(person_indicator);
  }

  lv_canvas_set_buffer(camera_canvas, buf, IMG_WD, IMG_HT, LV_COLOR_FORMAT_RGB565);
  bsp_display_unlock();
#endif // DISPLAY_SUPPORT
#if CONFIG_CAMERA_MODULE_AI_THINKER
  detection_led_init();
  gpio_set_level(DETECTION_LED_GPIO, person_detected ? 1 : 0);
#endif
  MicroPrintf("result:%s person:%d%%, no_person:%d%%",
              person_detected ? "PERSON" : "NO_PERSON",
              person_score_int,
              no_person_score_int);
}
