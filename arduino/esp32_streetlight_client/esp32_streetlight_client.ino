/*
 * Smart Streetlight Monitoring System - Phase 1
 * ESP32 - hotspot setup + push client
 *
 * Listens on UART2 (RX = GPIO16, 9600 baud) for state frames from the UNO:
 *
 *   L1:ON,L2:OFF,L3:ON\n
 *
 * and PUSHES the state to the dashboard server running on your PC:
 *
 *   POST http://<server>/api/ingest   {"led1":"ON","led2":"OFF","led3":"ON"}
 *
 * First boot: the ESP32 opens an open hotspot "Streetlight-Setup". Connect
 * your phone to it and the setup page opens automatically (or browse to
 * http://192.168.4.1). Enter your Wi-Fi network, its password, and the
 * address of the PC running the dashboard (its IPv4 from `ipconfig`, e.g.
 * 192.168.1.50 — port 5173 is assumed unless you type IP:port). Settings
 * are saved in flash and reused on every later boot.
 *
 * The hotspot reappears automatically when the saved Wi-Fi cannot be
 * reached. To reconfigure manually:
 *   - press and hold the BOOT button shortly after power-up, or
 *   - type "reset" in the serial monitor.
 *
 * On-board LED (GPIO2):
 *   solid       - Wi-Fi up and the server accepts pushes
 *   fast blink  - setup hotspot active
 *   slow blink  - running, but Wi-Fi down or pushes failing
 *
 * External internet LEDs:
 *   red (GPIO18)   - no internet connection
 *   green (GPIO19) - internet connection available
 *
 * Passive piezo buzzer (GPIO23):
 *   short two-tone melody at boot, one confirmation tone when internet is reached
 *
 * Board: "ESP32 Dev Module" | Upload speed: 115200 | Serial monitor: 115200
 */

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <HardwareSerial.h>

const char* AP_SSID   = "Streetlight-Setup";
const char* HOSTNAME  = "streetlight-esp32";

const int UNO_RX_PIN  = 16;  // UNO TX, through the voltage divider
const int UNO_TX_PIN  = 17;  // unused; reserved for future UNO commands
const int INTERNET_RED_LED_PIN   = 18;
const int INTERNET_GREEN_LED_PIN = 19;
const int BUZZER_PIN = 23;
// GPIO0, the BOOT button. Not named BOOT_PIN: the ESP32 core already
// declares that identifier in esp32-hal.h.
const int SETUP_BUTTON_PIN = 0;

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

const uint32_t UART_BAUD = 9600;

const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;
const unsigned long WIFI_RETRY_MS           = 10000;
const unsigned long INTERNET_CHECK_INTERVAL_MS = 10000;
const unsigned long SERIAL_STALE_MS         = 5000;
const unsigned long PUSH_HEARTBEAT_MS       = 2000;
const unsigned long HTTP_TIMEOUT_MS         = 2500;

HardwareSerial UnoSerial(2);
WebServer portal(80);
DNSServer dns;
Preferences prefs;

String cfgSsid;
String cfgPass;
String cfgServer;  // "192.168.1.50" or "192.168.1.50:5173"

bool ledOn[3] = { false, false, false };

bool everReceivedFrame = false;
bool serialStaleWarned = false;
bool pushHealthy = false;
bool portalDone = false;
bool internetCheckStarted = false;
bool internetConnected = false;

unsigned long lastFrameMs = 0;
unsigned long lastPushMs = 0;
unsigned long lastWifiAttemptMs = 0;
unsigned long lastInternetCheckMs = 0;

String cmdBuffer;
String scannedNetworks;  // <option> entries for the setup page

// ----------------------------- saved config -----------------------------

void loadConfig() {
  prefs.begin("cfg", true);
  cfgSsid   = prefs.getString("ssid", "");
  cfgPass   = prefs.getString("pass", "");
  cfgServer = prefs.getString("server", "");
  prefs.end();
}

void saveConfig() {
  prefs.begin("cfg", false);
  prefs.putString("ssid", cfgSsid);
  prefs.putString("pass", cfgPass);
  prefs.putString("server", cfgServer);
  prefs.end();
}

void wipeConfig() {
  prefs.begin("cfg", false);
  prefs.clear();
  prefs.end();
}

