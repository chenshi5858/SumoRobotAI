# Lab 3 - Ej4: Pose por WebSocket

Objetivo: el robot envia su posicion estimada a una pagina web en tiempo real. La pose se calcula por odometria abierta usando PWM (sin sensores).

## Variables clave para el calculo

Estas constantes estan en `main/main.c` y deben ajustarse al robot real:

- `WHEEL_RADIUS_M`: radio de la rueda (m). Usado para convertir RPM a velocidad lineal.
- `WHEEL_BASE_M`: distancia entre ruedas (m). Usada para calcular giro.
- `MOTOR_MAX_RPM`: RPM maxima estimada del motor (sin carga).
- `PWM_DEADBAND`: PWM minimo efectivo.
- `MOTOR_A_FORWARD_SIGN` / `MOTOR_B_FORWARD_SIGN`: signo de avance real por motor.
- `MOTOR_A_IS_LEFT`: define que motor corresponde a la rueda izquierda.
- `theta` inicial: se asume 0 rad y eje +Y como "frente".

Sin sensores (encoders o IMU) la pose es una estimacion y puede derivar con el tiempo.

## Firmware (ESP-IDF)

1) Edita `WIFI_SSID` y `WIFI_PASS` en `main/main.c`.
2) Compila y flashea:

```bash
cd Lab_3/4
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Logs utiles:
- `ws://IP:8000/ws` para WebSocket
- `http://IP:8000/move` para POST legacy

## Web page

```bash
cd Lab_3/4/web_page
npm install
npm run dev
```

Opcional: crea `.env` basado en `.env.example` con `VITE_API_URL`.

## Protocolo WebSocket

Comandos (cliente -> robot):
- `{ "action": "move", "direction": "forward" }`
- `{ "action": "stop" }`
- `{ "action": "speed_up" }` / `{ "action": "speed_down" }`
- `{ "action": "set_speed", "speed": 200 }`
- `{ "action": "reset_pose" }`

Mensajes (robot -> cliente):
- `type: "pose"` con `x`, `y`, `theta`, `theta_deg`, `v`, `omega`, `pwm_left`, `pwm_right`.

## Nota sobre el eje de referencia

Se usa un marco con `theta = 0` apuntando al eje +Y. La integracion es:

- `x += v * sin(theta) * dt`
- `y += v * cos(theta) * dt`
