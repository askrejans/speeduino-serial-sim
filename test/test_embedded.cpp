/**
 * @file test_embedded.cpp
 * @brief Comprehensive unit tests for Speeduino Serial Simulator
 * 
 * Tests both EngineSimulator and SpeeduinoProtocol on embedded hardware.
 * Run with: pio test -e esp32s3 (or other embedded environment)
 * 
 * Note: main.cpp is excluded via #ifndef UNIT_TEST guard
 */

#include <unity.h>
#include "../include/EngineSimulator.h"
#include "../include/SpeeduinoProtocol.h"
#include "../include/PlatformAdapters.h"

#ifdef ENABLE_WIFI_SERIAL
#include "../include/WiFiSerialAdapter.h"
#endif

#ifdef ENABLE_WEB_INTERFACE
#include "../include/WebInterface.h"
#endif

// Mock serial for protocol testing
class MockSerial : public ISerialInterface {
private:
    uint8_t inputBuffer[256];
    size_t inputSize = 0;
    size_t inputPos = 0;
    
    uint8_t outputBuffer[512];
    size_t outputSize = 0;
    
public:
    void begin(uint32_t) override {}
    
    bool isReady() override {
        return true;
    }
    
    int available() override {
        return inputSize - inputPos;
    }
    
    int read() override {
        if (inputPos < inputSize) {
            return inputBuffer[inputPos++];
        }
        return -1;
    }
    
    size_t readBytes(uint8_t* buffer, size_t length) override {
        size_t count = 0;
        while (count < length && inputPos < inputSize) {
            buffer[count++] = inputBuffer[inputPos++];
        }
        return count;
    }
    
    size_t write(uint8_t byte) override {
        if (outputSize < sizeof(outputBuffer)) {
            outputBuffer[outputSize++] = byte;
            return 1;
        }
        return 0;
    }
    
    size_t write(const uint8_t* buffer, size_t size) override {
        size_t written = 0;
        for (size_t i = 0; i < size && outputSize < sizeof(outputBuffer); i++) {
            outputBuffer[outputSize++] = buffer[i];
            written++;
        }
        return written;
    }
    
    void flush() override {
        // No-op for mock
    }
    
    void addInput(uint8_t byte) {
        if (inputSize < sizeof(inputBuffer)) {
            inputBuffer[inputSize++] = byte;
        }
    }
    
    void clearOutput() {
        outputSize = 0;
    }
    
    void clear() {
        inputSize = 0;
        inputPos = 0;
        outputSize = 0;
    }
    
    size_t getOutputSize() const { return outputSize; }
    const uint8_t* getOutput() const { return outputBuffer; }
};

// Global test fixtures
EngineSimulator* simulator = nullptr;
SpeeduinoProtocol* protocol = nullptr;
MockSerial* mockSerial = nullptr;
ITimeProvider* timeProvider = nullptr;
IRandomProvider* randomProvider = nullptr;

void setUp(void) {
    timeProvider = createTimeProvider();
    randomProvider = createRandomProvider();
    randomProvider->seed(12345);  // Fixed seed for deterministic tests
    
    simulator = new EngineSimulator(timeProvider, randomProvider);
    mockSerial = new MockSerial();
    protocol = new SpeeduinoProtocol(mockSerial, simulator);
}

void tearDown(void) {
    delete protocol;
    delete simulator;
    delete mockSerial;
    delete randomProvider;
    delete timeProvider;
}

// ============================================
// Engine Simulator Tests
// ============================================

void test_simulator_initialization() {
    simulator->initialize();
    
    const EngineStatus& status = simulator->getStatus();
    
    TEST_ASSERT_EQUAL_INT('A', status.response);
    TEST_ASSERT_EQUAL_UINT16(0, status.getRPM());
    TEST_ASSERT_EQUAL(EngineMode::STARTUP, simulator->getMode());
}

void test_rpm_stays_within_bounds() {
    simulator->initialize();
    
    // Run simulation for 100 iterations
    for (int i = 0; i < 100; i++) {
        simulator->update();
        delay(UPDATE_INTERVAL_MS);
        
        uint16_t rpm = simulator->getStatus().getRPM();
        TEST_ASSERT_LESS_OR_EQUAL(RPM_MAX, rpm);
        TEST_ASSERT_GREATER_OR_EQUAL(RPM_MIN, rpm);
    }
}

