#ifndef TDPII_LORA_TDPIILORA_H
#define TDPII_LORA_TDPIILORA_H

#include <cstddef>
#include <cstdint>

#include <Arduino.h>
#include <LoRaWan_APP.h>

namespace TdPII {

class TdPIILoRa {
public:
    TdPIILoRa();

    // Requiere que el sketch haya ejecutado SPI.begin(...) y Mcu.begin(...) previamente.
    // powerLevel: 1=10 dBm, 2=15 dBm, 3=20 dBm. sfLevel: 1=SF7, 2=SF10, 3=SF12.
    bool init(uint8_t powerLevel = 2, uint8_t sfLevel = 1);

    bool send(const uint8_t* data, size_t length);

    int recv(uint8_t* buffer, size_t bufferSize, uint32_t timeoutMs = 0);

private:
    static TdPIILoRa* activeInstance_;

    static void onTxDone();
    static void onTxTimeout();
    static void onRxDone(uint8_t* payload, uint16_t size, int16_t rssi, int8_t snr);
    static void onRxTimeout();
    static void onRxError();

    void handleTxDone();
    void handleTxTimeout();
    void handleRxDone(uint8_t* payload, uint16_t size, int16_t rssi, int8_t snr);
    void handleRxTimeout();
    void handleRxError();

    bool sendAck(uint8_t sequence);

    bool initialized_;
    RadioEvents_t radioEvents_;

    uint8_t powerLevelSetting_;
    uint8_t sfLevelSetting_;

    volatile bool txDone_;
    volatile bool txFailed_;
    volatile bool rxDone_;
    volatile bool rxFailed_;
    volatile bool rxTimedOut_;

    uint16_t rxSize_;
    uint8_t rxBuffer_[255];

    uint8_t nextSequence_;
    volatile bool awaitingAck_;
    volatile uint8_t awaitingAckSequence_;
    volatile bool ackReceived_;
    volatile bool ackTimedOut_;

    bool ackToSend_;
    uint8_t ackToSendSequence_;

    static constexpr uint32_t kAckTimeoutMs_ = 3000;
};

} // namespace TdPII

#endif

