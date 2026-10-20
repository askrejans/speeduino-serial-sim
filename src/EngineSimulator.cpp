/**
 * @file EngineSimulator.cpp
 * @brief Physics of a car lapping Biķernieki and the Speeduino logic on top.
 *
 * Calibration targets (checked by test/test_native): ~2:00 laps, idle
 * 850 rpm at ~35 kPa, overrun ~15 kPa, WOT ~99 kPa, idle PW ~2.3 ms, WOT PW
 * ~6–7 ms, coolant held near 90 °C on hot laps with the fan cycling in the
 * pit.
 */
#include "EngineSimulator.h"
#include "Track.h"
#include "Vehicle.h"
#include <math.h>
#include <string.h>

using namespace vehicle;

namespace {

constexpr float PI_F = 3.14159265f;
constexpr float G = 9.81f;
constexpr float AIR_DENSITY = 1.2f;

// Intake: inflow through the throttle and idle bypass, outflow into the
// cylinders. Units are arbitrary but consistent; tuned so WOT reaches ~99 kPa
// and closed throttle on overrun ~15 kPa.
constexpr float LEAK_AREA = 0.0015f;
constexpr float IDLE_VALVE_AREA = 0.012f;
constexpr float FLOW_IN = 14.0f;
constexpr float RESIDUAL_KPA = 14.0f;  // exhaust residual; pumping stops below it
constexpr float FRICTION_MAP_KPA = 18.0f;

#if SIM_DETAILED
constexpr uint8_t BISECTIONS = 16;
#else
constexpr uint8_t BISECTIONS = 10;
#endif

float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
float lerp(float a, float b, float t) { return a + (b - a) * t; }
float approach(float v, float target, float dt, float tau) {
    return v + (target - v) * clampf(dt / tau, 0.0f, 1.0f);
}

float plateArea(float tps) {
    float x = 1.0f - cosf(clampf(tps, 0, 100) / 100.0f * PI_F / 2.0f);
    return x <= 0 ? 0 : powf(x, 0.55f) * 0.996f;
}

float flowFactor(float pressureRatio) {
    // Choked (sonic) below 0.528, falling to zero as the manifold reaches baro.
    if (pressureRatio < 0.528f) return 1.0f;
    float k = (pressureRatio - 0.528f) / 0.472f;
    return k >= 1 ? 0 : sqrtf(1.0f - k * k);
}

float wotTorque(float rpm) {
    float pos = clampf(rpm, 0, 7500) / 500.0f;
    uint8_t i = (uint8_t)pos;
    if (i >= TORQUE_POINTS - 1) return pgm_read_byte(&TORQUE_WOT_NM[TORQUE_POINTS - 1]);
    float a = pgm_read_byte(&TORQUE_WOT_NM[i]), b = pgm_read_byte(&TORQUE_WOT_NM[i + 1]);
    return lerp(a, b, pos - i);
}

float friction(float rpm) { return 8.0f + 0.002f * rpm; }

float engineTorque(float rpm, float map, float baro) {
    return wotTorque(rpm) * (map - FRICTION_MAP_KPA) / (baro - FRICTION_MAP_KPA) - friction(rpm);
}

float rpmVe(float rpm) {
    float k = (rpm - 4600.0f) / 2600.0f;
    return 0.62f + 0.32f * expf(-k * k);
}

/// Warm-up enrichment table (coolant °C → %).
float wueFor(float clt) {
    if (clt >= 75) return 100;
    if (clt <= 0) return 160;
    if (clt < 20) return lerp(160, 135, clt / 20);
    if (clt < 40) return lerp(135, 118, (clt - 20) / 20);
    if (clt < 60) return lerp(118, 106, (clt - 40) / 20);
    return lerp(106, 100, (clt - 60) / 15);
}

/// AFR target table (lambda): stoichiometric cruise, richer with load.
float lambdaTargetFor(float rpm, float map, float clt) {
    float lambda = 1.0f;
    if (map > 70) lambda = lerp(1.0f, 0.88f, clampf((map - 70) / 22, 0, 1));
    if (map > 92 && rpm > 5000) lambda = 0.85f;
    if (rpm < 1100 && map < 50) lambda = 0.97f;
    if (clt < 60) lambda -= (60 - clt) * 0.002f;
    return lambda;
}

/// Ignition table, ° BTDC.
float advanceFor(float rpm, float map) {
    float r = rpm / 1000.0f;
    float adv = 10.0f + 6.5f * (r < 4 ? r : 4) + (r > 4 ? r - 4 : 0);
    if (map > 30) adv -= 0.12f * (map - 30);
    return clampf(adv, 8, 38);
}

}  // namespace

