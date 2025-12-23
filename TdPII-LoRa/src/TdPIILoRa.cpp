#include "TdPIILoRa.h"

#include <SPI.h>
#include <cstring>

namespace TdPII {
namespace {
constexpr long kFrequency = 915000000; // 915 MHz - banda ISM para Argentina
constexpr uint8_t kBandwidth = 0; // 0 = 125 kHz - ancho de banda estándar
constexpr uint8_t kCodingRate = 1; // 4/5 - tasa de corrección de errores

uint8_t levelToPowerDbm(uint8_t level) {
    switch (level) {
        case 1:
            return 10; // nivel 1 = 10 dBm
        case 3:
            return 20; // nivel 3 = 20 dBm
        default:
            return 15; // nivel 2 o inválido = 15 dBm
    }
}

uint8_t levelToSpreadingFactor(uint8_t level) {
    switch (level) {
        case 2:
            return 10; // nivel 2 = SF10
        case 3:
            return 12; // nivel 3 = SF12
        default:
            return 7; // nivel 1 o inválido = SF7
    }
}
uint16_t kPreambleLength = 8; // 8 símbolos - preámbulo estándar
constexpr uint16_t kSymbolTimeout = 0; // sin timeout de símbolos en recepción
constexpr bool kFixLengthPayload = false; // payload de longitud variable
constexpr bool kIqInversion = false; // sin inversión IQ (configuración normal)
constexpr uint32_t kSendGuardTimeoutMs = 6000; // timeout de seguridad para envío
constexpr size_t kMaxPayloadSize = 253; // máximo payload del usuario (255 - 2 bytes de header)
constexpr uint8_t kFrameTypeData = 0x01; // tipo de frame: datos del usuario
constexpr uint8_t kFrameTypeAck = 0x02; // tipo de frame: confirmación de recepción
} // namespace

TdPIILoRa* TdPIILoRa::activeInstance_ = nullptr; // puntero global para callbacks estáticos

TdPIILoRa::TdPIILoRa()
    : initialized_(false), // todavía no se llamó init()
      powerLevelSetting_(0), // nivel de potencia aún no configurado
      sfLevelSetting_(0), // nivel de SF aún no configurado
      txDone_(false), // flag: transmisión completada
      txFailed_(false), // flag: transmisión falló
      rxDone_(false), // flag: recepción completada
      rxFailed_(false), // flag: recepción falló
      rxTimedOut_(false), // flag: timeout en recepción
      rxSize_(0), // bytes recibidos en el último paquete
      nextSequence_(0), // próximo número de secuencia a enviar
      awaitingAck_(false), // esperando confirmación de un envío
      awaitingAckSequence_(0), // número de secuencia del ACK esperado
      ackReceived_(false), // flag: ACK recibido correctamente
      ackTimedOut_(false), // flag: timeout esperando ACK
      ackToSend_(false), // flag: hay que enviar un ACK al usuario
      ackToSendSequence_(0) { // secuencia del ACK a enviar
    std::memset(&radioEvents_, 0, sizeof(radioEvents_)); // limpia estructura de callbacks
    std::memset(rxBuffer_, 0, sizeof(rxBuffer_)); // limpia buffer de recepción
}

bool TdPIILoRa::init(uint8_t powerLevel, uint8_t sfLevel) {
    const uint8_t txPowerDbm = levelToPowerDbm(powerLevel);
    const uint8_t spreadingFactor = levelToSpreadingFactor(sfLevel);

    if (initialized_) { // reconfigura si los parámetros cambiaron
        if (powerLevel == powerLevelSetting_ && sfLevel == sfLevelSetting_) {
            return true;
        }

        Radio.SetTxConfig(MODEM_LORA, txPowerDbm, 0, kBandwidth,
                          spreadingFactor, kCodingRate,
                          kPreambleLength, kFixLengthPayload,
                          true, 0, 0, kIqInversion, 3000);
        Radio.SetRxConfig(MODEM_LORA, kBandwidth, spreadingFactor,
                          kCodingRate, 0, kPreambleLength,
                          kSymbolTimeout, kFixLengthPayload,
                          0, true, 0, 0, kIqInversion, true);
        Radio.Rx(0);
        powerLevelSetting_ = powerLevel;
        sfLevelSetting_ = sfLevel;
        return true;
    }

    activeInstance_ = this; // guarda puntero para callbacks

    std::memset(&radioEvents_, 0, sizeof(radioEvents_));
    radioEvents_.TxDone = onTxDone;
    radioEvents_.TxTimeout = onTxTimeout;
    radioEvents_.RxDone = onRxDone;
    radioEvents_.RxTimeout = onRxTimeout;
    radioEvents_.RxError = onRxError;

    Radio.Init(&radioEvents_);
    Radio.SetChannel(kFrequency);
    Radio.SetTxConfig(MODEM_LORA, txPowerDbm, 0, kBandwidth,
                      spreadingFactor, kCodingRate,
                      kPreambleLength, kFixLengthPayload,
                      true, 0, 0, kIqInversion, 3000);
    Radio.SetRxConfig(MODEM_LORA, kBandwidth, spreadingFactor,
                      kCodingRate, 0, kPreambleLength,
                      kSymbolTimeout, kFixLengthPayload,
                      0, true, 0, 0, kIqInversion, true);

    Radio.Rx(0);

    powerLevelSetting_ = powerLevel;
    sfLevelSetting_ = sfLevel;

    initialized_ = true;
    return true;
}

bool TdPIILoRa::send(const uint8_t* data, size_t length) {
    if (!initialized_ || data == nullptr || length == 0 || length > kMaxPayloadSize) { // valida parámetros
        return false;
    }

    uint8_t frame[kMaxPayloadSize + 2]; // buffer para frame completo (header + payload)
    const uint8_t sequence = nextSequence_++; // obtiene próximo número de secuencia y lo incrementa
    frame[0] = kFrameTypeData; // byte 0: tipo de frame (DATA)
    frame[1] = sequence; // byte 1: número de secuencia
    std::memcpy(&frame[2], data, length); // copia datos del usuario a partir del byte 2

    txDone_ = false; // resetea flags de transmisión
    txFailed_ = false;
    ackReceived_ = false; // resetea flags de ACK
    ackTimedOut_ = false;
    awaitingAck_ = true; // marca que estamos esperando ACK
    awaitingAckSequence_ = sequence; // guarda secuencia del ACK esperado

    Radio.Send(frame, length + 2); // envía frame completo (header + payload)

    const uint32_t txStart = millis(); // guarda tiempo de inicio para timeout
    while (!txDone_ && !txFailed_) { // espera a que termine la transmisión (bloqueante)
        Radio.IrqProcess(); // procesa interrupciones de la radio
        delay(1); // pequeña pausa para no saturar CPU
        if ((millis() - txStart) > kSendGuardTimeoutMs) { // timeout de seguridad (6s)
            txFailed_ = true;
        }
    }

    if (!txDone_ || txFailed_) { // si transmisión falló
        awaitingAck_ = false; // cancela espera de ACK
        Radio.Rx(0); // vuelve a modo recepción continua
        return false;
    }

    Radio.Rx(kAckTimeoutMs_); // pone radio en modo RX con timeout de 3s para recibir ACK
    const uint32_t ackStart = millis(); // guarda tiempo de inicio para timeout de ACK
    while (awaitingAck_ && !ackReceived_) { // espera ACK (bloqueante)
        Radio.IrqProcess(); // procesa interrupciones (ACK llegará por callback)
        delay(1); // pequeña pausa
        if ((millis() - ackStart) > (kAckTimeoutMs_ + 50)) { // timeout de ACK (3s + margen)
            ackTimedOut_ = true;
            awaitingAck_ = false; // cancela espera
        }
    }

    const bool success = ackReceived_ && !ackTimedOut_; // éxito si ACK llegó sin timeout
    awaitingAck_ = false; // limpia flag de espera

    Radio.Rx(0); // vuelve a modo recepción continua
    return success;
}

int TdPIILoRa::recv(uint8_t* buffer, size_t bufferSize, uint32_t timeoutMs) {
    if (!initialized_ || buffer == nullptr || bufferSize == 0) { // valida parámetros
        return -1;
    }

    rxDone_ = false; // resetea flags de recepción
    rxFailed_ = false;
    rxTimedOut_ = false;
    rxSize_ = 0; // resetea tamaño recibido

    Radio.Rx(timeoutMs); // pone radio en modo RX (0 = infinito, >0 = timeout en ms)

    const uint32_t start = millis(); // guarda tiempo de inicio para timeout
    while (!rxDone_ && !rxFailed_ && !rxTimedOut_) { // espera a recibir datos (bloqueante)
        Radio.IrqProcess(); // procesa interrupciones (datos llegarán por callback)
        delay(1); // pequeña pausa
        if (timeoutMs > 0 && (millis() - start) > (timeoutMs + 50)) { // timeout con margen
            rxTimedOut_ = true;
        }
    }

    if (!rxDone_) { // si no se recibió nada (timeout o error)
        Radio.Rx(0); // vuelve a modo recepción continua
        return -1;
    }

    if (ackToSend_) { // si hay que enviar ACK (se marcó en handleRxDone)
        (void)sendAck(ackToSendSequence_); // envía ACK con la secuencia recibida
        ackToSend_ = false; // limpia flag
    }

    const size_t copyLength = rxSize_ < bufferSize ? rxSize_ : bufferSize; // copia lo que entra en el buffer
    std::memcpy(buffer, rxBuffer_, copyLength); // copia datos recibidos al buffer del usuario

    Radio.Rx(0); // vuelve a modo recepción continua

    return static_cast<int>(copyLength); // retorna cantidad de bytes copiados
}

void TdPIILoRa::onTxDone() { // callback estático llamado por la radio cuando termina TX
    if (activeInstance_ != nullptr) { // si hay instancia activa
        activeInstance_->handleTxDone(); // llama al handler de la instancia
    }
}

void TdPIILoRa::onTxTimeout() { // callback estático llamado cuando TX hace timeout
    if (activeInstance_ != nullptr) {
        activeInstance_->handleTxTimeout();
    }
}

void TdPIILoRa::onRxDone(uint8_t* payload, uint16_t size, int16_t rssi, int8_t snr) { // callback estático cuando RX termina
    if (activeInstance_ != nullptr) {
        activeInstance_->handleRxDone(payload, size, rssi, snr); // pasa datos recibidos al handler
    }
}

void TdPIILoRa::onRxTimeout() { // callback estático cuando RX hace timeout
    if (activeInstance_ != nullptr) {
        activeInstance_->handleRxTimeout();
    }
}

void TdPIILoRa::onRxError() { // callback estático cuando hay error en RX
    if (activeInstance_ != nullptr) {
        activeInstance_->handleRxError();
    }
}

void TdPIILoRa::handleTxDone() { // handler cuando transmisión termina exitosamente
    txDone_ = true; // marca TX como completado
    txFailed_ = false; // asegura que no está marcado como fallido
    Radio.Sleep(); // pone radio en sleep para ahorrar energía
}

void TdPIILoRa::handleTxTimeout() { // handler cuando transmisión hace timeout
    txFailed_ = true; // marca TX como fallido
    Radio.Sleep(); // pone radio en sleep
}

void TdPIILoRa::handleRxDone(uint8_t* payload, uint16_t size, int16_t /*rssi*/, int8_t /*snr*/) { // handler cuando recepción termina
    if (size < 2U) { // frame debe tener al menos 2 bytes (tipo + secuencia)
        rxFailed_ = true; // marca RX como fallido
        Radio.Sleep();
        return;
    }

    const uint8_t frameType = payload[0]; // byte 0: tipo de frame
    const uint8_t sequence = payload[1]; // byte 1: número de secuencia

    if (frameType == kFrameTypeAck) { // si es un ACK
        if (awaitingAck_ && sequence == awaitingAckSequence_) { // si estamos esperando este ACK
            ackReceived_ = true; // marca ACK como recibido
            awaitingAck_ = false; // ya no espera ACK
        }
        Radio.Sleep(); // pone radio en sleep
        if (!awaitingAck_) { // si ya no espera ACK
            Radio.Rx(0); // vuelve a modo RX continuo
        }
        return; // termina (ACKs no se pasan al usuario)
    }

    if (frameType != kFrameTypeData) { // si no es DATA ni ACK, es frame desconocido
        rxFailed_ = true; // marca RX como fallido
        Radio.Sleep();
        return;
    }

    const uint16_t payloadSize = static_cast<uint16_t>(size - 2U); // tamaño del payload (sin header)
    const size_t copyLength = payloadSize < sizeof(rxBuffer_) ? payloadSize : sizeof(rxBuffer_); // copia lo que entra
    std::memcpy(rxBuffer_, payload + 2, copyLength); // copia payload (desde byte 2) al buffer interno
    rxSize_ = static_cast<uint16_t>(copyLength); // guarda tamaño recibido
    rxDone_ = true; // marca RX como completado
    rxFailed_ = false; // asegura que no está marcado como fallido

    ackToSend_ = true; // marca que hay que enviar ACK
    ackToSendSequence_ = sequence; // guarda secuencia para el ACK

    Radio.Sleep(); // pone radio en sleep
}

void TdPIILoRa::handleRxTimeout() { // handler cuando recepción hace timeout
    if (awaitingAck_) { // si estábamos esperando un ACK
        ackTimedOut_ = true; // marca timeout de ACK
        awaitingAck_ = false; // cancela espera
    } else { // si estábamos esperando datos del usuario
        rxTimedOut_ = true; // marca timeout de RX
    }
    Radio.Sleep(); // pone radio en sleep
}

void TdPIILoRa::handleRxError() { // handler cuando hay error en recepción
    if (awaitingAck_) { // si estábamos esperando un ACK
        ackTimedOut_ = true; // trata error como timeout de ACK
        awaitingAck_ = false; // cancela espera
    } else { // si estábamos esperando datos del usuario
        rxFailed_ = true; // marca RX como fallido
    }
    Radio.Sleep(); // pone radio en sleep
}

bool TdPIILoRa::sendAck(uint8_t sequence) { // envía frame ACK (función interna)
    uint8_t frame[2] = {kFrameTypeAck, sequence}; // frame ACK: [tipo][secuencia]
    txDone_ = false; // resetea flags de transmisión
    txFailed_ = false;

    Radio.Send(frame, sizeof(frame)); // envía ACK (2 bytes)

    const uint32_t start = millis(); // guarda tiempo de inicio
    while (!txDone_ && !txFailed_) { // espera a que termine TX (bloqueante)
        Radio.IrqProcess(); // procesa interrupciones
        delay(1); // pequeña pausa
        if ((millis() - start) > kSendGuardTimeoutMs) { // timeout de seguridad (6s)
            txFailed_ = true;
        }
    }

    Radio.Rx(0); // vuelve a modo recepción continua
    return txDone_ && !txFailed_; // retorna true si envío fue exitoso
}

} 







