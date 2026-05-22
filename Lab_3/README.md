# Lab 3 - TensorFlow Lite Micro para ESP

## Base del laboratorio

La parte inicial del laboratorio prepara el componente de TensorFlow Lite Micro
para ESP-IDF y deja disponible el ejemplo `hello_world`, que se usa como
referencia para comparar la implementacion directa de la seccion 1.1.

```bash
git clone https://github.com/espressif/esp-tflite-micro.git
cd esp-tflite-micro
idf.py add-dependency "esp-tflite-micro"
```

Dentro de la carpeta de trabajo de ESP-IDF, crear una carpeta para guardar los
proyectos:

```bash
cd esp
mkdir projects_tf
cd projects_tf
```

Crear y ejecutar el ejemplo base de TensorFlow Lite Micro:

```bash
idf.py create-project-from-example "esp-tflite-micro:hello_world"
cd hello_world
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

La carpeta `1.1` contiene la implementacion directa del mismo modelo
`hello_world`, sin usar el interprete de TensorFlow Lite Micro. La carpeta
`hello world` contiene el ejemplo equivalente usando TensorFlow Lite Micro,
instrumentado para medir `interpreter->Invoke()` y comparar contra `1.1`.

La carpeta `2` contiene el ejemplo `person_detection` de
`esp-tflite-micro`, preparado para la seccion de imagenes estaticas con
`CLI_ONLY_INFERENCE` activado y con instrucciones para reemplazar las imagenes
por fotos del grupo.
