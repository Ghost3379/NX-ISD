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
import argparse
from typing import Optional, Dict, Any, List

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

# Import standalone config tool logic for 100% binary parity
try:
    import nx_config_tool
    CONFIG_TOOL_AVAILABLE = True
except ImportError:
    CONFIG_TOOL_AVAILABLE = False


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
            f"Roll: {roll:+.1f}° | Pitch: {pitch:+.1f}°",
            f"Yaw: {yaw:+.1f}° (HDG: {head:03.0f}°)"
        )

        temp = data.get("temp", 0.0)
        hum = data.get("hum", 0.0)
        press = data.get("press", 0.0)
        alt = 44330.0 * (1.0 - (press / 1013.25) ** 0.1903) if press > 0 else 0
        table.add_row(
            "Atmosphere",
            "BME690",
            f"Temp: {temp:.1f}°C | Hum: {hum:.1f}%",
            f"Baro: {press:.1f} hPa (~{alt:.0f}m)"
        )

        gas = data.get("gas", 0.0)
        iaq = int(data.get("iaq", 25))
        iaq_txt = get_iaq_tier(iaq)
        table.add_row(
            "Air Quality",
            "BME690 MOX",
            f"IAQ Index: {iaq} ({iaq_txt})",
            f"MOX Resistance: {gas/1000.0:.1f} kΩ"
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


def cmd_monitor(dev: ISDDevice):
    """Live full-screen updating terminal monitor."""
    log_info("Starting Live HUD Monitor (Press Ctrl+C to stop)...")
    dev.send_command("stream.start")

    def render_hud(f: dict) -> Panel:
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
            f"  Roll:  {roll:+6.1f}°  Pitch: {pitch:+6.1f}°  Yaw: {yaw:+6.1f}°\n"
            f"  Level Reticle: [{bubble_str}]\n\n"
            f"[bold green]ENVIRONMENT & AIR QUALITY[/bold green]\n"
            f"  Temp:  {temp:5.1f} °C   Humidity: {hum:5.1f} %\n"
            f"  Baro:  {press:6.1f} hPa  Gas Res: {gas/1000.0:6.1f} kΩ\n"
            f"  IAQ:   {iaq:3d} ({get_iaq_tier(iaq)})\n\n"
            f"[bold yellow]POWER & BIOMETRICS[/bold yellow]\n"
            f"  Ambient: {lux:5.1f} Lux  Battery: {batt:3.0f}% ({volt:.2f}V) {'[bold green][CHARGING][/bold green]' if chg else ''}"
        )
        return Panel(content, title="[bold #FFB000]NX-ISD // TELEMETRY HUD[/bold #FFB000]", border_style="#FFB000", box=box.HEAVY)

    try:
        if RICH_AVAILABLE:
            with Live(console=console, refresh_per_second=8) as live:
                while True:
                    line = dev.read_line()
                    if line and "roll" in line:
                        try:
                            frame = json.loads(line)
                            live.update(render_hud(frame))
                        except Exception:
                            pass
        else:
            while True:
                line = dev.read_line()
                if line and "roll" in line:
                    print(line)
    except KeyboardInterrupt:
        log_info("Monitor terminated.")
        dev.send_command("stream.stop")


# ==============================================================================
# SECTION 3: INTEGRATED CONFIGURATION MANAGEMENT (nx_config_tool)
# ==============================================================================

def render_rich_config(cfg: dict, filepath: str):
    """Renders a gorgeous styled Rich table for parsed config.bin."""
    if not RICH_AVAILABLE:
        if CONFIG_TOOL_AVAILABLE:
            nx_config_tool.print_formatted_config(cfg, filepath)
        else:
            print(json.dumps(cfg, indent=2))
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
    tilt_names = ["OFF", "SENSITIVE", "BALANCED", "SLUGGISH"]
    t_idx = cfg.get("tiltMode", 2)
    t_str = tilt_names[t_idx] if 0 <= t_idx < len(tilt_names) else str(t_idx)
    timeout = cfg.get("screenTimeoutSec", 15)
    t_disp = "NEVER (Always On)" if timeout == 0 else f"{timeout} seconds"
    table.add_row("Display", "Brightness", f"{cfg.get('brightnessPercent', 80)}%", "Auto-Dim: " + ("[green]ON[/green]" if cfg.get("autoDimEnabled") else "[dim]OFF[/dim]"))
    table.add_row("Display", "Gestures & Timeout", f"Tilt: {t_str} | Timeout: {t_disp}", "Wrist Sleep: " + ("[green]ON[/green]" if cfg.get("wristCoverSleep") else "[dim]OFF[/dim]"))

    # Audio
    table.add_row("Audio", "Piezo Buzzer", f"{cfg.get('buzzerVolumePercent', 80)}% ({cfg.get('buzzerBaseFreqHz', 2700)} Hz)", "Audio: " + ("[dim]MUTED[/dim]" if cfg.get("silentMode") else "[green]ACTIVE[/green]"))

    # Notifications
    notif_names = ["SILENT", "ALL", "SOUND ONLY", "LIGHTS ONLY"]
    n_idx = cfg.get("notifMode", 1)
    n_str = notif_names[n_idx] if 0 <= n_idx < len(notif_names) else str(n_idx)
    table.add_row("Notifications", "Alerts & Reminders", f"Mode: {n_str}", "Matrix LED: " + ("[green]ON[/green]" if cfg.get("matrixLedAlerts") else "[dim]OFF[/dim]"))

    # NPM
    npm_pats = ["RADAR", "RAIN", "PLASMA", "TRACER", "BREATH", "OFF"]
    p_idx = cfg.get("npmPatternIdx", 0)
    p_str = npm_pats[p_idx] if 0 <= p_idx < len(npm_pats) else str(p_idx)
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

    if CONFIG_TOOL_AVAILABLE:
        cfg = nx_config_tool.parse_config(data)
        render_rich_config(cfg, filepath)
    else:
        log_err("nx_config_tool module not found in tools directory.")


