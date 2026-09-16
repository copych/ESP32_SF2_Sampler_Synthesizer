#pragma once

#define PARAM_SF2_FILENAME      0x0001
#define PARAM_SF2_FS_TYPE       0x0002  // 1 byte (FileSystemType enum)
#define PARAM_SAMPLE_LOAD_MODE  0x0003  // 1 byte (SampleLoadMode enum)

// Effects
#define PARAM_REVERB_TIME       0x0101
#define PARAM_REVERB_DAMP       0x0102
#define PARAM_DELAY_TIME        0x0201
#define PARAM_CHORUS_DEPTH      0x0301

// Per-channel (ch = 0..15)
#define PARAM_CHANNEL_BASE      0x1000
#define PARAM_CHANNEL(i)        (PARAM_CHANNEL_BASE + (i))



struct TLV {
    uint16_t id;
    uint16_t len;
    std::vector<uint8_t> data;
};

static void writeTLV(File& f, uint16_t id, const void* data, uint16_t len) {
    f.write((uint8_t*)&id, sizeof(id));
    f.write((uint8_t*)&len, sizeof(len));
    f.write((const uint8_t*)data, len);
}

static std::map<uint16_t, TLV> readTLV(File& f) {
    std::map<uint16_t, TLV> map;
    while (f.available() >= 4) {
        TLV t;
        f.read((uint8_t*)&t.id, 2);
        f.read((uint8_t*)&t.len, 2);
        t.data.resize(t.len);
        f.read(t.data.data(), t.len);
        map[t.id] = t;
    }
    return map;
}

