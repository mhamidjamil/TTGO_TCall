import { initializeApp } from 'https://www.gstatic.com/firebasejs/10.12.5/firebase-app.js';
import { getAuth, onAuthStateChanged, signInAnonymously } from 'https://www.gstatic.com/firebasejs/10.12.5/firebase-auth.js';
import {
  addDoc,
  collection,
  doc,
  getDoc,
  getDocs,
  getFirestore,
  limit,
  query,
  serverTimestamp,
  setDoc,
  updateDoc
} from 'https://www.gstatic.com/firebasejs/10.12.5/firebase-firestore.js';
import {
  get,
  getDatabase,
  ref,
  update
} from 'https://www.gstatic.com/firebasejs/10.12.5/firebase-database.js';

const paths = {
  configDoc: 'sim_module/config',
  deviceDoc: 'sim_module/device',
  allowedNumbers: 'sim_module/allowed_numbers/items',
  smsJobs: 'sim_module/sms_jobs/items',
  callJobs: 'sim_module/call_jobs/items',
  smsLogs: 'sim_module/sms_logs/items',
  callLogs: 'sim_module/call_logs/items',
  runtime: 'ttgo_tcall/settings/runtime'
};

const state = {
  db: null,
  rtdb: null,
  deviceId: 'device_001',
  countryCode: '92',
  activeView: 'devices',
  buildVersion: 'unknown',
  buildDate: 'unknown',
  config: {},
  device: {},
  runtime: {}
};

const $ = (selector) => document.querySelector(selector);

// Must stay in lockstep with normalizePhoneNumber() in sms_calls.ino. The device
// looks up allowed_numbers under safeId(normalizePhone(number)); if the dashboard
// stored a number in a different format the lookup misses and the job is wrongly
// rejected as "number_not_allowed". Defaults to Pakistan (92); the device's
// defaultCountryCode is surfaced via /api/firebase-web-config when available.
function normalizePhone(raw) {
  let digits = String(raw || '').replace(/[^0-9]/g, '');
  if (!digits) return '';
  const cc = state.countryCode || '92';
  if (digits.startsWith('00')) digits = digits.slice(2);
  if (digits.startsWith('0') && digits.length === 11) digits = cc + digits.slice(1);
  if (digits.length === 10) digits = cc + digits;
  if (digits.length < 12) return '';
  return '+' + digits;
}

// Canonical form used for both the stored value and the Firestore document id.
// Falls back to the raw text for alphanumeric sender ids that cannot normalize.
function canonicalNumber(raw) {
  return normalizePhone(raw) || String(raw || '').trim();
}

function safeId(value) {
  const id = String(value || '').replace(/[^A-Za-z0-9+_-]/g, '_').trim();
  return id && id !== '.' && id !== '..' ? id : 'unknown';
}

function text(value, fallback = '') {
  return value === undefined || value === null || value === '' ? fallback : String(value);
}

function epochFromTimestamp(value) {
  if (!value) return 0;
  if (typeof value.toMillis === 'function') return Math.floor(value.toMillis() / 1000);
  return Number(value) || 0;
}

function linesToArray(value) {
  return String(value || '').split(/\r?\n/).map((line) => line.trim()).filter(Boolean);
}

function arrayToLines(value) {
  return Array.isArray(value) ? value.join('\n') : '';
}

function statusPill(status) {
  const normalized = text(status, 'pending');
  return `<span class="pill ${normalized}">${normalized}</span>`;
}

function actionButton(label, action, id, phone = '') {
  return `<button class="mini" data-action="${action}" data-id="${id}" data-phone="${phone}" type="button">${label}</button>`;
}

function table(headers, rows) {
  if (!rows.length) {
    return '<div class="empty">No records</div>';
  }
  const head = headers.map((item) => `<th>${item}</th>`).join('');
  const body = rows.map((row) => `<tr>${row.map((cell) => `<td>${cell}</td>`).join('')}</tr>`).join('');
  return `<table><thead><tr>${head}</tr></thead><tbody>${body}</tbody></table>`;
}

function parseVersionText(raw) {
  const lines = String(raw || '').split(/\r?\n/).map((line) => line.trim()).filter(Boolean);
  if (!lines.length) return { version: 'unknown', date: 'unknown' };
  if (lines.length === 1) {
    const [version, date] = lines[0].split('|').map((part) => part.trim());
    return { version: version || 'unknown', date: date || 'unknown' };
  }
  return { version: lines[0] || 'unknown', date: lines[1] || 'unknown' };
}

