# Hardware wiring

Functional diagrams only. Confirm the exact ILA model and manufacturer manual before connecting sensor power or signal terminals. No connector pin numbers or cable colors are assumed.

## ESP32-S3

```mermaid
flowchart LR
    S[ILA sensor: verified RS485 terminals] <--> R[External RS485 transceiver]
    R -- RO to GPIO16 RX --> E[ESP32-S3]
    E -- GPIO18 TX to DI --> R
    E -- GPIO10 to DE/RE --> R
    E -- GPIO1 alert --> A[External relay driver]
    E -- GPIO2 alarm --> B[External relay driver]
    E -- GPIO46 --> L[NeoPixel]
    E -- GPIO5 CS; board default SPI --> D[Optional external SD]
```

These explicit pins preserve the imported firmware. The chosen ESP32-S3 build variant defaults to SPI SCK12, MISO13 and MOSI11. Verify against your board; do not use the old README's SCK18 assignment because GPIO18 is RS485 TX. External relay drivers need suitable supply and protection; GPIOs are logic signals.

## Cytron IRIV IO Controller

```mermaid
flowchart LR
    S[ILA sensor: verified RS485 terminals] <--> R[IRIV built-in isolated RS485]
    R <--> U[UART1: GP24 TX / GP25 RX]
    U <--> C[RP2350 gateway]
    C <--> W[W5500 Ethernet: built-in SPI]
    W <--> N[LAN / browser / PLC / MQTT]
    C -- DO0 GP12 alert --> A[External load or relay driver]
    C -- DO1 GP13 alarm --> B[External load or relay driver]
    C -- GP29 --> L[User status LED]
    T[USER button GP28: network fallback] --> C
    I[DI0-DI10 GP0-GP10: wired signals / push-buttons] --> C
    AN[AN0 GP26 / AN1 GP27: current or voltage inputs] --> C
    C -- DO2 GP14 --> BE[Optional alarm beacon / driver]
    C -- DO3 GP15 --> SO[Optional sounder / driver]
```

Pins follow the Arduino-Pico `cytron_iriv_io_controller` board variant and the existing 883M IRIV implementation. The RS485 driver handles direction automatically; no DE/RE output is driven. DO0/DO1 are isolated digital outputs, not mechanical relay contacts. Follow the [manufacturer's board resources](https://my.cytron.io/p-iriv-io-controller) for terminal wiring, load limits and power. GPIO5 is a digital input on this board; SD logging is disabled. Ethernet owns the onboard SPI bus.

The I/O pages use DI0?DI10 at GP0?GP10, AN0/AN1 at GP26/GP27, and the spare DO2/DO3 at GP14/GP15, following the board variant. New channels and outputs are disabled until configured. Use the physical AN-IN switch to select Current for 4?20 mA or Voltage for 0?10 V. The onboard USER button retains its five-second network fallback action; wired isolated inputs can be assigned capture or acknowledge actions. See the [I/O setup guide](io-alarms.md). No analog output is provided.
