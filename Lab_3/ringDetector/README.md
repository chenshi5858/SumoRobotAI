# Ring Detector ESP-NOW

Sistema dividido en dos firmwares ESP-IDF:

```text
ringDetector/
├── esp32cam/
│   └── main/
└── esp32s3/
    ├── main/
    └── web_page/
```

## Flujo de comunicacion

1. La ESP32-CAM captura continuamente frames `96x96` en `PIXFORMAT_GRAYSCALE`.
2. Analiza solo el 25% inferior de la imagen.
3. Cuenta pixeles blancos con valor `> 200`.
4. Si mas del 15% de esa region es blanco, envia `WHITE` por ESP-NOW.
5. Si no, envia `SAFE`.
6. La ESP32-S3 recibe el mensaje:
   - `SAFE`: avanza con `move_forward()`.
   - `WHITE`: ejecuta `rotate_fast()` por un tiempo corto y luego detiene.
7. La S3 mantiene el servidor HTTP/WebSocket reutilizado desde `Lab_3/4` para enviar pose, movimiento y estado del ring a la pagina web.

## MAC addresses y canal ESP-NOW

ESP-NOW necesita que ambos dispositivos trabajen en el mismo canal WiFi.

- En `esp32s3`, el canal queda definido por el router al que se conecta. El firmware imprime en monitor:
  - `ESP32-S3 STA MAC`
  - `Current WiFi/ESP-NOW channel`
- En `esp32cam/main/app_config.h`, ajustar:
  - `ESPNOW_CHANNEL` al canal impreso por la S3.
  - `ESPNOW_PEER_MAC_BYTES` con la MAC STA de la S3.

Para primera prueba, la CAM queda en broadcast:

```c
#define ESPNOW_PEER_MAC_BYTES {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
```

Para produccion, usar la MAC real:

```c
#define ESPNOW_PEER_MAC_BYTES {0x24, 0x6F, 0x28, 0xAA, 0xBB, 0xCC}
```

En `esp32s3/main/app_config.h`, se puede filtrar por MAC de la CAM:

```c
#define ACCEPT_ANY_CAMERA_MAC 0
#define CAMERA_MAC_BYTES {0x24, 0x6F, 0x28, 0x11, 0x22, 0x33}
```

## Flasheo rapido

ESP32-CAM:

```bash
cd Lab_3/ringDetector/esp32cam
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

ESP32-S3:

```bash
cd Lab_3/ringDetector/esp32s3
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

Pagina web:

```bash
cd Lab_3/ringDetector/esp32s3/web_page
npm install
npm run dev
```

La IP del WebSocket aparece en el monitor de la S3 como `ws://IP:8000/ws`.
