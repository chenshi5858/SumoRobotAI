# Ejercicio 2

Este ejercicio es un proyecto ESP-IDF que procesa una imagen (96x96) con Sobel y la imprime por consola (monitor serial) en formato ASCII.

## Requisitos

- ESP-IDF instalado y disponible en la shell.
- Placa objetivo: **ESP32-S3** (en este repo el `sdkconfig` ya está configurado para `esp32s3`).
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

Usa el puerto que corresponda a tu ESP32.

## Compilar, flashear y monitorear

Desde esta carpeta (`Ejercicio_2/`):

```bash
idf.py build
idf.py -p /dev/<PORT> flash
idf.py -p /dev/<PORT> monitor
```

O en un solo comando:

```bash
idf.py -p /dev/<PORT> flash monitor
```

## (Opcional) Regenerar la imagen (`main/image_data.h`)

Si necesitas cambiar la imagen de entrada, los scripts en Python generan el preview y el header.

- `transformar_imagen.py` toma `FAAHHHHHHH.png` y genera `input_preview.png`.
- `procesar_imagen.py` toma `input_preview.png` y genera `main/image_data.h`.

Ejemplo:

```bash
python3 transformar_imagen.py
python3 procesar_imagen.py
```

Luego vuelve a compilar con `idf.py build`.
