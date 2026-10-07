#!/usr/bin/env python3
"""
NX-ISD Config Tool (tools/nx_config_tool.py)
Dedicated PC utility to inspect, parse, verify, and generate /sys/config.bin
for the ISD-Core ZDSD NAND Flash storage.
"""

import sys
import os
import struct
import json
import argparse

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

    # Verify CRC over payload (bytes 8..63)
    calculated_crc = compute_crc16(data[8:])
    cfg["crc_valid"] = (cfg["crc16"] == calculated_crc)
    cfg["calculated_crc"] = calculated_crc

    return cfg


def pack_config(cfg: dict) -> bytes:
    # First pack with dummy CRC 0
    dummy_crc = 0
    payload_before_crc = struct.pack(
        "<H B B H",
        NX_CONFIG_MAGIC,
        cfg.get("version", NX_CONFIG_VERSION),
        0,
        STRUCT_SIZE,
    )

    payload_after_crc = struct.pack(
        "<B B B B H H B B B B H B B B B B B B B B B B B B B H 26s",
        # Display
        int(cfg.get("brightnessPercent", 80)),
        int(cfg.get("autoDimEnabled", False)),
        int(cfg.get("tiltMode", 2)),
        int(cfg.get("wristCoverSleep", True)),
        int(cfg.get("screenTimeoutSec", 15)),
        int(cfg.get("backlightFadeMs", 300)),
        # Audio
        int(cfg.get("silentMode", True)),
        int(cfg.get("buzzerVolumePercent", 80)),
        int(cfg.get("buzzerTickDurationMs", 8)),
        0,  # reservedAudio
        int(cfg.get("buzzerBaseFreqHz", 2700)),
        # Notifications
        int(cfg.get("notifMode", 1)),
        int(cfg.get("matrixLedAlerts", True)),
        int(cfg.get("hrmReminderIdx", 0)),
        0,  # reservedNotif
        # NPM
        int(cfg.get("npmPower", False)),
        int(cfg.get("npmBrightnessPercent", 50)),
        int(cfg.get("npmPatternIdx", 0)),
        0,  # reservedNpm
        # Power
        int(cfg.get("ecoMode", False)),
        int(cfg.get("autoStandby", True)),
        int(cfg.get("sensorSleep", False)),
        0,  # reservedPower
        # AI
        int(cfg.get("nxAisCoProc", False)),
        int(cfg.get("adaptiveSensing", True)),
        0,  # reservedAi
        b"\x00" * 26,  # futureExpansion
    )

    # Compute CRC16 across payload after CRC field
    crc = compute_crc16(payload_after_crc)

    full_struct = payload_before_crc + struct.pack("<H", crc) + payload_after_crc
    return full_struct


def print_formatted_config(cfg: dict, filepath: str):
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
    print(f"  • Brightness:     {cfg['brightnessPercent']}%")
    print(f"  • Auto Dim:       {'ENABLED' if cfg['autoDimEnabled'] else 'DISABLED'}")
    tilt_name = TILT_NAMES[cfg['tiltMode']] if 0 <= cfg['tiltMode'] < len(TILT_NAMES) else f"UNKNOWN({cfg['tiltMode']})"
    print(f"  • Tilt to Wake:   {tilt_name}")
    print(f"  • Wrist Sleep:    {'ENABLED' if cfg['wristCoverSleep'] else 'DISABLED'}")
    timeout_str = "NEVER" if cfg['screenTimeoutSec'] == 0 else f"{cfg['screenTimeoutSec']}s"
    print(f"  • Screen Timeout: {timeout_str}")
    fade_str = "OFF (Instant)" if cfg['backlightFadeMs'] == 0 else f"{cfg['backlightFadeMs']} ms"
    print(f"  • Backlight Fade: {fade_str}")
    print("-" * 60)
    print(" [AUDIO / BUZZER]")
    print(f"  • Master Audio:   {'MUTED' if cfg['silentMode'] else 'ACTIVE'}")
    print(f"  • Piezo Volume:   {cfg['buzzerVolumePercent']}%")
    print(f"  • Tick Duration:  {cfg['buzzerTickDurationMs']} ms")
    print(f"  • Base Frequency: {cfg['buzzerBaseFreqHz']} Hz")
    print("-" * 60)
    print(" [NOTIFICATIONS]")
    notif_name = NOTIF_NAMES[cfg['notifMode']] if 0 <= cfg['notifMode'] < len(NOTIF_NAMES) else f"UNKNOWN({cfg['notifMode']})"
    print(f"  • Alert Method:   {notif_name}")
    print(f"  • Matrix LED:     {'ENABLED' if cfg['matrixLedAlerts'] else 'DISABLED'}")
    hrm_name = HRM_REMINDERS[cfg['hrmReminderIdx']] if 0 <= cfg['hrmReminderIdx'] < len(HRM_REMINDERS) else f"UNKNOWN({cfg['hrmReminderIdx']})"
    print(f"  • HRM Reminder:   {hrm_name}")
    print("-" * 60)
    print(" [NEOPIXEL MATRIX (NPM)]")
    print(f"  • Matrix Power:   {'ON' if cfg['npmPower'] else 'OFF'}")
    print(f"  • Brightness:     {cfg['npmBrightnessPercent']}%")
    npm_pat = NPM_PATTERNS[cfg['npmPatternIdx']] if 0 <= cfg['npmPatternIdx'] < len(NPM_PATTERNS) else f"UNKNOWN({cfg['npmPatternIdx']})"
    print(f"  • Pattern:        {npm_pat}")
    print("-" * 60)
    print(" [POWER & SYSTEM]")
    print(f"  • Eco Mode:       {'ACTIVE' if cfg['ecoMode'] else 'OFF'}")
    print(f"  • Auto Standby:   {'ENABLED' if cfg['autoStandby'] else 'DISABLED'}")
    print(f"  • Sensor Sleep:   {'ENABLED' if cfg['sensorSleep'] else 'DISABLED'}")
    print("-" * 60)
    print(" [NX-AIS]")
    print(f"  • Co-Processor:   {'ACTIVE' if cfg['nxAisCoProc'] else 'DORMANT'}")
    print(f"  • Adaptive Sense: {'ENABLED' if cfg['adaptiveSensing'] else 'DISABLED'}")
    print("=" * 60)


