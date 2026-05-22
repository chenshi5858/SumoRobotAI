# ESP32-S3 - Movimiento, WebSocket y ESP-NOW RX

Firmware ESP-IDF puro para la ESP32-S3. Reutiliza la logica de servidor HTTP/WebSocket y odometria abierta del proyecto `Lab_3/4`, ahora separada en modulos e integrada con ESP-NOW.

## Modulos

- `main.c`: inicializacion general y tarea de odometria.
- `wifi_app.c`: conexion WiFi STA y logs de IP/MAC/canal.
- `web_server.c`: servidor HTTP `POST /move` y WebSocket `/ws`.
- `motor_control.c`: control de motores, PWM, odometria, `move_forward()`, `rotate_fast()` y `stop_motors()`.
- `espnow_receiver.c`: receptor ESP-NOW robusto y validacion de mensajes.
- `ring_logic.c`: tarea FreeRTOS que decide movimiento segun `SAFE` o `WHITE`.
- `robot_state.c`: estado compartido para WebSocket y logs.

## Comportamiento

- Mensaje `SAFE`: la S3 llama `move_forward()`.
- Mensaje `WHITE`: la S3 llama `rotate_fast()`, espera `AVOID_ROTATE_MS` y luego llama `stop_motors()`.
- Si no llegan mensajes por `ESPNOW_STATUS_TIMEOUT_MS`, se detienen los motores y el estado pasa a `NO_LINK`.

La pagina web sigue usando el WebSocket original y ahora recibe tambien:

- `ring_status`
- `motion`
- `camera_mac`
- `espnow_age_ms`

## Configuracion

Editar `main/app_config.h`:

```c
#define WIFI_SSID "TU_WIFI"
#define WIFI_PASS "TU_PASSWORD"
```

Pines de motores:

```c
#define MOTOR_A_PIN1 6
#define MOTOR_A_PIN2 7
#define MOTOR_B_PIN1 16
#define MOTOR_B_PIN2 17
#define MOTOR_A_ENA 9
#define MOTOR_B_ENB 10
```

Filtro opcional por MAC de la CAM:

```c
#define ACCEPT_ANY_CAMERA_MAC 0
#define CAMERA_MAC_BYTES {0x24, 0x6F, 0x28, 0x11, 0x22, 0x33}
```

## Flashear

```bash
cd Lab_3/ringDetector/esp32s3
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

En el monitor buscar:

- `ESP32-S3 STA MAC`
- `Current WiFi/ESP-NOW channel`
- `WebSocket endpoint`

Usa la MAC y canal impresos para configurar la ESP32-CAM.

## Pagina web

```bash
cd Lab_3/ringDetector/esp32s3/web_page
npm install
npm run dev
```

Luego escribir la IP de la S3 en el campo de conexion, por ejemplo:

```text
http://192.168.1.98:8000
```
