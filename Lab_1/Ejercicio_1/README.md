## Cómo correr

```bash
source ~/.zprofile && source ~/.zshrc && get_esp32 && idf.py build && idf.py flash -p /dev/<PORT> monitor


```md
> Nota: En nuestro caso, el puerto corresponde a /dev/ttyUSB0 (ESP32-CAM) o /dev/ttyACM0 (ESP32-S3).
