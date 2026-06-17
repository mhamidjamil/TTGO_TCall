# TTGO T-Call Configuration Guide

## Overview

This guide explains how to configure your TTGO T-Call device using the new SPIFFS-based configuration system. No more code changes needed for most settings!

## Quick Start

1. **Upload Code**: Flash the v7 firmware to your device
2. **Upload Dashboard**: Use "ESP32 Sketch Data Upload" to upload dashboard files
3. **Connect**: Access the web dashboard at `http://[device-ip]/dashboard`
4. **Configure**: Set your preferences via the web interface
5. **Save**: Click "Save Configuration" - settings persist across reboots

## Configuration Categories

### 1. General Settings

#### Device Information
- **Device ID**: Unique identifier for your device (e.g., "ttgo-gate-01")
- **Owner Name**: Your name or organization
- **Owner Phone**: Primary contact number (international format: +1234567890)

#### Basic Operations
- **Enable SMS Functionality**: Turn SMS features on/off
- **Enable Call Management**: Turn call handling on/off
- **Log All Messages**: Keep detailed logs of all communications
- **Delete After Forward**: Remove messages after forwarding to external services

#### Auto-Reply Settings
- **Auto-Reply Message**: Custom message sent to unknown callers
  - Example: "This is an automated gate system. Please contact +1234567890 for access."

### 2. API & Forwarding

#### External API Integration
- **Server URL**: Your main API server endpoint
  - Example: `https://api.yourcompany.com/gate`
- **API Key**: Authentication key for your server
  - Used in `X-Api-Secret` header for authentication

#### Message Forwarding
- **Forward SMS URL**: Webhook endpoint for SMS forwarding
  - Example: `https://hooks.slack.com/services/...`
  - Example: `https://api.telegram.org/bot.../sendMessage`

#### Gate ESP Integration
- **Gate ESP URL**: URL of your gate control ESP device
  - Example: `http://192.168.1.50/gate/open`
  - Used for automated gate control based on admin numbers

### 3. Notifications

