#pragma once
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "esp_heap_caps.h"
#include "config.h"

// Sample storage is a single contiguous PSRAM arena.
// Individual samples are packed at 4-byte aligned offsets; there is no
// per-sample 4 KB rounding anymore.
#define HASH_SIZE              512u   // keep power of 2
#define SAMPLE_POOL_ALIGNMENT  4u
#define FREE_RANGE_MAX         (HASH_SIZE + 1u)

static_assert((SAMPLE_POOL_ALIGNMENT & (SAMPLE_POOL_ALIGNMENT - 1u)) == 0u,
              "SAMPLE_POOL_ALIGNMENT must be a power of two");

// ===== TYPES =====

struct SampleHandle {
    const int16_t* data;
    uint32_t length;

    uint32_t loopStart;
    uint32_t loopEnd;

    uint32_t sampleRate;
    int8_t   rootKey;
    int8_t   pitchCorrection;  // SF2 shdr pitch correction, cents
};

struct SampleEntry {
    const int16_t* data;
    uint32_t length;

    uint32_t loopStart;
    uint32_t loopEnd;

    uint32_t sampleRate;
    int8_t   rootKey;
    int8_t   pitchCorrection;

    uint32_t key;
    uint32_t offset;

    uint16_t refCount;
    uint8_t  used;
};

struct SampleFreeRange {
    uint32_t offset;
    uint32_t size;
};

// ===== POOL =====

class SamplePool {
public:
    inline uint32_t capacityBytes() const { return totalBytes; }
    static constexpr uint32_t maxEntries() { return HASH_SIZE; }

    bool init(uint32_t reserveBytes) {
        deinit();

        const uint32_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);

        // rawPool is deliberately over-allocated by 64 bytes so the exposed
        // arena can remain 64-byte aligned. Samples inside it are 4-byte aligned.
        if (largest <= reserveBytes + 64u + SAMPLE_POOL_ALIGNMENT)
            return false;

        uint32_t target = largest - reserveBytes - 64u;
        target &= ~(SAMPLE_POOL_ALIGNMENT - 1u);
        if (target < SAMPLE_POOL_ALIGNMENT)
            return false;

        totalBytes = target;

        rawPool = (uint8_t*)heap_caps_malloc(
            totalBytes + 64u,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
        );
        if (!rawPool) {
            totalBytes = 0;
            return false;
        }

        uintptr_t aligned = ((uintptr_t)rawPool + 63u) & ~(uintptr_t)63u;
        pool = (uint8_t*)aligned;

        memset(hashTable, 0, sizeof(hashTable));
        resetFreeList();

