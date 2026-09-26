"""Exercise the PlatformIO upload action without starting a network transfer."""
import contextlib
import io
import os
from pathlib import Path
import runpy
from unittest.mock import patch

root = Path(__file__).resolve().parents[2]
class Platform:
    def get_package_dir(self, name):
        assert name == "framework-arduinoespressif32"
        return "C:/Toolchain With Spaces/framework"
class Environment:
    def Replace(self, **kwargs): self.action = kwargs["UPLOADCMD"]
    def PioPlatform(self): return Platform()
    def subst(self, value):
        assert value == "$UPLOAD_PORT"
        return "192.168.4.1"
env = Environment()
runpy.run_path(str(root / "scripts/ota_upload.py"), init_globals={"env": env, "Import": lambda _: None})
secret = 'spaces and $() ` & ! % characters'
with patch.dict(os.environ, {"FOXBODY_OTA_PASSWORD": secret}), patch("subprocess.call", return_value=0) as call:
    output = io.StringIO()
    with contextlib.redirect_stdout(output):
        assert env.action(["firmware with spaces.bin"], [], env) == 0
    args = call.call_args.args[0]
    assert args[args.index("--auth") + 1] == secret
    assert args[-1] == "firmware with spaces.bin"
    assert args[args.index("--port") + 1] == "3232"
    assert "--debug" not in args and secret not in output.getvalue()
    assert not call.call_args.kwargs.get("shell")
with patch.dict(os.environ, {"FOXBODY_OTA_PASSWORD": ""}), patch("sys.stdin.isatty", return_value=False), patch("subprocess.call") as call:
    with contextlib.redirect_stdout(io.StringIO()):
        assert env.action(["firmware.bin"], [], env) == 1
    call.assert_not_called()
print("PlatformIO uploader argument handling, credential privacy, and missing-password rejection passed")
