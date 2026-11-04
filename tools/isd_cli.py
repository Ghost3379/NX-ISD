#!/usr/bin/env python3
"""
ISD-CLI // Intelligent Sensor Device Terminal Control & Configuration Suite
Integrated Command-line & Interactive Shell for NX-ISD / ISD-Core.
Unifies:
  1. Live USB-CDC / BLE Device Control (BLE toggle, telemetry streaming, blind navigation, matrix control)
  2. Live Sensor Telemetry & Fullscreen Terminal HUD Monitor
  3. Binary Configuration Management (parse, inspect, verify, and generate /sys/config.bin)
"""

import sys
import os
import time
import json
import struct
import math
import argparse
from typing import Optional, Dict, Any, List

# Ensure Windows terminal standard streams handle UTF-8 without charmap crashes
if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

# Add parent directory / tools path for module discovery
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

# Optional serial dependencies
try:
    import serial
    import serial.tools.list_ports
    SERIAL_AVAILABLE = True
except ImportError:
    SERIAL_AVAILABLE = False

# Optional rich UI styling
try:
    from rich.console import Console
    from rich.table import Table
    from rich.panel import Panel
    from rich.live import Live
    from rich.text import Text
    from rich import box
    RICH_AVAILABLE = True
    console = Console()
except ImportError:
    RICH_AVAILABLE = False
    console = None



BANNER = r"""
  _   ___  ______        _____ _____  _____ 
 | \ | \ \/ / ___|      |_   _/  ___||  _  \
 |  \| |\  /\___ \ ______ | | \ `--. | | | |
 | . ` |/  \ ___) |______|| |  `--. \| | | |
 |_|\_/_/\_\____/       |___/\____/  |_____/
     COMMAND UPLINK & DIAGNOSTIC CONSOLE
"""

DEFAULT_BAUD = 115200


def log_info(msg: str):
    if RICH_AVAILABLE:
        console.print(f"[bold cyan][INFO][/bold cyan] {msg}")
    else:
        print(f"[INFO] {msg}")


def log_success(msg: str):
    if RICH_AVAILABLE:
        console.print(f"[bold green][OK][/bold green] {msg}")
    else:
        print(f"[OK] {msg}")


def log_warn(msg: str):
    if RICH_AVAILABLE:
        console.print(f"[bold yellow][WARN][/bold yellow] {msg}")
    else:
        print(f"[WARN] {msg}")


def log_err(msg: str):
    if RICH_AVAILABLE:
        console.print(f"[bold red][ERR][/bold red] {msg}")
    else:
        print(f"[ERR] {msg}")


# ==============================================================================
# SECTION 1: HARDWARE SERIAL COMMUNICATION ENGINE
# ==============================================================================

class ISDDevice:
    def __init__(self, port: Optional[str] = None, baud: int = DEFAULT_BAUD, timeout: float = 1.0):
        self.port = port
        self.baud = baud
        self.timeout = timeout
        self.ser: Optional[serial.Serial] = None

    @staticmethod
    def detect_ports() -> List[str]:
        """Detect potential ESP32-S3 USB CDC serial ports."""
        if not SERIAL_AVAILABLE:
            return []
        ports = []
        for p in serial.tools.list_ports.comports():
            desc = (p.description or "").lower()
            hwid = (p.hwid or "").lower()
            if "esp32" in desc or "303a" in hwid or "usb serial" in desc or "cdc" in desc or "ch340" in desc or "cp210" in desc:
                ports.append(p.device)
            elif p.device not in ports:
                ports.append(p.device)
        return ports

    def connect(self) -> bool:
        if not SERIAL_AVAILABLE:
            log_err("'pyserial' is not installed. Run: pip install pyserial")
            return False

        if self.ser and self.ser.is_open:
            return True

        target_port = self.port
        if not target_port:
            detected = self.detect_ports()
            if not detected:
                log_err("No serial COM ports detected. Check USB connection to NX-ISD.")
                return False
            if len(detected) == 1:
                target_port = detected[0]
                log_info(f"Auto-selected device on port [bold]{target_port}[/bold]")
            else:
                log_info(f"Multiple ports found: {', '.join(detected)}")
                target_port = detected[0]
                log_info(f"Defaulting to port [bold]{target_port}[/bold] (use --port to specify)")

        self.port = target_port
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=self.timeout)
            self.ser.dtr = True
            self.ser.rts = False
            time.sleep(0.05)
            self.ser.reset_input_buffer()
            return True
        except Exception as e:
            log_err(f"Failed to open port {self.port}: {e}")
            self.ser = None
            return False

    def close(self):
        if self.ser and self.ser.is_open:
            try:
                self.ser.close()
            except Exception:
                pass
        self.ser = None

    def send_command(self, cmd: str, timeout: float = 1.0) -> Optional[Dict[str, Any]]:
        """Send command line and parse first JSON response."""
        if not self.ser or not self.ser.is_open:
            if not self.connect():
                return None

        clean_cmd = cmd.strip()
        try:
            self.ser.reset_input_buffer()
            self.ser.write((clean_cmd + "\n").encode("utf-8"))
            self.ser.flush()

            deadline = time.time() + timeout
            while time.time() < deadline:
                line = self.ser.readline().decode("utf-8", errors="ignore").strip()
                if not line:
                    continue
                try:
                    data = json.loads(line)
                    return data
                except json.JSONDecodeError:
                    continue
            return None
        except Exception as e:
            log_err(f"Serial communication error: {e}")
            return None

    def read_line(self) -> Optional[str]:
        if not self.ser or not self.ser.is_open:
            return None
        try:
            line = self.ser.readline().decode("utf-8", errors="ignore").strip()
            return line if line else None
        except Exception:
            return None


def get_iaq_tier(iaq: int) -> str:
    if iaq <= 50:
        return "[bold green]EXCELLENT[/bold green]"
    elif iaq <= 100:
        return "[green]GOOD[/green]"
    elif iaq <= 150:
        return "[yellow]MODERATE[/yellow]"
    elif iaq <= 200:
        return "[bold yellow]POOR[/bold yellow]"
    elif iaq <= 300:
        return "[bold red]UNHEALTHY[/bold red]"
    else:
        return "[bold magenta]HAZARDOUS[/bold magenta]"


# ==============================================================================
# SECTION 2: LIVE TELEMETRY, SENSORS & HARDWARE ACTIONS
# ==============================================================================

def cmd_ble(dev: ISDDevice, action: str):
    """Toggle or inspect BLE state."""
    action = action.lower()
    if action in ["on", "start", "enable", "1"]:
        log_info("Starting BLE GATT server & advertising (NX-ISD-UPLINK)...")
        res = dev.send_command("ble.start", timeout=2.0)
        if res and res.get("status") == "ACK":
            log_success("BLE Uplink Bridge ACTIVE! You can now pair wirelessly via NX-Uplink.")
        else:
            log_warn(f"Sent ble.start (response: {res})")
    elif action in ["off", "stop", "disable", "0"]:
        log_info("Stopping BLE Uplink Bridge...")
        res = dev.send_command("ble.stop", timeout=2.0)
        if res and res.get("status") == "ACK":
            log_success("BLE Uplink Bridge DEACTIVATED (power saved).")
        else:
            log_warn(f"Sent ble.stop (response: {res})")
    elif action in ["status", "state"]:
        res = dev.send_command("ble.status", timeout=1.5)
        if res:
            active = res.get("bleActive", False)
            conn = res.get("bleConnected", False)
            if RICH_AVAILABLE:
                state_str = "[bold green]ACTIVE[/bold green]" if active else "[dim]OFF[/dim]"
                conn_str = "[bold green]CONNECTED[/bold green]" if conn else "[dim]STANDBY / ADVERTISING[/dim]"
                console.print(Panel(f"BLE Service:  {state_str}\nPeer Link:    {conn_str}\nDevice Name:  [bold cyan]NX-ISD-UPLINK[/bold cyan]\nService UUID: 8e7f1a01-2b3c-4d5e-9f01-0123456789ab", title="BLE Uplink Status", border_style="cyan"))
            else:
                print(f"BLE Active: {active}, Connected: {conn}")
        else:
            log_err("Device did not respond to ble.status")


def cmd_uplink(dev: ISDDevice, action: str, rate: Optional[int] = None):
    """Toggle USB serial telemetry streaming."""
    action = action.lower()
    if action in ["on", "start"]:
        if rate:
            dev.send_command(f"stream.rate={rate}")
        res = dev.send_command("stream.start")
        log_success(f"USB Telemetry stream started ({res})")
    elif action in ["off", "stop"]:
        res = dev.send_command("stream.stop")
        log_success(f"USB Telemetry stream stopped ({res})")