        ESP_LOGI("POOL",
                 "INIT: total=%u KB contiguous, sampleAlign=%u bytes",
                 (unsigned)(totalBytes >> 10),
                 (unsigned)SAMPLE_POOL_ALIGNMENT);
        return true;
    }

    // Reset allocations without releasing the arena itself.
    // Used only when no live voice points into the pool.
    inline void reset() {
        memset(hashTable, 0, sizeof(hashTable));
        resetFreeList();
    }

    // Release the complete arena back to PSRAM.
    inline void deinit() {
        if (rawPool) {
            heap_caps_free(rawPool);
            rawPool = nullptr;
        }

        pool = nullptr;
        totalBytes = 0;
        freeRangeCount = 0;
        memset(hashTable, 0, sizeof(hashTable));
    }

    // Lookup without changing ownership.
    inline SampleHandle* get(uint32_t key) {
        SampleEntry* e = find(key);
        return e ? (SampleHandle*)e : nullptr;
    }

    inline SampleHandle* acquire(uint32_t key) {
        SampleEntry* e = find(key);

        if (!e) return nullptr;
        if (e->refCount == UINT16_MAX) {
            ESP_LOGE("POOL", "refCount overflow key=%u", key);
            return nullptr;
        }
        e->refCount++;
        return (SampleHandle*)e;
    }

    inline void release(uint32_t key) {
        SampleEntry* e = find(key);
        if (!e) return;

        if (e->refCount == 0) {
            ESP_LOGE("POOL", "release underflow key=%u", key);
            return;
        }

        if (--e->refCount == 0) {
            const uint32_t sizeBytes = alignedSize(e->length << 1);
            freeRange(e->offset, sizeBytes);
            e->data = nullptr;
            e->used = 2;
        }
    }

    // Remove an uncommitted entry, e.g. after an SD read failure.
    inline void discard(uint32_t key) {
        SampleEntry* e = find(key);
        if (!e) return;

        const uint32_t sizeBytes = alignedSize(e->length << 1);
        freeRange(e->offset, sizeBytes);

        e->data = nullptr;
        e->refCount = 0;
        e->used = 2;
    }

    SampleHandle* insert(
        uint32_t key,
        const int16_t* src,
        uint32_t lengthSamples,
        uint32_t loopStart,
        uint32_t loopEnd,
        uint32_t sampleRate,
        int8_t   rootKey,
        int8_t   pitchCorrection = 0
    ) {
        SampleEntry* existing = find(key);
        if (existing) return (SampleHandle*)existing;
        if (!src || lengthSamples == 0) return nullptr;

        const uint32_t payloadBytes = lengthSamples << 1;
        const uint32_t allocBytes = alignedSize(payloadBytes);

        const int32_t offset = allocRange(allocBytes);
        if (offset < 0) return nullptr;

        int16_t* dst = (int16_t*)(pool + (uint32_t)offset);
        memcpy(dst, src, payloadBytes);

        SampleEntry* e = insertEntry(key);
        if (!e) {
            freeRange((uint32_t)offset, allocBytes);
            return nullptr;
        }

        fillEntry(*e, key, (uint32_t)offset, lengthSamples,
                  loopStart, loopEnd, sampleRate, rootKey, pitchCorrection);
        return (SampleHandle*)e;
    }

    inline SampleHandle* insertEmpty(
        uint32_t key,
        uint32_t lengthSamples,
        uint32_t loopStart,
        uint32_t loopEnd,
        uint32_t sampleRate,
        int8_t   rootKey,
        int8_t   pitchCorrection = 0
    ) {
        SampleEntry* existing = find(key);
        if (existing) return (SampleHandle*)existing;
        if (lengthSamples == 0) return nullptr;

        const uint32_t allocBytes = alignedSize(lengthSamples << 1);
        const int32_t offset = allocRange(allocBytes);
        if (offset < 0) return nullptr;

        SampleEntry* e = insertEntry(key);
        if (!e) {
            freeRange((uint32_t)offset, allocBytes);
            return nullptr;
        }

        fillEntry(*e, key, (uint32_t)offset, lengthSamples,
                  loopStart, loopEnd, sampleRate, rootKey, pitchCorrection);
        return (SampleHandle*)e;
    }

    uint8_t* pool = nullptr;

