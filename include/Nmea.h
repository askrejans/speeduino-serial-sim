/**
 * @file Nmea.h
 * @brief The simulated car as a GPS receiver: NMEA 0183 $GPRMC and $GPGGA
 *        sentences from its place on the lap, for gps-to-mqtt and other
 *        NMEA clients (desktop simulator, --gps-port).
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace nmea {

struct Fix {
    double lat, lon;     ///< degrees
    float speedKmh;
    float headingDeg;    ///< true course, clockwise from north
    float altitudeM;
    uint32_t utcSeconds; ///< seconds since midnight UTC
    uint16_t utcMs;
    uint16_t day, month, year;  ///< UTC date (year: four digits)
    uint8_t satellites;
    float hdop;
};

/// "$GPRMC,…*hh\r\n$GPGGA,…*hh\r\n" into out; returns the length.
size_t sentences(const Fix& f, char* out, size_t n);
/// The XOR checksum of the characters between '$' and '*'.
uint8_t checksum(const char* body);

/// Track metres east/north of an origin to latitude/longitude.
void toLatLon(double lat0, double lon0, float east, float north, double& lat, double& lon);

}  // namespace nmea