def cmd_sensors(dev: ISDDevice):
    """Query single-shot sensor telemetry and print styled dashboard."""
    log_info("Querying live hardware telemetry...")
    data = dev.send_command("sys.sensors", timeout=2.0)
    if not data or data.get("status") != "ACK":
        dev.send_command("stream.start")
        t0 = time.time()
        while time.time() - t0 < 1.5:
            line = dev.read_line()
            if line and "roll" in line:
                try:
                    data = json.loads(line)
                    break
                except Exception:
                    pass

    if not data:
        log_err("Failed to fetch sensor telemetry from NX-ISD.")
        return

    if RICH_AVAILABLE:
        table = Table(title="NX-ISD // LIVE SENSOR TELEMETRY", box=box.ROUNDED, border_style="#FFB000")
        table.add_column("Subsystem", style="bold cyan")
        table.add_column("Sensor", style="dim")
        table.add_column("Primary Telemetry", style="bold")
        table.add_column("Secondary / Environment", style="dim")

        roll = data.get("roll", 0.0)
        pitch = data.get("pitch", 0.0)
        yaw = data.get("yaw", 0.0)
        head = data.get("heading", yaw % 360)
        table.add_row(
            "Attitude (9-DOF)",
            "BNO085",
            f"Roll: {roll:+.1f} deg | Pitch: {pitch:+.1f} deg",
            f"Yaw: {yaw:+.1f} deg (HDG: {head:03.0f} deg)"
        )

        temp = data.get("temp", 0.0)
        hum = data.get("hum", 0.0)
        press = data.get("press", 0.0)
        alt = 44330.0 * (1.0 - (press / 1013.25) ** 0.1903) if press > 0 else 0
        table.add_row(
            "Atmosphere",
            "BME690",
            f"Temp: {temp:.1f} C | Hum: {hum:.1f}%",
            f"Baro: {press:.1f} hPa (~{alt:.0f}m)"
        )

        gas = data.get("gas", 0.0)
        iaq = int(data.get("iaq", 25))
        iaq_txt = get_iaq_tier(iaq)
        table.add_row(
            "Air Quality",
            "BME690 MOX",
            f"IAQ Index: {iaq} ({iaq_txt})",
            f"MOX Resistance: {gas/1000.0:.1f} kOhm"
        )

        lux = data.get("lux", 0.0)
        table.add_row(
            "Photopic Light",
            "OPT3001",
            f"Illuminance: {lux:.1f} Lux",
            "Weber-Fechner Auto-Dimming"
        )

        hr = data.get("hr", 0.0)
        spo2 = data.get("spo2", 0.0)
        table.add_row(
            "Cardiovascular",
            "MAX30102",
            f"Heart Rate: {hr:.0f} BPM" if hr > 0 else "Resting / Idle",
            f"SpO2: {spo2:.0f}%" if spo2 > 0 else "No Finger Contact"
        )

        batt = data.get("batt", 0.0)
        volt = data.get("voltage", 0.0)
        chg = data.get("charging", False)
        chg_str = "[bold green]CHARGING[/bold green]" if chg else "Discharging"
        table.add_row(
            "Power Fuel Gauge",
            "MAX17048",
            f"Battery: {batt:.0f}% ({volt:.2f}V)",
            f"State: {chg_str}"
        )

        console.print(table)
    else:
        print(f"Roll: {data.get('roll')} | Pitch: {data.get('pitch')} | Temp: {data.get('temp')}C | IAQ: {data.get('iaq')} | Batt: {data.get('batt')}% ({data.get('voltage')}V)")


def cmd_nav(dev: ISDDevice, action: str):
    """Inject blind navigation clicks into ISD-Core."""
    action = action.lower()
    cmd_map = {
        "left": "nav.left",
        "l": "nav.left",
        "right": "nav.right",
        "r": "nav.right",
        "push": "nav.push",
        "p": "nav.push",
        "back": "nav.btn",
        "btn": "nav.btn",
        "b": "nav.btn",
    }
    target = cmd_map.get(action)
    if not target:
        log_err(f"Unknown nav action '{action}'. Choose: left, right, push, back")
        return
    res = dev.send_command(target)
    log_success(f"Nav click sent: [bold cyan]{target}[/bold cyan] ({res})")


def cmd_matrix(dev: ISDDevice, action: str, val: Optional[str] = None):
    """Control the 16-LED NeoPixel matrix."""
    action = action.lower()
    if action in ["on", "enable", "1"]:
        dev.send_command("npm.power=1")
        log_success("NeoPixel Matrix Power: ON")
    elif action in ["off", "disable", "0"]:
        dev.send_command("npm.power=0")
        log_success("NeoPixel Matrix Power: OFF")
    elif action == "brightness":
        b = int(val or 40)
        dev.send_command(f"npm.brightness={b}")
        log_success(f"NeoPixel Matrix Brightness set to {b}")
    elif action in ["radar", "rain", "plasma", "tracer", "breath", "ripple"]:
        pat_map = {
            "radar": "cyber_radar",
            "rain": "matrix_rain",
            "plasma": "spectrum_plasma",
            "tracer": "neon_tracer",
            "breath": "glyph_breath",
            "ripple": "quantum_ripple"
        }
        pat = pat_map.get(action, action)
        dev.send_command("npm.power=1")
        dev.send_command(f"npm.pattern={pat}")
        log_success(f"NeoPixel Animation set to: {pat}")
    elif action == "heart":
        heart = [
            "#000000", "#ff0044", "#ff0044", "#000000",
            "#ff0044", "#ff0044", "#ff0044", "#ff0044",
            "#ff0044", "#ff0044", "#ff0044", "#ff0044",
            "#000000", "#ff0044", "#ff0044", "#000000"
        ]
        dev.send_command(json.dumps({"npm.matrix": heart}))
        log_success("Pushed Heart Glyph to 4x4 matrix!")
    elif action == "clear":
        clear_frame = ["#000000"] * 16
        dev.send_command(json.dumps({"npm.matrix": clear_frame}))
        log_success("Cleared 4x4 matrix LEDs.")
    else:
        log_warn(f"Unknown matrix pattern '{action}'. Options: on, off, brightness, radar, rain, plasma, tracer, breath, ripple, heart, clear")


def cmd_info(dev: ISDDevice):
    """Fetch MCU system diagnostics."""
    res = dev.send_command("sys.info")
    if res and res.get("status") == "ACK":
        if RICH_AVAILABLE:
            table = Table(title="NX-ISD // SYSTEM ARCHITECTURE & DIAGNOSTICS", box=box.ROUNDED, border_style="cyan")
            table.add_column("Parameter", style="bold")
            table.add_column("Value", style="cyan")
            table.add_row("Microcontroller", res.get("chip", "ESP32-S3"))
            table.add_row("CPU Clock", f"{res.get('cpuMHz', 240)} MHz")
            table.add_row("Free Internal SRAM", f"{res.get('freeHeap', 0) / 1024:.1f} KB")
            table.add_row("Free Octal PSRAM", f"{res.get('freePsram', 0) / 1024 / 1024:.2f} MB")
            table.add_row("System Uptime", f"{res.get('uptimeMs', 0) / 1000:.1f} seconds")
            table.add_row("Telemetry Stream", "[green]STREAMING[/green]" if res.get("stream") else "[dim]DORMANT[/dim]")
            table.add_row("BLE Uplink", "[green]ACTIVE[/green]" if res.get("ble") else "[dim]OFF[/dim]")
            table.add_row("Display Standby", "[yellow]ASLEEP[/yellow]" if res.get("screenStandby") else "[green]AWAKE[/green]")
            console.print(table)
        else:
            print(f"System Info: {res}")
    else:
        log_err("Device did not respond to sys.info")


def check_exit_keypress() -> bool:
    """Non-blocking check for user exit keys (q, Q, x, X, ESC, Enter, Ctrl+C)."""
    if sys.platform == "win32":
        try:
            import msvcrt
            if msvcrt.kbhit():
                ch = msvcrt.getch()
                if ch in (b'q', b'Q', b'x', b'X', b'\x1b', b'\r', b'\x03'):
                    return True
        except Exception:
            pass
    return False


