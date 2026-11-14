'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

class Node {
  constructor() { this.children = []; this.dataset = {}; this.handlers = {}; this.textContent = ''; }
  set innerHTML(_) { throw new Error('Advertiser-controlled content must not use HTML insertion'); }
  replaceChildren() { this.children = []; }
  appendChild(child) { this.children.push(child); }
  addEventListener(name, callback) { this.handlers[name] = callback; }
  focus() {}
  scrollIntoView() {}
  click() {}
}

(async () => {
  const root = path.resolve(__dirname, '..');
  const html = fs.readFileSync(path.join(root, 'web/dashboard.html'), 'utf8');
  const nodes = Object.fromEntries([...html.matchAll(/id="([^"]+)"/g)].map((match) => [match[1], new Node()]));
  nodes.sort.value = 'strongest';
  const malicious = '<img src=x onerror=alert(1)>';
  const payload = {
    schemaVersion: 1, uptimeMs: 15000, freeHeap: 90000, minFreeHeap: 85000, largestFreeBlock: 70000,
    wifi: {connected: true, ip: '192.0.2.10', rssi: -70},
    scanner: {status: 'listening', scans: 2, reports: 50, snapshotCompletedAtMs: 14000, outsideCapacityReports: 3},
    phone: {status: 'present', seen: true, latestMatchingMac: '70:81:94:0D:FB:AA', lastSeenAgoMs: 1000,
      lastSeenUptimeMs: 14000, lastRssi: -44, matchingReports: 5, addressChanges: 1},
    observations: [
      {name: malicious, address: '70:81:94:0D:FB:AA', addressType: 1, rssi: -80, lastSeenAtMs: 14000},
      {name: 'Test beacon', address: '41:22:33:44:55:66', addressType: 1, rssi: -35, lastSeenAtMs: 12000},
    ],
  };
  let fail = false;
  let clock = 100;
  let settings = {schemaVersion: 1, scannerName: 'Room scanner', scannerRoom: 'Living room', phoneTimeoutSeconds: 120, devices: []};
  let posted = null;
  let postedHeaders = null;
  let download = null;
  let confirmed = 0;
  const context = vm.createContext({
    document: {getElementById: (id) => { assert(nodes[id], `Missing element ${id}`); return nodes[id]; },
      createElement: () => new Node()},
    performance: {now: () => clock}, AbortController, TextEncoder, Blob,
    URL: {createObjectURL: (blob) => { download = blob; return 'blob:test'; }, revokeObjectURL: () => {}},
    confirm: () => { ++confirmed; return true; },
    setTimeout: () => 1, clearTimeout: () => {},
    fetch: async (url, options = {}) => {
      if (url === '/api/settings') {
        if (options.method === 'POST') {
          posted = JSON.parse(options.body); postedHeaders = options.headers; settings = posted;
          return {ok: true};
        }
        return {ok: true, json: async () => settings, headers: {get: () => 'test-csrf-token'}};
      }
      return {ok: !fail, status: fail ? 401 : 200, json: async () => payload};
    },
  });
  vm.runInContext(fs.readFileSync(path.join(root, 'web/dashboard.js'), 'utf8'), context);
  await new Promise(setImmediate);
  assert.equal(nodes['phone-status'].textContent, 'present');
  assert.equal(nodes['wifi-rssi'].textContent, '-70 dBm');
  assert.equal(nodes.devices.children[0].children[0].textContent, 'Test beacon');
  assert.equal(nodes.devices.children[1].children[0].textContent, malicious);
  nodes.sort.value = 'recent';
  nodes.sort.handlers.change();
  assert.equal(nodes.devices.children[0].children[0].textContent, malicious);
  clock = 5100;
  fail = true;
  await vm.runInContext('refresh()', context);
  assert.equal(nodes.connection.dataset.stale, 'true');
  assert.equal(nodes['phone-status'].textContent, 'Unknown · offline');
  nodes.sort.handlers.change();
  assert.equal(nodes['phone-status'].textContent, 'Unknown · offline');
  assert.equal(nodes['scan-status'].textContent, 'Unreachable');
  assert.equal(nodes['phone-seen'].textContent, '6 s ago');
  fail = false;
  payload.observations = [];
  payload.phone.status = 'not seen recently';
  await vm.runInContext('refresh()', context);
  assert.equal(nodes['phone-status'].textContent, 'not seen recently');
  assert.equal(nodes.empty.hidden, false);
  assert.equal(nodes.devices.children.length, 0);
  assert.equal(nodes.connection.dataset.stale, 'false');
  nodes['label-identity'].value = 'iphone';
  nodes['label-name'].value = malicious;
  nodes['label-room'].value = 'Living room';
  await nodes['label-form'].handlers.submit({preventDefault() {}});
  assert.equal(posted.devices[0].identity, 'iphone');
  assert.equal(postedHeaders['X-CSRF-Token'], 'test-csrf-token');
  assert.equal(postedHeaders['Content-Type'], 'application/json');
  assert.equal(nodes['phone-heading'].textContent, `${malicious} · Living room (manual)`);
  assert.equal(nodes['saved-labels'].children.length, 1);
  await nodes['download-settings'].handlers.click();
  const backup = JSON.parse(await download.text());
  assert.equal(backup.devices[0].name, malicious);
  assert.equal(backup.csrfToken, undefined);
  assert.equal(backup.WIFI_PASSWORD, undefined);
  const before = posted;
  await nodes['import-settings'].handlers.change({target: {files: [{size: 2, text: async () => '{}'}]}});
  assert.equal(posted, before);
  assert.match(nodes['settings-message'].textContent, /Unsupported settings/);
  const replacement = {...settings, scannerName: 'Bedroom scanner', devices: []};
  await nodes['import-settings'].handlers.change({target: {files: [{size: 200, text: async () => JSON.stringify(replacement)}]}});
  assert.equal(confirmed, 1);
  assert.equal(posted.scannerName, 'Bedroom scanner');
  assert.equal(posted.devices.length, 0);
  console.log('PASS: dashboard sorting, safe text, offline recovery, CSRF saves and backup/import UI');
})().catch((error) => { console.error(error); process.exitCode = 1; });
