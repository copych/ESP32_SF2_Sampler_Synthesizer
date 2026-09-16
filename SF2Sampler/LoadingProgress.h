#pragma once
#include <Arduino.h>

// Cross-task loading progress state. Producers never touch U8g2.
// The optional pump is installed only by GUITask while it is synchronously
// blocked inside the SF2 browser load callback.
namespace LoadingProgress {
using PumpFn = void (*)(void*, uint8_t);

inline volatile bool active = false;
inline volatile uint8_t value = 0;
inline volatile uint32_t revision = 0;
inline uint64_t doneBytes = 0;
inline uint64_t totalBytes = 0;
inline PumpFn pump = nullptr;
inline void* pumpCtx = nullptr;

inline uint8_t calcValue() {
    if (!totalBytes) return 0;
    uint64_t v = (doneBytes * 255ULL) / totalBytes;
    return (uint8_t)(v > 255 ? 255 : v);
}

inline void publish(bool force = false) {
    uint8_t v = calcValue();
    uint8_t old = __atomic_load_n(&value, __ATOMIC_RELAXED);
    if (!force && (v >> 2) == (old >> 2)) return; // ~64 visible steps max
    __atomic_store_n(&value, v, __ATOMIC_RELEASE);
    __atomic_add_fetch(&revision, 1u, __ATOMIC_RELEASE);
    PumpFn fn = pump;
    if (fn) fn(pumpCtx, v);
}

inline void begin(uint64_t total) {
    doneBytes = 0;
    totalBytes = total;
    __atomic_store_n(&value, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&active, true, __ATOMIC_RELEASE);
    __atomic_add_fetch(&revision, 1u, __ATOMIC_RELEASE);
    if (pump) pump(pumpCtx, 0);
}

inline void add(uint64_t bytes) {
    doneBytes += bytes;
    if (doneBytes > totalBytes) doneBytes = totalBytes;
    publish();
}

inline void finish() {
    if (__atomic_load_n(&active, __ATOMIC_ACQUIRE)) {
        doneBytes = totalBytes;
        publish(true);
    }
    __atomic_store_n(&active, false, __ATOMIC_RELEASE);
    // Completion is itself a display-state change. A fast load may begin and
    // finish entirely between two GUITask draw() calls, so the GUI must not
    // rely on having observed active=true.
    __atomic_add_fetch(&revision, 1u, __ATOMIC_RELEASE);
}

inline bool isActive() { return __atomic_load_n(&active, __ATOMIC_ACQUIRE); }
inline uint8_t get() { return __atomic_load_n(&value, __ATOMIC_ACQUIRE); }
inline uint32_t getRevision() { return __atomic_load_n(&revision, __ATOMIC_ACQUIRE); }
inline void setPump(PumpFn fn, void* ctx) { pump = fn; pumpCtx = ctx; }
inline void clearPump() { pump = nullptr; pumpCtx = nullptr; }
}