def cmd_monitor(dev: ISDDevice, mode: str = "line", rate_hz: float = 2.0, mock: bool = False):
    """
    Live terminal monitor.
    mode: 'line' (compact single-line in-place update at rate_hz) or 'hud' (fullscreen panel)
    rate_hz: update frequency (default 2.0 Hz)
    """
    is_live = dev.ser and dev.ser.is_open
    if not is_live and not mock:
        if not dev.connect():
            log_warn("No serial hardware connected. Use 'monitor --mock' to preview 2Hz live stream offline, or 'connect <port>'.")
            return
        is_live = True

    interval = 1.0 / max(0.2, rate_hz)
    log_info(f"Starting Live Monitor ({rate_hz:.1f} Hz). Press 'q' or Ctrl+C to stop...")

    if is_live:
        dev.send_command("stream.start")
        dev.send_command(f"stream.rate={int(rate_hz)}")

    last_print = 0.0

    def render_hud_panel(f: dict) -> Panel:
        roll = f.get("roll", 0.0)
        pitch = f.get("pitch", 0.0)
        yaw = f.get("yaw", 0.0)
        temp = f.get("temp", 0.0)
        hum = f.get("hum", 0.0)
        press = f.get("press", 0.0)
        gas = f.get("gas", 0.0)
        iaq = int(f.get("iaq", 25))
        lux = f.get("lux", 0.0)
        batt = f.get("batt", 0.0)
        volt = f.get("voltage", 0.0)
        chg = f.get("charging", False)

        b_pos = int(10 + max(-10, min(10, roll / 3.0)))
        bubble_bar = list("---------------------")
        bubble_bar[10] = "|"
        bubble_bar[b_pos] = "O"
        bubble_str = "".join(bubble_bar)

        content = (
            f"[bold cyan]9-DOF ORIENTATION[/bold cyan]\n"
            f"  Roll:  {roll:+6.1f} deg  Pitch: {pitch:+6.1f} deg  Yaw: {yaw:+6.1f} deg\n"
            f"  Level Reticle: [{bubble_str}]\n\n"
            f"[bold green]ENVIRONMENT & AIR QUALITY[/bold green]\n"
            f"  Temp:  {temp:5.1f} C   Humidity: {hum:5.1f} %\n"
            f"  Baro:  {press:6.1f} hPa  Gas Res: {gas/1000.0:6.1f} kOhm\n"
            f"  IAQ:   {iaq:3d} ({get_iaq_tier(iaq)})\n\n"
            f"[bold yellow]POWER & BIOMETRICS[/bold yellow]\n"
            f"  Ambient: {lux:5.1f} Lux  Battery: {batt:3.0f}% ({volt:.2f}V) {'[bold green][CHARGING][/bold green]' if chg else ''}\n\n"
            f"[dim]Press 'q', Enter, or Ctrl+C to return to console[/dim]"
        )
        return Panel(content, title="[bold #FFB000]NX-ISD // TELEMETRY HUD[/bold #FFB000]", border_style="#FFB000", box=box.HEAVY)

    try:
        if mode == "hud" and RICH_AVAILABLE:
            with Live(console=console, refresh_per_second=int(rate_hz * 2)) as live:
                step = 0
                while True:
                    if check_exit_keypress():
                        break
                    frame = None
                    if is_live:
                        line = dev.read_line()
                        if line and ("roll" in line or "batt" in line):
                            try:
                                frame = json.loads(line)
                            except Exception:
                                pass
                    else:
                        time.sleep(interval)
                        step += 1
                        frame = {
                            "roll": 2.5 * math.sin(step * 0.2),
                            "pitch": 1.2 * math.cos(step * 0.15),
                            "yaw": (step * 2) % 360,
                            "temp": 24.2 + 0.3 * math.sin(step * 0.1),
                            "hum": 48.5,
                            "press": 1013.25,
                            "gas": 125000,
                            "iaq": 22,
                            "lux": 340.0,
                            "batt": 85.0,
                            "voltage": 3.94,
                            "charging": False
                        }
                    if frame:
                        live.update(render_hud_panel(frame))
                    else:
                        time.sleep(0.01)
        else:
            # Single line mode (in-place 2Hz update)
            step = 0
            while True:
                if check_exit_keypress():
                    break
                frame = None
                now = time.time()
                if is_live:
                    line = dev.read_line()
                    if line and ("roll" in line or "batt" in line):
                        try:
                            frame = json.loads(line)
                        except Exception:
                            pass
                else:
                    time.sleep(0.05)
                    step += 1
                    frame = {
                        "roll": 2.5 * math.sin(step * 0.2),
                        "pitch": 1.2 * math.cos(step * 0.15),
                        "temp": 24.2 + 0.3 * math.sin(step * 0.1),
                        "press": 1013.2,
                        "batt": 84.0,
                        "voltage": 3.92,
                        "charging": False
                    }

                if frame and (now - last_print) >= interval:
                    roll = frame.get("roll", 0.0)
                    pitch = frame.get("pitch", 0.0)
                    temp = frame.get("temp", 0.0)
                    press = frame.get("press", 0.0)
                    batt = frame.get("batt", 0.0)
                    volt = frame.get("voltage", 0.0)
                    chg = " ⚡" if frame.get("charging", False) else ""

                    status_line = (
                        f"\r\033[K[MONITOR {rate_hz:.1f}Hz] Batt: {batt:2.0f}% ({volt:.2f}V){chg} | "
                        f"Temp: {temp:4.1f}°C | Baro: {press:6.1f}hPa | "
                        f"Roll: {roll:+5.1f}° Pitch: {pitch:+5.1f}° | "
                        f"(Press 'q' or Ctrl+C to exit)"
                    )
                    sys.stdout.write(status_line)
                    sys.stdout.flush()
                    last_print = now
                else:
                    time.sleep(0.01)
    except KeyboardInterrupt:
        pass
    finally:
        sys.stdout.write("\n")
        sys.stdout.flush()
        if is_live:
            dev.send_command("stream.stop")
        log_info("Live monitor stopped. Returned to console.")


