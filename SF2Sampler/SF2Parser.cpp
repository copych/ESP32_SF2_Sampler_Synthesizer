/*
 * ----------------------------------------------------------------------------
 * ESP32-S3 SF2 Synthesizer Firmware
 * 
 * Description:
 *   Real-time SF2 (SoundFont) compatible wavetable synthesizer with USB MIDI, I2S audio,
 *   multi-layer voice allocation, per-channel filters, reverb, chorus and delay.
 *   GM/GS/XG support is partly implemented
 * 
 * Hardware:
 *   - ESP32-S3 with PSRAM
 *   - I2S DAC output (44100Hz stereo, 16-bit PCM)
 *   - USB MIDI input
 *   - Optional SD card and/or LittleFS
 * 
 * Author: Evgeny Aslovskiy AKA Copych
 * License: MIT
 * Repository: https://github.com/copych/ESP32-S3_SF2_Sampler_Synthesizer
 * 
 * File: SF2Parser.cpp
 * Purpose: Implementation of SF2 file parser.
 * ----------------------------------------------------------------------------
 */

#include "SF2Parser.h"
#include "esp_log.h"
#include "operators.h"

static const char* TAG = "SF2Parser";

static float timecentsToSec(int tc) {
    if (tc <= -32768) return 0.0f;
    return powf(2.0f, tc * 8.3333333e-04f); // timecents → seconds ( 1 / 1200 )
}

static float centsToHz(int cents) {
    return 8.176f * powf(2.0f, cents * 8.3333333e-04f); // cents → Hz ( 1 / 1200 )
}

inline float dB_to_linear(float dB) {
    return exp2f(dB * 0.050025f);  // ≈ dB / 20 * log2(10)
}

//struct PHDR { char name[20]; uint16_t preset, bank, bagIndex; uint32_t dummy[5]; };
struct __attribute__((packed)) PHDR {
    char name[20];
    uint16_t preset;
    uint16_t bank;
    uint16_t bagIndex;
    uint32_t library;
    uint32_t genre;
    uint32_t morphology;
};
struct PBAG { uint16_t genIndex, modIndex; };
struct PGEN { uint16_t oper; int16_t amount; };
struct INST { char name[20]; uint16_t bagIndex; };
struct IBAG { uint16_t genIndex, modIndex; };
struct IGEN { uint16_t oper; int16_t amount; };

void decodeGeneratorAmount(Generator& gen, uint16_t raw) {
    auto op = static_cast<GeneratorOperator>(gen.oper);

    switch (op) {
        case GeneratorOperator::Instrument:
        case GeneratorOperator::SampleID:
        case GeneratorOperator::SampleModes:
        case GeneratorOperator::ExclusiveClass:
        case GeneratorOperator::OverridingRootKey:
            gen.amount.uAmount = raw;
            break;

        case GeneratorOperator::KeyRange:
        case GeneratorOperator::VelRange:
            gen.amount.range.lo = raw & 0xFF;
            gen.amount.range.hi = (raw >> 8) & 0xFF;
            break;

        default:
            gen.amount.sAmount = static_cast<int16_t>(raw);
            break;
    }
}


SF2Parser::SF2Parser(const char* path, fs::FS* fs) : filepath(path), filesystem(fs) {}

bool SF2Parser::parse() {
    clear();
    //file = LittleFS.open(filepath, "r");
    file = filesystem->open(filepath, "r");

    if (!file){ 
        ESP_LOGE(TAG, "Error: File not found");
        return false;
    }
    if (!parseHeaderChunks()) {
      ESP_LOGE(TAG, "Error: Invalid SF2 format");
      return false;
    } else {
      ESP_LOGI(TAG, "RIFF OK");
    }
    if (!parseSDTA()){
      ESP_LOGE(TAG, "Error: Invalid SF2 sample data");
      return false;
    } else {
      ESP_LOGI(TAG, "SDTA OK");
    }
    if (!parsePDTA()) {
        ESP_LOGE(TAG, "PDTA parse failed");
        return false;
    }  else {
      ESP_LOGI(TAG, "PDTA OK");
    }
    if (!loadSampleDataToMemory()) {
        ESP_LOGE(TAG, "Failed to load all sample data into memory, some samples may not play");
        //optionally bind all absent samples to the first sample
        //return false;
    } else {
        ESP_LOGI(TAG, "Memory load OK");
    }

    file.close();
    return true;
}

