# TTGO T-Call API Documentation

## Base URL
```
http://[device-ip-address]/
```

## Authentication

### Dashboard Authentication
Protected endpoints require dashboard authentication via:
- **Header**: `X-Dashboard-Auth: [password]`
- **Form Parameter**: `pw=[password]`

### API Key Authentication
Some endpoints support API key authentication:
- **Header**: `X-Api-Secret: [api-key]`

## Configuration API

### GET /api/config
Get current device configuration.

**Authentication**: None (sensitive fields masked without auth)

**Response**: 200 OK
```json
{
  "deviceId": "ttgo-001",
  "ownerName": "Device Owner",
  "ownerPhone": "+1234567890",
  "logMessages": true,
  "serverUrl": "https://api.example.com",
  "apiKey": "*****",
  "gateEspUrl": "http://192.168.1.100",
  "forwardSmsUrl": "https://webhook.example.com/sms",
  "ntfyEnabled": true,
  "ntfyUrl": "https://ntfy.sh",
  "ntfyTopic": "ttgo-alerts",
  "autoReplyMessage": "This is an automated response...",
  "smsAllowed": true,
  "callsAllowed": true,
  "adminNumbers": "+1234567890,+0987654321",
  "allowedNumbers": "*",
  "useApiSecret": true,
  "apiSecret": "*****",
  "forwardUrl": "https://webhook.example.com/sms",
  "forwardApiKey": "*****",
  "allowSms": true,
  "allowCall": true
}
```

**Notes**:
- Sensitive fields (apiKey, apiSecret) are masked unless authenticated
- Legacy field mappings provided for backward compatibility

### POST /api/config
Save device configuration.

**Authentication**: Dashboard password required

**Request Body**:
```json
{
  "deviceId": "ttgo-001",
  "ownerName": "New Owner Name",
  "ownerPhone": "+1234567890",
  "logMessages": true,
  "serverUrl": "https://new-api.example.com",
  "apiKey": "new-api-key-123",
  "gateEspUrl": "http://192.168.1.200",
  "forwardSmsUrl": "https://new-webhook.example.com/sms",
  "ntfyEnabled": true,
  "ntfyUrl": "https://ntfy.sh",
  "ntfyTopic": "ttgo-notifications",
  "autoReplyMessage": "Updated auto-reply message",
  "smsAllowed": true,
  "callsAllowed": true,
  "adminNumbers": "+1234567890,+1111111111",
  "allowedNumbers": "+1234567890,+2222222222"
}
```

**Response**: 200 OK
```json
{
  "success": true,
  "message": "Configuration saved successfully"
}
```

**Error Response**: 400/500
```json
{
  "success": false,
  "error": "Failed to save configuration"
}
```

### POST /api/config/reset
Reset configuration to default values.

**Authentication**: Dashboard password required

**Response**: 200 OK
```json
{
  "success": true,
  "message": "Configuration reset to defaults"
}
```

## Device Status API

### GET /api/status
Get current device status and system information.

**Authentication**: None

**Response**: 200 OK
```json
{
  "version": "v7.0.0",
  "uptime": "3600 seconds",
  "freeMemory": "125432 bytes",
  "networkStatus": "Connected",
  "signalStrength": "Unknown",
  "lastRestart": "System boot",
  "configSource": "SPIFFS"
}
```

### GET /api/auth
Check authentication status.

**Authentication**: Dashboard password (optional)

**Response**: 200 OK
```json
{
  "authenticated": true
}
```

## Communication API

### POST /api/send_sms
Send SMS message.

**Authentication**: API key (if configured)

**Request Body**:
```json
{
  "to": "+1234567890",
  "message": "Hello from TTGO T-Call"
}
```

**Response**: 200 OK
```json
{
  "ok": true
}
```

**Error Response**: 400/401
```json
{
  "ok": false,
  "error": "invalid number"
}
```

**Notes**:
- Phone numbers are automatically normalized
- Supports international formats
- Pakistan local numbers (+92) handled specially

### POST /api/call
Initiate phone call (placeholder).

**Authentication**: API key (if configured)

**Request Body**:
```json
{
  "to": "+1234567890"
}
```

**Response**: 200 OK
```json
{
  "ok": true
}
```

### POST /api/hangup
Hang up current call.

**Authentication**: API key (if configured)

**Response**: 200 OK
```json
{
  "ok": true
}
```

## Testing API

### POST /api/test/sms
Send test SMS message.

**Authentication**: Dashboard password required

**Request Body**:
```json
{
  "number": "+1234567890"
}
```

**Response**: 200 OK
```json
{
  "success": true,
  "message": "Test SMS sent successfully"
}
```

**Error Response**: 400
```json
{
  "success": false,
  "error": "SMS manager not available"
}
```

### POST /api/test/call
Test call functionality.

**Authentication**: Dashboard password required

**Request Body**:
```json
{
  "number": "+1234567890"
}
```

**Response**: 200 OK
```json
{
  "success": true,
  "message": "Call test completed (simulated)"
}
```

### POST /api/test/ntfy
Test NTFY notification service.

**Authentication**: Dashboard password required

**Response**: 200 OK
```json
{
  "success": true,
  "httpCode": 200,
  "message": "NTFY test notification sent"
}
```

**Error Response**: 400
```json
{
  "success": false,
  "httpCode": 404,
  "message": "NTFY test failed",
  "error": "Not Found"
}
```

### POST /api/test/gate-esp
Test Gate ESP communication.

**Authentication**: Dashboard password required

**Response**: 200 OK
```json
{
  "success": true,
  "httpCode": 200,
  "message": "Gate ESP communication successful"
}
```

**Error Response**: 400
```json
{
  "success": false,
  "httpCode": 500,
  "message": "Gate ESP communication failed",
  "error": "Connection timeout"
}
```

