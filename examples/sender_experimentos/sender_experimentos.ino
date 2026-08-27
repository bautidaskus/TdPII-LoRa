#include <HT_SSD1306Wire.h>
#include <LoRaWan_APP.h>
#include <SPI.h>
#include <TdPIILoRa.h>
#include <cstdio>
#include <cstring>

using namespace TdPII;

extern SSD1306Wire display;

namespace {
constexpr uint8_t kPowerLevel = 1;              // Ajustar segun lote (1=10dBm,2=15dBm,3=20dBm)
constexpr uint8_t kSpreadingFactorLevel = 1;    // Ajustar segun lote (1=SF7, 2=SF10, 3=SF12)
constexpr uint16_t kDistanceMeters = 100;       // Cambiar segun posicion antes de subir
constexpr uint16_t kPacketsPerBatch = 20;
constexpr uint16_t kInterPacketDelayMs = 5000;  //tiempo entre paquetes

TdPIILoRa radio;
uint32_t globalSequence = 0;
uint16_t okCount = 0;
uint16_t failCount = 0;
bool batchFinished = false;
uint32_t lastReportMs = 0;
char statusLine[17] = "Listo";
char dataLine[17] = "";

void initDisplay(SSD1306Wire& disp) {
  disp.wakeup();
  disp.init();
  delay(100);
  disp.flipScreenVertically();
  disp.setFont(ArialMT_Plain_10);
  disp.setTextAlignment(TEXT_ALIGN_LEFT);
  disp.clear();
  disp.display();
  disp.clear();
}

void setStatus(const char* text) {
  std::strncpy(statusLine, text, sizeof(statusLine) - 1);
  statusLine[sizeof(statusLine) - 1] = '\0';
}

void updateDataLine() {
  std::snprintf(dataLine, sizeof(dataLine), "D:%um P:%u",
                static_cast<unsigned>(kDistanceMeters),
                static_cast<unsigned>(kPowerLevel));
}

void renderDisplay() {
  char line[17];
  display.clear();
  display.drawString(0, 0, "EXP1 Potencia");
  display.drawString(0, 16, dataLine);
  std::snprintf(line, sizeof(line), "OK:%u FL:%u",
                static_cast<unsigned>(okCount),
                static_cast<unsigned>(failCount));
  display.drawString(0, 32, line);
  display.drawString(0, 48, statusLine);
  display.display();
}

void sendBatch() {
  okCount = 0;
  failCount = 0;

  for (uint16_t i = 0; i < kPacketsPerBatch; ++i) {
    char payload[48];
    const unsigned long seq = globalSequence++;
    std::snprintf(payload, sizeof(payload), "EXP1,D=%u,P=%u,S=%lu",
                  static_cast<unsigned>(kDistanceMeters),
                  static_cast<unsigned>(kPowerLevel), seq);

    setStatus("Enviando");
    renderDisplay();

    const bool sent = radio.send(reinterpret_cast<const uint8_t*>(payload),
                                 std::strlen(payload));
    if (sent) {
      ++okCount;
      setStatus("ACK OK");
    } else {
      ++failCount;
      setStatus("ACK falla");
    }
    renderDisplay();
    delay(kInterPacketDelayMs);
  }

  batchFinished = true;
  setStatus("Listo");
  renderDisplay();
}

void printSummary() {
  const float successRate = (okCount * 100.0f) / kPacketsPerBatch;
  Serial.printf("[EXP1] Dist=%um PowerLvl=%u SF=%u OK=%u FAIL=%u (%.1f%%)\r\n",
                kDistanceMeters, kPowerLevel, kSpreadingFactorLevel, okCount,
                failCount, successRate);
}
} // namespace

void setup() {
  Serial.begin(115200);
  delay(20);

  Serial.println();
  Serial.println(F("TdPII - Experimento 1 (Potencia vs Distancia)"));
  Serial.println(F("El sketch envia 20 paquetes y queda mostrando el resultado."));
  Serial.println(F("Ajusta kDistanceMeters y kPowerLevel antes de subir."));

  SPI.begin(SCK, MISO, MOSI, SS);
  Mcu.begin(WIFI_LoRa_32_V2, 0);
  initDisplay(display);
  updateDataLine();
  renderDisplay();

  if (!radio.init(kPowerLevel, kSpreadingFactorLevel)) {
    Serial.println(F("[EXP1] Error al inicializar la radio. Reinicia el equipo."));
    setStatus("Init fallo");
    renderDisplay();
    while (true) {
      delay(1000);
    }
  }

  setStatus("Enviando");
  renderDisplay();
  sendBatch();
  printSummary();
}

void loop() {
  if (!batchFinished) {
    delay(100);
    return;
  }

  const uint32_t now = millis();
  if (now - lastReportMs >= 2000U) {
    printSummary();
    lastReportMs = now;
  }
  delay(50);
}
