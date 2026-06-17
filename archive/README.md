# Archive

Older versions and supporting material, kept for reference. **Active development happens in [`/v8`](../v8).**

| Folder / file | What it is | Why it's kept |
|---|---|---|
| `v5/` | The original hand-written firmware. | **Behavioral source of truth** for legacy SMS handling, phone-number normalization, and allow/block identity rules. When v8 behavior is ambiguous, v5 is the baseline. |
| `v6/` | First modular split into managers. | Intermediate step; superseded by v7/v8. |
| `v7/` | Modular reference implementation. | Structural template for v8 (managers, config, dashboard/API layout). |
| `Modules_test/` | Standalone hardware test sketches (I2C OLED, MPU6050, ultrasonic, RTOS, power). | Bench tests; not production. |
| `server/` | Standalone Flask SMS bridge/logger. | Earlier server-side proxy; v8 talks to Firebase directly. |
| `TTgo_T-Call_ESP32_Module_Project.ino` | Original single-file sketch. | Historical reference. |
| `commands.txt` | Useful SIM800 AT commands + font headers list. | Handy AT-command cheatsheet. |
| `TTGO-T-call-Pinout-Diagram-Large.jpg` | Board pinout diagram. | Hardware reference (see also `v8/docs/02-PIN-MAP.md`). |

Nothing here is built or flashed as part of the current project.