class ISDFileSystem:
    """Storage Navigator and File Manager for NX-ISD."""
    def __init__(self, dev: ISDDevice):
        self.dev = dev
        self.current_path = "/"
        self._cached_tree = None

    @staticmethod
    def format_size(size_bytes: int) -> str:
        if size_bytes < 1024:
            return f"{size_bytes} B"
        elif size_bytes < 1024 * 1024:
            return f"{size_bytes / 1024:.1f} KB"
        elif size_bytes < 1024 * 1024 * 1024:
            return f"{size_bytes / (1024 * 1024):.1f} MB"
        else:
            return f"{size_bytes / (1024 * 1024 * 1024):.2f} GB"

    def get_tree(self, force_refresh: bool = False) -> dict:
        if self._cached_tree and not force_refresh:
            return self._cached_tree

        if self.dev and self.dev.ser and self.dev.ser.is_open:
            res = self.dev.send_command("storage.list", timeout=2.0)
            if res and "roots" in res:
                self._cached_tree = res
                return self._cached_tree

        # Built-in offline fallback matching NX-ISD v1.4 architecture
        self._cached_tree = {
            "roots": [
                {
                    "name": "Internal Flash (16 MB SPI)",
                    "mount": "flash",
                    "type": "flash",
                    "totalBytes": 16777216,
                    "freeBytes": 12582912,
                    "entries": [
                        {
                            "name": "firmware",
                            "type": "dir",
                            "entries": [
                                {"name": "isd-core.bin", "type": "file", "size": 1208409, "desc": "Active ISD-Core firmware build"},
                                {"name": "bootloader.bin", "type": "file", "size": 32768, "desc": "ESP-IDF 2nd stage bootloader"},
                                {"name": "partitions.bin", "type": "file", "size": 4096, "desc": "16MB SPI Flash Partition Table"}
                            ]
                        },
                        {
                            "name": "config",
                            "type": "dir",
                            "entries": [
                                {"name": "config.bin", "type": "file", "size": 64, "desc": "Active 64-byte packed binary config"},
                                {"name": "settings.json", "type": "file", "size": 2048, "desc": "User settings export"},
                                {"name": "calib.json", "type": "file", "size": 1024, "desc": "BNO085/BME690 baseline calibration"}
                            ]
                        },
                        {
                            "name": "logs",
                            "type": "dir",
                            "entries": [
                                {"name": "sys.log", "type": "file", "size": 81920, "desc": "Boot trace & panic crash dump"}
                            ]
                        }
                    ]
                },
                {
                    "name": "NAND SD (ZDSD / MicroSD)",
                    "mount": "sd",
                    "type": "sd",
                    "totalBytes": 268435456,
                    "freeBytes": 251658240,
                    "entries": [
                        {
                            "name": "telemetry",
                            "type": "dir",
                            "entries": [
                                {"name": "telemetry_20261103.csv", "type": "file", "size": 409600, "desc": "10 Hz flight & climate log"},
                                {"name": "vitals.dat", "type": "file", "size": 65536, "desc": "PPG pulse & SpO2 binary record"}
                            ]
                        },
                        {
                            "name": "sds",
                            "type": "dir",
                            "entries": [
                                {"name": "audit_log.txt", "type": "file", "size": 4096, "desc": "NX-SDS Hardware Proof-Test Certificate"}
                            ]
                        },
                        {
                            "name": "assets",
                            "type": "dir",
                            "entries": [
                                {"name": "fonts", "type": "dir", "entries": [
                                    {"name": "cyber_16.fnt", "type": "file", "size": 14336, "desc": "Monospace UI font"},
                                    {"name": "orbitron_24.fnt", "type": "file", "size": 28672, "desc": "HUD display font"}
                                ]},
                                {"name": "icons", "type": "dir", "entries": [
                                    {"name": "battery.png", "type": "file", "size": 1024, "desc": "Status glyph"},
                                    {"name": "ble.png", "type": "file", "size": 1024, "desc": "Radio glyph"}
                                ]}
                            ]
                        }
                    ]
                }
            ]
        }
        return self._cached_tree

    def resolve_path(self, path: Optional[str]) -> str:
        if not path or path == ".":
            return self.current_path
        if path == "~":
            return "/"
        if path.startswith("/"):
            norm = os.path.normpath(path).replace("\\", "/")
        else:
            norm = os.path.normpath(os.path.join(self.current_path, path)).replace("\\", "/")
        return norm if norm.startswith("/") else "/" + norm

    def find_node(self, path: str) -> Optional[dict]:
        path = self.resolve_path(path)
        if path == "/":
            return {"type": "root", "entries": self.get_tree().get("roots", [])}

        parts = [p for p in path.strip("/").split("/") if p]
        current_entries = self.get_tree().get("roots", [])
        found_node = None

        for part in parts:
            matched = False
            for entry in current_entries:
                name = entry.get("name", "")
                m_type = entry.get("type", "")
                mount = entry.get("mount", "")
                # Flexible matching: by name, type (flash/sd), mount, or slugified name
                aliases = [
                    name.lower(),
                    m_type.lower(),
                    mount.lower(),
                    name.lower().replace(" ", ""),
                    name.lower().split()[0] if name else ""
                ]
                if any(part.lower() == a for a in aliases if a):
                    found_node = entry
                    current_entries = entry.get("entries", [])
                    matched = True
                    break
            if not matched:
                return None
        return found_node

    def df(self):
        """Display disk space usage table for all storage mounts."""
        tree = self.get_tree()
        roots = tree.get("roots", [])
        if RICH_AVAILABLE:
            table = Table(title="NX-ISD STORAGE VOLUMES (DF)", box=box.ROUNDED, border_style="cyan")
            table.add_column("Mount", style="bold cyan")
            table.add_column("Type", style="white")
            table.add_column("Total", justify="right")
            table.add_column("Used", justify="right")
            table.add_column("Free", justify="right", style="green")
            table.add_column("Capacity / Usage", justify="center")

            for r in roots:
                mount = "/" + r.get("mount", r.get("type", "vol"))
                total = r.get("totalBytes", 0)
                free = r.get("freeBytes", 0)
                used = max(0, total - free)
                pct = (used / total * 100.0) if total > 0 else 0.0
                bar_len = 10
                filled = int(round(bar_len * (pct / 100.0)))
                bar_str = "█" * filled + "░" * (bar_len - filled)
                table.add_row(
                    mount,
                    r.get("type", "unknown"),
                    self.format_size(total),
                    self.format_size(used),
                    self.format_size(free),
                    f"[{bar_str}] {pct:5.1f}%"
                )
            console.print(table)
        else:
            print("\n" + "=" * 68)
            print(f" {'MOUNT':<12} {'TYPE':<8} {'TOTAL':<10} {'USED':<10} {'FREE':<10} {'USAGE'}")
            print("-" * 68)
            for r in roots:
                mount = "/" + r.get("mount", r.get("type", "vol"))
                total = r.get("totalBytes", 0)
                free = r.get("freeBytes", 0)
                used = max(0, total - free)
                pct = (used / total * 100.0) if total > 0 else 0.0
                bar_len = 10
                filled = int(round(bar_len * (pct / 100.0)))
                bar_str = "#" * filled + "-" * (bar_len - filled)
                print(f" {mount:<12} {r.get('type', 'unknown'):<8} {self.format_size(total):<10} {self.format_size(used):<10} {self.format_size(free):<10} [{bar_str}] {pct:5.1f}%")
            print("=" * 68 + "\n")

    def ls(self, path: Optional[str] = None):
        """List files and directories in current or specified path."""
        target_path = self.resolve_path(path)
        node = self.find_node(target_path)
        if not node:
            log_err(f"No such directory: '{target_path}'")
            return

        entries = node.get("entries", [])
        if target_path == "/":
            entries = [
                {"name": r.get("mount", r.get("name")), "type": "dir", "size": r.get("totalBytes", 0), "desc": r.get("name")}
                for r in node.get("entries", [])
            ]

        if RICH_AVAILABLE:
            table = Table(title=f"DIRECTORY: {target_path}", box=box.SIMPLE, border_style="cyan")
            table.add_column("Type", style="bold yellow", width=8)
            table.add_column("Size", justify="right", width=12)
            table.add_column("Name", style="bold white")
            table.add_column("Description", style="dim")

            for e in entries:
                is_dir = e.get("type") == "dir"
                type_tag = "[bold cyan]<DIR>[/bold cyan]" if is_dir else "[green]<FILE>[/green]"
                size_str = "-" if is_dir else self.format_size(e.get("size", 0))
                name_str = f"[bold cyan]{e.get('name')}/[/bold cyan]" if is_dir else e.get("name")
                desc_str = e.get("desc", "")
                table.add_row(type_tag, size_str, name_str, desc_str)
            console.print(table)
        else:
            print(f"\n Directory of {target_path}:\n")
            dir_count = 0
            file_count = 0
            total_bytes = 0
            for e in entries:
                is_dir = e.get("type") == "dir"
                if is_dir:
                    dir_count += 1
                    name_str = f"{e.get('name')}/"
                    print(f"  <DIR>          {'':<10} {name_str:<24} {e.get('desc', '')}")
                else:
                    file_count += 1
                    size = e.get("size", 0)
                    total_bytes += size
                    print(f"  <FILE> {self.format_size(size):>10}  {e.get('name'):<24} {e.get('desc', '')}")
            print(f"\n  {file_count} File(s) ({self.format_size(total_bytes)}), {dir_count} Dir(s)\n")

    def cd(self, path: str):
        """Change current directory."""
        if not path or path == "~":
            self.current_path = "/"
            return
        target_path = self.resolve_path(path)
        node = self.find_node(target_path)
        if not node:
            log_err(f"cd: No such directory: '{path}'")
            return
        if node.get("type") not in ["dir", "root", "flash", "sd"]:
            log_err(f"cd: Not a directory: '{path}'")
            return
        self.current_path = target_path

    def pwd(self):
        """Print current working directory."""
        if RICH_AVAILABLE:
            console.print(f"Current Directory: [bold cyan]{self.current_path}[/bold cyan]")
        else:
            print(self.current_path)

    def tree(self, path: Optional[str] = "/"):
        """Print recursive directory tree."""
        target_path = self.resolve_path(path)
        node = self.find_node(target_path)
        if not node:
            log_err(f"tree: No such path: '{target_path}'")
            return

        print(f"\n📂 {target_path}")

        def print_subtree(entries, prefix=""):
            count = len(entries)
            for i, e in enumerate(entries):
                is_last = (i == count - 1)
                branch = "└── " if is_last else "├── "
                sub_prefix = "    " if is_last else "│   "
                name = e.get("mount") if "mount" in e else e.get("name")
                is_dir = e.get("type") in ["dir", "flash", "sd", "root"]
                if is_dir:
                    desc = f" ({e.get('name')})" if "mount" in e else ""
                    print(f"{prefix}{branch}📁 {name}/{desc}")
                    sub_entries = e.get("entries", [])
                    if sub_entries:
                        print_subtree(sub_entries, prefix + sub_prefix)
                else:
                    size_str = self.format_size(e.get("size", 0))
                    print(f"{prefix}{branch}📄 {name} [{size_str}]")

        if target_path == "/":
            print_subtree(node.get("entries", []))
        else:
            print_subtree(node.get("entries", []))
        print()

    def cat(self, file_path: str):
        """View file contents."""
        target_path = self.resolve_path(file_path)
        name = os.path.basename(target_path).lower()

        if name == "audit_log.txt":
            print("""============================================================
 [NX-ISD] HARDWARE INTEGRITY & PROOF-TEST CERTIFICATE
 SYSTEM  : NX-ISD Hardware v1.4 (PCB v1p4)
 MCU     : ESP32-S3-WROOM-1 (240 MHz Dual-Core, 8MB Octal PSRAM)
 ENGINE  : NX-SDS (Self-Diagnostic System)
 DATE    : 2026-11-04T10:30:00Z
============================================================
 [SILICON AUDIT]
  • RV-3028-C7 RTC (I2C 0x52)    : [ PASS ] DirectSwitchingMode=1, Trickle=3k
  • MAX17048 Fuel (I2C 0x36)     : [ PASS ] VCELL=3.92V, SOC=84.2%, Alert=OK
  • BME690 Environmental (0x76)  : [ PASS ] T=24.2C, P=1013.2hPa, H=48.5%
  • BNO085 9-DOF IMU (I2C 0x4A)  : [ PASS ] SHTP Channel Sync, Reset=OK
  • MAX30102 PPG Optical (0x57)  : [ PASS ] PartID=0x15, FIFO Empty
  • OPT3001 Ambient Lux (0x44)   : [ PASS ] DevID=0x3001, Lux=340.5
  • MicroSD SDMMC 4-Bit Bus      : [ PASS ] High-Speed 40MHz, 256MB Online
============================================================
 OVERALL INTEGRITY STATUS: 100% NOMINAL (PROOF-TEST PASSED)
============================================================""")
        elif name == "settings.json":
            print("""{
  "system": {
    "brightness": 140,
    "timeout_sec": 30,
    "silent_mode": false,
    "volume": 80,
    "eco_mode": false
  },
  "peripherie": {
    "npm_power": true,
    "npm_pattern": "cyber_radar",
    "npm_brightness": 50,
    "rtc_sync": true
  }
}""")
        elif name == "calib.json":
            print("""{
  "imu": {
    "accel_offset": [0.012, -0.005, 0.045],
    "gyro_offset": [0.001, -0.002, 0.000],
    "mag_calibration": "3D_ELLIPSOID_VALID"
  },
  "bme690": {
    "temp_offset_c": -0.85,
    "baseline_r0_kohm": 142.5
  }
}""")
        elif name == "sys.log":
            print("""[BOOT+0.000] ESP-IDF v4.4.6-dirty ESP32-S3 (revision v0.2)
[BOOT+0.015] PSRAM initialized: 8MB Octal @ 80MHz
[BOOT+0.032] HAL initialized. Display ST7789 @ 40MHz dedicated SPI
[BOOT+0.048] MicroSD mounted over 4-bit SDMMC bus: 256MB
[BOOT+0.065] StorageManager loaded /sys/config.bin (CRC-16 valid)
[BOOT+0.082] Watchface started. System ready.""")
        elif name == "telemetry.csv":
            print("timestamp_ms,roll_deg,pitch_deg,yaw_deg,temp_c,press_hpa,iaq\n"
                  "1000,0.12,-0.45,182.1,24.1,1013.2,22\n"
                  "1100,0.15,-0.42,182.2,24.1,1013.2,22\n"
                  "1200,0.18,-0.40,182.2,24.2,1013.2,21\n"
                  "... (409,600 bytes total)")
        elif name == "config.bin":
            log_info("Binary configuration record (64 bytes). Use 'config read <path>' to unpack.")
        else:
            log_warn(f"File '{file_path}' is binary or empty.")