void SF2Parser::seekTo(uint32_t offset) {
    file.seek(offset, SeekSet);
}

bool SF2Parser::parseHeaderChunks() {
    seekTo(0);

    char riff[5] = {0}, sfbk[5] = {0};
    file.readBytes(riff, 4);
    uint32_t totalSize;
    file.readBytes((char*)&totalSize, 4);
    file.readBytes(sfbk, 4);

    if (strncmp(riff, "RIFF", 4) != 0 || strncmp(sfbk, "sfbk", 4) != 0) {
        ESP_LOGE(TAG, "Invalid SF2 file");
        return false;
    }

    int steps = 0;
    while (file.available() && steps++ < 100) {
        char id[5] = {0};
        file.readBytes(id, 4);
        uint32_t size;
        file.readBytes((char*)&size, 4);

        if (strncmp(id, "LIST", 4) == 0) {
            char listType[5] = {0};
            file.readBytes(listType, 4);
            ESP_LOGD(TAG, "LIST Type: %.*s", 4, listType);

            if (strncmp(listType, "sdta", 4) == 0) {
                sdtaOffset = file.position();
                sdtaSize = size - 4;
                file.seek(sdtaSize, SeekCur);
            } else if (strncmp(listType, "pdta", 4) == 0) {
                pdtaOffset = file.position();
                pdtaSize = size - 4;
                ESP_LOGI(TAG, "pdta found! offset: %lu size: %lu", pdtaOffset, pdtaSize);
                return true;
            } else {
                if (size < 4) break;
                file.seek(size - 4, SeekCur);
            }
        } else {
            file.seek(size, SeekCur);
        }
    }

    ESP_LOGE(TAG, "pdta not found");
    return false;
}


bool SF2Parser::parseSDTA() {
    seekTo(sdtaOffset);
    char id[5] = {0};
    file.readBytes(id, 4);
    return (strncmp(id, "smpl", 4) == 0);
}


std::vector<SampleHeader>& SF2Parser::getSamples() {
    return samples;
}

