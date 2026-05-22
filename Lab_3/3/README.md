# Deteccion del identificador

Esta carpeta contiene lo necesario para la seccion 3 del Lab 3.

## `identifier_detection_esp/`

Proyecto ESP-IDF para correr el modelo cuantizado en la ESP32-CAM.

Por defecto parte sin camara y usa una imagen estatica, para poder compilar y
flashear antes de conectar la ESP-CAM:

```bash
cd identifier_detection_esp
source /home/chen/esp/idf/esp-idf/export.sh
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Cuando la camara AI Thinker este conectada:

```bash
idf.py menuconfig
```

Activar `Identifier detector -> Use ESP-CAM as image source`.

El LED de deteccion queda en GPIO 4 por defecto. La logica es:

- clase `0`: identificador ausente, LED apagado
- clases `1`, `2`, `3`: identificador detectado, LED encendido

## `training/`

Codigo y artefactos usados para entrenar/exportar el modelo:

- `skippoolcnn_tf.py`: entrenamiento, QAT y exportacion a TFLite.
- `preprocesar_datasets.py`: preprocesamiento de imagenes.
- `outputs_tf/tflite/cnn_skip_pool_tf_int8.tflite`: modelo cuantizado usado por ESP.
- `outputs_tf/*.keras`, `summary.csv`, `history.csv`, `report.md`: artefactos de entrenamiento.

Para regenerar el arreglo C del modelo usado en ESP:

```bash
cd identifier_detection_esp
python tools/tflite_to_c.py ../training/outputs_tf/tflite/cnn_skip_pool_tf_int8.tflite main/identifier_model_data.cc
idf.py build
```
