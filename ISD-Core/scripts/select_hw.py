import os
import subprocess
import sys

Import("env")

# Cache choice within the same build process so user is only prompted once
_CACHE_KEY = "_NX_ISD_SELECTED_HW"

def get_last_choice(cache_file):
    if os.path.exists(cache_file):
        try:
            with open(cache_file, "r") as f:
                val = f.read().strip()
                if val in ["v1p4", "v1p3"]:
                    return val
        except Exception:
            pass
    return "v1p4"

def save_choice(cache_file, choice):
    try:
        with open(cache_file, "w") as f:
            f.write(choice)
    except Exception:
        pass

def prompt_user_gui(last_choice):
    # PowerShell script to show a clean modal dialog with two choices
    # Focused button defaults to the last selected revision so pressing [Enter] builds immediately.
    ps_script = f"""
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$form = New-Object System.Windows.Forms.Form
$form.Text = "NX-ISD Firmware Target"
$form.Size = New-Object System.Drawing.Size(420, 210)
$form.StartPosition = "CenterScreen"
$form.FormBorderStyle = "FixedDialog"
$form.MaximizeBox = $false
$form.MinimizeBox = $false
$form.TopMost = $true

$labelTitle = New-Object System.Windows.Forms.Label
$labelTitle.Location = New-Object System.Drawing.Point(20, 18)
$labelTitle.Size = New-Object System.Drawing.Size(370, 24)
$labelTitle.Font = New-Object System.Drawing.Font("Segoe UI", 10, [System.Drawing.FontStyle]::Bold)
$labelTitle.Text = "Select Hardware Revision to compile:"
$form.Controls.Add($labelTitle)

$labelDesc = New-Object System.Windows.Forms.Label
$labelDesc.Location = New-Object System.Drawing.Point(20, 44)
$labelDesc.Size = New-Object System.Drawing.Size(370, 20)
$labelDesc.Font = New-Object System.Drawing.Font("Segoe UI", 8.5)
$labelDesc.ForeColor = [System.Drawing.Color]::Gray
$labelDesc.Text = "Configures pinouts, SPI bus, and display timings in pins.h"
$form.Controls.Add($labelDesc)

$btn14 = New-Object System.Windows.Forms.Button
$btn14.Location = New-Object System.Drawing.Point(25, 80)
$btn14.Size = New-Object System.Drawing.Size(170, 48)
$btn14.Font = New-Object System.Drawing.Font("Segoe UI", 9.5, [System.Drawing.FontStyle]::Bold)
$btn14.Text = "v1.4 (Production)"
$btn14.DialogResult = [System.Windows.Forms.DialogResult]::Yes
$form.Controls.Add($btn14)

$btn13 = New-Object System.Windows.Forms.Button
$btn13.Location = New-Object System.Drawing.Point(210, 80)
$btn13.Size = New-Object System.Drawing.Size(170, 48)
$btn13.Font = New-Object System.Drawing.Font("Segoe UI", 9.5)
$btn13.Text = "v1.3 (Prototype)"
$btn13.DialogResult = [System.Windows.Forms.DialogResult]::No
$form.Controls.Add($btn13)

if ("{last_choice}" -eq "v1p3") {{
    $form.AcceptButton = $btn13
    $btn13.Select()
}} else {{
    $form.AcceptButton = $btn14
    $btn14.Select()
}}

$result = $form.ShowDialog()
if ($result -eq [System.Windows.Forms.DialogResult]::Yes) {{
    [Console]::Out.Write("v1p4")
}} elseif ($result -eq [System.Windows.Forms.DialogResult]::No) {{
    [Console]::Out.Write("v1p3")
}} else {{
    [Console]::Out.Write("cancel")
}}
"""
    try:
        proc = subprocess.Popen(
            ["powershell", "-NoProfile", "-NonInteractive", "-Command", ps_script],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        stdout, _ = proc.communicate()
        res = stdout.strip()
        if res in ["v1p4", "v1p3", "cancel"]:
            return res
    except Exception as e:
        print(f"[NX-ISD] GUI prompt failed: {e}")
    return "v1p4"


# Main logic
cache_path = os.path.join(env.get("PROJECT_DIR"), ".pio_hw_target")

# Check if already resolved in current SCons session
target_version = env.get(_CACHE_KEY)

if not target_version:
    # Allow override via environment variable if running headless (e.g. CI or terminal script)
    env_override = os.environ.get("NX_ISD_HW")
    if env_override in ["v1p4", "v1p3"]:
        target_version = env_override
        print(f"[NX-ISD] Hardware target set via environment variable: {target_version}")
    else:
        last = get_last_choice(cache_path)
        chosen = prompt_user_gui(last)
        if chosen == "cancel":
            print("\n" + "=" * 60)
            print(" [NX-ISD] Build aborted by user in target selection dialog.")
            print("=" * 60 + "\n")
            sys.exit(1)
        target_version = chosen
        save_choice(cache_path, target_version)

    env[_CACHE_KEY] = target_version

# Apply build define
if target_version == "v1p3":
    hw_define = "NX_ISD_HW_V1P3"
    desc = "v1.3 (Prototype Board - Shared SPI / Legacy Pinout)"
else:
    hw_define = "NX_ISD_HW_V1P4"
    desc = "v1.4 (Production Board - Dedicated Display SPI / New Pinout)"

env.Append(CPPDEFINES=[(hw_define, "1")])

print("\n" + "=" * 70)
print(f" [NX-ISD] TARGET HARDWARE : {desc}")
print(f" [NX-ISD] COMPILER FLAG   : -D {hw_define}=1")
print("=" * 70 + "\n")
