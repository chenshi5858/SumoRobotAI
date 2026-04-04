# Ejercicio 6

En este ejercicio, el repositorio contiene el archivo `captura_hex_esp32cam.txt` como evidencia/salida capturada para ESP32-CAM.

## Ejecución

Actualmente, dentro de `Ejercicio_6/` no hay un proyecto ESP-IDF compilable (la carpeta `esp32-camera/` está vacía en este workspace). Por lo tanto, este ejercicio **no se puede compilar/flash** directamente desde aquí.

Si agregas o enlazas un proyecto ESP-IDF para **ESP32 / ESP32-CAM**, la ejecución típica es:

1. Exportar el entorno de ESP-IDF (ruta típica en Linux):

   ```bash
   source ~/esp-idf/export.sh
   ```

   Si tu ESP-IDF está en otra ubicación, ajusta la ruta.

2. Asegurar el target correcto:

   ```bash
   idf.py set-target esp32s3
   ```

3. Elegir el puerto correcto y ejecutar build/flash/monitor:

   ```bash
   idf.py build
   idf.py -p /dev/<PORT> flash
   idf.py -p /dev/<PORT> monitor
   ```

## Puerto serial

En muchos setups de ESP32-CAM el puerto suele aparecer como `/dev/ttyUSB0`, pero siempre valida el puerto real de tu equipo.
