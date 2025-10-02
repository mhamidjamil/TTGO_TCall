from flask import Flask, request, jsonify, send_file, session, send_from_directory
from flasgger import Swagger, swag_from
from flask_httpauth import HTTPTokenAuth
import requests
import json
import os
from utils import append_message, ensure_csv
from config import Config

# file to persist device IP mappings provided via ping.set_ip
DEVICE_IPS_FILE = os.path.join(os.path.dirname(__file__), 'device_ips.json')

# in-memory mapping: device_key -> { ip, remote_addr, updated }
device_ips = {}


def load_device_ips():
    global device_ips
    try:
        if os.path.exists(DEVICE_IPS_FILE):
            with open(DEVICE_IPS_FILE, 'r') as f:
                device_ips = json.load(f)
        else:
            device_ips = {}
    except Exception as e:
        print(f"[BRIDGE] Failed loading device ips: {e}")
        device_ips = {}


def save_device_ips():
    try:
        with open(DEVICE_IPS_FILE, 'w') as f:
            json.dump(device_ips, f)
    except Exception as e:
        print(f"[BRIDGE] Failed saving device ips: {e}")

app = Flask(__name__)
app.config['SWAGGER'] = {
    'title': 'ESP32 SMS Bridge',
    'uiversion': 3
}
app.secret_key = os.getenv('FLASK_SECRET', 'replace-this-with-a-secret')
swagger = Swagger(app)
auth = HTTPTokenAuth(scheme='ApiKey')

# load config from env or file
cfg = Config()

@auth.verify_token
def verify_token(token):
    # Simple token check using dashboard password
    return token == cfg.DASHBOARD_PASSWORD


def dashboard_auth_required(f):
    from functools import wraps

    @wraps(f)
    def wrapper(*args, **kwargs):
        # Accept either ApiKey Authorization or session login
        auth_header = request.headers.get('Authorization')
        if auth_header and auth_header.startswith('ApiKey '):
            token = auth_header.split(' ', 1)[1]
            if token == cfg.DASHBOARD_PASSWORD:
                return f(*args, **kwargs)
        if session.get('dashboard_auth'):
            return f(*args, **kwargs)
        return jsonify({'error': 'unauthorized'}), 401

    return wrapper


@app.route('/events', methods=['POST'])
@swag_from({
    'tags': ['events'],
    'parameters': [
        {
            'name': 'body', 'in': 'body', 'required': True,
            'schema': {'type': 'object',
                       'properties': {'type': {'type': 'string'}, 'number': {'type': 'string'}, 'body': {'type': 'string'}, 'secret': {'type': 'string'}}}
        }
    ],
    'responses': {200: {'description': 'ok'}}
})
def events():
    # Accept device posts if:
    # - Authorization header (ApiKey) valid OR
    # - X-Api-Secret header equals API_SECRET OR
    # - JSON payload contains 'secret' that equals API_SECRET
    auth_ok = False
    # token auth
    auth_header = request.headers.get('Authorization')
    if auth_header and auth_header.startswith('ApiKey '):
        token = auth_header.split(' ', 1)[1]
        if token == cfg.DASHBOARD_PASSWORD:
            auth_ok = True

    # header secret
    if not auth_ok and cfg.API_SECRET:
        xsecret = request.headers.get('X-Api-Secret')
        if xsecret and xsecret == cfg.API_SECRET:
            auth_ok = True

    j = request.get_json(force=True)
    # json secret
    if not auth_ok and cfg.API_SECRET and isinstance(j, dict):
        if j.get('secret') and j.get('secret') == cfg.API_SECRET:
            auth_ok = True

    if not auth_ok and cfg.USE_API_SECRET:
        return jsonify({'error': 'unauthorized'}), 401

    t = j.get('type')
    number = j.get('number')
    body = j.get('body')
    # Log to console for debugging with request meta
    print(f"[BRIDGE] Received event from {request.remote_addr} headers={dict(request.headers)} payload={{'type':{t},'number':{number}}} body={body}")
    # Append to csv log
    ensure_csv(cfg.LOG_FILE)
    append_message(cfg.LOG_FILE, t, number, body)
    return jsonify({'ok': True})


@app.route('/forward', methods=['POST'])
def forward_alias():
    # alias to /events for compatibility
    return events()


