'use strict';

// Never insert advertiser-controlled strings as HTML. No cookies, local storage,
// third-party assets or credentials in JavaScript; the browser handles TLS auth.
const byId = (id) => document.getElementById(id);
const text = (id, value) => { byId(id).textContent = String(value); };
const seconds = (ms) => `${Math.max(0, Math.floor(ms / 1000))} s`;
let snapshot = null;
let receivedAt = 0;

function render(data, elapsed = 0) {
  const now = data.uptimeMs + elapsed;
  text('wifi-rssi', data.wifi.connected ? `${data.wifi.rssi} dBm` : 'Disconnected');
  text('wifi-detail', data.wifi.connected ? `Scanner IP ${data.wifi.ip}` : 'Waiting for the access point');
  text('scan-status', data.scanner.status);
  text('scan-detail', `${data.scanner.scans} rounds completed · ${data.scanner.reports} reports this round`);
  const phone = data.phone;
  text('phone-status', phone.status);
  const phoneLabel = labelFor('iphone');
  text('phone-heading', phoneLabel ? `${phoneLabel.name}${phoneLabel.room ? ` · ${phoneLabel.room} (manual)` : ''}` : 'Phone observations');
  text('phone-detail', phone.seen ? `Last observed ${seconds(phone.lastSeenAgoMs + elapsed)} ago` : 'No matching advertisement since startup');
  text('phone-mac', phone.latestMatchingMac || '—');
  text('phone-rssi', phone.lastRssi === null ? '—' : `${phone.lastRssi} dBm · historical`);
  text('phone-seen', phone.seen ? `${seconds(phone.lastSeenAgoMs + elapsed)} ago` : 'Never');
  text('phone-uptime', phone.lastSeenUptimeMs === null ? '—' : seconds(phone.lastSeenUptimeMs));
  text('phone-matches', phone.matchingReports);
  text('phone-changes', phone.addressChanges);
  const rows = [...data.observations];
  const mode = byId('sort').value;
  rows.sort(mode === 'recent' ? (a, b) => b.lastSeenAtMs - a.lastSeenAtMs :
    mode === 'name' ? (a, b) => (labelFor(identityFor(a))?.name || a.name).localeCompare(labelFor(identityFor(b))?.name || b.name) : (a, b) => b.rssi - a.rssi);
  const body = byId('devices');
  body.replaceChildren();
  const types = ['public', 'random', 'public identity', 'random identity'];
  for (const item of rows) {
    const row = document.createElement('tr');
    const identity = identityFor(item);
    const label = labelFor(identity);
    const displayName = label ? `${label.name}${label.room ? ` · ${label.room} (manual)` : ''}` : (item.name || '(not advertised)');
    for (const value of [displayName, item.address,
      types[item.addressType] || `type ${item.addressType}`, `${item.rssi} dBm`,
      `${seconds(now - item.lastSeenAtMs)} ago`]) {
      const cell = document.createElement('td');
      cell.textContent = value;
      row.appendChild(cell);
    }
    const action = document.createElement('td');
    const button = document.createElement('button');
    button.type = 'button';
    button.textContent = label ? 'Edit label' : 'Name device';
    button.addEventListener('click', () => selectLabel(identity));
    action.appendChild(button);
    row.appendChild(action);
    body.appendChild(row);
  }
  byId('empty').hidden = rows.length > 0;
  text('empty', data.scanner.scans ? 'No advertisements retained in the last completed round.' : 'Waiting for the first completed scan.');
  text('scan-age', data.scanner.scans ? `Completed ${seconds(now - data.scanner.snapshotCompletedAtMs)} ago · ${rows.length}/32 retained · ${data.scanner.outsideCapacityReports} reports outside capacity` : 'Waiting for a scan…');
  text('system', `Scanner uptime ${seconds(now)} · free heap ${Math.floor(data.freeHeap / 1024)} KiB · lowest free ${Math.floor(data.minFreeHeap / 1024)} KiB · largest free block ${Math.floor(data.largestFreeBlock / 1024)} KiB`);
}

async function refresh() {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), 6000);
  try {
    const response = await fetch('/api/status', {cache: 'no-store', credentials: 'same-origin', signal: controller.signal});
    if (!response.ok) throw new Error(`Status ${response.status}`);
    const data = await response.json();
    if (data.schemaVersion !== 1 || !Array.isArray(data.observations)) throw new Error('Unsupported status format');
    snapshot = data;
    receivedAt = performance.now();
    render(data);
    text('connection', 'Connected over HTTPS · refreshes every 3 seconds');
    byId('connection').dataset.stale = 'false';
  } catch {
    text('connection', 'Connection lost or access denied. Values below are historical; presence is unconfirmed.');
    byId('connection').dataset.stale = 'true';
    // Never keep a green/present headline when the scanner cannot be reached.
    text('phone-status', 'Unknown · offline');
    text('scan-status', 'Unreachable');
  } finally {
    clearTimeout(timer);
    setTimeout(refresh, 3000); // One request at a time, even on slow TLS/network.
  }
}

byId('sort').addEventListener('change', () => {
  if (!snapshot) return;
  const stale = byId('connection').dataset.stale === 'true';
  render(snapshot, performance.now() - receivedAt);
  if (stale) { text('phone-status', 'Unknown · offline'); text('scan-status', 'Unreachable'); }
});
refresh();

let savedSettings = null;
let csrfToken = '';
let saving = false;
const settingsMessage = (message) => text('settings-message', message);
const copySettings = () => JSON.parse(JSON.stringify(savedSettings));
const identityFor = (item) => snapshot?.phone.latestMatchingMac && item.addressType === 1 &&
  item.address.toUpperCase() === snapshot.phone.latestMatchingMac.toUpperCase()
  ? 'iphone' : `mac:${item.addressType}:${item.address.toUpperCase()}`;
