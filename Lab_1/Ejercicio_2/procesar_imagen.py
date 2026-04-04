from PIL import Image
import numpy as np
import os

# ========= CONFIG =========
INPUT_IMAGE = "input_preview.png"
OUTPUT_HEADER = os.path.join("main", "image_data.h")
WIDTH = 96
HEIGHT = 96

# ========= CARGAR IMAGEN =========
img = Image.open(INPUT_IMAGE).convert("L")
img = img.resize((WIDTH, HEIGHT))

arr = np.array(img, dtype=np.uint8)

print(f"Imagen cargada: {arr.shape}")

# Guardar una copia limpia por si quieres usarla en el informe
Image.fromarray(arr).save("input_96x96.png")

# ========= GENERAR HEADER =========
flat = arr.flatten()

with open(OUTPUT_HEADER, "w") as f:
    f.write("#ifndef IMAGE_DATA_H\n")
    f.write("#define IMAGE_DATA_H\n\n")
    f.write("#include <stdint.h>\n\n")
    f.write(f"#define IMG_W {WIDTH}\n")
    f.write(f"#define IMG_H {HEIGHT}\n\n")
    f.write("static const uint8_t input_image[IMG_W * IMG_H] = {\n")

    for i, val in enumerate(flat):
        if i % 16 == 0:
            f.write("    ")
        f.write(f"{int(val)}")
        if i < len(flat) - 1:
            f.write(", ")
        if i % 16 == 15:
            f.write("\n")

    if len(flat) % 16 != 0:
        f.write("\n")

    f.write("};\n\n")
    f.write("#endif\n")

print(f"Header generado en: {OUTPUT_HEADER}")
print("Listo.")