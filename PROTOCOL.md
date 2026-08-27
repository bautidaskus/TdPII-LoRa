# Protocolo de Comunicación TdPII-LoRa

## 📡 Especificación Técnica del Protocolo Punto a Punto

Documento técnico que describe el protocolo de comunicación implementado en la biblioteca TdPII-LoRa.

**Versión**: 1.0
**Fecha**: 2025
**Autores**: Proyecto TdPII-LoRa - UTN FRBA

---

## 🎯 Objetivo del Protocolo

Proveer comunicación **confiable** punto a punto sobre LoRa, con:
- Confirmación de recepción (ACK)
- Detección de pérdida de paquetes
- Detección de duplicados mediante secuenciación
- Simplicidad de implementación

---

## 📊 Arquitectura del Protocolo

### Modelo de Capas

```
┌─────────────────────────────────────┐
│    APLICACIÓN (Usuario)             │  ← send(data, len) / recv(buffer)
├─────────────────────────────────────┤
│    PROTOCOLO TdPII-LoRa             │  ← Frame Type, Sequence, ACK Logic
│    (esta biblioteca)                │
├─────────────────────────────────────┤
│    LoRa PHY (SX1278)                │  ← Modulación, CRC, FEC
│    via LoRaWan_APP                  │
└─────────────────────────────────────┘
```

### Roles de los Nodos

El protocolo es **simétrico**: cualquier nodo puede actuar como emisor o receptor.

- **Emisor (sender)**: Llama `send()`, espera ACK
- **Receptor (receiver)**: Llama `recv()`, envía ACK automático

Un mismo nodo puede alternar entre ambos roles (ej: ping-pong).

---

## 📦 Formato de Frame

### Estructura General

Todos los mensajes sobre LoRa siguen este formato:

```
 0       1       2                     N+2
┌───────┬───────┬──────────────────────────┐
│ Type  │  Seq  │       Payload            │
│1 byte │1 byte │      0-253 bytes         │
└───────┴───────┴──────────────────────────┘
```

**Campos:**

| Campo | Tamaño | Descripción |
|-------|--------|-------------|
| **Type** | 1 byte | Tipo de frame (0x01 = DATA, 0x02 = ACK) |
| **Seq** | 1 byte | Número de secuencia (0-255, circular) |
| **Payload** | 0-253 bytes | Datos del usuario (solo en DATA) |

**Tamaño Total**: 2 + N bytes (donde N = longitud del payload)

---

### Frame Type: DATA (0x01)

Contiene datos del usuario.

```
┌──────┬─────┬────────────────┐
│ 0x01 │ Seq │   User Data    │
└──────┴─────┴────────────────┘
```

**Ejemplo**:
```
Enviar "Hola":
[0x01][0x00]['H']['o']['l']['a']
      │     │   └─────────────── Payload (4 bytes)
      │     └───────────────────── Secuencia 0
      └─────────────────────────── Tipo DATA
```

---

### Frame Type: ACK (0x02)

Confirmación de recepción. No lleva payload.

```
┌──────┬─────┐
│ 0x02 │ Seq │
└──────┴─────┘
```

**Ejemplo**:
```
ACK para secuencia 0:
[0x02][0x00]
      │     └── Secuencia confirmada
      └──────── Tipo ACK
```

**Tamaño fijo**: 2 bytes

---

## 🔄 Flujo de Comunicación

### Envío Exitoso (Happy Path)

```
EMISOR                                   RECEPTOR
  │                                         │
  │  1. send(data)                          │
  ├─────────────────────────────────────────┤
  │                                         │
  │  2. [DATA][Seq=5]["Hola"]              │
  ├────────────────────────────────────────>│
  │                                         │ 3. Procesa frame
  │                                         │    Guarda "Hola"
  │                                         │
  │  4. [ACK][Seq=5]                        │
  │<────────────────────────────────────────┤
  │                                         │
  │  5. Valida ACK                          │ 6. recv() retorna "Hola"
  │     send() retorna true                 │
  │                                         │
  ▼                                         ▼
```

