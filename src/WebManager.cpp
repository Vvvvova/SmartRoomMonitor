#include "WebManager.h"
#if defined(ESP32)
#include <esp_task_wdt.h>
#endif

// -------------------------------------------------------------------------
// HTML & JS Bundle (Toast Notifications + Web Audio + Scaled Charts)
// -------------------------------------------------------------------------
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ACM-1 // CLIMATE CORE</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <link href="https://fonts.googleapis.com/css2?family=JetBrains+Mono:wght@400;700&family=Roboto:wght@300;400;700&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-color: #0b0c10;
            --card-bg: #1f2833;
            --text-main: #c5c6c7;
            --safe: #66fcf1;
            --warning: #ff9900;
            --danger: #ff3333;
            --border-subtle: rgba(255,255,255,0.1);
            --temp-color: #ffa500;
            --hum-color: #00ffff;
        }
        body {
            background-color: var(--bg-color);
            color: var(--text-main);
            font-family: 'Roboto', sans-serif;
            margin: 0;
            padding: 20px;
            display: flex;
            flex-direction: column;
            align-items: center;
            min-height: 100vh;
        }
        .container { width: 100%; max-width: 900px; }
        
        /* HEADER */
        .header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            border-bottom: 2px solid var(--safe);
            padding-bottom: 15px;
            margin-bottom: 30px;
        }
        .title {
            font-family: 'JetBrains Mono', monospace;
            color: var(--safe);
            font-size: 1.5rem;
            letter-spacing: 2px;
            font-weight: 700;
        }
        .clock {
            font-family: 'JetBrains Mono', monospace;
            color: var(--text-main);
            font-size: 1.2rem;
        }

        /* ADVICE BOX */
        .advice-box {
            background: linear-gradient(90deg, #1f2833 0%, #2a3b4c 100%);
            border-left: 4px solid var(--safe);
            padding: 15px;
            margin-bottom: 20px;
            font-family: 'Roboto', sans-serif;
            display: flex;
            align-items: center;
            justify-content: space-between;
            box-shadow: 0 4px 15px rgba(0,0,0,0.3);
            transition: all 0.5s ease;
        }
        .advice-text { font-size: 1.1rem; font-weight: bold; color: #fff; }
        .advice-label { font-size: 0.8rem; text-transform: uppercase; color: var(--safe); margin-bottom: 4px; }
        
        /* CARDS */
        .card {
            background-color: var(--card-bg);
            border: 1px solid var(--border-subtle);
            border-radius: 4px;
            padding: 20px;
            margin-bottom: 20px;
            box-shadow: 0 4px 15px rgba(0,0,0,0.5);
        }

        /* METRICS */
        .telemetry-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 20px;
            margin-bottom: 20px;
        }
        .metric-card { text-align: center; }
        .metric-label {
            text-transform: uppercase;
            font-size: 0.8rem;
            color: var(--safe);
            margin-bottom: 10px;
            letter-spacing: 1px;
        }
        
        /* STATUS CODES */
        .status-0 { border-left: 4px solid #4CAF50; background: linear-gradient(90deg, #1b262c 0%, #1f2e2e 100%); } /* GREEN */
        .status-1 { border-left: 4px solid #FFC107; background: linear-gradient(90deg, #1b262c 0%, #2e2c1f 100%); } /* YELLOW */
        .status-2 { border-left: 4px solid #F44336; background: linear-gradient(90deg, #1b262c 0%, #2e1f1f 100%); } /* RED */
        .status-3 { border-left: 4px solid #2196F3; background: linear-gradient(90deg, #1b262c 0%, #1f262e 100%); } /* BLUE */

        .metric-val {
            font-family: 'JetBrains Mono', monospace;
            font-size: 3.5rem;
            font-weight: 700;
        }
        .metric-unit { font-size: 1.2rem; color: var(--text-main); opacity: 0.7; }

        /* CHARTS */
        .chart-container { height: 250px; width: 100%; }
        .chart-title {
            font-size: 0.9rem; color: var(--text-main); margin-bottom: 5px; font-family: 'JetBrains Mono', monospace;
        }

        /* DEBUG */
        details {
            background-color: #171d25;
            padding: 10px;
            border-radius: 4px;
            border: 1px solid var(--border-subtle);
        }
        summary { cursor: pointer; color: var(--text-main); font-family: 'JetBrains Mono'; outline: none; }
        .debug-grid {
            display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-top: 10px;
            font-family: 'JetBrains Mono', monospace; font-size: 0.9rem; color: #888;
        }
        .debug-item span { color: #fff; }
    </style>
</head>
<body>
    <div class="container">
        <!-- HEADER -->
        <div class="header">
            <div class="title">ACM-1 // КЛИМАТ-КОНТРОЛЬ</div>
            <div class="clock" id="clock">00:00:00</div>
        </div>

        <!-- ADVICE BOX -->
        <div class="advice-box" id="advice-box">
            <div>
                <div class="advice-label">СОВЕТ СИСТЕМЫ</div>
                <div class="advice-text" id="advice-text">Загрузка...</div>
            </div>
            <div style="font-size: 2rem;">💡</div>
        </div>

        <!-- TELEMETRY -->
        <div class="telemetry-grid">
            <div class="card metric-card">
                <div class="metric-label">ТЕМПЕРАТУРА</div>
                <div class="metric-val" style="color: var(--temp-color);"><span id="val-t">--</span><span class="metric-unit">°C</span></div>
            </div>
            <div class="card metric-card">
                <div class="metric-label">ВЛАЖНОСТЬ</div>
                <div class="metric-val" style="color: var(--hum-color);"><span id="val-h">--</span><span class="metric-unit">%</span></div>
            </div>
            <div class="card metric-card">
                <div class="metric-label">ТОЧКА РОСЫ</div>
                <div class="metric-val"><span id="val-dp">--</span><span class="metric-unit">°C</span></div>
            </div>
        </div>

        <!-- CHARTS -->
        <div class="card">
            <div class="chart-title">ИСТОРИЯ ВЛАЖНОСТИ (%)</div>
            <div class="chart-container"><canvas id="humChart"></canvas></div>
            <hr style="border: 0; border-top: 1px solid var(--border-subtle); margin: 20px 0;">
            <div class="chart-title">ИСТОРИЯ ТЕМПЕРАТУРЫ (°C)</div>
            <div class="chart-container"><canvas id="tempChart"></canvas></div>
        </div>

        <!-- DEBUG SECTION -->
        <details>
            <summary>⚙️ ДАННЫЕ ОТЛАДКИ / ПОГОДА</summary>
            <div class="debug-grid">
                <div class="debug-item">Средняя Влажность (24ч): <span id="dbg-avg">--</span>%</div>
                <div class="debug-item">Абс. Влажность (Дома): <span id="dbg-in-abs">--</span> г/м³</div>
                <hr style="grid-column: span 2; border: 0; border-top: 1px solid #333; width: 100%;">
                <div class="debug-item">Погода (Weiden): <span id="dbg-weather-status">--</span></div>
                <div class="debug-item">Уличная Температура: <span id="dbg-out-t">--</span>°C</div>
                <div class="debug-item">Уличная Влажность: <span id="dbg-out-h">--</span>%</div>
                <div class="debug-item">Абс. Влажность (Улица): <span id="dbg-out-abs">--</span> г/м³</div>
            </div>
        </details>
    </div>

    <script>
        // --- Core Logic ---
        setInterval(() => {
            const now = new Date();
            document.getElementById('clock').innerText = now.toLocaleTimeString('ru-RU');
        }, 1000);

        let tempChart, humChart;

        function initCharts() {
            const commonOptions = {
                responsive: true, maintainAspectRatio: false, animation: false,
                interaction: { mode: 'index', intersect: false },
                plugins: { legend: { display: false } },
                elements: { point: { radius: 0, hitRadius: 20 }, line: { tension: 0.4 } }, // Smooth Curves
                scales: {
                    x: { display: false },
                    y: { display: true, grid: { color: 'rgba(255,255,255,0.05)' } }
                }
            };

            tempChart = new Chart(document.getElementById('tempChart').getContext('2d'), {
                type: 'line',
                data: { datasets: [{ label: 'Темп', borderColor: '#ffa500', backgroundColor: 'rgba(255,165,0,0.1)', fill: true, data: [] }] },
                options: { 
                    ...commonOptions, 
                    scales: { 
                        ...commonOptions.scales, 
                        y: { 
                            ...commonOptions.scales.y, 
                            ticks: { color: '#ffa500' },
                            suggestedMin: 15, // STABLE SCALE
                            suggestedMax: 30
                        } 
                    } 
                }
            });

            humChart = new Chart(document.getElementById('humChart').getContext('2d'), {
                type: 'line',
                data: { datasets: [{ label: 'Влаж', borderColor: '#00ffff', backgroundColor: 'rgba(0,255,255,0.1)', fill: true, data: [] }] },
                options: { 
                    ...commonOptions, 
                    scales: { 
                        ...commonOptions.scales, 
                        y: { 
                            ...commonOptions.scales.y, 
                            ticks: { color: '#00ffff' },
                            suggestedMin: 30, // STABLE SCALE
                            suggestedMax: 80 
                        },
                        x: { display: true, ticks: { color: '#888', maxTicksLimit: 8 } }
                    } 
                }
            });
        }

        // 1. FAST LOOP (Status Only) - 3s
        async function fetchStatus() {
            try {
                const res = await fetch('/api/status');
                const data = await res.json();

                // Values
                document.getElementById('val-t').innerText = data.t.toFixed(1);
                document.getElementById('val-h').innerText = data.h.toFixed(1);
                document.getElementById('val-dp').innerText = data.dp.toFixed(1);
                
                // Advice
                document.getElementById('advice-text').innerText = data.advice;
                
                const code = data.code;
                document.getElementById('advice-box').className = 'advice-box status-' + code;

                // Debug
                document.getElementById('dbg-avg').innerText = data.debug.avg.toFixed(1);
                document.getElementById('dbg-in-abs').innerText = data.debug.in_abs.toFixed(2);
                document.getElementById('dbg-weather-status').innerText = data.debug.status;
                if(data.debug.valid) {
                    document.getElementById('dbg-out-t').innerText = data.debug.out_t.toFixed(1);
                    document.getElementById('dbg-out-h').innerText = data.debug.out_h.toFixed(1);
                    document.getElementById('dbg-out-abs').innerText = data.debug.out_abs.toFixed(2);
                }
            } catch (e) {
                console.error("Status Sync Error", e);
            }
        }

        // 2. SLOW LOOP (Graphs High Payload) - 60s
        async function fetchHistory() {
            try {
                const res = await fetch('/api/history');
                const history = await res.json();

                if (tempChart && humChart) {
                    const labels = history.map(d => {
                        const date = new Date(d.time * 1000);
                        return date.getHours().toString().padStart(2,'0') + ":" + 
                                       date.getMinutes().toString().padStart(2,'0');
                    });
                    const tData = history.map(d => d.t);
                    const hData = history.map(d => d.h);

                    tempChart.data.labels = labels;
                    tempChart.data.datasets[0].data = tData;
                    tempChart.update('none');

                    humChart.data.labels = labels;
                    humChart.data.datasets[0].data = hData;
                    humChart.update('none');
                }
            } catch (e) {
                console.error("History Sync Error", e);
            }
        }

        initCharts();
        setInterval(fetchStatus, 3000); // 3s Status
        setInterval(fetchHistory, 60000); // 60s Graph
        fetchStatus();
        fetchHistory();
    </script>
</body>
</html>
)rawliteral";

WebManager::WebManager(SensorManager* sm) : server(80), sensorManager(sm) {}

void WebManager::begin() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html);
    });

    // 1. LIGHTWEIGHT STATUS API (Calling every 3s)
    server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request){
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        StaticJsonDocument<512> doc; // Static allocation - no heap fragmentation

        float t = sensorManager->getTemp();
        float h = sensorManager->getHum();
        
        doc["t"] = isnan(t) ? 0 : t;
        doc["h"] = isnan(h) ? 0 : h;
        doc["dp"] = sensorManager->getDewPoint();
        doc["advice"] = sensorManager->getRecommendation();
        doc["code"] = sensorManager->getAdviceCode();
        
        // Debug
        JsonObject dbg = doc.createNestedObject("debug");
        dbg["avg"] = sensorManager->getAvg24h();
        dbg["in_abs"] = sensorManager->getIndoorAbsHum();
        dbg["valid"] = sensorManager->isWeatherValid();
        dbg["status"] = sensorManager->getWeatherStatus();
        dbg["out_t"] = sensorManager->getOutdoorTemp();
        dbg["out_h"] = sensorManager->getOutdoorHum();
        dbg["out_abs"] = sensorManager->getOutdoorAbsHum();
        
        serializeJson(doc, *response);
        request->send(response);
    });

    // 2. HEAVY HISTORY API (Chunked Streaming - Zero RAM Allocation)
    server.on("/api/history", HTTP_GET, [this](AsyncWebServerRequest *request){
        // State for the chunker (Captured by value in lambda)
        // We use a safe batch size to prevent WDT and Stack Overflow
        struct ChunkerState {
            size_t offset = 0;
            bool finalized = false;
        };
        auto state = std::make_shared<ChunkerState>();
        
        request->send(request->beginChunkedResponse("application/json",
            [this, state](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
                // Returns 0 to signal end of stream
                if (state->finalized) return 0;

                // CRITICAL: Yield to allow WiFi and Watchdog to breathe
                // This prevents "Interrupt Watchdog" resets during slow transfers
                vTaskDelay(1); 
                #if defined(ESP32) && defined(CONFIG_ESP32_WDT)
                esp_task_wdt_reset();
                #endif

                size_t used = 0;
                
                // 1. Start Array
                if (state->offset == 0) {
                    if (maxLen > used) buffer[used++] = '[';
                }
                
                // 2. Determine Batch Size
                // We need enough space for at least one JSON object (~60 bytes)
                // If buffer is tiny, wait for next chunk
                if (maxLen - used < 64) return used;

                // Max items that fit in buffer (conservative estimate)
                size_t maxItems = (maxLen - used) / 64;
                // Cap at 32 to ensure we yield frequent enough (approx every 10-20ms)
                size_t batchLimit = (maxItems > 32) ? 32 : maxItems;
                
                if (batchLimit == 0) return used; // Should not happen given check above, but safety first

                Record batch[32]; 
                
                // 3. Fetch Batch (Thread Safe Copy)
                size_t count = sensorManager->copyHistory(state->offset, batchLimit, batch);
                
                // 4. Serialize Batch
                for (size_t i = 0; i < count; i++) {
                    // Check remaining space before writing
                    size_t remaining = maxLen - used;
                    if (remaining < 64) break; // Not enough space, continue in next chunk
                    
                    // Add comma if this is NOT the very first item
                    if (state->offset > 0 || i > 0) {
                        buffer[used++] = ',';
                        remaining--;
                    }
                    
                    // Format: {"t":22.5,"h":45.0,"time":1700000000}
                    int written = snprintf((char*)(buffer + used), remaining, 
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
                    // if no space for ']', we return 'used'. Next call, count will be 0 again, and we try ']' again.
                }

                return used;
            }
        ));
    });

    server.begin();
}