@app.route('/ping', methods=['GET', 'POST'])
def ping():
    # Health check + device ping. Accept POST to record device-provided 'set_ip'.
    from datetime import datetime
    print(f"[BRIDGE] Ping received from {request.remote_addr} headers={dict(request.headers)}")

    if request.method == 'POST':
        j = None
        try:
            j = request.get_json(force=True)
        except Exception:
            j = None

        # If device provides a 'set_ip', store it keyed by device identity.
        set_ip = None
        if isinstance(j, dict) and 'set_ip' in j:
            set_ip = j.get('set_ip')

        # If client asked for 'auto' or provided empty/true, try to extract public IP from headers
        if set_ip is not None:
            # normalize booleans
            if isinstance(set_ip, bool) and set_ip:
                set_ip = 'auto'
            if isinstance(set_ip, str) and set_ip.strip() == '':
                set_ip = 'auto'
            if isinstance(set_ip, (int, float)):
                set_ip = str(set_ip)

            if isinstance(set_ip, str) and set_ip.lower() == 'auto':
                xff = request.headers.get('X-Forwarded-For') or request.headers.get('X-Forwarded-For'.lower())
                if xff:
                    # X-Forwarded-For may contain a comma-separated list; take the first one
                    set_ip = xff.split(',')[0].strip()
                else:
                    set_ip = request.remote_addr

        # Determine device key: prefer X-Api-Secret (if used), else use client remote addr
        device_key = None
        xsecret = request.headers.get('X-Api-Secret')
        if xsecret:
            device_key = f"secret:{xsecret}"
        else:
            # try X-Device-Id header or fallback to remote_addr
            device_hdr = request.headers.get('X-Device-Id')
            if device_hdr:
                device_key = f"id:{device_hdr}"
            else:
                device_key = f"addr:{request.remote_addr}"

        if set_ip:
            # store mapping
            device_ips[device_key] = {
                'ip': set_ip,
                'remote_addr': request.remote_addr,
                'updated': datetime.utcnow().isoformat() + 'Z'
            }
            save_device_ips()
            print(f"[BRIDGE] Stored set_ip for {device_key} => {set_ip}")
            return jsonify({'ok': True, 'stored_ip': set_ip, 'device_key': device_key, 'server_time': datetime.utcnow().isoformat() + 'Z'})

    # default GET or POST without set_ip: return basic ok + server_time
    return jsonify({'ok': True, 'server_time': datetime.utcnow().isoformat() + 'Z'})


@app.route('/settings', methods=['GET'])
def settings():
    # Device requests settings; require API secret in header or JSON
    xsecret = request.headers.get('X-Api-Secret')
    if not xsecret or xsecret != cfg.API_SECRET:
        return jsonify({'error':'unauthorized'}), 401
    # Return server-side settings for the device
    out = {
        'useApiSecret': cfg.USE_API_SECRET,
        'apiSecret': cfg.API_SECRET,
        'forwardUrl': os.getenv('FORWARD_URL', ''),
        'forwardApiKey': os.getenv('FORWARD_API_KEY', ''),
        'allowSms': os.getenv('ALLOW_SMS', '1') == '1',
        'allowCall': os.getenv('ALLOW_CALL', '1') == '1',
        'settingsVersion': os.getenv('SETTINGS_VERSION', '1')
    }
    return jsonify(out)


@app.route('/messages', methods=['GET'])
@dashboard_auth_required
def messages_ui():
    # Simple page which allows an operator to request device messages or delete them
    page = """
<html><body>
<h3>Device Messages</h3>
<p>Use the buttons below to fetch messages from device or to delete all messages.</p>
<button onclick="fetch('/messages/list').then(r=>r.json()).then(j=>document.getElementById('out').textContent=JSON.stringify(j))">Fetch messages</button>
<button onclick="fetch('/messages/delete',{method:'POST'}).then(r=>r.text()).then(t=>document.getElementById('out').textContent=t)">Delete all messages</button>
<pre id='out'></pre>
</body></html>
"""
    return page


@app.route('/dashboard', methods=['GET'])
def dashboard_ui():
    # Serve frontend index from the frontend directory
    frontend_dir = os.path.join(os.path.dirname(__file__), 'frontend')
    return send_from_directory(frontend_dir, 'index.html')


@app.route('/dashboard/<path:filename>')
def dashboard_static(filename):
    frontend_dir = os.path.join(os.path.dirname(__file__), 'frontend')
    return send_from_directory(frontend_dir, filename)


@app.route('/dashboard/status', methods=['GET'])
@dashboard_auth_required
def dashboard_status():
    # Return server + device status for the frontend
    return jsonify({
        'device_host': cfg.DEVICE_HOST,
        'device_port': cfg.DEVICE_PORT,
        'device_ips': device_ips,
        'log_file': cfg.LOG_FILE,
        'use_api_secret': cfg.USE_API_SECRET
    })


@app.route('/dashboard/login', methods=['POST'])
def dashboard_login():
    j = request.get_json(force=True)
    pw = j.get('password') if isinstance(j, dict) else None
    if pw and pw == cfg.DASHBOARD_PASSWORD:
        session['dashboard_auth'] = True
        return jsonify({'ok': True})
    return jsonify({'error': 'unauthorized'}), 401