// ------------------------------- helpers --------------------------------

String htmlEscape(const String& in) {
  String out;
  out.reserve(in.length());
  for (unsigned int i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == '&') out += F("&amp;");
    else if (c == '<') out += F("&lt;");
    else if (c == '>') out += F("&gt;");
    else if (c == '"') out += F("&quot;");
    else if (c == '\'') out += F("&#39;");
    else out += c;
  }
  return out;
}

// Adds the Vite dev server port when the user typed a bare IP.
String serverBaseUrl() {
  String s = cfgServer;
  s.trim();
  if (s.indexOf(':') < 0) s += F(":5173");
  return s;
}

bool tryConnectWifi(const String& ssid, const String& pass, bool abortOnBootButton) {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.print(F("Connecting to Wi-Fi \""));
  Serial.print(ssid);
  Serial.print(F("\""));
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (abortOnBootButton && digitalRead(SETUP_BUTTON_PIN) == LOW) {
      Serial.println(F("\nBOOT button held - opening the setup hotspot"));
      WiFi.disconnect();
      return false;
    }
    if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
      Serial.println(F("\nWi-Fi connection timed out"));
      WiFi.disconnect();
      return false;
    }
    delay(250);
    Serial.print('.');
  }
  Serial.println();
  Serial.print(F("Wi-Fi connected. IP address: "));
  Serial.println(WiFi.localIP());
  return true;
}

void maintainWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  if (millis() - lastWifiAttemptMs < WIFI_RETRY_MS) return;
  lastWifiAttemptMs = millis();
  Serial.println(F("Wi-Fi lost, reconnecting..."));
  WiFi.disconnect();
  WiFi.begin(cfgSsid.c_str(), cfgPass.c_str());
}

// ----------------------------- setup portal ------------------------------

String configPageHtml(const String& notice, bool isError) {
  String html;
  html.reserve(2048);
  html += F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<title>Streetlight Setup</title><style>"
            "body{font-family:system-ui,sans-serif;background:#0f172a;color:#e2e8f0;"
            "max-width:420px;margin:0 auto;padding:24px 16px}"
            "h1{font-size:1.25rem}code{color:#22c55e}"
            "label{display:block;margin:14px 0 4px;font-size:.9rem;color:#94a3b8}"
            "input{width:100%;box-sizing:border-box;padding:10px;border-radius:8px;"
            "border:1px solid #334155;background:#1e293b;color:#e2e8f0;font-size:1rem}"
            "select{width:100%;box-sizing:border-box;padding:10px;border-radius:8px;"
            "border:1px solid #334155;background:#1e293b;color:#e2e8f0;font-size:1rem}"
            "input[type=submit]{margin-top:20px;background:#22c55e;color:#052e16;"
            "border:0;font-weight:700;padding:12px;cursor:pointer}"
            ".hint{font-size:.8rem;color:#64748b;margin-top:4px}"
            ".notice{padding:10px;border-radius:8px;margin:0 0 12px;font-size:.9rem}"
            ".ok{background:#14532d}.err{background:#7f1d1d}"
            "</style></head><body>"
            "<h1>Smart Streetlight &mdash; Setup</h1>");
  if (notice.length() > 0) {
    html += isError ? F("<p class='notice err'>") : F("<p class='notice ok'>");
    html += notice;
    html += F("</p>");
  }
  html += F("<form method='POST' action='/save'>"
            "<label for='networkList'>Choose a detected Wi-Fi network</label>"
            "<select id='networkList' onchange='document.getElementById(\"ssid\").value=this.value'>");
  html += scannedNetworks;
  html += F("</select>"
            "<label for='ssid'>Wi-Fi network name (or enter it manually)</label>"
            "<input id='ssid' name='ssid' value='");
  html += htmlEscape(cfgSsid);
  html += F("' required>"
            "<label for='pass'>Wi-Fi password</label>"
            "<input id='pass' name='pass' type='password' placeholder='");
  html += (cfgPass.length() > 0) ? F("leave blank to keep the saved password")
                                 : F("Wi-Fi password");
  html += F("'>"
            "<label for='server'>Dashboard server address</label>"
            "<input id='server' name='server' value='");
  html += htmlEscape(cfgServer);
  html += F("' placeholder='192.168.1.50 or 192.168.1.50:5173' required>"
            "<p class='hint'>IPv4 address of the PC running the dashboard "
            "(<code>npm run dev</code>) - find it with <code>ipconfig</code>. "
            "Port 5173 is assumed unless you type <code>IP:port</code>.</p>"
            "<input type='submit' value='Save and connect'>"
            "</form></body></html>");
  return html;
}

