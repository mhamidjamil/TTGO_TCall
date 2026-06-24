import { initializeApp } from 'https://www.gstatic.com/firebasejs/10.12.5/firebase-app.js';
import { getAuth, onAuthStateChanged, signInAnonymously } from 'https://www.gstatic.com/firebasejs/10.12.5/firebase-auth.js';
import {
  collection,
  doc,
  getDoc,
  getDocs,
  getFirestore,
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
  deviceDoc: 'sim_module/device',
  smsJobs: 'sim_module/sms/sms_jobs',
  smsReceived: 'sim_module/sms/sms_received',
  callJobs: 'sim_module/calls/call_jobs',
  callReceived: 'sim_module/calls/call_received',
  runtime: 'ttgo_tcall/settings/runtime'
};

const state = {
  db: null,
  rtdb: null,
  countryCode: '92',
  activeView: 'device',
  buildVersion: 'unknown',
  buildDate: 'unknown',
  device: {},
  runtime: {}
};

const $ = (selector) => document.querySelector(selector);

// Mirror normalizePhoneNumber() in sms_calls.ino so doc ids match the device.
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

function actionButton(label, action, id) {
  return `<button class="mini" data-action="${action}" data-id="${id}" type="button">${label}</button>`;
}

function table(headers, rows) {
  if (!rows.length) return '<div class="empty">No records</div>';
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
  $('#buildMeta').textContent = `LittleFS ${state.buildVersion} | ${state.buildDate}`;
}

async function loadFirebase() {
  const response = await fetch('/api/firebase-web-config');
  if (!response.ok) throw new Error('Firebase config unavailable');
  const config = await response.json();
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
    $('#connectionState').textContent = user ? 'Online' : 'Authenticating';
  });
  await signInAnonymously(auth);
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

const COUNTERS = [
  ['totalSmsSent', 'SMS sent'],
  ['totalSmsReceived', 'SMS received'],
  ['totalSmsBlockedOutgoing', 'SMS blocked (out)'],
  ['totalSmsMutedIncoming', 'SMS muted (in)'],
  ['totalCallsMade', 'Calls made'],
  ['totalCallsReceived', 'Calls received'],
  ['totalCallsBlockedOutgoing', 'Calls blocked (out)'],
  ['totalCallsMutedIncoming', 'Calls muted (in)']
];

async function renderDevice() {
  const device = await fetchDevice();
  const runtime = await fetchRuntime();
  const now = Math.floor(Date.now() / 1000);
  const lastSeen = device.last_seen_epoch || 0;
  const online = lastSeen && now - lastSeen < 120;

  $('#deviceForm').active.checked = device.active !== false;
  $('#deviceForm').name.value = text(device.name, 'TTGO T-Call');
  $('#runtimeForm').jobLogs.checked = runtime.jobLogs !== false;
  $('#runtimeForm').showFirebasePushLogs.checked = runtime.showFirebasePushLogs === true;
  $('#runtimeForm').showThingSpeakPushLogs.checked = runtime.showThingSpeakPushLogs === true;
  $('#runtimeForm').intervalOfDhtSeconds.value = text(runtime.intervalOfDhtSeconds, '');
  $('#runtimeForm').dailySmsLimit.value = text(runtime.dailySmsLimit, '');

  $('#blockForm').blockedIncomingCallers.value = arrayToLines(device.blockedIncomingCallers);
  $('#blockForm').blockedIncomingSms.value = arrayToLines(device.blockedIncomingSms);
  $('#blockForm').blockedOutgoingCallers.value = arrayToLines(device.blockedOutgoingCallers);
  $('#blockForm').blockedOutgoingSms.value = arrayToLines(device.blockedOutgoingSms);

  $('#deviceList').innerHTML = `
    <article class="device-card">
      <div class="card-head">
        <strong>${text(device.name, 'TTGO T-Call')}</strong>
        <span class="pill ${online ? 'online' : 'offline'}">${online ? 'online' : 'offline'}</span>
      </div>
      <dl>
        <dt>Active</dt><dd>${device.active !== false ? 'yes' : 'no'}</dd>
        <dt>Battery</dt><dd>${text(device.battery_percent, 'n/a')}%</dd>
        <dt>Signal</dt><dd>${text(device.signal_strength, 'n/a')}</dd>
        <dt>Operator</dt><dd>${text(device.network_operator, 'n/a')}</dd>
        <dt>Last seen</dt><dd>${lastSeen ? new Date(lastSeen * 1000).toLocaleString() : 'n/a'}</dd>
      </dl>
    </article>`;

  $('#counters').innerHTML = COUNTERS.map(([key, label]) =>
    `<div class="counter"><span class="counter-value">${text(device[key], 0)}</span><span class="counter-label">${label}</span></div>`
  ).join('');
}