@app.route('/dashboard/logout', methods=['POST'])
def dashboard_logout():
    session.pop('dashboard_auth', None)
    return jsonify({'ok': True})


@app.route('/messages/list', methods=['GET'])
@dashboard_auth_required
def messages_list():
    # Proxy to device to get messages
    # choose device host: prefer stored mapping (by X-Api-Secret or X-Device-Id), else config
    device_key = None
    xsecret = request.headers.get('X-Api-Secret')
    if xsecret:
        device_key = f"secret:{xsecret}"
    else:
        device_hdr = request.headers.get('X-Device-Id')
        if device_hdr:
            device_key = f"id:{device_hdr}"
        else:
            device_key = f"addr:{request.remote_addr}"

    host_to_use = cfg.DEVICE_HOST
    if device_key in device_ips and 'ip' in device_ips[device_key] and device_ips[device_key]['ip']:
        host_to_use = device_ips[device_key]['ip']

    device_url = f"http://{host_to_use}:{cfg.DEVICE_PORT}/api/messages"
    headers = {}
    if cfg.USE_API_SECRET and cfg.API_SECRET:
        headers['X-Api-Secret'] = cfg.API_SECRET
    resp = requests.get(device_url, headers=headers, timeout=10)
    return (resp.text, resp.status_code, resp.headers.items())


@app.route('/messages/delete', methods=['POST'])
@dashboard_auth_required
def messages_delete():
    device_key = None
    xsecret = request.headers.get('X-Api-Secret')
    if xsecret:
        device_key = f"secret:{xsecret}"
    else:
        device_hdr = request.headers.get('X-Device-Id')
        if device_hdr:
            device_key = f"id:{device_hdr}"
        else:
            device_key = f"addr:{request.remote_addr}"

    host_to_use = cfg.DEVICE_HOST
    if device_key in device_ips and 'ip' in device_ips[device_key] and device_ips[device_key]['ip']:
        host_to_use = device_ips[device_key]['ip']

    device_url = f"http://{host_to_use}:{cfg.DEVICE_PORT}/api/messages/delete_all"
    headers = {}
    if cfg.USE_API_SECRET and cfg.API_SECRET:
        headers['X-Api-Secret'] = cfg.API_SECRET
    resp = requests.post(device_url, headers=headers, timeout=10)
    return (resp.text, resp.status_code, resp.headers.items())


@app.route('/send', methods=['POST'])
@swag_from({
    'tags': ['send'],
    'parameters': [
        {'name': 'body', 'in': 'body', 'required': True,
         'schema': {'type': 'object', 'properties': {'to': {'type': 'string'}, 'message': {'type': 'string'}}}}
    ],
    'responses': {200: {'description': 'sent'}}
})
@dashboard_auth_required
def send():
    j = request.get_json(force=True)
    to = j.get('to')
    msg = j.get('message')
    if not to or not msg:
        return jsonify({'error': 'missing fields'}), 400

    # proxy to ESP32 device
    device_key = None
    xsecret = request.headers.get('X-Api-Secret')
    if xsecret:
        device_key = f"secret:{xsecret}"
    else:
        device_hdr = request.headers.get('X-Device-Id')
        if device_hdr:
            device_key = f"id:{device_hdr}"
        else:
            device_key = f"addr:{request.remote_addr}"

    host_to_use = cfg.DEVICE_HOST
    if device_key in device_ips and 'ip' in device_ips[device_key] and device_ips[device_key]['ip']:
        host_to_use = device_ips[device_key]['ip']

    device_url = f"http://{host_to_use}:{cfg.DEVICE_PORT}/api/send_sms"
    headers = {'Content-Type': 'application/json'}
    if cfg.USE_API_SECRET and cfg.API_SECRET:
        headers['X-Api-Secret'] = cfg.API_SECRET

    print(f"[BRIDGE] Proxying send to device {device_url} headers={headers} payload={{'to':{to},'message':{msg}}}")
    resp = requests.post(device_url, json={'to': to, 'message': msg}, headers=headers, timeout=10)
    out = {'status_code': resp.status_code, 'text': resp.text}
    # log outgoing message
    ensure_csv(cfg.LOG_FILE)
    append_message(cfg.LOG_FILE, 'outgoing', to, msg)
    return jsonify(out), resp.status_code


@app.route('/download_log', methods=['GET'])
@dashboard_auth_required
def download_log():
    ensure_csv(cfg.LOG_FILE)
    return send_file(cfg.LOG_FILE, as_attachment=True)


@app.route('/device_ips', methods=['GET'])
@dashboard_auth_required
def list_device_ips():
    # Return the in-memory device IP mapping (persisted to device_ips.json)
    return jsonify(device_ips)


if __name__ == '__main__':
    ensure_csv(cfg.LOG_FILE)
    load_device_ips()
    app.run(host='0.0.0.0', port=cfg.SERVER_PORT)
