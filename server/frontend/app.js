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

  $('loginBtn').addEventListener('click', login);
  $('refreshStatus').addEventListener('click', loadStatus);
  $('sendBtn').addEventListener('click', sendSms);
  $('fetchMsgs').addEventListener('click', fetchMsgs);
  $('deleteMsgs').addEventListener('click', deleteMsgs);

})();
