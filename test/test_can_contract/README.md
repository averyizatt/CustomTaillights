These native tests check the firmware state encoder against the schema 2 shared
CCM decoder and fixed wire bytes, temperature boundaries, raw derating and both input sets,
and CAN brightness persistence across repeated requests. They do not require an
ESP32 or CAN controller.

Run from the project root with a C++17 compiler and assertions enabled:

```sh
mkdir -p .pio
g++ -std=c++17 -Wall -Wextra -Werror -Isrc -Iinclude test/test_can_contract/test_can_contract.cpp -o .pio/test_can_contract
.pio/test_can_contract
```

With MSYS2 UCRT64 installed on Windows, use PowerShell:

```powershell
New-Item -ItemType Directory -Force .pio | Out-Null
$env:PATH = 'C:\msys64\ucrt64\bin;' + $env:PATH
g++ -std=c++17 -Wall -Wextra -Werror -Isrc -Iinclude test/test_can_contract/test_can_contract.cpp -o .pio/test_can_contract.exe
if ($LASTEXITCODE -ne 0) { throw 'CAN contract test compilation failed' }
& .\.pio\test_can_contract.exe
if ($LASTEXITCODE -ne 0) { throw 'CAN contract tests failed' }
```
