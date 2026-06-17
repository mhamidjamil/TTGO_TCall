// TTGO T-Call Dashboard JavaScript
class Dashboard {
    constructor() {
        this.config = {};
        this.authenticated = false;
        this.init();
    }

    init() {
        this.setupTabs();
        this.setupEventListeners();
        this.checkAuth();
        this.loadConfig();
        this.loadDeviceInfo();
    }

    setupTabs() {
        const tabButtons = document.querySelectorAll('.tab-btn');
        const tabContents = document.querySelectorAll('.tab-content');

        tabButtons.forEach(button => {
            button.addEventListener('click', () => {
                const targetTab = button.getAttribute('data-tab');
                
                // Remove active class from all buttons and contents
                tabButtons.forEach(btn => btn.classList.remove('active'));
                tabContents.forEach(content => content.classList.remove('active'));
                
                // Add active class to clicked button and corresponding content
                button.classList.add('active');
                document.getElementById(targetTab).classList.add('active');
            });
        });

        // Activate first tab by default
        if (tabButtons.length > 0) {
            tabButtons[0].click();
        }
    }

    setupEventListeners() {
        // Save Configuration
        document.getElementById('saveConfig').addEventListener('click', () => {
            this.saveConfig();
        });

        // Test buttons
        document.getElementById('testSMS').addEventListener('click', () => {
            this.testSMS();
        });

        document.getElementById('testCall').addEventListener('click', () => {
            this.testCall();
        });

        document.getElementById('testNTFY').addEventListener('click', () => {
            this.testNTFY();
        });

        document.getElementById('testGateESP').addEventListener('click', () => {
            this.testGateESP();
        });

        // Refresh device info
        document.getElementById('refreshInfo').addEventListener('click', () => {
            this.loadDeviceInfo();
        });

        // Clear logs
        document.getElementById('clearLogs').addEventListener('click', () => {
            this.clearLogs();
        });

        // Download logs
        document.getElementById('downloadLogs').addEventListener('click', () => {
            this.downloadLogs();
        });

        // Restart device
        document.getElementById('restartDevice').addEventListener('click', () => {
            this.restartDevice();
        });

        // Reset config
        document.getElementById('resetConfig').addEventListener('click', () => {
            this.resetConfig();
        });

        // Form validation
        this.setupFormValidation();
    }

    setupFormValidation() {
        // URL validation
        const urlInputs = document.querySelectorAll('input[type="url"]');
        urlInputs.forEach(input => {
            input.addEventListener('blur', () => {
                if (input.value && !this.isValidURL(input.value)) {
                    input.style.borderColor = 'var(--error-color)';
                } else {
                    input.style.borderColor = 'var(--border-color)';
                }
            });
        });

        // Phone number validation for admin numbers
        const adminNumbersInput = document.getElementById('adminNumbers');
        if (adminNumbersInput) {
            adminNumbersInput.addEventListener('blur', () => {
                const numbers = adminNumbersInput.value.split(',').map(n => n.trim());
                let valid = true;
                numbers.forEach(number => {
                    if (number && !this.isValidPhoneNumber(number)) {
                        valid = false;
                    }
                });
                adminNumbersInput.style.borderColor = valid ? 'var(--border-color)' : 'var(--error-color)';
            });
        }
    }

    isValidURL(string) {
        try {
            new URL(string);
            return true;
        } catch (_) {
            return false;
        }
    }

    isValidPhoneNumber(number) {
        // Basic phone number validation (international format)
        const phoneRegex = /^\+?[1-9]\d{1,14}$/;
        return phoneRegex.test(number.replace(/[\s\-\(\)]/g, ''));
    }

    async checkAuth() {
        try {
            const response = await fetch('/api/auth');
            const data = await response.json();
            this.authenticated = data.authenticated || false;
            
            if (!this.authenticated) {
                document.getElementById('authMessage').style.display = 'block';
            }
        } catch (error) {
            console.error('Auth check failed:', error);
            this.authenticated = false;
        }
    }

    async loadConfig() {
        try {
            this.showLoading('Loading configuration...');
            const response = await fetch('/api/config');
            
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            
            const data = await response.json();
            this.config = data;
            this.populateForm(data);
            this.showResult('Configuration loaded successfully', 'success');
        } catch (error) {
            console.error('Failed to load config:', error);
            this.showResult('Failed to load configuration: ' + error.message, 'error');
        } finally {
            this.hideLoading();
        }
    }

