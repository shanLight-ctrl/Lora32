#include <Arduino.h>
#include <RadioLib.h>
#include <ArduinoJson.h>
#include <SSD1306Wire.h>

// ── LoRa (SX1262) ─────────────────────────────────────
SX1262 radio = new Module(8, 14, 12, 13);

// ── OLED (ThingPulse, 700 kHz — Heltec V3 native) ────
#define OLED_RST 21
SSD1306Wire display(0x3c, 17, 18, GEOMETRY_128_64, I2C_ONE, 700000);

// ── Last received data ─────────────────────────────────
float g_alt = 0, g_temp = 0, g_ax = 0, g_ay = 0, g_az = 0, g_rssi = 0;
int   g_sats = 0;
bool  g_hasData = false;

#define DISPLAY_INTERVAL_MS 250
unsigned long lastDisplay = 0;

void updateDisplay() {
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0,  "Pioneer Ground RX");
    display.drawString(0, 11, "------------------");
    if (!g_hasData) {
        display.drawString(0, 22, "Waiting for signal");
        display.drawString(0, 33, "...");
    } else {
        display.drawString(0, 22, "Alt: " + String(g_alt, 1)  + " m");
        display.drawString(0, 33, "Tmp: " + String(g_temp, 1) + " C");
        display.drawString(0, 44, "GPS: " + (g_sats > 0 ? String(g_sats) + " sats" : String("No fix")));
        display.drawString(0, 55, "RSSI: " + String((int)g_rssi) + " dBm");
    }
    display.display();
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    // Enable Vext to power the OLED panel (Heltec V3 specific)
    pinMode(36, OUTPUT);
    digitalWrite(36, LOW);
    delay(100);

    // Manual OLED reset
    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, LOW);
    delay(50);
    digitalWrite(OLED_RST, HIGH);
    delay(50);

    if (!display.init()) {
        Serial.println("OLED init failed");
    } else {
        Serial.println("OLED OK");
    }
    display.setContrast(255);
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0, "Pioneer Ground RX");
    display.drawString(0, 12, "Booting...");
    display.display();

    int state = radio.begin(915.0);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("LoRa failed, code %d\n", state);
        display.clear();
        display.drawString(0, 0, "LoRa FAILED!");
        display.display();
        while (1);
    }
    Serial.println("Ready! Waiting for packets...");
    display.clear();
    display.drawString(0, 0, "Pioneer Ground RX");
    display.drawString(0, 12, "Waiting...");
    display.display();
}

void loop() {
    String received;
    int state = radio.receive(received, 200);

    if (state == RADIOLIB_ERR_NONE) {
        // Strip non-JSON trailing bytes
        int end = received.lastIndexOf('}');
        if (end >= 0) received = received.substring(0, end + 1);

        StaticJsonDocument<512> doc;
        if (deserializeJson(doc, received) == DeserializationError::Ok) {
            g_alt  = doc["alt"]  | 0.0f;
            g_temp = doc["temp"] | 0.0f;
            g_sats = doc["sats"] | 0;
            g_ax   = doc["ax"]   | 0.0f;
            g_ay   = doc["ay"]   | 0.0f;
            g_az   = doc["az"]   | 0.0f;
            g_rssi = radio.getRSSI();
            g_hasData = true;
            doc["rssi"] = (int)g_rssi;
            String out;
            serializeJson(doc, out);
            Serial.println(out);
        }
    }

    if (millis() - lastDisplay >= DISPLAY_INTERVAL_MS) {
        lastDisplay = millis();
        updateDisplay();
    }
}
