These tests compile the real CAN service and command parser against a fake
MCP2515/SPI register model. They cover a missing controller, one-shot readback,
asynchronous missing-ACK failures, bounded retry/logging, reconnection, stuck TX,
fault-frame backoff, bus-off recovery, and bounded RX processing.

From PowerShell with MSYS2 UCRT64 on PATH:

```powershell
g++ -std=c++17 -Wall -Wextra -Werror -Wno-unused-parameter -Itest/test_can_runtime/stubs -Isrc -Iinclude test/test_can_runtime/test_can_runtime.cpp src/canbus.cpp -o .pio/test_can_runtime.exe
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed' }
& .pio/test_can_runtime.exe
if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
```

The fake models register behavior, not electrical signaling. An ESP32 firmware
build is also required; physical LED output still needs confirmation on the unit.
