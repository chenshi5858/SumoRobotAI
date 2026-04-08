#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "image_data.h"

static const char *TAG = "HIST_EQ";

static void compute_histogram(const uint8_t *img, size_t size, uint32_t hist[256])
{
    memset(hist, 0, 256 * sizeof(uint32_t));

    for (size_t i = 0; i < size; i++) {
        hist[img[i]]++;
    }
}

static bool build_equalization_lut(const uint32_t hist[256], size_t total_pixels, uint8_t lut[256])
{
    uint32_t cdf[256];
    uint32_t accum = 0;
    uint32_t cdf_min = 0;
    bool found_nonzero = false;

    for (int i = 0; i < 256; i++) {
        accum += hist[i];
        cdf[i] = accum;

        if (!found_nonzero && hist[i] != 0) {
            cdf_min = cdf[i];
            found_nonzero = true;
        }
    }

    if (!found_nonzero) {
        // Imagen vacía o inválida
        for (int i = 0; i < 256; i++) {
            lut[i] = (uint8_t)i;
        }
        return false;
    }

    if (total_pixels == cdf_min) {
        // Todos los píxeles tienen el mismo valor
        for (int i = 0; i < 256; i++) {
            lut[i] = (uint8_t)i;
        }
        return false;
    }

    for (int i = 0; i < 256; i++) {
        // Fórmula clásica de ecualización:
        // s_k = round( (cdf(k) - cdf_min) / (N - cdf_min) * 255 )
        int32_t numerator = (int32_t)(cdf[i] - cdf_min);
        if (numerator < 0) {
            numerator = 0;
        }

        float value = ((float)numerator / (float)(total_pixels - cdf_min)) * 255.0f;

        if (value < 0.0f) value = 0.0f;
        if (value > 255.0f) value = 255.0f;

        lut[i] = (uint8_t)(value + 0.5f); // redondeo
    }

    return true;
}

static void apply_lut(const uint8_t *input, uint8_t *output, size_t size, const uint8_t lut[256])
{
    for (size_t i = 0; i < size; i++) {
        output[i] = lut[input[i]];
    }
}

static void print_histogram(const char *label, const uint32_t hist[256])
{
    printf("\n===== %s =====\n", label);
    for (int i = 0; i < 256; i++) {
        printf("%3d:%lu", i, (unsigned long)hist[i]);
        if ((i + 1) % 8 == 0) {
            printf("\n");
        } else {
            printf("  ");
        }
    }
    printf("===== END %s =====\n", label);
}

static void print_image_hex_block(const char *label, const uint8_t *img, int width, int height)
{
    printf("\n===== %s =====\n", label);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            printf("0x%02X", img[y * width + x]);
            if (!(y == height - 1 && x == width - 1)) {
                printf(",");
            }
        }
        printf("\n");
    }

    printf("===== END %s =====\n", label);
}

void app_main(void)
{
    static uint8_t equalized_img[IMG_SIZE];
    uint32_t hist_before[256];
    uint32_t hist_after[256];
    uint8_t lut[256];

    ESP_LOGI(TAG, "Iniciando ecualizacion de histograma...");
    ESP_LOGI(TAG, "Tamano imagen: %dx%d (%d pixeles)", IMG_W, IMG_H, IMG_SIZE);

    compute_histogram(input_img, IMG_SIZE, hist_before);

    bool changed = build_equalization_lut(hist_before, IMG_SIZE, lut);
    apply_lut(input_img, equalized_img, IMG_SIZE, lut);
    compute_histogram(equalized_img, IMG_SIZE, hist_after);

    if (changed) {
        ESP_LOGI(TAG, "Ecualizacion aplicada correctamente.");
    } else {
        ESP_LOGW(TAG, "La imagen era uniforme o invalida. Se mantuvo LUT identidad.");
    }

    // Imprime histogramas para evidencia
    print_histogram("HISTOGRAMA ORIGINAL", hist_before);
    print_histogram("HISTOGRAMA ECUALIZADO", hist_after);

    // Imprime imagen original y ecualizada para copiar desde monitor/serial
    // y reconstruirlas luego en Python/Colab si te lo piden.
    print_image_hex_block("IMAGEN ORIGINAL", input_img, IMG_W, IMG_H);
    print_image_hex_block("IMAGEN ECUALIZADA", equalized_img, IMG_W, IMG_H);

    ESP_LOGI(TAG, "Proceso finalizado.");
}