# Web Page - Control de Auto

App React para controlar el auto desde Lab_1.

Requisitos:
- Node >=16

Instalación y uso:

```bash
cd Lab_2/web_page
npm install
# crear .env con VITE_API_URL apuntando al servidor que controla el vehículo, por ejemplo:
# VITE_API_URL=http://192.168.1.100:8000
npm run dev
```

La aplicación envía peticiones POST a `${VITE_API_URL}/move` con JSON:

```json
{ "direction": "forward" } // o "back", "left", "right"
```

Puedes adaptar el endpoint según tu firmware (ESP32).
