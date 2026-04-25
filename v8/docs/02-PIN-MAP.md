# v8 Pin Map

## Production Pins
| Pin | Role | Status |
|---|---|---|
| 4 | Modem PWRKEY | Occupied |
| 5 | Modem RESET | Occupied |
| 21 | I2C SDA | Occupied |
| 22 | I2C SCL | Occupied |
| 23 | Modem POWER_ON | Occupied |
| 26 | Modem RX | Occupied |
| 27 | Modem TX | Occupied |
| 33 | DHT sensor | Occupied |

## Optional / Status Pins
| Pin | Role | Status |
|---|---|---|
| 12 | RGB red / test usage | Caution |
| 13 | Status LED | Caution |
| 14 | RGB green / test usage | Caution |
| 32 | RGB blue / test usage | Caution |

## Free or Cautionary Pins
| Pin | Status | Notes |
|---|---|---|
| 0 | Caution | Boot strapping risk. |
| 1 | Free/Caution | UART0 dependent. |
| 2 | Caution | Boot behavior concerns. |
| 3 | Free/Caution | UART0 dependent. |
| 15 | Caution | Boot strapping risk. |
| 34 | Free | Input-only. |
| 35 | Free | Input-only. |
| 36 | Free | Input-only. |
| 37 | Free | Input-only. |
| 38 | Free | Input-only. |
| 39 | Free | Input-only. |

## Test-Only Pins
- Power and sensor test modules from older versions should stay documented separately from production allocations.
