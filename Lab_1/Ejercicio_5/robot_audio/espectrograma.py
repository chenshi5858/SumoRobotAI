import numpy as np
import matplotlib.pyplot as plt

# ================= CONFIG =================
CSV_FILE = "data.csv"
SAMPLE_RATE = 16000
FFT_SIZE = 1024

# ================= CARGAR DATOS =================

data = []

with open(CSV_FILE, "r", errors="ignore") as f:
    for line in f:
        try:
            row = [float(x) for x in line.strip().split(",") if x]
            if len(row) > 0:
                data.append(row)
        except:
            pass  # ignora líneas basura (boot, etc.)

data = np.array(data)

print("Shape:", data.shape)  # (tiempo, frecuencias)

# ================= PREPARAR EJES =================

time_axis = np.arange(data.shape[0]) * (FFT_SIZE / SAMPLE_RATE)
freq_axis = np.linspace(0, SAMPLE_RATE/2, data.shape[1])

# ================= GRAFICAR =================

plt.figure(figsize=(10, 6))

# Convertir a dB (más visual)
data_db = 10 * np.log10(data + 1e-6)

plt.imshow(
    data_db.T,
    aspect='auto',
    origin='lower',
    extent=[time_axis[0], time_axis[-1], freq_axis[0], freq_axis[-1]]
)

plt.colorbar(label="Power (dB)")
plt.xlabel("Tiempo (s)")
plt.ylabel("Frecuencia (Hz)")
plt.title("Espectrograma (ESP32)")

plt.tight_layout()
plt.show()