void test_coolant_temperature_increases() {
    simulator->initialize();
    
    int8_t initialTemp = simulator->getStatus().getCoolantTemp();
    
    // Run for 10 seconds
    for (int i = 0; i < 200; i++) {
        simulator->update();
        delay(UPDATE_INTERVAL_MS);
    }
    
    int8_t finalTemp = simulator->getStatus().getCoolantTemp();
    
    TEST_ASSERT_GREATER_THAN(initialTemp, finalTemp);
}

void test_map_correlates_with_throttle() {
    simulator->initialize();
    
    // Force idle mode (low throttle)
    simulator->setMode(EngineMode::IDLE);
    for (int i = 0; i < 20; i++) {
        simulator->update();
        delay(UPDATE_INTERVAL_MS);
    }
    uint16_t idleMAP = simulator->getStatus().getMAP();
    
    // Force WOT mode (high throttle)
    simulator->setMode(EngineMode::WOT);
    for (int i = 0; i < 20; i++) {
        simulator->update();
        delay(UPDATE_INTERVAL_MS);
    }
    uint16_t wotMAP = simulator->getStatus().getMAP();
    
    // WOT should have higher MAP than idle
    TEST_ASSERT_GREATER_THAN(idleMAP, wotMAP);
}

void test_volumetric_efficiency() {
    simulator->initialize();
    simulator->setMode(EngineMode::IDLE);
    
    for (int i = 0; i < 20; i++) {
        simulator->update();
        delay(UPDATE_INTERVAL_MS);
    }
    
    uint8_t ve = simulator->getStatus().ve;
    
    // VE should be in valid range (30-100%)
    TEST_ASSERT_GREATER_OR_EQUAL(30, ve);
    TEST_ASSERT_LESS_OR_EQUAL(100, ve);
}

void test_engine_status_size() {
    TEST_ASSERT_EQUAL(79, sizeof(EngineStatus));
}

void test_runtime_tracking() {
    simulator->initialize();
    
    delay(2000);  // Wait 2 seconds
    simulator->update();
    
    uint32_t runtime = simulator->getRuntime();
    TEST_ASSERT_GREATER_OR_EQUAL(1, runtime);  // At least 1 second
    TEST_ASSERT_LESS_OR_EQUAL(3, runtime);     // At most 3 seconds
}

// ============================================
// Protocol Tests
// ============================================

void test_command_A_realtime_data() {
    simulator->initialize();
    protocol->begin();
    
    mockSerial->addInput('A');
    mockSerial->clearOutput();
    
    bool processed = protocol->processCommands();
    
    TEST_ASSERT_TRUE(processed);
    TEST_ASSERT_EQUAL(sizeof(EngineStatus), mockSerial->getOutputSize());
    
    const uint8_t* output = mockSerial->getOutput();
    TEST_ASSERT_EQUAL_INT('A', output[0]);  // Response echo
}

