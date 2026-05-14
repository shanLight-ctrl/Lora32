import serial
import json
import time
import os
from datetime import datetime
from rich.live import Live
from rich.table import Table
from rich.console import Console
from rich.panel import Panel
from rich.columns import Columns
from rich import box

PORT  = "COM7"
BAUD  = 115200

console = Console()

def make_dashboard(data):
    env_table = Table(box=box.ROUNDED, show_header=True, header_style="bold cyan")
    env_table.add_column("Sensor",   style="dim")
    env_table.add_column("Value",    justify="right")
    env_table.add_column("Unit",     style="dim")
    env_table.add_row("Altitude",    f"{data.get('alt',  0):.2f}",  "m")
    env_table.add_row("Pressure",    f"{data.get('pres', 0):.1f}",  "Pa")
    env_table.add_row("Temperature", f"{data.get('temp', 0):.2f}",  "°C")

    accel_table = Table(box=box.ROUNDED, show_header=True, header_style="bold yellow")
    accel_table.add_column("Axis",  style="dim")
    accel_table.add_column("Accel", justify="right")
    accel_table.add_column("Gyro",  justify="right")
    accel_table.add_row("X", f"{data.get('ax', 0):.3f}", f"{data.get('gx', 0):.3f}")
    accel_table.add_row("Y", f"{data.get('ay', 0):.3f}", f"{data.get('gy', 0):.3f}")
    accel_table.add_row("Z", f"{data.get('az', 0):.3f}", f"{data.get('gz', 0):.3f}")

    sats = data.get('sats', 0)
    gps_color = "green" if sats >= 4 else "red"
    gps_table = Table(box=box.ROUNDED, show_header=True, header_style="bold green")
    gps_table.add_column("GPS",     style="dim")
    gps_table.add_column("Value",   justify="right")
    gps_table.add_row("Latitude",   f"{data.get('lat', 0):.6f}")
    gps_table.add_row("Longitude",  f"{data.get('lng', 0):.6f}")
    gps_table.add_row("Speed",      f"{data.get('spd', 0):.2f} km/h")
    gps_table.add_row("Satellites", f"[{gps_color}]{sats}[/{gps_color}]")

    return Panel(
        Columns([
            Panel(env_table,   title="[cyan]Environment[/cyan]",              expand=True),
            Panel(accel_table, title="[yellow]IMU (m/s²  rad/s)[/yellow]",   expand=True),
            Panel(gps_table,   title="[green]GPS[/green]",                    expand=True),
        ]),
        title="[bold white] Pioneer Rocket — Live Telemetry [/bold white]",
        border_style="bright_blue"
    )

def main():
    data = {}
    log_dir = os.path.dirname(os.path.abspath(__file__))
    json_filename = datetime.now().strftime("telemetry_%Y%m%d_%H%M%S.json")
    json_path = os.path.join(log_dir, json_filename)
    packets = []

    console.print(f"[dim]Connecting to {PORT} at {BAUD} baud...[/dim]")
    try:
        ser = serial.Serial(PORT, BAUD, timeout=1)
    except Exception as e:
        console.print(f"[red]Error: {e}[/red]")
        return

    console.print(f"[green]Connected. Logging to {json_filename}[/green]\n")
    time.sleep(1)

    with Live(make_dashboard(data), refresh_per_second=4, screen=True) as live:
        while True:
            try:
                line = ser.readline().decode("utf-8", errors="ignore").strip()
                if line.startswith("{"):
                    data = json.loads(line)
                    data["timestamp"] = datetime.now().isoformat()
                    packets.append(data)
                    with open(json_path, "w") as f:
                        json.dump(packets, f, indent=2)
                    live.update(make_dashboard(data))
            except json.JSONDecodeError:
                pass
            except KeyboardInterrupt:
                break

    ser.close()
    console.print(f"[dim]Disconnected. {len(packets)} packets saved to {json_filename}[/dim]")

if __name__ == "__main__":
    main()
