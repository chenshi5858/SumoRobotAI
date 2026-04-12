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

typedef struct {
    uint32_t runs;
    uint64_t total_x;
    uint64_t total_time_us;
    uint64_t total_cycles;
} totals_t;

// -----------------------------
// Lectura de ciclos
// -----------------------------
static inline uint32_t read_cycle_count(void) {
    return xthal_get_ccount();
}

static inline uint64_t cycle_delta_u64(uint32_t start, uint32_t end)
{
    return (uint64_t)((uint32_t)(end - start));
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

static void totals_add_metrics(totals_t *totals, metrics_t m)
{
    totals->runs += 1;
    totals->total_x += m.x;
    totals->total_time_us += m.time_us;
    totals->total_cycles += m.cycles;
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

static void print_totals_csv(const char *label, totals_t totals, uint32_t cpu_mhz)
{
    const double total_time_from_cycles_us = (double)totals.total_cycles / (double)cpu_mhz;
    const double avg_cycles_per_run = (totals.runs > 0)
        ? ((double)totals.total_cycles / (double)totals.runs)
        : 0.0;
    const double avg_cycles_per_iter = (totals.total_x > 0)
        ? ((double)totals.total_cycles / (double)totals.total_x)
        : 0.0;

    printf("TOTAL_CSV,%s,%" PRIu32 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%.6f,%.6f,%.3f\n",
           label,
           totals.runs,
           totals.total_x,
           totals.total_time_us,
           totals.total_cycles,
           avg_cycles_per_run,
           avg_cycles_per_iter,
           total_time_from_cycles_us);
}

static void print_end_to_end_csv(const char *label,
                                 uint64_t total_time_us,
                                 uint64_t total_cycles,
                                 uint32_t cpu_mhz)
{
    const double total_time_from_cycles_us = (double)total_cycles / (double)cpu_mhz;

    printf("END_TO_END,%s,%" PRIu64 ",%" PRIu64 ",%.3f\n",
           label,
           total_time_us,
           total_cycles,
           total_time_from_cycles_us);
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
    totals_t totals_all_ops = {0};
    totals_t totals_single_ops = {0};
    totals_t totals_all_kernels = {0};

    const uint64_t exercise_t0 = esp_timer_get_time();
    const uint32_t exercise_c0 = read_cycle_count();

    printf("==== EJERCICIO 3 (PROFILING) ====\n");
    printf("CPU_MHZ,%" PRIu32 "\n", cpu_mhz);
    printf("CSV_FORMAT,label,X,time_us,cycles,cycles_per_iter,time_from_cycles_us\n");

    // 1) Barrido de X para el kernel completo
    for (size_t i = 0; i < N; i++) {
        metrics_t m = measure_kernel(kernel_all, X_values[i], cpu_mhz);
        totals_add_metrics(&totals_all_ops, m);
        totals_add_metrics(&totals_all_kernels, m);
        print_metrics_csv("all_ops", m);

        printf("CHECK,%" PRIu32 ",%s\n",
               X_values[i],
               check_results() ? "PASS" : "FAIL");
    }

    // 2) Profiling por operación con X fijo
    metrics_t m_add_vars = measure_kernel(kernel_add_vars, X_profile, cpu_mhz);
    totals_add_metrics(&totals_single_ops, m_add_vars);
    totals_add_metrics(&totals_all_kernels, m_add_vars);
    print_metrics_csv("add_vars", m_add_vars);

    metrics_t m_add_const = measure_kernel(kernel_add_const, X_profile, cpu_mhz);
    totals_add_metrics(&totals_single_ops, m_add_const);
    totals_add_metrics(&totals_all_kernels, m_add_const);
    print_metrics_csv("add_const", m_add_const);

    metrics_t m_mod = measure_kernel(kernel_mod, X_profile, cpu_mhz);
    totals_add_metrics(&totals_single_ops, m_mod);
    totals_add_metrics(&totals_all_kernels, m_mod);
    print_metrics_csv("mod", m_mod);

    metrics_t m_mul = measure_kernel(kernel_mul, X_profile, cpu_mhz);
    totals_add_metrics(&totals_single_ops, m_mul);
    totals_add_metrics(&totals_all_kernels, m_mul);
    print_metrics_csv("mul", m_mul);

    metrics_t m_div = measure_kernel(kernel_div, X_profile, cpu_mhz);
    totals_add_metrics(&totals_single_ops, m_div);
    totals_add_metrics(&totals_all_kernels, m_div);
    print_metrics_csv("div", m_div);

    const uint64_t exercise_time_us = esp_timer_get_time() - exercise_t0;
    const uint64_t exercise_cycles = cycle_delta_u64(exercise_c0, read_cycle_count());

    printf("TOTAL_FORMAT,label,runs,total_iterations,total_time_us,total_cycles,avg_cycles_per_run,avg_cycles_per_iter,total_time_from_cycles_us\n");
    print_totals_csv("all_ops_sweep", totals_all_ops, cpu_mhz);
    print_totals_csv("single_op_profile", totals_single_ops, cpu_mhz);
    print_totals_csv("all_measured_kernels", totals_all_kernels, cpu_mhz);
    printf("END_TO_END_FORMAT,label,total_time_us,total_cycles,total_time_from_cycles_us\n");
    print_end_to_end_csv("exercise_3_full_run", exercise_time_us, exercise_cycles, cpu_mhz);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
