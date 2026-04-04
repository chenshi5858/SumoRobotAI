#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_attr.h"
#include "esp_cpu.h"
#include "esp_heap_caps.h"
#include "sdkconfig.h"

#define KB(x)                  ((size_t)(x) * 1024u)
#define NINTS(bytes)           ((bytes) / sizeof(int))
#define ARRAY_LEN(x)           (sizeof(x) / sizeof((x)[0]))
#define REPEATS                7

// -----------------------------
// Tamaños máximos por región
// Ajusta si tu board anda corto.
// -----------------------------
#define STATIC_DRAM_MAX_BYTES  KB(8)
#define STATIC_RTC_MAX_BYTES   KB(2)
#define FLASH_MAX_BYTES        KB(32)
#define FLASH_EVICT_BYTES      KB(62)

#define HEAP_DRAM_MAX_BYTES    KB(32)
#define HEAP_IRAM_MAX_BYTES    KB(16)
#define HEAP_PSRAM_MAX_BYTES   KB(64)

// Para flash source, el resultado va a DRAM interna (flash es RO)
#define FLASH_DST_MAX_BYTES    FLASH_MAX_BYTES

// Tamaños a barrer en el gráfico.
// Ojo: cada memoria solo imprimirá los tamaños que soporte.
static const size_t g_test_sizes_bytes[] = {
    256, 512, KB(1), KB(2), KB(4), KB(8), KB(16), KB(32), KB(64), KB(128)
};

// -----------------------------
// Buffers estáticos
// -----------------------------
DRAM_ATTR static int s_dram_src[NINTS(STATIC_DRAM_MAX_BYTES)];
DRAM_ATTR static int s_dram_dst[NINTS(STATIC_DRAM_MAX_BYTES)];
DRAM_ATTR static int s_dram_scalar = 5;

RTC_DATA_ATTR static int s_rtc_src[NINTS(STATIC_RTC_MAX_BYTES)];
RTC_DATA_ATTR static int s_rtc_dst[NINTS(STATIC_RTC_MAX_BYTES)];
RTC_DATA_ATTR static int s_rtc_scalar = 5;

// Flash const (DROM por defecto)
static const int s_flash_src[NINTS(FLASH_MAX_BYTES)] = {
    [0 ... (int)NINTS(FLASH_MAX_BYTES) - 1] = 1
};

// Buffer separado para “ensuciar” la caché compartida flash/PSRAM
static const int s_flash_evict[NINTS(FLASH_EVICT_BYTES)] = {
    [0 ... (int)NINTS(FLASH_EVICT_BYTES) - 1] = 7
};

static const int s_flash_scalar = 5;

// -----------------------------
// Buffers dinámicos
// -----------------------------
static int *g_dram_heap_src = NULL;
static int *g_dram_heap_dst = NULL;
static int *g_dram_heap_scalar = NULL;

static int *g_iram_heap_src = NULL;
static int *g_iram_heap_dst = NULL;
static int *g_iram_heap_scalar = NULL;

static int *g_psram_heap_src = NULL;
static int *g_psram_heap_dst = NULL;
static int *g_psram_heap_scalar = NULL;

// Resultado para caso flash source
static int *g_flash_dst_dram = NULL;

// Sink para evitar optimizaciones
DRAM_ATTR static volatile int32_t g_sink = 0;

// Spinlock para reducir jitter local durante la medición
static portMUX_TYPE g_bench_mux = portMUX_INITIALIZER_UNLOCKED;

typedef struct {
    const char *name;
    const int  *src;
    int        *dst;
    const int  *scalar_ptr;
    size_t      max_bytes;
    bool        enabled;
} bench_case_t;

// ----------------------------------------------------------
// Helpers
// ----------------------------------------------------------
static void fill_pattern(int *buf, size_t n, int seed)
{
    for (size_t i = 0; i < n; ++i) {
        buf[i] = seed + (int)(i & 0x7F);
    }
}

static bool is_aligned_4(const void *p)
{
    return (((uintptr_t)p & 0x3u) == 0u);
}

// Ensucia la caché compartida leyendo una región grande de flash.
// Está fuera del tramo medido.
static void IRAM_ATTR __attribute__((noinline)) evict_shared_cache(void)
{
    int32_t acc = 0;
    const size_t n = NINTS(FLASH_EVICT_BYTES);

    for (size_t i = 0; i < n; i += 8) {
        acc += s_flash_evict[i];
    }

    g_sink ^= acc;
}

// Kernel medido.
// Está en IRAM para que el código del benchmark no se vea afectado
// por misses de flash durante la medición.
static void IRAM_ATTR __attribute__((noinline))
multiply_vector_scalar_kernel(const int *src, int scalar, int *dst, size_t n)
{
    int32_t acc = 0;

    for (size_t i = 0; i < n; ++i) {
        int v = src[i];
        int r = v * scalar;
        dst[i] = r;
        acc += r;
    }

    g_sink = acc;
}

