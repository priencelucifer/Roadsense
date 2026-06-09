/*
 * ESP32 GPS + IMU Live Web Dashboard + Hazard Detection
 * 
 * Pin Configuration:
 *   IMU (BMI160) - I2C:
 *     SDA -> GPIO 23
 *     SCL -> GPIO 22
 *   GPS (NEO-M8N) - UART2:
 *     GPS TX -> GPIO 16 (ESP32 RX2)
 *     GPS RX -> GPIO 17 (ESP32 TX2)
 *
 * Libraries needed:
 *   - TinyGPS++        (for GPS parsing)
 *   - DFRobot_BMI160   (for BMI160 IMU)
 *   - WiFi             (built-in)
 *   - WebServer        (built-in)
 *   - HTTPClient       (built-in)
 *   - WiFiClientSecure (built-in)
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <Wire.h>
#include <TinyGPS++.h>
#include <DFRobot_BMI160.h>
#include <math.h>

// ======================== CONFIGURATION ========================
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Cloud API
const char* API_URL    = "https://your-api-endpoint.com/api/hazards";
const char* SECRET_KEY = "your_secret_key_here";

// Hardware Pins
#define I2C_SDA 23
#define I2C_SCL 22
#define GPS_RX  16
#define GPS_TX  17
#define GPS_BAUD 115200

// BMI160 I2C address
const int8_t IMU_ADDR = 0x69;

// ======================== HAZARD TUNING ========================
const unsigned long CALIBRATION_TIME = 120000;  // 2 minutes calibration
const float SPIKE_THRESHOLD = 0.4;              // G above baseline = hazard (lowered for normal jerk)
const unsigned long HAZARD_COOLDOWN = 3000;     // 3 seconds between reports

// ======================== GLOBALS ========================
WebServer server(80);
TinyGPSPlus gps;
DFRobot_BMI160 bmi160;

bool imuReady = false;

// IMU data
float accelX = 0, accelY = 0, accelZ = 0;
float gyroX  = 0, gyroY  = 0, gyroZ  = 0;
float currentGForce = 1.0;

// GPS data
double latitude  = 0.0;
double longitude = 0.0;
double altitude  = 0.0;
double speed_kmh = 0.0;
int    satellites = 0;
String gpsTime   = "--:--:--";
String gpsDate   = "--/--/----";
bool   gpsFix    = false;

// Calibration & Hazard
bool isCalibrated = false;
double sumGForce = 0;
unsigned long readingCount = 0;
float baselineGForce = 1.0;
unsigned long lastHazardTime = 0;
String lastHazardStatus = "None";
int hazardCount = 0;

// Wi-Fi Fallback Location
float wifiLat = 0.0;
float wifiLng = 0.0;
bool hasWifiLocation = false;

// Timing
unsigned long lastIMURead = 0;
const int IMU_READ_INTERVAL = 20; // 50 Hz like working code
unsigned long lastWiringCheck = 0;
bool wiringError = false;
unsigned long lastStatusPrint = 0;

// ======================== WEB PAGE ========================
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 GPS &amp; IMU Dashboard</title>
  <style>
    @import url('https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&display=swap');

    * { margin: 0; padding: 0; box-sizing: border-box; }

    body {
      font-family: 'Inter', sans-serif;
      background: #0a0e1a;
      color: #e0e6f0;
      min-height: 100vh;
      overflow-x: hidden;
    }

    body::before {
      content: '';
      position: fixed;
      top: -50%; left: -50%;
      width: 200%; height: 200%;
      background: radial-gradient(ellipse at 20% 50%, rgba(56, 189, 248, 0.08) 0%, transparent 50%),
                  radial-gradient(ellipse at 80% 20%, rgba(139, 92, 246, 0.08) 0%, transparent 50%),
                  radial-gradient(ellipse at 50% 80%, rgba(16, 185, 129, 0.06) 0%, transparent 50%);
      animation: bgShift 20s ease-in-out infinite alternate;
      z-index: 0;
    }
    @keyframes bgShift {
      0% { transform: translate(0, 0) rotate(0deg); }
      100% { transform: translate(-5%, 3%) rotate(3deg); }
    }

    .container {
      position: relative;
      z-index: 1;
      max-width: 1100px;
      margin: 0 auto;
      padding: 24px 16px;
    }

    header { text-align: center; margin-bottom: 32px; }
    header h1 {
      font-size: 2rem;
      font-weight: 700;
      background: linear-gradient(135deg, #38bdf8, #8b5cf6, #10b981);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
      background-clip: text;
    }
    header p { color: #64748b; font-size: 0.85rem; margin-top: 6px; }

    #status {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      background: rgba(16, 185, 129, 0.1);
      border: 1px solid rgba(16, 185, 129, 0.25);
      padding: 6px 16px;
      border-radius: 20px;
      font-size: 0.8rem;
      color: #10b981;
      margin-top: 10px;
      transition: all 0.3s;
    }
    #status.error {
      background: rgba(239, 68, 68, 0.1);
      border-color: rgba(239, 68, 68, 0.25);
      color: #ef4444;
    }
    #status .dot {
      width: 8px; height: 8px;
      border-radius: 50%;
      background: #10b981;
      animation: pulse 2s infinite;
    }
    #status.error .dot { background: #ef4444; }
    @keyframes pulse {
      0%, 100% { opacity: 1; transform: scale(1); }
      50% { opacity: 0.4; transform: scale(0.8); }
    }

    .grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 20px;
    }
    @media (max-width: 700px) {
      .grid { grid-template-columns: 1fr; }
    }

    .card {
      background: rgba(15, 23, 42, 0.7);
      border: 1px solid rgba(148, 163, 184, 0.1);
      border-radius: 16px;
      padding: 24px;
      backdrop-filter: blur(12px);
      transition: border-color 0.3s, box-shadow 0.3s;
    }
    .card:hover {
      border-color: rgba(148, 163, 184, 0.2);
      box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);
    }

    .card-title {
      display: flex;
      align-items: center;
      gap: 10px;
      font-size: 0.85rem;
      font-weight: 600;
      text-transform: uppercase;
      letter-spacing: 1px;
      margin-bottom: 20px;
    }
    .card-title .icon {
      width: 32px; height: 32px;
      border-radius: 8px;
      display: flex;
      align-items: center;
      justify-content: center;
      font-size: 1rem;
    }
    .gps-card .card-title .icon { background: rgba(56, 189, 248, 0.15); color: #38bdf8; }
    .gps-card .card-title { color: #38bdf8; }
    .accel-card .card-title .icon { background: rgba(139, 92, 246, 0.15); color: #8b5cf6; }
    .accel-card .card-title { color: #8b5cf6; }
    .gyro-card .card-title .icon { background: rgba(16, 185, 129, 0.15); color: #10b981; }
    .gyro-card .card-title { color: #10b981; }
    .hazard-card .card-title .icon { background: rgba(239, 68, 68, 0.15); color: #ef4444; }
    .hazard-card .card-title { color: #ef4444; }

    .data-row {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 10px 0;
      border-bottom: 1px solid rgba(148, 163, 184, 0.06);
    }
    .data-row:last-child { border-bottom: none; }
    .data-label { font-size: 0.82rem; color: #94a3b8; }
    .data-value { font-size: 1rem; font-weight: 600; font-variant-numeric: tabular-nums; }

    .axis-x { color: #f87171; }
    .axis-y { color: #34d399; }
    .axis-z { color: #60a5fa; }
    .gps-val { color: #e2e8f0; }

    .bar-container { display: flex; align-items: center; gap: 10px; }
    .bar { height: 6px; border-radius: 3px; transition: width 0.3s ease; min-width: 4px; }
    .bar-x { background: linear-gradient(90deg, #f87171, #fca5a5); }
    .bar-y { background: linear-gradient(90deg, #34d399, #6ee7b7); }
    .bar-z { background: linear-gradient(90deg, #60a5fa, #93c5fd); }

    .full-width { grid-column: 1 / -1; }

    .fix-badge {
      display: inline-block; padding: 2px 10px;
      border-radius: 10px; font-size: 0.75rem; font-weight: 600;
    }
    .fix-yes { background: rgba(16, 185, 129, 0.15); color: #10b981; border: 1px solid rgba(16, 185, 129, 0.3); }
    .fix-no { background: rgba(239, 68, 68, 0.15); color: #ef4444; border: 1px solid rgba(239, 68, 68, 0.3); }

    .gforce-big { font-size: 2.5rem; font-weight: 700; text-align: center; margin: 10px 0; }
    .hazard-flash { animation: hazardFlash 0.5s ease 3; }
    @keyframes hazardFlash {
      0%, 100% { border-color: rgba(148, 163, 184, 0.1); }
      50% { border-color: #ef4444; box-shadow: 0 0 30px rgba(239, 68, 68, 0.3); }
    }
    .calibrating { color: #fbbf24; }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <h1>&#128752; RoadSense Dashboard</h1>
      <p>BMI160 IMU &amp; NEO-M8N GPS &mdash; Hazard Detection &amp; Live Telemetry</p>
      <div id="status">
        <span class="dot"></span>
        <span id="statusText">Connecting...</span>
      </div>
    </header>

    <div class="grid">
      <!-- Hazard Card -->
      <div class="card hazard-card full-width" id="hazardCard">
        <div class="card-title"><div class="icon">&#9888;</div> Hazard Detection</div>
        <div class="gforce-big" id="gforce" style="color:#10b981;">1.00 G</div>
        <div style="display:grid; grid-template-columns: 1fr 1fr; gap: 0 32px;">
          <div class="data-row">
            <span class="data-label">Status</span>
            <span class="data-value" id="calStatus" style="color:#fbbf24;">Calibrating...</span>
          </div>
          <div class="data-row">
            <span class="data-label">Baseline</span>
            <span class="data-value gps-val" id="baseline">-- G</span>
          </div>
          <div class="data-row">
            <span class="data-label">Threshold</span>
            <span class="data-value gps-val" id="threshold">-- G</span>
          </div>
          <div class="data-row">
            <span class="data-label">Bumps Detected</span>
            <span class="data-value" id="hazardCount" style="color:#ef4444;">0</span>
          </div>
          <div class="data-row">
            <span class="data-label">Last Event</span>
            <span class="data-value gps-val" id="lastHazard">None</span>
          </div>
        </div>
      </div>

      <!-- GPS Card -->
      <div class="card gps-card full-width">
        <div class="card-title"><div class="icon">&#127760;</div> GPS Data</div>
        <div style="display:grid; grid-template-columns: 1fr 1fr; gap: 0 32px;">
          <div class="data-row">
            <span class="data-label">Latitude</span>
            <span class="data-value gps-val" id="lat">--</span>
          </div>
          <div class="data-row">
            <span class="data-label">Longitude</span>
            <span class="data-value gps-val" id="lng">--</span>
          </div>
          <div class="data-row">
            <span class="data-label">Altitude</span>
            <span class="data-value gps-val" id="alt">-- m</span>
          </div>
          <div class="data-row">
            <span class="data-label">Speed</span>
            <span class="data-value gps-val" id="spd">-- km/h</span>
          </div>
          <div class="data-row">
            <span class="data-label">Satellites</span>
            <span class="data-value gps-val" id="sats">--</span>
          </div>
          <div class="data-row">
            <span class="data-label">Fix</span>
            <span class="data-value" id="fix"><span class="fix-badge fix-no">NO FIX</span></span>
          </div>
          <div class="data-row">
            <span class="data-label">Date</span>
            <span class="data-value gps-val" id="date">--/--/----</span>
          </div>
          <div class="data-row">
            <span class="data-label">Time (UTC)</span>
            <span class="data-value gps-val" id="time">--:--:--</span>
          </div>
        </div>
      </div>

      <!-- Accelerometer Card -->
      <div class="card accel-card">
        <div class="card-title"><div class="icon">&#9889;</div> Accelerometer</div>
        <div class="data-row">
          <span class="data-label">X-Axis</span>
          <div class="bar-container">
            <div class="bar bar-x" id="axBar" style="width:20px;"></div>
            <span class="data-value axis-x" id="ax">0.00 g</span>
          </div>
        </div>
        <div class="data-row">
          <span class="data-label">Y-Axis</span>
          <div class="bar-container">
            <div class="bar bar-y" id="ayBar" style="width:20px;"></div>
            <span class="data-value axis-y" id="ay">0.00 g</span>
          </div>
        </div>
        <div class="data-row">
          <span class="data-label">Z-Axis</span>
          <div class="bar-container">
            <div class="bar bar-z" id="azBar" style="width:20px;"></div>
            <span class="data-value axis-z" id="az">0.00 g</span>
          </div>
        </div>
      </div>

      <!-- Gyroscope Card -->
      <div class="card gyro-card">
        <div class="card-title"><div class="icon">&#128260;</div> Gyroscope</div>
        <div class="data-row">
          <span class="data-label">X-Axis</span>
          <div class="bar-container">
            <div class="bar bar-x" id="gxBar" style="width:20px;"></div>
            <span class="data-value axis-x" id="gx">0.00 &deg;/s</span>
          </div>
        </div>
        <div class="data-row">
          <span class="data-label">Y-Axis</span>
          <div class="bar-container">
            <div class="bar bar-y" id="gyBar" style="width:20px;"></div>
            <span class="data-value axis-y" id="gy">0.00 &deg;/s</span>
          </div>
        </div>
        <div class="data-row">
          <span class="data-label">Z-Axis</span>
          <div class="bar-container">
            <div class="bar bar-z" id="gzBar" style="width:20px;"></div>
            <span class="data-value axis-z" id="gz">0.00 &deg;/s</span>
          </div>
        </div>
      </div>
    </div>
  </div>

  <script>
    let prevHazardCount = 0;

    function fetchData() {
      fetch('/api/data')
        .then(r => r.json())
        .then(d => {
          document.getElementById('status').className = '';
          document.getElementById('statusText').textContent = 'Connected \u2014 Live';

          if (d.wiring_error) {
            document.getElementById('statusText').textContent = '\u26a0 GPS Wiring Error!';
            document.getElementById('status').className = 'error';
          }

          // GPS
          document.getElementById('lat').textContent = d.lat.toFixed(6) + '\u00B0';
          document.getElementById('lng').textContent = d.lng.toFixed(6) + '\u00B0';
          document.getElementById('alt').textContent = d.alt.toFixed(1) + ' m';
          document.getElementById('spd').textContent = d.spd.toFixed(1) + ' km/h';
          document.getElementById('sats').textContent = d.sats;
          document.getElementById('date').textContent = d.date;
          document.getElementById('time').textContent = d.time;

          const fixEl = document.getElementById('fix');
          fixEl.innerHTML = d.fix
            ? '<span class="fix-badge fix-yes">3D FIX</span>'
            : '<span class="fix-badge fix-no">NO FIX</span>';

          // Accel
          setAxis('ax', 'axBar', d.ax, ' g', 4);
          setAxis('ay', 'ayBar', d.ay, ' g', 4);
          setAxis('az', 'azBar', d.az, ' g', 4);

          // Gyro
          setAxis('gx', 'gxBar', d.gx, ' \u00B0/s', 500);
          setAxis('gy', 'gyBar', d.gy, ' \u00B0/s', 500);
          setAxis('gz', 'gzBar', d.gz, ' \u00B0/s', 500);

          // G-Force display
          const gfEl = document.getElementById('gforce');
          gfEl.textContent = d.gforce.toFixed(2) + ' G';
          if (d.calibrated) {
            gfEl.style.color = d.gforce > d.threshold ? '#ef4444' : '#10b981';
          }

          // Hazard info
          document.getElementById('calStatus').textContent = d.calibrated ? 'Active \u2014 Monitoring' : 'Calibrating...';
          document.getElementById('calStatus').style.color = d.calibrated ? '#10b981' : '#fbbf24';
          document.getElementById('baseline').textContent = d.baseline.toFixed(2) + ' G';
          document.getElementById('threshold').textContent = d.threshold.toFixed(2) + ' G';
          document.getElementById('hazardCount').textContent = d.hazards;
          document.getElementById('lastHazard').textContent = d.last_hazard;

          // Flash card on new hazard
          if (d.hazards > prevHazardCount) {
            document.getElementById('hazardCard').classList.remove('hazard-flash');
            void document.getElementById('hazardCard').offsetWidth;
            document.getElementById('hazardCard').classList.add('hazard-flash');
          }
          prevHazardCount = d.hazards;
        })
        .catch(() => {
          document.getElementById('status').className = 'error';
          document.getElementById('statusText').textContent = 'Disconnected';
        });
    }
    setInterval(fetchData, 1000);
    fetchData();

    function setAxis(valId, barId, value, unit, maxVal) {
      document.getElementById(valId).textContent = value.toFixed(2) + unit;
      const pct = Math.min(Math.abs(value) / maxVal * 100, 100);
      document.getElementById(barId).style.width = Math.max(pct, 3) + '%';
    }
  </script>
</body>
</html>
)rawliteral";

// ======================== WI-FI GEOLOCATION FALLBACK ========================
void fetchWifiLocationFallback() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Downloading approximate location via Wi-Fi...");

    WiFiClient basicClient;
    HTTPClient http;
    http.begin(basicClient, "http://ip-api.com/csv/?fields=lat,lon");

    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      int commaIndex = payload.indexOf(',');
      if (commaIndex > 0) {
        wifiLat = payload.substring(0, commaIndex).toFloat();
        wifiLng = payload.substring(commaIndex + 1).toFloat();
        hasWifiLocation = true;
        Serial.printf("Wi-Fi Location: %.6f, %.6f\n", wifiLat, wifiLng);
      }
    } else {
      Serial.printf("Wi-Fi location failed. Code: %d\n", httpCode);
    }
    http.end();
  }
}

// ======================== CLOUD API ========================
void sendHazardToCloud(float lat, float lng, float severityG) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, API_URL);
    http.addHeader("Content-Type", "application/json");

    String payload = "{";
    payload += "\"lat\":" + String(lat, 6) + ",";
    payload += "\"lng\":" + String(lng, 6) + ",";
    payload += "\"type\":\"pothole\",";
    payload += "\"severity\":" + String(severityG, 1) + ",";
    payload += "\"secret_key\":\"" + String(SECRET_KEY) + "\"";
    payload += "}";

    Serial.println("Sending Hazard to Cloud...");
    Serial.println(payload);

    int httpResponseCode = http.POST(payload);
    if (httpResponseCode > 0) {
      Serial.printf("Cloud OK: %d\n", httpResponseCode);
      lastHazardStatus = "Sent (" + String(severityG, 1) + "G)";
    } else {
      Serial.printf("Cloud Error: %d\n", httpResponseCode);
      lastHazardStatus = "Send Failed";
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected. Cannot send.");
    lastHazardStatus = "No WiFi";
  }
}

// ======================== SETUP ========================
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=== RoadSense GPS + IMU Dashboard ===");

  // 1. Initialize GPS FIRST (like working code)
  Serial2.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
  Serial.printf("GPS UART2: RX=%d, TX=%d, Baud=%d\n", GPS_RX, GPS_TX, GPS_BAUD);

  // 2. Initialize I2C and BMI160 (like working code)
  Wire.begin(I2C_SDA, I2C_SCL);
  if (bmi160.softReset() != BMI160_OK || bmi160.I2cInit(IMU_ADDR) != BMI160_OK) {
    Serial.println("BMI160 Init Failed! Check Wiring.");
    imuReady = false;
  } else {
    imuReady = true;
    Serial.println("BMI160 OK!");
  }

  // 3. Connect to WiFi
  Serial.printf("Connecting to WiFi: %s ", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" CONNECTED!");
    Serial.print("Dashboard URL: http://");
    Serial.println(WiFi.localIP());
    // Grab backup Wi-Fi location
    fetchWifiLocationFallback();
  } else {
    Serial.println(" FAILED! Restarting...");
    delay(5000);
    ESP.restart();
  }

  // 4. Start web server (polling, no SSE)
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/data", HTTP_GET, handleData);
  server.begin();
  Serial.println("Web server started on port 80");
  Serial.println("Starting 2-minute calibration...");
  Serial.println("==========================================");
}

// ======================== LOOP ========================
void loop() {
  // Handle web clients
  server.handleClient();

  // Read GPS (exactly like working code)
  while (Serial2.available() > 0) {
    char c = Serial2.read();
    gps.encode(c);
  }

  // Update GPS variables
  updateGPSVars();

  // Read IMU & detect hazards
  if (millis() - lastIMURead >= IMU_READ_INTERVAL) {
    lastIMURead = millis();
    readIMUAndDetect();
  }

  // GPS wiring check every 5 seconds
  if (millis() - lastWiringCheck > 5000) {
    lastWiringCheck = millis();
    wiringError = (gps.charsProcessed() < 10);
  }

  // Print status to serial every 10 seconds
  if (millis() - lastStatusPrint > 10000) {
    lastStatusPrint = millis();
    printSystemStatus();
  }
}

// ======================== IMU READ + HAZARD DETECTION ========================
void readIMUAndDetect() {
  if (!imuReady) return;

  int16_t accelGyro[6] = {0};

  if (bmi160.getSensorData(BMI160_GYRO_SEL | BMI160_ACCEL_SEL, accelGyro) == BMI160_OK) {
    // Accel at indices 3,4,5 — Gyro at indices 0,1,2
    accelX = accelGyro[3] / 16384.0;
    accelY = accelGyro[4] / 16384.0;
    accelZ = accelGyro[5] / 16384.0;

    gyroX = accelGyro[0] / 16.4;
    gyroY = accelGyro[1] / 16.4;
    gyroZ = accelGyro[2] / 16.4;

    currentGForce = sqrt((accelX * accelX) + (accelY * accelY) + (accelZ * accelZ));

    // PHASE 1: CALIBRATION (first 2 minutes)
    if (millis() < CALIBRATION_TIME) {
      sumGForce += currentGForce;
      readingCount++;
    }
    // PHASE 2: ACTIVE MONITORING
    else {
      if (!isCalibrated) {
        baselineGForce = sumGForce / readingCount;
        isCalibrated = true;
        Serial.printf("\nCALIBRATION COMPLETE! Baseline: %.2f G\n", baselineGForce);
      }

      float dynamicThreshold = baselineGForce + SPIKE_THRESHOLD;

      if (currentGForce > dynamicThreshold && (millis() - lastHazardTime > HAZARD_COOLDOWN)) {
        Serial.printf("\nBUMP DETECTED! G-Force: %.2f G (Threshold: %.2f)\n", currentGForce, dynamicThreshold);
        hazardCount++;

        // Priority 1: Hardware GPS
        if (gps.location.isValid()) {
          Serial.println("Using Hardware GPS Location.");
          sendHazardToCloud(gps.location.lat(), gps.location.lng(), currentGForce);
        }
        // Priority 2: Wi-Fi fallback
        else if (hasWifiLocation) {
          Serial.println("GPS searching. Using Wi-Fi fallback location.");
          sendHazardToCloud(wifiLat, wifiLng, currentGForce);
        }
        // Priority 3: No location
        else {
          Serial.println("No location available. Skipping upload.");
          lastHazardStatus = "No Location";
        }

        lastHazardTime = millis();
      }
    }
  }
}

// ======================== GPS UPDATE ========================
void updateGPSVars() {
  if (gps.location.isValid()) {
    latitude  = gps.location.lat();
    longitude = gps.location.lng();
    gpsFix    = true;
  } else {
    gpsFix = false;
  }
  if (gps.altitude.isValid()) altitude = gps.altitude.meters();
  if (gps.speed.isValid()) speed_kmh = gps.speed.kmph();
  if (gps.satellites.isValid()) satellites = gps.satellites.value();
  if (gps.time.isValid()) {
    char buf[10];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
             gps.time.hour(), gps.time.minute(), gps.time.second());
    gpsTime = String(buf);
  }
  if (gps.date.isValid()) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%02d/%02d/%04d",
             gps.date.day(), gps.date.month(), gps.date.year());
    gpsDate = String(buf);
  }
}

// ======================== STATUS PRINT ========================
void printSystemStatus() {
  Serial.println("\n--- SYSTEM STATUS ---");
  Serial.print("WiFi: ");
  Serial.println(WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED");

  Serial.print("GPS:  ");
  if (gps.charsProcessed() < 10) {
    Serial.println("NO DATA (Check wiring!)");
  } else if (gps.location.isValid()) {
    Serial.printf("LOCKED (%d sats)\n", gps.satellites.value());
  } else {
    Serial.printf("SEARCHING (%d sats)\n", gps.satellites.value());
  }

  Serial.printf("IMU:  %s | G-Force: %.2f\n", imuReady ? "OK" : "FAILED", currentGForce);
  Serial.printf("Cal:  %s | Baseline: %.2f | Bumps: %d\n",
                isCalibrated ? "DONE" : "IN PROGRESS", baselineGForce, hazardCount);
  Serial.println("---------------------\n");
}

// ======================== WEB HANDLERS ========================

void handleRoot() {
  server.send_P(200, "text/html", HTML_PAGE);
}

void handleData() {
  float dynThreshold = baselineGForce + SPIKE_THRESHOLD;

  String json = "{";
  json += "\"ax\":" + String(accelX, 3) + ",";
  json += "\"ay\":" + String(accelY, 3) + ",";
  json += "\"az\":" + String(accelZ, 3) + ",";
  json += "\"gx\":" + String(gyroX, 3) + ",";
  json += "\"gy\":" + String(gyroY, 3) + ",";
  json += "\"gz\":" + String(gyroZ, 3) + ",";
  json += "\"gforce\":" + String(currentGForce, 3) + ",";
  json += "\"lat\":" + String(latitude, 6) + ",";
  json += "\"lng\":" + String(longitude, 6) + ",";
  json += "\"alt\":" + String(altitude, 1) + ",";
  json += "\"spd\":" + String(speed_kmh, 1) + ",";
  json += "\"sats\":" + String(satellites) + ",";
  json += "\"fix\":" + String(gpsFix ? "true" : "false") + ",";
  json += "\"wiring_error\":" + String(wiringError ? "true" : "false") + ",";
  json += "\"calibrated\":" + String(isCalibrated ? "true" : "false") + ",";
  json += "\"baseline\":" + String(baselineGForce, 2) + ",";
  json += "\"threshold\":" + String(dynThreshold, 2) + ",";
  json += "\"hazards\":" + String(hazardCount) + ",";
  json += "\"last_hazard\":\"" + lastHazardStatus + "\",";
  json += "\"time\":\"" + gpsTime + "\",";
  json += "\"date\":\"" + gpsDate + "\"";
  json += "}";

  server.send(200, "application/json", json);
}
