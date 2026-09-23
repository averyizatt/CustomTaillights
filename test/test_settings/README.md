Compile the actual settings and profile code with in-memory NVS and the installed
ArduinoJson library. Covers reboot/revert, failed writes, all six slots, corrupt
profiles, excluded network/runtime fields, and legacy/custom timing bounds.

```powershell
g++ -std=c++17 -Wall -Wextra -Werror -Itest/test_settings/stubs -Itest/test_matrix_animations/stubs -I.pio/libdeps/esp32-s3-pcb/ArduinoJson/src -Isrc -Iinclude test/test_settings/test_settings.cpp src/settings.cpp src/profiles.cpp -o .pio/test_settings.exe
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed' }
& .pio/test_settings.exe
if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
```