void scanNetworks() {
  Serial.println(F("Scanning Wi-Fi networks..."));
  int n = WiFi.scanNetworks();
  scannedNetworks = "";
  if (n < 0) {
    scannedNetworks = F("<option value=''>Network scan failed - enter SSID manually</option>");
    Serial.printf("Wi-Fi scan failed (error %d); enter the SSID manually\n", n);
    WiFi.scanDelete();
    return;
  }
  scannedNetworks = F("<option value=''>Select a detected Wi-Fi network</option>");
  int visibleNetworks = 0;
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) continue;
    visibleNetworks++;
    scannedNetworks += F("<option value='");
    scannedNetworks += htmlEscape(ssid);
    scannedNetworks += F("'");
    if (ssid == cfgSsid) scannedNetworks += F(" selected");
    scannedNetworks += F(">");
    scannedNetworks += htmlEscape(ssid);
    scannedNetworks += F(" (");
    scannedNetworks += String(WiFi.RSSI(i));
    scannedNetworks += F(" dBm)</option>");
  }
  if (visibleNetworks == 0) {
    scannedNetworks = F("<option value=''>No networks detected - enter SSID manually</option>");
    Serial.println(F("No visible Wi-Fi networks found; enter the SSID manually"));
  }
  WiFi.scanDelete();
  if (visibleNetworks > 0) Serial.printf("Found %d visible network(s)\n", visibleNetworks);
}

void handlePortalRoot() {
  portal.send(200, "text/html", configPageHtml("", false));
}

void handlePortalSave() {
  String ssid = portal.arg("ssid");
  ssid.trim();
  String pass = portal.arg("pass");
  String server = portal.arg("server");
  server.trim();
  if (server.startsWith(F("http://"))) server = server.substring(7);
  if (server.endsWith("/")) server = server.substring(0, server.length() - 1);

  if (ssid.length() == 0 || server.length() == 0) {
    portal.send(400, "text/html",
                configPageHtml(F("Wi-Fi network and server address are required."), true));
    return;
  }
  if (pass.length() == 0 && cfgSsid == ssid) {
    pass = cfgPass;  // reconfiguring only the server: keep the saved password
  }
  cfgSsid = ssid;
  if (pass.length() > 0) cfgPass = pass;
  cfgServer = server;
  saveConfig();

  if (tryConnectWifi(cfgSsid, cfgPass, false)) {
    String page = F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
                    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                    "<title>Saved</title><style>"
                    "body{font-family:system-ui,sans-serif;background:#0f172a;color:#e2e8f0;"
                    "max-width:420px;margin:0 auto;padding:24px 16px}code{color:#22c55e}"
                    "</style></head><body><h1>Saved</h1>"
                    "<p>Wi-Fi connected. This hotspot will close and the ESP32 will start "
                    "pushing to <code>http://");
    page += serverBaseUrl();
    page += F("/api/ingest</code>.</p>"
             "<p>If the dashboard stays Offline, check that the dev server is running, the "
             "address above is right, and Windows Firewall allows Node.js on private "
             "networks.</p>"
             "<p>You can now reconnect your phone to your normal Wi-Fi.</p></body></html>");
    portal.send(200, "text/html", page);
    delay(1500);  // let the page reach the phone before the hotspot closes
    portalDone = true;
  } else {
    portal.send(200, "text/html",
                configPageHtml(F("Could not join that Wi-Fi network - check the name "
                                 "and password, then try again."),
                               true));
  }
}