**Tiempos típicos** (SF7, 50 bytes payload):
1. TX frame DATA: ~51 ms (time on air)
2. Procesamiento RX: ~5 ms
3. TX frame ACK: ~10 ms (time on air)
4. **Total**: ~70-100 ms

---

### Timeout de ACK

```
EMISOR                                   RECEPTOR
  │                                         │
  │  [DATA][Seq=3]["Test"]                 │
  ├────────────────────────────────────────>│
  │                                         │
  │                                         ✗ (Receptor apagado)
  │                                         │
  │  Espera ACK...                          │
  │  (timeout 3000 ms)                      │
  │  ⏰                                      │
  │                                         │
  │  send() retorna false                   │
  │  (sin ACK)                              │
  │                                         │
  ▼                                         ▼
```

**Timeout de ACK**: 3000 ms (fijo, no configurable actualmente)

---

### Frame Corrupto (CRC Error)

```
EMISOR                                   RECEPTOR
  │                                         │
  │  [DATA][Seq=2]["corrupto"]             │
  ├────────────────────────────────────────>│
  │                                         │
  │                                         ✗ CRC inválido
  │                                         │ (LoRa detecta error)
  │                                         │
  │  Espera ACK...                          │ No envía ACK
  │  ⏰ (timeout 3s)                        │
  │                                         │
  │  send() retorna false                   │
  │                                         │
  ▼                                         ▼
```

**Nota**: El CRC de LoRa está en la capa PHY (manejado por SX1278).
No requiere implementación adicional en el protocolo.

---

## 🔢 Manejo de Secuencias

### Generación de Número de Secuencia

- **Rango**: 0-255 (1 byte)
- **Incremento**: Automático en cada `send()`
- **Wrap-around**: Al llegar a 255, vuelve a 0

```cpp
uint8_t sequence = 0;

// Primera llamada: sequence = 0, envía con Seq=0, incrementa a 1
send(data1);

// Segunda llamada: sequence = 1, envía con Seq=1, incrementa a 2
send(data2);

// ...

// Llamada 256: sequence = 255, envía con Seq=255, incrementa a 0 (wrap)
send(data256);
```

### Validación de ACK

El emisor solo acepta ACK si:
1. **Frame Type** = 0x02 (ACK)
2. **Sequence** = secuencia del paquete enviado

Cualquier otro ACK es ignorado (puede ser para otro paquete).

```cpp
// Emisor envió DATA con Seq=42
awaitingAckSequence_ = 42;

// Llega [ACK][42] → ✓ Aceptado
// Llega [ACK][41] → ✗ Ignorado (secuencia antigua)
// Llega [ACK][43] → ✗ Ignorado (secuencia futura)
```

### Detección de Duplicados

Actualmente **no implementada**. El receptor no verifica si ya recibió un frame con la misma secuencia.

**Razón**: En comunicación punto a punto simple, el emisor no reintenta automáticamente.
Si `send()` falla, es responsabilidad de la aplicación decidir si reenvía.

**Posible mejora futura**:
```cpp
// Receptor guarda última secuencia recibida
if (sequence == lastReceivedSeq_) {
  // Duplicado, enviar ACK pero no procesar payload
  sendAck(sequence);
  return;
}
lastReceivedSeq_ = sequence;
```

---

## ⚙️ Parámetros Configurables

### Constantes de Protocolo

Definidas en `TdPIILoRa.cpp`:

| Parámetro | Valor | Descripción |
|-----------|-------|-------------|
| `kFrameTypeData` | 0x01 | Identificador de frame DATA |
| `kFrameTypeAck` | 0x02 | Identificador de frame ACK |
| `kMaxPayloadSize` | 253 bytes | Payload máximo del usuario |
| `kAckTimeoutMs` | 3000 ms | Timeout para recibir ACK |
| `kSendGuardTimeoutMs` | 6000 ms | Timeout de seguridad para TX |

