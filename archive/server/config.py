import os
from dotenv import load_dotenv

load_dotenv()

class Config:
    # ESP32 device host/port
    # DEVICE_HOST = os.getenv('DEVICE_HOST', '192.168.100.90')
    DEVICE_PORT = int(os.getenv('DEVICE_PORT', '420'))

    # Dashboard password used by ESP32 and the Flask docs protection
    DASHBOARD_PASSWORD = os.getenv('DASHBOARD_PASSWORD', 'admin123')

    # If the ESP32 expects API secret header, the bridge can provide it
    USE_API_SECRET = os.getenv('USE_API_SECRET', '0') == '1'
    API_SECRET = os.getenv('API_SECRET', '')

    # Forwarding defaults that can be passed to device via /settings
    FORWARD_URL = os.getenv('FORWARD_URL', os.getenv('FORWARD_URL_DEFAULT', ''))
    FORWARD_API_KEY = os.getenv('FORWARD_API_KEY', '')
    ALLOW_SMS = os.getenv('ALLOW_SMS', '1') == '1'
    ALLOW_CALL = os.getenv('ALLOW_CALL', '1') == '1'
    SETTINGS_VERSION = os.getenv('SETTINGS_VERSION', os.getenv('SETTINGS_VERSION_DEFAULT', '1'))

    # Local flask server port
    SERVER_PORT = int(os.getenv('SERVER_PORT', '5000'))

    # Log file
    LOG_FILE = os.getenv('LOG_FILE', 'messages.csv')

# Function to update the IP in ip-config.txt
def update_device_ip(new_ip):
    ip_config_path = "ip-config.txt"
    try:
        with open(ip_config_path, "w") as file:
            file.write(new_ip)
        print(f"[CONFIG] Updated DEVICE_HOST to {new_ip}")
    except Exception as e:
        print(f"[CONFIG] Failed to update IP: {e}")

# Function to load the IP from ip-config.txt
def load_device_ip():
    ip_config_path = "ip-config.txt"
    try:
        with open(ip_config_path, "r") as file:
            return file.read().strip()
    except FileNotFoundError:
        print("[CONFIG] ip-config.txt not found. Using default IP.")
        try:
            with open(ip_config_path, "w") as file:
                file.write('192.168.100.90')
            print("[CONFIG] Created ip-config.txt with default IP.")
        except Exception as e:
            print(f"[CONFIG] Failed to create ip-config.txt: {e}")
    except Exception as e:
        print(f"[CONFIG] Failed to load IP: {e}")
    return '192.168.100.90'

# Update DEVICE_HOST dynamically
Config.DEVICE_HOST = load_device_ip()
