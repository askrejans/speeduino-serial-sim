/**
 * @file test_embedded.cpp
 * @brief Comprehensive unit tests for Speeduino Serial Simulator
 * 
 * Tests both EngineSimulator and SpeeduinoProtocol on embedded hardware.
 * Run with: pio test -e esp32s2 (or other embedded environment)
 */

#include <unity.h>
#include "../include/EngineSimulator.h"
#include "../include/SpeeduinoProtocol.h"
#include "../include/PlatformAdapters.h"

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
    
    UNITY_END();
}

void loop() {
    // Tests run once in setup()
}
