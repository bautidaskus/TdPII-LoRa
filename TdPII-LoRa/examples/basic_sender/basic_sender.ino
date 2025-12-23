#include <HT_SSD1306Wire.h>
#include <LoRaWan_APP.h>
#include <SPI.h>
#include <TdPIILoRa.h>
#include <cstdio>
#include <cstring>

using namespace TdPII;

extern SSD1306Wire display;
TdPIILoRa radio;

namespace {
constexpr uint8_t kPowerLevel = 2;          // 1=10dBm, 2=15dBm, 3=20dBm
constexpr uint8_t kSpreadingFactorLevel = 1; // 1=SF7, 2=SF10, 3=SF12
constexpr uint16_t kPacketsToSend = 50;
constexpr uint16_t kInterPacketDelayMs = 200;

struct Metrics {
  uint16_t ok = 0;
  uint16_t fail = 0;
} metrics;

char statusLine[17] = "Preparado";
bool batchFinished = false;
uint32_t lastReportMs = 0;

void initBoard() {
  Serial.begin(115200);
  delay(20);
  SPI.begin(SCK, MISO, MOSI, SS);
  Mcu.begin(WIFI_LoRa_32_V2, 0);
}

void initDisplay(SSD1306Wire& display) {
  display.wakeup();
  display.init();
  delay(100);
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.clear();
  display.display();
  display.clear();
}

void setStatus(const char* text) {
  std::strncpy(statusLine, text, sizeof(statusLine) - 1);
  statusLine[sizeof(statusLine) - 1] = '\0';
}

void renderDisplay(SSD1306Wire& display) {
  char line[17];

  display.clear();
  display.drawString(0, 0, "TdPII Sender");

  std::snprintf(line, sizeof(line), "OK:%u FAIL:%u",
                static_cast<unsigned>(metrics.ok),
                static_cast<unsigned>(metrics.fail));
  display.drawString(0, 16, line);

  std::snprintf(line, sizeof(line), "Estado:%s", statusLine);
  display.drawString(0, 32, line);

  display.display();
}

void runBatch() {
  for (uint16_t i = 0; i < kPacketsToSend; ++i) {
    char payload[32];
    std::snprintf(payload, sizeof(payload), "BASIC,%u",
                  static_cast<unsigned>(i));

    setStatus("Enviando");
    renderDisplay(::display);
    delay(5);

    setStatus("Esperando");
    renderDisplay(::display);

    const bool ok = radio.send(reinterpret_cast<const uint8_t*>(payload),
                               std::strlen(payload));

    if (ok) {
      ++metrics.ok;
      setStatus("ACK OK");
    } else {
      ++metrics.fail;
      setStatus("ACK fallo");
    }
    renderDisplay(::display);
    delay(kInterPacketDelayMs);
  }

  setStatus("Listo");
  batchFinished = true;
  renderDisplay(::display);
}
} // namespace

void setup() {
  initBoard();
  initDisplay(display);
  renderDisplay(::display);

  if (!radio.init(kPowerLevel, kSpreadingFactorLevel)) {
    setStatus("Init fallo");
    renderDisplay(::display);
    Serial.println("[basic_sender] radio init fallo");
    while (true) {
      delay(1000);
    }
  }

  setStatus("Listo");
  renderDisplay(::display);
  Serial.println("[basic_sender] enviando 50 mensajes...");

  runBatch();
  Serial.println("[basic_sender] lote finalizado, reinicia para repetir.");
}

void loop() {
  if (!batchFinished) {
    delay(50);
    return;
  }

  const uint32_t now = millis();
  if (now - lastReportMs >= 2000U) {
    Serial.printf("[basic_sender] OK=%u FAIL=%u\r\n",
                  static_cast<unsigned>(metrics.ok),
                  static_cast<unsigned>(metrics.fail));
    lastReportMs = now;
  }

  delay(100);
}