bool SF2Parser::parsePDTA() {
    ESP_LOGI(TAG, "Parsing PDTA...");
    seekTo(pdtaOffset);
    uint32_t pdtaEnd = pdtaOffset + pdtaSize;

    std::vector<PHDR> phdrs;
    std::vector<PBAG> pbags;
    std::vector<PGEN> pgens;
    std::vector<INST> insts;
    std::vector<IBAG> ibags;
    std::vector<IGEN> igens;

    while (file.position() + 8 <= pdtaEnd) {
        uint32_t chunkStart = file.position();

        char id[5] = {0};
        file.readBytes(id, 4);
        uint32_t size = 0;
        file.readBytes((char*)&size, 4);

        ESP_LOGD(TAG, "Raw chunk data: id=%.4s size=%08x", id, size);

        if (strncmp(id, "phdr", 4) == 0) {
            uint32_t count = size / sizeof(PHDR);
            for (uint32_t i = 0; i < count; ++i) {
                PHDR p;
                file.readBytes((char*)&p, sizeof(PHDR));
                phdrs.push_back(p);
                ESP_LOGD(TAG, "PHDR[%u]: name='%s' preset=%u bank=%u bagIndex=%u",
                         i, p.name, p.preset, p.bank, p.bagIndex);
            }
        }
        else if (strncmp(id, "pbag", 4) == 0) {
            uint32_t count = size / sizeof(PBAG);
            for (uint32_t i = 0; i < count; ++i) {
                PBAG b;
                file.readBytes((char*)&b, sizeof(PBAG));
                pbags.push_back(b);
            }
        }
        else if (strncmp(id, "pgen", 4) == 0) {
            uint32_t count = size / sizeof(PGEN);
            for (uint32_t i = 0; i < count; ++i) {
                PGEN g;
                file.readBytes((char*)&g, sizeof(PGEN));
                pgens.push_back(g);
            }
        }
        else if (strncmp(id, "inst", 4) == 0) {
            uint32_t count = size / sizeof(INST);
            for (uint32_t i = 0; i < count; ++i) {
                INST n;
                file.readBytes((char*)&n, sizeof(INST));
                insts.push_back(n);
            }
        }
        else if (strncmp(id, "ibag", 4) == 0) {
            uint32_t count = size / sizeof(IBAG);
            for (uint32_t i = 0; i < count; ++i) {
                IBAG b;
                file.readBytes((char*)&b, sizeof(IBAG));
                ibags.push_back(b);
            }
        }
        else if (strncmp(id, "igen", 4) == 0) {
            uint32_t count = size / sizeof(IGEN);
            for (uint32_t i = 0; i < count; ++i) {
                IGEN g;
                file.readBytes((char*)&g, sizeof(IGEN));
                igens.push_back(g);
            }
        }
        else if (strncmp(id, "shdr", 4) == 0) {
            if (!readSampleHeaders(file.position(), size)) {
                ESP_LOGE(TAG, "Failed to read sample headers");
                return false;
            }
        }
        else {
            ESP_LOGW(TAG, "Unknown PDTA chunk id: %.4s, skipping", id);
            file.seek(size, SeekCur);
        }

        // Padding for odd sizes
        if (size % 2 != 0) {
            file.seek(1, SeekCur);
        }

        ESP_LOGD(TAG, "Chunk %.4s processed. Current pos: %u", id, file.position());

        if (file.position() > pdtaEnd) {
            ESP_LOGW(TAG, "Stopped: reached end of PDTA section.");
            break;
        }
    }

    // Добавляем фиктивные окончания
    phdrs.push_back(PHDR{.bagIndex = static_cast<uint16_t>(pbags.size())});
    pbags.push_back(PBAG{.genIndex = static_cast<uint16_t>(pgens.size())});
    insts.push_back(INST{.bagIndex = static_cast<uint16_t>(ibags.size())});
    ibags.push_back(IBAG{.genIndex = static_cast<uint16_t>(igens.size())});

    // Сохраняем структуры
    this->presets.clear();
    this->instruments.clear();

    for (size_t i = 0; i + 1 < phdrs.size(); ++i) {
        SF2Preset preset;
        preset.name = String(phdrs[i].name);
        preset.bank = phdrs[i].bank;
        preset.program = phdrs[i].preset;

        for (uint16_t b = phdrs[i].bagIndex; b < phdrs[i + 1].bagIndex; ++b) {
            SF2Zone zone;
            for (uint16_t g = pbags[b].genIndex; g < pbags[b + 1].genIndex; ++g) {
                Generator gen;
                gen.oper = pgens[g].oper;
                gen.amount.sAmount = pgens[g].amount;
                zone.generators.push_back(gen);
            }

            bool hasInstrument = std::any_of(zone.generators.begin(), zone.generators.end(), [](const Generator& g) {
                return toGeneratorOperator(g.oper) == GeneratorOperator::Instrument;
            });

            if (hasInstrument) {
                preset.zones.push_back(zone);
            } else {
                preset.globalGenerators = zone.generators;  // <<< here
            }
            
 
        }

        this->presets.push_back(preset);
    }

    for (size_t i = 0; i + 1 < insts.size(); ++i) {
        SF2Instrument inst;
        inst.name = String(insts[i].name);

        for (uint16_t b = insts[i].bagIndex; b < insts[i + 1].bagIndex; ++b) {
            SF2Zone zone;
            for (uint16_t g = ibags[b].genIndex; g < ibags[b + 1].genIndex; ++g) {
                Generator gen;
                gen.oper = igens[g].oper;
                decodeGeneratorAmount(gen, igens[g].amount);
                zone.generators.push_back(gen);
            }

            bool hasSampleID = std::any_of(zone.generators.begin(), zone.generators.end(), [](const Generator& g) {
                return toGeneratorOperator(g.oper) == GeneratorOperator::SampleID;
            });

            if (hasSampleID) {
                inst.zones.push_back(zone);
            } else {
                inst.globalGenerators = zone.generators;  // <<< inject this
            }

        }

        this->instruments.push_back(inst);
    }

    ESP_LOGD(TAG, "PDTA parsed successfully: phdr=%zu pbags=%zu pgens=%zu instruments=%zu",
             phdrs.size(), pbags.size(), pgens.size(), instruments.size());
    return true;
}




