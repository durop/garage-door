// ============================================================
//  Arduino.h — Mock for native (desktop) unit testing
//  Provides stubs for Arduino functions used by the firmware.
// ============================================================
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdarg>

// ─── Arduino types ──────────────────────────────────────────
typedef uint8_t byte;

// ─── Pin modes / values ─────────────────────────────────────
#define INPUT        0x0
#define OUTPUT       0x1
#define INPUT_PULLUP 0x2

#define LOW  0x0
#define HIGH 0x1

// ─── Mock state (single translation-unit, accessed by tests) ─
#define MOCK_MAX_PINS 64

static uint32_t _mock_millis          = 0;
static int      _mock_pin_reads [MOCK_MAX_PINS] = {};
static uint8_t  _mock_pin_writes[MOCK_MAX_PINS] = {};
static uint8_t  _mock_pin_modes [MOCK_MAX_PINS] = {};

// ─── Test helpers ───────────────────────────────────────────
static inline void mock_reset() {
    _mock_millis = 0;
    memset(_mock_pin_reads,  0, sizeof(_mock_pin_reads));
    memset(_mock_pin_writes, 0, sizeof(_mock_pin_writes));
    memset(_mock_pin_modes,  0, sizeof(_mock_pin_modes));
}

static inline void mock_set_millis(uint32_t ms)           { _mock_millis = ms; }
static inline void mock_advance_millis(uint32_t ms)       { _mock_millis += ms; }
static inline void mock_set_pin_value(uint8_t pin, int v) { _mock_pin_reads[pin] = v; }
static inline int  mock_get_pin_write(uint8_t pin)        { return _mock_pin_writes[pin]; }
static inline int  mock_get_pin_mode(uint8_t pin)         { return _mock_pin_modes[pin]; }

// ─── Arduino API stubs ─────────────────────────────────────
static inline uint32_t millis()                            { return _mock_millis; }
static inline void     delay(unsigned long ms)             { _mock_millis += ms; }
static inline void     pinMode(uint8_t pin, uint8_t mode)  { _mock_pin_modes[pin] = mode; }
static inline void     digitalWrite(uint8_t pin, uint8_t v){ _mock_pin_writes[pin] = v; }
static inline int      digitalRead(uint8_t pin)            { return _mock_pin_reads[pin]; }

// ─── Serial mock (absorbs all debug output) ─────────────────
class HardwareSerial {
public:
    void   begin(unsigned long) {}
    size_t print(const char*)        { return 0; }
    size_t print(int)                { return 0; }
    size_t print(unsigned int)       { return 0; }
    size_t print(unsigned long)      { return 0; }
    size_t print(double)             { return 0; }
    size_t println(const char* = "") { return 0; }
    size_t println(int)              { return 0; }
    size_t println(unsigned int)     { return 0; }
    size_t println(unsigned long)    { return 0; }
    size_t printf(const char*, ...) __attribute__((format(printf, 2, 3))) { return 0; }
};

static HardwareSerial Serial;