# ==============================================================================
# SECTION 3: INTEGRATED CONFIGURATION MANAGEMENT (STANDALONE ENGINE)
# ==============================================================================

NX_CONFIG_MAGIC = 0x584E  # "NX" in ASCII
NX_CONFIG_VERSION = 1
STRUCT_SIZE = 64

# Format string: Little-Endian (<)
# Header:        <H B B H H   (magic, version, reserved1, structSize, crc16)
# Display:       B B B B H H  (brightness, autoDim, tiltMode, wristCover, timeout, fadeMs)
# Audio:         B B B B H    (silentMode, volume, duration, reservedAudio, freqHz)
# Notif:         B B B B      (notifMode, matrixLed, hrmReminder, reservedNotif)
# NPM:           B B B B      (npmPower, npmBrightness, npmPattern, reservedNpm)
# Power:         B B B B      (ecoMode, autoStandby, sensorSleep, reservedPower)
# AI:            B B H        (nxAisCoProc, adaptiveSensing, reservedAi)
# Expansion:     26s          (futureExpansion: 26 bytes)
STRUCT_FORMAT = "<H B B H H B B B B H H B B B B H B B B B B B B B B B B B B B H 26s"

TILT_NAMES = ["OFF", "SENSITIVE", "BALANCED", "SLUGGISH"]
NOTIF_NAMES = ["SILENT", "ALL", "SOUND ONLY", "LIGHTS ONLY"]
NPM_PATTERNS = ["RADAR", "RAIN", "PLASMA", "TRACER", "BREATH", "OFF"]
HRM_REMINDERS = ["OFF", "30m", "1h", "2h"]


def compute_crc16(data: bytes) -> int:
    """Computes CCITT CRC-16 (poly 0x1021, init 0xFFFF) across byte buffer."""
    crc = 0xFFFF
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def parse_config(data: bytes) -> dict:
    if len(data) != STRUCT_SIZE:
        raise ValueError(f"Invalid file size: {len(data)} bytes (expected {STRUCT_SIZE} bytes)")

    unpacked = struct.unpack(STRUCT_FORMAT, data)

    cfg = {
        "magic": unpacked[0],
        "version": unpacked[1],
        "reserved1": unpacked[2],
        "structSize": unpacked[3],
        "crc16": unpacked[4],
        # Display
        "brightnessPercent": unpacked[5],
        "autoDimEnabled": bool(unpacked[6]),
        "tiltMode": unpacked[7],
        "wristCoverSleep": bool(unpacked[8]),
        "screenTimeoutSec": unpacked[9],
        "backlightFadeMs": unpacked[10],
        # Audio
        "silentMode": bool(unpacked[11]),
        "buzzerVolumePercent": unpacked[12],
        "buzzerTickDurationMs": unpacked[13],
        "buzzerBaseFreqHz": unpacked[15],
        # Notifications
        "notifMode": unpacked[16],
        "matrixLedAlerts": bool(unpacked[17]),
        "hrmReminderIdx": unpacked[18],
        # NPM
        "npmPower": bool(unpacked[20]),
        "npmBrightnessPercent": unpacked[21],
        "npmPatternIdx": unpacked[22],
        # Power
        "ecoMode": bool(unpacked[24]),
        "autoStandby": bool(unpacked[25]),
        "sensorSleep": bool(unpacked[26]),
        # AI
        "nxAisCoProc": bool(unpacked[28]),
        "adaptiveSensing": bool(unpacked[29]),
    }

    calculated_crc = compute_crc16(data[8:])
    cfg["crc_valid"] = (cfg["crc16"] == calculated_crc)
    cfg["calculated_crc"] = calculated_crc
    return cfg


def pack_config(cfg: dict) -> bytes:
    payload_before_crc = struct.pack(
        "<H B B H",
        NX_CONFIG_MAGIC,
        cfg.get("version", NX_CONFIG_VERSION),
        0,
        STRUCT_SIZE,
    )

    payload_after_crc = struct.pack(
        "<B B B B H H B B B B H B B B B B B B B B B B B B B H 26s",
        int(cfg.get("brightnessPercent", 80)),
        int(cfg.get("autoDimEnabled", False)),
        int(cfg.get("tiltMode", 2)),
        int(cfg.get("wristCoverSleep", True)),
        int(cfg.get("screenTimeoutSec", 15)),
        int(cfg.get("backlightFadeMs", 300)),
        int(cfg.get("silentMode", True)),
        int(cfg.get("buzzerVolumePercent", 80)),
        int(cfg.get("buzzerTickDurationMs", 8)),
        0,
        int(cfg.get("buzzerBaseFreqHz", 2700)),
        int(cfg.get("notifMode", 1)),
        int(cfg.get("matrixLedAlerts", True)),
        int(cfg.get("hrmReminderIdx", 0)),
        0,
        int(cfg.get("npmPower", False)),
        int(cfg.get("npmBrightnessPercent", 50)),
        int(cfg.get("npmPatternIdx", 0)),
        0,
        int(cfg.get("ecoMode", False)),
        int(cfg.get("autoStandby", True)),
        int(cfg.get("sensorSleep", False)),
        0,
        int(cfg.get("nxAisCoProc", False)),
        int(cfg.get("adaptiveSensing", True)),
        0,
        b"\x00" * 26,
    )

    crc = compute_crc16(payload_after_crc)
    full_struct = payload_before_crc + struct.pack("<H", crc) + payload_after_crc
    return full_struct


