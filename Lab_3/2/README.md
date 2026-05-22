# Parte 2 - Deteccion de Personas

Esta carpeta deja preparado el ejemplo `person_detection` de
`espressif/esp-tflite-micro` para la seccion de imagenes estaticas del
laboratorio.

## Estructura

- `_vendor/esp-tflite-micro`: clon sparse del repositorio oficial usado como
  referencia.
- `person_detection`: copia editable del ejemplo para compilar y flashear.
- `tools/convert_to_raw96.sh`: ayuda local para convertir una foto a formato
  raw 96x96 en escala de grises.

## Evaluacion del ejemplo

El ejemplo oficial trae 10 imagenes embebidas en
`person_detection/static_images/sample_images`. Cada archivo es una imagen raw
de `96x96` pixeles en escala de grises. El componente `static_images` las
embebe en el firmware y, con el modo CLI activado, se clasifican desde
`idf.py monitor` usando:

```text
detect_image <image_number>
```

Las descripciones esperadas estan en
`person_detection/static_images/sample_images/README.md`:

```text
image0: A person
image1: A dog
image2: A person
image3: A Monkey
image4: A person
image5: A cat
image6: A person (not in focus)
image7: Portrait of a person
image8: A person
image9: A person
```

Cambios aplicados sobre el ejemplo:

- `main/esp_main.h` define `CLI_ONLY_INFERENCE 1` para usar imagenes estaticas.
- `main/idf_component.yml` ya no usa `override_path: "../../../"`, porque esta
  copia no vive dentro de la raiz del componente clonado. Asi ESP-IDF descarga
  `espressif/esp-tflite-micro` con el Component Manager.

## Compilar y ejecutar

Desde una terminal con ESP-IDF activo:

```bash
cd Lab_3/2/person_detection
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Si la placa del grupo es ESP32-S3, usar:

```bash
idf.py set-target esp32s3
```

En el monitor serial, probar las imagenes originales:

```text
detect_image 0
detect_image 1
detect_image 2
```

Para salir del monitor: `Ctrl+]`.

## Ver las imagenes originales

Convertir una imagen raw a BMP para revisarla visualmente:

```bash
cd Lab_3/2/person_detection/static_images/sample_images
ffmpeg -f rawvideo -pixel_format gray -video_size 96x96 -i image0 image0.bmp
```

## Usar fotos del grupo

Convertir una foto del grupo al mismo formato del ejemplo:

```bash
cd Lab_3/2
./tools/convert_to_raw96.sh ruta/a/foto.jpg person_detection/static_images/sample_images/image0
```

Luego recompilar y flashear el proyecto:

```bash
cd person_detection
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

En el monitor:

```text
detect_image 0
```

Para la entrega, tomar un screenshot de la foto usada y del resultado mostrado
por el ESP32, y armar `output.pdf` con una imagen por integrante.
