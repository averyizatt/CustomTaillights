"""Restore the WiFi SDK include path if the installed framework omits it."""

from pathlib import Path

Import("env")

platform = env.PioPlatform()
mcu = env.BoardConfig().get("build.mcu")
sdk_dir = platform.get_package_dir("framework-arduinoespressif32-libs")
if sdk_dir:
    wifi_include = Path(sdk_dir) / mcu / "include" / "esp_wifi" / "include"
else:
    # Arduino 2.x bundles the SDK inside the framework package.
    framework_dir = platform.get_package_dir("framework-arduinoespressif32")
    wifi_include = Path(framework_dir) / "tools" / "sdk" / mcu / "include" / "esp_wifi" / "include"

if not (wifi_include / "esp_wifi_types.h").is_file():
    raise RuntimeError("WiFi SDK header is missing; reinstall the Arduino framework packages.")

env.AppendUnique(CPPPATH=[str(wifi_include)])
# Newer SDKs keep native radio types in a separate include directory.
native_include = wifi_include / "local"
if native_include.is_dir():
    env.AppendUnique(CPPPATH=[str(native_include)])
