from PIL import Image
import numpy as np
import matplotlib.pyplot as plt

# ===== CONFIG =====
IMAGE_PATH = "FAAHHHHHHH.png"

# ===== CARGAR Y PREPROCESAR =====
img = Image.open(IMAGE_PATH).convert("L")
img = img.resize((96, 96))

arr = np.array(img, dtype=np.uint8)

print("Imagen cargada:", arr.shape)

# Mostrar imagen base
plt.imshow(arr, cmap="binary")
plt.axis("off")
plt.savefig("input_preview.png", bbox_inches='tight', pad_inches=0)
plt.close()