async function renderJobs(pathKey, mountId, isCall) {
  const snap = await getDocs(collection(state.db, paths[pathKey]));
  const rows = [];
  snap.forEach((item) => {
    const d = item.data();
    if (isCall) {
      rows.push([
        text(d.phone_number, item.id),
        statusPill(d.status),
        d.user_picked ? 'yes' : 'no',
        text(d.duration_seconds, 0),
        text(d.enque_by),
        text(d.error),
        actionButton('Retry', 'retry-call', item.id)
      ]);
    } else {
      rows.push([
        text(d.phone_number, item.id),
        text(d.message),
        statusPill(d.status),
        text(d.enque_by),
        text(d.error),
        actionButton('Retry', 'retry-sms', item.id)
      ]);
    }
  });
  const headers = isCall
    ? ['Number', 'Status', 'Picked', 'Seconds', 'Enqueued by', 'Error', '']
    : ['Number', 'Message', 'Status', 'Enqueued by', 'Error', ''];
  $(mountId).innerHTML = table(headers, rows);
}

async function renderReceived(pathKey, mountId, isCall) {
  const snap = await getDocs(collection(state.db, paths[pathKey]));
  const rows = [];
  snap.forEach((item) => {
    const d = item.data();
    if (isCall) {
      rows.push([
        text(d.number, item.id),
        d.notified ? 'yes' : 'no',
        d.blocked ? 'yes' : 'no',
        text(d.last_received_at)
      ]);
    } else {
      rows.push([
        text(d.number, item.id),
        text(d.normalized_message),
        d.was_decoded ? 'yes' : 'no',
        d.notified ? 'yes' : 'no',
        d.blocked ? 'yes' : 'no',
        text(d.last_received_at)
      ]);
    }
  });
  const headers = isCall
    ? ['Number', 'Notified', 'Blocked', 'Last received']
    : ['Number', 'Message', 'Decoded', 'Notified', 'Blocked', 'Last received'];
  $(mountId).innerHTML = table(headers, rows);
}

async function renderSms() {
  await renderJobs('smsJobs', '#smsJobs', false);
  await renderReceived('smsReceived', '#smsReceived', false);
}

async function renderCalls() {
  await renderJobs('callJobs', '#callJobs', true);
  await renderReceived('callReceived', '#callReceived', true);
}

async function renderWifi() {
  const runtime = await fetchRuntime();
  const form = $('#wifiForm');
  form.ssid1.value = text(runtime.wifiSsid1);
  form.pass1.value = text(runtime.wifiPass1);
  form.ssid2.value = text(runtime.wifiSsid2);
  form.pass2.value = text(runtime.wifiPass2);
  $('#wifiState').textContent = 'Loaded WiFi settings from Firebase';
}

function renderAbout() {
  $('#aboutVersion').textContent = state.buildVersion;
  $('#aboutDate').textContent = state.buildDate;
}

async function refresh() {
  if (!state.db) return;
  if (state.activeView === 'device') await renderDevice();
  if (state.activeView === 'wifi') await renderWifi();
  if (state.activeView === 'sms') await renderSms();
  if (state.activeView === 'calls') await renderCalls();
  if (state.activeView === 'about') renderAbout();
}

