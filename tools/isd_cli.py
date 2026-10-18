#!/usr/bin/env python3
"""
ISD-CLI // Intelligent Sensor Device Terminal Control Utility
Command-line & Interactive Shell for NX-ISD / ISD-Core over USB-CDC Serial.
Enables full watch control, BLE activation, telemetry streaming, blind navigation,
and sensor inspection without needing a working physical display.
"""

import sys
import os
import time
import json
import argparse
from typing import Optional, Dict, Any, List

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("Error: 'pyserial' is required. Please install via: pip install pyserial")
    sys.exit(1)

try:
    from rich.console import Console
    from rich.table import Table
    from rich.panel import Panel
    from rich.live import Live
    from rich.text import Text
    from rich.layout import Layout
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


class ISDDevice:
    def __init__(self, port: Optional[str] = None, baud: int = DEFAULT_BAUD, timeout: float = 1.0):
        self.port = port
        self.baud = baud
        self.timeout = timeout
        self.ser: Optional[serial.Serial] = None

    @staticmethod
    def detect_ports() -> List[str]:
        """Detect potential ESP32-S3 USB CDC serial ports."""
        ports = []
        for p in serial.tools.list_ports.comports():
            # Check for ESP32-S3 USB JTAG / CDC or standard USB-serial descriptors
            desc = (p.description or "").lower()
            hwid = (p.hwid or "").lower()
            if "esp32" in desc or "303a" in hwid or "usb serial" in desc or "cdc" in desc or "ch340" in desc or "cp210" in desc:
                ports.append(p.device)
            elif p.device not in ports:
                ports.append(p.device)
        return ports

    def connect(self) -> bool:
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
            # ESP32-S3 USB-CDC toggle DTR/RTS to stabilize connection
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
                    # Echo or debug print line
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


def cmd_ble(dev: ISDDevice, action: str):
    """Toggle or inspect BLE state."""
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
        # Fallback: maybe stream is running or try reading next packet
        log_warn("Waiting for telemetry frame...")
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
        table = Table(title="NX-ISD // LIVE SENSOR TELEMETRY", box=box.ROUNDED, border_style="amber")
        table.add_column("Subsystem", style="bold cyan")
        table.add_column("Sensor", style="dim")
        table.add_column("Primary Telemetry", style="bold")
        table.add_column("Secondary / Environment", style="dim")

        # 1. 9-DOF IMU
        roll = data.get("roll", 0.0)
        pitch = data.get("pitch", 0.0)
        yaw = data.get("yaw", 0.0)
        head = data.get("heading", yaw % 360)
        table.add_row(
            "Attitude (9-DOF)",
            "BNO085",
            f"Roll: {roll:+.1f}° • Pitch: {pitch:+.1f}°",
            f"Yaw: {yaw:+.1f}° (HDG: {head:03.0f}°)"
        )

        # 2. Climate & Barometer
        temp = data.get("temp", 0.0)
        hum = data.get("hum", 0.0)
        press = data.get("press", 0.0)
        alt = 44330.0 * (1.0 - (press / 1013.25) ** 0.1903) if press > 0 else 0
        table.add_row(
            "Atmosphere",
            "BME690",
            f"Temp: {temp:.1f}°C • Hum: {hum:.1f}%",
            f"Baro: {press:.1f} hPa (~{alt:.0f}m)"
        )

        # 3. MOX Gas & IAQ
        gas = data.get("gas", 0.0)
        iaq = int(data.get("iaq", 25))
        iaq_txt = get_iaq_tier(iaq)
        table.add_row(
            "Air Quality",
            "BME690 MOX",
            f"IAQ Index: {iaq} ({iaq_txt})",
            f"MOX Resistance: {gas/1000.0:.1f} kΩ"
        )

        # 4. Ambient Light
        lux = data.get("lux", 0.0)
        table.add_row(
            "Photopic Light",
            "OPT3001",
            f"Illuminance: {lux:.1f} Lux",
            "Weber-Fechner Auto-Dimming"
        )

        # 5. Biometrics
        hr = data.get("hr", 0.0)
        spo2 = data.get("spo2", 0.0)
        table.add_row(
            "Cardiovascular",
            "MAX30102",
            f"Heart Rate: {hr:.0f} BPM" if hr > 0 else "Resting / Idle",
            f"SpO2: {spo2:.0f}%" if spo2 > 0 else "No Finger Contact"
        )

        # 6. Power & Battery
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

        # Draw a text-based bubble level (21 chars wide)
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
        return Panel(content, title="[bold amber]NX-ISD // TELEMETRY HUD[/bold amber]", border_style="amber", box=box.HEAVY)

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


def interactive_shell(dev: ISDDevice):
    """Interactive Command Shell for blind control."""
    if RICH_AVAILABLE:
        console.print(f"[bold amber]{BANNER}[/bold amber]")
        console.print("[dim]Type [bold]help[/bold] for commands, [bold]exit[/bold] or Ctrl+C to quit.[/dim]\n")
    else:
        print(BANNER)

    if not dev.connect():
        log_err("Cannot enter shell without connection. Check USB cable and port.")
        return

    log_success(f"Connected to NX-ISD on [bold]{dev.port}[/bold] @ {dev.baud} baud\n")

    while True:
        try:
            prompt = "NX-ISD // LINK > " if not RICH_AVAILABLE else "[bold amber]NX-ISD[/bold amber] [dim]>[/dim] "
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
                log_info("Disconnecting. Goodbye!")
                break
            elif verb in ["help", "h", "?"]:
                if RICH_AVAILABLE:
                    table = Table(title="ISD-CLI COMMAND CHEATSHEET", box=box.ROUNDED, border_style="cyan")
                    table.add_column("Command", style="bold cyan")
                    table.add_column("Description", style="white")
                    table.add_row("ble on / off / status", "Toggle or inspect BLE GATT Uplink Server")
                    table.add_row("sensors (s)", "Print snapshot table of all live sensors")
                    table.add_row("monitor (m)", "Open live fullscreen updating HUD in terminal")
                    table.add_row("uplink on / off", "Toggle USB-CDC serial telemetry streaming")
                    table.add_row("nav left/right/push/back", "Inject blind navigation clicks (l/r/p/b)")
                    table.add_row("matrix <radar/rain/heart/off>", "Control 16-LED NeoPixel matrix")
                    table.add_row("info", "Query MCU CPU, Heap, PSRAM, uptime")
                    table.add_row("reboot", "Cleanly reboot ESP32-S3")
                    table.add_row("sync", "Sync RTC clock with computer local time")
                    table.add_row("raw <command>", "Send raw protocol command line")
                    console.print(table)
                else:
                    print("Commands: ble [on|off|status], sensors, monitor, uplink [on|off], nav [left|right|push|back], matrix [pattern], info, reboot, raw <cmd>")
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
                # Direct raw command fallback
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


def main():
    parser = argparse.ArgumentParser(description="ISD-CLI // Intelligent Sensor Device Terminal Control Utility")
    parser.add_argument("--port", "-p", help="Serial port (e.g. COM17, /dev/ttyACM0). Auto-detected if omitted.")
    parser.add_argument("--baud", "-b", type=int, default=DEFAULT_BAUD, help=f"Baud rate (default: {DEFAULT_BAUD})")

    subparsers = parser.add_subparsers(dest="command", help="Sub-commands")

    # shell
    subparsers.add_parser("shell", help="Enter interactive terminal control shell")

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
