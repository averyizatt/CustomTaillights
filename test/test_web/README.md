Run `python test/test_web/run.py` from the project root on Windows with Chrome
installed at its standard Program Files location. The harness extracts the
actual embedded page, mocks controller HTTP responses, and runs headless Chrome
with an isolated profile under `.pio/web-test`. No controller is contacted.

Checks staged edits, Apply versus Save, Revert, profile save/load/delete,
credential exclusion, custom timing summaries, failed saves, and edits during a
request. OTA scenarios cover image/header rejection, active inputs, password
errors, duplicate starts, upload progress, disconnects, and reboot confirmation.
All controller traffic, including firmware upload, is mocked. The rendered test result is in `.pio/web-test/result.html`.