async function saveDevice(form) {
  await setDoc(doc(state.db, paths.deviceDoc), {
    active: form.get('active') === 'on',
    name: form.get('name').trim() || 'TTGO T-Call',
    updated_at: serverTimestamp()
  }, { merge: true });
}

async function saveRuntime(form) {
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
  await setDoc(doc(state.db, paths.deviceDoc), {
    blockedIncomingCallers: linesToArray(form.get('blockedIncomingCallers')),
    blockedIncomingSms: linesToArray(form.get('blockedIncomingSms')),
    blockedOutgoingCallers: linesToArray(form.get('blockedOutgoingCallers')),
    blockedOutgoingSms: linesToArray(form.get('blockedOutgoingSms')),
    updated_at: serverTimestamp()
  }, { merge: true });
}

async function retryJob(pathKey, id, isCall) {
  const reset = { status: 'pending', processing_started_at: null, completed_at: null, error: null };
  if (isCall) { reset.user_picked = false; reset.duration_seconds = 0; }
  await updateDoc(doc(state.db, paths[pathKey], id), reset);
}

async function fullSync() {
  const btn = $('#syncRuntimeBtn');
  const original = btn.textContent;
  btn.disabled = true;
  btn.textContent = 'Syncing…';
  try {
    if (state.activeView === 'device') {
      await saveDevice(new FormData($('#deviceForm')));
      await saveRuntime(new FormData($('#runtimeForm')));
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
    await saveDevice(new FormData(event.currentTarget));
    await renderDevice();
  });

  $('#runtimeForm').addEventListener('submit', async (event) => {
    event.preventDefault();
    await saveRuntime(new FormData(event.currentTarget));
    await fetch('/api/sync-runtime', { method: 'POST' });
    await renderDevice();
  });

  $('#blockForm').addEventListener('submit', async (event) => {
    event.preventDefault();
    await saveBlocks(new FormData(event.currentTarget));
    await fetch('/api/sync-runtime', { method: 'POST' });
    await renderDevice();
  });

  $('#wifiForm').addEventListener('submit', async (event) => {
    event.preventDefault();
    const form = new FormData(event.currentTarget);
    $('#wifiState').textContent = 'Saving...';
    try {
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

  $('#smsForm').addEventListener('submit', async (event) => {
    event.preventDefault();
    const form = new FormData(event.currentTarget);
    const number = canonicalNumber(form.get('phone_number'));
    await setDoc(doc(state.db, paths.smsJobs, safeId(number)), {
      phone_number: number,
      message: form.get('message').trim(),
      status: 'pending',
      enque_by: form.get('enque_by').trim(),
      created_at: serverTimestamp(),
      processing_started_at: null,
      completed_at: null,
      error: null
    });
    event.currentTarget.reset();
    await renderSms();
  });

  $('#callForm').addEventListener('submit', async (event) => {
    event.preventDefault();
    const form = new FormData(event.currentTarget);
    const number = canonicalNumber(form.get('phone_number'));
    await setDoc(doc(state.db, paths.callJobs, safeId(number)), {
      phone_number: number,
      status: 'pending',
      enque_by: form.get('enque_by').trim(),
      created_at: serverTimestamp(),
      processing_started_at: null,
      completed_at: null,
      user_picked: false,
      duration_seconds: 0,
      error: null
    });
    event.currentTarget.reset();
    await renderCalls();
  });
}

function bindActions() {
  document.body.addEventListener('click', async (event) => {
    const button = event.target.closest('[data-action]');
    if (!button) return;
    const { action, id } = button.dataset;
    if (action === 'retry-sms') { await retryJob('smsJobs', id, false); await renderSms(); }
    if (action === 'retry-call') { await retryJob('callJobs', id, true); await renderCalls(); }
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
