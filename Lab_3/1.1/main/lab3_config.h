#ifndef LAB3_CONFIG_H_
#define LAB3_CONFIG_H_

/* Cantidad de inferencias usadas para promediar el tiempo en cada punto. */
#define BENCHMARK_REPEATS 1000

/*
 * Pin opcional para medir potencia con osciloscopio, analizador logico o power
 * profiler. Si se deja en -1, no se configura ningun GPIO.
 */
#define MEASURE_GPIO -1

/* Mismo ciclo usado por el ejemplo hello_world de TensorFlow Lite Micro. */
#define INFERENCES_PER_CYCLE 20
#define OUTPUT_PERIOD_MS 500

#endif  // LAB3_CONFIG_H_
