/*
 * Smart Streetlight Monitoring System - Phase 1
 * Arduino UNO - streetlight controller
 *
 * Drives 3 LED streetlights and reports their state to the ESP32 over
 * UART (9600 baud) once per second, using this frame format:
 *
 *   L1:ON,L2:OFF,L3:ON\n
 *
 * Each streetlight is self-powered: solar panel -> TP4056 charger -> 18650
 * battery, and the battery (not the UNO) powers a 3V 3W COB LED strip.
 * An IRLZ44N logic-level MOSFET switches the strip, driven by the pins below. Build x3:
 *
 *   Solar + -> TP4056 IN+          Solar - -> TP4056 IN-
 *   18650 + -> TP4056 B+           18650 - -> TP4056 B-
 *   TP4056 OUT+ -> COB strip + pad (no resistor; strip matches battery)
 *   strip - pad -> IRLZ44N drain
 *   IRLZ44N source -> TP4056 OUT-
 *   D5 -> 220R -> gate of light 1  (D6 for light 2, D7 for light 3)
 *   10kR from gate to source       (pulldown: keeps the strip dark at boot)
 *   UNO GND -> each TP4056 OUT-    (common ground is required)
 *
 *   D1 (TX) -> voltage divider (1k/2k) -> ESP32 GPIO16 (RX)
 *   GND -> ESP32 GND
 *
 * The same frames are visible in the USB serial monitor at 9600 baud.
 * If uploading fails, temporarily unplug the wire leaving D1 (TX).
 */

const uint8_t LED_PINS[3] = { 5, 6, 7 };  // Streetlight 1, 2, 3

const bool DEMO_MODE = true;  // cycle patterns to demo live dashboard updates
const unsigned long SEND_INTERVAL_MS = 1000;
const unsigned long DEMO_STEP_MS = 5000;

const bool DEMO_PATTERNS[][3] = {
  { true,  false, true  },
  { true,  true,  false },
  { false, true,  true  },
  { true,  true,  true  },
};

bool ledState[3] = { true, false, true };  // initial pattern: ON, OFF, ON
uint8_t demoStep = 0;
unsigned long lastSendMs = 0;
unsigned long lastDemoMs = 0;

void applyLeds() {
  for (uint8_t i = 0; i < 3; i++) {
    digitalWrite(LED_PINS[i], ledState[i] ? HIGH : LOW);
  }
}

void sendState() {
  Serial.print(F("L1:"));
  Serial.print(ledState[0] ? F("ON") : F("OFF"));
  Serial.print(F(",L2:"));
  Serial.print(ledState[1] ? F("ON") : F("OFF"));
  Serial.print(F(",L3:"));
  Serial.print(ledState[2] ? F("ON") : F("OFF"));
  Serial.write('\n');
}

void setup() {
  for (uint8_t i = 0; i < 3; i++) {
    pinMode(LED_PINS[i], OUTPUT);
  }
  applyLeds();
  Serial.begin(9600);
}

void loop() {
  unsigned long now = millis();

  if (DEMO_MODE && now - lastDemoMs >= DEMO_STEP_MS) {
    lastDemoMs = now;
    demoStep = (demoStep + 1) % (sizeof(DEMO_PATTERNS) / sizeof(DEMO_PATTERNS[0]));
    for (uint8_t i = 0; i < 3; i++) {
      ledState[i] = DEMO_PATTERNS[demoStep][i];
    }
    applyLeds();
  }

  if (now - lastSendMs >= SEND_INTERVAL_MS) {
    lastSendMs = now;
    sendState();
  }
}
