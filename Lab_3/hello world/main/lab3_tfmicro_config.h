#ifndef LAB3_TFMICRO_CONFIG_H_
#define LAB3_TFMICRO_CONFIG_H_

#define BENCHMARK_REPEATS 1000
#define OUTPUT_PERIOD_MS 500

/*
 * Pin opcional para medir potencia durante interpreter->Invoke().
 * Dejar en -1 desactiva la ventana de medicion por GPIO.
 */
#define MEASURE_GPIO -1

#endif  // LAB3_TFMICRO_CONFIG_H_