    populateForm(config) {
        // General Settings
        this.setInputValue('deviceId', config.deviceId);
        this.setInputValue('ownerName', config.ownerName);
        this.setInputValue('ownerPhone', config.ownerPhone);
        this.setInputValue('logMessages', config.logMessages);

        // API & Forwarding
        this.setInputValue('serverUrl', config.serverUrl);
        this.setInputValue('apiKey', config.apiKey);
        this.setInputValue('gateEspUrl', config.gateEspUrl);
        this.setInputValue('forwardSmsUrl', config.forwardSmsUrl);

        // Notifications
        this.setInputValue('ntfyEnabled', config.ntfyEnabled);
        this.setInputValue('ntfyUrl', config.ntfyUrl);
        this.setInputValue('ntfyTopic', config.ntfyTopic);
        this.setInputValue('autoReplyMessage', config.autoReplyMessage);

        // Security & Permissions
        this.setInputValue('smsAllowed', config.smsAllowed);
        this.setInputValue('callsAllowed', config.callsAllowed);
        this.setInputValue('adminNumbers', config.adminNumbers);
        this.setInputValue('allowedNumbers', config.allowedNumbers);
    }

    setInputValue(id, value) {
        const element = document.getElementById(id);
        if (!element) return;

        if (element.type === 'checkbox') {
            element.checked = value === true || value === 'true' || value === '1';
        } else {
            element.value = value || '';
        }
    }

    getFormData() {
        const formData = {};
        
        // General Settings
        formData.deviceId = document.getElementById('deviceId').value.trim();
        formData.ownerName = document.getElementById('ownerName').value.trim();
        formData.ownerPhone = document.getElementById('ownerPhone').value.trim();
        formData.logMessages = document.getElementById('logMessages').checked;

        // API & Forwarding
        formData.serverUrl = document.getElementById('serverUrl').value.trim();
        formData.apiKey = document.getElementById('apiKey').value.trim();
        formData.gateEspUrl = document.getElementById('gateEspUrl').value.trim();
        formData.forwardSmsUrl = document.getElementById('forwardSmsUrl').value.trim();

        // Notifications
        formData.ntfyEnabled = document.getElementById('ntfyEnabled').checked;
        formData.ntfyUrl = document.getElementById('ntfyUrl').value.trim();
        formData.ntfyTopic = document.getElementById('ntfyTopic').value.trim();
        formData.autoReplyMessage = document.getElementById('autoReplyMessage').value.trim();

        // Security & Permissions
        formData.smsAllowed = document.getElementById('smsAllowed').checked;
        formData.callsAllowed = document.getElementById('callsAllowed').checked;
        formData.adminNumbers = document.getElementById('adminNumbers').value.trim();
        formData.allowedNumbers = document.getElementById('allowedNumbers').value.trim();

        return formData;
    }

    validateFormData(data) {
        const errors = [];

        // Required fields
        if (!data.deviceId) errors.push('Device ID is required');
        if (!data.ownerName) errors.push('Owner name is required');

        // URL validations
        if (data.serverUrl && !this.isValidURL(data.serverUrl)) {
            errors.push('Server URL is not valid');
        }
        if (data.gateEspUrl && !this.isValidURL(data.gateEspUrl)) {
            errors.push('Gate ESP URL is not valid');
        }
        if (data.forwardSmsUrl && !this.isValidURL(data.forwardSmsUrl)) {
            errors.push('Forward SMS URL is not valid');
        }
        if (data.ntfyUrl && !this.isValidURL(data.ntfyUrl)) {
            errors.push('NTFY URL is not valid');
        }

        // Phone number validations
        if (data.adminNumbers) {
            const numbers = data.adminNumbers.split(',').map(n => n.trim());
            numbers.forEach(number => {
                if (number && !this.isValidPhoneNumber(number)) {
                    errors.push(`Invalid admin number: ${number}`);
                }
            });
        }

        return errors;
    }