def cmd_read(args):
    if not os.path.exists(args.file):
        print(f"Error: File not found: {args.file}")
        sys.exit(1)

    with open(args.file, "rb") as f:
        data = f.read()

    cfg = parse_config(data)
    print_formatted_config(cfg, args.file)


def cmd_to_json(args):
    if not os.path.exists(args.file):
        print(f"Error: File not found: {args.file}")
        sys.exit(1)

    with open(args.file, "rb") as f:
        data = f.read()

    cfg = parse_config(data)
    out_file = args.output or os.path.splitext(args.file)[0] + ".json"

    with open(out_file, "w") as f:
        json.dump(cfg, f, indent=2)

    print(f"Successfully exported JSON to: {out_file}")


def cmd_from_json(args):
    if not os.path.exists(args.file):
        print(f"Error: File not found: {args.file}")
        sys.exit(1)

    with open(args.file, "r") as f:
        cfg = json.load(f)

    packed = pack_config(cfg)
    out_file = args.output or "config.bin"

    with open(out_file, "wb") as f:
        f.write(packed)

    print(f"Successfully generated binary config ({len(packed)} bytes) at: {out_file}")


def cmd_create_default(args):
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

    packed = pack_config(default_cfg)
    out_file = args.output or "config.bin"

    with open(out_file, "wb") as f:
        f.write(packed)

    print(f"Successfully generated factory default config.bin ({len(packed)} bytes) at: {out_file}")


def main():
    parser = argparse.ArgumentParser(description="NX-ISD Binary Config Tool")
    subparsers = parser.add_subparsers(dest="command", required=True)

    # Subcommand: read
    p_read = subparsers.add_parser("read", help="Parse and display a config.bin file")
    p_read.add_argument("file", help="Path to config.bin")
    p_read.set_defaults(func=cmd_read)

    # Subcommand: to-json
    p_to_json = subparsers.add_parser("to-json", help="Convert config.bin to JSON")
    p_to_json.add_argument("file", help="Path to config.bin")
    p_to_json.add_argument("-o", "--output", help="Optional output JSON path")
    p_to_json.set_defaults(func=cmd_to_json)

    # Subcommand: from-json
    p_from_json = subparsers.add_parser("from-json", help="Create config.bin from JSON")
    p_from_json.add_argument("file", help="Path to JSON file")
    p_from_json.add_argument("-o", "--output", help="Output .bin path (default: config.bin)")
    p_from_json.set_defaults(func=cmd_from_json)

    # Subcommand: create-default
    p_def = subparsers.add_parser("create-default", help="Generate factory default config.bin")
    p_def.add_argument("-o", "--output", help="Output .bin path (default: config.bin)")
    p_def.set_defaults(func=cmd_create_default)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
