# Ring Detector ESP-NOW

Sistema dividido en dos firmwares ESP-IDF:

```text
ringDetector/
├── esp32cam/
│   └── main/
└── esp32s3/
    └── main/
```

## Flujo de comunicacion

1. La ESP32-CAM captura continuamente frames `96x96` en `PIXFORMAT_RGB565`.
2. Analiza una franja superior configurada con `BORDER_ROI_*`.
3. Detecta el limite solo con convolucion Sobel sobre luminancia, pensado para casos donde dentro y fuera del ring tienen colores similares.
4. Confirma la deteccion con `BORDER_DEBOUNCE_COUNT` para evitar falsos positivos de un solo frame.
5. Si hay borde confirmado, envia `WHITE` por ESP-NOW; si no, envia `SAFE`.
6. La ESP32-S3 recibe el mensaje:
   - `SAFE`: avanza con `move_forward()`.
   - `WHITE`: ejecuta `rotate_fast()` por un tiempo corto y luego detiene.
7. La S3 funciona de forma autonoma; no abre servidor web ni recibe comandos desde el computador.

## MAC addresses y canal ESP-NOW

ESP-NOW necesita que ambos dispositivos trabajen en el mismo canal WiFi.

- En `esp32s3`, el canal se configura fijo en `ESPNOW_CHANNEL`. El firmware imprime en monitor:
  - `ESP32-S3 STA MAC`
  - `ESP-NOW channel`
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

No hay pagina web ni control desde computador en esta version; el comportamiento queda completo en las ESP.
