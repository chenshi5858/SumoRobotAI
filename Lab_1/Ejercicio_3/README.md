# Ejercicio 3

Este ejercicio es un proyecto ESP-IDF que perfila (en ciclos y tiempo) distintas operaciones aritméticas en Xtensa y las reporta por consola en formato CSV.

## Requisitos

- ESP-IDF instalado y disponible en la shell.
- Placa objetivo: **ESP32-S3** (en este repo el `sdkconfig` está configurado para `esp32s3`).
- Tener identificado el puerto serie correcto (por ejemplo `/dev/ttyACM0` o `/dev/ttyUSB0`).

## Preparación del entorno

Exporta el entorno de ESP-IDF (ruta típica en Linux):

```bash
source ~/esp-idf/export.sh
```

Si tu ESP-IDF está en otra ubicación, ajusta la ruta.

## Verificar placa/target (ESP32 correcto)

Este ejercicio debe compilarse para **ESP32-S3**. Si tu entorno quedó apuntando a otro target, fuerza el target (solo cuando sea necesario):

```bash
idf.py set-target esp32s3
```

## Elegir el puerto correcto

Con la placa conectada, identifica el puerto (ejemplos típicos):

```bash
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

## Compilar, flashear y monitorear

Desde esta carpeta (`Ejercicio_3/`):

```bash
idf.py build
idf.py -p /dev/<PORT> flash
idf.py -p /dev/<PORT> monitor
```

O en un solo comando:

```bash
idf.py -p /dev/<PORT> flash monitor
```

## Exportar a CSV

Si quieres los datos en archivos `.csv` reales y separados por columnas, desde esta carpeta puedes usar:

```bash
python3 export_csv.py
```

Eso toma el log más reciente con líneas `CSV,...` y genera:

- `csv_output/metrics.csv`
- `csv_output/totals.csv`
- `csv_output/end_to_end.csv`
- `csv_output/checks.csv`
- `csv_output/metadata.csv`

Si quieres exportar desde un log específico:

```bash
python3 export_csv.py --input build/log/idf_py_stdout_output_<ID>
```

## Salida esperada

En el monitor deberías ver líneas tipo:

- `CSV,...` (métricas)
- `TOTAL_CSV,...` (totales acumulados de las mediciones)
- `END_TO_END,...` (tiempo y ciclos totales de la ejecución completa del ejercicio)
- `CHECK,...,PASS/FAIL`
