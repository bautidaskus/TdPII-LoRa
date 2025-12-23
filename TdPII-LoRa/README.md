# TdPII-LoRa

Biblioteca Arduino para comunicación LoRa simplificada en placas Heltec ESP32.

Proporciona funciones `init()`, `send()` (con espera de ACK) y `recv()` para pruebas punto a punto utilizando el stack LoRaWan de Heltec.

## Requisitos

### Hardware
- Placa Heltec ESP32 con LoRa (WiFi LoRa 32, Wireless Stick, etc.)

### Software
- Arduino IDE 1.8.19 o superior

### Dependencias
Esta biblioteca requiere las siguientes librerías instaladas:

| Biblioteca | Versión | Descripción |
|------------|---------|-------------|
| Heltec ESP32 Dev-Boards | v2.1.5 | Controladores de bajo nivel del hardware Heltec |
| Adafruit GFX Library | v1.12.3 | Biblioteca gráfica para el display OLED |
| Adafruit BusIO | v1.17.4 | Abstracción de buses I2C/SPI (dependencia de Adafruit GFX) |

## Guía de Instalación para Arduino IDE 1.8.19

### Paso 1: Descargar las bibliotecas

Descargá los archivos ZIP de las siguientes bibliotecas:

1. **TdPII-LoRa** (esta biblioteca):
   - Ir a https://github.com/bautidaskus/TdPII-LoRa
   - Click en `Code` → `Download ZIP`

2. **Heltec ESP32 Dev-Boards v2.1.5**:
   - Descargar desde el repositorio oficial de Heltec

3. **Adafruit GFX Library v1.12.3**:
   - https://github.com/adafruit/Adafruit-GFX-Library
   - Click en `Code` → `Download ZIP`

4. **Adafruit BusIO v1.17.4**:
   - https://github.com/adafruit/Adafruit_BusIO
   - Click en `Code` → `Download ZIP`

### Paso 2: Instalar las bibliotecas en Arduino IDE

1. Abrir Arduino IDE 1.8.19
2. Ir a **Programa** → **Incluir Librería** → **Añadir biblioteca .ZIP...**
3. Navegar hasta la carpeta donde descargaste los archivos ZIP
4. Seleccionar cada archivo ZIP y hacer click en **Abrir**
5. Repetir para las 4 bibliotecas

> **Nota:** Instalar primero las dependencias (Adafruit BusIO, Adafruit GFX, Heltec ESP32) y por último TdPII-LoRa.

### Paso 3: Configurar soporte para placas Heltec ESP32

1. Ir a **Archivo** → **Preferencias**
2. En el campo **"Gestor de URLs Adicionales de Tarjetas"**, agregar:
   ```
   https://github.com/Heltec-Aaron-Lee/WiFi_Kit_series/releases/download/0.0.9/package_heltec_esp32_index.json
   ```
3. Click en **OK**
4. Ir a **Herramientas** → **Placa** → **Gestor de tarjetas...**
5. Buscar **"Heltec"** e instalar **"Heltec ESP32 Series Dev-boards"**
6. Cerrar el Gestor de tarjetas

### Paso 4: Seleccionar la placa correcta

1. Ir a **Herramientas** → **Placa**
2. En la sección **"Heltec ESP32 Arduino"**, seleccionar tu modelo:
   - `WiFi LoRa 32(V2)` para WiFi LoRa 32 V2
   - `WiFi LoRa 32(V3)` para WiFi LoRa 32 V3
   - `Wireless Stick` para Wireless Stick
   - (seleccionar según tu hardware)

### Paso 5: Abrir y compilar los ejemplos

1. Ir a **Archivo** → **Ejemplos** → **TdPII-LoRa**
2. Seleccionar uno de los ejemplos disponibles:
   - `basic_sender` - Ejemplo básico de envío
   - `sender_experimentos` - Ejemplo para experimentos
   - `web_gateway` - Gateway con interfaz web
3. Conectar la placa Heltec al puerto USB
4. Seleccionar el puerto en **Herramientas** → **Puerto**
5. Click en **Verificar** (✓) para compilar o **Subir** (→) para cargar en la placa

## Solución de problemas

### Error: "Librería inválida: Missing 'url' from library"

Arduino IDE 1.8.x requiere que el archivo `library.properties` incluya el campo `url`. Este campo es obligatorio en versiones anteriores del IDE, aunque Arduino IDE 2.x lo permite como opcional.

La biblioteca TdPII-LoRa ya incluye este campo configurado:
```properties
url=https://github.com/bautidaskus/TdPII-LoRa
```

Si descargaste una versión anterior sin este campo, actualizá el archivo `library.properties` agregando la línea anterior.

### Error: "Error compilando para la tarjeta Arduino Uno"

Esta biblioteca está diseñada exclusivamente para placas **ESP32 con LoRa**. No es compatible con Arduino Uno, Mega, Nano ni otras placas AVR.

Asegurate de seleccionar una placa Heltec ESP32 en **Herramientas** → **Placa** (ver Paso 4).

## Estructura de la biblioteca

```
TdPII-LoRa/
├── library.properties    # Metadatos de la biblioteca
├── README.md             # Este archivo
├── src/
│   ├── TdPIILoRa.h       # Header de la biblioteca
│   └── TdPIILoRa.cpp     # Implementación
└── examples/
    ├── basic_sender/     # Ejemplo básico
    ├── sender_experimentos/  # Experimentos
    └── web_gateway/      # Gateway web
```

## Licencia

Desarrollado por TdPII Team para el Taller de Proyecto II.
