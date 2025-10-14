(function(){
  window._DASH_PW = window._DASH_PW || '';
  const headers = (window._DASH_PW)?{'X-Dashboard-Auth':window._DASH_PW,'Content-Type':'application/json'}:{'Content-Type':'application/json'};
  const $ = (id)=>document.getElementById(id);
  function load(){
    fetch('/api/config',{headers}).then(r=>{
      if (!r.ok) throw new Error('unauthorized');
      return r.json();
    }).then(j=>{
      $('useApiSecret').checked=!!j.useApiSecret;
      $('apiSecret').value=j.apiSecret||'';
      $('forwardUrl').value=j.forwardUrl||'';
      $('forwardApiKey').value=j.forwardApiKey||'';
      $('allowSms').checked=!!j.allowSms;
      $('allowCall').checked=!!j.allowCall;
    }).catch(e=>{
      document.getElementById('authMessage').style.display='block';
      document.getElementById('config').style.display='none';
      document.getElementById('tests').style.display='none';
      console.error('Failed to load config', e);
    });
  }

  document.addEventListener('DOMContentLoaded', function(){
    load();
    $('save').addEventListener('click', ()=>{
      const body={useApiSecret:$('useApiSecret').checked,apiSecret:$('apiSecret').value,forwardUrl:$('forwardUrl').value,forwardApiKey:$('forwardApiKey').value,allowSms:$('allowSms').checked,allowCall:$('allowCall').checked};
      fetch('/api/config',{method:'POST',headers,body:JSON.stringify(body)}).then(r=>r.json()).then(j=>$('saveResult').textContent=JSON.stringify(j)).catch(e=>$('saveResult').textContent='Save failed');
    });

    $('testForward').addEventListener('click', ()=>{
      fetch('/api/test_forward',{method:'POST',headers}).then(r=>r.json()).then(j=>$('testForwardOut').textContent=JSON.stringify(j)).catch(e=>$('testForwardOut').textContent='failed');
    });

    $('sendSms').addEventListener('click', ()=>{
      const body={to:$('smsTo').value,message:$('smsMsg').value};
      fetch('/api/send_sms',{method:'POST',headers,body:JSON.stringify(body)}).then(r=>r.json()).then(j=>$('sendSmsOut').textContent=JSON.stringify(j)).catch(e=>$('sendSmsOut').textContent='send failed');
    });

    $('testCall').addEventListener('click', ()=>{
      const body={number:$('callNum').value};
      fetch('/api/test_call',{method:'POST',headers,body:JSON.stringify(body)}).then(r=>r.json()).then(j=>$('testCallOut').textContent=JSON.stringify(j)).catch(e=>$('testCallOut').textContent='failed');
    });
  });
})();
