Recovery safeguards as pure logic: the boot plan after each kind of restart
(`boot_guard.h`), when the lamps count as delivered or stalled (`output_guard.h`),
the limit on how fast the LED current may rise (`power_ramp.h`), and the second
opinions on brake, reverse and brightness (`fail_safe.h`).

```powershell
$env:PATH = 'C:\msys64\ucrt64\bin;' + $env:PATH
g++ -std=c++17 -Wall -Wextra -Werror -Isrc test/test_boot_safety/test_boot_safety.cpp -o .pio/test_boot_safety.exe
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed' }
& .pio/test_boot_safety.exe
if ($LASTEXITCODE -ne 0) { throw 'Boot safety tests failed' }
```

These tests do not cover the hardware watchdog itself (`hw_watchdog.c`) or the
wiring of the safeguards into `setup()` and `loop()`; those need the board.