async function loadVersionMeta() {
  try {
    const response = await fetch('/version.txt', { cache: 'no-store' });
    if (!response.ok) throw new Error('version file unavailable');
    const meta = parseVersionText(await response.text());
    state.buildVersion = meta.version;
    state.buildDate = meta.date;
  } catch {
    state.buildVersion = 'unknown';
    state.buildDate = 'unknown';
  }
  $('#buildMeta').textContent = `SPIFFS ${state.buildVersion} | ${state.buildDate}`;
  $('#aboutVersion').textContent = state.buildVersion;
  $('#aboutDate').textContent = state.buildDate;
}

async function loadFirebase() {
  const response = await fetch('/api/firebase-web-config');
  if (!response.ok) throw new Error('Firebase config unavailable');
  const config = await response.json();
  state.deviceId = config.deviceId || state.deviceId;
  state.countryCode = config.defaultCountryCode || state.countryCode;
  const app = initializeApp({
    apiKey: config.apiKey,
    authDomain: config.authDomain,
    databaseURL: config.databaseURL,
    projectId: config.projectId
  });
  state.db = getFirestore(app);
  state.rtdb = getDatabase(app);
  const auth = getAuth(app);
  onAuthStateChanged(auth, (user) => {
    $('#connectionState').textContent = user ? `Online as ${state.deviceId}` : 'Authenticating';
  });
  await signInAnonymously(auth);
}

async function fetchConfig() {
  const snap = await getDoc(doc(state.db, paths.configDoc));
  state.config = snap.exists() ? snap.data() : {};
  return state.config;
}

async function fetchDevice() {
  const snap = await getDoc(doc(state.db, paths.deviceDoc));
  state.device = snap.exists() ? snap.data() : {};
  return state.device;
}

async function fetchRuntime() {
  const snap = await get(ref(state.rtdb, paths.runtime));
  state.runtime = snap.exists() ? snap.val() : {};
  return state.runtime;
}

async function renderDevices() {
  const config = await fetchConfig();
  const device = await fetchDevice();
  const runtime = await fetchRuntime();
  const now = Math.floor(Date.now() / 1000);
  const lastSeen = device.last_seen_epoch || epochFromTimestamp(device.last_seen_at);
  const online = lastSeen && now - lastSeen < 120;

  $('#deviceForm').active.checked = config.active !== false;
  $('#deviceForm').name.value = text(config.name, 'TTGO T-Call');
  $('#runtimeForm').jobLogs.checked = runtime.jobLogs !== false;
  $('#runtimeForm').showFirebasePushLogs.checked = runtime.showFirebasePushLogs === true;
  $('#runtimeForm').showThingSpeakPushLogs.checked = runtime.showThingSpeakPushLogs === true;
  $('#runtimeForm').intervalOfDhtSeconds.value = text(runtime.intervalOfDhtSeconds, '');
  $('#runtimeForm').dailySmsLimit.value = text(runtime.dailySmsLimit, '');

  $('#deviceList').innerHTML = `
    <article class="device-card">
      <div class="card-head">
        <strong>${text(config.name, device.name || state.deviceId)}</strong>
        <span class="pill ${online ? 'online' : 'offline'}">${online ? 'online' : 'offline'}</span>
      </div>
      <dl>
        <dt>Active</dt><dd>${config.active !== false ? 'yes' : 'no'}</dd>
        <dt>Battery</dt><dd>${text(device.battery_percent, 'n/a')}%</dd>
        <dt>Signal</dt><dd>${text(device.signal_strength, 'n/a')}</dd>
        <dt>Operator</dt><dd>${text(device.network_operator, 'n/a')}</dd>
        <dt>Last seen</dt><dd>${lastSeen ? new Date(lastSeen * 1000).toLocaleString() : 'n/a'}</dd>
        <dt>Job logs</dt><dd>${runtime.jobLogs !== false ? 'enabled' : 'disabled'}</dd>
      </dl>
    </article>
  `;
}

async function renderWifi() {
  try {
    const runtime = await fetchRuntime();
    const form = $('#wifiForm');
    form.ssid1.value = text(runtime.wifiSsid1);
    form.pass1.value = text(runtime.wifiPass1);
    form.ssid2.value = text(runtime.wifiSsid2);
    form.pass2.value = text(runtime.wifiPass2);
    $('#wifiState').textContent = 'Loaded WiFi settings from Firebase';
  } catch (error) {
    $('#wifiState').textContent = `Failed to load: ${error.message}`;
  }
}