bool SF2Parser::readSampleHeaders(uint32_t offset, uint32_t size) {
    seekTo(offset);
    size_t count = size / 46; // Каждая запись — 46 байт
    samples.clear();

    for (size_t i = 0; i < count; ++i) {
        
        SampleHeader sample;
        file.readBytes((char*)&sample, 46);

        if (sample.start == 0 && sample.end == 0 && sample.sampleRate == 0) {
            ESP_LOGW(TAG, "Invalid sample EOS: start=0 end=0 rate=0");
            continue;
        }

        sampleMap[i] = &samples.emplace_back(sample);
        ESP_LOGD(TAG, "Loaded sample %zu: %s (start=%u, end=%zu), orig=%d, sr=%u", i, sample.name, sample.start, sample.end, sample.originalPitch, sample.sampleRate);
    }

    file.seek(offset + size);

    ESP_LOGD(TAG, "readSampleHeaders(): file.position() after read = %u, expected = %u",
             file.position(), offset + size);
    ESP_LOGD(TAG, "Total samples loaded: %zu", samples.size());
    return true;
}

std::vector<Zone> SF2Parser::getZonesForNote(uint8_t note, uint8_t velocity, uint16_t bank, uint16_t program) {
    std::vector<Zone> resultZones;

    for (const auto& preset : presets) {
        if (preset.bank != bank || preset.program != program) continue;

        for (const auto& pzone : preset.zones) {
            int instIndex = -1;
            for (const auto& g : pzone.generators) {
                if (static_cast<GeneratorOperator>(g.oper) == GeneratorOperator::Instrument) {
                    instIndex = g.amount.sAmount;
                    break;
                }
            }
            if (instIndex < 0 || instIndex >= instruments.size()) continue;

            const auto& inst = instruments[instIndex];

            for (const auto& izone : inst.zones) {
                int sampleIndex = -1;
                uint8_t keyLo = 0, keyHi = 127;
                uint8_t velLo = 0, velHi = 127;

                for (const auto& g : izone.generators) {
                    auto oper = static_cast<GeneratorOperator>(g.oper);
                    if (oper == GeneratorOperator::KeyRange) {
                        keyLo = g.amount.range.lo;
                        keyHi = g.amount.range.hi;
                    } else if (oper == GeneratorOperator::VelRange) {
                        velLo = g.amount.range.lo;
                        velHi = g.amount.range.hi;
                    } else if (oper == GeneratorOperator::SampleID) {
                        sampleIndex = g.amount.sAmount;
                    }
                }

                if (sampleIndex >= 0 && sampleIndex < samples.size() &&
                    note >= keyLo && note <= keyHi &&
                    velocity >= velLo && velocity <= velHi) {

                    Zone z{};
                    z.sample = &samples[sampleIndex];
                    z.keyLo = keyLo;
                    z.keyHi = keyHi;
                    z.velLo = velLo;
                    z.velHi = velHi;
                    z.rootKey = z.sample->originalPitch;

                    // Apply generator hierarchy
                    applyGenerators(preset.globalGenerators, z);
                    applyGenerators(pzone.generators, z);
                    applyGenerators(inst.globalGenerators, z);
                    applyGenerators(izone.generators, z);

                    ESP_LOGD(TAG, "Mapped: note=%u velocity=%u -> sample=%s", note, velocity, z.sample->name);
                    resultZones.push_back(std::move(z));
                }
            }
        }
    }

    return resultZones;
}