const char* phaseName(Phase p) {
    switch (p) {
        case Phase::Off: return "off";
        case Phase::Cranking: return "cranking";
        case Phase::WarmUp: return "warm-up";
        case Phase::Launch: return "launch";
        case Phase::OutLap: return "out-lap";
        case Phase::HotLap: return "hot lap";
        case Phase::CoolLap: return "cool-down";
        case Phase::Pit: return "pit";
    }
    return "?";
}

EngineSimulator::EngineSimulator(ITimeProvider* t, IRandomProvider* r) : time(t), random(r) {
    memset(&status, 0, sizeof(status));
}

void EngineSimulator::begin() {
    memset(&status, 0, sizeof(status));
    tel = Telemetry();
    lastMs = time->millis();
    uptimeMs = 0;
    seclBaseMs = 0;
    tsOnSecondaryAllowed = true;
    random->seed(lastMs ^ 0x5EED);
    rpm = speed = distance = 0;
    gear = 0;
    clutch = 0;
    pedal = throttle = brake = 0;
    mapKpa = settings_.baroKpa;
    coolant = settings_.startCoolantC;
    oil = settings_.startCoolantC - 5;
    intake = settings_.ambientC + (settings_.startCoolantC - settings_.ambientC) * 0.15f;
    idleValve = 0.3f;
    fan = fuelCut = sparkCut = false;
    ego = 100;
    aeAmount = 100;
    lambdaSensed = 1;
    for (float& l : lambdaDelay) l = 1;
    prevTps = 0;
    prevMap = mapKpa;
    prevRpm = 0;
    setPhase(Phase::Off);
    stepEcu(0);
}

void EngineSimulator::setPhase(Phase p) {
    phase_ = p;
    phaseS = 0;
    if (p == Phase::Launch) {
        distance = 0;
        speed = 0;
        gear = 1;
        clutch = 0;
        startLap();
    }
}

void EngineSimulator::startLap() { tel.lapTimeS = 0; }

void EngineSimulator::resetSeconds() {
    seclBaseMs = uptimeMs;
    tsOnSecondaryAllowed = false;
    status.secl = 0;
}

void EngineSimulator::command(Command c) {
    bool running = rpm > 300 && phase_ != Phase::Off && phase_ != Phase::Cranking;
    bool moving = phase_ == Phase::Launch || phase_ == Phase::OutLap || phase_ == Phase::HotLap ||
                  phase_ == Phase::CoolLap;
    switch (c) {
        case Command::StartEngine:
            if (phase_ == Phase::Off) setPhase(Phase::Cranking);
            break;
        case Command::StopEngine:
            gear = 0;
            clutch = 0;
            setPhase(Phase::Off);
            break;
        case Command::StartSession:
            if (running && !moving) setPhase(Phase::Launch);
            break;
        case Command::Pit:
            if (phase_ == Phase::OutLap || phase_ == Phase::HotLap) setPhase(Phase::CoolLap);
            break;
        case Command::Blip:
            if (running && !moving) blipTimer = 0.6f;
            break;
    }
}

bool EngineSimulator::update() {
    uint32_t now = time->millis();
    uint32_t elapsed = now - lastMs;
    if (elapsed < SIM_STEP_MS) return false;
    // Catch up at most a quarter second: a stalled loop must not replay minutes.
    uint8_t steps = 0;
    while (elapsed >= SIM_STEP_MS && steps < 250 / SIM_STEP_MS) {
        stepPhysics(SIM_STEP_MS / 1000.0f);
        lastMs += SIM_STEP_MS;
        uptimeMs += SIM_STEP_MS;
        elapsed -= SIM_STEP_MS;
        steps++;
    }
    if (elapsed >= SIM_STEP_MS) lastMs = now;
    return true;
}

float EngineSimulator::noise(float amplitude) {
    if (!settings_.sensorNoise) return 0;
    return amplitude * (float)random->random(-1000, 1001) / 1000.0f;
}

