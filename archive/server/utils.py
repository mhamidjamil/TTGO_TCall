import os
import csv
from datetime import datetime


def ensure_csv(path):
    if not os.path.exists(path):
        with open(path, 'w', newline='', encoding='utf-8') as f:
            writer = csv.writer(f)
            writer.writerow(['timestamp', 'type', 'number', 'body'])


def append_message(path, mtype, number, body):
    ensure_csv(path)
    ts = datetime.utcnow().isoformat() + 'Z'
    with open(path, 'a', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow([ts, mtype, number, body])
