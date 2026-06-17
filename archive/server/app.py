from flask import Flask, request, jsonify, send_file, session, send_from_directory
from flasgger import Swagger, swag_from
from flask_httpauth import HTTPTokenAuth
from flask_sock import Sock
import requests
import json
import os
from utils import append_message, ensure_csv
from config import Config
import threading
import time
import paho.mqtt.client as mqtt
import os

# file to persist device IP mappings provided via ping.set_ip
DEVICE_IPS_FILE = os.path.join(os.path.dirname(__file__), 'device_ips.json')

# in-memory mapping: device_key -> { ip, remote_addr, updated }
device_ips = {}
connected_clients = 0
known_devices = set()
mqtt_client = None


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


def on_mqtt_connect(client, userdata, flags, rc):
    print(f"[MQTT] Connected with result code {rc}")
    # subscribe to events/status/auth topics for all devices so we learn connected devices
    client.subscribe("ttgo/+/events")
    client.subscribe("ttgo/+/status")
    client.subscribe("ttgo/+/auth")


def on_mqtt_message(client, userdata, msg):
    try:
        payload = msg.payload.decode('utf-8')
        print(f"[MQTT] Message on {msg.topic}: {payload}")
        # topic: ttgo/<clientId>/events
        parts = msg.topic.split('/')
        if len(parts) >= 3:
            clientId = parts[1]
            topic_tail = parts[2]
            # treat status/auth/events as device presence indicators
            if topic_tail in ('events', 'status', 'auth'):
                known_devices.add(clientId)
        # try parse JSON and log
        j = json.loads(payload)
        t = j.get('type')
        number = j.get('number')
        body = j.get('body')
        if t and number and body:
            ensure_csv(cfg.LOG_FILE)
            append_message(cfg.LOG_FILE, t, number, body)
            print(f"[MQTT-BRIDGE] Logged event: {t} from {number}")
                # NOTE: no ntfy notification here — notifications are only sent
                # when a forwarded device-originated POST hits /forward
    except Exception as e:
        print(f"[MQTT] Error handling message: {e}")


def start_mqtt():
    global mqtt_client
    broker = os.getenv('MQTT_BROKER', 'test.mosquitto.org')
    port = int(os.getenv('MQTT_PORT', '1883'))
    mqtt_client = mqtt.Client()
    mqtt_client.on_connect = on_mqtt_connect
    mqtt_client.on_message = on_mqtt_message
    try:
        mqtt_client.connect(broker, port, 60)
    except Exception as e:
        print(f"[MQTT] Failed to connect to {broker}:{port} - {e}")
        return
    thread = threading.Thread(target=mqtt_client.loop_forever, daemon=True)
    thread.start()


def send_ntfy_notification(number, body):
    """Send an ntfy notification for incoming SMS if NTFY_SERVER_URL and SMS_NTFY_TOPIC are set in env."""
    ntfy_url = os.getenv('NTFY_SERVER_URL')
    topic = os.getenv('SMS_NTFY_TOPIC')
    if not ntfy_url or not topic:
        return False
    try:
        # Compose a simple text notification
        text = f"SMS from {number}: {body}"
        url = ntfy_url.rstrip('/') + '/' + topic
        headers = {'Content-Type': 'text/plain'}
        # Optionally include title header for ntfy
        headers['X-Title'] = 'Incoming SMS'
        resp = requests.post(url, data=text.encode('utf-8'), headers=headers, timeout=5)
        print(f"[NTFY] POST {url} -> {resp.status_code}")
        return resp.ok
    except Exception as e:
        print(f"[NTFY] Error sending notification: {e}")
        return False

app = Flask(__name__)
app.config['SWAGGER'] = {
    'title': 'ESP32 SMS Bridge',
    'uiversion': 3
}
app.secret_key = os.getenv('FLASK_SECRET', 'replace-this-with-a-secret')
swagger = Swagger(app)
auth = HTTPTokenAuth(scheme='ApiKey')
sock = Sock(app)

# load config from env or file
cfg = Config()

# List of connected WebSocket clients
connected_ws = []

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
        # No NTFY here — /forward endpoint is considered authoritative for
        # device-originated events and will trigger notifications.
    return jsonify({'ok': True})


