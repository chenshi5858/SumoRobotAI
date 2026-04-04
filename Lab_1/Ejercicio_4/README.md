# Ejercicio 4

Este ejercicio es un proyecto ESP-IDF que ejecuta benchmarks de acceso/cómputo sobre distintos tipos de memoria (buffers estáticos y dinámicos en DRAM/IRAM/PSRAM y lectura desde flash) y reporta resultados por consola.

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

Desde esta carpeta (`Ejercicio_4/`):

```bash
idf.py build
idf.py -p /dev/<PORT> flash
idf.py -p /dev/<PORT> monitor
```

O en un solo comando:

```bash
idf.py -p /dev/<PORT> flash monitor
```

## Notas

- Si tu placa no tiene PSRAM habilitada, algunas pruebas pueden imprimirse como warnings o quedar deshabilitadas.
- Para repetir una corrida desde cero: `idf.py fullclean` y luego `idf.py build`.