### Parámetros LoRa (Capa PHY)

Definidos en `TdPIILoRa.cpp`:

| Parámetro | Valor | Justificación |
|-----------|-------|---------------|
| **Frecuencia** | 915 MHz | Banda ISM Americas |
| **Spreading Factor** | SF7 *(por defecto)* | Balance velocidad/alcance |
| **Bandwidth** | 125 kHz | Estándar LoRa |
| **Coding Rate** | 4/5 | Protección básica de errores |
| **TX Power** | 15 dBm *(por defecto)* | Máximo permitido sin licencia |
| **Preamble** | 8 símbolos | Estándar LoRa |
| **CRC** | Habilitado | Detección de corrupción |

El spreading factor y la potencia de transmisión **son configurables** desde
`init(powerLevel, sfLevel)`; los valores de la tabla son los que quedan con los
argumentos por defecto (`powerLevel = 2`, `sfLevel = 1`):

| Nivel | `powerLevel` → TX Power | `sfLevel` → Spreading Factor |
|-------|-------------------------|------------------------------|
| 1 | 10 dBm | SF7 *(default)* |
| 2 | 15 dBm *(default)* | SF10 |
| 3 | 20 dBm | SF12 |

Un SF más alto aumenta el alcance y la sensibilidad a costa de un tiempo en el
aire mucho mayor, por lo que la tabla de ToA de abajo sólo aplica a SF7.

**Tiempo en el aire** (Time on Air) para diferentes payloads:

| Payload | ToA (SF7, BW125) |
|---------|------------------|
| 10 bytes | ~31 ms |
| 50 bytes | ~51 ms |
| 100 bytes | ~82 ms |
| 200 bytes | ~144 ms |
| 253 bytes (máx) | ~174 ms |

