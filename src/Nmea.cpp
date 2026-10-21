/**
 * @file Nmea.cpp
 * @brief NMEA 0183 sentences for the simulated GPS.
 */
#include "Nmea.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace nmea {

namespace {

// ddmm.mmmmm and the hemisphere letter
void angle(double deg, bool latitude, char* out, size_t n, char& hemi) {
    hemi = latitude ? (deg < 0 ? 'S' : 'N') : (deg < 0 ? 'W' : 'E');
    deg = fabs(deg);
    int whole = (int)deg;
    double minutes = (deg - whole) * 60.0;
    if (minutes >= 59.999995) {  // rounding would print 60.00000
        whole += 1;
        minutes = 0;
    }
    snprintf(out, n, latitude ? "%02d%08.5f" : "%03d%08.5f", whole, minutes);
}

size_t append(char* out, size_t n, size_t at, const char* body) {
    int k = snprintf(out + at, at < n ? n - at : 0, "$%s*%02X\r\n", body, checksum(body));
    return k < 0 ? at : at + (size_t)k;
}

}  // namespace

uint8_t checksum(const char* body) {
    uint8_t c = 0;
    for (const char* p = body; *p && *p != '*'; ++p) c ^= (uint8_t)*p;
    return c;
}

void toLatLon(double lat0, double lon0, float east, float north, double& lat, double& lon) {
    const double mPerDegLat = 111320.0;
    lat = lat0 + north / mPerDegLat;
    lon = lon0 + east / (mPerDegLat * cos(lat0 * 3.14159265358979 / 180.0));
}

size_t sentences(const Fix& f, char* out, size_t n) {
    if (!n) return 0;
    out[0] = 0;
    char la[16], lo[16], hla, hlo;
    angle(f.lat, true, la, sizeof(la), hla);
    angle(f.lon, false, lo, sizeof(lo), hlo);
    unsigned h = f.utcSeconds / 3600 % 24, m = f.utcSeconds / 60 % 60, s = f.utcSeconds % 60;
    float course = fmodf(f.headingDeg + 360.0f, 360.0f);
    char body[128];
    snprintf(body, sizeof(body), "GPRMC,%02u%02u%02u.%02u,A,%s,%c,%s,%c,%.2f,%.1f,%02u%02u%02u,,,A", h, m, s,
             (unsigned)(f.utcMs / 10), la, hla, lo, hlo, f.speedKmh / 1.852f, course, (unsigned)f.day,
             (unsigned)f.month, (unsigned)(f.year % 100));
    size_t at = append(out, n, 0, body);
    snprintf(body, sizeof(body), "GPGGA,%02u%02u%02u.%02u,%s,%c,%s,%c,1,%02u,%.1f,%.1f,M,22.1,M,,", h, m, s,
             (unsigned)(f.utcMs / 10), la, hla, lo, hlo, (unsigned)f.satellites, f.hdop, f.altitudeM);
    at = append(out, n, at, body);
    return at < n ? at : n - 1;
}

}  // namespace nmea
