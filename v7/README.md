# TTGO T-Call ESP32 Project Documentation

## Project Overview

This project provides a comprehensive SMS and Call management system for the TTGO T-Call ESP32 board with cellular modem capabilities. The system features persistent SPIFFS-based configuration, web dashboard management, and integration with external services.

## Version 7 Architecture

Version 7 represents a complete overhaul of the configuration management system, introducing:
- **SPIFFS-based persistent storage** for all configuration parameters
- **Comprehensive web dashboard** with tabbed interface
- **JSON-based configuration** serialization and API
- **Hierarchical configuration** system with compile-time defaults

## Key Features

### 🔧 Configuration Management
- **Persistent Storage**: All settings saved to SPIFFS file system
- **Web-based Configuration**: No code changes needed for settings updates
- **Hierarchical Defaults**: Compile-time → SPIFFS → Runtime configuration layers
- **JSON API**: RESTful configuration management via `/api/config`

### 📱 Communication Features
- **SMS Management**: Send/receive SMS with filtering and forwarding
- **Call Handling**: Incoming call detection with admin number verification
- **Auto-Reply**: Customizable responses for unknown callers
- **Message Logging**: Comprehensive message and call logging

### 🌐 Integration Capabilities
- **NTFY Notifications**: Real-time notifications via NTFY service
- **Gate ESP Integration**: Communication with gate control ESP devices
- **External API Forwarding**: Forward messages to external services
- **Web Dashboard**: Complete device management interface

### 🔒 Security Features
- **Admin Number Management**: Multiple admin numbers with permissions
- **API Key Authentication**: Secure API access with configurable keys
- **Dashboard Password**: Protected web interface access
- **Number Filtering**: Allow/block specific phone numbers

## Project Structure

```
v7/
├── sms_calls/
│   ├── sms_calls.ino          # Main Arduino sketch
│   ├── ConfigManager.h        # Configuration structure definitions
│   ├── ConfigManager.cpp      # SPIFFS-based configuration management
│   ├── secrets.h              # Compile-time configuration defaults
│   ├── SMSManager.h/.cpp      # SMS handling implementation
│   ├── CallManager.h/.cpp     # Call management implementation
│   ├── WebDashboard.h/.cpp    # Web server and dashboard API
│   ├── DisplayManager.h/.cpp  # OLED display management
│   ├── DHTManager.h/.cpp      # Temperature/humidity sensor
│   ├── SPIFFSUtils.h/.cpp     # SPIFFS file system utilities
│   └── data/                  # Web dashboard files (upload to SPIFFS)
│       ├── dashboard.html     # Comprehensive tabbed dashboard UI
│       ├── dashboard.css      # Modern styling for dashboard
│       ├── dashboard.js       # JavaScript functionality
│       └── version.txt        # Configuration version tracking
```

## Configuration System

### Configuration Parameters

The system manages 16+ configuration parameters organized into categories:

#### General Settings
- `deviceId` - Unique device identifier
- `ownerName` - Device owner name
- `ownerPhone` - Primary contact number
- `logMessages` - Enable/disable message logging

#### API & Forwarding
- `serverUrl` - External API server URL
- `apiKey` - API authentication key
- `gateEspUrl` - Gate ESP device URL
- `forwardSmsUrl` - SMS forwarding endpoint

#### Notifications
- `ntfyEnabled` - Enable NTFY notifications
- `ntfyUrl` - NTFY server URL
- `ntfyTopic` - NTFY notification topic
- `autoReplyMessage` - Auto-reply message template

#### Security & Permissions
- `smsAllowed` - Enable SMS functionality
- `callsAllowed` - Enable call management
- `adminNumbers` - Comma-separated admin numbers
- `allowedNumbers` - Comma-separated allowed numbers

### Configuration Storage Hierarchy

1. **Compile-time Defaults** (`secrets.h`)
   - Hardcoded fallback values
   - Version control friendly
   - Development environment specific

