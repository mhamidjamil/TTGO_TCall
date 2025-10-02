from flask import Flask, request, jsonify, send_file
from flasgger import Swagger, swag_from
from flask_httpauth import HTTPTokenAuth
import requests
import os
from utils import append_message, ensure_csv
from config import Config

app = Flask(__name__)
app.config['SWAGGER'] = {
    'title': 'ESP32 SMS Bridge',
    'uiversion': 3
}
swagger = Swagger(app)
auth = HTTPTokenAuth(scheme='ApiKey')

# load config from env or file
cfg = Config()

@auth.verify_token
def verify_token(token):
    # Simple token check using dashboard password
    return token == cfg.DASHBOARD_PASSWORD


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


@app.route('/ping', methods=['GET'])
def ping():
    # Simple health check - returns OK and server time
    from datetime import datetime
    print(f"[BRIDGE] Ping received from {request.remote_addr} headers={dict(request.headers)}")
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
@auth.login_required
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


@app.route('/messages/list', methods=['GET'])
@auth.login_required
def messages_list():
    # Proxy to device to get messages
    device_url = f"http://{cfg.DEVICE_HOST}:{cfg.DEVICE_PORT}/api/messages"
    headers = {}
    if cfg.USE_API_SECRET and cfg.API_SECRET:
        headers['X-Api-Secret'] = cfg.API_SECRET
    resp = requests.get(device_url, headers=headers, timeout=10)
    return (resp.text, resp.status_code, resp.headers.items())


@app.route('/messages/delete', methods=['POST'])
@auth.login_required
def messages_delete():
    device_url = f"http://{cfg.DEVICE_HOST}:{cfg.DEVICE_PORT}/api/messages/delete_all"
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
@auth.login_required
def send():
    j = request.get_json(force=True)
    to = j.get('to')
    msg = j.get('message')
    if not to or not msg:
        return jsonify({'error': 'missing fields'}), 400

    # proxy to ESP32 device
    device_url = f"http://{cfg.DEVICE_HOST}:{cfg.DEVICE_PORT}/api/send_sms"
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
@auth.login_required
def download_log():
    ensure_csv(cfg.LOG_FILE)
    return send_file(cfg.LOG_FILE, as_attachment=True)


if __name__ == '__main__':
    ensure_csv(cfg.LOG_FILE)
    app.run(host='0.0.0.0', port=cfg.SERVER_PORT)
