#pragma once

/**
 * @file FrontendAssets.h
 * @brief Web Frontend HTML/CSS/JS stored in PROGMEM
 * 
 * Separated from WebManager for cleaner code organization.
 * Edit this file to modify the web dashboard UI.
 */

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>АКМ-1 // КЛИМАТ-КОНТРОЛЬ</title>
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
            <div class="title">АКМ-1 // КЛИМАТ-КОНТРОЛЬ</div>
            <div class="clock" id="clock">00:00:00</div>
        </div>

        <!-- ADVICE BOX -->
        <div class="advice-box" id="advice-box">
            <div>
                <div class="advice-label">СОВЕТ</div>
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
            <summary>⚙️ ОТЛАДКА / ПОГОДА</summary>
            <div class="debug-grid">
                <div class="debug-item">Ср. влажность (24ч): <span id="dbg-avg">--</span>%</div>
                <div class="debug-item">Абс. влажность (в): <span id="dbg-in-abs">--</span> г/м³</div>
                <hr style="grid-column: span 2; border: 0; border-top: 1px solid #333; width: 100%;">
                <div class="debug-item">Heap свободно: <span id="dbg-heap-free">--</span> КБ</div>
                <div class="debug-item">Heap мин: <span id="dbg-heap-min">--</span> КБ</div>
                <hr style="grid-column: span 2; border: 0; border-top: 1px solid #333; width: 100%;">
                <div class="debug-item">Погода (Вайден): <span id="dbg-weather-status">--</span></div>
                <div class="debug-item">Температура снаружи: <span id="dbg-out-t">--</span>°C</div>
                <div class="debug-item">Влажность снаружи: <span id="dbg-out-h">--</span>%</div>
                <div class="debug-item">Абс. влажность (сн): <span id="dbg-out-abs">--</span> г/м³</div>
                <hr style="grid-column: span 2; border: 0; border-top: 1px solid #333; width: 100%;">
                <div class="debug-item">Скорость сушки: <span id="dbg-drying-rate">--</span> г/м³/мин <span id="dbg-drying-ind">-</span></div>
            </div>
        </details>
    </div>

    <script>
        // --- Core Logic ---
        setInterval(() => {
            const now = new Date();
            document.getElementById('clock').innerText = now.toLocaleTimeString('ru-RU', {hour12: false});
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
                document.getElementById('dbg-heap-free').innerText = (data.debug.heap_free / 1024).toFixed(1);
                document.getElementById('dbg-heap-min').innerText = (data.debug.heap_min / 1024).toFixed(1);
                document.getElementById('dbg-weather-status').innerText = data.debug.status;
                if(data.debug.valid) {
                    document.getElementById('dbg-out-t').innerText = data.debug.out_t.toFixed(1);
                    document.getElementById('dbg-out-h').innerText = data.debug.out_h.toFixed(1);
                    document.getElementById('dbg-out-abs').innerText = data.debug.out_abs.toFixed(2);
                }
                // Drying Speedometer v3.3
                document.getElementById('dbg-drying-rate').innerText = data.debug.drying_rate.toFixed(3);
                document.getElementById('dbg-drying-ind').innerText = data.debug.drying_ind;
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
