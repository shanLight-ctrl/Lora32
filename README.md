# Lora32

LoRa telemetry system for the Pioneer Rocket using two Heltec WiFi LoRa 32 V3 boards.

## Repository structure

```
Lora32/
├── rocket-firmware/     ← Flash to the ROCKET board (Heltec V3)
├── ground-firmware/     ← Flash to the GROUND board (Heltec V3)
└── dashboard/           ← Python dashboard — run on your PC
```

---

## Hardware

| Board | Role |
|-------|------|
| Heltec WiFi LoRa 32 V3 | Rocket OBC — reads sensors, transmits via LoRa |
| Heltec WiFi LoRa 32 V3 | Ground station — receives LoRa, shows data on OLED |

Sensors wired to the **rocket board** (I2C: SDA=41, SCL=42):
- BMP280 — altitude / pressure / temperature (address 0x76)
- MPU6050 — accelerometer / gyroscope (address 0x68)
- GPS module — UART1 TX=37 at 9600 baud

---

## Requirements

### Firmware (both boards)
- [VS Code](https://code.visualstudio.com/) + [PlatformIO extension](https://platformio.org/install/ide?install=vscode)

### Dashboard
- Python 3.8+
- Install once:
  ```
  pip install pyserial rich
  ```

---

## Step 1 — Flash the rocket board

1. Plug in the rocket Heltec board via USB
2. Open `rocket-firmware/` as a PlatformIO project in VS Code
3. Check `platformio.ini` — set `upload_port` to your COM port
4. Click **Upload** or run:
   ```
   pio run --target upload
   ```
5. OLED shows **"Pioneer Rocket TX"** with live altitude, temp, IMU, and GPS

---

## Step 2 — Flash the ground board

1. Plug in the ground Heltec board via USB
2. Open `ground-firmware/` as a PlatformIO project in VS Code
3. Check `platformio.ini` — set `upload_port` to your COM port
4. Click **Upload** or run:
   ```
   pio run --target upload
   ```
5. OLED shows **"Pioneer Ground RX"** → **"Waiting for signal"** until rocket is on

---

## Step 3 — Run the dashboard

1. Keep the ground board plugged in
2. Open `dashboard/dashboard.py` and set `PORT` to your ground board's COM port
3. Open a terminal in `dashboard/` and run:
   ```
   python dashboard.py
   ```
4. Live terminal dashboard shows three panels:
   - **Environment** — altitude (m), pressure (Pa), temperature (°C)
   - **IMU** — accelerometer X/Y/Z (m/s²) and gyroscope X/Y/Z (rad/s)
   - **GPS** — latitude, longitude, speed, satellites
5. Press **Ctrl+C** to stop

---

## Data logging

Every run of `dashboard.py` automatically saves a timestamped file in the `dashboard/` folder.

### JSON file

Created as `telemetry_YYYYMMDD_HHMMSS.json` — a JSON array, one object per packet:

```json
[
  {
    "seq": 1,
    "alt": 186.54,
    "pres": 99104.4,
    "temp": 23.75,
    "ax": 1.980, "ay": 0.481, "az": 10.470,
    "gx": -0.085, "gy": -0.009, "gz": -0.007,
    "lat": 0.0, "lng": 0.0, "spd": 0.0, "sats": 0,
    "rssi": -45,
    "timestamp": "2026-05-14T14:30:22.123456"
  }
]
```

Load it in Python:
```python
import json
with open("telemetry_20260514_143022.json") as f:
    data = json.load(f)
print(data[0]["alt"])  # first packet altitude
```

### How to also save as CSV

Add these imports at the top of `dashboard.py`:
```python
import csv
```

Add this before `ser = serial.Serial(...)`:
```python
csv_path = json_path.replace(".json", ".csv")
csv_file = open(csv_path, "w", newline="")
writer = csv.DictWriter(csv_file, fieldnames=["timestamp","seq","alt","pres","temp","ax","ay","az","gx","gy","gz","lat","lng","spd","sats","rssi"])
writer.writeheader()
```

Inside the `if line.startswith("{"):` block, after `packets.append(data)`:
```python
writer.writerow({k: data.get(k, 0) for k in writer.fieldnames})
csv_file.flush()
```

Add before `ser.close()`:
```python
csv_file.close()
```

Open the CSV in Excel or pandas:
```python
import pandas as pd
df = pd.read_csv("telemetry_20260514_143022.csv")
print(df["alt"].max())  # peak altitude
```

---

## COM port troubleshooting

If you get **"Access Denied"** on a COM port:
- Close the PlatformIO serial monitor in VS Code
- Open Task Manager → end any `python.exe` or `pio.exe` process
- Try again

COM port numbers change on each USB reconnect — always check `platformio.ini` and `dashboard.py` if the upload fails.

---

## LoRa settings

Both boards must match:

| Setting | Value |
|---------|-------|
| Frequency | 915.0 MHz |
| Pins | CS=8, DIO1=14, RST=12, BUSY=13 |
| Library | RadioLib 7.x |

---

## Packet rate

~1 packet per 1.2 seconds. The rocket transmits every 500 ms but `radio.transmit()` blocks ~700 ms, making the effective rate ~0.83 packets/sec.
