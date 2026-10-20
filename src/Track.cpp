/**
 * @file Track.cpp
 * @brief Interpolated lookups into the generated track tables.
 */
#include "Config.h"
#include "Track.h"
#include "TrackData.h"
#include <math.h>

namespace {

constexpr float PI_F = 3.14159265f;
constexpr float KMH = 1.0f / 3.6f;

struct Node {
    uint16_t i;
    uint16_t next;
    float frac;
};

Node locate(float s) {
    float pos = track::wrap(s) / TRACK_STEP_M;
    uint16_t i = (uint16_t)pos;
    if (i >= TRACK_NODES) i = TRACK_NODES - 1;
    return {i, (uint16_t)((i + 1) % TRACK_NODES), pos - i};
}

float headingAt(uint16_t i) { return flashByte(trackHeading, i) * (2.0f * PI_F / 256.0f); }

float angleDelta(float from, float to) {
    float d = to - from;
    while (d > PI_F) d -= 2.0f * PI_F;
    while (d < -PI_F) d += 2.0f * PI_F;
    return d;
}

}  // namespace

namespace track {

const char* name() { return TRACK_NAME; }
float length() { return TRACK_LENGTH_M; }

float wrap(float s) {
    s = fmodf(s, TRACK_LENGTH_M);
    return s < 0 ? s + TRACK_LENGTH_M : s;
}

float envelope(float s) {
    Node n = locate(s);
    float a = flashByte(trackEnvelopeKmh, n.i), b = flashByte(trackEnvelopeKmh, n.next);
    return (a + (b - a) * n.frac) * KMH;
}

float heading(float s) {
    Node n = locate(s);
    float a = headingAt(n.i);
    float h = a + angleDelta(a, headingAt(n.next)) * n.frac;
    return h < 0 ? h + 2.0f * PI_F : h;
}

float curvature(float s) {
    // Over ±2 nodes: heading is quantised to 1.4°, so a single step is noisy.
    Node n = locate(s);
    uint16_t back = (uint16_t)((n.i + TRACK_NODES - 2) % TRACK_NODES);
    uint16_t ahead = (uint16_t)((n.i + 2) % TRACK_NODES);
    return angleDelta(headingAt(back), headingAt(ahead)) / (4.0f * TRACK_STEP_M);
}

#ifdef TRACK_OUTLINE
uint16_t outlinePoints() { return TRACK_OUTLINE_POINTS; }

void outlinePoint(uint16_t i, int16_t& x, int16_t& y) {
    x = flashInt16(trackOutline, (uint16_t)(2 * i));
    y = flashInt16(trackOutline, (uint16_t)(2 * i + 1));
}

void position(float s, float& x, float& y) {
    // The outline keeps every 4th node; its closing segment back to the start
    // line is shorter when the node count is not a multiple of 4.
    float node = wrap(s) / TRACK_STEP_M;
    uint16_t i = (uint16_t)(node / 4.0f);
    if (i >= TRACK_OUTLINE_POINTS) i = TRACK_OUTLINE_POINTS - 1;
    uint16_t j = (uint16_t)((i + 1) % TRACK_OUTLINE_POINTS);
    float span = (i == TRACK_OUTLINE_POINTS - 1) ? (float)(TRACK_NODES - 4 * i) : 4.0f;
    float f = (node - 4.0f * i) / span;
    int16_t ax, ay, bx, by;
    outlinePoint(i, ax, ay);
    outlinePoint(j, bx, by);
    x = ax + (bx - ax) * f;
    y = ay + (by - ay) * f;
}

double originLat() { return TRACK_ORIGIN_LAT; }
double originLon() { return TRACK_ORIGIN_LON; }
#endif

}  // namespace track
