#include <Arduino.h>
#include <Wire.h>
#include <RadioLib.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <SSD1306Wire.h>

// ── LoRa (SX1262) ─────────────────────────────────────
SX1262 radio = new Module(8, 14, 12, 13);

// ── I2C buses ─────────────────────────────────────────
#define SDA_PIN   41   // external sensors
#define SCL_PIN   42
#define OLED_RST  21

// ── OLED on Wire1 (ThingPulse, Heltec V3 native) ──────
SSD1306Wire display(0x3c, 17, 18, GEOMETRY_128_64, I2C_TWO, 700000);

// ── GPS ───────────────────────────────────────────────
#define GPS_RX_PIN 36
#define GPS_TX_PIN 37
HardwareSerial gpsSerial(1);
TinyGPSPlus    gps;

// ── Sensors ────────────────────────────────────────────
Adafruit_BMP280  bmp;
Adafruit_MPU6050 mpu;

// ── Shared state ──────────────────────────────────────
float g_alt = 0, g_pres = 0, g_temp = 0;
float g_ax = 0,  g_ay = 0,   g_az = 0;
float g_gx = 0,  g_gy = 0,   g_gz = 0;
float g_lat = 0, g_lng = 0,  g_spd = 0;
int   g_sats = 0;
uint32_t g_seq = 0;

// ── Timers ────────────────────────────────────────────
#define SEND_INTERVAL_MS    500
#define DISPLAY_INTERVAL_MS 250
unsigned long lastSend    = 0;
unsigned long lastDisplay = 0;

void updateDisplay() {
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0,  "Pioneer Rocket TX");
    display.drawString(0, 11, "------------------");
    display.drawString(0, 22, "Alt: " + String(g_alt,  1) + " m");
    display.drawString(0, 33, "Tmp: " + String(g_temp, 1) + " C");
    display.drawString(0, 44, "GPS: " + (g_sats > 0 ? String(g_sats) + " sats" : String("No fix")));
    display.drawString(0, 55, "Ax: "  + String(g_ax,   2));
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

    // Sensor I2C bus
    Wire.begin(SDA_PIN, SCL_PIN);

    // OLED (Wire1, handled by ThingPulse internally)
    if (!display.init()) {
        Serial.println("OLED init failed");
    } else {
        Serial.println("OLED OK");
    }
    display.setContrast(255);
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0,  "Pioneer Rocket");
    display.drawString(0, 12, "Booting...");
    display.display();

    gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    // GPS init reconfigures GPIO36 (UART idle=HIGH), killing Vext.
    // Restore Vext immediately — GPS RX on GPIO36 is sacrificed until rewired.
    pinMode(36, OUTPUT);
    digitalWrite(36, LOW);

    if (!bmp.begin(0x76) && !bmp.begin(0x77)) {
        Serial.println("BMP280 not found!");
        display.clear();
        display.drawString(0, 0, "BMP280 FAILED!");
        display.display();
        while (1);
    }
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                    Adafruit_BMP280::SAMPLING_X2,
                    Adafruit_BMP280::SAMPLING_X16,
                    Adafruit_BMP280::FILTER_X16,
                    Adafruit_BMP280::STANDBY_MS_500);

    if (!mpu.begin()) {
        Serial.println("MPU6050 not found!");
        display.clear();
        display.drawString(0, 0, "MPU6050 FAILED!");
        display.display();
        while (1);
    }
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    int state = radio.begin(915.0);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("LoRa failed, code %d\n", state);
        display.clear();
        display.drawString(0, 0, "LoRa FAILED!");
        display.display();
        while (1);
    }

    Serial.println("All systems ready!");
    lastDisplay = millis() + 500;  // show data immediately on first loop
}

void loop() {
    while (gpsSerial.available())
        gps.encode(gpsSerial.read());

    if (millis() - lastSend >= SEND_INTERVAL_MS) {
        lastSend = millis();

        g_alt  = bmp.readAltitude(1013.25);
        g_pres = bmp.readPressure();
        g_temp = bmp.readTemperature();

        sensors_event_t accel, gyro, temp;
        mpu.getEvent(&accel, &gyro, &temp);
        g_ax = accel.acceleration.x; g_ay = accel.acceleration.y; g_az = accel.acceleration.z;
        g_gx = gyro.gyro.x;          g_gy = gyro.gyro.y;          g_gz = gyro.gyro.z;

        g_lat  = gps.location.isValid()   ? gps.location.lat()      : 0.0;
        g_lng  = gps.location.isValid()   ? gps.location.lng()      : 0.0;
        g_spd  = gps.speed.isValid()      ? gps.speed.kmph()        : 0.0;
        g_sats = gps.satellites.isValid() ? gps.satellites.value()  : 0;

        g_seq++;
        String json = "{";
        json += "\"seq\":"   + String(g_seq);
        json += ",\"alt\":"  + String(g_alt,  2);
        json += ",\"pres\":" + String(g_pres, 1);
        json += ",\"temp\":" + String(g_temp, 2);
        json += ",\"ax\":"   + String(g_ax,   3);
        json += ",\"ay\":"   + String(g_ay,   3);
        json += ",\"az\":"   + String(g_az,   3);
        json += ",\"gx\":"   + String(g_gx,   3);
        json += ",\"gy\":"   + String(g_gy,   3);
        json += ",\"gz\":"   + String(g_gz,   3);
        json += ",\"lat\":"  + String(g_lat,  6);
        json += ",\"lng\":"  + String(g_lng,  6);
        json += ",\"spd\":"  + String(g_spd,  2);
        json += ",\"sats\":" + String(g_sats);
        json += "}";

        Serial.println(json);
        radio.transmit(json);
        lastDisplay = millis();  // reset display timer after blocking transmit
    }

    if (millis() - lastDisplay >= DISPLAY_INTERVAL_MS) {
        lastDisplay = millis();
        updateDisplay();
    }
}