private:
    uint8_t* rawPool = nullptr;
    uint32_t totalBytes = 0;

    SampleEntry hashTable[HASH_SIZE];

    // Sorted free ranges. With at most HASH_SIZE live allocations, HASH_SIZE+1
    // descriptors is more than sufficient for every possible set of holes.
    SampleFreeRange freeRanges[FREE_RANGE_MAX];
    uint16_t freeRangeCount = 0;

    static inline uint32_t alignedSize(uint32_t sizeBytes) {
        return (sizeBytes + (SAMPLE_POOL_ALIGNMENT - 1u))
             & ~(SAMPLE_POOL_ALIGNMENT - 1u);
    }

    inline void resetFreeList() {
        freeRangeCount = 0;
        if (pool && totalBytes) {
            freeRanges[0].offset = 0;
            freeRanges[0].size = totalBytes;
            freeRangeCount = 1;
        }
    }

    inline void fillEntry(
        SampleEntry& e,
        uint32_t key,
        uint32_t offset,
        uint32_t lengthSamples,
        uint32_t loopStart,
        uint32_t loopEnd,
        uint32_t sampleRate,
        int8_t rootKey,
        int8_t pitchCorrection
    ) {
        e.data       = (int16_t*)(pool + offset);
        e.length     = lengthSamples;
        e.loopStart  = loopStart;
        e.loopEnd    = loopEnd;
        e.sampleRate = sampleRate;
        e.rootKey    = rootKey;
        e.pitchCorrection = pitchCorrection;
        e.key        = key;
        e.offset     = offset;
        e.refCount   = 0;
        e.used       = 1;
    }

    // First-fit allocation from the sorted free list.
    // Runs only on the control/loading side, never in the audio render loop.
    inline int32_t allocRange(uint32_t sizeBytes) {
        if (!pool || sizeBytes == 0) return -1;

        for (uint16_t i = 0; i < freeRangeCount; ++i) {
            SampleFreeRange& r = freeRanges[i];
            if (r.size < sizeBytes) continue;

            const uint32_t offset = r.offset;
            r.offset += sizeBytes;
            r.size -= sizeBytes;

            if (r.size == 0) removeFreeRange(i);
            return (int32_t)offset;
        }

        return -1;
    }

    inline void removeFreeRange(uint16_t index) {
        if (index >= freeRangeCount) return;

        for (uint16_t i = index + 1; i < freeRangeCount; ++i)
            freeRanges[i - 1] = freeRanges[i];

        --freeRangeCount;
    }

    // Insert a released range in address order and coalesce adjacent ranges.
    inline void freeRange(uint32_t offset, uint32_t sizeBytes) {
        if (!pool || sizeBytes == 0) return;

        if (offset >= totalBytes || sizeBytes > totalBytes - offset) {
            ESP_LOGE("POOL", "free range invalid off=%u size=%u total=%u",
                     (unsigned)offset, (unsigned)sizeBytes, (unsigned)totalBytes);
            return;
        }

        uint16_t pos = 0;
        while (pos < freeRangeCount && freeRanges[pos].offset < offset)
            ++pos;

        // Merge with previous range if directly adjacent.
        if (pos > 0) {
            SampleFreeRange& prev = freeRanges[pos - 1];
            if (prev.offset + prev.size == offset) {
                prev.size += sizeBytes;

                // The expanded previous range may now touch the next range.
                if (pos < freeRangeCount &&
                    prev.offset + prev.size == freeRanges[pos].offset) {
                    prev.size += freeRanges[pos].size;
                    removeFreeRange(pos);
                }
                return;
            }
        }

        // Merge into next range if directly adjacent.
        if (pos < freeRangeCount &&
            offset + sizeBytes == freeRanges[pos].offset) {
            freeRanges[pos].offset = offset;
            freeRanges[pos].size += sizeBytes;
            return;
        }

        if (freeRangeCount >= FREE_RANGE_MAX) {
            // This should be unreachable: at most HASH_SIZE allocations can
            // exist, therefore the number of free holes cannot exceed +1.
            ESP_LOGE("POOL", "free range table overflow");
            return;
        }

        for (uint16_t i = freeRangeCount; i > pos; --i)
            freeRanges[i] = freeRanges[i - 1];

        freeRanges[pos].offset = offset;
        freeRanges[pos].size = sizeBytes;
        ++freeRangeCount;
    }

    // ===== HASH =====

    inline uint32_t hash(uint32_t k) const {
        return (k * 2654435761u) & (HASH_SIZE - 1);
    }

    inline SampleEntry* find(uint32_t k) {
        uint32_t idx = hash(k);

        for (uint32_t i = 0; i < HASH_SIZE; i++) {
            SampleEntry& e = hashTable[idx];

            if (e.used == 0) return nullptr;
            if (e.used == 1 && e.key == k) return &e;

            idx = (idx + 1) & (HASH_SIZE - 1);
        }
        return nullptr;
    }

    inline SampleEntry* insertEntry(uint32_t key) {
        uint32_t idx = hash(key);
        SampleEntry* firstDeleted = nullptr;

        for (uint32_t i = 0; i < HASH_SIZE; i++) {
            SampleEntry& e = hashTable[idx];

            if (e.used == 1) {
                if (e.key == key) return &e;
            } else if (e.used == 2) {
                if (!firstDeleted) firstDeleted = &e;
            } else {
                return firstDeleted ? firstDeleted : &e;
            }

            idx = (idx + 1) & (HASH_SIZE - 1);
        }

        return firstDeleted;
    }
};