float EngineSimulator::idleTarget() const {
    return lerp(IDLE_RPM_COLD, IDLE_RPM_HOT, clampf((coolant - 10) / 60, 0, 1));
}

float EngineSimulator::wheelRpm(uint8_t g) const {
    if (g == 0) return 0;
    return speed / (2 * PI_F * WHEEL_RADIUS_M) * GEAR_RATIOS[g - 1] * FINAL_DRIVE * 60;
}

float EngineSimulator::trueVe(float r, float kpa) const {
    return rpmVe(r) * (0.85f + 0.15f * kpa / settings_.baroKpa);
}

float EngineSimulator::steadyMap(float tps, float valve, float r) const {
    const float baro = settings_.baroKpa;
    const float area = LEAK_AREA + valve * IDLE_VALVE_AREA + plateArea(tps);
    const float pump = (r < 50 ? 50 : r) / 1000.0f * rpmVe(r);
    float lo = RESIDUAL_KPA, hi = baro;
    for (uint8_t i = 0; i < BISECTIONS; i++) {
        float m = (lo + hi) / 2;
        float in = FLOW_IN * area * baro * flowFactor(m / baro);
        float out = pump * (0.85f + 0.15f * m / baro) * (m - RESIDUAL_KPA);
        if (in > out) lo = m; else hi = m;
    }
    return (lo + hi) / 2;
}

float EngineSimulator::throttleFor(float target, float r) const {
    // Inverse of steadyMap for the plate alone (idle valve on its dashpot).
    const float baro = settings_.baroKpa;
    float psi = flowFactor(target / baro);
    if (psi < 0.02f) return 100;
    float out = (r < 50 ? 50 : r) / 1000.0f * trueVe(r, target) * (target - RESIDUAL_KPA);
    float plate = out / (FLOW_IN * baro * psi) - LEAK_AREA - idleValve * IDLE_VALVE_AREA;
    if (plate <= 0) return 0;
    float x = powf(clampf(plate / 0.996f, 0, 1), 1.0f / 0.55f);
    return clampf(acosf(1.0f - x) * 2.0f / PI_F * 100.0f, 0, 100);
}

void EngineSimulator::stepPhysics(float dt) {
    phaseS += dt;
    stepDriver(dt);
    stepEngine(dt);
    stepThermal(dt);
    stepEcu(dt);
}