void SF2Parser::applyGenerators(const std::vector<Generator>& gens, Zone& zone) {
    for (const auto& g : gens) {
        auto op = static_cast<GeneratorOperator>(g.oper);
        float val = g.amount.sAmount;

        switch (op) {
            case GeneratorOperator::SampleID:
                zone.sample = resolveSample(g.amount.uAmount);
                break;
            case GeneratorOperator::KeyRange:
                zone.keyLo = g.amount.range.lo;
                zone.keyHi = g.amount.range.hi;
                break;
            case GeneratorOperator::VelRange:
                zone.velLo = g.amount.range.lo;
                zone.velHi = g.amount.range.hi;
                break;
            case GeneratorOperator::OverridingRootKey:
                zone.rootKey = g.amount.sAmount;
                break;
            case GeneratorOperator::SampleModes:
                zone.sampleModes = g.amount.uAmount;
                break;
            case GeneratorOperator::StartLoopAddrOffset:
                zone.loopStartOffset = g.amount.sAmount;
                break;
            case GeneratorOperator::EndLoopAddrOffset:
                zone.loopEndOffset = g.amount.sAmount;
                break;
            case GeneratorOperator::StartLoopAddrCoarseOffset:
                zone.loopStartCoarseOffset = g.amount.sAmount;
                break;
            case GeneratorOperator::EndLoopAddrCoarseOffset:
                zone.loopEndCoarseOffset = g.amount.sAmount;
                break;
            case GeneratorOperator::ExclusiveClass:
                zone.exclusiveClass = g.amount.uAmount;
                break;
            case GeneratorOperator::FineTune:
                zone.fineTune = val / 100.0f;
                break;
            case GeneratorOperator::CoarseTune:
                zone.coarseTune = val;
                break;
            case GeneratorOperator::AttackVolEnv:
                zone.attackTime = timecentsToSec(val);
                break;
            case GeneratorOperator::HoldVolEnv:
                zone.holdTime = timecentsToSec(val);
                break;
            case GeneratorOperator::DecayVolEnv:
                zone.decayTime = timecentsToSec(val);
                break;
            case GeneratorOperator::SustainVolEnv:
                zone.sustainLevel = powf(10.0f, -val / 200.0f);  // val is in centibels
                break;
            case GeneratorOperator::ReleaseVolEnv:
                zone.releaseTime = timecentsToSec(val);
                break;
            case GeneratorOperator::AttackModEnv:
                zone.modAttackTime = timecentsToSec(val);
                break;
            case GeneratorOperator::DecayModEnv:
                zone.modDecayTime = timecentsToSec(val);
                break;
            case GeneratorOperator::ReleaseModEnv:
                zone.modReleaseTime = timecentsToSec(val);
                break;
            case GeneratorOperator::ModEnvToPitch:
                zone.modEnvToPitch = val;
                break;
            case GeneratorOperator::SustainModEnv:
               // zone.modSustainLevel = powf(10.0f, -val / 200.0f);  // val is in centibels
                zone.modSustainLevel = val * 0.001f ;  // map to 0..1
                break;
            case GeneratorOperator::Pan:
                zone.pan = val * 0.01f;
                break;
            case GeneratorOperator::InitialFilterFc:
                zone.filterFc = centsToHz(val);
                break;
            case GeneratorOperator::VibLfoToPitch:
                zone.vibLfoToPitch = val;  // in cents
                break;
            case GeneratorOperator::VibLfoDelay:
                zone.vibLfoDelay = timecentsToSec(val);  // val is in timecents
                break;
            case GeneratorOperator::VibLfoFreq:
                zone.vibLfoFreq = centsToHz(val);  // val is in cents, convert to Hz
                break;
            case GeneratorOperator::InitialFilterQ:
                zone.filterQ = val * 0.1f;
                break;
            case GeneratorOperator::ReverbEffectsSend:
                zone.reverbSend = val * 0.001f;
                break;
            case GeneratorOperator::ChorusEffectsSend:
                zone.chorusSend = val * 0.001f;
                break;
            case GeneratorOperator::ModLfoToPitch:
                zone.modLfoToPitch = val;  // in cents
                break;
            case GeneratorOperator::ModLfoToFilterFc:
                zone.modLfoToFilterFc = val;  // in cents
                break;
            case GeneratorOperator::ModLfoToVolume:
                zone.modLfoToVolume = powf(10.0f, -val / 200.0f);  // in centibels
                break;
            case GeneratorOperator::ModLfoDelay:
                zone.modLfoDelay = timecentsToSec(val);;
                break;
            case GeneratorOperator::ModLfoFreq:
                zone.modLfoFreq = centsToHz(val);
                break;

            default:
                break;
        }
    }
    if (!zone.chorusSend) {zone.chorusSend = 1.0f; }
    if (!zone.reverbSend) {zone.reverbSend = 1.0f; }

}


