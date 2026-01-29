#include "WebManager.h"
#include "CoreState.h"
#include "FrontendAssets.h" // HTML/CSS/JS separated
#if defined(ESP32)
#include <esp_task_wdt.h>
#endif

// SensorManager dependency removed - History is now in CoreState!

WebManager::WebManager() : server(80) {}


void WebManager::begin() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  // 1. LIGHTWEIGHT STATUS API (Calling every 3s)
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncResponseStream *response =
        request->beginResponseStream("application/json");
    StaticJsonDocument<640> doc; // Static allocation - no heap fragmentation

    // Read from CoreState (safe snapshot)
    CoreSnapshot sn = g_state.getSnapshot();
    float t = sn.temp;
    float h = sn.hum;
    float dp = sn.dewPoint;
    String advice = sn.advice;
    int code = sn.adviceCode;
    float avg = sn.avg24h;
    float inAbs = sn.absHum;
    uint32_t heapFree = sn.heapFree;
    uint32_t heapMin = sn.heapMin;
    bool valid = sn.weatherValid;
    String status = sn.weatherStatus;
    float outT = sn.outTemp;
    float outH = sn.outHum;
    float outAbs = sn.outAbsHum;
    float dryingRate = sn.dryingRate;
    String dryingInd = sn.dryingInd;

    doc["t"] = isnan(t) ? 0 : t;
    doc["h"] = isnan(h) ? 0 : h;
    doc["dp"] = isnan(dp) ? 0 : dp;
    doc["advice"] = advice;
    doc["code"] = code;

    // Debug
    JsonObject dbg = doc.createNestedObject("debug");
    dbg["avg"] = isnan(avg) ? 0 : avg;
    dbg["in_abs"] = isnan(inAbs) ? 0 : inAbs;
    dbg["heap_free"] = heapFree;
    dbg["heap_min"] = heapMin;
    dbg["valid"] = valid;
    dbg["status"] = status;
    dbg["out_t"] = isnan(outT) ? 0 : outT;
    dbg["out_h"] = isnan(outH) ? 0 : outH;
    dbg["out_abs"] = isnan(outAbs) ? 0 : outAbs;
    dbg["drying_rate"] = dryingRate;
    dbg["drying_ind"] = dryingInd;

    serializeJson(doc, *response);
    request->send(response);
  });

  // 2. HEAVY HISTORY API (Chunked Streaming - Zero RAM Allocation)
  server.on("/api/history", HTTP_GET, [](AsyncWebServerRequest *request) {
    // State for the chunker (Captured by value in lambda)
    // We use a safe batch size to prevent WDT and Stack Overflow
    struct ChunkerState {
      size_t offset = 0;
      bool finalized = false;
    };
    auto state = std::make_shared<ChunkerState>();

    request->send(request->beginChunkedResponse(
        "application/json",
        [state](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
          // Returns 0 to signal end of stream
          if (state->finalized)
            return 0;

          // CRITICAL: Yield to allow WiFi and Watchdog to breathe
          // This prevents "Interrupt Watchdog" resets during slow transfers
          vTaskDelay(1);
#if defined(ESP32) && defined(CONFIG_ESP32_WDT)
          esp_task_wdt_reset();
#endif

          size_t used = 0;

          // 1. Start Array
          if (state->offset == 0) {
            if (maxLen > used)
              buffer[used++] = '[';
          }

          // 2. Determine Batch Size
          // We need enough space for at least one JSON object (~60 bytes)
          // If buffer is tiny, wait for next chunk
          if (maxLen - used < 64)
            return used;

          // Max items that fit in buffer (conservative estimate)
          size_t maxItems = (maxLen - used) / 64;
          // Cap at 32 to ensure we yield frequent enough (approx every 10-20ms)
          size_t batchLimit = (maxItems > 32) ? 32 : maxItems;

          if (batchLimit == 0)
            return used; // Should not happen given check above, but safety
                         // first

          Record batch[32];

          // 3. Fetch Batch (Thread Safe Copy) - uses CoreState directly!
          size_t count =
              g_state.copyHistoryChunk(state->offset, batchLimit, batch);

          // 4. Serialize Batch
          for (size_t i = 0; i < count; i++) {
            // Check remaining space before writing
            size_t remaining = maxLen - used;
            if (remaining < 64)
              break; // Not enough space, continue in next chunk

            // Add comma if this is NOT the very first item
            if (state->offset > 0 || i > 0) {
              buffer[used++] = ',';
              remaining--;
            }

            // Format: {"t":22.5,"h":45.0,"time":1700000000}
            int written = snprintf((char *)(buffer + used), remaining,
                                   "{\"t\":%.1f,\"h\":%.1f,\"time\":%lu}",
                                   isnan(batch[i].t) ? 0.0f : batch[i].t,
                                   isnan(batch[i].h) ? 0.0f : batch[i].h,
                                   (unsigned long)batch[i].ts);

            // FIX: Proper snprintf overflow check
            if (written > 0 && written < (int)remaining) {
              used += written;
            } else {
              // Truncation occurred or error, stop this batch
              break;
            }
          }

          state->offset += count;

          // 5. Finalize if Done
          if (count == 0) {
            // We asked for data but got 0 -> End of Buffer
            if (maxLen - used >= 1) {
              buffer[used++] = ']';
              state->finalized = true;
            }
            // if no space for ']', we return 'used'. Next call, count will be 0
            // again, and we try ']' again.
          }

          return used;
        }));
  });

  server.begin();
}