## System Management API

### POST /api/restart
Restart the device.

**Authentication**: Dashboard password required

**Response**: 200 OK
```json
{
  "success": true,
  "message": "Device restart initiated"
}
```

**Notes**:
- Device will restart after 1 second delay
- All connections will be terminated

### POST /api/logs/clear
Clear system logs.

**Authentication**: Dashboard password required

**Response**: 200 OK
```json
{
  "success": true,
  "message": "Logs cleared successfully"
}
```

### GET /api/logs/download
Download system logs as text file.

**Authentication**: Dashboard password required

**Response**: 200 OK
```
Content-Type: text/plain
Content-Disposition: attachment; filename=ttgo-logs.txt

TTGO T-Call System Logs
Generated: 1234567ms since boot
Free Memory: 125432 bytes
Configuration loaded from SPIFFS
System operational
```

## Legacy API Endpoints

### POST /api/test_forward
Test SMS forwarding (legacy).

**Authentication**: Dashboard password required

**Response**: 200 OK
```json
{
  "code": 200,
  "response": "OK",
  "ok": true
}
```

### POST /api/test_call
Simulate incoming call (legacy).

**Authentication**: Dashboard password required

**Response**: 200 OK
```json
{
  "code": 200,
  "response": "OK",
  "ok": true
}
```

### POST /api/check_settings
Check and apply remote settings.

**Authentication**: Dashboard password required

**Response**: 200 OK
```json
{
  "ok": true,
  "applied": false
}
```

### GET /api/messages
List all SMS messages (if SMS manager available).

**Authentication**: API key (X-Api-Secret header)

**Response**: 200 OK
```json
[
  {
    "id": 1,
    "from": "+1234567890",
    "message": "Hello",
    "timestamp": "2023-01-01T12:00:00Z"
  }
]
```

### POST /api/messages/delete_all
Delete all SMS messages.

**Authentication**: API key (X-Api-Secret header)

**Response**: 200 OK
```json
{
  "ok": true
}
```

## Web Interface

### GET /dashboard
Access the web dashboard interface.

**Authentication**: Dashboard password (form-based or header)

**Response**: 200 OK (HTML)
- Comprehensive tabbed interface
- Real-time configuration management
- Testing and diagnostic tools

### GET /
Root endpoint information.

**Response**: 200 OK (Plain Text)
```
SMS & Calls module running
```

### GET /docs
API documentation interface.

**Authentication**: Dashboard password required

**Response**: 200 OK (HTML)
- Interactive API documentation
- Request examples
- Authentication helpers

## Error Codes

| Code | Description |
|------|-------------|
| 200  | OK - Request successful |
| 400  | Bad Request - Invalid parameters or JSON |
| 401  | Unauthorized - Authentication required |
| 404  | Not Found - Endpoint doesn't exist |
| 500  | Internal Server Error - Server-side error |

## Rate Limiting

Currently no rate limiting implemented. Consider implementing for production use.

## CORS Policy

CORS headers not currently set. Add if needed for web applications.

## Request/Response Format

- All API requests and responses use JSON format
- Content-Type: application/json required for POST requests
- Responses include appropriate HTTP status codes
- Error responses include descriptive error messages

## Example Usage

### cURL Examples

**Get Configuration:**
```bash
curl -X GET http://192.168.1.100/api/config
```

**Save Configuration:**
```bash
curl -X POST http://192.168.1.100/api/config \
  -H "Content-Type: application/json" \
  -H "X-Dashboard-Auth: your_password" \
  -d '{"deviceId":"ttgo-001","ownerName":"John Doe"}'
```

**Send SMS:**
```bash
curl -X POST http://192.168.1.100/api/send_sms \
  -H "Content-Type: application/json" \
  -H "X-Api-Secret: your_api_key" \
  -d '{"to":"+1234567890","message":"Test message"}'
```

**Test NTFY:**
```bash
curl -X POST http://192.168.1.100/api/test/ntfy \
  -H "X-Dashboard-Auth: your_password"
```

### JavaScript Examples

**Load Configuration:**
```javascript
fetch('/api/config')
  .then(response => response.json())
  .then(config => {
    console.log('Current config:', config);
  });
```

**Save Configuration:**
```javascript
const config = {
  deviceId: 'ttgo-001',
  ownerName: 'New Name'
};

fetch('/api/config', {
  method: 'POST',
  headers: {
    'Content-Type': 'application/json',
    'X-Dashboard-Auth': 'your_password'
  },
  body: JSON.stringify(config)
})
.then(response => response.json())
.then(result => {
  console.log('Save result:', result);
});
```

## Security Considerations

1. **Change Default Password**: Update DASHBOARD_PASSWORD in secrets.h
2. **Use HTTPS**: Consider HTTPS for production deployments
3. **API Key Rotation**: Regularly update API keys
4. **Network Security**: Use VPN or firewall rules to restrict access
5. **Input Validation**: All inputs are validated server-side

## Troubleshooting API Issues

### Common Problems

**401 Unauthorized:**
- Verify dashboard password or API key
- Check header format and spelling
- Ensure authentication is properly configured

**400 Bad Request:**
- Validate JSON format
- Check required fields are present
- Verify parameter types and values

**500 Internal Server Error:**
- Check device memory and storage
- Verify SPIFFS is properly formatted
- Review serial console for error messages

**Connection Issues:**
- Verify device IP address
- Check network connectivity
- Ensure device is powered and running

### Debug Steps

1. Enable debug logging in secrets.h
2. Monitor serial console output
3. Test with simple GET requests first
4. Verify authentication separately
5. Check network configuration

## API Versioning

Current API version: v7.0.0

Breaking changes will increment major version number. Backward compatibility maintained where possible through legacy field mappings.