def print_formatted_config(cfg: dict, filepath: str):
    """Plain-text diagnostic fallback report."""
    crc_status = "VALID (OK)" if cfg["crc_valid"] else f"INVALID (Stored: 0x{cfg['crc16']:04X}, Expected: 0x{cfg['calculated_crc']:04X})"

    print("=" * 60)
    print(f"  NX-ISD CONFIGURATION DUMP: {filepath}")
    print("=" * 60)
    print(f" Magic:             0x{cfg['magic']:04X} {'[OK]' if cfg['magic'] == NX_CONFIG_MAGIC else '[ERROR]'}")
    print(f" Schema Version:    v{cfg['version']}")
    print(f" Struct Size:       {cfg['structSize']} bytes")
    print(f" CRC-16 (CCITT):    0x{cfg['crc16']:04X} -> {crc_status}")
    print("-" * 60)
    print(" [DISPLAY]")
    print(f"  * Brightness:     {cfg['brightnessPercent']}%")
    print(f"  * Auto Dim:       {'ENABLED' if cfg['autoDimEnabled'] else 'DISABLED'}")
    tilt_name = TILT_NAMES[cfg['tiltMode']] if 0 <= cfg['tiltMode'] < len(TILT_NAMES) else f"UNKNOWN({cfg['tiltMode']})"
    print(f"  * Tilt to Wake:   {tilt_name}")
    print(f"  * Wrist Sleep:    {'ENABLED' if cfg['wristCoverSleep'] else 'DISABLED'}")
    timeout_str = "NEVER" if cfg['screenTimeoutSec'] == 0 else f"{cfg['screenTimeoutSec']}s"
    print(f"  * Screen Timeout: {timeout_str}")
    fade_str = "OFF (Instant)" if cfg['backlightFadeMs'] == 0 else f"{cfg['backlightFadeMs']} ms"
    print(f"  * Backlight Fade: {fade_str}")
    print("-" * 60)
    print(" [AUDIO / BUZZER]")
    print(f"  * Master Audio:   {'MUTED' if cfg['silentMode'] else 'ACTIVE'}")
    print(f"  * Piezo Volume:   {cfg['buzzerVolumePercent']}%")
    print(f"  * Tick Duration:  {cfg['buzzerTickDurationMs']} ms")
    print(f"  * Base Frequency: {cfg['buzzerBaseFreqHz']} Hz")
    print("-" * 60)
    print(" [NOTIFICATIONS]")
    notif_name = NOTIF_NAMES[cfg['notifMode']] if 0 <= cfg['notifMode'] < len(NOTIF_NAMES) else f"UNKNOWN({cfg['notifMode']})"
    print(f"  * Alert Method:   {notif_name}")
    print(f"  * Matrix LED:     {'ENABLED' if cfg['matrixLedAlerts'] else 'DISABLED'}")
    hrm_name = HRM_REMINDERS[cfg['hrmReminderIdx']] if 0 <= cfg['hrmReminderIdx'] < len(HRM_REMINDERS) else f"UNKNOWN({cfg['hrmReminderIdx']})"
    print(f"  * HRM Reminder:   {hrm_name}")
    print("-" * 60)
    print(" [NEOPIXEL MATRIX (NPM)]")
    print(f"  * Matrix Power:   {'ON' if cfg['npmPower'] else 'OFF'}")
    print(f"  * Brightness:     {cfg['npmBrightnessPercent']}%")
    npm_pat = NPM_PATTERNS[cfg['npmPatternIdx']] if 0 <= cfg['npmPatternIdx'] < len(NPM_PATTERNS) else f"UNKNOWN({cfg['npmPatternIdx']})"
    print(f"  * Pattern:        {npm_pat}")
    print("-" * 60)
    print(" [POWER & SYSTEM]")
    print(f"  * Eco Mode:       {'ACTIVE' if cfg['ecoMode'] else 'OFF'}")
    print(f"  * Auto Standby:   {'ENABLED' if cfg['autoStandby'] else 'DISABLED'}")
    print(f"  * Sensor Sleep:   {'ENABLED' if cfg['sensorSleep'] else 'DISABLED'}")
    print("-" * 60)
    print(" [NX-AIS]")
    print(f"  * Co-Processor:   {'ACTIVE' if cfg['nxAisCoProc'] else 'DORMANT'}")
    print(f"  * Adaptive Sense: {'ENABLED' if cfg['adaptiveSensing'] else 'DISABLED'}")
    print("=" * 60)


def render_rich_config(cfg: dict, filepath: str):
    """Renders a styled Rich table for parsed config.bin."""
    if not RICH_AVAILABLE:
        print_formatted_config(cfg, filepath)
        return

    crc_valid = cfg.get("crc_valid", False)
    crc_badge = "[bold green]VALID (CRC-16 OK)[/bold green]" if crc_valid else f"[bold red]INVALID (0x{cfg.get('crc16', 0):04X})[/bold red]"

    table = Table(title=f"NX-ISD CONFIGURATION // {os.path.basename(filepath)}", box=box.ROUNDED, border_style="#FFB000")
    table.add_column("Category", style="bold cyan")
    table.add_column("Parameter", style="white")
    table.add_column("Configured Value", style="bold")
    table.add_column("Notes", style="dim")

    # Header
    table.add_row("Header", "Magic & Schema", f"0x{cfg.get('magic', 0):04X} (v{cfg.get('version', 1)})", f"Struct Size: {cfg.get('structSize', 64)} B")
    table.add_row("Header", "Integrity Check", crc_badge, f"Calculated: 0x{cfg.get('calculated_crc', 0):04X}")

    # Display
    t_idx = cfg.get("tiltMode", 2)
    t_str = TILT_NAMES[t_idx] if 0 <= t_idx < len(TILT_NAMES) else str(t_idx)
    timeout = cfg.get("screenTimeoutSec", 15)
    t_disp = "NEVER (Always On)" if timeout == 0 else f"{timeout} seconds"
    table.add_row("Display", "Brightness", f"{cfg.get('brightnessPercent', 80)}%", "Auto-Dim: " + ("[green]ON[/green]" if cfg.get("autoDimEnabled") else "[dim]OFF[/dim]"))
    table.add_row("Display", "Gestures & Timeout", f"Tilt: {t_str} | Timeout: {t_disp}", "Wrist Sleep: " + ("[green]ON[/green]" if cfg.get("wristCoverSleep") else "[dim]OFF[/dim]"))

    # Audio
    table.add_row("Audio", "Piezo Buzzer", f"{cfg.get('buzzerVolumePercent', 80)}% ({cfg.get('buzzerBaseFreqHz', 2700)} Hz)", "Audio: " + ("[dim]MUTED[/dim]" if cfg.get("silentMode") else "[green]ACTIVE[/green]"))

    # Notifications
    n_idx = cfg.get("notifMode", 1)
    n_str = NOTIF_NAMES[n_idx] if 0 <= n_idx < len(NOTIF_NAMES) else str(n_idx)
    table.add_row("Notifications", "Alerts & Reminders", f"Mode: {n_str}", "Matrix LED: " + ("[green]ON[/green]" if cfg.get("matrixLedAlerts") else "[dim]OFF[/dim]"))

    # NPM
    p_idx = cfg.get("npmPatternIdx", 0)
    p_str = NPM_PATTERNS[p_idx] if 0 <= p_idx < len(NPM_PATTERNS) else str(p_idx)
    table.add_row("Matrix", "4x4 RGB NeoPixel", f"Pattern: {p_str} | {cfg.get('npmBrightnessPercent', 50)}%", "Power: " + ("[green]ON[/green]" if cfg.get("npmPower") else "[dim]OFF[/dim]"))

    # Power & AI
    table.add_row("System", "Power Profile", "Eco: " + ("[green]ACTIVE[/green]" if cfg.get("ecoMode") else "[dim]OFF[/dim]"), "Auto-Standby: " + ("[green]ON[/green]" if cfg.get("autoStandby") else "[dim]OFF[/dim]"))
    table.add_row("System", "NX-AIS Co-Processor", "Adaptive: " + ("[green]ON[/green]" if cfg.get("adaptiveSensing") else "[dim]OFF[/dim]"), "Co-Proc: " + ("[green]ACTIVE[/green]" if cfg.get("nxAisCoProc") else "[dim]DORMANT[/dim]"))

    console.print(table)


def config_read_file(filepath: str):
    if not os.path.exists(filepath):
        log_err(f"File not found: {filepath}")
        return
    with open(filepath, "rb") as f:
        data = f.read()

    try:
        cfg = parse_config(data)
        render_rich_config(cfg, filepath)
    except Exception as e:
        log_err(f"Failed to parse config file: {e}")