@app.route('/forward', methods=['POST'])
def forward_alias():
    # alias to /events for compatibility, but also treat /forward as the
    # authoritative device-originated event that should trigger NTFY.
    # First let events() perform the logging and auth checks.
    resp = events()
    try:
        j = request.get_json(force=True)
        t = j.get('type')
        number = j.get('number')
        body = j.get('body')
        if t and number and body:
            print(f"[FORWARD] forwarding event -> notify ntfy: type={t} number={number}")
            ok = send_ntfy_notification(number, body)
            print(f"[FORWARD] ntfy send result: {ok}")
    except Exception as e:
        print(f"[FORWARD] Failed to notify ntfy: {e}")
    return resp


@app.route('/ping', methods=['GET', 'POST'])
def ping():
    from datetime import datetime
    print(f"[BRIDGE] Ping received from {request.remote_addr} headers={dict(request.headers)}")

    # Always try to extract public IP from headers, regardless of method
    public_ip = request.headers.get('X-Forwarded-For')
    if public_ip:
        public_ip = public_ip.split(',')[0].strip()
    else:
        public_ip = request.headers.get('Cf-Connecting-Ip')
    if not public_ip:
        public_ip = request.remote_addr

    # Save public IP to ip-config.txt
    try:
        with open("ip-config.txt", "w") as file:
            file.write(public_ip)
        print(f"[BRIDGE] Updated ip-config.txt with public IP: {public_ip}")
    except Exception as e:
        print(f"[BRIDGE] Failed to update ip-config.txt: {e}")

    # For POST, handle set_ip logic as before
    if request.method == 'POST':
        j = None
        try:
            j = request.get_json(force=True)
        except Exception:
            j = None
        set_ip = None
        if isinstance(j, dict) and 'set_ip' in j:
            set_ip = j.get('set_ip')
            print(f"\nset_ip provided: {set_ip}\n")
        else:
            print("\nno json body\n")

        # If client asked for 'auto' or provided empty/true, try to extract public IP from headers
        if set_ip is not None:
            # Extract public IP from headers if 'auto' is specified
            if isinstance(set_ip, str) and set_ip.lower() == 'auto':
                xff = request.headers.get('X-Forwarded-For') or request.headers.get('X-Forwarded-For'.lower())
                if xff:
                    # X-Forwarded-For may contain a comma-separated list; take the first one
                    set_ip = xff.split(',')[0].strip()
                else:
                    set_ip = request.remote_addr

            # Ensure set_ip is valid
            if set_ip:
                # Update the IP in ip-config.txt
                try:
                    with open("ip-config.txt", "w") as file:
                        file.write(set_ip)
                    print(f"[BRIDGE] Updated ip-config.txt with IP: {set_ip}")
                except Exception as e:
                    print(f"[BRIDGE] Failed to update ip-config.txt: {e}")
        else:
            print("\nno set_ip provided\n")

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

    # default GET or POST without set_ip: return basic ok + server_time
    return jsonify({'ok': True, 'public_ip': public_ip, 'server_time': datetime.utcnow().isoformat() + 'Z'})


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
        'use_api_secret': cfg.USE_API_SECRET,
        'websocket_connected': len(connected_ws) > 0
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
def send():
    j = request.get_json(force=True)
    to = j.get('to')
    msg = j.get('message')
    if not to or not msg:
        return jsonify({'error': 'missing fields'}), 400

    # Send via MQTT to device(s) if available
    message = json.dumps({'command': 'send_sms', 'to': to, 'message': msg})
    published = False
    if mqtt_client:
        # If request provided a specific X-Device-Id header, target that device
        device_hdr = request.headers.get('X-Device-Id')
        if device_hdr:
            topic = f"ttgo/{device_hdr}/cmd"
            try:
                mqtt_client.publish(topic, message)
                print(f"[BRIDGE] Published SMS to {topic}")
                published = True
            except Exception as e:
                print(f"[BRIDGE] Failed to publish to {topic}: {e}")

        # If we haven't published yet, publish to known devices
        if not published and known_devices:
            for clientId in list(known_devices):
                topic = f"ttgo/{clientId}/cmd"
                try:
                    mqtt_client.publish(topic, message)
                    print(f"[BRIDGE] Published SMS to {topic}")
                    published = True
                except Exception as e:
                    print(f"[BRIDGE] Failed to publish to {topic}: {e}")

        # If still not published, send to a broadcast topic which devices may subscribe to
        if not published:
            try:
                mqtt_client.publish('ttgo/all/cmd', message)
                print("[BRIDGE] Published SMS to broadcast ttgo/all/cmd")
                published = True
            except Exception as e:
                print(f"[BRIDGE] Failed to publish broadcast: {e}")
    # Try HTTP proxy to device only if explicitly enabled via env ENABLE_HTTP_PROXY=1
    http_sent = False
    if os.getenv('ENABLE_HTTP_PROXY', '0') == '1':
        try:
            print("[BRIDGE] HTTP proxy enabled - attempting HTTP proxy to device")
            # Determine device key similar to messages_list
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

            if host_to_use:
                device_url = f"http://{host_to_use}:{cfg.DEVICE_PORT}/api/send_sms"
                headers = {'Content-Type': 'application/json'}
                if cfg.USE_API_SECRET and cfg.API_SECRET:
                    headers['X-Api-Secret'] = cfg.API_SECRET
                resp = requests.post(device_url, headers=headers, json={'to': to, 'message': msg}, timeout=10)
                print(f"[BRIDGE] HTTP proxy POST to {device_url} -> {resp.status_code}")
                if resp.ok:
                    http_sent = True
        except Exception as e:
            print(f"[BRIDGE] HTTP proxy failed: {e}")
    else:
        print("[BRIDGE] HTTP proxy disabled by configuration; skipping HTTP attempts")

    # If still not delivered via MQTT or HTTP, fallback to WebSocket clients
    if not published and not http_sent:
        print(f"[BRIDGE] Sending SMS via WebSocket to {to}: {msg}")
        for ws in connected_ws:
            try:
                ws.send(message)
                print(f"[BRIDGE] Sent to WebSocket client")
            except Exception as e:
                print(f"[BRIDGE] Failed to send to WebSocket: {e}")
    
    # Log outgoing message
    ensure_csv(cfg.LOG_FILE)
    append_message(cfg.LOG_FILE, 'outgoing', to, msg)
    if http_sent:
        return jsonify({'status_code': 200, 'text': 'Sent via HTTP to device'}), 200
    if mqtt_client and published:
        return jsonify({'status_code': 200, 'text': 'Published via MQTT'}), 200
    if connected_ws:
        return jsonify({'status_code': 200, 'text': 'Sent via WebSocket'}), 200
    return jsonify({'status_code': 200, 'text': 'Queued (no active transport)'}), 200


