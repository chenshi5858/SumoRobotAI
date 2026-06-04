# ESP32-S3 - Movimiento autonomo y ESP-NOW RX

Firmware ESP-IDF puro para la ESP32-S3. La placa funciona de forma autonoma: recibe el estado de la ESP32-CAM por ESP-NOW y controla los motores sin servidor web ni comandos desde el computador.

## Modulos

- `main.c`: inicializacion general autonoma.
- `motor_control.c`: control de motores, PWM, `move_forward()`, `rotate_fast()` y `stop_motors()`.
- `espnow_receiver.c`: inicializa la radio en modo STA solo para ESP-NOW, recibe mensajes y valida MAC/payload.
- `ring_logic.c`: tarea FreeRTOS que decide movimiento segun `SAFE` o `WHITE`.
- `robot_state.c`: estado compartido interno para logs.

## Comportamiento

- Mensaje `SAFE`: la S3 llama `move_forward()`.
- Mensaje `WHITE`: la S3 llama `rotate_fast()` para mantenerse dentro del ring.
- Si no llegan mensajes por `ESPNOW_STATUS_TIMEOUT_MS`, se detienen los motores y el estado pasa a `NO_LINK`.

## Configuracion

Editar `main/app_config.h`:

```c
#define ESPNOW_CHANNEL 6
```

`ESPNOW_CHANNEL` debe coincidir con el canal configurado en la ESP32-CAM.

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
- `ESP-NOW channel`

Usa la MAC y canal impresos para configurar la ESP32-CAM.
