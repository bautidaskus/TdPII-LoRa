#ifndef TDPII_LORA_GATEWAY_HTML_H
#define TDPII_LORA_GATEWAY_HTML_H

#include <Arduino.h>
#include <Print.h>

namespace TdPII {
namespace GatewayHtml {

static const char kHttpHtmlHeader[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Connection: close\r\n\r\n"
    "<!DOCTYPE html><html><head>"
    "<meta charset='utf-8'>"
    "<meta http-equiv='refresh' content='2'>"
    "<title>TdPII LoRa Gateway</title>"
    "<style>body{font-family:sans-serif;margin:16px;}"
    "ul{padding-left:18px;} h1{margin-bottom:4px;}</style>"
    "</head><body>";

static const char kHtmlFooter[] = "</body></html>";

inline void writeHttpHeader(Print& out) {
    out.print(kHttpHtmlHeader);
}

inline void writeHtmlFooter(Print& out) {
    out.print(kHtmlFooter);
}

inline void writeStatusSection(Print& out,
                               const char* status,
                               const char* lastPayload,
                               unsigned long receivedOk,
                               unsigned long timeouts,
                               const char* apIp) {
    out.print("<h1>");
    out.print(status);
    out.print("</h1>");

    out.print("<p>Ultimo mensaje: ");
    out.print(lastPayload);
    out.print("</p>");

    out.print("<p>Recibidos: ");
    out.print(String(receivedOk));
    out.print(" &nbsp; Timeouts: ");
    out.print(String(timeouts));
    out.print("</p>");

    out.print("<p>Access Point IP: ");
    out.print(apIp);
    out.print("</p>");
}

inline String renderLogsHtml(const String* logLines, size_t maxEntries, size_t startIndex) {
    String html;
    html.reserve(512);
    html += "<h3>Eventos recientes</h3><ul>";
    for (size_t i = 0; i < maxEntries; ++i) {
        size_t idx = (startIndex + i) % maxEntries;
        const String& entry = logLines[idx];
        if (entry.length() == 0) {
            continue;
        }
        html += "<li>";
        html += entry;
        html += "</li>";
    }
    html += "</ul>";
    return html;
}

inline void writePage(Print& out,
                      const char* status,
                      const char* lastPayload,
                      unsigned long receivedOk,
                      unsigned long timeouts,
                      const char* apIp,
                      const String* logLines,
                      size_t maxEntries,
                      size_t startIndex) {
    writeHttpHeader(out);
    writeStatusSection(out, status, lastPayload, receivedOk, timeouts, apIp);
    out.print(renderLogsHtml(logLines, maxEntries, startIndex));
    writeHtmlFooter(out);
}

} // namespace GatewayHtml
} // namespace TdPII

#endif // TDPII_LORA_GATEWAY_HTML_H

