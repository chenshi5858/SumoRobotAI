# Ejercicio1 - Firmware BLE para control de motores

Este directorio contiene un ejemplo de firmware para ESP32 que expone un servicio BLE sencillo
para recibir comandos de movimiento (`forward`, `back`, `left`, `right`, `stop`) desde una
aplicación web mediante Web Bluetooth.

Archivos principales:
- `main/main.c` : código del firmware (GATT server que acepta escrituras en una característica)
- `CMakeLists.txt` : CMake para ESP-IDF

Build y flasheo (desde máquina con ESP-IDF configurado):

```bash
cd Lab_2/ejercicio1
# configurar el entorno IDF (source /opt/esp/idf/export.sh u otra ruta)
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Notas:
- El ejemplo usa un servicio BLE con UUID 0xFFE0 y una característica 0xFFE1. La página web
  en `Lab_2/web_page` puede conectarse mediante Web Bluetooth a ese servicio y escribir
  cadenas ASCII con el comando (por ejemplo `forward`).
- Ajusta los pines de los motores en `main.c` según tu conexión física.
