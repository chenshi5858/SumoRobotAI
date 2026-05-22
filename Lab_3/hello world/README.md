# Hello World - TensorFlow Lite Micro

Este proyecto corre el ejemplo `hello_world` usando el runtime de TensorFlow
Lite Micro. Esta es la version de referencia para comparar contra
`Lab_3/1.1`, que implementa el mismo modelo directamente en C.

La salida por serial usa las mismas columnas que la implementacion directa:

```text
source,x,y_model,y_sin,avg_inference_us,q_input,q_output
tfmicro,0.000000,0.000000,0.000000,123.456,-128,4
```

`avg_inference_us` mide el tiempo promedio de `interpreter->Invoke()` sobre
`BENCHMARK_REPEATS` repeticiones.

## Compilar y flashear

Por el espacio en el nombre de la carpeta, usar comillas:

```bash
cd "Lab_3/hello world"
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Si tu placa es ESP32-S3, usar:

```bash
idf.py set-target esp32s3
```

## Medicion de potencia

Para generar una ventana de medicion durante `interpreter->Invoke()`, editar
`main/lab3_tfmicro_config.h` y cambiar `MEASURE_GPIO` de `-1` a un pin libre.
Ese pin queda en alto solo mientras corre el benchmark.

Luego se puede comparar:

- tiempo promedio de `tfmicro` contra `direct`
- consumo durante la ventana de GPIO
- forma de la salida `y_model` contra `y_sin`
