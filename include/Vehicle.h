/**
 * @file Vehicle.h
 * @brief The simulated car: a 1.8 L 16-valve front-wheel-drive hatchback on
 *        semi-slicks, the class of car G86 runs at Biķernieki.
 *
 * Numbers are typical for the type, chosen so laps, speeds and engine values
 * land where such a car really is (about 2:00 at Biķernieki, 150 km/h on the
 * longest straight, 6800 rpm shifts).
 */
#pragma once

#include <stdint.h>
#include "Progmem.h"

namespace vehicle {

constexpr float MASS_KG = 1080.0f;          // car + driver
constexpr float CDA_M2 = 0.64f;             // drag coefficient × frontal area
constexpr float ROLLING = 0.013f;
constexpr float WHEEL_RADIUS_M = 0.287f;    // 185/60 R14
constexpr float DRIVELINE_EFFICIENCY = 0.88f;
constexpr float TRACTION_G = 0.62f;         // front-drive launch/exit limit
constexpr float BRAKE_G = 0.95f;
constexpr uint8_t GEAR_COUNT = 5;
constexpr float GEAR_RATIOS[GEAR_COUNT] = {3.45f, 2.12f, 1.44f, 1.13f, 0.91f};
constexpr float FINAL_DRIVE = 3.67f;

constexpr float ENGINE_INERTIA = 0.18f;     // kg·m², flywheel included
constexpr uint16_t SHIFT_RPM = 6800;
constexpr uint16_t SOFT_LIMIT_RPM = 7000;
constexpr uint16_t HARD_LIMIT_RPM = 7200;
constexpr uint16_t LAUNCH_RPM = 4200;
constexpr uint16_t IDLE_RPM_HOT = 850;
constexpr uint16_t IDLE_RPM_COLD = 1200;
constexpr uint16_t CRANK_RPM = 240;

// Wide-open-throttle torque, N·m, every 500 rpm from 0.
static const uint8_t TORQUE_WOT_NM[] PROGMEM = {
    0, 45, 90, 128, 142, 150, 157, 162, 166, 168, 166, 161, 153, 143, 130, 112,
};
constexpr uint8_t TORQUE_POINTS = sizeof(TORQUE_WOT_NM);

// Speeduino-side calibration of this engine.
constexpr float REQ_FUEL_US = 6000.0f;      // per squirt, 2 squirts per cycle
constexpr float INJ_OPEN_US = 1000.0f;      // dead time at 14 V
constexpr uint8_t SQUIRTS_PER_CYCLE = 2;
constexpr float STOICH_AFR = 14.7f;

}  // namespace vehicle
