"""Use the bundled espota client without exposing the password in build commands."""
import getpass
import os
from pathlib import Path
import subprocess
import sys

Import("env")

def upload_firmware(source, target, env):
    password = os.environ.get("FOXBODY_OTA_PASSWORD", "")
    if not password:
        if not sys.stdin.isatty():
            print("Set FOXBODY_OTA_PASSWORD to the controller AP password, then run Upload again.")
            return 1
        password = getpass.getpass("Controller WiFi password: ")
    if not password:
        print("An OTA password is required.")
        return 1
    framework = Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32"))
    # Direct arguments keep special characters literal and credentials out of
    # SCons command output. The bundled client matches this core's authentication.
    return subprocess.call([
        sys.executable, str(framework / "tools" / "espota.py"),
        "--progress", "--ip", env.subst("$UPLOAD_PORT"), "--port", "3232",
        "--auth", password, "--file", str(source[0]),
    ])

env.Replace(UPLOADCMD=upload_firmware)
