/**
 * @file Progmem.h
 * @brief Read-only tables in flash on every target.
 *
 * AVR keeps tables in program memory (2 KB of RAM on an Uno); ESP32 and
 * ESP8266 map flash into the address space; the native build reads RAM.
 */
#pragma once

#include <stdint.h>

#if defined(ARDUINO_ARCH_AVR)
  #include <avr/pgmspace.h>
#elif defined(ARDUINO)
  #include <pgmspace.h>
#else
  #define PROGMEM
  #define pgm_read_byte(addr) (*(const uint8_t*)(addr))
  #define pgm_read_word(addr) (*(const uint16_t*)(addr))
#endif

inline uint8_t flashByte(const uint8_t* table, uint16_t i) { return pgm_read_byte(table + i); }
inline int16_t flashInt16(const int16_t* table, uint16_t i) {
    return (int16_t)pgm_read_word((const uint16_t*)(table + i));
}
