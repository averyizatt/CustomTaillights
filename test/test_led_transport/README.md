Compile the real SPI3 DMA transport against small ESP-IDF/FastLED adapter fakes.
Tests verify independent GPIO4/GPIO6 packets, unused GPIO5, all 256 encoded byte
values, GRB order, complete 380-pixel packets, LOW reset intervals, DMA alignment,
exclusive pin routing, and bounded waits. They also verify that both the buffer
and transaction descriptor remain untouched during a timeout, then recover after
late completion; initialization/device/clock failures must not abort the program.

```powershell
$env:PATH = 'C:\msys64\ucrt64\bin;' + $env:PATH
g++ -std=c++17 -Wall -Wextra -Werror -DCUSTOM_TAILLIGHTS_PCB=1 -Itest/test_led_transport/stubs -Itest/test_can_runtime/stubs -Isrc -Iinclude test/test_led_transport/test_led_transport.cpp src/led_transport.cpp -o .pio/test_led_transport.exe
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed' }
& .pio/test_led_transport.exe
if ($LASTEXITCODE -ne 0) { throw 'Transport tests failed' }
& .pio/test_led_transport.exe --init-failure
if ($LASTEXITCODE -ne 0) { throw 'Initialization tests failed' }
& .pio/test_led_transport.exe --device-failure
if ($LASTEXITCODE -ne 0) { throw 'Device initialization tests failed' }
& .pio/test_led_transport.exe --bad-clock
if ($LASTEXITCODE -ne 0) { throw 'Clock validation tests failed' }
```

These tests verify software behavior, not the electrical waveform. Also compile
the PCB firmware and verify that the legacy RMT driver is absent from the ELF.
