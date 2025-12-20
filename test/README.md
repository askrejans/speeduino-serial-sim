# Testing Guide

## Overview

The Speeduino Serial Simulator includes comprehensive embedded unit tests using the Unity test framework. All tests run directly on hardware (ESP32, ESP8266, Arduino).

## Test File

- `test_embedded.cpp` - Comprehensive tests for EngineSimulator and SpeeduinoProtocol (16 test cases)

## Running Tests

### Requirements
- Physical hardware connected via USB
- PlatformIO installed
- Correct serial port permissions

### Run Tests on ESP32-S2

```bash
# Upload and run tests
pio test -e esp32s2

# Verbose output
pio test -e esp32s2 -vvv

# Specify serial port
pio test -e esp32s2 --upload-port /dev/cu.usbserial-11410 --test-port /dev/cu.usbserial-11410
```

### Run Tests on Other Platforms

```bash
# ESP32
pio test -e esp32

# ESP32-S3
pio test -e esp32s3

# ESP8266
pio test -e esp8266

# Arduino Mega (has more memory than Uno)
pio test -e mega
```

## Test Coverage

### Engine Simulator Tests (8 tests)
- `test_simulator_initialization` - Verify initial state
- `test_rpm_stays_within_bounds` - RPM limits validation
- `test_startup_to_warmup` - State machine transitions
- `test_coolant_temperature_increases` - Thermal simulation
- `test_map_correlates_with_throttle` - MAP/throttle correlation
- `test_volumetric_efficiency` - VE calculation range
- `test_engine_status_size` - Structure size validation (79 bytes)
- `test_runtime_tracking` - Runtime counter accuracy

### Protocol Tests (8 tests)
- `test_command_A_realtime_data` - 'A' command (79-byte response)
- `test_command_V_version` - 'V' command (version string)
- `test_command_Q_status` - 'Q' command (4-byte status)
- `test_command_S_signature` - 'S' command (20-byte signature)
- `test_command_n_page_sizes` - 'n' command (page sizes)
- `test_unknown_command` - Error handling for invalid commands
- `test_command_counter` - Command statistics tracking
- `test_no_command_available` - Empty buffer handling

## Test Output

Successful test run output:
```
Testing...
--------------------------------------------------------------------
test_simulator_initialization    [PASSED]
test_rpm_stays_within_bounds      [PASSED]
test_startup_to_warmup            [PASSED]
test_coolant_temperature_increases [PASSED]
test_map_correlates_with_throttle  [PASSED]
test_volumetric_efficiency        [PASSED]
test_engine_status_size           [PASSED]
test_runtime_tracking             [PASSED]
test_command_A_realtime_data      [PASSED]
test_command_V_version            [PASSED]
test_command_Q_status             [PASSED]
test_command_S_signature          [PASSED]
test_command_n_page_sizes         [PASSED]
test_unknown_command              [PASSED]
test_command_counter              [PASSED]
test_no_command_available         [PASSED]
--------------------------------------------------------------------
16 Tests 0 Failures 0 Ignored
OK
```

## Troubleshooting

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

## CI/CD Integration

For GitHub Actions or other CI systems, hardware-based tests require self-hosted runners with connected devices. See `.github/workflows/ci.yml` for example configuration.

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
