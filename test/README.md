# Testing Guide

## Overview

The Speeduino Serial Simulator includes comprehensive embedded unit tests using the Unity test framework. All tests run directly on hardware (ESP32, ESP8266, Arduino).

## Test File

- `test_embedded.cpp` - Comprehensive tests for EngineSimulator, SpeeduinoProtocol, WiFiSerialAdapter, and WebInterface (40+ test cases)

## Test Coverage

### Engine Simulator Tests (15 tests)
- Initialization and basic operation
- RPM bounds checking
- Temperature progression
- MAP/throttle correlation
- Volumetric efficiency
- Pulse width calculation
- Timing advance changes
- Battery voltage stability
- AFR target changes with load
- TPS changes with engine mode
- Mode transitions
- Sensor range validation
- Runtime tracking

### Speeduino Protocol Tests (12 tests)
- Command 'A' (realtime data)
- Command 'V' (version)
- Command 'Q' (status)
- Command 'S' (signature)
- Command 'n' (page sizes)
- Unknown command handling
- Command counter
- Multiple command sequences
- Response format validation
- Error handling

### WiFi Serial Adapter Tests (7 tests) - ESP32/ESP8266 only
- TCP socket adapter creation
- Begin/initialization without crash
- Ready status without client
- Available bytes check
- Read operations without data
- Write operations without client
- Protocol integration with WiFi serial

### Web Interface Tests (4 tests) - ESP32/ESP8266 only
- Web interface object creation
- Server initialization (begin)
- IP address retrieval
- AP mode detection

### Integration Tests (3 tests)
- Full simulation cycle with commands
- Rapid command processing
- All modes stability testing

## Running Tests

### Requirements
- Physical hardware connected via USB
- PlatformIO installed
- Correct serial port permissions

### Run Tests on ESP32-S3 (Full Test Suite)

```bash
# Upload and run all tests (includes WiFi/network tests)
platformio test -e esp32s3

# Verbose output
platformio test -e esp32s3 -vvv

# Specify serial port
platformio test -e esp32s3 --upload-port /dev/cu.usbmodem11101 --test-port /dev/cu.usbmodem11101
```

**Note:** ESP32-S3 environment runs the complete test suite including WiFi Serial Adapter and Web Interface tests (41 tests total).

### Run Tests on ESP32-S2

```bash
# Upload and run tests (includes web interface tests, no WiFi serial)
platformio test -e esp32s2

# Verbose output
platformio test -e esp32s2 -vvv

# Specify serial port
platformio test -e esp32s2 --upload-port /dev/cu.usbserial-11410 --test-port /dev/cu.usbserial-11410
```

**Note:** ESP32-S2 runs 34 tests (WiFi Serial Adapter tests excluded, Web Interface tests included).

### Run Tests on Arduino (Basic Tests Only)

```bash
# Arduino Mega (recommended for AVR platform testing)
platformio test -e mega

# Arduino Uno (minimal memory, may struggle with full test suite)
platformio test -e uno
```

**Note:** Arduino AVR platforms run 30 tests only (no WiFi/web tests).

### Run Tests on Other Platforms

```bash
# ESP32 (full WiFi tests)
platformio test -e esp32

# ESP8266 (full WiFi tests)platformio test -e esp8266
```

## Troubleshooting

### Upload Failed on ESP32-S3

If you see `*** [upload] Error 1` or `*** [upload] Error 2`:

**Method 1: Reset to bootloader mode**
1. Hold down the **BOOT** button (or **B** button) on the ESP32-S3
2. While holding BOOT, press and release the **RESET** button (or **R** button)
3. Release the BOOT button
4. Immediately run: `platformio test -e esp32s3`

**Method 2: Use esptool directly**
```bash
# Check if device is detected
platformio device list

# Try manual upload with reset
platformio test -e esp32s3 --upload-port /dev/cu.usbmodem11101 --test-port /dev/cu.usbmodem11101
```

**Method 3: Change USB connection**
- Try a different USB cable (must support data, not just power)
- Try a different USB port on your computer
- Some USB-C hubs cause issues - try direct connection

### Port Permission Denied
```bash
# Linux
sudo usermod -a -G dialout $USER
# Logout and login again

# macOS - no special permissions needed
```

### Upload Failed
- Verify device is connected: `pio device list`
- Ensure no other program (Serial Monitor, Arduino IDE) is using the port
- Try pressing BOOT button on ESP32 during upload
- Check USB cable (must support data, not just power)

### Test Timeout
- Increase test timeout in test file if needed
- Some tests take up to 10 seconds (thermal simulation)
- Check serial baudrate matches (115200)

### Out of Memory (Arduino Uno/Nano)
- Arduino AVR boards may not have enough RAM for tests
- Use ESP32, ESP8266, or Arduino Mega for testing
- Uno/Nano are build-only environments

## Adding New Tests

1. Add test function to `test_embedded.cpp`:
```cpp
void test_my_new_feature() {
    // Your test code
    TEST_ASSERT_EQUAL(expected, actual);
}
```

2. Register test in `setup()`:
```cpp
void setup() {
    UNITY_BEGIN();
    // ... existing tests ...
    RUN_TEST(test_my_new_feature);
    UNITY_END();
}
```

3. Run tests: `pio test -e esp32s2`

## Further Reading

- [Unity Test Framework](https://github.com/ThrowTheSwitch/Unity)
- [PlatformIO Testing](https://docs.platformio.org/en/latest/plus/unit-testing.html)
- [EngineSimulator API](../docs/API.md)
- [Speeduino Protocol](../docs/PROTOCOL.md)
