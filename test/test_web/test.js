function check(value, message) { if (!value) throw new Error(message); }
function pause(ms) { return new Promise(function(resolve) { setTimeout(resolve, ms || 40); }); }
function edit(id, value) {
  var input = document.getElementById(id);
  input.value = value;
  input.dispatchEvent(new Event('input', {bubbles:true}));
  input.dispatchEvent(new Event('change', {bubbles:true}));
}
async function tests() {
  await pause(100);
  check(g_haveSettings && g_profiles.length === 6, 'Initial settings and profiles load');
  var ids = Array.from(document.querySelectorAll('[id]')).map(function(e) { return e.id; });
  check(ids.length === new Set(ids).size, 'Unique control IDs');
  check(document.getElementById('custom-turn-fields').hidden, 'Custom timing starts hidden');
  var posts = calls.filter(function(c) { return c.method === 'POST'; }).length;
  edit('brake_speed', 160);
  await pause();
  check(calls.filter(function(c) { return c.method === 'POST'; }).length === posts, 'Editing must not auto-save/apply');
  await saveSettings(false); await pause();
  check(current.brake_speed === 160 && saved.brake_speed === 100, 'Apply changes runtime only');
  check(g_pendingSettings && !g_formDirty, 'Applied state is displayed');
  edit('brake_speed', 200);
  await revertSettings(); await pause();
  check(current.brake_speed === 100 && +document.getElementById('brake_speed').value === 100, 'Revert restores saved values');
  edit('turn_custom', 1); edit('turn_sweep_ms', 120); edit('turn_hold_ms', 250); edit('turn_off_ms', 430);
  check(document.getElementById('turn_blink_ms').disabled && !document.getElementById('custom-turn-fields').hidden, 'Custom timing toggles controls');
  check(document.getElementById('turn-timing-summary').textContent.includes('800 ms'), 'Timing summary adds all three phases');
  await saveSettings(true); await pause();
  check(saved.turn_custom === 1 && saved.turn_hold_ms === 250 && !g_pendingSettings, 'Save persists custom timing');
  edit('brake_speed', 170);
  document.getElementById('profile-name').value = 'Cruise';
  await profileAction('save'); await pause();
  check(storedProfiles[0].settings.brake_speed === 170, 'Profile captures staged form');
  check(current.brake_speed === 100 && saved.brake_speed === 100, 'Saving profile does not apply or change startup');
  check(!('ap_pass' in storedProfiles[0].settings) && !('show_mode' in storedProfiles[0].settings), 'Profile excludes WiFi and runtime overrides');
  await profileAction('load'); await pause();
  check(current.brake_speed === 170 && saved.brake_speed === 100, 'Loading profile is temporary');
  check(+document.getElementById('brake_speed').value === 170, 'Loaded profile updates form');
  await profileAction('delete'); await pause();
  check(!storedProfiles[0], 'Delete removes profile');
  edit('brake_speed', 150);
  failNext = true;
  await saveSettings(true); await pause();
  check(g_formDirty && saved.brake_speed === 100, 'Failed save preserves edits');
  check(document.getElementById('toast').textContent === 'Storage unavailable', 'Server errors are shown');
  postDelay = 100;
  var saving = saveSettings(true);
  edit('brake_speed', 180);
  await saving; await pause();
  check(saved.brake_speed === 150 && +document.getElementById('brake_speed').value === 180 && g_formDirty, 'Edits during save survive response and refresh');
  // Firmware updates must not use the four-second settings request deadline.
  function firmwareFile(valid) {
    var bytes = new Uint8Array(512);
    if (valid) { bytes[0]=0xe9; bytes[12]=9; bytes[32]=0x32; bytes[33]=0x54; bytes[34]=0xcd; bytes[35]=0xab; }
    var transfer = new DataTransfer();
    transfer.items.add(new File([bytes], 'firmware.bin', {type:'application/octet-stream'}));
    document.getElementById('fw-file').files = transfer.files;
    document.getElementById('fw-password').value = 'test-password';
  }
  firmwareFile(false);
  check(await uploadFirmware() === false && firmwareRequests.length === 0, 'Invalid image blocked before upload');
  check(document.getElementById('fw-status').textContent.includes('application firmware.bin'), 'Bad header gets an image error');
  firmwareFile(true);
  firmwareInfo.inputs_active = true;
  check(await uploadFirmware() === false && firmwareRequests.length === 0, 'Active inputs block browser upload');
  firmwareInfo.inputs_active = false;
  var updating = uploadFirmware();
  check(await uploadFirmware() === false, 'Duplicate firmware start blocked during preflight');
  await pause(50);
  check(g_firmwareUploading && document.getElementById('save-btn').disabled, 'Sending 100% must not report success before reboot: busy=' + g_firmwareUploading + ', disabled=' + document.getElementById('save-btn').disabled + ', status=' + document.getElementById('fw-status').textContent);
  check(await updating === true && !g_firmwareUploading, 'Update waits for a different boot ID');
  check(document.getElementById('fw-status').textContent.includes('back online'), 'Restart confirmation shown');
  check(firmwareRequests[0].headers['X-Firmware-Size'] === '512' && firmwareRequests[0].headers.Authorization.startsWith('Basic '), 'Upload uses authentication and exact size');
  check(document.getElementById('fw-password').value === '', 'Password cleared after sending');
  firmwareFile(true); firmwareUploadMode = 'reject';
  check(await uploadFirmware() === false && !g_firmwareUploading, 'Rejected firmware restores controls');
  check(document.getElementById('fw-status').textContent.includes('Incorrect'), 'Upload rejection reason shown');
  firmwareFile(true); firmwareUploadMode = 'disconnect';
  check(await uploadFirmware() === false && !g_firmwareUploading, 'Disconnect verifies status and restores controls');
  firmwareFile(true); firmwareUploadMode = 'lost-ack';
  check(await uploadFirmware() === false && !g_firmwareUploading, 'Reboot without an acknowledged upload must not claim success');
  check(document.getElementById('fw-status').textContent.includes('not acknowledged'), 'Ambiguous upload outcome is explicit');
  check(testErrors.length === 0, 'No browser script errors: ' + testErrors.join(', '));
  check(innerWidth === 390, 'Test viewport must be exactly 390px');
  check(document.documentElement.scrollWidth <= innerWidth, 'No horizontal overflow at phone width');
  document.querySelectorAll('.mode-btn').forEach(function(button) {
    button.click();
    check(document.documentElement.scrollWidth <= innerWidth, 'No horizontal overflow on ' + button.dataset.tab);
  });
  document.querySelector('[data-tab=display]').click();
  document.getElementById('test-result').textContent = 'PASS: Apply, Save, Revert, profiles, turn timing, error handling, edit races, OTA success/rejection/disconnect, mobile layout';
}
window.addEventListener('load', function() { tests().catch(function(e) { document.getElementById('test-result').textContent = 'FAIL: ' + e.message; }).finally(function() { parent.postMessage(document.getElementById('test-result').textContent, '*'); }); });
