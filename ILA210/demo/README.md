# Local IOC demo

From the parent workspace:

```powershell
node ILA210/demo/mock_server.js
```

Open **http://localhost:8091/ioc**. Set `PORT` to use another port. The server binds only to the local computer.

The demo renders the actual embedded IOC pages and adds a clearly labeled simulator panel. Try high pressure, a broken current loop, external/vibration alarms, capture and acknowledge push-buttons, and offline/reconnect. Separate pages let you edit digital input actions, analog scaling/alarms and DO2/DO3 functions. DO0/DO1 represent existing sensor outputs.

Demo presets enable DI0 capture, DI1 acknowledge, DI2 external alarm, AN0 pressure, AN1 temperature, DO2 latched beacon and DO3 sounder. Firmware defaults remain disabled. Changes affect this demo's memory only. Reset demo presets restores the starting configuration. All tabs share one simulated device; config revisions protect against stale saves.

Simulation is for viewing and exercising the interface, not validating firmware timing, GPIO wiring, sensor registers, ADC accuracy, MQTT or physical alarm operation.

Sensor communication settings are previewed at `/sensor_fault_settings`. Use Disconnect sensor / Reconnect sensor to test the configured timeout and combined alarm independently of the simulated HTTP offline state. DO0/DO1 in this demo represent enabled vibration outputs. All settings remain in memory.
