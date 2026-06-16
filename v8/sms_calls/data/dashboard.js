import { initializeApp } from 'https://www.gstatic.com/firebasejs/10.12.5/firebase-app.js';
import { getAuth, signInAnonymously, onAuthStateChanged } from 'https://www.gstatic.com/firebasejs/10.12.5/firebase-auth.js';
import {
  addDoc,
  collection,
  doc,
  getDocs,
  getFirestore,
  limit,
  query,
  serverTimestamp,
  setDoc,
  updateDoc
} from 'https://www.gstatic.com/firebasejs/10.12.5/firebase-firestore.js';

const state = {
  db: null,
  deviceId: 'device_001',
  activeView: 'devices',
  buildVersion: 'unknown',
  buildDate: 'unknown'
};

const $ = (selector) => document.querySelector(selector);

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

function statusPill(status) {
  return `<span class="pill ${text(status, 'pending')}">${text(status, 'pending')}</span>`;
}

function actionButton(label, action, id) {
  return `<button class="mini" data-action="${action}" data-id="${id}" type="button">${label}</button>`;
}

function table(headers, rows) {
  if (!rows.length) {
    return '<div class="empty">No records</div>';
  }
  const head = headers.map((item) => `<th>${item}</th>`).join('');
  const body = rows.map((row) => `<tr>${row.map((cell) => `<td>${cell}</td>`).join('')}</tr>`).join('');
  return `<table><thead><tr>${head}</tr></thead><tbody>${body}</tbody></table>`;
}

async function loadFirebase() {
  const response = await fetch('/api/firebase-web-config');
  if (!response.ok) throw new Error('Firebase config unavailable');
  const config = await response.json();
  state.deviceId = config.deviceId || state.deviceId;
  const app = initializeApp({
    apiKey: config.apiKey,
    authDomain: config.authDomain,
    projectId: config.projectId
  });
  state.db = getFirestore(app);
  const auth = getAuth(app);
  onAuthStateChanged(auth, (user) => {
    $('#connectionState').textContent = user ? `Online as ${state.deviceId}` : 'Authenticating';
  });
  await signInAnonymously(auth);
}

function parseVersionText(text) {
  const lines = String(text || '').split(/\r?\n/).map((line) => line.trim()).filter(Boolean);
  if (!lines.length) {
    return { version: 'unknown', date: 'unknown' };
  }
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

async function renderDevices() {
  const snap = await getDocs(collection(state.db, 'sim_modules'));
  const now = Math.floor(Date.now() / 1000);
  const cards = [];
  snap.forEach((item) => {
    const data = item.data();
    const lastSeen = data.last_seen_epoch || epochFromTimestamp(data.last_seen_at);
    const online = lastSeen && now - lastSeen < 120;
    cards.push(`
      <article class="device-card">
        <div class="card-head">
          <strong>${text(data.name, item.id)}</strong>
          <span class="pill ${online ? 'online' : 'offline'}">${online ? 'online' : 'offline'}</span>
        </div>
        <dl>
          <dt>Battery</dt><dd>${text(data.battery_percent, 'n/a')}%</dd>
          <dt>Signal</dt><dd>${text(data.signal_strength, 'n/a')}</dd>
          <dt>Operator</dt><dd>${text(data.network_operator, 'n/a')}</dd>
          <dt>Last seen</dt><dd>${lastSeen ? new Date(lastSeen * 1000).toLocaleString() : 'n/a'}</dd>
          <dt>Poll</dt><dd>${text(data.poll_interval_seconds, 'n/a')}s</dd>
        </dl>
      </article>
    `);
  });
  $('#deviceList').innerHTML = cards.join('') || '<div class="empty">No devices</div>';
}

async function renderNumbers() {
  const snap = await getDocs(collection(state.db, 'allowed_numbers'));
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

async function renderSmsJobs() {
  const snap = await getDocs(query(collection(state.db, 'sms_jobs'), limit(50)));
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
        ['failed', 'blocked', 'quota_exceeded'].includes(data.status) ? actionButton('Retry', 'retry-sms', item.id) : ''
      ]
    });
  });
  rows.sort((a, b) => b.sort - a.sort);
  $('#smsJobs').innerHTML = table(['Number', 'Message', 'Status', 'Error', ''], rows.map((row) => row.cells));
}

