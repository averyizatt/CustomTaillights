Compile the actual input reader against fake GPIO levels. Tests cover all six
PCB connectors, active-LOW polarity, pull-ups, release, debounce, and the
physical-input condition that allows or blocks Wi-Fi previews.

```powershell
$env:PATH = 'C:/msys64/ucrt64/bin;' + $env:PATH
g++ -std=c++17 -Wall -Wextra -Werror -DCUSTOM_TAILLIGHTS_PCB=1 -Itest/test_inputs/stubs -Isrc -Iinclude test/test_inputs/test_inputs.cpp src/inputs.cpp -o .pio/test_inputs.exe
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed' }
& .pio/test_inputs.exe
if ($LASTEXITCODE -ne 0) { throw 'Input tests failed' }
```