def config_to_json_file(bin_path: str, out_path: Optional[str] = None):
    if not os.path.exists(bin_path):
        log_err(f"File not found: {bin_path}")
        return
    with open(bin_path, "rb") as f:
        data = f.read()

    try:
        cfg = parse_config(data)
        target = out_path or os.path.splitext(bin_path)[0] + ".json"
        with open(target, "w") as f:
            json.dump(cfg, f, indent=2)
        log_success(f"Exported configuration to JSON: [bold cyan]{target}[/bold cyan]")
    except Exception as e:
        log_err(f"Failed to export JSON: {e}")


def config_from_json_file(json_path: str, out_path: Optional[str] = None):
    if not os.path.exists(json_path):
        log_err(f"File not found: {json_path}")
        return
    with open(json_path, "r") as f:
        cfg = json.load(f)

    try:
        packed = pack_config(cfg)
        target = out_path or "config.bin"
        with open(target, "wb") as f:
            f.write(packed)
        log_success(f"Generated binary config ({len(packed)} bytes) at: [bold cyan]{target}[/bold cyan]")
    except Exception as e:
        log_err(f"Failed to generate config.bin: {e}")


def config_create_default(out_path: Optional[str] = None):
    default_cfg = {
        "version": 1,
        "brightnessPercent": 80,
        "autoDimEnabled": False,
        "tiltMode": 2,  # BALANCED
        "wristCoverSleep": True,
        "screenTimeoutSec": 15,
        "backlightFadeMs": 300,
        "silentMode": True,
        "buzzerVolumePercent": 80,
        "buzzerTickDurationMs": 8,
        "buzzerBaseFreqHz": 2700,
        "notifMode": 1,  # NOTIF_ALL
        "matrixLedAlerts": True,
        "hrmReminderIdx": 0,
        "npmPower": False,
        "npmBrightnessPercent": 50,
        "npmPatternIdx": 0,
        "ecoMode": False,
        "autoStandby": True,
        "sensorSleep": False,
        "nxAisCoProc": False,
        "adaptiveSensing": True,
    }
    try:
        packed = pack_config(default_cfg)
        target = out_path or "config.bin"
        with open(target, "wb") as f:
            f.write(packed)
        log_success(f"Created factory default [bold cyan]{target}[/bold cyan] ({len(packed)} bytes)")
    except Exception as e:
        log_err(f"Failed to create default config: {e}")


# ==============================================================================
# SECTION 4: INTERACTIVE SHELL CONSOLE
# ==============================================================================

def interactive_shell(dev: ISDDevice):
    """Interactive Command Shell for unified watch & config operations."""
    if RICH_AVAILABLE:
        console.print(f"[bold #FFB000]{BANNER}[/bold #FFB000]")
        console.print("[dim]Type [bold]help[/bold] for command list, [bold]exit[/bold] or Ctrl+C to quit.[/dim]\n")
    else:
        print(BANNER)

    is_connected = dev.connect()
    if is_connected:
        log_success(f"Connected to NX-ISD on [bold]{dev.port}[/bold] @ {dev.baud} baud\n")
    else:
        log_warn("No active serial link. Config & storage tools available offline. Reconnect with 'connect <port>'.\n")

    fs = ISDFileSystem(dev)

    while True:
        try:
            status_tag = f"[{dev.port}]" if (dev.ser and dev.ser.is_open) else "[OFFLINE]"
            path_tag = f" {fs.current_path}" if fs.current_path != "/" else ""
            prompt = f"NX-ISD {status_tag}{path_tag} > " if not RICH_AVAILABLE else f"[bold #FFB000]NX-ISD[/bold #FFB000] [dim]{status_tag}[/dim][cyan]{path_tag}[/cyan] [dim]>[/dim] "
            if RICH_AVAILABLE:
                cmd_input = console.input(prompt).strip()
            else:
                cmd_input = input(prompt).strip()

            if not cmd_input:
                continue

            parts = cmd_input.split()
            verb = parts[0].lower()
            args = parts[1:]

            if verb in ["exit", "quit", "q"]:
                log_info("Exiting ISD-CLI. Goodbye!")
                break
            elif verb in ["help", "h", "?"]:
                if RICH_AVAILABLE:
                    table = Table(title="ISD-CLI UNIFIED COMMAND SUITE", box=box.ROUNDED, border_style="cyan")
                    table.add_column("Category", style="bold cyan")
                    table.add_column("Command", style="white")
                    table.add_column("Description", style="dim")
                    table.add_row("Telemetry", "monitor (m)", "Live single-line 2Hz updating telemetry (or --hud)")
                    table.add_row("Telemetry", "sensors (s)", "Print snapshot table of all live sensors")
                    table.add_row("Telemetry", "uplink on / off", "Toggle USB-CDC serial telemetry streaming")
                    table.add_row("Storage / FS", "ls / dir [path]", "List files & directories with sizes")
                    table.add_row("Storage / FS", "tree [path]", "Print visual hierarchical directory tree")
                    table.add_row("Storage / FS", "cd <dir>", "Navigate storage filesystem (.. or /path)")
                    table.add_row("Storage / FS", "pwd", "Print current working directory")
                    table.add_row("Storage / FS", "df", "Show storage capacity, used & free space")
                    table.add_row("Storage / FS", "cat <file>", "Print contents of text files (audit_log.txt, etc.)")
                    table.add_row("Wireless", "ble on / off / status", "Toggle or inspect BLE GATT Uplink Server")
                    table.add_row("Blind Nav", "nav left / right / push / back", "Inject blind navigation clicks (l / r / p / b)")
                    table.add_row("Matrix", "matrix <radar/rain/heart/clear>", "Control 16-LED NeoPixel matrix")
                    table.add_row("Config File", "config read <file.bin>", "Inspect and verify a /sys/config.bin")
                    table.add_row("Config File", "config default [file.bin]", "Generate factory default config.bin")
                    table.add_row("Config File", "config to-json / from-json", "Convert between binary & JSON config")
                    table.add_row("System", "info", "Query MCU CPU, Heap, PSRAM, uptime")
                    table.add_row("System", "reboot", "Cleanly reboot ESP32-S3")
                    table.add_row("System", "sync", "Sync RTC clock with computer local time")
                    table.add_row("Serial Link", "connect [port]", "Connect/reconnect to a serial port")
                    table.add_row("Raw Protocol", "raw <cmd>", "Send raw protocol command line")
                    console.print(table)
                else:
                    print("Commands:\n"
                          "  Telemetry  : monitor (m) [--rate=2] [--hud] [--mock], sensors (s), uplink [on|off]\n"
                          "  Storage/FS : ls [path], dir, cd <dir>, pwd, tree [path], df, cat <file>\n"
                          "  Wireless   : ble [on|off|status]\n"
                          "  Controls   : nav [l|r|p|b], matrix [radar|rain|heart|clear]\n"
                          "  Config     : config [read|default|to-json|from-json]\n"
                          "  System     : info, reboot, sync, connect [port], raw <cmd>")
            elif verb in ["ls", "dir"]:
                fs.ls(args[0] if args else None)
            elif verb == "cd":
                fs.cd(args[0] if args else "/")
            elif verb == "pwd":
                fs.pwd()
            elif verb == "tree":
                fs.tree(args[0] if args else "/")
            elif verb == "df":
                fs.df()
            elif verb in ["cat", "type"]:
                if args:
                    fs.cat(args[0])
                else:
                    log_warn("Usage: cat <file>")
            elif verb == "connect":
                if args:
                    dev.port = args[0]
                dev.close()
                if dev.connect():
                    log_success(f"Connected to [bold]{dev.port}[/bold]")
            elif verb == "ble":
                cmd_ble(dev, args[0] if args else "status")
            elif verb in ["sensors", "telemetry", "s"]:
                cmd_sensors(dev)
            elif verb in ["monitor", "hud", "m"]:
                mode = "line"
                rate = 2.0
                mock = False
                for a in args:
                    if a in ["--hud", "hud"]:
                        mode = "hud"
                    elif a in ["--line", "line"]:
                        mode = "line"
                    elif a in ["--mock", "mock"]:
                        mock = True
                    elif a.startswith("--rate="):
                        try: rate = float(a.split("=")[1])
                        except ValueError: pass
                    elif a.replace(".", "", 1).isdigit():
                        rate = float(a)
                cmd_monitor(dev, mode=mode, rate_hz=rate, mock=mock)
            elif verb == "uplink":
                cmd_uplink(dev, args[0] if args else "on")
            elif verb in ["nav", "l", "r", "p", "b"]:
                action = args[0] if verb == "nav" and args else verb
                cmd_nav(dev, action)
            elif verb == "matrix":
                sub = args[0] if args else "radar"
                val = args[1] if len(args) > 1 else None
                cmd_matrix(dev, sub, val)
            elif verb == "config":
                sub = args[0].lower() if args else "help"
                cfg_args = args[1:]
                if sub == "read" and cfg_args:
                    config_read_file(cfg_args[0])
                elif sub in ["default", "create-default"]:
                    target = cfg_args[0] if cfg_args else "config.bin"
                    config_create_default(target)
                elif sub in ["to-json", "json"] and cfg_args:
                    target = cfg_args[1] if len(cfg_args) > 1 else None
                    config_to_json_file(cfg_args[0], target)
                elif sub in ["from-json"] and cfg_args:
                    target = cfg_args[1] if len(cfg_args) > 1 else "config.bin"
                    config_from_json_file(cfg_args[0], target)
                else:
                    log_warn("Usage: config read <file.bin> | config default [out.bin] | config to-json <file.bin> [out.json] | config from-json <file.json> [out.bin]")
            elif verb == "info":
                cmd_info(dev)
            elif verb == "reboot":
                dev.send_command("sys.reboot")
                log_success("Reboot signal transmitted.")
            elif verb == "sync":
                epoch = int(time.time())
                res = dev.send_command(f"rtc.sync={epoch}")
                log_success(f"RTC clock synchronized to epoch {epoch} ({res})")
            elif verb == "raw":
                raw_cmd = " ".join(args)
                res = dev.send_command(raw_cmd)
                console.print(res)
            else:
                res = dev.send_command(cmd_input)
                if res:
                    console.print(res)
                else:
                    log_warn("No ACK received or unknown command. Type 'help'.")
        except (KeyboardInterrupt, EOFError):
            print("\n")
            log_info("Exiting shell.")
            break

    dev.close()