void EngineSimulator::stepDriver(float dt) {
    const bool onTrack = phase_ == Phase::OutLap || phase_ == Phase::HotLap || phase_ == Phase::CoolLap;
    switch (phase_) {
        case Phase::Off:
            pedal = 0;
            if (settings_.autoStart && phaseS > 2.0f) setPhase(Phase::Cranking);
            return;
        case Phase::Cranking:
            pedal = 0;
            if (phaseS > 0.9f) {
                runningS = 0;
                setPhase(Phase::WarmUp);
            }
            return;
        case Phase::WarmUp:
        case Phase::Pit: {
            gear = 0;
            clutch = 0;
            brake = 3;
            pedal = blipTimer > 0 ? 45 : 0;
            if (blipTimer > 0) blipTimer -= dt;
            bool ready = phase_ == Phase::WarmUp ? (coolant >= 60 && phaseS > 15) || phaseS > 300
                                                 : phaseS > 60;
            if (settings_.autoStart && ready) setPhase(Phase::Launch);
            return;
        }
        case Phase::Launch: {
            // Launch control holds the revs, then the clutch comes up.
            bool hold = settings_.launchControl ? phaseS < 1.5f : phaseS < 0.5f;
            pedal = settings_.launchControl ? 100 : 35;
            brake = 0;
            if (!hold) clutch = clampf(clutch + dt / (settings_.launchControl ? 0.6f : 1.2f), 0, 1);
            if (clutch >= 1 && speed > 8) setPhase(Phase::OutLap);
            break;
        }
        default:
            break;
    }
    if (!onTrack && phase_ != Phase::Launch) return;

    if (phase_ != Phase::Launch) {
        float pace = phase_ == Phase::HotLap ? clampf(settings_.pace, 0.5f, 1.05f)
                     : phase_ == Phase::OutLap ? 0.82f : 0.6f;
        float look = distance + speed * 0.4f;
        float target = track::envelope(look) * sqrtf(pace);
        if (phase_ == Phase::CoolLap) {
            float toLine = track::length() - distance - 2.0f;
            if (tel.lapTimeS > 10 && toLine < 400) target = fminf(target, sqrtf(2 * 2.0f * fmaxf(0, toLine)));
        }
        float resist = 0.5f * AIR_DENSITY * CDA_M2 * speed * speed + ROLLING * MASS_KG * G;
        float aDesired = clampf((target - speed) / 0.6f, -BRAKE_G * G, G);
        float need = MASS_KG * aDesired + resist;
        brake = 0;
        if (shiftTimer > 0) {
            pedal = 0;
        } else if (gear > 0) {
            float k = GEAR_RATIOS[gear - 1] * FINAL_DRIVE * DRIVELINE_EFFICIENCY / WHEEL_RADIUS_M;
            float fMax = fminf(engineTorque(rpm, settings_.baroKpa, settings_.baroKpa) * k,
                               TRACTION_G * MASS_KG * G);
            float fMin = engineTorque(rpm, 15, settings_.baroKpa) * k;
            if (need >= fMax) {
                pedal = 100;
            } else if (need <= fMin) {
                pedal = 0;
                brake = (fMin - need) / MASS_KG;
            } else {
                float frac = (need / k + friction(rpm)) / fmaxf(1, wotTorque(rpm));
                pedal = throttleFor(FRICTION_MAP_KPA + frac * (settings_.baroKpa - FRICTION_MAP_KPA), rpm);
            }
            // Gearbox: upshift on the limiter side, downshift while braking.
            if (rpm > SHIFT_RPM && gear < GEAR_COUNT && pedal > 50) {
                shiftTarget = gear + 1;
                shiftTimer = 0.22f;
            } else if (gear > 1 && brake > 1 && wheelRpm(gear - 1) < 6000) {
                gear--;
                blipTimer = 0.12f;
            }
            if (blipTimer > 0) {
                blipTimer -= dt;
                pedal = fmaxf(pedal, 30);
            }
        }
        // Rolling to a stop at the end of the cool-down lap.
        if (phase_ == Phase::CoolLap && speed < 3.0f && tel.lapTimeS > 10) {
            clutch = 0;
            gear = 0;
            brake = 4;
            pedal = 0;
            if (speed < 0.2f) {
                speed = 0;
                tel.lap++;
                tel.lastLapS = tel.lapTimeS;
                setPhase(Phase::Pit);
            }
        }
    }
    (void)dt;
}