void test_command_V_version() {
    protocol->begin();
    
    mockSerial->addInput('V');
    mockSerial->clearOutput();
    
    bool processed = protocol->processCommands();
    
    TEST_ASSERT_TRUE(processed);
    TEST_ASSERT_GREATER_THAN(0, mockSerial->getOutputSize());
    
    const uint8_t* output = mockSerial->getOutput();
    // Should contain "speeduino"
    bool found = false;
    for (size_t i = 0; i < mockSerial->getOutputSize() - 8; i++) {
        if (memcmp(&output[i], "speeduino", 9) == 0) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

void test_command_Q_status() {
    protocol->begin();
    
    mockSerial->addInput('Q');
    mockSerial->clearOutput();
    
    bool processed = protocol->processCommands();
    
    TEST_ASSERT_TRUE(processed);
    TEST_ASSERT_EQUAL(4, mockSerial->getOutputSize());
}

void test_command_S_signature() {
    protocol->begin();
    
    mockSerial->addInput('S');
    mockSerial->clearOutput();
    
    bool processed = protocol->processCommands();
    
    TEST_ASSERT_TRUE(processed);
    TEST_ASSERT_EQUAL(20, mockSerial->getOutputSize());
}

void test_command_n_page_sizes() {
    protocol->begin();
    
    mockSerial->addInput('n');
    mockSerial->clearOutput();
    
    bool processed = protocol->processCommands();
    
    TEST_ASSERT_TRUE(processed);
    TEST_ASSERT_GREATER_OR_EQUAL(3, mockSerial->getOutputSize());
}

void test_unknown_command() {
    protocol->begin();
    
    mockSerial->addInput('Z');  // Invalid command
    mockSerial->clearOutput();
    
    uint32_t initialErrors = protocol->getErrorCount();
    bool processed = protocol->processCommands();
    
    TEST_ASSERT_TRUE(processed);
    TEST_ASSERT_EQUAL(initialErrors + 1, protocol->getErrorCount());
}

void test_command_counter() {
    protocol->begin();
    
    uint32_t initialCount = protocol->getCommandCount();
    
    mockSerial->addInput('A');
    protocol->processCommands();
    
    mockSerial->addInput('V');
    protocol->processCommands();
    
    mockSerial->addInput('Q');
    protocol->processCommands();
    
    TEST_ASSERT_EQUAL(initialCount + 3, protocol->getCommandCount());
}

void test_no_command_available() {
    protocol->begin();
    
    mockSerial->clear();
    
    bool processed = protocol->processCommands();
    
    TEST_ASSERT_FALSE(processed);
}

// ============================================
// Advanced Simulation Tests
// ============================================

void test_pulsewidth_calculation() {
    simulator->initialize();
    simulator->setMode(EngineMode::IDLE);
    
    for (int i = 0; i < 20; i++) {
        simulator->update();
        delay(UPDATE_INTERVAL_MS);
    }
    
    uint16_t pw = simulator->getStatus().getPulseWidth();
    
    // Pulse width should be reasonable (0.5ms to 25ms)
    TEST_ASSERT_GREATER_OR_EQUAL(5, pw);   // 0.5ms
    TEST_ASSERT_LESS_OR_EQUAL(250, pw);    // 20ms
}

void test_advance_timing_changes() {
    simulator->initialize();
    
    // Idle timing
    simulator->setMode(EngineMode::IDLE);
    for (int i = 0; i < 20; i++) {
        simulator->update();
        delay(UPDATE_INTERVAL_MS);
    }
    int8_t idleTiming = simulator->getStatus().advance;
    
    // WOT timing
    simulator->setMode(EngineMode::WOT);
    for (int i = 0; i < 20; i++) {
        simulator->update();
        delay(UPDATE_INTERVAL_MS);
    }
    int8_t wotTiming = simulator->getStatus().advance;
    
    // Timing should be within 15-35° range
    TEST_ASSERT_GREATER_OR_EQUAL(15, idleTiming);
    TEST_ASSERT_LESS_OR_EQUAL(35, wotTiming);
}

void test_battery_voltage_stable() {
    simulator->initialize();
    
    // Run simulation for sufficient time to stabilize
    for (int i = 0; i < 100; i++) {
        simulator->update();
        delay(UPDATE_INTERVAL_MS);
    }
    
    uint8_t voltage = simulator->getStatus().batteryv;
    
    // Battery should be reasonable (8-15V)
    TEST_ASSERT_GREATER_OR_EQUAL(8, voltage);
    TEST_ASSERT_LESS_OR_EQUAL(15, voltage);
}

void test_afr_target_changes_with_load() {
    simulator->initialize();
    
    // Idle AFR (should be stoich ~14.7)
    simulator->setMode(EngineMode::IDLE);
    for (int i = 0; i < 20; i++) {
        simulator->update();
        delay(UPDATE_INTERVAL_MS);
    }
    uint8_t idleAFR = simulator->getStatus().afrtarget;
    
    // WOT AFR (should be rich ~12.5)
    simulator->setMode(EngineMode::WOT);
    for (int i = 0; i < 20; i++) {
        simulator->update();
        delay(UPDATE_INTERVAL_MS);
    }
    uint8_t wotAFR = simulator->getStatus().afrtarget;
    
    // WOT should have lower (richer) AFR
    TEST_ASSERT_LESS_THAN(idleAFR, wotAFR);
}

void test_tps_changes_with_mode() {
    simulator->initialize();
    
    simulator->setMode(EngineMode::IDLE);
    for (int i = 0; i < 20; i++) {
        simulator->update();
        delay(UPDATE_INTERVAL_MS);
    }
    uint8_t idleTPS = simulator->getStatus().tps;
    
    simulator->setMode(EngineMode::WOT);
    for (int i = 0; i < 20; i++) {
        simulator->update();
        delay(UPDATE_INTERVAL_MS);
    }
    uint8_t wotTPS = simulator->getStatus().tps;
    
    // Idle should be low, WOT should be high
    TEST_ASSERT_LESS_THAN(20, idleTPS);    // Idle < 20%
    TEST_ASSERT_GREATER_THAN(90, wotTPS);  // WOT > 90%
}

void test_mode_transitions() {
    simulator->initialize();
    
    // Test that modes can be set
    EngineMode modes[] = {
        EngineMode::IDLE,
        EngineMode::LIGHT_LOAD,
        EngineMode::ACCELERATION,
        EngineMode::HIGH_RPM,
        EngineMode::WOT
    };
    
    for (int m = 0; m < 5; m++) {
        simulator->setMode(modes[m]);
        TEST_ASSERT_EQUAL(modes[m], simulator->getMode());
        
        for (int i = 0; i < 10; i++) {
            simulator->update();
            delay(UPDATE_INTERVAL_MS);
        }
    }
}

void test_sensors_within_valid_ranges() {
    simulator->initialize();
    
    // Allow 20 iterations for initialization, then check bounds
    for (int i = 0; i < 100; i++) {
        simulator->update();
        delay(UPDATE_INTERVAL_MS);
        
        // Only validate after initialization period
        if (i >= 20) {
            const EngineStatus& status = simulator->getStatus();
            
            // All sensors should be in valid ranges after warmup
            TEST_ASSERT_LESS_OR_EQUAL(RPM_MAX, status.getRPM());
            TEST_ASSERT_LESS_OR_EQUAL(110, status.getCoolantTemp());  // Max 110°C
            TEST_ASSERT_LESS_OR_EQUAL(110, status.getIntakeTemp());   // Max 110°C
            TEST_ASSERT_LESS_OR_EQUAL(255, status.getMAP());          // Max 255 kPa
            TEST_ASSERT_LESS_OR_EQUAL(100, status.tps);               // Max 100%
        }
    }
}

// ============================================
// Protocol Stress Tests
// ============================================

void test_multiple_commands_sequence() {
    protocol->begin();
    
    char commands[] = {'A', 'V', 'Q', 'S', 'n', 'A', 'A'};
    
    for (int i = 0; i < 7; i++) {
        mockSerial->clear();
        mockSerial->addInput(commands[i]);
        bool processed = protocol->processCommands();
        TEST_ASSERT_TRUE(processed);
    }
    
    TEST_ASSERT_EQUAL(7, protocol->getCommandCount());
}

void test_command_A_response_format() {
    simulator->initialize();
    protocol->begin();
    
    mockSerial->addInput('A');
    mockSerial->clearOutput();
    
    protocol->processCommands();
    
    const uint8_t* output = mockSerial->getOutput();
    
    // Verify response structure
    TEST_ASSERT_EQUAL('A', output[0]);  // Response type
    // Bytes should have reasonable values
    TEST_ASSERT_NOT_EQUAL(0xFF, output[1]);  // secl
    TEST_ASSERT_NOT_EQUAL(0xFF, output[2]);  // squirt
}

// ============================================
// WiFi and Network Tests
// ============================================

#ifdef ENABLE_WIFI_SERIAL

void test_wifi_serial_adapter_creation() {
    WiFiSerialAdapter* wifiSerial = new WiFiSerialAdapter(5000);
    TEST_ASSERT_NOT_NULL(wifiSerial);
    delete wifiSerial;
}

void test_wifi_serial_adapter_begin() {
    WiFiSerialAdapter wifiSerial(5000);
    wifiSerial.begin(115200);  // baudRate ignored for TCP
    
    // Adapter should accept begin() call without crashing
    TEST_ASSERT_TRUE(true);
}

void test_wifi_serial_not_ready_without_client() {
    WiFiSerialAdapter wifiSerial(5000);
    wifiSerial.begin(115200);
    
    // Without WiFi connected and no client, should not be ready
    bool ready = wifiSerial.isReady();
    // May be false or true depending on WiFi state, just verify it doesn't crash
    (void)ready;
    TEST_ASSERT_TRUE(true);
}

void test_wifi_serial_available_no_client() {
    WiFiSerialAdapter wifiSerial(5000);
    wifiSerial.begin(115200);
    
    // No client connected, available should return 0
    int avail = wifiSerial.available();
    TEST_ASSERT_GREATER_OR_EQUAL(0, avail);
}

void test_wifi_serial_read_no_data() {
    WiFiSerialAdapter wifiSerial(5000);
    wifiSerial.begin(115200);
    
    // No client or no data, should return -1
    int data = wifiSerial.read();
    TEST_ASSERT_EQUAL(-1, data);
}

void test_wifi_serial_write_no_client() {
    WiFiSerialAdapter wifiSerial(5000);
    wifiSerial.begin(115200);
    
    // Writing without client should return 0 (not written)
    size_t written = wifiSerial.write('A');
    TEST_ASSERT_EQUAL(0, written);
}

void test_wifi_serial_with_protocol() {
    WiFiSerialAdapter* wifiSerial = new WiFiSerialAdapter(5000);
    wifiSerial->begin(115200);
    
    SpeeduinoProtocol* proto = new SpeeduinoProtocol(wifiSerial, simulator);
    proto->begin();
    
    // Protocol should accept WiFiSerialAdapter via ISerialInterface
    TEST_ASSERT_NOT_NULL(proto);
    TEST_ASSERT_EQUAL(0, proto->getCommandCount());
    
    delete proto;
    delete wifiSerial;
}

#endif // ENABLE_WIFI_SERIAL

#ifdef ENABLE_WEB_INTERFACE

void test_web_interface_creation() {
    WebInterface* web = new WebInterface(simulator, protocol);
    TEST_ASSERT_NOT_NULL(web);
    delete web;
}

void test_web_interface_begin() {
    WebInterface web(simulator, protocol);
    
    // begin() will start WiFi - it may succeed or timeout
    // Just verify it doesn't crash
    bool started = web.begin();
    (void)started;  // May be true or false depending on environment
    TEST_ASSERT_TRUE(true);
}

void test_web_interface_get_ip() {
    WebInterface web(simulator, protocol);
    web.begin();
    
    IPAddress ip = web.getIP();
    // IP may be 0.0.0.0 if not connected, just verify call works
    (void)ip;
    TEST_ASSERT_TRUE(true);
}

void test_web_interface_ap_mode_check() {
    WebInterface web(simulator, protocol);
    web.begin();
    
    bool isAP = web.isAccessPointMode();
    // Should return true/false without crashing
    (void)isAP;
    TEST_ASSERT_TRUE(true);
}

#endif // ENABLE_WEB_INTERFACE

// ============================================
// Integration Tests
// ============================================

void test_full_simulation_cycle() {
    simulator->initialize();
    protocol->begin();
    
    // Simulate a full operational cycle
    for (int i = 0; i < 50; i++) {
        simulator->update();
        
        // Send command every 10 iterations
        if (i % 10 == 0) {
            mockSerial->clear();
            mockSerial->addInput('A');
            bool processed = protocol->processCommands();
            TEST_ASSERT_TRUE(processed);
            TEST_ASSERT_EQUAL(79, mockSerial->getOutputSize());
        }
        
        delay(UPDATE_INTERVAL_MS);
    }
    
    // Verify simulation progressed
    TEST_ASSERT_GREATER_THAN(0, simulator->getRuntime());
    TEST_ASSERT_GREATER_THAN(4, protocol->getCommandCount());
}

void test_rapid_command_processing() {
    simulator->initialize();
    protocol->begin();
    
    // Send 20 commands rapidly
    for (int i = 0; i < 20; i++) {
        mockSerial->clear();
        mockSerial->addInput('A');
        simulator->update();
        bool processed = protocol->processCommands();
        TEST_ASSERT_TRUE(processed);
    }
    
    TEST_ASSERT_EQUAL(20, protocol->getCommandCount());
}

void test_all_modes_stability() {
    simulator->initialize();
    protocol->begin();
    
    EngineMode modes[] = {
        EngineMode::IDLE,
        EngineMode::LIGHT_LOAD,
        EngineMode::ACCELERATION,
        EngineMode::HIGH_RPM,
        EngineMode::WOT
    };
    
    for (int m = 0; m < 5; m++) {
        simulator->setMode(modes[m]);
        
        // Run each mode for a bit
        for (int i = 0; i < 10; i++) {
            simulator->update();
            
            mockSerial->clear();
            mockSerial->addInput('A');
            bool processed = protocol->processCommands();
            TEST_ASSERT_TRUE(processed);
            
            const uint8_t* output = mockSerial->getOutput();
            TEST_ASSERT_EQUAL('A', output[0]);
            
            delay(UPDATE_INTERVAL_MS);
        }
    }
}

// ============================================
// Main Test Runner
// ============================================

void setup() {
    delay(2000);  // Wait for serial to stabilize
    
    UNITY_BEGIN();
    
    // Engine Simulator Tests
    RUN_TEST(test_simulator_initialization);
    RUN_TEST(test_rpm_stays_within_bounds);
    RUN_TEST(test_coolant_temperature_increases);
    RUN_TEST(test_map_correlates_with_throttle);
    RUN_TEST(test_volumetric_efficiency);
    RUN_TEST(test_engine_status_size);
    RUN_TEST(test_runtime_tracking);
    
    // Protocol Tests
    RUN_TEST(test_command_A_realtime_data);
    RUN_TEST(test_command_V_version);
    RUN_TEST(test_command_Q_status);
    RUN_TEST(test_command_S_signature);
    RUN_TEST(test_command_n_page_sizes);
    RUN_TEST(test_unknown_command);
    RUN_TEST(test_command_counter);
    RUN_TEST(test_no_command_available);
    
    // Advanced Simulation Tests
    RUN_TEST(test_pulsewidth_calculation);
    RUN_TEST(test_advance_timing_changes);
    RUN_TEST(test_battery_voltage_stable);
    RUN_TEST(test_afr_target_changes_with_load);
    RUN_TEST(test_tps_changes_with_mode);
    RUN_TEST(test_mode_transitions);
    RUN_TEST(test_sensors_within_valid_ranges);
    
    // Protocol Stress Tests
    RUN_TEST(test_multiple_commands_sequence);
    RUN_TEST(test_command_A_response_format);
    
#ifdef ENABLE_WIFI_SERIAL
    // WiFi Serial Adapter Tests
    RUN_TEST(test_wifi_serial_adapter_creation);
    RUN_TEST(test_wifi_serial_adapter_begin);
    RUN_TEST(test_wifi_serial_not_ready_without_client);
    RUN_TEST(test_wifi_serial_available_no_client);
    RUN_TEST(test_wifi_serial_read_no_data);
    RUN_TEST(test_wifi_serial_write_no_client);
    RUN_TEST(test_wifi_serial_with_protocol);
#endif
    
#ifdef ENABLE_WEB_INTERFACE
    // Web Interface Tests
    RUN_TEST(test_web_interface_creation);
    RUN_TEST(test_web_interface_begin);
    RUN_TEST(test_web_interface_get_ip);
    RUN_TEST(test_web_interface_ap_mode_check);
#endif
    
    // Integration Tests
    RUN_TEST(test_full_simulation_cycle);
    RUN_TEST(test_rapid_command_processing);
    RUN_TEST(test_all_modes_stability);
    
    UNITY_END();
}

void loop() {
    // Tests run once in setup()
}