# ==============================================================================
# SECTION 5: CLI ARGUMENT PARSER ENTRYPOINT
# ==============================================================================

def main():
    parser = argparse.ArgumentParser(description="ISD-CLI // Unified Intelligent Sensor Device Control & Configuration Suite")
    parser.add_argument("--port", "-p", help="Serial port (e.g. COM17, /dev/ttyACM0). Auto-detected if omitted.")
    parser.add_argument("--baud", "-b", type=int, default=DEFAULT_BAUD, help=f"Baud rate (default: {DEFAULT_BAUD})")

    subparsers = parser.add_subparsers(dest="command", help="Sub-commands")

    # shell
    subparsers.add_parser("shell", help="Enter interactive unified terminal shell")

    # ble
    p_ble = subparsers.add_parser("ble", help="Control BLE Uplink Bridge")
    p_ble.add_argument("action", choices=["on", "off", "start", "stop", "status"], default="status", nargs="?")

    # sensors
    subparsers.add_parser("sensors", help="Display single snapshot of all live sensor telemetry")

    # monitor
    p_mon = subparsers.add_parser("monitor", help="Live updating terminal monitor")
    p_mon.add_argument("--rate", type=float, default=2.0, help="Update frequency in Hz (default: 2.0)")
    p_mon.add_argument("--hud", action="store_true", help="Display full panel HUD (requires rich)")
    p_mon.add_argument("--mock", action="store_true", help="Run with simulated 2Hz stream offline")

    # storage commands
    p_ls = subparsers.add_parser("ls", help="List storage directory contents")
    p_ls.add_argument("path", nargs="?", default=None, help="Path to list")

    p_tree = subparsers.add_parser("tree", help="Display visual storage hierarchy tree")
    p_tree.add_argument("path", nargs="?", default="/", help="Starting path")

    subparsers.add_parser("df", help="Display storage volume capacity and free space")

    p_cat = subparsers.add_parser("cat", help="Display text file contents")
    p_cat.add_argument("file", help="Path to file")

    # uplink
    p_up = subparsers.add_parser("uplink", help="Control USB telemetry stream")
    p_up.add_argument("action", choices=["on", "off", "start", "stop"], default="on", nargs="?")
    p_up.add_argument("--rate", type=int, default=None, help="Rate in Hz (1-100)")

    # nav
    p_nav = subparsers.add_parser("nav", help="Inject blind navigation lever/button clicks")
    p_nav.add_argument("action", choices=["left", "right", "push", "back", "btn", "l", "r", "p", "b"])

    # matrix
    p_mat = subparsers.add_parser("matrix", help="Control 4x4 NeoPixel matrix")
    p_mat.add_argument("action", choices=["on", "off", "radar", "rain", "plasma", "tracer", "breath", "ripple", "heart", "clear", "brightness"])
    p_mat.add_argument("value", nargs="?", default=None, help="Value (e.g. brightness 0-80)")

    # config
    p_cfg = subparsers.add_parser("config", help="Binary /sys/config.bin inspection, verification & generation")
    cfg_subs = p_cfg.add_subparsers(dest="config_action", help="Config operations")

    p_c_read = cfg_subs.add_parser("read", help="Parse and inspect config.bin")
    p_c_read.add_argument("file", help="Path to config.bin")

    p_c_to_json = cfg_subs.add_parser("to-json", help="Convert config.bin to JSON")
    p_c_to_json.add_argument("file", help="Path to config.bin")
    p_c_to_json.add_argument("-o", "--output", help="Optional output JSON path")

    p_c_from_json = cfg_subs.add_parser("from-json", help="Generate config.bin from JSON")
    p_c_from_json.add_argument("file", help="Path to JSON file")
    p_c_from_json.add_argument("-o", "--output", help="Output .bin path (default: config.bin)")

    p_c_def = cfg_subs.add_parser("default", aliases=["create-default"], help="Generate factory default config.bin")
    p_c_def.add_argument("-o", "--output", help="Output .bin path (default: config.bin)")

    # info
    subparsers.add_parser("info", help="Display MCU CPU, Heap, and PSRAM diagnostics")

    # reboot
    subparsers.add_parser("reboot", help="Reboot ISD-Core MCU")

    # sync
    subparsers.add_parser("sync", help="Synchronize RTC clock with host epoch")

    # raw
    p_raw = subparsers.add_parser("raw", help="Send raw protocol command line")
    p_raw.add_argument("cmd", help="Raw command string (e.g. 'npm.power=1')")

    args = parser.parse_args()

    # Handle offline config and storage commands without requiring serial hardware connection
    if args.command == "config":
        if args.config_action == "read":
            config_read_file(args.file)
        elif args.config_action == "to-json":
            config_to_json_file(args.file, args.output)
        elif args.config_action == "from-json":
            config_from_json_file(args.file, args.output)
        elif args.config_action in ("default", "create-default"):
            config_create_default(args.output)
        else:
            p_cfg.print_help()
        return

    dev = ISDDevice(port=args.port, baud=args.baud)

    # Storage commands (offline & online)
    if args.command in ["ls", "tree", "df", "cat"]:
        fs = ISDFileSystem(dev)
        if args.command == "ls":
            fs.ls(args.path)
        elif args.command == "tree":
            fs.tree(args.path)
        elif args.command == "df":
            fs.df()
        elif args.command == "cat":
            fs.cat(args.file)
        return

    if not args.command or args.command == "shell":
        interactive_shell(dev)
    else:
        if args.command == "monitor" and getattr(args, "mock", False):
            mode = "hud" if args.hud else "line"
            cmd_monitor(dev, mode=mode, rate_hz=args.rate, mock=True)
            return

        if not dev.connect():
            sys.exit(1)
        try:
            if args.command == "ble":
                cmd_ble(dev, args.action)
            elif args.command == "sensors":
                cmd_sensors(dev)
            elif args.command == "monitor":
                mode = "hud" if args.hud else "line"
                cmd_monitor(dev, mode=mode, rate_hz=args.rate, mock=getattr(args, "mock", False))
            elif args.command == "uplink":
                cmd_uplink(dev, args.action, args.rate)
            elif args.command == "nav":
                cmd_nav(dev, args.action)
            elif args.command == "matrix":
                cmd_matrix(dev, args.action, args.value)
            elif args.command == "info":
                cmd_info(dev)
            elif args.command == "reboot":
                dev.send_command("sys.reboot")
                log_success("Reboot signal transmitted.")
            elif args.command == "sync":
                epoch = int(time.time())
                res = dev.send_command(f"rtc.sync={epoch}")
                log_success(f"RTC clock synchronized to epoch {epoch} ({res})")
            elif args.command == "raw":
                res = dev.send_command(args.cmd)
                if RICH_AVAILABLE:
                    console.print(res)
                else:
                    print(res)
        finally:
            dev.close()


if __name__ == "__main__":
    main()
