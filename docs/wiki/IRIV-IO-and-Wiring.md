# IRIV I/O and wiring

The Cytron IRIV IO Controller gateway supports Wilcoxon 883M over its RS485 port and a separate 4–20 mA sensor such as the PCC420V on AN0 or AN1. Its I/O settings let an operator select PCC420V RMS or peak model presets, or enter a manual 4–20 mA scale. Enabled analog inputs appear on the main dashboard with the scaled value, raw current and loop fault state; disabled inputs do not have a dashboard card. DI0–DI10 actions and DO0–DO3 measurement rules are configurable.

See the [wiring guide](https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/blob/main/docs/wiring.md) and the [IRIV I/O guide](https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/blob/main/IRIV%20IOC/README.md#ioc-io) before connecting a sensor or load. The [cartoon wiring image](https://github.com/dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT/blob/main/docs/images/iriv-pcc420-wiring.svg) shows the 883M and PCC420V as separate sensors. Use the physical device manuals to verify terminal labels and power requirements.

PCC420V and IRIV analog/digital I/O behavior are implemented but still need physical bench validation. Demo readings are simulated.
