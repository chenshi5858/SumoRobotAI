# Captura de ejemplos sin identificador

La herramienta guarda imágenes `96x96` en escala de grises directamente en:

```text
C:\Users\chens\OneDrive\Escritorio\lab2embebidos\lab2embebidos\
└── dataset_grupo_99\
    └── esp\
        ├── esp_0001.png
        ├── esp_0002.png
        └── etiquetas.txt
```

Cada captura exitosa agrega automáticamente una fila con clase ausente:

```text
esp_0001.png, 0, 0
```

## Uso desde WSL

Instalar dependencias una vez:

```bash
python3 -m venv proyecto_final/Tools/.venv
source proyecto_final/Tools/.venv/bin/activate
python -m pip install -r proyecto_final/Tools/requirements.txt
```

Compilar y grabar el firmware de captura:

```bash
source "$HOME/esp/esp-idf/export.sh"
cd proyecto_final/Tools/esp_cam_capture
idf.py -B build_offline -p /dev/ttyUSB0 flash
```

Cerrar el monitor serial si está abierto y volver a la raíz del repositorio.
Para tomar 200 fotos automáticamente:

```bash
python3 proyecto_final/Tools/capture_images.py --port /dev/ttyUSB0 --count 200
```

Para decidir manualmente el instante de cada foto:

```bash
python3 proyecto_final/Tools/capture_images.py --port /dev/ttyUSB0 --count 200 --manual
```

La numeración continúa entre ejecuciones y no sobrescribe capturas anteriores.

Cuando termines la sesión, prepara la carpeta que leen los entrenadores:

```bash
cd /mnt/c/Users/chens/OneDrive/Escritorio/lab2embebidos/lab2embebidos
python preprocesar_datasets.py
```

Esto crea `dataset_grupo_99_reescalado/esp/` con las imágenes y etiquetas.