#### NTFY Service Integration
[NTFY](https://ntfy.sh) provides real-time notifications to your phone/computer.

- **Enable NTFY**: Turn notifications on/off
- **NTFY Server URL**: NTFY server (use `https://ntfy.sh` for public service)
- **NTFY Topic**: Your notification channel
  - Example: `my-gate-system` (keep it unique and private)
  - You'll subscribe to this topic on your phone/computer

#### Notification Examples
When enabled, you'll receive notifications for:
- Incoming calls from admin numbers
- SMS messages received
- System status changes
- Gate access events

### 4. Security & Permissions

#### Admin Numbers
- **Admin Numbers**: Comma-separated list of authorized phone numbers
  - Example: `+1234567890,+0987654321,+1122334455`
  - These numbers can control the gate and receive auto-replies
  - Use international format with country code

#### Access Control
- **SMS Allowed**: Global SMS functionality toggle
- **Calls Allowed**: Global call handling toggle
- **Allowed Numbers**: Whitelist of numbers (use `*` for all, or comma-separated list)
  - Example: `*` (allow all)
  - Example: `+1234567890,+0987654321` (specific numbers only)

#### Security Best Practices
- Use strong, unique API keys
- Keep admin number list up to date
- Regularly review allowed numbers
- Change dashboard password from default

## Configuration Methods

### Method 1: Web Dashboard (Recommended)

1. **Access Dashboard**
   ```
   http://[device-ip]/dashboard
   ```

2. **Login**
   - Enter dashboard password (set in secrets.h)

3. **Navigate Tabs**
   - Use tab navigation for different settings categories
   - Changes are validated in real-time

4. **Save Configuration**
   - Click "Save Configuration" button
   - Settings are immediately saved to SPIFFS
   - No device restart required for most changes

### Method 2: API Calls

You can also configure via direct API calls:

**Get Current Configuration:**
```bash
curl http://[device-ip]/api/config
```

**Update Configuration:**
```bash
curl -X POST http://[device-ip]/api/config \
  -H "Content-Type: application/json" \
  -H "X-Dashboard-Auth: your_password" \
  -d '{
    "deviceId": "gate-controller-01",
    "ownerName": "John Smith",
    "ownerPhone": "+1234567890",
    "smsAllowed": true,
    "callsAllowed": true,
    "adminNumbers": "+1234567890,+0987654321"
  }'
```

### Method 3: Compile-Time Defaults

For initial setup or deployment, set defaults in `secrets.h`:

```cpp
// Device Information
#define DEVICE_ID_DEFAULT "ttgo-gate-01"
#define OWNER_NAME_DEFAULT "Gate System"
#define OWNER_PHONE_DEFAULT "+1234567890"

// Security
#define DASHBOARD_PASSWORD "your_secure_password"
#define ADMIN_NUMBERS_DEFAULT "+1234567890,+0987654321"

// Services
#define NTFY_SERVER_URL_DEFAULT "https://ntfy.sh"
#define NTFY_TOPIC_DEFAULT "my-gate-alerts"
```

## Common Configuration Scenarios

### Scenario 1: Home Gate Controller

```json
{
  "deviceId": "home-gate-01",
  "ownerName": "Smith Family",
  "ownerPhone": "+1234567890",
  "smsAllowed": true,
  "callsAllowed": true,
  "adminNumbers": "+1234567890,+1234567891,+1234567892",
  "allowedNumbers": "*",
  "gateEspUrl": "http://192.168.1.100/gate/toggle",
  "ntfyEnabled": true,
  "ntfyTopic": "smith-home-gate",
  "autoReplyMessage": "Smith residence gate system. For access, call +1234567890"
}
```

### Scenario 2: Office Building Access

```json
{
  "deviceId": "office-gate-main",
  "ownerName": "ABC Company Security",
  "ownerPhone": "+1234567890",
  "smsAllowed": true,
  "callsAllowed": true,
  "adminNumbers": "+1234567890,+1111111111",
  "allowedNumbers": "+1234567890,+1111111111,+2222222222",
  "serverUrl": "https://api.abccompany.com/gate",
  "apiKey": "abc_security_key_123",
  "gateEspUrl": "http://10.0.1.50/access/main",
  "ntfyEnabled": true,
  "ntfyTopic": "abc-office-security",
  "forwardSmsUrl": "https://hooks.slack.com/services/.../security",
  "autoReplyMessage": "ABC Company security system. Business hours: 9AM-5PM. Emergency: +1234567890"
}
```

### Scenario 3: Apartment Complex

```json
{
  "deviceId": "complex-gate-a",
  "ownerName": "Sunset Apartments Management",
  "ownerPhone": "+1234567890",
  "smsAllowed": true,
  "callsAllowed": true,
  "adminNumbers": "+1234567890,+1111111111,+2222222222",
  "allowedNumbers": "*",
  "serverUrl": "https://api.sunsetapts.com/gate",
  "gateEspUrl": "http://192.168.10.20/gate/building-a",
  "ntfyEnabled": true,
  "ntfyTopic": "sunset-apts-gate-a",
  "logMessages": true,
  "autoReplyMessage": "Sunset Apartments Gate A. For deliveries or visitors, call the office at +1234567890 or your host directly."
}
```

## Testing Your Configuration

Use the built-in testing tools in the Dashboard → Testing tab:

### 1. Test SMS
- Enter a phone number
- Sends test SMS with timestamp
- Verifies SMS functionality and number formatting

### 2. Test NTFY Notifications
- Tests connection to NTFY service
- Sends test notification to your configured topic
- Verifies notification configuration

### 3. Test Gate ESP Communication
- Tests connection to your Gate ESP device
- Sends test command to gate controller
- Verifies integration setup

### 4. Test External API
- Tests connection to your external API server
- Sends test data with authentication
- Verifies API key and endpoint configuration

## Troubleshooting Configuration

### Configuration Not Saving

**Problem**: Changes don't persist after restart

**Solutions**:
1. Check SPIFFS formatting: Enable format in Arduino IDE
2. Verify sufficient flash space for SPIFFS
3. Check serial monitor for SPIFFS errors
4. Try "Reset Configuration" and reconfigure

### Dashboard Access Issues

**Problem**: Cannot access web dashboard

**Solutions**:
1. Verify device IP address (check router/serial monitor)
2. Ensure device is connected to network
3. Check dashboard password in secrets.h
4. Try different browser/clear cache

### SMS/Call Not Working

**Problem**: Device doesn't send/receive SMS or calls

**Solutions**:
1. Verify SIM card is activated and has service
2. Check cellular signal strength
3. Ensure SMS/Call functionality is enabled in config
4. Verify admin numbers are in international format

### API Integration Problems

**Problem**: External services not receiving data

**Solutions**:
1. Test API endpoints separately (Postman/curl)
2. Verify API keys and authentication
3. Check URL formatting (include http:// or https://)
4. Review server logs for incoming requests
5. Test with simple webhook services first

### NTFY Notifications Not Working

**Problem**: Not receiving notifications

**Solutions**:
1. Verify NTFY topic name (case sensitive)
2. Check NTFY server URL
3. Test with NTFY web interface first
4. Ensure phone/computer is subscribed to topic
5. Try different topic name

## Advanced Configuration

### Configuration File Format

The SPIFFS configuration is stored as JSON in `/config.json`:

```json
{
  "deviceId": "ttgo-001",
  "ownerName": "Device Owner",
  "ownerPhone": "+1234567890",
  "logMessages": true,
  "serverUrl": "https://api.example.com",
  "apiKey": "your-api-key",
  "gateEspUrl": "http://192.168.1.100",
  "forwardSmsUrl": "https://webhook.example.com",
  "ntfyEnabled": true,
  "ntfyUrl": "https://ntfy.sh",
  "ntfyTopic": "your-topic",
  "autoReplyMessage": "Auto reply message",
  "smsAllowed": true,
  "callsAllowed": true,
  "adminNumbers": "+1234567890,+0987654321",
  "allowedNumbers": "*"
}
```

### Configuration Hierarchy

1. **Compile-time defaults** (secrets.h) - Always available
2. **SPIFFS storage** (/config.json) - Persistent across reboots
3. **Runtime memory** - Active configuration

### Backup and Restore

**Backup Configuration**:
```bash
curl http://[device-ip]/api/config > config_backup.json
```

**Restore Configuration**:
```bash
curl -X POST http://[device-ip]/api/config \
  -H "Content-Type: application/json" \
  -H "X-Dashboard-Auth: password" \
  -d @config_backup.json
```

### Bulk Deployment

For multiple devices:

1. Configure one device completely
2. Export configuration via API
3. Modify device-specific fields (deviceId, etc.)
4. Deploy to other devices via API calls

### Configuration Validation

The system validates:
- Phone number formats
- URL formats
- Required field presence
- Data type correctness
- String length limits

Invalid configurations are rejected with error messages.

## Security Considerations

### Dashboard Security
- Change default password in secrets.h
- Use strong, unique passwords
- Consider VPN access for remote management

### API Security
- Use unique API keys
- Rotate keys regularly
- Monitor API access logs
- Implement IP restrictions if needed

### Phone Number Security
- Keep admin numbers list private
- Regularly audit authorized numbers
- Use international format consistently
- Monitor for unauthorized access attempts

### Network Security
- Use WPA2/WPA3 WiFi security
- Consider isolated IoT network
- Implement firewall rules
- Monitor network traffic

## Maintenance

### Regular Tasks
1. **Review Configuration**: Monthly review of all settings
2. **Update Admin Numbers**: Keep authorization list current
3. **Monitor Logs**: Check for unusual activity
4. **Test Functionality**: Regular testing of all features
5. **Backup Configuration**: Regular configuration backups

### Updates and Upgrades
1. **Firmware Updates**: Keep device firmware current
2. **Configuration Migration**: Follow upgrade guides
3. **Compatibility Testing**: Test after updates
4. **Rollback Plan**: Keep working configuration backup

## Support and Resources

### Documentation
- Main README: Project overview and setup
- API Documentation: Complete API reference
- Hardware Guide: Physical setup instructions

### Debugging
- Enable debug output in secrets.h
- Monitor serial console for detailed logs
- Use dashboard testing tools
- Check system status via /api/status

### Common Support Issues
1. Configuration not persisting → SPIFFS issues
2. Network connectivity → WiFi/cellular setup
3. Authentication failures → Password/API key issues
4. Service integration → URL/endpoint configuration

For additional support, enable debug logging and review the serial console output for detailed error information.