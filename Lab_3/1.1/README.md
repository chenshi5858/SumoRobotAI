# 1.1 - Implementacion directa de `hello_world`

Este proyecto implementa directamente en C el modelo `hello_world` usado por
`esp-tflite-micro`. El modelo recibe un valor `x` entre `0` y `2*pi` y estima
`sin(x)` con tres capas densas cuantizadas en int8:

- `dense`: 1 entrada -> 16 neuronas, ReLU
- `dense_1`: 16 -> 16 neuronas, ReLU
- `dense_2`: 16 -> 1 salida

Los pesos, sesgos y parametros de cuantizacion fueron extraidos del ejemplo
`esp-tflite-micro:hello_world`. La inferencia no usa el interprete de
TensorFlow Lite Micro.

## Compilar y ejecutar

```bash
cd Lab_3/1.1
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

La salida por serial queda en formato CSV:

```text
source,x,y_model,y_sin,avg_inference_us,q_input,q_output
direct,0.000000,0.050832,0.000000,12.340,-128,10
```

`avg_inference_us` es el tiempo promedio de una inferencia directa, calculado
con `esp_timer_get_time()` sobre `BENCHMARK_REPEATS` repeticiones.

## Medicion de potencia

Para medir potencia durante la inferencia, editar `main/lab3_config.h` y cambiar
`MEASURE_GPIO` desde `-1` a un pin libre. El firmware pondra ese pin en alto
solo durante la ventana de benchmark. Con un power profiler, osciloscopio o
analizador logico se puede sincronizar el consumo de corriente con esa ventana.

La potencia se obtiene con:

```text
P = V * I
```

Luego comparar contra el ejemplo de TensorFlow Lite Micro ejecutado con las
mismas condiciones de placa, alimentacion y frecuencia.

## Comparar con TensorFlow Lite Micro

Ejecutar el ejemplo base y guardar su monitor:

```bash
cd ~/esp/projects_tf/hello_world
idf.py -p /dev/ttyUSB0 flash monitor | tee tfmicro.log
```

Ejecutar esta implementacion directa y guardar su monitor:

```bash
cd Lab_3/1.1
idf.py -p /dev/ttyUSB0 flash monitor | tee direct.log
```

Comparar salidas:

```bash
python3 tools/compare_outputs.py direct.log tfmicro.log
```

El script genera `comparison.csv` y, si `matplotlib` esta instalado,
`comparison.png`. Si `matplotlib` no esta instalado, genera
`comparison.svg`.

## Resultados capturados

Con el ESP32-CAM en `/dev/ttyUSB0` se flashearon ambos firmwares y se guardaron:

- `direct.log`: salida serial de esta implementacion directa.
- `tfmicro.log`: salida serial del ejemplo con TensorFlow Lite Micro.
- `comparison.csv`: comparacion punto a punto y diferencia de tiempo.
- `comparison.svg`: visualizacion de `sin(x)`, `direct C` y `TF Micro`.
- `power_measurements.csv`: mediciones de voltaje, corriente y potencia
  obtenidas con el medidor USB.
- `direct_power_measurement.png`: foto de la medicion de potencia para
  `direct`.
- `tfmicro_power_measurement.png`: foto de la medicion de potencia para
  `TF Micro`.

Resumen de tiempo promedio de inferencia:

```text
direct:   73.928 us
TF Micro: 61.463 us
direct - TF Micro: 12.465 us
```

Resumen de potencia medido:

```text
direct:   5.1018 V * 0.11205 A = 0.5717 W  (display: 0.5714 W)
TF Micro: 5.1083 V * 0.09782 A = 0.4997 W  (display: 0.5000 W)
direct - TF Micro: 0.0720 W
```

Las fotos de respaldo quedaron guardadas como `direct_power_measurement.png` y
`tfmicro_power_measurement.png`.
