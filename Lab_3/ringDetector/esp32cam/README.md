# ESP32-CAM - Vision y ESP-NOW TX

Firmware ESP-IDF puro para la ESP32-CAM. No usa Arduino.

## Modulos

- `main.c`: inicializa NVS, camara, ESP-NOW y crea `vision_task`.
- `camera_app.c`: contiene `init_camera()` y `detect_ring_border()`.
- `espnow_comm.c`: inicializa WiFi STA + ESP-NOW y contiene `send_status()`.
- `app_config.h`: pines, umbrales, canal y MAC peer.

## Vision

Configuracion de camara:

- Resolucion: `FRAMESIZE_96X96`.
- Formato: `PIXFORMAT_RGB565`.
- Analisis: franja superior configurada con `BORDER_ROI_*`.
- Borde: convolucion Sobel 3x3 sobre luminancia para detectar la transicion piso/limite aunque los colores sean parecidos.
- Salida estable: `BORDER_DEBOUNCE_COUNT` confirma la deteccion antes de mandar `WHITE`.

Mensajes enviados:

- `WHITE`: borde blanco detectado.
- `SAFE`: zona inferior sin suficiente blanco.

## Configuracion ESP-NOW

Editar `main/app_config.h`:

```c
#define ESPNOW_CHANNEL 1
#define ESPNOW_PEER_MAC_BYTES {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
```

El canal debe coincidir con el canal WiFi/ESP-NOW de la ESP32-S3. Para pruebas se usa broadcast. Para una conexion directa, cambiar `ESPNOW_PEER_MAC_BYTES` por la MAC STA de la S3 que aparece en su monitor serial.

## Flashear

```bash
cd Lab_3/ringDetector/esp32cam
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Logs utiles:

- `Local ESP32-CAM STA MAC`
- `ESP-NOW WiFi channel`
- `Vision status=SAFE`
- `Vision status=WHITE`
- `edge=... rows=... best=...`: fuerza, filas activas y mejor fila de la linea Sobel.
- `edge_thr=...`: umbral adaptativo Sobel usado en ese frame.

## Ajustes comunes

- Si marca todo como borde, subir `BORDER_EDGE_GRADIENT_MIN`, `BORDER_EDGE_GRADIENT_OFFSET` o `BORDER_EDGE_ROW_MIN_PIXELS`; tambien se puede bajar `BORDER_EDGE_MAX_ROWS`.
- Si no detecta el borde real, bajar `BORDER_EDGE_GRADIENT_MIN`, `BORDER_EDGE_GRADIENT_OFFSET` o `BORDER_EDGE_ROW_MIN_PIXELS`.
- Si detecta tarde, bajar `BORDER_DEBOUNCE_COUNT` o `CAPTURE_INTERVAL_MS`.
- Si la camara no inicia, revisar el pinout en `app_config.h`.