def config_to_json_file(bin_path: str, out_path: Optional[str] = None):
    if not os.path.exists(bin_path):
        log_err(f"File not found: {bin_path}")
        return
    with open(bin_path, "rb") as f:
        data = f.read()

    if not CONFIG_TOOL_AVAILABLE:
        log_err("nx_config_tool module not found.")
        return

    cfg = nx_config_tool.parse_config(data)
    target = out_path or os.path.splitext(bin_path)[0] + ".json"
    with open(target, "w") as f:
        json.dump(cfg, f, indent=2)
    log_success(f"Exported configuration to JSON: [bold cyan]{target}[/bold cyan]")


def config_from_json_file(json_path: str, out_path: Optional[str] = None):
    if not os.path.exists(json_path):
        log_err(f"File not found: {json_path}")
        return
    with open(json_path, "r") as f:
        cfg = json.load(f)

    if not CONFIG_TOOL_AVAILABLE:
        log_err("nx_config_tool module not found.")
        return

    packed = nx_config_tool.pack_config(cfg)
    target = out_path or "config.bin"
    with open(target, "wb") as f:
        f.write(packed)
    log_success(f"Generated binary config ({len(packed)} bytes) at: [bold cyan]{target}[/bold cyan]")


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
    if not CONFIG_TOOL_AVAILABLE:
        log_err("nx_config_tool module not found.")
        return
    packed = nx_config_tool.pack_config(default_cfg)
    target = out_path or "config.bin"
    with open(target, "wb") as f:
        f.write(packed)
    log_success(f"Created factory default [bold cyan]{target}[/bold cyan] ({len(packed)} bytes)")


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
        log_warn("No active serial link. Config tools available offline. Reconnect with 'connect <port>'.\n")

    while True:
        try:
            status_tag = f"[{dev.port}]" if (dev.ser and dev.ser.is_open) else "[OFFLINE]"
            prompt = f"NX-ISD {status_tag} > " if not RICH_AVAILABLE else f"[bold #FFB000]NX-ISD[/bold #FFB000] [dim]{status_tag}[/dim] [dim]>[/dim] "
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
                    table.add_row("Wireless", "ble on / off / status", "Toggle or inspect BLE GATT Uplink Server")
                    table.add_row("Telemetry", "sensors (s)", "Print snapshot table of all live sensors")
                    table.add_row("Telemetry", "monitor (m)", "Open live fullscreen updating HUD in terminal")
                    table.add_row("Telemetry", "uplink on / off", "Toggle USB-CDC serial telemetry streaming")
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
                    print("Commands: ble [on|off|status], sensors, monitor, uplink [on|off], nav [l|r|p|b], matrix [pat], config [read|default|to-json|from-json], info, reboot, sync, raw <cmd>")
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
                cmd_monitor(dev)
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
    subparsers.add_parser("monitor", help="Live fullscreen terminal HUD")

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
    p_cfg = subparsers.add_parser("config", help="Binary /sys/config.bin inspection & generation (nx_config_tool)")
    cfg_subs = p_cfg.add_subparsers(dest="config_action", help="Config operations")

    p_c_read = cfg_subs.add_parser("read", help="Parse and inspect config.bin")
    p_c_read.add_argument("file", help="Path to config.bin")

    p_c_to_json = cfg_subs.add_parser("to-json", help="Convert config.bin to JSON")
    p_c_to_json.add_argument("file", help="Path to config.bin")
    p_c_to_json.add_argument("-o", "--output", help="Optional output JSON path")

    p_c_from_json = cfg_subs.add_parser("from-json", help="Generate config.bin from JSON")
    p_c_from_json.add_argument("file", help="Path to JSON file")
    p_c_from_json.add_argument("-o", "--output", help="Output .bin path (default: config.bin)")

    p_c_def = cfg_subs.add_parser("default", help="Generate factory default config.bin")
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

    # Handle offline config commands without requiring serial hardware connection
    if args.command == "config":
        if args.config_action == "read":
            config_read_file(args.file)
        elif args.config_action == "to-json":
            config_to_json_file(args.file, args.output)
        elif args.config_action == "from-json":
            config_from_json_file(args.file, args.output)
        elif args.config_action == "default":
            config_create_default(args.output)
        else:
            p_cfg.print_help()
        return

    dev = ISDDevice(port=args.port, baud=args.baud)

    if not args.command or args.command == "shell":
        interactive_shell(dev)
    else:
        if not dev.connect():
            sys.exit(1)
        try:
            if args.command == "ble":
                cmd_ble(dev, args.action)
            elif args.command == "sensors":
                cmd_sensors(dev)
            elif args.command == "monitor":
                cmd_monitor(dev)
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