@app.route('/download_log', methods=['GET'])
def download_log():
    ensure_csv(cfg.LOG_FILE)
    return send_file(cfg.LOG_FILE, as_attachment=True)


def _parse_date_param(s: str):
    """Parse a YYYY-MM-DD date string to a date object. Return None if s is falsy."""
    if not s:
        return None
    try:
        from datetime import datetime
        return datetime.strptime(s, '%Y-%m-%d').date()
    except Exception:
        return None


def _filter_log_rows(start_date, end_date):
    """Yield rows from the log file whose timestamp date is between start_date and end_date inclusive.

    start_date/end_date are datetime.date objects. If either is None it's unbounded on that side.
    """
    import csv
    from datetime import datetime
    ensure_csv(cfg.LOG_FILE)
    with open(cfg.LOG_FILE, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            ts = row.get('timestamp')
            if not ts:
                continue
            # handle ISO format with optional Z
            try:
                if ts.endswith('Z'):
                    ts = ts[:-1]
                dt = datetime.fromisoformat(ts)
            except Exception:
                # fallback: try parse as date
                try:
                    dt = datetime.strptime(ts, '%Y-%m-%d')
                except Exception:
                    continue
            d = dt.date()
            if start_date and d < start_date:
                continue
            if end_date and d > end_date:
                continue
            yield row


@app.route('/logs', methods=['GET'])
def logs_view():
    """Return JSON list of log rows filtered by date range.

    Query parameters:
      - start=YYYY-MM-DD
      - end=YYYY-MM-DD
      - days=N   (if provided and start omitted, start = today - (N-1) days)

    If no params are provided, defaults to last 7 days.
    """
    from datetime import date, timedelta

    start_q = request.args.get('start')
    end_q = request.args.get('end')
    days_q = request.args.get('days')

    end_date = _parse_date_param(end_q)
    start_date = _parse_date_param(start_q)

    if not start_date and days_q:
        try:
            n = int(days_q)
            end_date = end_date or date.today()
            start_date = end_date - timedelta(days=max(n-1, 0))
        except Exception:
            return jsonify({'error': 'invalid days parameter'}), 400

    # default last 7 days
    if not start_date and not end_date:
        end_date = date.today()
        start_date = end_date - timedelta(days=6)

    # if only start provided, set end to start
    if start_date and not end_date:
        end_date = start_date

    rows = list(_filter_log_rows(start_date, end_date))
    return jsonify({'start': start_date.isoformat(), 'end': end_date.isoformat(), 'count': len(rows), 'rows': rows})


@app.route('/logs/download', methods=['GET'])
def logs_download():
    """Download filtered CSV of logs using same query parameters as /logs."""
    from datetime import date, timedelta

    start_q = request.args.get('start')
    end_q = request.args.get('end')
    days_q = request.args.get('days')

    end_date = _parse_date_param(end_q)
    start_date = _parse_date_param(start_q)
    if not start_date and days_q:
        try:
            n = int(days_q)
            end_date = end_date or date.today()
            start_date = end_date - timedelta(days=max(n-1, 0))
        except Exception:
            return jsonify({'error': 'invalid days parameter'}), 400
    if not start_date and not end_date:
        end_date = date.today()
        start_date = end_date - timedelta(days=6)
    if start_date and not end_date:
        end_date = start_date

    # create a temporary CSV in-memory
    import io, csv
    output = io.StringIO()
    writer = None
    count = 0
    for row in _filter_log_rows(start_date, end_date):
        if writer is None:
            fieldnames = list(row.keys())
            writer = csv.DictWriter(output, fieldnames=fieldnames)
            writer.writeheader()
        writer.writerow(row)
        count += 1
    output.seek(0)
    # return as attachment
    return send_file(io.BytesIO(output.getvalue().encode('utf-8')), mimetype='text/csv', as_attachment=True, download_name=f'filtered_logs_{start_date.isoformat()}_{end_date.isoformat()}.csv')


@app.route('/device_ips', methods=['GET'])
def list_device_ips():
    # Return the in-memory device IP mapping (persisted to device_ips.json)
    return jsonify(device_ips)


@sock.route('/ws')
def websocket_route(ws):
    connected_ws.append(ws)
    print(f"[WS] Client connected, total: {len(connected_ws)}")

    try:
        while True:
            data = ws.receive()
            print(f"[WS] Received: {data}")
            try:
                event = json.loads(data)
                # Handle event, e.g., SMS received
                t = event.get('type')
                number = event.get('number')
                body = event.get('body')
                if t and number and body:
                    ensure_csv(cfg.LOG_FILE)
                    append_message(cfg.LOG_FILE, t, number, body)
                    print(f"[BRIDGE] Logged event: {t} from {number}")
                    # No NTFY from websockets; /forward handles device-originated notifications
                    try:
                        pass
                    except Exception:
                        pass
            except json.JSONDecodeError:
                print("[WS] Invalid JSON received")
    except Exception as e:
        print(f"[WS] Client disconnected: {e}")
    finally:
        connected_ws.remove(ws)
        print(f"[WS] Client disconnected, total: {len(connected_ws)}")



if __name__ == '__main__':
    ensure_csv(cfg.LOG_FILE)
    load_device_ips()
    # Start MQTT client in background so /send can publish to devices over MQTT
    start_mqtt()
    app.run(host='0.0.0.0', port=cfg.SERVER_PORT)