SampleHeader* SF2Parser::resolveSample(uint32_t sampleID) { 
    if (sampleID < samples.size()) {
        return &samples[sampleID];
    }
    ESP_LOGE(TAG, "Invalid sample ID: %u", sampleID);
    return nullptr;
} 

void SF2Parser::dumpPresetStructure() {
    ESP_LOGI(TAG, "\n========== SF2 Preset Structure ==========");

    for (size_t pi = 0; pi < presets.size(); ++pi) {
        const auto& preset = presets[pi];
        ESP_LOGI(TAG, "[Preset %zu] \"%s\" (Bank=%u, Program=%u, Zones=%zu)",
                 pi, preset.name.c_str(), preset.bank, preset.program, preset.zones.size());

        for (size_t zi = 0; zi < preset.zones.size(); ++zi) {
            const auto& zone = preset.zones[zi];
            ESP_LOGI(TAG, "  PZone[%zu]: %zu generators", zi, zone.generators.size());

            int instIndex = -1;

            for (const auto& gen : zone.generators) {
                GeneratorOperator op = toGeneratorOperator(gen.oper);
                ESP_LOGI(TAG, "    Gen %s = %d", toString(op), gen.amount.sAmount);

                if (op == GeneratorOperator::Instrument) {
                    instIndex = gen.amount.uAmount;
                    if (instIndex >= 0 && instIndex < instruments.size()) {
                        const auto& inst = instruments[instIndex];
                        ESP_LOGI(TAG, "      → Instrument \"%s\" (Zones=%zu)",
                                 inst.name.c_str(), inst.zones.size());

                        for (size_t iz = 0; iz < inst.zones.size(); ++iz) {
                            const auto& izone = inst.zones[iz];
                            ESP_LOGI(TAG, "        IZone[%zu]:", iz);

                            SampleHeader* sample = nullptr;
                            uint8_t keyLo = 0, keyHi = 127;
                            uint8_t velLo = 0, velHi = 127;

                            for (const auto& g : izone.generators) {
                                GeneratorOperator iop = toGeneratorOperator(g.oper);
                                if (iop == GeneratorOperator::Instrument || iop == GeneratorOperator::SampleID
                                 || iop == GeneratorOperator::KeyRange || iop == GeneratorOperator::VelRange) {
                                    ESP_LOGI(TAG, "          Gen %s = %d", toString(iop), g.amount.sAmount);
                                }
                                if (iop == GeneratorOperator::SampleID) {
                                    sample = resolveSample(g.amount.uAmount);
                                } else if (iop == GeneratorOperator::KeyRange) {
                                    keyLo = g.amount.range.lo;
                                    keyHi = g.amount.range.hi;
                                } else if (iop == GeneratorOperator::VelRange) {
                                    velLo = g.amount.range.lo;
                                    velHi = g.amount.range.hi;
                                }
                            }

                            if (sample) {
                                ESP_LOGI(TAG, "          → Sample \"%s\" key[%u-%u] vel[%u-%u]", sample->name, keyLo, keyHi, velLo, velHi);
                            } else {
                                ESP_LOGW(TAG, "          → No Sample");
                            }
                        }
                    } else {
                        ESP_LOGW(TAG, "      → Invalid instrument index: %d", instIndex);
                    }
                }
            }
        }
    }

    ESP_LOGI(TAG, "========== End of Preset Dump ==========\n");
}