void EngineSimulator::stepEngine(float dt) {
    const float baro = settings_.baroKpa;
    const bool running = phase_ != Phase::Off && phase_ != Phase::Cranking;

    // Throttle plate follows the pedal (cable + TPS filtering).
    throttle = approach(throttle, pedal, dt, 0.05f);

    // Idle control: closed loop on rpm when the throttle is shut and the
    // engine is not dragged by the wheels, dashpot opening otherwise.
    bool idleControl = running && pedal < 1 && (gear == 0 || clutch < 0.5f);
    if (idleControl) {
        float err = idleTarget() - rpm;
        idleIntegral = clampf(idleIntegral + err * dt * 0.0006f, -0.4f, 0.6f);
        idleValve = clampf(0.42f + err * 0.0009f + idleIntegral, 0.05f, 1.0f);
    } else {
        idleValve = approach(idleValve, 0.45f, dt, 0.5f);
    }

    // Manifold fills toward its mass-balance pressure with a time constant
    // set by how fast the engine pumps it out.
    float effectiveRpm = phase_ == Phase::Cranking ? CRANK_RPM : rpm;
    float target = phase_ == Phase::Off ? baro : steadyMap(throttle, idleValve, effectiveRpm);
    mapKpa = approach(mapKpa, target, dt, 0.03f * 3000.0f / fmaxf(effectiveRpm, 800.0f));

    // Engine torque, with the ECU's cuts from the previous step.
    float torque = 0;
    if (running) {
        torque = fuelCut || sparkCut ? -friction(rpm) : engineTorque(rpm, mapKpa, baro);
        if (status.status2 & bits::SOFT_LIMIT) torque *= 0.6f;
    }

    const float resist = 0.5f * AIR_DENSITY * CDA_M2 * speed * speed + (speed > 0.1f ? ROLLING * MASS_KG * G : 0);
    float drive = 0;  // N at the wheels
    float k = gear ? GEAR_RATIOS[gear - 1] * FINAL_DRIVE * DRIVELINE_EFFICIENCY / WHEEL_RADIUS_M : 0;

    if (shiftTimer > 0) {
        shiftTimer -= dt;
        // Clutch in: the engine falls freely toward the next gear's speed.
        rpm = fmaxf(rpm + (torque - 10) / ENGINE_INERTIA * dt * 60 / (2 * PI_F), 900);
        if (shiftTimer <= 0) gear = shiftTarget;
    } else if (phase_ == Phase::Cranking) {
        rpm = approach(rpm, CRANK_RPM + noise(20), dt, 0.15f);
    } else if (phase_ == Phase::Off) {
        rpm = approach(rpm, 0, dt, 0.25f);
        if (rpm < 20) rpm = 0;
    } else if (gear == 0 || clutch <= 0) {
        float load = 6.0f;  // alternator, water and oil pumps, power steering
        rpm += (torque - load) / ENGINE_INERTIA * dt * 60 / (2 * PI_F);
    } else {
        float wheel = wheelRpm(gear);
        if (clutch >= 1 && fabsf(rpm - wheel) < 150) {
            rpm = wheel;  // locked: engine speed follows the wheels
            drive = torque * k;
        } else {
            // Slipping clutch transfers torque toward the slower side.
            float capacity = clutch * 260.0f;
            float transfer = rpm > wheel ? capacity : -capacity;
            rpm += (torque - transfer) / ENGINE_INERTIA * dt * 60 / (2 * PI_F);
            drive = transfer * k;
        }
    }

    // Limiters and launch control act on the ECU side (stepEcu); keep the
    // physical speed sane in between.
    if (rpm < 0) rpm = 0;
    if (rpm > HARD_LIMIT_RPM + 300) rpm = HARD_LIMIT_RPM + 300;

    drive = clampf(drive, -TRACTION_G * MASS_KG * G * 2, TRACTION_G * MASS_KG * G);
    float accel = (drive - resist - brake * MASS_KG) / MASS_KG;
    if (speed <= 0 && accel < 0) accel = 0;
    speed = fmaxf(0, speed + accel * dt);

    // Lap distance, timing and IMU-like telemetry.
    const bool timing = phase_ == Phase::Launch || phase_ == Phase::OutLap || phase_ == Phase::HotLap ||
                        phase_ == Phase::CoolLap;
    if (timing) {
        tel.lapTimeS += dt;
        distance += speed * dt;
        if (distance >= track::length()) {
            distance -= track::length();
            tel.lap++;
            tel.lastLapS = tel.lapTimeS;
            if (phase_ == Phase::HotLap && (tel.bestLapS == 0 || tel.lapTimeS < tel.bestLapS))
                tel.bestLapS = tel.lapTimeS;
            startLap();
            if (phase_ == Phase::OutLap) {
                hotLapsDone = 0;
                setPhase(Phase::HotLap);
            } else if (phase_ == Phase::HotLap && ++hotLapsDone >= settings_.hotLaps) {
                setPhase(Phase::CoolLap);
            }
        }
    }
    tel.distanceM = distance;
    tel.speedKmh = speed * 3.6f;
    tel.gear = clutch > 0 ? gear : 0;
    tel.longG = accel / G;
    tel.latG = speed * speed * track::curvature(distance) / G * (timing ? 1 : 0);
    tel.brake = clampf(brake / (BRAKE_G * G), 0, 1);
    tel.throttle = pedal;
    tel.oilC = oil;
    tel.fanOn = fan;
}

