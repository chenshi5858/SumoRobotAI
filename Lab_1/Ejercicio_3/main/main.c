#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "xtensa/core-macros.h"

// -----------------------------
// Configuración
// -----------------------------
static const int32_t EXPECT_R0 = 233 + 128;
static const int32_t EXPECT_R1 = 233 + 10;
static const int32_t EXPECT_R2 = 233 % 128;
static const int32_t EXPECT_R3 = 233 * 128;
static const int32_t EXPECT_R4 = 233 / 128;

static volatile int32_t g_var1 = 233;
static volatile int32_t g_var2 = 128;

static volatile int32_t g_sink0 = 0;
static volatile int32_t g_sink1 = 0;
static volatile int32_t g_sink2 = 0;
static volatile int32_t g_sink3 = 0;
static volatile int32_t g_sink4 = 0;
static volatile int32_t g_acc   = 0;

// -----------------------------
// Métricas
// -----------------------------
typedef struct {
    uint32_t x;
    uint64_t time_us;
    uint32_t cycles;
    double   cycles_per_iter;
    double   time_from_cycles_us;
} metrics_t;

// -----------------------------
// Lectura de ciclos
// -----------------------------
static inline uint32_t read_cycle_count(void) {
    return xthal_get_ccount();
}

// -----------------------------
// Kernels
// -----------------------------
__attribute__((noinline))
static void kernel_all(uint32_t X)
{
    int32_t var1 = g_var1;
    int32_t var2 = g_var2;

    for (uint32_t i = 0; i < X; i++) {
        int32_t r0 = var1 + var2;
        int32_t r1 = var1 + 10;
        int32_t r2 = var1 % var2;
        int32_t r3 = var1 * var2;
        int32_t r4 = var1 / var2;

        g_sink0 = r0;
        g_sink1 = r1;
        g_sink2 = r2;
        g_sink3 = r3;
        g_sink4 = r4;

        g_acc += (r0 + r1 + r2 + r3 + r4);
    }
}

__attribute__((noinline))
static void kernel_add_vars(uint32_t X)
{
    int32_t var1 = g_var1;
    int32_t var2 = g_var2;

    for (uint32_t i = 0; i < X; i++) {
        int32_t r = var1 + var2;
        g_sink0 = r;
        g_acc += r;
    }
}

__attribute__((noinline))
static void kernel_add_const(uint32_t X)
{
    int32_t var1 = g_var1;

    for (uint32_t i = 0; i < X; i++) {
        int32_t r = var1 + 10;
        g_sink1 = r;
        g_acc += r;
    }
}

__attribute__((noinline))
static void kernel_mod(uint32_t X)
{
    int32_t var1 = g_var1;
    int32_t var2 = g_var2;

    for (uint32_t i = 0; i < X; i++) {
        int32_t r = var1 % var2;
        g_sink2 = r;
        g_acc += r;
    }
}

__attribute__((noinline))
static void kernel_mul(uint32_t X)
{
    int32_t var1 = g_var1;
    int32_t var2 = g_var2;

    for (uint32_t i = 0; i < X; i++) {
        int32_t r = var1 * var2;
        g_sink3 = r;
        g_acc += r;
    }
}

__attribute__((noinline))
static void kernel_div(uint32_t X)
{
    int32_t var1 = g_var1;
    int32_t var2 = g_var2;

    for (uint32_t i = 0; i < X; i++) {
        int32_t r = var1 / var2;
        g_sink4 = r;
        g_acc += r;
    }
}

// -----------------------------
// Medición
// -----------------------------
static metrics_t measure_kernel(void (*fn)(uint32_t), uint32_t X, uint32_t cpu_mhz)
{
    metrics_t m = {0};
    m.x = X;

    uint64_t t0 = esp_timer_get_time();
    uint32_t c0 = read_cycle_count();

    fn(X);

    uint32_t c1 = read_cycle_count();
    uint64_t t1 = esp_timer_get_time();

    m.time_us = t1 - t0;
    m.cycles  = c1 - c0;
    m.time_from_cycles_us = (double)m.cycles / (double)cpu_mhz;

    if (X > 0) {
        m.cycles_per_iter = (double)m.cycles / (double)X;
    } else {
        m.cycles_per_iter = 0.0;
    }

    return m;
}

// -----------------------------
// Validación
// -----------------------------
static bool check_results(void)
{
    return (g_sink0 == EXPECT_R0 &&
            g_sink1 == EXPECT_R1 &&
            g_sink2 == EXPECT_R2 &&
            g_sink3 == EXPECT_R3 &&
            g_sink4 == EXPECT_R4);
}

// -----------------------------
// CSV
// -----------------------------
static void print_metrics_csv(const char *label, metrics_t m)
{
    printf("CSV,%s,%" PRIu32 ",%" PRIu64 ",%" PRIu32 ",%.6f,%.3f\n",
           label,
           m.x,
           m.time_us,
           m.cycles,
           m.cycles_per_iter,
           m.time_from_cycles_us);
}

// -----------------------------
// MAIN
// -----------------------------
void app_main(void)
{
    const uint32_t cpu_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;

    const uint32_t X_values[] = {10, 100, 1000, 10000, 100000, 1000000};
    const size_t N = sizeof(X_values) / sizeof(X_values[0]);

    const uint32_t X_profile = 1000000;

    printf("==== EJERCICIO 3 (PROFILING) ====\n");
    printf("CPU_MHZ,%" PRIu32 "\n", cpu_mhz);
    printf("CSV_FORMAT,label,X,time_us,cycles,cycles_per_iter,time_from_cycles_us\n");

    // 1) Barrido de X para el kernel completo
    for (size_t i = 0; i < N; i++) {
        metrics_t m = measure_kernel(kernel_all, X_values[i], cpu_mhz);
        print_metrics_csv("all_ops", m);

        printf("CHECK,%" PRIu32 ",%s\n",
               X_values[i],
               check_results() ? "PASS" : "FAIL");
    }

    // 2) Profiling por operación con X fijo
    print_metrics_csv("add_vars",  measure_kernel(kernel_add_vars,  X_profile, cpu_mhz));
    print_metrics_csv("add_const", measure_kernel(kernel_add_const, X_profile, cpu_mhz));
    print_metrics_csv("mod",       measure_kernel(kernel_mod,       X_profile, cpu_mhz));
    print_metrics_csv("mul",       measure_kernel(kernel_mul,       X_profile, cpu_mhz));
    print_metrics_csv("div",       measure_kernel(kernel_div,       X_profile, cpu_mhz));

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}