async function renderNumbers() {
  const snap = await getDocs(collection(state.db, paths.allowedNumbers));
  const rows = [];
  snap.forEach((item) => {
    const data = item.data();
    rows.push([
      text(data.phone_number, item.id),
      statusPill(data.enabled ? 'enabled' : 'disabled'),
      text(data.sms_limit_per_day, 0),
      text(data.call_limit_per_day, 0),
      text(data.notes),
      actionButton(data.enabled ? 'Disable' : 'Enable', 'toggle-number', item.id)
    ]);
  });
  $('#numbersList').innerHTML = table(['Number', 'State', 'SMS/day', 'Calls/day', 'Notes', ''], rows);
}

async function renderBlocks() {
  const config = await fetchConfig();
  $('#blockForm').blockedCallers.value = arrayToLines(config.blockedCallers);
  $('#blockForm').blockedSmsSenders.value = arrayToLines(config.blockedSmsSenders);
}

function jobActions(kind, itemId, data) {
  const actions = [];
  const retryAction = kind === 'sms' ? 'retry-sms' : 'retry-call';
  const allowAction = kind === 'sms' ? 'allow-retry-sms' : 'allow-retry-call';
  if (['failed', 'blocked', 'quota_exceeded'].includes(data.status)) {
    actions.push(actionButton('Retry', retryAction, itemId));
  }
  if (data.status === 'blocked' && data.error === 'number_not_allowed') {
    actions.push(actionButton('Allow + Retry', allowAction, itemId, data.phone_number));
  }
  return actions.join(' ');
}

async function renderSmsJobs() {
  const snap = await getDocs(query(collection(state.db, paths.smsJobs), limit(50)));
  const rows = [];
  snap.forEach((item) => {
    const data = item.data();
    rows.push({
      sort: epochFromTimestamp(data.created_at) || data.created_epoch || 0,
      cells: [
        text(data.phone_number),
        text(data.message),
        statusPill(data.status),
        text(data.error),
        jobActions('sms', item.id, data)
      ]
    });
  });
  rows.sort((a, b) => b.sort - a.sort);
  $('#smsJobs').innerHTML = table(['Number', 'Message', 'Status', 'Error', ''], rows.map((row) => row.cells));
}

async function renderCallJobs() {
  const snap = await getDocs(query(collection(state.db, paths.callJobs), limit(50)));
  const rows = [];
  snap.forEach((item) => {
    const data = item.data();
    rows.push({
      sort: epochFromTimestamp(data.created_at) || data.created_epoch || 0,
      cells: [
        text(data.phone_number),
        statusPill(data.status),
        data.user_picked ? 'yes' : 'no',
        text(data.duration_seconds, 0),
        text(data.error),
        jobActions('call', item.id, data)
      ]
    });
  });
  rows.sort((a, b) => b.sort - a.sort);
  $('#callJobs').innerHTML = table(['Number', 'Status', 'Picked', 'Seconds', 'Error', ''], rows.map((row) => row.cells));
}

async function renderLogs() {
  const collectionName = $('#logType').value === 'calls' ? paths.callLogs : paths.smsLogs;
  const snap = await getDocs(query(collection(state.db, collectionName), limit(80)));
  const rows = [];
  snap.forEach((item) => {
    const data = item.data();
    const epoch = data.timestamp_epoch || epochFromTimestamp(data.timestamp);
    if ($('#logType').value === 'calls') {
      rows.push({
        sort: epoch,
        cells: [
          text(data.direction),
          text(data.phone_number),
          data.answered ? 'yes' : 'no',
          text(data.duration_seconds, 0),
          statusPill(data.status),
          epoch ? new Date(epoch * 1000).toLocaleString() : ''
        ]
      });
    } else {
      rows.push({
        sort: epoch,
        cells: [
          text(data.direction),
          text(data.phone_number),
          text(data.message),
          statusPill(data.status),
          epoch ? new Date(epoch * 1000).toLocaleString() : ''
        ]
      });
    }
  });
  rows.sort((a, b) => b.sort - a.sort);
  $('#logsList').innerHTML = $('#logType').value === 'calls'
    ? table(['Direction', 'Number', 'Answered', 'Seconds', 'Status', 'Time'], rows.map((row) => row.cells))
    : table(['Direction', 'Number', 'Message', 'Status', 'Time'], rows.map((row) => row.cells));
}

function renderTesting() {
  $('#notifyTestState').textContent = 'Idle';
}

function renderAbout() {
  $('#aboutVersion').textContent = state.buildVersion;
  $('#aboutDate').textContent = state.buildDate;
}

async function refresh() {
  if (!state.db) return;
  if (state.activeView === 'devices') await renderDevices();
  if (state.activeView === 'wifi') await renderWifi();
  if (state.activeView === 'numbers') await renderNumbers();
  if (state.activeView === 'blocks') await renderBlocks();
  if (state.activeView === 'sms') await renderSmsJobs();
  if (state.activeView === 'calls') await renderCallJobs();
  if (state.activeView === 'logs') await renderLogs();
  if (state.activeView === 'testing') renderTesting();
  if (state.activeView === 'about') renderAbout();
}