const labelFor = (identity) => savedSettings?.devices.find((item) => item.identity === identity);

function selectLabel(identity) {
  const label = labelFor(identity);
  byId('label-identity').value = identity;
  byId('label-name').value = label?.name || '';
  byId('label-room').value = label?.room || '';
  byId('label-name').focus();
  byId('label-form').scrollIntoView({behavior: 'smooth', block: 'center'});
}

async function settingsRequest(options = {}) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), 8000);
  try {
    return await fetch('/api/settings', {...options, credentials: 'same-origin', cache: 'no-store', signal: controller.signal});
  } finally { clearTimeout(timer); }
}

async function loadSettings() {
  const response = await settingsRequest();
  if (!response.ok) throw new Error('Saved settings could not be loaded. Reload after checking the connection.');
  const settings = await response.json();
  if (settings.schemaVersion !== 1 || !Array.isArray(settings.devices)) throw new Error('Unsupported saved settings version.');
  savedSettings = settings;
  csrfToken = response.headers.get('X-CSRF-Token') || '';
  byId('setting-name').value = settings.scannerName;
  byId('setting-room').value = settings.scannerRoom;
  byId('setting-timeout').value = settings.phoneTimeoutSeconds;
  const labels = byId('saved-labels');
  labels.replaceChildren();
  for (const label of settings.devices) {
    const item = document.createElement('li');
    const button = document.createElement('button');
    button.type = 'button';
    button.textContent = `${label.name} · ${label.room || 'No room'} · ${label.identity}`;
    button.addEventListener('click', () => selectLabel(label.identity));
    item.appendChild(button);
    labels.appendChild(item);
  }
  text('scanner-name', settings.scannerName);
  text('scanner-room', settings.scannerRoom ? `Manual scanner room: ${settings.scannerRoom}` : 'Scanner room not assigned');
  if (snapshot) {
    const stale = byId('connection').dataset.stale === 'true';
    render(snapshot, performance.now() - receivedAt);
    if (stale) { text('phone-status', 'Unknown · offline'); text('scan-status', 'Unreachable'); }
  }
}

async function saveSettings(next) {
  if (saving) throw new Error('A save is already in progress.');
  if (!savedSettings || !csrfToken) throw new Error('Reload the dashboard before changing settings.');
  const body = JSON.stringify(next);
  if (new TextEncoder().encode(body).length > 8192) throw new Error('Settings must contain at most 8 KiB.');
  saving = true;
  settingsMessage('Saving to the ESP32…');
  try {
    const response = await settingsRequest({method: 'POST',
      headers: {'Content-Type': 'application/json', 'X-CSRF-Token': csrfToken}, body});
    if (!response.ok) throw new Error(await response.text());
    await loadSettings();
    settingsMessage('Saved on the ESP32. Download a backup before a full flash erase.');
  } finally { saving = false; }
}

byId('label-phone').addEventListener('click', () => selectLabel('iphone'));
byId('scanner-form').addEventListener('submit', async (event) => {
  event.preventDefault();
  try {
    const next = copySettings();
    if (!next) throw new Error('Wait for saved settings to load.');
    next.scannerName = byId('setting-name').value;
    next.scannerRoom = byId('setting-room').value;
    next.phoneTimeoutSeconds = Number(byId('setting-timeout').value);
    await saveSettings(next);
  } catch (error) { settingsMessage(error.message); }
});
byId('label-form').addEventListener('submit', async (event) => {
  event.preventDefault();
  try {
    const next = copySettings();
    if (!next) throw new Error('Wait for saved settings to load.');
    const identity = byId('label-identity').value;
    if (!identity) throw new Error('Choose a Bluetooth device or the iPhone first.');
    const label = {identity, name: byId('label-name').value, room: byId('label-room').value};
    const index = next.devices.findIndex((item) => item.identity === identity);
    if (index < 0) next.devices.push(label); else next.devices[index] = label;
    await saveSettings(next);
  } catch (error) { settingsMessage(error.message); }
});
byId('remove-label').addEventListener('click', async () => {
  try {
    const next = copySettings();
    if (!next) throw new Error('Wait for saved settings to load.');
    next.devices = next.devices.filter((item) => item.identity !== byId('label-identity').value);
    await saveSettings(next);
    byId('label-name').value = '';
    byId('label-room').value = '';
  } catch (error) { settingsMessage(error.message); }
});
byId('download-settings').addEventListener('click', async () => {
  try {
    await loadSettings();
    const blob = new Blob([JSON.stringify(savedSettings, null, 2) + '\n'], {type: 'application/json'});
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.download = 'ble-tracker-settings.json';
    link.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
    settingsMessage('Downloaded settings. Network credentials and IRKs are excluded.');
  } catch (error) { settingsMessage(error.message); }
});
byId('import-settings').addEventListener('change', async (event) => {
  try {
    const file = event.target.files[0];
    if (!file) return;
    if (file.size > 8192) throw new Error('Import must contain at most 8 KiB.');
    const next = JSON.parse(await file.text());
    if (next.schemaVersion !== 1 || !Array.isArray(next.devices)) throw new Error('Unsupported settings format. Nothing saved.');
    if (!confirm(`Replace scanner settings and ${savedSettings?.devices.length || 0} saved labels with this file (${next.devices.length} labels)?`)) return;
    await saveSettings(next);
  } catch (error) { settingsMessage(`Import failed: ${error.message}`); }
  finally { event.target.value = ''; }
});
loadSettings().then(() => settingsMessage('Settings loaded from the ESP32.')).catch((error) => settingsMessage(error.message));