_Calculado con [LoRa Airtime Calculator](https://www.loratools.nl/#/airtime)_

---

## 🔒 Seguridad

### Autenticación

**No implementada**. Cualquier nodo con la misma configuración LoRa puede comunicarse.

**Implicaciones**:
- Cualquiera puede escuchar mensajes (sniffing)
- Cualquiera puede enviar frames falsos (spoofing)

**Mitigaciones posibles** (futuro):
- Encriptación (AES-128 en payload)
- Firma digital (HMAC)
- Autenticación por secuencia compartida (challenge-response)

### Integridad

**Parcialmente implementada**:
- ✅ **CRC de LoRa**: Detecta corrupción en transmisión (capa PHY)
- ❌ **HMAC**: No hay verificación de autenticidad del mensaje

Si un atacante puede enviar frames válidos, el receptor los aceptará.

---

## 📈 Performance y Limitaciones

### Throughput Máximo

```
Payload máximo: 253 bytes
ToA (SF7, BW125): ~174 ms
Overhead ACK: ~10 ms
Procesamiento: ~20 ms

Tiempo total: ~204 ms/paquete
Throughput: 253 bytes / 0.204 s ≈ 1.24 KB/s ≈ 9.9 kbps
```

**En práctica**: ~8-10 kbps debido a:
- Tiempo de procesamiento variable
- Colisiones (si hay múltiples emisores)
- Reintentos (por fallos de comunicación)

### Comparación con LoRaWAN

| Característica | TdPII-LoRa | LoRaWAN |
|----------------|------------|---------|
| Complejidad | Muy simple | Compleja |
| Overhead de protocolo | 2 bytes | 13+ bytes |
| ACKs | Siempre | Opcional (confirmed uplinks) |
| Slots de tiempo | No | Sí (Class A/B/C) |
| Múltiples gateways | No | Sí |
| Escalabilidad | Baja (1:1) | Alta (miles de nodos) |
| Latencia | ~100-200 ms | Variable (segundos) |

**Cuándo usar TdPII-LoRa**:
- Comunicación punto a punto
- Baja latencia requerida
- Simplicidad sobre escalabilidad
- Entorno educativo/prototipado

**Cuándo usar LoRaWAN**:
- Red de sensores (muchos nodos)
- Infraestructura existente (TTN, Helium)
- Requisitos de seguridad estrictos
- Gestión centralizada

---

## 🐛 Casos de Borde y Manejo de Errores

### 1. Payload Vacío

```cpp
radio.send(nullptr, 0);  // Retorna false (inválido)
```

**Comportamiento**: `send()` retorna `false` sin transmitir.

---

### 2. Payload Demasiado Grande

```cpp
uint8_t huge[300];
radio.send(huge, 300);  // Retorna false (excede 253 bytes)
```

**Comportamiento**: `send()` retorna `false` sin transmitir.

---

### 3. ACK de Secuencia Incorrecta

```
Emisor envía: [DATA][Seq=10]
Receptor responde: [ACK][Seq=9]  (error del receptor)

Resultado: Emisor ignora ACK, timeout, send() retorna false
```

---

### 4. Múltiples Receptores (Broadcast Accidental)

Si hay 2+ receptores escuchando:

```
EMISOR                 RX1              RX2
  │                     │                │
  │  [DATA][5]["Hi"]   │                │
  ├────────────────────>├────────────────>│
  │                     │                │
  │  [ACK][5]           │  [ACK][5]      │
  │<────────────────────┤<───────────────┤
  │                     │                │
  ✓ Acepta primer ACK   │                │
    (el segundo se ignora)               │
```

**Problema**: ¡Colisión de ACKs! Ambos receptores transmiten a la vez.

**Resultado**: Posible corrupción, emisor puede no recibir ningún ACK válido.

**Solución futura**: TDMA, CSMA, o ID único por receptor.

---

### 5. Buffer de Recepción Overflow

```cpp
uint8_t pequeño[10];
// Llega frame con 50 bytes de payload
int len = radio.recv(pequeño, sizeof(pequeño));

// len = 10 (truncado)
// Solo se copian los primeros 10 bytes
```

**Comportamiento**: Se trunca silenciosamente. El receptor envía ACK de todos modos.

**Recomendación**: Usar buffer >= 255 bytes para no perder datos.

---

## 🔮 Mejoras Futuras

### Versión 1.1 (Corto Plazo)

- [ ] Getter para RSSI/SNR del último paquete
- [ ] Timeout de ACK configurable (`setAckTimeout(ms)`)
- [ ] Contador de secuencia expuesto (`getLastSeq()`)

### Versión 2.0 (Mediano Plazo)

- [ ] Reintentos automáticos en `send()`
- [ ] Detección de duplicados en receptor
- [ ] API no bloqueante (callbacks opcionales)
- [ ] Métricas (struct con stats)

### Versión 3.0 (Largo Plazo)

- [ ] Soporte multi-nodo (broadcast con addressing)
- [ ] Encriptación AES-128 opcional
- [ ] Fragmentación para payloads > 253 bytes
- [ ] CSMA/CA (escucha antes de transmitir)

---

## 📚 Referencias

### Estándares y Especificaciones

- [LoRa Modulation Basics](https://lora-developers.semtech.com/documentation/tech-papers-and-guides/lora-and-lorawan/)
- [Semtech SX1278 Datasheet](https://www.semtech.com/products/wireless-rf/lora-transceivers/sx1278)
- [LoRaWAN Specification (para comparación)](https://lora-alliance.org/resource_hub/lorawan-specification-v1-0-3/)

### Herramientas

- [LoRa Airtime Calculator](https://www.loratools.nl/#/airtime)
- [LoRa Range Calculator](https://www.rfwireless-world.com/calculators/LoRa-Range-calculator.html)

---

## 📧 Contacto

Preguntas técnicas sobre el protocolo:
**Email**: [proyecto email]
**GitHub**: [link al repo]

---

**Última actualización**: 2025
**Versión del documento**: 1.0