void EngineSimulator::stepThermal(float dt) {
    const bool running = phase_ != Phase::Off && phase_ != Phase::Cranking;
    const float amb = settings_.ambientC;
#if SIM_DETAILED
    // Coolant: combustion heat in, radiator out (thermostat × airflow), slow
    // losses from the block.
    float power = running ? fmaxf(0, engineTorque(rpm, mapKpa, settings_.baroKpa)) * rpm * 2 * PI_F / 60000.0f : 0;
    float heatIn = running ? 2.5f + power * 1.0f : 0;  // kW
    float thermostat = clampf((coolant - 82.0f) / 10.0f, 0.03f, 1.0f);
    float airflow = 0.3f + speed * 0.38f + (fan ? 3.5f : 0);
    float heatOut = thermostat * airflow * (coolant - amb) * 0.055f + 0.01f * (coolant - amb);
    coolant += (heatIn - heatOut) / 35.0f * dt;  // 35 kJ/K
    if (coolant > 96) fan = true;
    if (coolant < 91) fan = false;
    float intakeTarget = amb + 22.0f * expf(-speed / 12.0f) * (running ? 1 : 0.5f) + 0.06f * (coolant - amb);
    intake = approach(intake, intakeTarget, dt, 20.0f);
    oil = approach(oil, coolant + (running ? 8.0f * power / 80.0f : -3.0f), dt, 120.0f);
#else
    // Compact model: warm up toward a fixed running temperature.
    coolant = approach(coolant, running ? 88.0f : amb, dt, running ? 240.0f : 1800.0f);
    fan = false;
    intake = approach(intake, amb + (speed < 5 ? 15.0f : 4.0f), dt, 30.0f);
    oil = approach(oil, coolant + 4, dt, 120.0f);
#endif
}

