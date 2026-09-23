Host-side checks exercise the same image validator used by both OTA transports.
They reject wrong-chip, wrong-target, bootloader/factory, truncated-header, and
oversized input and test target markers split across transfer chunks.

```powershell
g++ -std=c++17 -Wall -Wextra -Werror -DCUSTOM_TAILLIGHTS_PCB=1 -Isrc test/test_firmware/test_firmware_image.cpp -o .pio/test_firmware_image.exe
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed' }
& .pio/test_firmware_image.exe .pio/build/esp32-s3-pcb/firmware.bin
if ($LASTEXITCODE -ne 0) { throw 'Firmware image tests failed' }
python test/test_firmware/test_uploader.py
```

The uploader test mocks process launch; it never sends firmware. Browser OTA
success, rejection, active-input, duplicate-request and disconnect flows are
covered by `test/test_web/run.py` using mocked HTTP/XHR. Hardware validation still
needs real uploads through both transports, interrupted transfer recovery,
watchdog behavior, and physical input/output checks on the controller.
