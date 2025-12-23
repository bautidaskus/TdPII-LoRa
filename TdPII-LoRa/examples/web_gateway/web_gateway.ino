#include <LoRaWan_APP.h>
#include <HT_SSD1306Wire.h>
#include <WiFi.h>
#include <cstring>
#include <cstdio>
#include <TdPIILoRa.h>
#include "GatewayHtml.h"

using namespace TdPII;

namespace {
constexpr char kSsid[] = "TdPII-LoRa"; // SSID del Access Point WiFi
constexpr char kPassword[] = "12345678"; // password del AP (mínimo 8 caracteres)
constexpr uint8_t kMaxLogEntries = 20; // cantidad máxima de eventos en log circular
constexpr uint32_t kRecvTimeoutMs = 2000; // timeout de recepción LoRa (2 segundos)
constexpr size_t kDisplayChars = 16; // máximo caracteres por línea en display OLED
constexpr size_t kLineBufferSize = kDisplayChars + 1; // +1 para null terminator
constexpr uint8_t kPowerLevel = 3; // 1=10dBm, 2=15dBm, 3=20dBm
constexpr uint8_t kSpreadingFactorLevel = 3; // 1=SF7, 2=SF10, 3=SF12
const IPAddress kApIp(10, 10, 10, 10);
const IPAddress kApGateway(10, 10, 10, 10);
const IPAddress kApSubnet(255, 255, 255, 0);

struct GatewayMetrics {
  uint32_t receivedOk = 0;
  uint32_t timeouts = 0;
} metrics;

String logLines[kMaxLogEntries];
size_t logIndex = 0;

char lastStatus[32] = "Esperando";
char lastPayload[96] = "";
char apIpLine[32] = "";

void appendLog(const String& entry) {
  logLines[logIndex] = entry;
  logIndex = (logIndex + 1) % kMaxLogEntries;
  Serial.println(entry);
}

void makeLine(char* dest, size_t destSize, const char* prefix, const char* value) {
  if (destSize == 0) {
    return;
  }
  size_t idx = 0;
  while (prefix[idx] != '\0' && idx < destSize - 1 && idx < kDisplayChars) {
    dest[idx] = prefix[idx];
    ++idx;
  }
  size_t remaining = (idx < destSize - 1) ? (destSize - 1 - idx) : 0;
  size_t valueLen = strlen(value);
  if (valueLen > remaining) {
    valueLen = remaining;
  }
  if (valueLen > 0) {
    memcpy(dest + idx, value, valueLen);
    idx += valueLen;
  }
  dest[idx] = '\0';
}

void initBoard() {
  Serial.begin(115200);
  delay(20);
  SPI.begin(SCK, MISO, MOSI, SS);
  Mcu.begin(WIFI_LORA_32_V2, 0);
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

void renderDisplay(SSD1306Wire& display) {
  char line[kLineBufferSize];

  display.clear();
  display.drawString(0, 0, "TdPII Gateway");

  snprintf(line, sizeof(line), "OK:%lu FAIL:%lu",
           static_cast<unsigned long>(metrics.receivedOk),
           static_cast<unsigned long>(metrics.timeouts));
  line[kDisplayChars] = '\0';
  display.drawString(0, 16, line);

  makeLine(line, sizeof(line), "Estado:", lastStatus);
  display.drawString(0, 32, line);

  display.display();
}
} 

extern SSD1306Wire display;
TdPIILoRa radio;
WiFiServer server(80);

void handleClient(WiFiClient& client) {
  TdPII::GatewayHtml::writePage(
      client,
      lastStatus,
      lastPayload,
      static_cast<unsigned long>(metrics.receivedOk),
      static_cast<unsigned long>(metrics.timeouts),
      apIpLine,
      logLines,
      kMaxLogEntries,
      logIndex);
}

void setup() {
  initBoard();
  initDisplay(display);
  renderDisplay(display);

  if (!radio.init(kPowerLevel, kSpreadingFactorLevel)) {
    snprintf(lastStatus, sizeof(lastStatus), "Init fallo");
    renderDisplay(display);
    appendLog("[gateway] radio init fallo");
    while (true) {
      delay(1000);
    }
  }

  WiFi.mode(WIFI_AP);
  if (!WiFi.softAPConfig(kApIp, kApGateway, kApSubnet)) {
    appendLog("[gateway] error al configurar IP del AP");
  }
  if (WiFi.softAP(kSsid, kPassword)) {
    IPAddress ip = WiFi.softAPIP();
    snprintf(apIpLine, sizeof(apIpLine), "%s", ip.toString().c_str());
    appendLog(String("[gateway] AP listo IP=") + ip.toString());
  } else {
    snprintf(apIpLine, sizeof(apIpLine), "AP fallo");
    appendLog("[gateway] error al iniciar AP");
  }
  server.begin();

  snprintf(lastStatus, sizeof(lastStatus), "Esperando");
  renderDisplay(display);
  appendLog("[gateway] listo");
}

void loop() {
  static uint32_t lastDisplayUpdate = 0;
  static uint32_t lastMessageMs = 0;
  const uint32_t kTimeoutWindowMs = 8000;

  uint32_t now = millis();
  if (lastMessageMs == 0) {
    lastMessageMs = now;
  }

  uint8_t buffer[255];
  const int received = radio.recv(buffer, sizeof(buffer), kRecvTimeoutMs);
  now = millis();

  if (received > 0) {
    const size_t copyLen = static_cast<size_t>(received) < sizeof(lastPayload) - 1
                               ? static_cast<size_t>(received)
                               : sizeof(lastPayload) - 1;
    memcpy(lastPayload, buffer, copyLen);
    lastPayload[copyLen] = '\0';

    ++metrics.receivedOk;
    snprintf(lastStatus, sizeof(lastStatus), "RX OK");
    lastMessageMs = now;
    renderDisplay(display);
    lastDisplayUpdate = now;
    appendLog(String("[gateway] RX: ") + lastPayload);
  } else {
    if ((now - lastMessageMs) >= kTimeoutWindowMs) {
      ++metrics.timeouts;
      snprintf(lastStatus, sizeof(lastStatus), "Timeout");
      lastMessageMs = now;
      renderDisplay(display);
      lastDisplayUpdate = now;
      appendLog("[gateway] timeout sin datos");
    } else {
      snprintf(lastStatus, sizeof(lastStatus), "Esperando");
    }
  }

  if (now - lastDisplayUpdate > 1000) {
    renderDisplay(display);
    lastDisplayUpdate = now;
  }

  WiFiClient client = server.available();
  if (client) {
    // Antes: esto metía ruido en la página
    // appendLog("[gateway] cliente conectado");
    Serial.println("[gateway] cliente conectado");

    // Espera hasta 1 s a que llegue algo de la solicitud
    uint32_t t0 = millis();
    while (client.connected() && client.available() == 0 && (millis() - t0) < 1000) {
      delay(5);
    }

    // Drenar cabeceras HTTP (hasta \r\n\r\n)
    String line;
    bool endHeaders = false;
    t0 = millis();
    while (client.connected() && !endHeaders && (millis() - t0) < 1000) {
      while (client.available()) {
        char c = client.read();
        if (c == '\n') {
          if (line.length() == 0 || line == "\r") { endHeaders = true; break; }
          line = "";
        } else if (c != '\r') {
          line += c;
        }
      }
      delay(1);
    }

    // Generar y enviar la respuesta
    handleClient(client);

    // Dar tiempo a que se envíe
    delay(80);

    // Descartar resto y cerrar
    while (client.available()) client.read();
    client.stop();

    // Antes: también metía ruido en la página
    // appendLog("[gateway] cliente desconectado");
    Serial.println("[gateway] cliente desconectado");
  }

}