void runSetupPortal() {
  Serial.println(F("Starting setup hotspot"));
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID);  // open network; the portal itself needs no password
  scanNetworks();

  IPAddress apIp = WiFi.softAPIP();  // 192.168.4.1
  dns.start(53, "*", apIp);
  String redirectUrl = F("http://");
  redirectUrl += apIp.toString();
  redirectUrl += '/';

  portal.on("/", HTTP_GET, handlePortalRoot);
  portal.on("/save", HTTP_POST, handlePortalSave);
  portal.onNotFound([&redirectUrl]() {
    // captive-portal probes (generate_204, hotspot-detect.html, ...) land here
    portal.sendHeader(F("Location"), redirectUrl, true);
    portal.send(302, F("text/plain"), "");
  });
  portal.begin();

  Serial.print(F("Connect your phone to the \""));
  Serial.print(AP_SSID);
  Serial.print(F("\" Wi-Fi - the setup page should open automatically, or visit "));
  Serial.println(redirectUrl);

  portalDone = false;
  while (!portalDone) {
    dns.processNextRequest();
    portal.handleClient();
    static unsigned long lastToggleMs = 0;
    static bool ledOnNow = false;
    if (millis() - lastToggleMs >= 150) {
      lastToggleMs = millis();
      ledOnNow = !ledOnNow;
      digitalWrite(LED_BUILTIN, ledOnNow);
    }
    delay(2);
  }

  dns.stop();
  portal.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
}

// --------------------------- UNO serial input ----------------------------

void applyToken(const String& token) {
  // Expected token shape: L<1-3>:<ON|OFF>
  if (token.length() < 5 || token[0] != 'L' || token[2] != ':') return;
  int index = token[1] - '1';
  if (index < 0 || index > 2) return;
  String value = token.substring(3);
  value.trim();
  value.toUpperCase();
  if (value != "ON" && value != "OFF") return;

  bool on = (value == "ON");
  if (ledOn[index] != on) {
    ledOn[index] = on;
    Serial.printf("L%d -> %s\n", index + 1, on ? "ON" : "OFF");
  }
}

void parseFrame(const String& frame) {
  int start = 0;
  while (start <= (int)frame.length()) {
    int comma = frame.indexOf(',', start);
    String token = (comma == -1) ? frame.substring(start) : frame.substring(start, comma);
    token.trim();
    if (token.length() > 0) {
      applyToken(token);
    }
    if (comma == -1) break;
    start = comma + 1;
  }
}

bool readUnoSerial() {
  static String buffer;
  bool frameParsed = false;
  while (UnoSerial.available() > 0) {
    char c = (char)UnoSerial.read();
    if (c == '\n') {
      buffer.trim();
      if (buffer.length() > 0) {
        parseFrame(buffer);
        lastFrameMs = millis();
        everReceivedFrame = true;
        serialStaleWarned = false;
        frameParsed = true;
      }
      buffer = "";
    } else if (c != '\r') {
      buffer += c;
      if (buffer.length() > 64) {  // discard corrupted/oversized input
        buffer = "";
      }
    }
  }
  return frameParsed;
}

void reportSerialHealth() {
  if (!everReceivedFrame || serialStaleWarned) return;
  if (millis() - lastFrameMs < SERIAL_STALE_MS) return;
  serialStaleWarned = true;
  Serial.println(F("WARNING: no data from UNO for 5s - pushing last known state"));
}

// ------------------------------ push client ------------------------------

String buildStateJson() {
  String json;
  json.reserve(64);
  json += F("{\"led1\":\"");
  json += ledOn[0] ? F("ON") : F("OFF");
  json += F("\",\"led2\":\"");
  json += ledOn[1] ? F("ON") : F("OFF");
  json += F("\",\"led3\":\"");
  json += ledOn[2] ? F("ON") : F("OFF");
  json += F("\"}");
  return json;
}

void pushState() {
  HTTPClient http;
  String url = F("http://");
  url += serverBaseUrl();
  url += F("/api/ingest");
  http.begin(url);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.addHeader(F("Content-Type"), F("application/json"));
  int code = http.POST(buildStateJson());
  http.end();

  bool ok = (code == 200);
  if (ok != pushHealthy) {
    if (ok) {
      Serial.print(F("Pushing to "));
      Serial.print(url);
      Serial.println(F(" - OK"));
    } else if (code > 0) {
      Serial.printf("Server rejected the push (HTTP %d)\n", code);
    } else {
      Serial.print(F("Server unreachable at "));
      Serial.print(url);
      Serial.println(F(" - check the address, that the dashboard is running, and the PC firewall"));
    }
    pushHealthy = ok;
  }
  lastPushMs = millis();
}