// Una sola medición “cold-ish”: primero evicción, luego medición.
// No hay printf dentro del tramo medido.
static uint32_t measure_once(const int *src, const int *scalar_ptr, int *dst, size_t n)
{
    evict_shared_cache();

    __asm__ __volatile__("" ::: "memory");

    portENTER_CRITICAL(&g_bench_mux);
    esp_cpu_set_cycle_count(0);

    int scalar = *scalar_ptr;
    multiply_vector_scalar_kernel(src, scalar, dst, n);

    uint32_t cycles = esp_cpu_get_cycle_count();
    portEXIT_CRITICAL(&g_bench_mux);

    __asm__ __volatile__("" ::: "memory");
    return cycles;
}

// Nos quedamos con el mínimo para aproximar la corrida menos contaminada por ruido.
static uint32_t measure_best_of_n(const int *src, const int *scalar_ptr, int *dst, size_t n, int repeats)
{
    uint32_t best = UINT32_MAX;

    for (int r = 0; r < repeats; ++r) {
        uint32_t cyc = measure_once(src, scalar_ptr, dst, n);
        if (cyc < best) {
            best = cyc;
        }
    }

    return best;
}

static bool alloc_dynamic_buffers(void)
{
    // DRAM interna pura: NO malloc(), para no arriesgar PSRAM.
    g_dram_heap_src = (int *)heap_caps_malloc(
        HEAP_DRAM_MAX_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    g_dram_heap_dst = (int *)heap_caps_malloc(
        HEAP_DRAM_MAX_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    g_dram_heap_scalar = (int *)heap_caps_malloc(
        sizeof(int), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    // IRAM heap: accesos int alineados de 32 bits.
g_iram_heap_src = (int *)heap_caps_malloc(
    HEAP_IRAM_MAX_BYTES, MALLOC_CAP_32BIT);
g_iram_heap_dst = (int *)heap_caps_malloc(
    HEAP_IRAM_MAX_BYTES, MALLOC_CAP_32BIT);
g_iram_heap_scalar = (int *)heap_caps_malloc(
    sizeof(int), MALLOC_CAP_32BIT);

    // PSRAM
    g_psram_heap_src = (int *)heap_caps_malloc(
        HEAP_PSRAM_MAX_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    g_psram_heap_dst = (int *)heap_caps_malloc(
        HEAP_PSRAM_MAX_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    g_psram_heap_scalar = (int *)heap_caps_malloc(
        sizeof(int), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    // Resultado para caso flash
    g_flash_dst_dram = (int *)heap_caps_malloc(
        FLASH_DST_MAX_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    bool ok = true;

    if (!g_dram_heap_src || !g_dram_heap_dst || !g_dram_heap_scalar) {
        printf("WARN,dram_heap_alloc_failed\n");
        ok = false;
    }
    if (!g_iram_heap_src || !g_iram_heap_dst || !g_iram_heap_scalar) {
        printf("WARN,iram_heap_alloc_failed\n");
    }
    if (!g_psram_heap_src || !g_psram_heap_dst || !g_psram_heap_scalar) {
        printf("WARN,psram_heap_alloc_failed\n");
    }
    if (!g_flash_dst_dram) {
        printf("WARN,flash_dst_dram_alloc_failed\n");
        ok = false;
    }

    if (g_dram_heap_src && g_dram_heap_dst && g_dram_heap_scalar) {
        if (!is_aligned_4(g_dram_heap_src) || !is_aligned_4(g_dram_heap_dst) || !is_aligned_4(g_dram_heap_scalar)) {
            printf("WARN,dram_heap_not_4byte_aligned\n");
        }
        fill_pattern(g_dram_heap_src, NINTS(HEAP_DRAM_MAX_BYTES), 10);
        fill_pattern(g_dram_heap_dst, NINTS(HEAP_DRAM_MAX_BYTES), 0);
        *g_dram_heap_scalar = 5;
    }

    if (g_iram_heap_src && g_iram_heap_dst && g_iram_heap_scalar) {
        if (!is_aligned_4(g_iram_heap_src) || !is_aligned_4(g_iram_heap_dst) || !is_aligned_4(g_iram_heap_scalar)) {
            printf("WARN,iram_heap_not_4byte_aligned\n");
        }
        fill_pattern(g_iram_heap_src, NINTS(HEAP_IRAM_MAX_BYTES), 20);
        fill_pattern(g_iram_heap_dst, NINTS(HEAP_IRAM_MAX_BYTES), 0);
        *g_iram_heap_scalar = 5;
    }

    if (g_psram_heap_src && g_psram_heap_dst && g_psram_heap_scalar) {
        if (!is_aligned_4(g_psram_heap_src) || !is_aligned_4(g_psram_heap_dst) || !is_aligned_4(g_psram_heap_scalar)) {
            printf("WARN,psram_heap_not_4byte_aligned\n");
        }
        fill_pattern(g_psram_heap_src, NINTS(HEAP_PSRAM_MAX_BYTES), 30);
        fill_pattern(g_psram_heap_dst, NINTS(HEAP_PSRAM_MAX_BYTES), 0);
        *g_psram_heap_scalar = 5;
    }

    if (g_flash_dst_dram) {
        fill_pattern(g_flash_dst_dram, NINTS(FLASH_DST_MAX_BYTES), 0);
    }

    return ok;
}

static void init_static_buffers(void)
{
    fill_pattern(s_dram_src, NINTS(STATIC_DRAM_MAX_BYTES), 1);
    fill_pattern(s_dram_dst, NINTS(STATIC_DRAM_MAX_BYTES), 0);
    s_dram_scalar = 5;

    fill_pattern(s_rtc_src, NINTS(STATIC_RTC_MAX_BYTES), 2);
    fill_pattern(s_rtc_dst, NINTS(STATIC_RTC_MAX_BYTES), 0);
    s_rtc_scalar = 5;
}

static void run_case(const bench_case_t *bc)
{
    if (!bc->enabled) {
        return;
    }

    for (size_t i = 0; i < ARRAY_LEN(g_test_sizes_bytes); ++i) {
        size_t working_set_bytes = g_test_sizes_bytes[i];

        if (working_set_bytes > bc->max_bytes) {
            continue;
        }

        size_t n = NINTS(working_set_bytes);
        uint32_t best_cycles = measure_best_of_n(
            bc->src, bc->scalar_ptr, bc->dst, n, REPEATS);

        // Por elemento: 4 bytes read src + 4 bytes write dst = 8 bytes.
        // Ignoramos la lectura única del escalar porque se amortiza.
        double bytes_touched = (double)n * 2.0 * sizeof(int);
        double cycles_per_byte = (double)best_cycles / bytes_touched;

        printf("%s,%u,%" PRIu32 ",%.6f,%" PRId32 "\n",
               bc->name,
               (unsigned)working_set_bytes,
               best_cycles,
               cycles_per_byte,
               g_sink);
    }
}

void app_main(void)
{
    // Dejar que el sistema se estabilice un poco antes de medir.
    vTaskDelay(pdMS_TO_TICKS(300));

    init_static_buffers();

    // Verificación básica de PSRAM disponible.
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    printf("INFO,psram_free_bytes,%u\n", (unsigned)psram_free);

    if (!alloc_dynamic_buffers()) {
        printf("ERROR,not_enough_internal_memory_for_benchmark\n");
        return;
    }

    bench_case_t cases[] = {
        {
            .name = "dram_static",
            .src = s_dram_src,
            .dst = s_dram_dst,
            .scalar_ptr = &s_dram_scalar,
            .max_bytes = STATIC_DRAM_MAX_BYTES,
            .enabled = true
        },
        {
            .name = "rtc_static",
            .src = s_rtc_src,
            .dst = s_rtc_dst,
            .scalar_ptr = &s_rtc_scalar,
            .max_bytes = STATIC_RTC_MAX_BYTES,
            .enabled = true
        },
        {
            .name = "flash_const",
            .src = s_flash_src,
            .dst = g_flash_dst_dram,
            .scalar_ptr = &s_flash_scalar,
            .max_bytes = FLASH_MAX_BYTES,
            .enabled = (g_flash_dst_dram != NULL)
        },
        {
            .name = "dram_heap_internal",
            .src = g_dram_heap_src,
            .dst = g_dram_heap_dst,
            .scalar_ptr = g_dram_heap_scalar,
            .max_bytes = HEAP_DRAM_MAX_BYTES,
            .enabled = (g_dram_heap_src && g_dram_heap_dst && g_dram_heap_scalar)
        },
        {
            .name = "iram_heap",
            .src = g_iram_heap_src,
            .dst = g_iram_heap_dst,
            .scalar_ptr = g_iram_heap_scalar,
            .max_bytes = HEAP_IRAM_MAX_BYTES,
            .enabled = (g_iram_heap_src && g_iram_heap_dst && g_iram_heap_scalar)
        },
        {
            .name = "psram_heap",
            .src = g_psram_heap_src,
            .dst = g_psram_heap_dst,
            .scalar_ptr = g_psram_heap_scalar,
            .max_bytes = HEAP_PSRAM_MAX_BYTES,
            .enabled = (g_psram_heap_src && g_psram_heap_dst && g_psram_heap_scalar)
        }
    };

    printf("mem_type,working_set_bytes,best_cycles,cycles_per_byte,checksum\n");

    for (size_t i = 0; i < ARRAY_LEN(cases); ++i) {
        run_case(&cases[i]);
    }

    printf("DONE\n");
}