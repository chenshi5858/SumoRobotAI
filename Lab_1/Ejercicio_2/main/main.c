#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "image_data.h"

static uint8_t sobel_output[IMG_W * IMG_H];

static inline uint8_t clamp_u8(int value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return (uint8_t)value;
}

void apply_sobel(const uint8_t *input, uint8_t *output, int width, int height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {

            if (x == 0 || x == width - 1 || y == 0 || y == height - 1) {
                output[y * width + x] = 0;
                continue;
            }

            int p00 = input[(y - 1) * width + (x - 1)];
            int p01 = input[(y - 1) * width + (x    )];
            int p02 = input[(y - 1) * width + (x + 1)];

            int p10 = input[(y    ) * width + (x - 1)];
            int p12 = input[(y    ) * width + (x + 1)];

            int p20 = input[(y + 1) * width + (x - 1)];
            int p21 = input[(y + 1) * width + (x    )];
            int p22 = input[(y + 1) * width + (x + 1)];

            // Sobel horizontal (Gx)
            int gx = (-1 * p00) + (0 * p01) + (1 * p02)
                   + (-2 * p10) + (0)       + (2 * p12)
                   + (-1 * p20) + (0 * p21) + (1 * p22);

            // Sobel vertical (Gy)
            int gy = (-1 * p00) + (-2 * p01) + (-1 * p02)
                   + ( 0)       + ( 0)       + ( 0)
                   + ( 1 * p20) + ( 2 * p21) + ( 1 * p22);

            int magnitude = abs(gx) + abs(gy);
            magnitude /= 8;

            output[y * width + x] = clamp_u8(magnitude);
        }
    }
}

void print_image_serial(const uint8_t *image, int width, int height) {
    printf("BEGIN_IMAGE\n");
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            printf("%d", image[y * width + x]);
            if (x < width - 1) {
                printf(",");
            }
        }
        printf("\n");
    }
    printf("END_IMAGE\n");
}

void print_image_ascii(const uint8_t *image, int width, int height) {
    printf("BEGIN_ASCII\n");
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t p = image[y * width + x];

            if (p < 30) {
                printf("  ");
            } else if (p < 60) {
                printf("..");
            } else if (p < 100) {
                printf("**");
            } else if (p < 160) {
                printf("##");
            } else {
                printf("@@");
            }
        }
        printf("\n");
    }
    printf("END_ASCII\n");
}

void app_main(void) {
    printf("Procesando imagen con Sobel %dx%d...\n", IMG_W, IMG_H);

    apply_sobel(input_image, sobel_output, IMG_W, IMG_H);

    printf("Procesamiento terminado.\n");
    // print_image_serial(sobel_output, IMG_W, IMG_H);
    print_image_ascii(sobel_output, IMG_W, IMG_H);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}