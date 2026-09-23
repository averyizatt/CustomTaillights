window.confirm = function() { return true; };
Object.defineProperty(document, 'hidden', {value: false});
window.testErrors = [];
window.addEventListener('error', function(e) { testErrors.push(e.message); });
window.addEventListener('unhandledrejection', function(e) { testErrors.push(String(e.reason)); });
var firmwareInfo = {available:true, busy:false, inputs_active:false, boot_id:101, target:'FOXBODY-ESP32S3-PCB-OTA-V1', build:'test build', max_size:3342336, error:''};
var firmwareUploadMode = 'success', firmwareRequests = [];
var current, saved, calls = [], storedProfiles = new Array(6).fill(null), failNext = false, postDelay = 0;
function copy(value) { return JSON.parse(JSON.stringify(value)); }
window.fetch = function(url, options) {
  var method = options && options.method || 'GET';
  var body = options && options.body ? JSON.parse(options.body) : {};
  calls.push({url: url, method: method, body: copy(body)});
  if (!current) { current = collectSettings(); saved = copy(current); }
  var result = {ok: true}, status = 200;
  if (method === 'POST' && failNext) { failNext = false; status = 500; result = {error: 'Storage unavailable'}; }
  else if (url === '/api/settings') {
    if (method === 'POST') {
      Object.keys(body).forEach(function(key) { if (key !== 'persist') current[key] = body[key]; });
      if (body.persist) saved = copy(current);
    } else result = Object.assign(copy(current), {settings_pending: JSON.stringify(current) !== JSON.stringify(saved)});
  } else if (url === '/api/firmware') { result = copy(firmwareInfo); }
  else if (url === '/api/revert') { current = copy(saved); }
  else if (url === '/api/profiles') {
    if (method === 'GET') result = {profiles: storedProfiles.map(function(p, slot) { return {slot: slot, name: p ? p.name : '', occupied: !!p}; })};
    else if (body.action === 'save') storedProfiles[body.slot] = {name: body.name, settings: copy(body.settings)};
    else if (body.action === 'load') Object.assign(current, copy(storedProfiles[body.slot].settings));
    else if (body.action === 'delete') storedProfiles[body.slot] = null;
  }
  var response = new Response(JSON.stringify(result), {status: status, headers: {'Content-Type':'application/json'}});
  var delay = method === 'POST' ? postDelay : 0;
  postDelay = 0;
  return new Promise(function(resolve) { setTimeout(function() { resolve(response); }, delay); });
};

window.XMLHttpRequest = function() {
  var self = this;
  this.upload = {}; this.headers = {};
  this.open = function(method,url) { self.method=method; self.url=url; };
  this.setRequestHeader = function(key,value) { self.headers[key]=value; };
  this.send = function(body) {
    firmwareRequests.push({url:self.url, headers:copy(self.headers), name:body.get('firmware').name});
    setTimeout(function() {
      self.upload.onprogress({lengthComputable:true, loaded:512, total:512});
      if (firmwareUploadMode === 'lost-ack') { firmwareInfo.error=''; firmwareInfo.boot_id++; self.onerror(); return; }
      if (firmwareUploadMode === 'disconnect') {
        firmwareInfo.error = 'Upload disconnected'; self.onerror(); return;
      }
      self.status = firmwareUploadMode === 'reject' ? 401 : 200;
      self.responseText = JSON.stringify(self.status === 200 ? {ok:true,rebooting:true} : {error:'Incorrect controller WiFi password'});
      self.onload();
      if (self.status === 200) setTimeout(function() { firmwareInfo.boot_id++; }, 20);
    }, 30);
  };
};