    async saveConfig() {
        try {
            const formData = this.getFormData();
            const errors = this.validateFormData(formData);

            if (errors.length > 0) {
                this.showResult('Validation errors:\n• ' + errors.join('\n• '), 'error');
                return;
            }

            this.showLoading('Saving configuration...');
            
            const response = await fetch('/api/config', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify(formData)
            });

            if (!response.ok) {
                const errorText = await response.text();
                throw new Error(`Server error: ${response.status} - ${errorText}`);
            }

            const result = await response.json();
            
            if (result.success) {
                this.config = formData;
                this.showResult('Configuration saved successfully! Changes will take effect on next restart.', 'success');
            } else {
                throw new Error(result.error || 'Unknown error occurred');
            }
        } catch (error) {
            console.error('Save failed:', error);
            this.showResult('Failed to save configuration: ' + error.message, 'error');
        } finally {
            this.hideLoading();
        }
    }

    async loadDeviceInfo() {
        try {
            this.showLoading('Loading device information...');
            const response = await fetch('/api/status');
            
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            
            const data = await response.json();
            this.updateDeviceInfo(data);
        } catch (error) {
            console.error('Failed to load device info:', error);
            document.getElementById('deviceInfo').innerHTML = 
                '<p style="color: var(--error-color);">Failed to load device information</p>';
        } finally {
            this.hideLoading();
        }
    }

    updateDeviceInfo(info) {
        const deviceInfoElement = document.getElementById('deviceInfo');
        deviceInfoElement.innerHTML = `
            <p><strong>Firmware Version:</strong> ${info.version || 'Unknown'}</p>
            <p><strong>Uptime:</strong> ${info.uptime || 'Unknown'}</p>
            <p><strong>Free Memory:</strong> ${info.freeMemory || 'Unknown'}</p>
            <p><strong>Network Status:</strong> ${info.networkStatus || 'Unknown'}</p>
            <p><strong>Signal Strength:</strong> ${info.signalStrength || 'Unknown'}</p>
            <p><strong>Last Restart:</strong> ${info.lastRestart || 'Unknown'}</p>
            <p><strong>Config Source:</strong> ${info.configSource || 'Unknown'}</p>
        `;
    }

    async testSMS() {
        const testNumber = prompt('Enter phone number to send test SMS:');
        if (!testNumber) return;

        try {
            this.showLoading('Sending test SMS...');
            const response = await fetch('/api/test/sms', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ number: testNumber })
            });

            const result = await response.json();
            this.updateTestOutput('smsTestOutput', 
                response.ok ? 'SMS sent successfully' : 'SMS failed: ' + result.error);
        } catch (error) {
            this.updateTestOutput('smsTestOutput', 'Error: ' + error.message);
        } finally {
            this.hideLoading();
        }
    }

    async testCall() {
        const testNumber = prompt('Enter phone number to test call:');
        if (!testNumber) return;

        try {
            this.showLoading('Testing call...');
            const response = await fetch('/api/test/call', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ number: testNumber })
            });

            const result = await response.json();
            this.updateTestOutput('callTestOutput', 
                response.ok ? 'Call test completed' : 'Call test failed: ' + result.error);
        } catch (error) {
            this.updateTestOutput('callTestOutput', 'Error: ' + error.message);
        } finally {
            this.hideLoading();
        }
    }

    async testNTFY() {
        try {
            this.showLoading('Testing NTFY notification...');
            const response = await fetch('/api/test/ntfy', {
                method: 'POST'
            });

            const result = await response.json();
            this.updateTestOutput('ntfyTestOutput', 
                response.ok ? 'NTFY notification sent successfully' : 'NTFY test failed: ' + result.error);
        } catch (error) {
            this.updateTestOutput('ntfyTestOutput', 'Error: ' + error.message);
        } finally {
            this.hideLoading();
        }
    }

    async testGateESP() {
        try {
            this.showLoading('Testing Gate ESP communication...');
            const response = await fetch('/api/test/gate-esp', {
                method: 'POST'
            });

            const result = await response.json();
            this.updateTestOutput('gateTestOutput', 
                response.ok ? 'Gate ESP communication successful' : 'Gate ESP test failed: ' + result.error);
        } catch (error) {
            this.updateTestOutput('gateTestOutput', 'Error: ' + error.message);
        } finally {
            this.hideLoading();
        }
    }

    updateTestOutput(outputId, message) {
        const output = document.getElementById(outputId);
        if (output) {
            const timestamp = new Date().toLocaleTimeString();
            output.textContent = `[${timestamp}] ${message}`;
        }
    }

    async clearLogs() {
        if (!confirm('Are you sure you want to clear all logs?')) return;

        try {
            this.showLoading('Clearing logs...');
            const response = await fetch('/api/logs/clear', {
                method: 'POST'
            });

            const result = await response.json();
            if (response.ok) {
                this.showResult('Logs cleared successfully', 'success');
            } else {
                throw new Error(result.error || 'Failed to clear logs');
            }
        } catch (error) {
            this.showResult('Error clearing logs: ' + error.message, 'error');
        } finally {
            this.hideLoading();
        }
    }

    async downloadLogs() {
        try {
            this.showLoading('Preparing logs for download...');
            const response = await fetch('/api/logs/download');
            
            if (!response.ok) {
                throw new Error('Failed to download logs');
            }

            const blob = await response.blob();
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `ttgo-logs-${new Date().toISOString().split('T')[0]}.txt`;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            window.URL.revokeObjectURL(url);
            
            this.showResult('Logs downloaded successfully', 'success');
        } catch (error) {
            this.showResult('Error downloading logs: ' + error.message, 'error');
        } finally {
            this.hideLoading();
        }
    }

    async restartDevice() {
        if (!confirm('Are you sure you want to restart the device? This will interrupt all connections.')) return;

        try {
            this.showLoading('Restarting device...');
            const response = await fetch('/api/restart', {
                method: 'POST'
            });

            if (response.ok) {
                this.showResult('Device restart initiated. Please wait 30 seconds before refreshing...', 'success');
                setTimeout(() => {
                    window.location.reload();
                }, 30000);
            } else {
                throw new Error('Failed to restart device');
            }
        } catch (error) {
            this.showResult('Error restarting device: ' + error.message, 'error');
        } finally {
            this.hideLoading();
        }
    }

    async resetConfig() {
        if (!confirm('Are you sure you want to reset all configuration to defaults? This cannot be undone.')) return;

        try {
            this.showLoading('Resetting configuration...');
            const response = await fetch('/api/config/reset', {
                method: 'POST'
            });

            const result = await response.json();
            if (response.ok) {
                this.showResult('Configuration reset successfully. Reloading...', 'success');
                setTimeout(() => {
                    this.loadConfig();
                }, 2000);
            } else {
                throw new Error(result.error || 'Failed to reset configuration');
            }
        } catch (error) {
            this.showResult('Error resetting configuration: ' + error.message, 'error');
        } finally {
            this.hideLoading();
        }
    }

    showLoading(message) {
        const buttons = document.querySelectorAll('button');
        buttons.forEach(btn => {
            btn.classList.add('loading');
            btn.disabled = true;
        });
        
        this.showResult(message, 'info');
    }

    hideLoading() {
        const buttons = document.querySelectorAll('button');
        buttons.forEach(btn => {
            btn.classList.remove('loading');
            btn.disabled = false;
        });
    }

    showResult(message, type) {
        const resultElement = document.getElementById('saveResult');
        if (resultElement) {
            resultElement.textContent = message;
            resultElement.className = type;
            resultElement.style.display = 'block';
            
            // Auto-hide success/info messages after 5 seconds
            if (type === 'success' || type === 'info') {
                setTimeout(() => {
                    resultElement.style.display = 'none';
                }, 5000);
            }
        }
    }
}

// Initialize dashboard when DOM is loaded
document.addEventListener('DOMContentLoaded', () => {
    new Dashboard();
});

// Utility functions for global access
function formatBytes(bytes, decimals = 2) {
    if (bytes === 0) return '0 Bytes';
    const k = 1024;
    const dm = decimals < 0 ? 0 : decimals;
    const sizes = ['Bytes', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
}

function formatUptime(milliseconds) {
    const seconds = Math.floor(milliseconds / 1000);
    const minutes = Math.floor(seconds / 60);
    const hours = Math.floor(minutes / 60);
    const days = Math.floor(hours / 24);
    
    if (days > 0) return `${days}d ${hours % 24}h ${minutes % 60}m`;
    if (hours > 0) return `${hours}h ${minutes % 60}m`;
    if (minutes > 0) return `${minutes}m ${seconds % 60}s`;
    return `${seconds}s`;
}
