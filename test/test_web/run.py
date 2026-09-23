from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[2]
source = (root / 'src/wifi_server.cpp').read_text(encoding='utf-8')
html = source.split('R"rawhtml(', 1)[1].split(')rawhtml"', 1)[0]
folder = root / '.pio/web-test'
folder.mkdir(parents=True, exist_ok=True)
mock = (root / 'test/test_web/mock.js').read_text(encoding='utf-8')
tests = (root / 'test/test_web/test.js').read_text(encoding='utf-8')
if '--firmware-view' in sys.argv:
    tests = tests.replace("document.querySelector('[data-tab=display]').click();", "document.querySelector('[data-tab=network]').click(); window.scrollTo(0, document.getElementById('firmware-card').getBoundingClientRect().top + window.scrollY - 180);")
html = html.replace('<script>', '<script>' + mock + '</script><script>', 1)
html = html.replace('</body>', '<pre hidden id="test-result">RUNNING</pre><script>' + tests + '</script></body>')
page = folder / 'test.html'
page.write_text(html, encoding='utf-8')
wrapper = folder / 'viewport.html'
wrapper.write_text('''<!DOCTYPE html><html><head><style>html,body{margin:0;padding:0}iframe{border:0;width:390px;height:844px;display:block}</style></head><body><iframe src="test.html"></iframe><pre hidden id="test-result">RUNNING</pre><script>window.addEventListener('message',function(e){document.getElementById('test-result').textContent=e.data;});</script></body></html>''', encoding='utf-8')
chrome = Path('C:/Program Files/Google/Chrome/Application/chrome.exe')
command = [str(chrome), '--headless=new', '--disable-gpu', '--no-first-run', '--no-default-browser-check',
           '--disable-background-networking', '--disable-extensions', '--allow-file-access-from-files',
           '--user-data-dir=' + str(folder / 'browser-data'), '--window-size=390,844',
           '--virtual-time-budget=12000', '--screenshot=' + str(folder / 'mobile.png'), '--dump-dom', wrapper.as_uri()]
result = subprocess.run(command, capture_output=True, timeout=60, creationflags=subprocess.CREATE_NO_WINDOW)
output = result.stdout.decode('utf-8', errors='replace')
(folder / 'result.html').write_text(output, encoding='utf-8')
import re
match = re.search(r'<pre[^>]*id="test-result"[^>]*>([^<]+)</pre>', output)
print(match.group(1) if match else 'Browser did not return test results: ' + result.stderr.decode('utf-8', errors='replace')[-1200:])
sys.exit(0 if match and match.group(1).startswith('PASS:') else 1)