// ------------------------------ status LED -------------------------------

void updateStatusLed() {
  if (WiFi.status() == WL_CONNECTED && pushHealthy) {
    digitalWrite(LED_BUILTIN, HIGH);
    return;
  }
  static unsigned long lastToggleMs = 0;
  static bool ledOnNow = false;
  if (millis() - lastToggleMs >= 400) {
    lastToggleMs = millis();
    ledOnNow = !ledOnNow;
    digitalWrite(LED_BUILTIN, ledOnNow);
  }
}

void updateInternetLeds() {
  if (WiFi.status() != WL_CONNECTED) {
    internetConnected = false;
  } else if (!internetCheckStarted ||
             millis() - lastInternetCheckMs >= INTERNET_CHECK_INTERVAL_MS) {
    internetCheckStarted = true;
    lastInternetCheckMs = millis();

    bool wasConnected = internetConnected;
    HTTPClient http;
    String checkUrl = F("http://connectivitycheck.gstatic.com/generate_204");
    if (http.begin(checkUrl)) {
      http.setTimeout(HTTP_TIMEOUT_MS);
      int responseCode = http.GET();
      http.end();
      internetConnected = (responseCode == 204);
      if (internetConnected && !wasConnected) {
        tone(BUZZER_PIN, 2200, 300);
      }
    } else {
      internetConnected = false;
      Serial.println(F("Could not start internet connectivity check"));
    }
    Serial.println(internetConnected ? F("Internet reachable") : F("No internet connection"));
  }

  digitalWrite(INTERNET_RED_LED_PIN, internetConnected ? LOW : HIGH);
  digitalWrite(INTERNET_GREEN_LED_PIN, internetConnected ? HIGH : LOW);
}

void playBootSound() {
  tone(BUZZER_PIN, 1600, 120);
  delay(150);
  tone(BUZZER_PIN, 2200, 120);
  delay(150);
  noTone(BUZZER_PIN);
}

// --------------------------- serial commands -----------------------------

void handleSerialCommand() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n') {
      cmdBuffer.trim();
      cmdBuffer.toLowerCase();
      if (cmdBuffer == "reset") {
        Serial.println(F("Clearing saved settings and opening the setup hotspot..."));
        wipeConfig();
        delay(100);
        ESP.restart();  // boots with no config -> setup hotspot
      }
      cmdBuffer = "";
    } else if (c != '\r') {
      cmdBuffer += c;
      if (cmdBuffer.length() > 32) cmdBuffer = "";
    }
  }
}

// --------------------------------- main ----------------------------------

void setup() {
  pinMode(SETUP_BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(INTERNET_RED_LED_PIN, OUTPUT);
  pinMode(INTERNET_GREEN_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(INTERNET_RED_LED_PIN, HIGH);
  digitalWrite(INTERNET_GREEN_LED_PIN, LOW);
  playBootSound();

  Serial.begin(115200);
  UnoSerial.begin(UART_BAUD, SERIAL_8N1, UNO_RX_PIN, UNO_TX_PIN);

  loadConfig();

  bool connected = false;
  if (cfgSsid.length() > 0 && cfgServer.length() > 0) {
    connected = tryConnectWifi(cfgSsid, cfgPass, true);
  } else {
    Serial.println(F("No saved settings yet."));
  }
  if (!connected) {
    runSetupPortal();
  }

  Serial.print(F("Pushing streetlight state to http://"));
  Serial.print(serverBaseUrl());
  Serial.println(F("/api/ingest"));
}

void loop() {
  bool frameArrived = readUnoSerial();
  handleSerialCommand();
  maintainWifi();
  updateInternetLeds();
  reportSerialHealth();

  if (WiFi.status() == WL_CONNECTED) {
    // push on every frame (~1/s); heartbeat keeps pushing the last known
    // state even when the UNO goes silent
    if (frameArrived || millis() - lastPushMs >= PUSH_HEARTBEAT_MS) {
      pushState();
    }
  }

  updateStatusLed();
}