async function renderCallJobs() {
  const snap = await getDocs(query(collection(state.db, 'call_jobs'), limit(50)));
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
        ['failed', 'blocked', 'quota_exceeded'].includes(data.status) ? actionButton('Retry', 'retry-call', item.id) : ''
      ]
    });
  });
  rows.sort((a, b) => b.sort - a.sort);
  $('#callJobs').innerHTML = table(['Number', 'Status', 'Picked', 'Seconds', 'Error', ''], rows.map((row) => row.cells));
}

async function renderLogs() {
  const type = $('#logType').value;
  const collectionName = type === 'calls' ? 'call_logs' : 'sms_logs';
  const snap = await getDocs(query(collection(state.db, collectionName), limit(80)));
  const rows = [];
  snap.forEach((item) => {
    const data = item.data();
    const epoch = data.timestamp_epoch || epochFromTimestamp(data.timestamp);
    if (type === 'calls') {
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
  $('#logsList').innerHTML = type === 'calls'
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
  if (state.activeView === 'numbers') await renderNumbers();
  if (state.activeView === 'sms') await renderSmsJobs();
  if (state.activeView === 'calls') await renderCallJobs();
  if (state.activeView === 'logs') await renderLogs();
  if (state.activeView === 'testing') renderTesting();
  if (state.activeView === 'about') renderAbout();
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
  $('#numberForm').addEventListener('submit', async (event) => {
    event.preventDefault();
    const form = new FormData(event.currentTarget);
    const phone = form.get('phone_number').trim();
    await setDoc(doc(state.db, 'allowed_numbers', safeId(phone)), {
      phone_number: phone,
      enabled: form.get('enabled') === 'on',
      sms_limit_per_day: Number(form.get('sms_limit_per_day')) || 0,
      call_limit_per_day: Number(form.get('call_limit_per_day')) || 0,
      notes: form.get('notes').trim(),
      updated_at: serverTimestamp()
    }, { merge: true });
    event.currentTarget.reset();
    event.currentTarget.enabled.checked = true;
    await renderNumbers();
  });

  $('#smsForm').addEventListener('submit', async (event) => {
    event.preventDefault();
    const form = new FormData(event.currentTarget);
    await addDoc(collection(state.db, 'sms_jobs'), {
      device_id: state.deviceId,
      phone_number: form.get('phone_number').trim(),
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

  $('#callForm').addEventListener('submit', async (event) => {
    event.preventDefault();
    const form = new FormData(event.currentTarget);
    await addDoc(collection(state.db, 'call_jobs'), {
      device_id: state.deviceId,
      phone_number: form.get('phone_number').trim(),
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
    const { action, id } = button.dataset;
    if (action === 'toggle-number') {
      const rowText = button.closest('tr').textContent;
      const enable = rowText.includes('disabled');
      await updateDoc(doc(state.db, 'allowed_numbers', id), { enabled: enable, updated_at: serverTimestamp() });
      await renderNumbers();
    }
    if (action === 'retry-sms') {
      await updateDoc(doc(state.db, 'sms_jobs', id), {
        status: 'pending',
        processing_started_at: null,
        completed_at: null,
        error: null
      });
      await renderSmsJobs();
    }
    if (action === 'retry-call') {
      await updateDoc(doc(state.db, 'call_jobs', id), {
        status: 'pending',
        processing_started_at: null,
        completed_at: null,
        user_picked: false,
        duration_seconds: 0,
        error: null
      });
      await renderCallJobs();
    }
  });
}

async function boot() {
  bindTabs();
  bindForms();
  bindActions();
  $('#refreshBtn').addEventListener('click', refresh);
  $('#logType').addEventListener('change', renderLogs);
  $('#notifyTestBtn').addEventListener('click', async () => {
    const output = $('#notifyTestState');
    output.textContent = 'Sending...';
    try {
      const response = await fetch('/api/notify-test', { method: 'POST' });
      const body = await response.json();
      output.textContent = body.ok ? `Sent: ${body.message}` : `Failed: ${body.error}`;
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
