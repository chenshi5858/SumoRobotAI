# Análisis De Espectro

Esta carpeta agrega una variante separada del ejercicio para capturar la señal del micrófono, calcular su FFT y exportar una FFT cada 1 segundo en un formato `CSV` compacto por el monitor serie. Luego, un script de Python toma el log y genera los `CSV` ordenados y un espectrograma `PNG` apilando varias FFT sucesivas en el tiempo. Así evitamos cargar la memoria de la ESP32 con guardado de imágenes en la placa.

## Qué Incluye

- [main/main.c](/home/yuusha1/esp/projects/labs/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_1/Ejercicio_5/analisis_espectro/main/main.c): arranque del sistema y tarea principal de análisis.
- [main/audio_sampler.c](/home/yuusha1/esp/projects/labs/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_1/Ejercicio_5/analisis_espectro/main/audio_sampler.c): captura continua desde ADC.
- [main/spectrum_analyzer.c](/home/yuusha1/esp/projects/labs/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_1/Ejercicio_5/analisis_espectro/main/spectrum_analyzer.c): ventana, FFT y detección del pico dominante.
- [main/spectrum_console.c](/home/yuusha1/esp/projects/labs/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_1/Ejercicio_5/analisis_espectro/main/spectrum_console.c): gráfico ASCII opcional por consola.
- [main/spectrum_export.c](/home/yuusha1/esp/projects/labs/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_1/Ejercicio_5/analisis_espectro/main/spectrum_export.c): exportación `CSV` compacta por serial.
- [tools/plot_spectrum.py](/home/yuusha1/esp/projects/labs/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_1/Ejercicio_5/analisis_espectro/tools/plot_spectrum.py): genera `CSV` limpios y espectrogramas `PNG` a partir del log.

## Parámetros Relevantes

En [main/app_config.h](/home/yuusha1/esp/projects/labs/Rep_SE_202601_grupo_Diego_Santiago_Chen/Lab_1/Ejercicio_5/analisis_espectro/main/app_config.h) puedes variar directamente:

- `SAMPLE_RATE`: frecuencia de muestreo del ADC.
- `FFT_SIZE`: cantidad de muestras usadas por la FFT.
- `PLOT_INTERVAL_MS`: tiempo entre espectros dibujados.
- `USE_HANN_WINDOW`: `1` usa ventana de Hann, `0` usa ventana rectangular.
- `ENABLE_ASCII_PLOT`: `1` mantiene el gráfico ASCII en el monitor, `0` deja solo la exportación `CSV`.
- `SPECTRUM_PLOT_WIDTH` y `SPECTRUM_PLOT_HEIGHT`: tamaño del gráfico ASCII.

Con la configuración actual:

- `SAMPLE_RATE = 16000 Hz`
- `FFT_SIZE = 1024`
- Resolución espectral `df = fs / N = 15.625 Hz`
- Ventana temporal analizada `N / fs = 64 ms`

## Ejecución

Desde esta carpeta:

```bash
source /home/yuusha1/esp/idf/esp-idf/export.sh
idf.py set-target esp32s3
idf.py build
```

Para ejecutar todo con un solo comando:

```bash
idf.py -p /dev/<PORT> flash monitor | tee monitor.log
```

Cada `1 s` verás en el monitor:

- una línea `SPECTRUM_CFG` con la configuración
- una línea `SPECTRUM_FRAME` con todos los bins de una FFT en formato CSV
- un resumen del pico dominante
- opcionalmente, el gráfico ASCII si `ENABLE_ASCII_PLOT = 1`

Luego puedes generar los archivos ordenados y el espectrograma con:

```bash
python3 tools/plot_spectrum.py monitor.log
```

Eso crea por defecto:

- `exports/spectrum_summary.csv`: una fila por captura
- `exports/spectrum_bins.csv`: una fila por bin de frecuencia
- `exports/plots/spectrogram_full_fs16000_....png`: espectrograma con todas las capturas disponibles
- `exports/plots/spectrogram_latest_fs16000_....png`: espectrograma de la ventana más reciente

Cada vez que vuelves a correr `plot_spectrum.py`, el script conserva los espectrogramas anteriores y genera archivos nuevos sin pisarlos. Además, cada imagen incluye el `SAMPLE_RATE` en el nombre del archivo y en el título del gráfico. Si alguna vez quieres limpiar los anteriores antes de generar una nueva tanda, puedes usar:

```bash
python3 tools/plot_spectrum.py monitor.log --clean-existing-plots
```

## Breve Análisis De Parámetros

- Al aumentar `SAMPLE_RATE`, sube la frecuencia máxima observable (`fs/2`). Si `FFT_SIZE` se mantiene fijo, la resolución en frecuencia empeora porque cada bin cubre más Hz.
- Al disminuir `SAMPLE_RATE`, mejoras la resolución para un `FFT_SIZE` fijo, pero puedes introducir aliasing si la señal contiene componentes por encima de Nyquist.
- Al aumentar `FFT_SIZE`, el pico espectral se localiza mejor y el gráfico se ve más estable, pero también aumenta la latencia porque necesitas más muestras antes de calcular la FFT.
- Al reducir `FFT_SIZE`, la respuesta es más rápida, aunque los picos aparecen más anchos y es más difícil distinguir frecuencias cercanas.
- Usar ventana de Hann (`USE_HANN_WINDOW = 1`) reduce la fuga espectral y limpia el fondo del gráfico cuando la frecuencia no cae exactamente en un bin de la FFT.
- Usar ventana rectangular (`USE_HANN_WINDOW = 0`) deja picos más estrechos en condiciones ideales, pero introduce lóbulos laterales más notorios y hace más visible la fuga espectral.
- Como el espectrograma se construye apilando varias FFT, su resolución temporal depende directamente de `PLOT_INTERVAL_MS`: con `1 s`, cada columna representa aproximadamente 1 segundo de evolución.
- Exportar una FFT por segundo mantiene una resolución temporal mucho más útil para el espectrograma, a costa de generar más datos por serial y logs más grandes.
- Sacar el guardado de imágenes desde la ESP32 reduce consumo de RAM y evita presión extra sobre la flash, a cambio de mover la generación del gráfico al computador host.

## Nota Sobre El Error De Stack

La versión anterior hacía la adquisición con un buffer grande dentro de `app_main()`, lo que provocaba el `stack overflow` que viste en `main`. Ahora:

- el buffer ADC vive en almacenamiento estático
- la lógica corre en una tarea dedicada con stack propio
- la exportación de datos es liviana y el graficado pesado se hace fuera de la placa