2. **SPIFFS Persistent Storage** (`/config.json`)
   - Runtime configuration changes
   - Survives device reboots
   - Web dashboard updates

3. **Runtime Configuration** (Memory)
   - Active configuration object
   - Loaded from SPIFFS at startup
   - Updated via dashboard/API

### Configuration Methods

```cpp
// Loading configuration
Config config = configManager.get();

// Saving configuration
bool success = configManager.save(config);

// Reset to defaults
bool success = configManager.resetToDefaults();

// Print current configuration
configManager.printConfig();
```

## Web Dashboard

### Dashboard Features

The comprehensive web dashboard provides:

1. **General Settings Tab**
   - Device identification
   - Owner information
   - Basic functionality toggles

2. **API & Forwarding Tab**
   - Server URLs and API keys
   - Integration endpoints
   - Forwarding configuration

3. **Notifications Tab**
   - NTFY service configuration
   - Auto-reply message settings
   - Notification preferences

4. **Security Tab**
   - Admin number management
   - Permission settings
   - Access control

5. **Device Info Tab**
   - System information
   - Memory usage
   - Network status

6. **Testing Tab**
   - SMS/Call testing
   - Service connectivity tests
   - Diagnostic tools

7. **About Tab**
   - Project information
   - Version details
   - Documentation links

### Dashboard Access

- **URL**: `http://[device-ip]/dashboard`
- **Authentication**: Password-based (set in `secrets.h`)
- **Mobile Responsive**: Works on phones and tablets

## API Reference

### Configuration API

#### GET /api/config
Returns current configuration as JSON.

**Response:**
```json
{
  "deviceId": "ttgo-001",
  "ownerName": "John Doe",
  "ownerPhone": "+1234567890",
  "logMessages": true,
  "serverUrl": "https://api.example.com",
  "apiKey": "*****",
  "gateEspUrl": "http://192.168.1.100",
  "forwardSmsUrl": "https://webhook.example.com",
  "ntfyEnabled": true,
  "ntfyUrl": "https://ntfy.sh",
  "ntfyTopic": "ttgo-alerts",
  "autoReplyMessage": "This is an automated system...",
  "smsAllowed": true,
  "callsAllowed": true,
  "adminNumbers": "+1234567890,+0987654321",
  "allowedNumbers": "*"
}
```

#### POST /api/config
Save configuration changes.

**Request Body:** Configuration JSON (same format as GET response)

**Authentication:** Dashboard password via `X-Dashboard-Auth` header

### Device Status API

#### GET /api/status
Returns device status information.

**Response:**
```json
{
  "version": "v7.0.0",
  "uptime": "3600 seconds",
  "freeMemory": "125432 bytes",
  "networkStatus": "Connected",
  "signalStrength": "-65 dBm",
  "lastRestart": "Power cycle",
  "configSource": "SPIFFS"
}
```

### Testing API

#### POST /api/test/sms
Test SMS functionality.

**Request Body:**
```json
{
  "number": "+1234567890"
}
```

#### POST /api/test/ntfy
Test NTFY notification service.

#### POST /api/test/gate-esp
Test Gate ESP communication.

### System Control API

#### POST /api/restart
Restart the device.

#### POST /api/config/reset
Reset configuration to defaults.

#### POST /api/logs/clear
Clear system logs.

#### GET /api/logs/download
Download system logs as text file.

## Setup Instructions

### 1. Hardware Setup
- Connect TTGO T-Call ESP32 board
- Insert SIM card with data plan
- Connect optional sensors (DHT22, etc.)
- Connect OLED display (optional)

### 2. Software Configuration

#### Update secrets.h
Set your default configuration values:
```cpp
#define DEVICE_ID_DEFAULT "ttgo-001"
#define OWNER_NAME_DEFAULT "Your Name"
#define OWNER_PHONE_DEFAULT "+1234567890"
#define DASHBOARD_PASSWORD "your_password"
#define NTFY_SERVER_URL_DEFAULT "https://ntfy.sh"
#define ADMIN_NUMBERS_DEFAULT "+1234567890"
```

