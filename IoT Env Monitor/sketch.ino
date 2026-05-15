/*
 * IoT Environmental Monitor — ESP32 + DHT22 + SSD1306 OLED + SD + Web Dashboard
 * Target: Wokwi (ESP32)
 *
 * Required libraries (libraries.txt for Wokwi):
 *   SSD1306Ascii
 *   DHT sensor library
 *   Adafruit Unified Sensor
 */
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SSD1306Ascii.h>
#include <SSD1306AsciiWire.h>
#include <DHT.h>

// ---------- Pin / config ----------
#define DHT_PIN    15
#define DHT_TYPE   DHT22
#define SD_CS       5
#define I2C_SDA    21
#define I2C_SCL    22
#define OLED_ADDR 0x3C

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

const unsigned long SAMPLE_INTERVAL = 2000;

// ---------- Globals ----------
DHT               dht(DHT_PIN, DHT_TYPE);
SSD1306AsciiWire  oled;
WebServer         server(80);

float    temperature = NAN;
float    humidity    = NAN;
uint32_t sampleCount = 0;
bool     sdReady     = false;
unsigned long lastSampleMs = 0;

// ---------- Forward declarations ----------
void readSensor();
void updateOLED();
void logToSD();
void handleRoot();
void handleData();

// ---------- Dashboard HTML ----------
const char DASHBOARD_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="en"><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Environmental Monitor</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,Segoe UI,Roboto,sans-serif;
background:linear-gradient(135deg,#0f172a,#1e293b);color:#e2e8f0;min-height:100vh;padding:24px}
.wrap{max-width:720px;margin:0 auto}
h1{font-size:28px;text-align:center;color:#38bdf8;margin-bottom:6px}
.sub{text-align:center;color:#64748b;font-size:13px;margin-bottom:28px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:16px}
@media(max-width:520px){.grid{grid-template-columns:1fr}}
.card{background:#1e293b;border:1px solid #334155;border-radius:14px;padding:28px;text-align:center}
.label{font-size:11px;letter-spacing:2px;text-transform:uppercase;color:#94a3b8;margin-bottom:10px}
.value{font-size:44px;font-weight:700;color:#fbbf24}
.value.hum{color:#34d399}
.unit{font-size:18px;color:#94a3b8;margin-left:4px}
.stats{margin-top:20px;display:flex;justify-content:space-between;font-size:12px;color:#64748b;
padding:14px 20px;background:#0f172a;border-radius:10px;border:1px solid #1e293b}
.dot{display:inline-block;width:8px;height:8px;border-radius:50%;background:#22c55e;
margin-right:6px;animation:pulse 2s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.4}}
</style></head><body>
<div class="wrap">
<h1>Environmental Monitor</h1>
<p class="sub">Live readings from ESP32 &middot; updated every 2s</p>
<div class="grid">
<div class="card"><div class="label">Temperature</div>
<div class="value"><span id="t">--</span><span class="unit">&deg;C</span></div></div>
<div class="card"><div class="label">Humidity</div>
<div class="value hum"><span id="h">--</span><span class="unit">%</span></div></div>
</div>
<div class="stats"><span><span class="dot"></span>Live</span>
<span>Samples: <b id="n">0</b></span><span>Uptime: <b id="u">0s</b></span></div>
</div>
<script>
async function tick(){try{const r=await fetch('/data');const d=await r.json();
document.getElementById('t').textContent=d.temp.toFixed(1);
document.getElementById('h').textContent=d.hum.toFixed(1);
document.getElementById('n').textContent=d.n;
document.getElementById('u').textContent=Math.floor(d.up/1000)+'s';}catch(e){}}
tick();setInterval(tick,2000);
</script></body></html>
)HTML";

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n--- ESP32 Environmental Monitor ---");

  // I2C + OLED
  Wire.begin(I2C_SDA, I2C_SCL);
  oled.begin(&Adafruit128x64, OLED_ADDR);
  oled.setFont(System5x7);
  oled.clear();
  oled.println("Booting...");

  // DHT
  dht.begin();
  Serial.println("DHT22 ready");

  // SD card (optional — runs fine without)
  if (SD.begin(SD_CS)) {
    sdReady = true;
    Serial.println("SD card mounted");
    if (!SD.exists("/log.csv")) {
      File f = SD.open("/log.csv", FILE_WRITE);
      if (f) {
        f.println("millis,temperature_C,humidity_%");
        f.close();
      }
    }
  } else {
    Serial.println("SD init FAILED (continuing without logging)");
  }

  // WiFi
  oled.clear();
  oled.println("WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Dashboard ready at http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");

  // Web server
  server.on("/",     handleRoot);
  server.on("/data", handleData);
  server.begin();
}

// ---------- Loop ----------
void loop() {
  server.handleClient();
  if (millis() - lastSampleMs >= SAMPLE_INTERVAL) {
    lastSampleMs = millis();
    readSensor();
    updateOLED();
    logToSD();
  }
}

// ---------- Helpers ----------
void readSensor() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (!isnan(h) && !isnan(t)) {
    humidity    = h;
    temperature = t;
    sampleCount++;
    Serial.printf("[%lu] T=%.2f C  H=%.2f %%\n", sampleCount, t, h);
  } else {
    Serial.println("DHT read failed");
  }
}

void updateOLED() {
  oled.clear();
  oled.set1X();
  oled.println("Env Monitor");
  oled.println("-----------");
  oled.set2X();
  if (!isnan(temperature)) { oled.print(temperature, 1); oled.println(" C"); }
  else                     { oled.println("-- C"); }
  if (!isnan(humidity))    { oled.print(humidity, 1);    oled.println(" %"); }
  else                     { oled.println("-- %"); }
  oled.set1X();
  oled.print("IP:");
  oled.println(WiFi.localIP());
}

void logToSD() {
  if (!sdReady) return;
  File f = SD.open("/log.csv", FILE_APPEND);
  if (!f) return;
  f.print(millis());        f.print(",");
  f.print(temperature, 2);  f.print(",");
  f.println(humidity, 2);
  f.close();
}

void handleRoot() {
  server.send_P(200, "text/html", DASHBOARD_HTML);
}

void handleData() {
  float t = isnan(temperature) ? 0.0f : temperature;
  float h = isnan(humidity)    ? 0.0f : humidity;
  String json = "{";
  json += "\"temp\":" + String(t, 2) + ",";
  json += "\"hum\":"  + String(h, 2) + ",";
  json += "\"n\":"    + String(sampleCount) + ",";
  json += "\"up\":"   + String(millis());
  json += "}";
  server.send(200, "application/json", json);
}
