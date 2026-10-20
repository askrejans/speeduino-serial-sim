/**
 * @file EngineSimulator.h
 * @brief A car lapping Biķernieki, and the Speeduino that runs its engine.
 *
 * Two layers, like the real thing:
 *   - Physics: a driver follows the track's speed envelope, a gearbox and
 *     clutch couple a 1.8 L engine to the wheels, the intake manifold fills
 *     by mass balance, coolant and intake air heat and cool.
 *   - ECU: Speeduino's own calculations on the sensed values — VE and
 *     ignition tables, warm-up/after-start/acceleration enrichment, EGO
 *     closed loop, idle control, deceleration fuel cut, rev limiters,
 *     launch control — encoded into the 130-byte realtime packet.
 *
 * A session runs by itself: key on → crank → warm-up idle → launch from the
 * pit lane → out-lap → hot laps → cool-down lap → idle in the pit → again.
 * The physics advances in fixed steps (SIM_STEP_MS) from ITimeProvider, so
 * tests can drive it with a fake clock.
 */
#pragma once

#include <stdint.h>
#include "Config.h"
#include "EngineStatus.h"
#include "IRandomProvider.h"
#include "ITimeProvider.h"

enum class Phase : uint8_t {
    Off,        ///< key on, engine stopped
    Cranking,
    WarmUp,     ///< idling in the pit until the coolant is warm enough
    Launch,     ///< launch control on the pit exit, then clutch drop
    OutLap,
    HotLap,
    CoolLap,    ///< slow lap, stops at the start line
    Pit,        ///< idling after the run
};

const char* phaseName(Phase p);

struct SimSettings {
    float pace = 1.0f;            ///< 0.5 … 1.05 of the grip envelope on hot laps
    uint8_t hotLaps = 5;
    float ambientC = 18.0f;
    float startCoolantC = 55.0f;  ///< coolant temperature at key-on
    float baroKpa = 101.3f;
    bool autoStart = true;        ///< start the engine and run sessions without commands
    bool launchControl = true;
    bool sensorNoise = SIM_DETAILED != 0;
};

/// What a lap timer, GPS and IMU would see (not part of the Speeduino packet).
struct Telemetry {
    float distanceM = 0;     ///< since the start line
    float speedKmh = 0;
    uint8_t gear = 0;        ///< 0 = neutral
    uint16_t lap = 0;        ///< laps completed this power cycle
    float lapTimeS = 0;      ///< running lap
    float lastLapS = 0;
    float bestLapS = 0;
    float latG = 0;
    float longG = 0;
    float brake = 0;         ///< 0 … 1
    float throttle = 0;      ///< driver pedal, %
    float oilC = 0;
    bool fanOn = false;
};

enum class Command : uint8_t { StartEngine, StopEngine, StartSession, Pit, Blip };

class EngineSimulator {
public:
    EngineSimulator(ITimeProvider* time, IRandomProvider* random);

    /// Reset to key-on with the current settings.
    void begin();
    /// Advance the physics to "now". Returns true when the packet changed.
    bool update();
    void command(Command c);

    const EngineStatus& getStatus() const { return status; }
    const Telemetry& telemetry() const { return tel; }
    Phase phase() const { return phase_; }
    SimSettings& settings() { return settings_; }
    uint32_t uptimeS() const { return uptimeMs / 1000; }
    /// 'S' handling in comms.cpp: zero secl, keep TunerStudio off the secondary port.
    void resetSeconds();

private:
    ITimeProvider* time;
    IRandomProvider* random;
    SimSettings settings_;
    EngineStatus status;
    Telemetry tel;
    Phase phase_ = Phase::Off;

    uint32_t lastMs = 0;
    uint32_t uptimeMs = 0;
    uint32_t seclBaseMs = 0;
    bool tsOnSecondaryAllowed = true;
    float phaseS = 0;          // time in the current phase
    float runningS = 0;        // since the engine started

    // Physics state
    float rpm = 0;
    float speed = 0;           // m/s
    float distance = 0;        // m along the lap
    uint8_t gear = 0;          // 0 = neutral, else 1 … GEAR_COUNT
    float shiftTimer = 0;
    uint8_t shiftTarget = 0;
    float clutch = 1;          // 0 open … 1 engaged
    float pedal = 0;           // driver throttle, %
    float throttle = 0;        // throttle plate, % (after actuator lag)
    float brake = 0;           // m/s² requested
    float mapKpa = 101;
    float idleValve = 0.3f;    // 0 … 1 of the idle air bypass
    float coolant = 20, intake = 20, oil = 20;
    bool fan = false;
    bool fuelCut = false;
    bool sparkCut = false;
    float dfcoTimer = 0;
    float blipTimer = 0;
    uint8_t hotLapsDone = 0;

    // ECU state
    float ego = 100;
    float egoTimer = 0;
    float aeAmount = 100;
    float aeTimer = 0;
    float film = 0;
    float prevFilmMap = 101;
    float lambdaSensed = 1.0f;
    float lambdaDelay[4] = {1, 1, 1, 1};
    float prevTps = 0, prevMap = 101, prevRpm = 0;
    float tpsDot = 0, mapDot = 0, rpmDot = 0;
    float idleIntegral = 0;

    void stepPhysics(float dt);
    void stepDriver(float dt);
    void stepEngine(float dt);
    void stepThermal(float dt);
    void stepEcu(float dt);
    void setPhase(Phase p);
    void startLap();

    float noise(float amplitude);
    float idleTarget() const;
    float steadyMap(float tps, float valve, float rpmNow) const;
    float throttleFor(float mapTarget, float rpmNow) const;
    float trueVe(float rpmNow, float kpa) const;
    float wheelRpm(uint8_t g) const;
};