#### Upload Sketch
1. Open `sms_calls.ino` in Arduino IDE
2. Select correct board and port
3. Upload sketch to ESP32

#### Upload Dashboard Files
1. Install ESP32 SPIFFS upload plugin
2. Select "Tools > ESP32 Sketch Data Upload"
3. Upload files from `data/` directory to SPIFFS

### 3. Initial Configuration

1. Connect to device WiFi hotspot or find IP on network
2. Navigate to `http://[device-ip]/dashboard`
3. Login with dashboard password
4. Configure all settings via web interface
5. Test functionality using testing tab

## Usage Examples

### Basic SMS Operations

```cpp
// Send SMS
String error;
bool success = smsManager.sendSms("+1234567890", "Hello World", error);

// Check for incoming SMS
smsManager.checkForSms();
```

### Configuration Management

```cpp
// Load current configuration
Config config = configManager.get();

// Modify settings
config.ntfyEnabled = true;
config.ntfyUrl = "https://ntfy.sh";

// Save changes
configManager.save(config);
```

### Web Dashboard Updates

Settings changed via the web dashboard are automatically:
1. Validated on the client side
2. Sent to `/api/config` endpoint
3. Saved to SPIFFS storage
4. Applied to running configuration

## Troubleshooting

### Common Issues

**Configuration not persisting:**
- Ensure SPIFFS is properly formatted
- Check available flash memory
- Verify write permissions

**Dashboard not accessible:**
- Check network connectivity
- Verify correct IP address
- Ensure dashboard password is correct

**SMS not working:**
- Verify SIM card activation
- Check cellular signal strength
- Confirm APN settings

**API authentication fails:**
- Check API key configuration
- Verify header format
- Ensure authentication is enabled

### Debug Information

Enable debug output in `secrets.h`:
```cpp
#define DEBUG_ENABLED true
```

Monitor serial output for detailed logging of:
- Configuration loading/saving
- SMS/Call operations
- API requests and responses
- Error conditions

### Reset Procedures

**Soft Reset (Configuration):**
- Use dashboard "Reset Config" button
- Or call `/api/config/reset` endpoint

**Hard Reset (Complete):**
- Use dashboard "Restart Device" button
- Or physically power cycle device

**Factory Reset (SPIFFS):**
- Re-upload sketch with SPIFFS format enabled
- Or manually delete `/config.json` file

## Development Notes

### Adding New Configuration Parameters

1. Update `Config` struct in `ConfigManager.h`
2. Add default values in `secrets.h`
3. Update `configToJson()` and `jsonToConfig()` methods
4. Add form fields to `dashboard.html`
5. Update JavaScript handling in `dashboard.js`
6. Update API handlers in `WebDashboard.cpp`

### Testing Configuration

Use the built-in testing tools:
- Dashboard testing tab
- API endpoints for validation
- Serial monitor debugging
- Configuration export/import

### Performance Considerations

- Configuration loaded once at startup
- Changes saved immediately to SPIFFS
- JSON parsing optimized for memory usage
- Web dashboard uses efficient caching

## Contributing

When contributing to this project:

1. Maintain backward compatibility
2. Update documentation for new features
3. Test configuration persistence
4. Validate web dashboard functionality
5. Follow existing code patterns

## License

This project is open source. Please refer to the LICENSE file for details.

## Version History

- **v7.0.0**: Complete SPIFFS configuration system overhaul
- **v6.x**: Basic configuration with NVS storage
- **v5.x**: Initial SMS/Call functionality
- **Earlier**: Prototype versions

## Support

For support and questions:
1. Check this documentation
2. Review troubleshooting section
3. Enable debug logging
4. Check project issues/discussions