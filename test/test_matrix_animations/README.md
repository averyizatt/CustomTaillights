These tests compile the actual new animation implementations and `TailLight`
pixel mapping with a small Arduino clock/RGB stub. They verify pixel bounds,
red-diffuser filtering, passenger mirroring, running brightness caps, instant
brake illumination, outward turn direction, off intervals, and independent side
timing. They also verify solid lower brake sections throughout BRAKE_TURN.

From PowerShell with MSYS2 UCRT64 on PATH:

```powershell
g++ -std=c++17 -Wall -Wextra -Wno-unused-parameter -Itest/test_matrix_animations/stubs -Isrc -Iinclude test/test_matrix_animations/test_matrix_animations.cpp src/matrix_animations.cpp src/taillight.cpp -o .pio/test_matrix_animations.exe
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed' }
& .pio/test_matrix_animations.exe
if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
python test/test_matrix_animations/make_preview.py
```

Open `.pio/new-animations-preview.html` for an animated schematic of both lamps.
This preview uses captured firmware frames, with an exposure control for screen
visibility. It does not simulate the physical lens or electrical power limits.
