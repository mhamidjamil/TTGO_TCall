# v8 Boot Sequence

1. Power and modem initialization.
2. IP5306 keep-on setup.
3. Serial modem start.
4. SIM module restart.
5. Attempt STA WiFi connection.
6. If STA fails, start AP fallback.
7. Start web dashboard and API.
8. Initialize DHT and display managers.
9. Print startup summary logs including docs URL.

## Acceptance Criteria
- Boot order is deterministic.
- AP fallback is automatic.
- Startup logs are concise and professional.
