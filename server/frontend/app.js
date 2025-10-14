(function(){
  const year = new Date().getFullYear();
  document.body.innerHTML = document.body.innerHTML.replace('{year}', year);

  const $ = id=>document.getElementById(id);
  const show = ()=>{ $('login').classList.add('hidden'); $('main').classList.remove('hidden'); };
  const getHeaders = () => {
    const headers = {'Content-Type': 'application/json'};
    const apiKey = $('apiKey').value.trim();
    if (apiKey) headers['X-Api-Secret'] = apiKey;
    return headers;
  };

  async function login(){
    const pw = $('pw').value;
    const res = await fetch('/dashboard/login', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({password: pw})});
    if (res.ok) { $('loginMsg').textContent = 'ok'; show(); loadStatus(); } else { $('loginMsg').textContent = 'bad password'; }
  }

  async function loadStatus(){
    const res = await fetch('/dashboard/status', {headers: getHeaders()});
    if (!res.ok) { $('status').textContent = 'unauthorized'; return; }
    const j = await res.json();
    $('status').textContent = `Device host: ${j.device_host}:${j.device_port}\nStored devices: ${Object.keys(j.device_ips).length}`;
    $('msgsOut').textContent = '';
  }

  async function sendSms(){
    const to = $('to').value; const message = $('message').value;
    const res = await fetch('/send', {method:'POST', headers: getHeaders(), body: JSON.stringify({to:to, message:message})});
    const text = await res.text(); $('sendResp').textContent = res.status + '\n' + text;
  }

  async function fetchMsgs(){
    const res = await fetch('/messages/list', {headers: getHeaders()});
    const text = await res.text();
    try { const j = JSON.parse(text); $('msgsOut').textContent = JSON.stringify(j, null, 2); } catch(e){ $('msgsOut').textContent = text; }
  }

  async function deleteMsgs(){
    const res = await fetch('/messages/delete', {method:'POST', headers: getHeaders()});
    const text = await res.text(); $('msgsOut').textContent = text;
  }

  /* Logs UI handlers */
  function openLogsModal(){
    $('logsModal').classList.remove('hidden');
  }
  function closeLogsModal(){
    $('logsModal').classList.add('hidden');
  }

  function buildQueryParams(params){
    const esc = encodeURIComponent;
    return Object.keys(params).filter(k=>params[k]!=null && params[k] !== '').map(k=>`${esc(k)}=${esc(params[k])}`).join('&');
  }

  async function showLogs(){
    const start = $('logStart').value;
    const end = $('logEnd').value;
    const days = $('logDays').value;
    const qp = buildQueryParams({start:start, end:end, days: days});
    const url = '/logs' + (qp ? ('?' + qp) : '');
    const res = await fetch(url, {headers: getHeaders()});
    if (!res.ok){
      $('logsOut').textContent = 'Failed to fetch logs: ' + res.status;
      return;
    }
    const j = await res.json();
    $('logsMeta').textContent = `Showing ${j.count} rows from ${j.start} to ${j.end}`;
    // render table
    const rows = j.rows || [];
    if (rows.length === 0){
      $('logsOut').textContent = 'No rows';
      $('downloadFiltered').href = '#';
      return;
    }
    const table = document.createElement('table');
    table.className = 'logs-table';
    const thead = document.createElement('thead');
    const headerRow = document.createElement('tr');
    const cols = ['timestamp','type','number','body'];
    cols.forEach(k=>{ const th = document.createElement('th'); th.textContent = k; headerRow.appendChild(th); });
    thead.appendChild(headerRow);
    table.appendChild(thead);
    const tbody = document.createElement('tbody');
    rows.forEach(r=>{
      const tr = document.createElement('tr');
      cols.forEach(c=>{ const td = document.createElement('td'); td.textContent = r[c] || ''; tr.appendChild(td); });
      tbody.appendChild(tr);
    });
    table.appendChild(tbody);
    const out = $('logsOut'); out.innerHTML = ''; out.appendChild(table);

    // update download link
    const dl = '/logs/download' + (qp ? ('?' + qp) : '');
    $('downloadFiltered').href = dl;
  }

  $('loginBtn').addEventListener('click', login);
  $('refreshStatus').addEventListener('click', loadStatus);
  $('sendBtn').addEventListener('click', sendSms);
  $('fetchMsgs').addEventListener('click', fetchMsgs);
  $('deleteMsgs').addEventListener('click', deleteMsgs);
  // logs
  const seeLogsBtn = $('seeLogsBtn'); if (seeLogsBtn) seeLogsBtn.addEventListener('click', openLogsModal);
  const closeLogsBtn = $('closeLogsBtn'); if (closeLogsBtn) closeLogsBtn.addEventListener('click', ()=>{ closeLogsModal(); $('logsOut').innerHTML=''; $('logsMeta').textContent=''; $('logStart').value=''; $('logEnd').value=''; $('logDays').value=''; });
  const showLogsBtn = $('showLogsBtn'); if (showLogsBtn) showLogsBtn.addEventListener('click', showLogs);

})();
