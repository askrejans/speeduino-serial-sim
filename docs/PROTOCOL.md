# Speeduino Protocol Specification

## Overview

This document describes the Speeduino serial protocol implementation in the simulator. The protocol is binary-based and command-response oriented.

## Serial Configuration

- **Baud Rate**: 115200 bps
- **Data Bits**: 8
- **Parity**: None
- **Stop Bits**: 1
- **Flow Control**: None

## Command Format

Commands are single ASCII characters sent to the ECU:

```
[Command Byte]
```

## Implemented Commands

### Command 'A' - Real-time Data

Returns complete engine status (79 bytes).

**Request**: `0x41` ('A')

**Response**: 79-byte binary structure

```c
struct EngineStatus {
    uint8_t response;        // 0: 'A' echo
    uint8_t secl;           // 1: Seconds counter
    uint8_t status1;        // 2: Status flags
    uint8_t engine;         // 3: Engine flags
    uint8_t dwell;          // 4: Dwell time (0.1ms)
    uint8_t maplo;          // 5: MAP low byte
    uint8_t maphi;          // 6: MAP high byte (kPa)
    uint8_t iat;            // 7: IAT (°C + 40)
    uint8_t clt;            // 8: CLT (°C + 40)
    uint8_t batcorrection;  // 9: Battery correction %
    uint8_t batteryv;       // 10: Battery voltage (0.1V)
    uint8_t o2;             // 11: O2 sensor
    uint8_t egocorrection;  // 12: EGO correction %
    uint8_t iatcorrection;  // 13: IAT correction %
    uint8_t wue;            // 14: Warmup enrichment %
    uint8_t rpmlo;          // 15: RPM low byte
    uint8_t rpmhi;          // 16: RPM high byte
    uint8_t taeamount;      // 17: Accel enrichment %
    uint8_t gammae;         // 18: Total correction %
    uint8_t ve;             // 19: VE %
    uint8_t afrtarget;      // 20: Target AFR (0.1)
    uint8_t pw1lo;          // 21: PW low byte
    uint8_t pw1hi;          // 22: PW high byte (0.1ms)
    uint8_t tpsdot;         // 23: TPS rate (%/s)
    uint8_t advance;        // 24: Ignition advance (°)
    uint8_t tps;            // 25: TPS %
    uint8_t loopslo;        // 26-27: Loop counter
    uint8_t loopshi;
    uint8_t freeramlo;      // 28-29: Free RAM
    uint8_t freeramhi;
    uint8_t boosttarget;    // 30: Boost target kPa
    uint8_t boostduty;      // 31: Boost duty %
    uint8_t spark;          // 32: Spark flags
    uint8_t rpmdotlo;       // 33-34: RPM acceleration
    uint8_t rpmdothi;
    uint8_t ethanolpct;     // 35: Ethanol %
    uint8_t flexcorrection; // 36: Flex fuel correction
    uint8_t flexigncorrection; // 37: Flex ign correction
    uint8_t idleload;       // 38: Idle load
    uint8_t testoutputs;    // 39: Test outputs
    uint8_t o2_2;           // 40: Secondary O2
    uint8_t baro;           // 41: Barometric pressure kPa
    uint8_t canin[32];      // 42-73: CAN data
    uint8_t tpsadc;         // 74: TPS ADC raw
    uint8_t errors;         // 75: Error code
    uint8_t unused[3];      // 76-78: Reserved
};
```

**Example**:
```python
import serial
ser = serial.Serial('/dev/ttyUSB0', 115200)
ser.write(b'A')
data = ser.read(79)
rpm = (data[16] << 8) | data[15]
print(f"RPM: {rpm}")
```

---

### Command 'Q' - Status Request

Returns ECU status and capabilities.

**Request**: `0x51` ('Q')

**Response**: 4 bytes

```
Byte 0: 0x00 (Signature: speeduino)
Byte 1: 0x01 (Status flags)
Byte 2: 0x01 (Number of pages)
Byte 3: 0x00 (Reserved)
```

---

### Command 'V' - Version Request

Returns firmware version string.

**Request**: `0x56` ('V') or `0x76` ('v')

**Response**: Variable-length ASCII string ending with newline

**Format**: `"speeduino YYYYMM-sim X.Y.Z\n"`

**Example**: `"speeduino 202310-sim 2.0.0\n"`

---

### Command 'S' - Signature Request

Returns ECU identification signature.

**Request**: `0x53` ('S')

**Response**: 20 bytes (null-padded string)

**Value**: `"speeduino 202310\0\0\0\0"`

Used by TunerStudio to identify ECU type.

---

### Command 'n' - Page Sizes

Returns configuration page count and sizes.

**Request**: `0x6E` ('n')

**Response**: Variable length

```
Byte 0: Number of pages (2)
Bytes 1-2: Page 0 size (little-endian)
Bytes 3-4: Page 1 size (little-endian)
...
```

**Example**:
```
0x02 0x20 0x00 0x00 0x01  = 2 pages: 32 bytes, 256 bytes
```

---

## Data Encoding

### Multi-byte Values

All multi-byte integers use **little-endian** format:

```c
uint16_t rpm = (rpmhi << 8) | rpmlo;
uint16_t map = (maphi << 8) | maplo;
```

### Temperature Encoding

Temperatures offset by 40 to allow negative values:

```c
int8_t celsius = encoded_value - 40;
uint8_t encoded = celsius + 40;
```

**Range**: -40°C to +215°C

### Scaled Values

- **Battery Voltage**: Value × 0.1 = Volts (e.g., 140 = 14.0V)
- **AFR**: Value × 0.1 = AFR (e.g., 147 = 14.7:1)
- **Pulse Width**: Value × 0.1 = ms (e.g., 35 = 3.5ms)
- **Dwell**: Value × 0.1 = ms

### Percentage Values

Direct 0-255 range:
- 100 = 100%
- 150 = 150% (enrichment)
- 50 = 50% (lean)

---

## Error Handling

### Unknown Commands

Simulator responds with single byte `0xFF` for unknown commands.

### Timeouts

No explicit timeout mechanism. Client should implement timeout logic.

---

## TunerStudio Integration

The simulator is compatible with TunerStudio when:
1. Signature matches expected pattern
2. Page sizes are reported correctly
3. Real-time data format matches

**INI File Configuration**:
```ini
[Constants]
   signature = "speeduino 202310"

[OutputChannels]
   rpm = { rpmhi * 256 + rpmlo }
   coolant = { clt - 40 }
   ; ... etc
```

---

## Future Commands (Not Implemented)

These commands exist in full Speeduino but not in simulator:

- **'B'**: Burn to flash
- **'C'**: Test outputs
- **'E'**: Error codes
- **'F'**: Get page
- **'L'**: List CAN devices
- **'P'**: Get page (legacy)
- **'R'**: Read EEPROM
- **'T'**: Tooth log
- **'W'**: Write EEPROM
- **'Z'**: Composite logger

---

## Protocol Version History

- **v0.4**: Current (Speeduino 202310+)
- **v0.3**: Legacy format (79-byte page)
- **v0.2**: Older Speeduino versions

---

## References

- [Speeduino Wiki](https://wiki.speeduino.com/)
- [TunerStudio MS Documentation](https://www.tunerstudio.com/index.php/manuals)
- [Original Speeduino Source](https://github.com/noisymime/speeduino)