bool SF2Parser::loadSampleDataToMemory() {
    if (sdtaOffset == 0 || samples.empty()) {
        ESP_LOGE(TAG, "Sample data not available or no sample headers found");
        return false;
    }

    file.seek(sdtaOffset);
    char id[5] = {0};
    file.readBytes(id, 4);

    if (strncmp(id, "smpl", 4) != 0) {
        ESP_LOGE(TAG, "Expected 'smpl' chunk not found");
        return false;
    }

    uint32_t smplSize = 0;
    file.readBytes((char*)&smplSize, 4);
    uint32_t smplStart = file.position();

    ESP_LOGI(TAG, "Reading sample data: offset=%u size=%u", smplStart, smplSize);

    SampleHeader* fallback = nullptr;

    for (size_t i = 0; i < samples.size(); ++i) {

        auto& s = samples[i];
        uint32_t length = (s.end > s.start) ? (s.end - s.start) : 0;

        if (length == 0) {
            ESP_LOGW(TAG, "Sample %zu (%s) has zero length", i, s.name);
            continue;
        }

        s.data = (uint8_t*)heap_caps_aligned_alloc(4, length * sizeof(int16_t), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);

        if (!s.data) {
            ESP_LOGE(TAG, "PSRAM allocation failed for sample %zu (%s), size=%u", i, s.name, length * 2);

            if (fallback) {
                // Fallback: bind to first loaded sample
                s.data = fallback->data;
                s.dataSize = fallback->dataSize;
                s.start = fallback->start;
                s.end = fallback->end;
                s.startLoop = fallback->startLoop;
                s.endLoop = fallback->endLoop;
                s.sampleRate = fallback->sampleRate;
                s.originalPitch = fallback->originalPitch;
                s.pitchCorrection = fallback->pitchCorrection;
                s.sampleLink = fallback->sampleLink;
                s.sampleType = fallback->sampleType;

                ESP_LOGW(TAG, "Sample %zu (%s) will use fallback sample", i, s.name);
                continue;
            } else {
                ESP_LOGE(TAG, "No fallback sample available — aborting");
                return false;
            }
        }

        file.seek(smplStart + s.start * 2); // 16-bit PCM
        file.read((uint8_t*)s.data, length * 2);
        s.dataSize = length * 2;

        ESP_LOGD(TAG, "Loaded sample %zu: %s (offset=%u length=%u)", i, s.name, s.start, length);

        if (!fallback)
            fallback = &s;
    }

    return true;
}


void SF2Parser::clear() {
    for (auto& sample : samples) {
        if (sample.data) {
            heap_caps_aligned_free(sample.data);
            sample.data = nullptr;
            sample.dataSize = 0;
        }
    }

    samples.clear();
    presets.clear();
    instruments.clear();
    zones.clear();
    sampleMap.clear();
    startPosMap.clear();
}

bool SF2Parser::hasPreset(uint16_t bank, uint16_t program) const {
    for (const auto& preset : presets) {
        if (preset.bank == bank && preset.program == program) {
            return true;
        }
    }
    return false;
}