async function saveAllowedNumber(phone, options = {}) {
  const normalized = canonicalNumber(phone);
  await setDoc(doc(state.db, paths.allowedNumbers, safeId(normalized)), {
    phone_number: normalized,
    enabled: options.enabled ?? true,
    sms_limit_per_day: options.smsLimit ?? 5,
    call_limit_per_day: options.callLimit ?? 5,
    notes: options.notes ?? '',
    updated_at: serverTimestamp()
  }, { merge: true });
}

async function saveDeviceSettings(form) {
  await setDoc(doc(state.db, paths.configDoc), {
    active: form.get('active') === 'on',
    name: form.get('name').trim() || 'TTGO T-Call',
    updated_at: serverTimestamp()
  }, { merge: true });
}

async function saveRuntimeSettings(form) {
  await update(ref(state.rtdb, paths.runtime), {
    jobLogs: form.get('jobLogs') === 'on',
    showFirebasePushLogs: form.get('showFirebasePushLogs') === 'on',
    showThingSpeakPushLogs: form.get('showThingSpeakPushLogs') === 'on',
    intervalOfDhtSeconds: Number(form.get('intervalOfDhtSeconds')) || 30,
    dailySmsLimit: Number(form.get('dailySmsLimit')) || 200,
    updatedAtMs: Date.now()
  });
}

async function saveBlocks(form) {
  await setDoc(doc(state.db, paths.configDoc), {
    blockedCallers: linesToArray(form.get('blockedCallers')),
    blockedSmsSenders: linesToArray(form.get('blockedSmsSenders')),
    updated_at: serverTimestamp()
  }, { merge: true });
}

// Two-way sync: push edits from the active settings view to Firebase, ask the
// device to re-read runtime settings + blocklists now, then pull the latest
// values back into the UI. Pushing only the active view avoids overwriting a
// tab the user never opened (whose form fields would still be empty).
async function fullSync() {
  const btn = $('#syncRuntimeBtn');
  const original = btn.textContent;
  btn.disabled = true;
  btn.textContent = 'Syncing…';
  try {
    if (state.activeView === 'devices') {
      await saveDeviceSettings(new FormData($('#deviceForm')));
      await saveRuntimeSettings(new FormData($('#runtimeForm')));
    } else if (state.activeView === 'blocks') {
      await saveBlocks(new FormData($('#blockForm')));
    }
    await fetch('/api/sync-runtime', { method: 'POST' });
    await refresh();
    btn.textContent = 'Synced ✓';
  } catch (error) {
    btn.textContent = 'Sync failed';
  } finally {
    setTimeout(() => { btn.disabled = false; btn.textContent = original; }, 1500);
  }
}

async function retrySms(id) {
  await updateDoc(doc(state.db, paths.smsJobs, id), {
    status: 'pending',
    processing_started_at: null,
    completed_at: null,
    error: null
  });
}

async function retryCall(id) {
  await updateDoc(doc(state.db, paths.callJobs, id), {
    status: 'pending',
    processing_started_at: null,
    completed_at: null,
    user_picked: false,
    duration_seconds: 0,
    error: null
  });
}

function bindTabs() {
  document.querySelectorAll('.tab').forEach((tab) => {
    tab.addEventListener('click', async () => {
      state.activeView = tab.dataset.view;
      document.querySelectorAll('.tab').forEach((item) => item.classList.toggle('active', item === tab));
      document.querySelectorAll('.view').forEach((item) => item.classList.toggle('active', item.id === state.activeView));
      await refresh();
    });
  });
}

