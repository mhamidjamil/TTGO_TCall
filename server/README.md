# ESP32 SMS Bridge (Flask)

This lightweight Flask server acts as a bridge and logger for the ESP32 SMS module. It provides:

- /events — endpoint to receive forwarded events from the ESP32 (incoming SMS/call)
- /send — endpoint to send SMS via the ESP32 (proxies to the device's /api/send_sms)
- /download_log — download the messages Excel log
- /docs — Swagger UI documenting the APIs (protected by the dashboard password)

Requirements
- Python 3.8+
- Install dependencies from requirements.txt

Usage
1. Copy and edit environment variables (optional):

   export DEVICE_HOST=192.168.100.90
   export DEVICE_PORT=420
   export DASHBOARD_PASSWORD=your_dashboard_pw
   export USE_API_SECRET=1  # if the ESP32 expects X-Api-Secret
   export API_SECRET=your_api_secret
   export LOG_FILE=messages.xlsx

2. Install dependencies and run:

   pip install -r requirements.txt
   python app.py

3. Open Swagger UI at http://localhost:5000/apidocs (or /docs depending on flasgger). Use the Dashboard password as the API token when prompted.

Notes
- The server stores messages in 'messages.xlsx' by default. Both incoming and outgoing messages are appended with UTC timestamp.
- The /send endpoint proxies to the ESP32's /api/send_sms endpoint. Configure DEVICE_HOST and DEVICE_PORT accordingly.