void EngineSimulator::stepEcu(float dt) {
    const float baro = settings_.baroKpa;
    const bool cranking = phase_ == Phase::Cranking;
    const bool running = phase_ != Phase::Off && !cranking && rpm > 300;
    if (running) runningS += dt;

    EngineStatus& s = status;
    s.secl = (uint8_t)(((uptimeMs - seclBaseMs) / 1000) & 0xFF);

    // ---- Sensors ----
    float mapS = mapKpa + noise(0.4f);
    float rpmS = (running || cranking) ? rpm + noise(running ? 6 : 15) : 0;
    // A calibrated TPS reads a steady 0 at the stop; noise only once it is open.
    float tpsS = throttle < 1.0f ? 0.0f : clampf(throttle + noise(0.3f), 0, 100);
    s.setMapKpa(mapS);
    s.setRpm(rpmS);
    s.setTpsPercent(tpsS);
    s.tpsADC = EngineStatus::clampByte(16 + tpsS * 2.14f);  // calibrated 16 … 230
    s.setCoolantC(coolant);
    s.setIntakeC(intake);
    s.baro = EngineStatus::clampByte(baro);
    s.setLoads(mapS);
    s.fuelTemp = 40;  // no flex sensor fitted: reads 0 °C like the bench ECU

    // Rates. Speeduino ignores TPS changes below ~15 %/s since 202201.
    if (dt > 0) {
        float rawTpsDot = (tpsS - prevTps) / dt;
        tpsDot = approach(tpsDot, fabsf(rawTpsDot) < 15 ? 0 : rawTpsDot, dt, 0.03f);
        mapDot = approach(mapDot, (mapS - prevMap) / dt, dt, 0.05f);
        rpmDot = approach(rpmDot, (rpmS - prevRpm) / dt, dt, 0.1f);
    }
    prevTps = tpsS;
    prevMap = mapS;
    prevRpm = rpmS;
    s.setTpsDot(tpsDot);
    s.setMapDot(mapDot);
    s.setRpmDot(rpmDot);

    // ---- Electrical ----
    float volts = !running && !cranking ? 12.5f : cranking ? 10.4f : (rpm < 1200 ? 13.6f : 14.2f) - (fan ? 0.2f : 0);
    s.setBattery(volts + noise(0.05f));
    // CORE4 bench: ~3900 loops/s stopped and lightly polled; ignition and
    // injection interrupts take their share with rpm.
    s.setLoops((uint16_t)(3900 - rpmS * 0.12f + noise(20)));
    s.setFreeRam(445);

    // ---- Protection and cuts ----
    bool launchHold = phase_ == Phase::Launch && clutch <= 0 && settings_.launchControl;
    bool hardLimit = running && (rpm > HARD_LIMIT_RPM || (launchHold && rpm > LAUNCH_RPM + 150));
    bool softLimit = running && !hardLimit && (rpm > SOFT_LIMIT_RPM || (launchHold && rpm > LAUNCH_RPM));
    bool closed = tpsS < 1.0f;
    if (running && closed && rpm > 1500 && coolant > 60 && gear > 0 && clutch >= 1) {
        dfcoTimer += dt;
    } else if (!closed || rpm < 1300) {
        dfcoTimer = 0;
    }
    bool dfco = dfcoTimer > 0.3f;
    // Rev limiter and launch control cut spark (Speeduino default); DFCO cuts fuel.
    sparkCut = hardLimit;
    fuelCut = dfco;

    // ---- Fuel ----
    float wue = running ? wueFor(coolant) : 0;
    float ase = 0;
    if (running && runningS < 30) ase = 100 + 25 * (1 - runningS / 30);
    else if (running) ase = 100;
    float iatCorr = running ? clampf(100.0f * 303.0f / (273.0f + intake), 85, 115) : 0;
    float baroCorr = running ? baro / 101.3f * 100 : 0;
    float batCorr = running ? clampf(100 + (14.0f - volts) * 8, 80, 140) : 0;

    // Acceleration enrichment on TPSdot, held for aeTime (200 ms).
    if (running && tpsDot > 40) {
        aeAmount = fmaxf(aeAmount, 100 + fminf(35, tpsDot * 0.02f));
        aeTimer = 0.15f;
    }
    if (aeTimer > 0) aeTimer -= dt; else aeAmount = approach(aeAmount, 100, dt, 0.1f);

    // Fuel film on the port walls: a rising MAP takes fuel into the film
    // (lean) for ~0.25 s, a falling one releases it (rich).
    if (dt > 0) {
        film += (mapS - prevFilmMap) / baro * 0.4f;
        film = approach(clampf(film, -0.2f, 0.4f), 0, dt, 0.25f);
    }
    prevFilmMap = mapS;

    float lambdaTarget = lambdaTargetFor(rpm, mapS, coolant);
    float veTable = trueVe(rpm, mapS) * 100 * (1 + 0.035f * sinf(rpm / 850.0f) + 0.02f * cosf(mapS / 9.0f));

    // EGO closed loop (simple algorithm): ±1 % every 100 ms toward target.
    bool closedLoop = SIM_DETAILED && running && runningS > 20 && coolant > 70 && rpm > 1000 &&
                      rpm < 5500 && tpsS < 70 && !fuelCut;
    if (closedLoop) {
        egoTimer += dt;
        if (egoTimer >= 0.1f) {
            egoTimer = 0;
            if (lambdaSensed > lambdaTarget * 1.01f) ego += 1;
            else if (lambdaSensed < lambdaTarget * 0.99f) ego -= 1;
            ego = clampf(ego, 85, 115);
        }
    } else {
        ego = 100;  // open loop: correctionsAFRClosedLoop() returns 100
    }

    float gammaE = running ? wue / 100 * ase / 100 * iatCorr / 100 * baroCorr / 100 * ego / 100 * aeAmount / 100 * 100 : 0;
    float pw = 0;
    if (running && !fuelCut) {
        pw = REQ_FUEL_US * veTable / 100 * mapS / 100 * gammaE / 100 + INJ_OPEN_US * batCorr / 100;
    } else if (cranking) {
        pw = 4800;  // cranking pulse
    }
    s.setPulseWidthUs(pw);
    s.ve1 = s.ve = EngineStatus::clampByte(running || cranking ? veTable : rpmVe(0) * 100 * 0.97f);
    s.ve2 = 0;
    s.wueCorrection = EngineStatus::clampByte(wue);
    s.aseValue = EngineStatus::clampByte(ase);
    s.iatCorrection = EngineStatus::clampByte(iatCorr);
    s.baroCorrection = EngineStatus::clampByte(baroCorr);
    s.batCorrection = EngineStatus::clampByte(batCorr);
    s.egoCorrection = EngineStatus::clampByte(ego);
    s.aeAmount = EngineStatus::clampByte(aeAmount / 2);
    s.setCorrections(gammaE);
    s.setAfrTarget(running ? lambdaTarget * STOICH_AFR : 0);

    // Exhaust lambda: what the mixture really is, then the wideband's
    // transport delay and response. Wall wetting leans tip-ins, AE covers it.
    if (running) {
        float trueLambda;
        if (fuelCut) {
            trueLambda = 1.65f;
        } else if (sparkCut) {
            trueLambda = 1.35f;  // unburnt charge: the wideband sees oxygen
        } else {
            float wall = 1 + film;
            trueLambda = lambdaTarget * (trueVe(rpm, mapS) * 100 / veTable) * (100 / ego) * wall / (aeAmount / 100);
        }
        trueLambda = clampf(trueLambda, 0.6f, 1.7f);
        lambdaDelay[3] = lambdaDelay[2];
        lambdaDelay[2] = lambdaDelay[1];
        lambdaDelay[1] = lambdaDelay[0];
        lambdaDelay[0] = trueLambda;
        lambdaSensed = approach(lambdaSensed, lambdaDelay[3], dt, 0.08f);
        s.setAfr(lambdaSensed * STOICH_AFR + noise(0.05f));
    } else {
        lambdaSensed = 1;
        s.o2 = 0;  // cold wideband
    }
    s.o2_2 = 0;

    // ---- Ignition ----
    float adv = cranking ? 10 : advanceFor(rpm, mapS);
    bool idling = running && closed && (gear == 0 || clutch < 0.5f);
    if (idling) adv = 12 + clampf((idleTarget() - rpm) * 0.01f, -4, 6);
    if (softLimit) adv -= 8;
    if (running && coolant < 50) adv += 2;
    s.advance = (int8_t)clampf(running || cranking ? adv : 12, -10, 50);
    s.advance1 = s.advance;
    s.advance2 = 0;
    float dwell = !running && !cranking ? 0 : cranking ? 4500 : 3200 * (1 + 0.08f * (14 - volts));
    s.setDwellUs(dwell, dwell > 0 ? dwell + noise(15) : 0);

    // ---- Auxiliaries ----
    float oilVisc = clampf(1 + (90 - oil) * 0.006f, 0.9f, 1.5f);
    s.oilPressure = EngineStatus::clampByte(running ? fminf(75, (12 + rpm * 0.0085f) * oilVisc) + noise(0.5f) : 0);
    bool pump = running || cranking || (phase_ == Phase::Off && phaseS < 2);
    s.fuelPressure = EngineStatus::clampByte(pump ? 43 - (baro - mapS) * 0.145f + noise(0.3f) : 0);
    s.clIdleTarget = EngineStatus::clampByte(idleTarget() / 10);
    s.idleLoad = EngineStatus::clampByte(running ? idleValve * 100 : 0);
    s.fanDuty = fan ? 200 : 0;
    s.setVss(speed * 3.6f);
    s.gear = tel.gear;

    // ---- Status bits ----
    uint8_t engine = 0;
    if (running) engine |= bits::RUNNING;
    if (cranking) engine |= bits::CRANKING;
    if (running && runningS < 30) engine |= bits::ASE;
    if (running && wue > 100) engine |= bits::WARMUP;
    if (aeTimer > 0) engine |= bits::TPS_ACCEL;
    if (running && tpsDot < -40) engine |= bits::TPS_DECEL;
    s.engine = engine;

    uint8_t s1 = 0;
    if (dfco) s1 |= bits::DFCO;
    if (pw > 0) {
#if SIM_DETAILED
        float cycleUs = rpm > 100 ? 120e6f / rpm / SQUIRTS_PER_CYCLE : 1e9f;
        float duty = clampf(pw / cycleUs, 0, 1);
        for (uint8_t i = 0; i < 4; i++)
            if (random->random(0, 1000) < duty * 1000) s1 |= (uint8_t)(bits::INJ1 << i);
#else
        s1 |= 0x0F;
#endif
    }
    s.status1 = s1;

    uint8_t s2 = 0;
    if ((running || cranking) && (running || phaseS > 0.3f)) s2 |= bits::SYNC;
    if (idling) s2 |= bits::IDLE_CONTROL;
    if (softLimit) s2 |= bits::SOFT_LIMIT;
    if (hardLimit) s2 |= bits::HARD_LIMIT;
    if (launchHold) s2 |= bits::LAUNCH_SOFT;
    s.status2 = s2;
    s.status3 = (uint8_t)(SQUIRTS_PER_CYCLE << 5);
    s.status4 = (uint8_t)(bits::ALLOW_LEGACY_COMMS | (fan ? bits::FAN : 0));
    s.status5 = tsOnSecondaryAllowed ? 0x20 : 0;  // BIT_STATUS5_ALLOW_TS_ON_SECONDARY_COMMS
    s.engineProtectStatus = 0;
    s.errors = 0;
}