function bindForms() {
  $('#deviceForm').addEventListener('submit', async (event) => {
    event.preventDefault();
    await saveDeviceSettings(new FormData(event.currentTarget));
    await renderDevices();
  });

  $('#runtimeForm').addEventListener('submit', async (event) => {
    event.preventDefault();
    await saveRuntimeSettings(new FormData(event.currentTarget));
    await fetch('/api/sync-runtime', { method: 'POST' });
    await renderDevices();
  });

  $('#numberForm').addEventListener('submit', async (event) => {
    event.preventDefault();
    const form = new FormData(event.currentTarget);
    await saveAllowedNumber(form.get('phone_number').trim(), {
      enabled: form.get('enabled') === 'on',
      smsLimit: Number(form.get('sms_limit_per_day')) || 0,
      callLimit: Number(form.get('call_limit_per_day')) || 0,
      notes: form.get('notes').trim()
    });
    event.currentTarget.reset();
    event.currentTarget.enabled.checked = true;
    await renderNumbers();
  });

  $('#blockForm').addEventListener('submit', async (event) => {
    event.preventDefault();
    await saveBlocks(new FormData(event.currentTarget));
    await renderBlocks();
  });

  $('#smsForm').addEventListener('submit', async (event) => {
    event.preventDefault();
    const form = new FormData(event.currentTarget);
    await addDoc(collection(state.db, paths.smsJobs), {
      phone_number: canonicalNumber(form.get('phone_number')),
      message: form.get('message').trim(),
      status: 'pending',
      created_at: serverTimestamp(),
      processing_started_at: null,
      completed_at: null,
      error: null
    });
    event.currentTarget.reset();
    await renderSmsJobs();
  });

  $('#wifiForm').addEventListener('submit', async (event) => {
    event.preventDefault();
    const form = new FormData(event.currentTarget);
    $('#wifiState').textContent = 'Saving...';
    try {
      // WiFi is just another runtime setting: write it to the same RTDB node the
      // device syncs, then ask the device to re-read. It applies on next reboot.
      await update(ref(state.rtdb, paths.runtime), {
        wifiSsid1: form.get('ssid1').trim(),
        wifiPass1: form.get('pass1'),
        wifiSsid2: form.get('ssid2').trim(),
        wifiPass2: form.get('pass2'),
        updatedAtMs: Date.now()
      });
      await fetch('/api/sync-runtime', { method: 'POST' });
      $('#wifiState').textContent = 'Saved to Firebase. Reboot the device to connect with the new network.';
    } catch (error) {
      $('#wifiState').textContent = `Failed: ${error.message}`;
    }
  });

  $('#callForm').addEventListener('submit', async (event) => {
    event.preventDefault();
    const form = new FormData(event.currentTarget);
    await addDoc(collection(state.db, paths.callJobs), {
      phone_number: canonicalNumber(form.get('phone_number')),
      status: 'pending',
      created_at: serverTimestamp(),
      processing_started_at: null,
      completed_at: null,
      user_picked: false,
      duration_seconds: 0,
      error: null
    });
    event.currentTarget.reset();
    await renderCallJobs();
  });
}

function bindActions() {
  document.body.addEventListener('click', async (event) => {
    const button = event.target.closest('[data-action]');
    if (!button) return;
    const { action, id, phone } = button.dataset;
    if (action === 'toggle-number') {
      const enable = button.closest('tr').textContent.includes('disabled');
      await updateDoc(doc(state.db, paths.allowedNumbers, id), { enabled: enable, updated_at: serverTimestamp() });
      await renderNumbers();
    }
    if (action === 'retry-sms') {
      await retrySms(id);
      await renderSmsJobs();
    }
    if (action === 'retry-call') {
      await retryCall(id);
      await renderCallJobs();
    }
    if (action === 'allow-retry-sms') {
      await saveAllowedNumber(phone, { enabled: true });
      await retrySms(id);
      await renderSmsJobs();
    }
    if (action === 'allow-retry-call') {
      await saveAllowedNumber(phone, { enabled: true });
      await retryCall(id);
      await renderCallJobs();
    }
  });
}

async function boot() {
  bindTabs();
  bindForms();
  bindActions();
  $('#refreshBtn').addEventListener('click', refresh);
  $('#syncRuntimeBtn').addEventListener('click', fullSync);
  $('#rebootBtn').addEventListener('click', async () => {
    if (!window.confirm('Reboot the device now? It will be offline for a few seconds.')) return;
    $('#wifiState').textContent = 'Rebooting...';
    try {
      await fetch('/api/reboot', { method: 'POST' });
      $('#wifiState').textContent = 'Reboot requested. Reconnect once the device is back online.';
    } catch (error) {
      $('#wifiState').textContent = `Reboot request failed: ${error.message}`;
    }
  });
  $('#logType').addEventListener('change', renderLogs);
  $('#notifyTestBtn').addEventListener('click', async () => {
    const output = $('#notifyTestState');
    output.textContent = 'Sending...';
    try {
      const response = await fetch('/api/notify-test', { method: 'POST' });
      const body = await response.json();
      output.textContent = body.ok ? `Sent: ${body.message}` : `Failed: ${body.message || body.error}`;
    } catch (error) {
      output.textContent = `Failed: ${error.message}`;
    }
  });
  try {
    await loadVersionMeta();
    await loadFirebase();
    await refresh();
  } catch (error) {
    $('#connectionState').textContent = error.message;
  }
}

boot();
