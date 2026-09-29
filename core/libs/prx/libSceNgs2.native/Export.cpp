
#include <cstdarg>
#include "prx/libc/include/VerboseLog.hpp"
#include <cstdint>
#include <cstdio>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fstream>
#endif
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int32_t kOk = 0;
constexpr int32_t kErrInvalidOutAddress = static_cast<int32_t>(0x804A0053);
constexpr int32_t kErrInvalidSystemHandle = static_cast<int32_t>(0x804A0230);
constexpr int32_t kErrInvalidRackHandle = static_cast<int32_t>(0x804A0261);
constexpr int32_t kErrInvalidVoiceHandle = static_cast<int32_t>(0x804A0300);
constexpr int32_t kErrInvalidParam = static_cast<int32_t>(0x804A0001);

constexpr uint32_t kRackSampler = 0x1000;
constexpr uint32_t kRackSubmixer = 0x2000;
constexpr uint32_t kRackReverb = 0x2001;
constexpr uint32_t kRackMastering = 0x3000;
constexpr uint32_t kRackCustomSampler = 0x4001;
constexpr uint32_t kRackCustomSubmixer = 0x4002;
constexpr uint32_t kRackCustomMastering = 0x4003;

constexpr bool IsSamplerRack(uint32_t id) { return id == kRackSampler || id == kRackCustomSampler; }
constexpr bool IsSubmixerRack(uint32_t id) { return id == kRackSubmixer || id == kRackCustomSubmixer; }
constexpr bool IsMasteringRack(uint32_t id) { return id == kRackMastering || id == kRackCustomMastering; }

constexpr uint32_t kMaxVoicesPerRack = 4096;
constexpr int kMaxPorts = 16;
constexpr int kMaxMatrices = 8;
constexpr int kMaxMatrixLevels = 64;
constexpr uint32_t kRepeatInfinite = 0xFFFFFFFFu;

constexpr uint32_t kWavePcmI8 = 0x10, kWavePcmU8 = 0x11, kWavePcmI16Le = 0x12, kWavePcmI16Be = 0x13;
constexpr uint32_t kWavePcmI32Le = 0x16, kWavePcmI32Be = 0x17, kWavePcmF32Le = 0x18, kWavePcmF32Be = 0x19;

uint32_t PcmBytes(uint32_t t) {
    switch (t) {
        case kWavePcmI8: case kWavePcmU8: return 1;
        case kWavePcmI16Le: case kWavePcmI16Be: return 2;
        case kWavePcmI32Le: case kWavePcmI32Be: case kWavePcmF32Le: case kWavePcmF32Be: return 4;
        default: return 0;
    }
}

struct SysOptionLayout {
    std::size_t size;
    char name[64];
    std::uintptr_t job[4];
    uint32_t flags, max_grain, num_grain, sample_rate, max_voice_channels, reserved[5];
};
struct RackOptionLayout {
    std::size_t size;
    char name[64];
    uint32_t flags, max_grain, max_voices, max_input_delay, max_matrices, max_ports, max_voice_channels, max_output_channels, reserved[18];
};
static_assert(sizeof(SysOptionLayout) == 144);
static_assert(sizeof(RackOptionLayout) == 176);

struct WaveFormatLayout { uint32_t type, channels, rate, config, frame_offset, frame_margin; };

struct Block {
    const uint8_t* data = nullptr;
    uint64_t size = 0;
    uint32_t repeats = 0, skip = 0, num_samples = 0;
    uint64_t user = 0;
};

enum class VState { Empty, Playing, Paused, Stopping };

struct Rack;
struct Voice {
    Rack* rack = nullptr;
    uint32_t index = 0;
    WaveFormatLayout fmt{};
    bool have_fmt = false;
    std::deque<Block> blocks;
    bool accepts_blocks = true;
    float pitch = 1.0f;
    VState state = VState::Empty;
    bool cursor_init = false;
    double pos = 0;
    uint32_t repeats_done = 0;
    uint32_t cur_frames = 0, cur_start = 0, cur_nch = 1;
    uint32_t cur_kind = 0;
    uint32_t cur_type = 0;
    double cur_rate = 48000;
    std::vector<int16_t> adpcm;
    uint64_t decoded_samples = 0, decoded_bytes = 0;
    uint64_t last_user = 0;
    const void* last_data = nullptr;
    float peak = 0;
    float stop_gain = 1.0f;
    uint32_t starve_grains = 0;
    bool had_blocks_since_play = false;
    struct Port { bool patched = false; std::uintptr_t dest = 0; uint32_t dest_input = 0; float volume = 1.0f; int32_t matrix = -1; } ports[kMaxPorts];
    std::vector<float> matrices[kMaxMatrices];
    bool matrix_set[kMaxMatrices] = {};
    uint64_t last_active_render = 0;
    std::uintptr_t cb_fn = 0, cb_user = 0;
    uint32_t cb_flags = 0;
};
struct System;
struct Rack {
    System* sys = nullptr;
    uint32_t id = 0;
    uint32_t max_voices = 0;
    std::uintptr_t user_data = 0;
    std::vector<std::unique_ptr<Voice>> voices;
};
struct System {
    uint32_t grain = 256, max_grain = 512, rate = 48000;
    std::uintptr_t user_data = 0;
    uint64_t renders = 0;
    std::vector<Rack*> racks;
};

struct Diag {
    std::map<uint32_t, uint64_t> param_count;
    std::map<uint32_t, uint64_t> param_unimpl;
    std::map<uint64_t, uint64_t> fmt_count;
    std::map<uint32_t, uint64_t> cmd_count;
    std::set<std::string> once;
    uint64_t calls_control = 0, calls_runcmd = 0, calls_getstate = 0, calls_render = 0;
    uint64_t blocks_started = 0, blocks_unsupported = 0, blocks_added = 0, events = 0;
    uint64_t voices_started = 0;
    float peak_interval = 0, peak_max = 0;
    uint32_t active_max = 0;
    std::chrono::steady_clock::time_point last_report = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
};

struct Globals {
    std::mutex mtx;
    std::unordered_set<std::uintptr_t> systems, racks, voices;
    Diag diag;
    std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now();
    bool deadline_init = false;
    struct PendingCb { std::uintptr_t fn, user, handle; uint32_t flags; };
    std::vector<PendingCb> pending_cb;
};
Globals& G() {
    static Globals* g = new Globals();
    return *g;
}

void Log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void Log(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    std::fprintf(stderr, "%s\n", buf);
    std::fflush(stderr);
}

bool Once(const std::string& key) { return G().diag.once.insert(key).second; }

std::string Hex(const void* p, size_t n) {
    std::string s;
    char b[4];
    for (size_t i = 0; i < n; ++i) {
        std::snprintf(b, sizeof(b), "%02x", static_cast<const uint8_t*>(p)[i]);
        s += b;
    }
    return s;
}

#ifdef _WIN32
bool Readable(const void* p, size_t n) {
    if (p == nullptr) return false;
    if (n == 0) return true;
    const auto* c = static_cast<const uint8_t*>(p);
    const auto* end = c + n;
    if (end < c) return false;
    MEMORY_BASIC_INFORMATION mbi;
    while (c < end) {
        if (VirtualQuery(c, &mbi, sizeof(mbi)) == 0) return false;
        if (mbi.State != MEM_COMMIT || mbi.Protect == 0 || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
        c = static_cast<const uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
    }
    return true;
}
bool Writable(void* p, size_t n) {
    if (p == nullptr) return false;
    if (n == 0) return true;
    auto* c = static_cast<uint8_t*>(p);
    auto* end = c + n;
    if (end < c) return false;
    MEMORY_BASIC_INFORMATION mbi;
    while (c < end) {
        if (VirtualQuery(c, &mbi, sizeof(mbi)) == 0) return false;
        if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD | PAGE_READONLY | PAGE_EXECUTE_READ | PAGE_EXECUTE)) || mbi.Protect == 0) return false;
        c = static_cast<uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
    }
    return true;
}
#else
bool ProbeMapping(const void* p, size_t n, bool requireWrite) {
    if (p == nullptr) return false;
    if (n == 0) return true;
    const auto start = reinterpret_cast<uintptr_t>(p);
    const auto end = start + n;
    if (end < start) return false;
    std::ifstream maps("/proc/self/maps");
    if (!maps) return false;
    std::string line;
    auto cursor = start;
    while (cursor < end && std::getline(maps, line)) {
        unsigned long long regionStart = 0, regionEnd = 0;
        char r = '-', w = '-';
        if (std::sscanf(line.c_str(), "%llx-%llx %c%c", &regionStart, &regionEnd, &r, &w) != 4) continue;
        if (regionEnd <= cursor) continue;
        if (regionStart > cursor || r != 'r') return false;
        if (requireWrite && w != 'w') return false;
        cursor = regionEnd;
    }
    return cursor >= end;
}
bool Readable(const void* p, size_t n) { return ProbeMapping(p, n, false); }
bool Writable(void* p, size_t n) { return ProbeMapping(p, n, true); }
#endif

template <class T> T Rd(const uint8_t* p) { T v; std::memcpy(&v, p, sizeof(T)); return v; }

System* FindSystem(std::uintptr_t h) { return G().systems.count(h) ? reinterpret_cast<System*>(h) : nullptr; }
Rack* FindRack(std::uintptr_t h) { return G().racks.count(h) ? reinterpret_cast<Rack*>(h) : nullptr; }
Voice* FindVoice(std::uintptr_t h) { return G().voices.count(h) ? reinterpret_cast<Voice*>(h) : nullptr; }

const char* RackName(uint32_t id) {
    switch (id) {
        case kRackSampler: return "sampler";
        case kRackSubmixer: return "submixer";
        case kRackReverb: return "reverb";
        case kRackMastering: return "mastering";
        case kRackCustomSampler: return "custom-sampler";
        case kRackCustomSubmixer: return "custom-submixer";
        case kRackCustomMastering: return "custom-mastering";
        default: return "other";
    }
}

constexpr int kCoeff0[5] = {0, 60, 115, 98, 122};
constexpr int kCoeff1[5] = {0, 0, -52, -55, -60};

bool LooksLikeAdpcm(const uint8_t* d, uint64_t size) {
    if (size < 16 || size % 16 != 0) return false;
    const uint64_t frames = std::min<uint64_t>(size / 16, 64);
    for (uint64_t f = 0; f < frames; ++f) {
        const uint8_t hdr = d[f * 16], flags = d[f * 16 + 1];
        if ((hdr >> 4) > 4 || (hdr & 0xF) > 12) return false;
        if (flags > 7 || flags == 5) return false;
    }
    return true;
}
void DecodeAdpcm(const uint8_t* frames, uint64_t bytes, std::vector<int16_t>& out) {
    const uint64_t n = bytes / 16;
    out.clear();
    out.reserve(n * 28);
    int32_t h1 = 0, h2 = 0;
    for (uint64_t f = 0; f < n; ++f) {
        const uint8_t* fr = frames + f * 16;
        const uint32_t shift = fr[0] & 0xF;
        uint32_t filter = (fr[0] >> 4) & 0xF;
        if (filter > 4) filter = 0;
        const uint8_t flags = fr[1];
        for (int i = 0; i < 14; ++i) {
            for (int nib = 0; nib < 2; ++nib) {
                const uint32_t raw = nib == 0 ? (fr[2 + i] & 0xF) : (fr[2 + i] >> 4);
                const int32_t s = static_cast<int32_t>(static_cast<int16_t>(raw << 12)) >> shift;
                int32_t v = s + ((h1 * kCoeff0[filter] + h2 * kCoeff1[filter]) >> 6);
                v = std::clamp(v, -32768, 32767);
                out.push_back(static_cast<int16_t>(v));
                h2 = h1;
                h1 = v;
            }
        }
        if (flags == 1 || flags == 7) break;
    }
}

void Report(System* focus_unused = nullptr) {
    (void)focus_unused;
    if (!VerboseLog()) return;
    auto& g = G();
    auto& d = g.diag;
    const auto now = std::chrono::steady_clock::now();
    if (now - d.last_report < std::chrono::seconds(5)) return;
    d.last_report = now;
    std::map<uint32_t, uint32_t> rack_voices;
    uint32_t active = 0, sampler_playing = 0;
    for (auto sh : g.racks) {
        const Rack* r = reinterpret_cast<Rack*>(sh);
        rack_voices[r->id] += static_cast<uint32_t>(r->voices.size());
        for (auto& v : r->voices)
            if (v->state != VState::Empty) { ++active; if (IsSamplerRack(r->id)) ++sampler_playing; }
    }
    std::string rv;
    for (auto& [id, n] : rack_voices) { char b[64]; std::snprintf(b, sizeof(b), " %s(0x%x)=%u", RackName(id), id, n); rv += b; }
    Log("[NGS2] t=%llds systems=%zu racks=%zu voices:%s active=%u (sampler %u) renders=%llu ctl=%llu runcmd=%llu getstate=%llu "
        "events=%llu blocks(added=%llu started=%llu unsupported=%llu) peak=%.3f peakmax=%.3f",
        (long long)std::chrono::duration_cast<std::chrono::seconds>(now - d.start).count(), g.systems.size(), g.racks.size(), rv.c_str(), active,
        sampler_playing, (unsigned long long)d.calls_render, (unsigned long long)d.calls_control, (unsigned long long)d.calls_runcmd,
        (unsigned long long)d.calls_getstate, (unsigned long long)d.events, (unsigned long long)d.blocks_added,
        (unsigned long long)d.blocks_started, (unsigned long long)d.blocks_unsupported, d.peak_interval, d.peak_max);
    if (!d.param_unimpl.empty()) {
        std::string s;
        for (auto& [id, n] : d.param_unimpl) { char b[48]; std::snprintf(b, sizeof(b), " 0x%08x x%llu", id, (unsigned long long)n); s += b; }
        Log("[NGS2]   NOT IMPLEMENTED param ids:%s", s.c_str());
    }
    if (!d.fmt_count.empty()) {
        std::string s;
        for (auto& [k, n] : d.fmt_count) {
            char b[80];
            std::snprintf(b, sizeof(b), " type=0x%x/ch%u/%uHz x%llu", static_cast<uint32_t>(k >> 40), static_cast<uint32_t>((k >> 32) & 0xFF),
                          static_cast<uint32_t>(k & 0xFFFFFFFFu), (unsigned long long)n);
            s += b;
        }
        Log("[NGS2]   waveform formats:%s", s.c_str());
    }
    if (!d.cmd_count.empty()) {
        std::string s;
        for (auto& [id, n] : d.cmd_count) { char b[48]; std::snprintf(b, sizeof(b), " 0x%08x x%llu", id, (unsigned long long)n); s += b; }
        Log("[NGS2]   RunCommands leading ids:%s", s.c_str());
    }
    d.peak_interval = 0;
    d.active_max = 0;
}

void ClearVoice(Voice& v) {
    v.blocks.clear();
    v.cursor_init = false;
    v.pos = 0;
    v.repeats_done = 0;
    v.adpcm.clear();
    v.accepts_blocks = true;
    v.pitch = 1.0f;
    v.state = VState::Empty;
    v.stop_gain = 1.0f;
    v.starve_grains = 0;
    v.had_blocks_since_play = false;
}

void ApplyEvent(Voice& v, uint32_t ev) {
    auto& d = G().diag;
    ++d.events;
    if (d.events <= 12) Log("[NGS2] event 0x%x on voice %u of rack 0x%x (%s) state=%d", ev, v.index, v.rack->id, RackName(v.rack->id), (int)v.state);
    switch (ev) {
        case 0x01:
            if (v.state == VState::Empty || v.state == VState::Stopping) {
                v.state = VState::Playing;
                v.stop_gain = 1.0f;
                v.starve_grains = 0;
                v.had_blocks_since_play = !v.blocks.empty();
                ++d.voices_started;
            }
            break;
        case 0x02:
            if (v.state == VState::Playing || v.state == VState::Paused) v.state = VState::Stopping;
            break;
        case 0x04:
        case 0x08:
            ClearVoice(v);
            break;
        case 0x10:
            if (v.state == VState::Playing) v.state = VState::Paused;
            break;
        case 0x20:
            if (v.state == VState::Paused) v.state = VState::Playing;
            break;
        default:
            if (Once("event" + std::to_string(ev))) Log("[NGS2] UNIMPLEMENTED voice event id 0x%x", ev);
            ++d.param_unimpl[0x00060000u | (ev & 0xFFFF)];
            break;
    }
}

void HandleSamplerBlocks(Voice& v, const uint8_t* p, uint32_t size) {
    auto& d = G().diag;
    if (size < 32) { if (Once("blocks-short")) Log("[NGS2] waveform-blocks param too short (%u)", size); return; }
    const uint8_t* base = reinterpret_cast<const uint8_t*>(Rd<uint64_t>(p + 8));
    const uint32_t flags = Rd<uint32_t>(p + 16);
    const uint32_t n = Rd<uint32_t>(p + 20);
    const uint8_t* arr = reinterpret_cast<const uint8_t*>(Rd<uint64_t>(p + 24));
    if (n == 0) {
        if ((flags & 4u) == 0 && Once("blocks-n")) Log("[NGS2] waveform-blocks param with numBlocks=0 flags=0x%x", flags);
    } else if (n > 64 || !Readable(arr, static_cast<size_t>(n) * 40) || !Readable(base, 1)) {
        if (Once("blocks-bad")) Log("[NGS2] waveform-blocks param unusable: base=%p blocks=%p numBlocks=%u flags=0x%x size=%u",
                                    (const void*)base, (const void*)arr, n, flags, size);
        return;
    }
    if (flags & ~0x17u) { if (Once("blocks-flags")) Log("[NGS2] waveform-blocks flags 0x%x has unknown bits (handled: 1=keep accepting, 2=raw data, 4=reset)", flags); }
    if (flags & 4u) {
        v.blocks.clear();
        v.cursor_init = false;
        v.pos = 0;
        v.repeats_done = 0;
        v.adpcm.clear();
    }
    v.accepts_blocks = (flags & 1u) != 0;
    const bool only_data = (flags & 2u) != 0;
    if (Once("blocks-first")) Log("[NGS2] first waveform-blocks param: base=%p flags=0x%x numBlocks=%u fmt: type=0x%x ch=%u rate=%u have_fmt=%d bytes=%s",
                                  (const void*)base, flags, n, v.fmt.type, v.fmt.channels, v.fmt.rate, (int)v.have_fmt, Hex(p, std::min<uint32_t>(size, 48)).c_str());
    for (uint32_t i = 0; i < n; ++i) {
        const uint8_t* b = arr + static_cast<size_t>(i) * 40;
        const uint64_t off = Rd<uint64_t>(b);
        Block blk;
        blk.size = Rd<uint64_t>(b + 8);
        blk.repeats = Rd<uint32_t>(b + 16);
        blk.skip = Rd<uint32_t>(b + 20);
        blk.num_samples = Rd<uint32_t>(b + 24);
        blk.user = Rd<uint64_t>(b + 32);
        if (blk.size == 0 && blk.num_samples == 0) continue;
        if (only_data) {
            if (Once("blocks-raw")) Log("[NGS2] raw-data waveform block (compressed codec stream append) NOT IMPLEMENTED: size=%llu", (unsigned long long)blk.size);
            ++d.blocks_unsupported;
            continue;
        }
        blk.data = base + off;
        if (blk.size > (1ull << 30) || !Readable(blk.data, static_cast<size_t>(blk.size))) {
            if (Once("blocks-size")) Log("[NGS2] block with unusable size %llu ignored (data=%p off=%llu)", (unsigned long long)blk.size, (const void*)blk.data, (unsigned long long)off);
            continue;
        }
        if (d.blocks_added < 8)
            Log("[NGS2] block #%llu: data=%p size=%llu repeats=%u skip=%u samples=%u user=%llx (voice %u rack 0x%x)", (unsigned long long)d.blocks_added, (const void*)blk.data,
                (unsigned long long)blk.size, blk.repeats, blk.skip, blk.num_samples, (unsigned long long)blk.user, v.index, v.rack->id);
        ++d.blocks_added;
        v.blocks.push_back(blk);
        if (v.state == VState::Playing) v.had_blocks_since_play = true;
    }
    if (v.blocks.size() > 256) v.blocks.erase(v.blocks.begin(), v.blocks.begin() + (v.blocks.size() - 256));
}

void HandleParam(Voice& v, const uint8_t* p, uint32_t size, uint32_t id) {
    auto& d = G().diag;
    ++d.param_count[id];
    const uint32_t fam = id >> 16, cid = id & 0x7FFF;
    bool impl = false;
    if (Once("pid" + std::to_string(id)))
        Log("[NGS2] new voice param id=0x%08x size=%u rack=0x%x(%s) bytes=%s", id, size, v.rack->id, RackName(v.rack->id), Hex(p, std::min<uint32_t>(size, 48)).c_str());
    if ((id >> 15) & 1) {
    } else if (fam == 0) {
        switch (cid) {
            case 1:
                if (size >= 24) {
                    const uint32_t mid = Rd<uint32_t>(p + 8), nl = Rd<uint32_t>(p + 12);
                    const float* lv = reinterpret_cast<const float*>(Rd<uint64_t>(p + 16));
                    if (mid < kMaxMatrices && nl <= kMaxMatrixLevels && Readable(lv, nl * 4)) {
                        v.matrices[mid].assign(lv, lv + nl);
                        v.matrix_set[mid] = true;
                        impl = true;
                        if (d.param_count[id] <= 6) {
                            std::string s;
                            for (uint32_t i = 0; i < std::min<uint32_t>(nl, 8); ++i) { char b[24]; std::snprintf(b, sizeof(b), " %.3f", lv[i]); s += b; }
                            Log("[NGS2] matrix levels voice %u/rack 0x%x id=%u n=%u:%s", v.index, v.rack->id, mid, nl, s.c_str());
                        }
                    }
                }
                break;
            case 2:
                if (size >= 16) {
                    const uint32_t port = Rd<uint32_t>(p + 8);
                    const float lvl = Rd<float>(p + 12);
                    if (port < kMaxPorts && std::isfinite(lvl)) { v.ports[port].volume = lvl; impl = true; }
                }
                break;
            case 3:
                if (size >= 16) {
                    const uint32_t port = Rd<uint32_t>(p + 8);
                    if (port < kMaxPorts) { v.ports[port].matrix = Rd<int32_t>(p + 12); impl = true; }
                }
                break;
            case 4: impl = true; break;
            case 5:
                if (size >= 24) {
                    const uint32_t port = Rd<uint32_t>(p + 8);
                    if (port < kMaxPorts) {
                        v.ports[port].dest_input = Rd<uint32_t>(p + 12);
                        v.ports[port].dest = static_cast<std::uintptr_t>(Rd<uint64_t>(p + 16));
                        v.ports[port].patched = v.ports[port].dest != 0;
                        impl = true;
                        if (d.param_count[id] <= 12) Log("[NGS2] patch voice %u/rack 0x%x port %u -> %llx input %u", v.index, v.rack->id, port, (unsigned long long)v.ports[port].dest, v.ports[port].dest_input);
                    }
                }
                break;
            case 6:
                if (size >= 12) { ApplyEvent(v, Rd<uint32_t>(p + 8)); impl = true; }
                break;
            case 7:
                if (size >= 28) {
                    v.cb_fn = static_cast<std::uintptr_t>(Rd<uint64_t>(p + 8));
                    v.cb_user = static_cast<std::uintptr_t>(Rd<uint64_t>(p + 16));
                    v.cb_flags = Rd<uint32_t>(p + 24);
                    if (Once("callback")) Log("[NGS2] voice callback registered: fn=%llx user=%llx flags=%x", (unsigned long long)v.cb_fn, (unsigned long long)v.cb_user, v.cb_flags);
                }
                impl = true;
                break;
            default: break;
        }
    } else if (IsSamplerRack(fam) && IsSamplerRack(v.rack->id)) {
        switch (id & 0xFFFF) {
            case 0:
                if (size >= 32) {
                    WaveFormatLayout f;
                    std::memcpy(&f, p + 8, sizeof(f));
                    v.fmt = f;
                    v.have_fmt = true;
                    impl = true;
                    const uint64_t key = (static_cast<uint64_t>(f.type) << 40) | (static_cast<uint64_t>(f.channels & 0xFF) << 32) | f.rate;
                    if (d.fmt_count[key]++ == 0)
                        Log("[NGS2] sampler voice format: type=0x%x channels=%u rate=%u config=0x%x margin=%u offset=%u%s", f.type, f.channels, f.rate, f.config, f.frame_margin, f.frame_offset,
                            PcmBytes(f.type) ? "" : " (non-PCM: VAG/ATRAC9?)");
                }
                break;
            case 1: HandleSamplerBlocks(v, p, size); impl = true; break;
            case 4:
                for (auto& b : v.blocks) { if (b.repeats != 0) { b.repeats = 0; break; } }
                impl = true;
                break;
            case 5:
                if (size >= 12) {
                    const float ratio = Rd<float>(p + 8);
                    if (std::isfinite(ratio)) { v.pitch = std::clamp(ratio, 0.0f, 4.0f); impl = true; }
                }
                break;
            default: break;
        }
    } else if (fam == 0x4000) {
        if (Once("modctl" + std::to_string(id & 0xFFFF)))
            Log("[NGS2] custom module control id=0x%08x (module 0x%x, ctl %u, no. %u) accepted and ignored (no FX chain)",
                id, (id >> 8) & 0xFF, (id >> 5) & 7, id & 0x1F);
        impl = true;
    }
    if (!impl) ++d.param_unimpl[id];
}

void WalkParams(Voice& v, const uint8_t* p, const char* who) {
    for (int i = 0; i < 64; ++i) {
        if (!Readable(p, 8)) { if (Once(std::string("badparam") + who)) Log("[NGS2] %s: unreadable param header %p", who, (const void*)p); return; }
        const uint16_t size = Rd<uint16_t>(p);
        const int16_t next = Rd<int16_t>(p + 2);
        const uint32_t id = Rd<uint32_t>(p + 4);
        if (size < 8 || size > 0x4000 || !Readable(p, size)) {
            if (Once(std::string("badsize") + who)) Log("[NGS2] %s: bad param header size=%u next=%d id=0x%08x bytes=%s", who, size, next, id, Hex(p, 16).c_str());
            return;
        }
        HandleParam(v, p, size, id);
        if (next <= 0) return;
        p += next;
    }
}

float PcmSample(const uint8_t* p, uint32_t type) {
    switch (type) {
        case kWavePcmI8: return static_cast<float>(static_cast<int8_t>(*p)) * (1.0f / 128.0f);
        case kWavePcmU8: return (static_cast<float>(*p) - 128.0f) * (1.0f / 128.0f);
        case kWavePcmI16Le: return static_cast<float>(Rd<int16_t>(p)) * (1.0f / 32768.0f);
        case kWavePcmI16Be: return static_cast<float>(static_cast<int16_t>((p[0] << 8) | p[1])) * (1.0f / 32768.0f);
        case kWavePcmI32Le: return static_cast<float>(Rd<int32_t>(p)) * (1.0f / 2147483648.0f);
        case kWavePcmI32Be: return static_cast<float>(static_cast<int32_t>((uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3])) * (1.0f / 2147483648.0f);
        case kWavePcmF32Le: return Rd<float>(p);
        case kWavePcmF32Be: { uint32_t u = (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; float f; std::memcpy(&f, &u, 4); return f; }
        default: return 0.0f;
    }
}

bool InitBlock(Voice& v, uint32_t sys_rate) {
    (void)sys_rate;
    auto& d = G().diag;
    const Block& b = v.blocks.front();
    if (!Readable(b.data, static_cast<size_t>(b.size))) {
        if (Once("unreadable-block")) Log("[NGS2] block data %p size %llu is not readable; block skipped", (const void*)b.data, (unsigned long long)b.size);
        ++d.blocks_unsupported;
        return false;
    }
    const uint32_t type = v.fmt.type;
    const uint32_t nch = std::clamp<uint32_t>(v.fmt.channels, 1, 8);
    v.cur_type = type;
    v.cur_rate = v.fmt.rate >= 4000 && v.fmt.rate <= 384000 ? v.fmt.rate : 48000;
    if (PcmBytes(type) != 0) {
        v.cur_kind = 0;
        v.cur_nch = nch;
        uint64_t total = b.size / (PcmBytes(type) * nch);
        if (b.num_samples != 0 && b.num_samples < total) total = b.num_samples;
        v.cur_frames = static_cast<uint32_t>(std::min<uint64_t>(total, 0x7FFFFFFF));
    } else {
        const uint8_t* data = b.data;
        uint64_t size = b.size;
        if (size >= 0x30 && Rd<uint32_t>(data) == 0x70474156u) {
            data += 0x30;
            size -= 0x30;
            size -= size % 16;
        }
        if (!LooksLikeAdpcm(data, size)) {
            if (Once("unsupported-fmt" + std::to_string(type)))
                Log("[NGS2] UNSUPPORTED waveform type 0x%x (ch=%u rate=%u, block %llu bytes: %s) -- likely ATRAC9 or unknown codec; block skipped. "
                    "audiodec/AT9 decode is NOT implemented in libSceNgs2",
                    type, v.fmt.channels, v.fmt.rate, (unsigned long long)b.size, Hex(b.data, 24).c_str());
            ++d.blocks_unsupported;
            return false;
        }
        v.cur_kind = 1;
        v.cur_nch = 1;
        DecodeAdpcm(data, size, v.adpcm);
        v.cur_frames = static_cast<uint32_t>(v.adpcm.size());
        if (Once("adpcm-decoded")) Log("[NGS2] decoding PS-ADPCM (VAG) waveform: type=0x%x %llu bytes -> %u samples", type, (unsigned long long)b.size, v.cur_frames);
    }
    v.cur_start = std::min(b.skip, v.cur_frames ? v.cur_frames - 1 : 0);
    if (v.pos < v.cur_start) v.pos = v.cur_start;
    v.repeats_done = 0;
    ++d.blocks_started;
    return v.cur_frames > 0;
}

void FoldMatrix(const std::vector<float>& lv, uint32_t nin, float g[2][2]) {
    std::memset(g, 0, sizeof(float) * 4);
    uint32_t nout = nin ? static_cast<uint32_t>(lv.size()) / nin : 0;
    if (nout == 0 || static_cast<uint32_t>(lv.size()) % nin != 0) nout = std::min<uint32_t>(2, static_cast<uint32_t>(lv.size()));
    if (lv.size() == 64) nout = 8;
    if (nout == 0) nout = 2;
    if (nout == 1 && nin == 2 && lv.size() == 2) {
        g[0][0] = std::isfinite(lv[0]) ? lv[0] : 0.0f;
        g[1][1] = std::isfinite(lv[1]) ? lv[1] : 0.0f;
        return;
    }
    for (uint32_t i = 0; i < std::min<uint32_t>(nin, 2); ++i) {
        for (uint32_t o = 0; o < nout && o < 8; ++o) {
            const size_t idx = static_cast<size_t>(i) * nout + o;
            if (idx >= lv.size()) break;
            const float l = std::isfinite(lv[idx]) ? lv[idx] : 0.0f;
            switch (o) {
                case 0: case 4: case 6: g[i][0] += l; break;
                case 1: case 5: case 7: g[i][1] += l; break;
                case 2: g[i][0] += l * 0.7071f; g[i][1] += l * 0.7071f; break;
                default: break;
            }
        }
    }
}

float RouteGain(std::uintptr_t dest, int depth) {
    if (dest == 0 || depth > 4) return 0.0f;
    if (Voice* dv = FindVoice(dest)) {
        switch (dv->rack->id) {
            case kRackMastering: case kRackCustomMastering: return 1.0f;
            case kRackSubmixer: case kRackCustomSubmixer: {
                bool any = false;
                float sum = 0;
                for (const auto& p : dv->ports) {
                    if (!p.patched) continue;
                    any = true;
                    sum += p.volume * RouteGain(p.dest, depth + 1);
                }
                return any ? sum : 1.0f;
            }
            default: return 0.0f;
        }
    }
    if (G().racks.count(dest) != 0) return 1.0f;
    return 0.0f;
}

void ComputeGains(const Voice& v, float gain[2][2]) {
    std::memset(gain, 0, sizeof(float) * 4);
    const uint32_t nin = std::clamp<uint32_t>(v.cur_nch, 1, 8);
    bool any = false;
    for (int p = 0; p < kMaxPorts; ++p) {
        const auto& port = v.ports[p];
        if (!port.patched) continue;
        any = true;
        const float down = RouteGain(port.dest, 0);
        if (down <= 0.0f) continue;
        int mid = port.matrix;
        if (mid < 0 || mid >= kMaxMatrices || !v.matrix_set[mid]) mid = v.matrix_set[0] ? 0 : -1;
        float m[2][2];
        if (mid >= 0) FoldMatrix(v.matrices[mid], nin, m);
        else if (nin == 1) { m[0][0] = 1; m[0][1] = 1; m[1][0] = m[1][1] = 0; }
        else { m[0][0] = 1; m[0][1] = 0; m[1][0] = 0; m[1][1] = 1; }
        const float pv = port.volume * down;
        for (int i = 0; i < 2; ++i) for (int o = 0; o < 2; ++o) gain[i][o] += m[i][o] * pv;
    }
    float total = 0;
    for (int i = 0; i < 2; ++i) for (int o = 0; o < 2; ++o) total += std::fabs(gain[i][o]);
    if (!any || total == 0.0f) {
        if (any && Once("route-unmodelled")) Log("[NGS2] voices are patched but every route folded to zero gain; mixing direct to output (dest handles are unresolved)");
        float m[2][2];
        if (v.matrix_set[0]) FoldMatrix(v.matrices[0], nin, m);
        else if (nin == 1) { m[0][0] = 1; m[0][1] = 1; m[1][0] = m[1][1] = 0; }
        else { m[0][0] = 1; m[0][1] = 0; m[1][0] = 0; m[1][1] = 1; }
        std::memcpy(gain, m, sizeof(m));
    }
}

void MixVoice(Voice& v, float* bus, uint32_t frames, uint32_t sys_rate, uint32_t grain_for_timeout) {
    if (v.state == VState::Empty || v.state == VState::Paused) return;
    float gain[2][2];
    float vpeak = 0;
    for (uint32_t f = 0; f < frames; ++f) {
        if (v.blocks.empty()) {
            if (v.state == VState::Stopping || (v.had_blocks_since_play && ++v.starve_grains > 8u * grain_for_timeout) ||
                (!v.had_blocks_since_play && ++v.starve_grains > 96u * grain_for_timeout)) {
                ClearVoice(v);
            }
            break;
        }
        if (!v.cursor_init) {
            v.cursor_init = true;
            if (!InitBlock(v, sys_rate)) { v.blocks.pop_front(); v.cursor_init = false; v.pos = 0; --f; continue; }
            ComputeGains(v, gain);
        }
        if (f == 0) ComputeGains(v, gain);
        const Block& b = v.blocks.front();
        const uint32_t i0 = static_cast<uint32_t>(v.pos);
        const uint32_t i1 = std::min(i0 + 1, v.cur_frames - 1);
        const float fr = static_cast<float>(v.pos - i0);
        float s[2] = {0, 0};
        if (v.cur_kind == 1) {
            const float a = v.adpcm[i0] * (1.0f / 32768.0f), c = v.adpcm[i1] * (1.0f / 32768.0f);
            s[0] = a + (c - a) * fr;
        } else {
            const uint32_t bps = PcmBytes(v.cur_type), nch = v.cur_nch;
            for (uint32_t ch = 0; ch < std::min<uint32_t>(nch, 2); ++ch) {
                const float a = PcmSample(b.data + (static_cast<size_t>(i0) * nch + ch) * bps, v.cur_type);
                const float c = PcmSample(b.data + (static_cast<size_t>(i1) * nch + ch) * bps, v.cur_type);
                s[ch] = a + (c - a) * fr;
            }
        }
        float sg = 1.0f;
        if (v.state == VState::Stopping) {
            v.stop_gain -= 1.0f / 480.0f;
            if (v.stop_gain <= 0.0f) { ClearVoice(v); break; }
            sg = v.stop_gain;
        }
        const float l = (s[0] * gain[0][0] + s[1] * gain[1][0]) * sg;
        const float r = (s[0] * gain[0][1] + s[1] * gain[1][1]) * sg;
        bus[f * 2] += l;
        bus[f * 2 + 1] += r;
        vpeak = std::max(vpeak, std::max(std::fabs(l), std::fabs(r)));
        v.pos += v.cur_rate * static_cast<double>(v.pitch) / static_cast<double>(sys_rate);
        if (v.pos >= v.cur_frames) {
            const double carry = v.pos - v.cur_frames;
            ++v.repeats_done;
            if (b.repeats == kRepeatInfinite || v.repeats_done <= b.repeats) {
                v.pos = v.cur_start + carry;
                if (v.pos >= v.cur_frames) v.pos = v.cur_start;
                v.decoded_samples += v.cur_frames - v.cur_start;
            } else {
                v.decoded_samples += v.cur_frames - v.cur_start;
                v.decoded_bytes += b.size;
                v.last_user = b.user;
                v.last_data = b.data;
                v.blocks.pop_front();
                if (v.cb_fn != 0) G().pending_cb.push_back({v.cb_fn, v.cb_user, reinterpret_cast<std::uintptr_t>(&v), v.cb_flags});
                v.cursor_init = false;
                v.pos = carry;
                v.starve_grains = 0;
                if (v.blocks.empty()) v.pos = 0;
            }
        }
    }
    v.peak = vpeak;
}

uint32_t BufferBytesPerSample(uint32_t type) {
    const uint32_t b = PcmBytes(type);
    return b ? b : 4;
}
void WriteOut(uint8_t* dst, uint32_t type, uint32_t ch, uint32_t frames, const float* bus) {
    const uint32_t bps = BufferBytesPerSample(type);
    for (uint32_t f = 0; f < frames; ++f) {
        for (uint32_t c = 0; c < ch; ++c) {
            float x = c < 2 ? std::clamp(bus[f * 2 + c], -1.0f, 1.0f) : 0.0f;
            uint8_t* o = dst + (static_cast<size_t>(f) * ch + c) * bps;
            switch (type) {
                case kWavePcmI16Le: { int16_t v = static_cast<int16_t>(x * 32767.0f); std::memcpy(o, &v, 2); break; }
                case kWavePcmI16Be: { int16_t v = static_cast<int16_t>(x * 32767.0f); o[0] = uint8_t(v >> 8); o[1] = uint8_t(v); break; }
                case kWavePcmI8: *o = static_cast<uint8_t>(static_cast<int8_t>(x * 127.0f)); break;
                case kWavePcmU8: *o = static_cast<uint8_t>(x * 127.0f + 128.0f); break;
                case kWavePcmI32Le: { int32_t v = static_cast<int32_t>(static_cast<double>(x) * 2147483647.0); std::memcpy(o, &v, 4); break; }
                default: std::memcpy(o, &x, 4); break;
            }
        }
    }
}

uint32_t ClampU32(uint32_t v, uint32_t lo, uint32_t hi, uint32_t def) { return (v >= lo && v <= hi) ? v : def; }

void EnsureVoices(Rack& r, uint32_t n) {
    n = std::min(n, kMaxVoicesPerRack);
    while (r.voices.size() < n) {
        auto v = std::make_unique<Voice>();
        v->rack = &r;
        v->index = static_cast<uint32_t>(r.voices.size());
        G().voices.insert(reinterpret_cast<std::uintptr_t>(v.get()));
        r.voices.push_back(std::move(v));
    }
}

int32_t CreateSystem(const SysOptionLayout* opt, std::uintptr_t* handle) {
    if (handle == nullptr) return kErrInvalidOutAddress;
    auto* s = new System();
    if (opt != nullptr && Readable(opt, 8) && opt->size >= sizeof(SysOptionLayout) && Readable(opt, sizeof(SysOptionLayout))) {
        s->max_grain = ClampU32(opt->max_grain, 64, 8192, 512);
        s->grain = ClampU32(opt->num_grain, 64, 8192, 256);
        s->rate = ClampU32(opt->sample_rate, 8000, 192000, 48000);
    }
    const auto h = reinterpret_cast<std::uintptr_t>(s);
    {
        std::lock_guard lock(G().mtx);
        G().systems.insert(h);
        Log("[NGS2] system created: grain=%u maxgrain=%u rate=%u optsize=%zu", s->grain, s->max_grain, s->rate, opt ? opt->size : 0);
    }
    *handle = h;
    return kOk;
}

int32_t CreateRack(std::uintptr_t system_handle, uint32_t rack_id, const RackOptionLayout* opt, std::uintptr_t* handle) {
    if (handle == nullptr) return kErrInvalidOutAddress;
    std::lock_guard lock(G().mtx);
    System* s = FindSystem(system_handle);
    if (s == nullptr) return kErrInvalidSystemHandle;
    auto* r = new Rack();
    r->sys = s;
    r->id = rack_id;
    r->max_voices = IsSamplerRack(rack_id) ? 256 : 1;
    if (opt != nullptr && Readable(opt, sizeof(RackOptionLayout)) && opt->size >= 88) r->max_voices = ClampU32(opt->max_voices, 1, kMaxVoicesPerRack, r->max_voices);
    EnsureVoices(*r, r->max_voices);
    s->racks.push_back(r);
    const auto h = reinterpret_cast<std::uintptr_t>(r);
    G().racks.insert(h);
    Log("[NGS2] rack created: id=0x%x (%s) maxVoices=%u optsize=%zu", rack_id, RackName(rack_id), r->max_voices, opt ? opt->size : 0);
    if (!RackName(rack_id)[0] || std::strcmp(RackName(rack_id), "other") == 0) Log("[NGS2] UNSUPPORTED rack id 0x%x (voices accepted but produce no sound)", rack_id);
    *handle = h;
    return kOk;
}

void DestroyRack(Rack* r) {
    auto& g = G();
    for (auto& v : r->voices) g.voices.erase(reinterpret_cast<std::uintptr_t>(v.get()));
    g.racks.erase(reinterpret_cast<std::uintptr_t>(r));
    auto& vec = r->sys->racks;
    vec.erase(std::remove(vec.begin(), vec.end(), r), vec.end());
    delete r;
}

uint32_t RackBufferSize(uint32_t rack_id, const RackOptionLayout* opt) {
    uint32_t nv = IsSamplerRack(rack_id) ? 256 : 1;
    if (opt != nullptr && Readable(opt, sizeof(RackOptionLayout)) && opt->size >= 88) nv = ClampU32(opt->max_voices, 1, kMaxVoicesPerRack, nv);
    return 0x1000 + nv * 0x200;
}

}  // namespace

extern "C" {

int APS5_VABI sceNgs2CalcWaveformBlock(const Ngs2WaveformFormat* format, uint32_t sample_pos, uint32_t num_samples, Ngs2WaveformBlock* block) {
    if (block == nullptr || format == nullptr) return kErrInvalidParam;
    std::memset(block, 0, sizeof(*block));
    const uint32_t bps = PcmBytes(format->waveform_type);
    const uint32_t ch = std::max<uint32_t>(format->num_channels, 1);
    if (bps != 0) {
        block->data_offset = sample_pos * bps * ch;
        block->data_size = num_samples * bps * ch;
    } else {
        block->data_offset = sample_pos / 28 * 16;
        block->data_size = (num_samples + 27) / 28 * 16;
        block->num_skip_samples = sample_pos % 28;
    }
    block->num_samples = num_samples;
    return kOk;
}

int APS5_VABI sceNgs2ParseWaveformData(const void* data, size_t data_size, Ngs2WaveformInfo* info) {
    if (info == nullptr || data == nullptr || !Readable(data, std::min<size_t>(data_size, 64))) return kErrInvalidParam;
    std::memset(info, 0, sizeof(*info));
    const auto* d = static_cast<const uint8_t*>(data);
    info->format.num_channels = 1;
    info->format.sample_rate = 48000;
    info->num_audio_unit_samples = 1;
    info->num_audio_unit_per_frame = 1;
    info->num_audio_frame_samples = 1;
    if (data_size >= 12 && !std::memcmp(d, "RIFF", 4) && !std::memcmp(d + 8, "WAVE", 4)) {
        uint32_t tag = 0, bits = 16, ch = 1, rate = 48000, loop_b = 0, loop_e = 0, fact = 0;
        uint64_t off = 12, doff = 0, dsize = 0;
        while (off + 8 <= data_size) {
            const uint32_t csz = Rd<uint32_t>(d + off + 4);
            const uint8_t* c = d + off + 8;
            if (!std::memcmp(d + off, "fmt ", 4) && off + 8 + 16 <= data_size) {
                tag = Rd<uint16_t>(c); ch = Rd<uint16_t>(c + 2); rate = Rd<uint32_t>(c + 4); bits = Rd<uint16_t>(c + 14);
                if (tag == 0xFFFE && csz >= 26 && off + 8 + 26 <= data_size) tag = 0x10000 | Rd<uint16_t>(c + 24);
            } else if (!std::memcmp(d + off, "data", 4)) {
                doff = off + 8;
                dsize = std::min<uint64_t>(csz, data_size - doff);
            } else if (!std::memcmp(d + off, "fact", 4) && csz >= 4) {
                fact = Rd<uint32_t>(c);
            } else if (!std::memcmp(d + off, "smpl", 4) && csz >= 36 + 24 && Rd<uint32_t>(c + 28) >= 1 && off + 8 + 36 + 24 <= data_size) {
                loop_b = Rd<uint32_t>(c + 36 + 8);
                loop_e = Rd<uint32_t>(c + 36 + 12);
            }
            off += 8 + static_cast<uint64_t>(csz) + (csz & 1);
        }
        uint32_t type = 0;
        if ((tag & 0xFFFF) == 1) type = bits == 8 ? kWavePcmU8 : bits == 16 ? kWavePcmI16Le : bits == 32 ? kWavePcmI32Le : 0;
        else if ((tag & 0xFFFF) == 3 && bits == 32) type = kWavePcmF32Le;
        if (type == 0) {
            std::lock_guard lock(G().mtx);
            if (Once("parse-unsupported")) Log("[NGS2] ParseWaveformData: unsupported RIFF format tag 0x%x bits %u (ATRAC9/ADPCM WAV not parsed)", tag, bits);
            return kErrInvalidParam;
        }
        const uint32_t fsz = PcmBytes(type) * std::max<uint32_t>(ch, 1);
        info->format.waveform_type = type;
        info->format.num_channels = ch;
        info->format.sample_rate = rate;
        info->data_offset = static_cast<uint32_t>(doff);
        info->data_size = static_cast<uint32_t>(dsize);
        info->num_samples = fact ? fact : static_cast<uint32_t>(dsize / fsz);
        info->loop_begin_position = loop_b;
        info->loop_end_position = loop_e;
        info->audio_unit_size = fsz;
        info->audio_frame_size = fsz;
    } else if (data_size >= 0x30 && !std::memcmp(d, "VAGp", 4)) {
        const uint32_t rate = (uint32_t(d[0x10]) << 24) | (uint32_t(d[0x11]) << 16) | (uint32_t(d[0x12]) << 8) | d[0x13];
        info->format.waveform_type = 0x40;
        info->format.num_channels = 1;
        info->format.sample_rate = rate ? rate : 48000;
        info->data_offset = 0x30;
        info->data_size = static_cast<uint32_t>((data_size - 0x30) / 16 * 16);
        info->num_samples = info->data_size / 16 * 28;
        info->audio_unit_size = 16;
        info->num_audio_unit_samples = 28;
        info->audio_frame_size = 16;
        info->num_audio_frame_samples = 28;
    } else {
        std::lock_guard lock(G().mtx);
        if (Once("parse-unknown")) Log("[NGS2] ParseWaveformData: unknown container, first bytes %s", Hex(d, std::min<size_t>(data_size, 16)).c_str());
        return kErrInvalidParam;
    }
    info->num_blocks = 1;
    info->block[0].data_offset = info->data_offset;
    info->block[0].data_size = info->data_size;
    info->block[0].num_samples = info->num_samples;
    return kOk;
}

struct GVec { float x, y, z; };
struct GListenerParam { GVec pos, front, up, vel; float sound_speed; uint32_t reserved[2]; };
struct GListenerWork { float pos[3], right[3], up[3], front[3], vel[3], sound_speed; uint32_t coordinate; };
struct GSourceParam {
    GVec pos, vel, dir; float cone[4]; uint32_t rolloff_model; float max_dist, rolloff_factor, ref_dist;
    float doppler, fbw, lfe, max_level, min_level, radius; uint32_t num_speakers, matrix_format, reserved[2];
};

int APS5_VABI sceNgs2GeomResetListenerParam(Ngs2GeomListenerParam* out) {
    if (out == nullptr) return kErrInvalidParam;
    auto* p = reinterpret_cast<GListenerParam*>(out);
    std::memset(p, 0, sizeof(GListenerParam));
    p->front = {0, 0, 1};
    p->up = {0, 1, 0};
    p->sound_speed = 343.0f;
    return kOk;
}
int APS5_VABI sceNgs2GeomResetSourceParam(Ngs2GeomSourceParam* out) {
    if (out == nullptr) return kErrInvalidParam;
    auto* p = reinterpret_cast<GSourceParam*>(out);
    std::memset(p, 0, sizeof(GSourceParam));
    p->dir = {0, 0, 1};
    p->cone[0] = 1.0f; p->cone[1] = 360.0f; p->cone[2] = 1.0f; p->cone[3] = 360.0f;
    p->max_dist = 1000000.0f; p->rolloff_factor = 1.0f; p->ref_dist = 1.0f;
    p->doppler = 1.0f; p->fbw = 1.0f; p->lfe = 1.0f; p->max_level = 1.0f; p->min_level = 0.0f;
    p->num_speakers = 2; p->matrix_format = 2;
    return kOk;
}
int APS5_VABI sceNgs2GeomCalcListener(const Ngs2GeomListenerParam* param, Ngs2GeomListenerWork* out_work, uint32_t flags) {
    if (param == nullptr || out_work == nullptr) return kErrInvalidParam;
    const auto* p = reinterpret_cast<const GListenerParam*>(param);
    auto* w = reinterpret_cast<GListenerWork*>(out_work);
    std::memset(out_work, 0, sizeof(GListenerWork));
    auto norm = [](GVec v) { float l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); return l > 1e-6f ? GVec{v.x / l, v.y / l, v.z / l} : GVec{0, 0, 1}; };
    const GVec f = norm(p->front);
    GVec u = norm(p->up);
    GVec r = {u.y * f.z - u.z * f.y, u.z * f.x - u.x * f.z, u.x * f.y - u.y * f.x};
    r = norm(r);
    u = {f.y * r.z - f.z * r.y, f.z * r.x - f.x * r.z, f.x * r.y - f.y * r.x};
    w->pos[0] = p->pos.x; w->pos[1] = p->pos.y; w->pos[2] = p->pos.z;
    w->right[0] = r.x; w->right[1] = r.y; w->right[2] = r.z;
    w->up[0] = u.x; w->up[1] = u.y; w->up[2] = u.z;
    w->front[0] = f.x; w->front[1] = f.y; w->front[2] = f.z;
    w->vel[0] = p->vel.x; w->vel[1] = p->vel.y; w->vel[2] = p->vel.z;
    w->sound_speed = p->sound_speed > 0 ? p->sound_speed : 343.0f;
    w->coordinate = flags & 1u;
    return kOk;
}
int APS5_VABI sceNgs2GeomApply(const Ngs2GeomListenerWork* listener, const Ngs2GeomSourceParam* source, Ngs2GeomAttribute* out_attrib, uint32_t flags) {
    (void)flags;
    if (listener == nullptr || source == nullptr || out_attrib == nullptr) return kErrInvalidParam;
    const auto* w = reinterpret_cast<const GListenerWork*>(listener);
    const auto* s = reinterpret_cast<const GSourceParam*>(source);
    auto* out = reinterpret_cast<float*>(out_attrib);
    std::memset(out_attrib, 0, sizeof(Ngs2GeomAttribute));
    float* level = out + 1;
    float d[3] = {s->pos.x - w->pos[0], s->pos.y - w->pos[1], s->pos.z - w->pos[2]};
    const float dist = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    const float lx = d[0] * w->right[0] + d[1] * w->right[1] + d[2] * w->right[2];
    const float lz = d[0] * w->front[0] + d[1] * w->front[1] + d[2] * w->front[2];
    float att = 1.0f;
    const float ref = s->ref_dist > 0 ? s->ref_dist : 1.0f;
    if (dist > ref) att = ref / (ref + s->rolloff_factor * (std::min(dist, s->max_dist > 0 ? s->max_dist : dist) - ref));
    const float maxl = s->max_level > 0 ? s->max_level : 1.0f;
    att = std::clamp(att * maxl, s->min_level, std::max(maxl, s->min_level));
    float pan = 0.0f;
    if (dist > 1e-4f) pan = std::clamp(lx / std::sqrt(lx * lx + lz * lz + 1e-12f), -1.0f, 1.0f);
    if (dist <= std::max(s->radius, 1e-4f)) pan = 0.0f;
    const float ang = (pan + 1.0f) * 0.25f * 3.14159265f;
    const float gl = std::cos(ang) * 1.41421356f * 0.7071f * att * (s->fbw > 0 ? s->fbw : 1.0f);
    const float gr = std::sin(ang) * 1.41421356f * 0.7071f * att * (s->fbw > 0 ? s->fbw : 1.0f);
    level[0] = gl; level[1] = gr;
    level[2] = 0.0f; level[3] = gr;
    level[8] = 0.0f; level[9] = gr;
    float pitch = 1.0f;
    if (s->doppler > 0 && dist > 1e-4f) {
        const float c = w->sound_speed > 0 ? w->sound_speed : 343.0f;
        const float vl = (w->vel[0] * d[0] + w->vel[1] * d[1] + w->vel[2] * d[2]) / dist;
        const float vs = (s->vel.x * d[0] + s->vel.y * d[1] + s->vel.z * d[2]) / dist;
        const float denom = c - std::min(vs, c * 0.9f) * s->doppler;
        if (denom > 1.0f) pitch = std::clamp((c - std::min(vl, c * 0.9f) * s->doppler) / denom, 0.5f, 2.0f);
    }
    out[0] = pitch;
    return kOk;
}

struct PanWorkL { float angles[8]; float unit; uint32_t n; };
struct PanParamL { float angle, distance, fbw, lfe; };
int APS5_VABI sceNgs2PanInit(Ngs2PanWork* work, const float* speaker_angles, float unit_angle, uint32_t num_speakers) {
    if (work == nullptr) return kErrInvalidParam;
    std::memset(work, 0, sizeof(Ngs2PanWork));
    auto* w = reinterpret_cast<PanWorkL*>(work);
    w->unit = unit_angle;
    w->n = std::min<uint32_t>(num_speakers, 8);
    if (speaker_angles != nullptr && Readable(speaker_angles, w->n * 4)) for (uint32_t i = 0; i < w->n; ++i) w->angles[i] = speaker_angles[i];
    return kOk;
}
int APS5_VABI sceNgs2PanGetVolumeMatrix(Ngs2PanWork* work, const Ngs2PanParam* params, uint32_t num_params, uint32_t matrix_format, float* out) {
    if (work == nullptr || (num_params && (params == nullptr || out == nullptr))) return kErrInvalidParam;
    const auto* w = reinterpret_cast<const PanWorkL*>(work);
    const auto* pp = reinterpret_cast<const PanParamL*>(params);
    const uint32_t ns = w->n ? w->n : (matrix_format ? std::min<uint32_t>(matrix_format, 8) : 2);
    const float pi2 = 6.28318531f;
    for (uint32_t p = 0; p < num_params; ++p) {
        float* row = out + static_cast<size_t>(p) * ns;
        for (uint32_t i = 0; i < ns; ++i) row[i] = 0;
        const float a = pp[p].angle;
        float best0 = 1e9f, best1 = 1e9f;
        int i0 = -1, i1 = -1;
        for (uint32_t i = 0; i < ns; ++i) {
            float diff = std::fmod(a - (w->n ? w->angles[i] : (i == 0 ? 0.5236f : -0.5236f)), pi2);
            if (diff > pi2 / 2) diff -= pi2;
            if (diff < -pi2 / 2) diff += pi2;
            const float ad = std::fabs(diff);
            if (ad < best0) { best1 = best0; i1 = i0; best0 = ad; i0 = static_cast<int>(i); }
            else if (ad < best1) { best1 = ad; i1 = static_cast<int>(i); }
        }
        const float lvl = pp[p].fbw > 0 ? pp[p].fbw : 1.0f;
        if (i0 >= 0 && i1 >= 0 && best0 + best1 > 1e-6f) {
            const float t = best0 / (best0 + best1);
            row[i0] = std::cos(t * 1.5707963f) * lvl;
            row[i1] = std::sin(t * 1.5707963f) * lvl;
        } else if (i0 >= 0) row[i0] = lvl;
        if (ns >= 4 && pp[p].lfe > 0) row[3] = pp[p].lfe;
    }
    return kOk;
}

int APS5_VABI sceNgs2SystemResetOption(Ngs2SystemOption* option) {
    if (option == nullptr) return kErrInvalidParam;
    auto* o = reinterpret_cast<SysOptionLayout*>(option);
    std::memset(o, 0, sizeof(SysOptionLayout));
    o->size = sizeof(SysOptionLayout);
    o->max_grain = 512;
    o->num_grain = 256;
    o->sample_rate = 48000;
    return kOk;
}
int APS5_VABI sceNgs2SystemQueryBufferSize(const Ngs2SystemOption* option, Ngs2ContextBufferInfo* buffer_info) {
    (void)option;
    if (buffer_info == nullptr) return kErrInvalidOutAddress;
    std::memset(buffer_info, 0, sizeof(*buffer_info));
    buffer_info->host_buffer_size = 0x4000;
    return kOk;
}
int APS5_VABI sceNgs2SystemCreate(const Ngs2SystemOption* option, const Ngs2ContextBufferInfo* buffer_info, uintptr_t* handle) {
    (void)buffer_info;
    return CreateSystem(reinterpret_cast<const SysOptionLayout*>(option), handle);
}
int APS5_VABI sceNgs2SystemCreateWithAllocator(const Ngs2SystemOption* option, const Ngs2BufferAllocator* allocator, uintptr_t* handle) {
    (void)allocator;
    return CreateSystem(reinterpret_cast<const SysOptionLayout*>(option), handle);
}
int APS5_VABI sceNgs2SystemDestroy(uintptr_t system_handle, Ngs2ContextBufferInfo* buffer_info) {
    if (buffer_info != nullptr && Writable(buffer_info, sizeof(*buffer_info))) std::memset(buffer_info, 0, sizeof(*buffer_info));
    std::lock_guard lock(G().mtx);
    System* s = FindSystem(system_handle);
    if (s == nullptr) return kErrInvalidSystemHandle;
    while (!s->racks.empty()) DestroyRack(s->racks.back());
    G().systems.erase(system_handle);
    delete s;
    return kOk;
}
int APS5_VABI sceNgs2SystemGetInfo(uintptr_t system_handle, Ngs2SystemInfo* info, size_t info_size) {
    std::lock_guard lock(G().mtx);
    System* s = FindSystem(system_handle);
    if (s == nullptr) return kErrInvalidSystemHandle;
    if (info == nullptr || info_size == 0 || info_size > 0x1000 || !Writable(info, info_size)) return kErrInvalidOutAddress;
    std::memset(info, 0, info_size);
    if (info_size >= 0x98) {
        auto* b = reinterpret_cast<uint8_t*>(info);
        const uint64_t h = system_handle;
        std::memcpy(b + 64, &h, 8);
        const uint32_t vals[] = {static_cast<uint32_t>(h), 64u, s->max_grain, 0u, static_cast<uint32_t>(s->racks.size())};
        std::memcpy(b + 136, vals, sizeof(vals));
    }
    return kOk;
}
int APS5_VABI sceNgs2SystemSetGrainSamples(uintptr_t system_handle, uint32_t num_samples) {
    std::lock_guard lock(G().mtx);
    System* s = FindSystem(system_handle);
    if (s == nullptr) return kErrInvalidSystemHandle;
    if (num_samples >= 64 && num_samples <= 8192) s->grain = num_samples;
    return kOk;
}
int APS5_VABI sceNgs2SystemSetSampleRate(uintptr_t system_handle, uint32_t rate) {
    std::lock_guard lock(G().mtx);
    System* s = FindSystem(system_handle);
    if (s == nullptr) return kErrInvalidSystemHandle;
    if (rate >= 8000 && rate <= 192000) s->rate = rate;
    return kOk;
}
int APS5_VABI sceNgs2SystemLock(uintptr_t system_handle) {
    std::lock_guard lock(G().mtx);
    return FindSystem(system_handle) ? kOk : kErrInvalidSystemHandle;
}
int APS5_VABI sceNgs2SystemUnlock(uintptr_t system_handle) {
    std::lock_guard lock(G().mtx);
    return FindSystem(system_handle) ? kOk : kErrInvalidSystemHandle;
}
int APS5_VABI sceNgs2SystemSetUserData(uintptr_t system_handle, uintptr_t user_data) {
    std::lock_guard lock(G().mtx);
    System* s = FindSystem(system_handle);
    if (s == nullptr) return kErrInvalidSystemHandle;
    s->user_data = user_data;
    return kOk;
}
int APS5_VABI sceNgs2SystemGetUserData(uintptr_t system_handle, uintptr_t* user_data) {
    std::lock_guard lock(G().mtx);
    System* s = FindSystem(system_handle);
    if (s == nullptr) return kErrInvalidSystemHandle;
    if (user_data == nullptr) return kErrInvalidOutAddress;
    *user_data = s->user_data;
    return kOk;
}

int APS5_VABI sceNgs2RackQueryBufferSize(uint32_t rack_id, const Ngs2RackOption* option, Ngs2ContextBufferInfo* buffer_info) {
    if (buffer_info == nullptr) return kErrInvalidOutAddress;
    std::memset(buffer_info, 0, sizeof(*buffer_info));
    buffer_info->host_buffer_size = RackBufferSize(rack_id, reinterpret_cast<const RackOptionLayout*>(option));
    return kOk;
}
int APS5_VABI sceNgs2RackCreate(uintptr_t system_handle, uint32_t rack_id, const Ngs2RackOption* option, const Ngs2ContextBufferInfo* buffer_info, uintptr_t* handle) {
    (void)buffer_info;
    return CreateRack(system_handle, rack_id, reinterpret_cast<const RackOptionLayout*>(option), handle);
}
int APS5_VABI sceNgs2RackCreateWithAllocator(uintptr_t system_handle, uint32_t rack_id, const Ngs2RackOption* option, const Ngs2BufferAllocator* allocator, uintptr_t* handle) {
    (void)allocator;
    return CreateRack(system_handle, rack_id, reinterpret_cast<const RackOptionLayout*>(option), handle);
}
int APS5_VABI sceNgs2RackDestroy(uintptr_t rack_handle, Ngs2ContextBufferInfo* buffer_info) {
    if (buffer_info != nullptr && Writable(buffer_info, sizeof(*buffer_info))) std::memset(buffer_info, 0, sizeof(*buffer_info));
    std::lock_guard lock(G().mtx);
    Rack* r = FindRack(rack_handle);
    if (r == nullptr) return kErrInvalidRackHandle;
    DestroyRack(r);
    return kOk;
}
int APS5_VABI sceNgs2RackGetVoiceHandle(uintptr_t rack_handle, uint32_t voice_id, uintptr_t* handle) {
    if (handle == nullptr) return kErrInvalidOutAddress;
    std::lock_guard lock(G().mtx);
    Rack* r = FindRack(rack_handle);
    if (r == nullptr) return kErrInvalidRackHandle;
    if (voice_id >= kMaxVoicesPerRack) return kErrInvalidParam;
    if (voice_id >= r->voices.size()) {
        if (Once("voice-beyond-max" + std::to_string(r->id))) Log("[NGS2] voice id %u >= maxVoices %u on rack 0x%x; growing rack", voice_id, r->max_voices, r->id);
        EnsureVoices(*r, voice_id + 1);
    }
    *handle = reinterpret_cast<uintptr_t>(r->voices[voice_id].get());
    return kOk;
}
int APS5_VABI sceNgs2RackLock(uintptr_t rack_handle) {
    std::lock_guard lock(G().mtx);
    return FindRack(rack_handle) ? kOk : kErrInvalidRackHandle;
}
int APS5_VABI sceNgs2RackUnlock(uintptr_t rack_handle) {
    std::lock_guard lock(G().mtx);
    return FindRack(rack_handle) ? kOk : kErrInvalidRackHandle;
}
int APS5_VABI sceNgs2RackSetUserData(uintptr_t rack_handle, uintptr_t user_data) {
    std::lock_guard lock(G().mtx);
    Rack* r = FindRack(rack_handle);
    if (r == nullptr) return kErrInvalidRackHandle;
    r->user_data = user_data;
    return kOk;
}
int APS5_VABI sceNgs2RackGetUserData(uintptr_t rack_handle, uintptr_t* user_data) {
    std::lock_guard lock(G().mtx);
    Rack* r = FindRack(rack_handle);
    if (r == nullptr) return kErrInvalidRackHandle;
    if (user_data == nullptr) return kErrInvalidOutAddress;
    *user_data = r->user_data;
    return kOk;
}

int APS5_VABI sceNgs2VoiceControl(uintptr_t voice_handle, const Ngs2VoiceParamHeader* param_list) {
    std::lock_guard lock(G().mtx);
    Voice* v = FindVoice(voice_handle);
    if (v == nullptr) return kErrInvalidVoiceHandle;
    ++G().diag.calls_control;
    if (param_list != nullptr) WalkParams(*v, reinterpret_cast<const uint8_t*>(param_list), "VoiceControl");
    Report();
    return kOk;
}

bool RunCommandArray(Voice& v, const uint8_t* p, uint32_t num_commands) {
    auto& d = G().diag;
    const uint32_t first = Rd<uint32_t>(p);
    if ((first & 0xffffffu) > 7) return false;
    const uint32_t n = std::min<uint32_t>(num_commands == 0 ? 1 : num_commands, 4096);
    if (!Readable(p, static_cast<size_t>(n) * 12)) return false;
    for (uint32_t i = 0; i < n; ++i) {
        const uint8_t* c = p + static_cast<size_t>(i) * 12;
        const uint32_t id = Rd<uint32_t>(c);
        const uint8_t type = c[5];
        const uint16_t count = Rd<uint16_t>(c + 6);
        const uint32_t value = Rd<uint32_t>(c + 8);
        const uint32_t op = id & 0xffffffu, index = id >> 24;
        ++d.cmd_count[op];
        switch (op) {
            case 2: ApplyEvent(v, value); break;
            case 5: {
                const float* lv = reinterpret_cast<const float*>(static_cast<uintptr_t>(value));
                if (index < kMaxMatrices && count <= kMaxMatrixLevels && Readable(lv, static_cast<size_t>(count) * 4)) {
                    v.matrices[index].assign(lv, lv + count);
                    v.matrix_set[index] = true;
                }
                break;
            }
            case 6:
                if (index < kMaxPorts) {
                    const float lvl = (type == 1 || type == 0) ? Rd<float>(c + 8) : static_cast<float>(static_cast<int32_t>(value)) / 4096.0f;
                    if (std::isfinite(lvl)) v.ports[index].volume = lvl;
                }
                break;
            case 7:
                if (index < kMaxPorts) v.ports[index].matrix = static_cast<int32_t>(value);
                break;
            default:
                if (Once("runcmd" + std::to_string(op))) Log("[NGS2] UNIMPLEMENTED RunCommands op 0x%x (id=0x%08x type=%u count=%u value=0x%x)", op, id, type, count, value);
                ++d.param_unimpl[0x40000000u | op];
                break;
        }
    }
    return true;
}

int APS5_VABI sceNgs2VoiceRunCommands(uintptr_t voice_handle, const void* commands, uint32_t num_commands, uint32_t flags) {
    std::lock_guard lock(G().mtx);
    Voice* v = FindVoice(voice_handle);
    if (v == nullptr) return kErrInvalidVoiceHandle;
    auto& d = G().diag;
    ++d.calls_runcmd;
    if (commands != nullptr && Readable(commands, 12)) {
        const auto* p = static_cast<const uint8_t*>(commands);
        if (d.calls_runcmd <= 6) Log("[NGS2] VoiceRunCommands #%llu voice=%u/rack 0x%x cmds=%u flags=0x%x bytes=%s", (unsigned long long)d.calls_runcmd, v->index, v->rack->id, num_commands, flags, Hex(p, Readable(p, 48) ? 48 : 12).c_str());
        if (!RunCommandArray(*v, p, num_commands)) {
            const uint32_t id = Rd<uint32_t>(p + 4);
            const uint16_t size = Rd<uint16_t>(p);
            ++d.cmd_count[id];
            const uint32_t fam = id >> 16;
            if (size >= 8 && size <= 0x4000 && (fam == 0 || fam == 0x1000 || IsSamplerRack(fam) || fam >= 0x4000)) WalkParams(*v, p, "RunCommands");
            else if (Once("runcmd-unparsed")) Log("[NGS2] RunCommands buffer is neither a command array nor a param list (word0=0x%08x id=0x%08x size=%u); NOT IMPLEMENTED", Rd<uint32_t>(p), id, size);
        }
    }
    Report();
    return kOk;
}

int APS5_VABI sceNgs2VoiceGetState(uintptr_t voice_handle, Ngs2VoiceState* state, size_t state_size) {
    std::lock_guard lock(G().mtx);
    Voice* v = FindVoice(voice_handle);
    if (v == nullptr) return kErrInvalidVoiceHandle;
    ++G().diag.calls_getstate;
    if (state == nullptr || state_size == 0) return kOk;
    if (state_size > 0x1000 || !Writable(state, state_size)) return kErrInvalidOutAddress;
    std::memset(state, 0, state_size);
    uint32_t flags = 0;
    switch (v->state) {
        case VState::Empty: flags = 0; break;
        case VState::Playing: flags = 0x3; break;
        case VState::Paused: flags = 0x5; break;
        case VState::Stopping: flags = 0xb; break;
    }
    auto* b = reinterpret_cast<uint8_t*>(state);
    std::memcpy(b, &flags, 4);
    const uint64_t user = v->blocks.empty() ? v->last_user : v->blocks.front().user;
    const void* wd = v->blocks.empty() ? v->last_data : v->blocks.front().data;
    if (v->rack->id == kRackCustomSampler && state_size >= 48) {
        std::memcpy(b + 8, &wd, 8);
        std::memcpy(b + 16, &v->decoded_samples, 8);
        std::memcpy(b + 24, &v->decoded_bytes, 8);
        std::memcpy(b + 32, &user, 8);
    } else if (v->rack->id == kRackSampler && state_size >= 56) {
        const float env = v->state == VState::Empty ? 0.0f : 1.0f;
        std::memcpy(b + 8, &env, 4);
        std::memcpy(b + 12, &v->peak, 4);
        std::memcpy(b + 24, &v->decoded_samples, 8);
        std::memcpy(b + 32, &v->decoded_bytes, 8);
        std::memcpy(b + 40, &user, 8);
        std::memcpy(b + 48, &wd, 8);
    }
    Report();
    return kOk;
}
int APS5_VABI sceNgs2VoiceGetStateFlags(uintptr_t voice_handle, uint32_t* state_flags) {
    std::lock_guard lock(G().mtx);
    Voice* v = FindVoice(voice_handle);
    if (v == nullptr) return kErrInvalidVoiceHandle;
    if (state_flags == nullptr) return kErrInvalidOutAddress;
    *state_flags = v->state == VState::Empty ? 0u : v->state == VState::Playing ? 0x3u : v->state == VState::Paused ? 0x5u : 0xbu;
    return kOk;
}
int APS5_VABI sceNgs2VoiceGetOwner(uintptr_t voice_handle, uintptr_t* rack_handle, uint32_t* voice_id) {
    std::lock_guard lock(G().mtx);
    Voice* v = FindVoice(voice_handle);
    if (v == nullptr) return kErrInvalidVoiceHandle;
    if (rack_handle) *rack_handle = reinterpret_cast<uintptr_t>(v->rack);
    if (voice_id) *voice_id = v->index;
    return kOk;
}

int APS5_VABI sceNgs2SystemRender(uintptr_t system_handle, const Ngs2RenderBufferInfo* buffer_info, uint32_t num_buffer_info) {
    uint32_t grain = 256, rate = 48000;
    int32_t result = kOk;
    {
        std::lock_guard lock(G().mtx);
        auto& g = G();
        System* s = FindSystem(system_handle);
        if (s == nullptr) {
            result = kErrInvalidSystemHandle;
        } else {
            grain = s->grain;
            rate = s->rate;
            ++s->renders;
            ++g.diag.calls_render;
            if (g.diag.calls_render <= 3) {
                Log("[NGS2] render #%llu: %u buffer(s), grain=%u rate=%u", (unsigned long long)g.diag.calls_render, num_buffer_info, grain, rate);
                for (uint32_t i = 0; i < num_buffer_info && buffer_info != nullptr && i < 4; ++i)
                    Log("[NGS2]   buffer[%u]: ptr=%p size=%zu type=0x%x channels=%u", i, buffer_info[i].buffer, buffer_info[i].buffer_size, buffer_info[i].waveform_type, buffer_info[i].num_channels);
            }
            std::vector<float> bus(static_cast<size_t>(grain) * 2, 0.0f);
            uint32_t active = 0;
            for (Rack* r : s->racks) {
                if (!IsSamplerRack(r->id)) continue;
                for (auto& vp : r->voices) {
                    Voice& v = *vp;
                    if (v.state == VState::Empty) continue;
                    ++active;
                    MixVoice(v, bus.data(), grain, rate, std::max<uint32_t>(1, 1));
                }
            }
            float peak = 0;
            for (float x : bus) peak = std::max(peak, std::fabs(x));
            for (float& x : bus) {
                const float a = std::fabs(x);
                if (a > 0.9f) x = std::copysign(0.9f + 0.1f * std::tanh((a - 0.9f) * 10.0f), x);
            }
            g.diag.peak_interval = std::max(g.diag.peak_interval, peak);
            g.diag.peak_max = std::max(g.diag.peak_max, peak);
            g.diag.active_max = std::max(g.diag.active_max, active);
            if (buffer_info != nullptr) {
                for (uint32_t i = 0; i < num_buffer_info; ++i) {
                    const auto& bi = buffer_info[i];
                    if (bi.buffer == nullptr || bi.buffer_size == 0 || bi.buffer_size > (64u << 20) || !Writable(bi.buffer, bi.buffer_size)) continue;
                    auto* dst = static_cast<uint8_t*>(bi.buffer);
                    std::memset(dst, 0, bi.buffer_size);
                    if (i != 0) continue;
                    const uint32_t ch = bi.num_channels >= 1 && bi.num_channels <= 8 ? bi.num_channels : 2;
                    const uint32_t bps = BufferBytesPerSample(bi.waveform_type);
                    const uint32_t frames = std::min<uint64_t>(grain, bi.buffer_size / (static_cast<uint64_t>(ch) * bps));
                    if (Once("rendertype" + std::to_string(bi.waveform_type)) && bi.waveform_type != kWavePcmF32Le && bi.waveform_type != 0)
                        Log("[NGS2] render buffer waveform type 0x%x written as %s", bi.waveform_type, PcmBytes(bi.waveform_type) ? "that PCM format" : "F32 (unknown type)");
                    WriteOut(dst, bi.waveform_type, ch, frames, bus.data());
                }
            }
            Report();
        }
    }
    std::vector<Globals::PendingCb> cbs;
    { std::lock_guard lock(G().mtx); cbs.swap(G().pending_cb); }
    for (const auto& c : cbs) {
        uint8_t info[0x40] = {};
        std::memcpy(info + 0x00, &c.user, 8);
        info[0x10] = static_cast<uint8_t>(c.flags | 1u);
        std::memcpy(info + 0x18, &c.handle, 8);
        reinterpret_cast<void(APS5_VABI*)(void*)>(c.fn)(info);
    }
    {
        static std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now();
        const auto period = std::chrono::microseconds(static_cast<int64_t>(grain) * 1000000 / rate);
        deadline += period;
        const auto now = std::chrono::steady_clock::now();
        if (deadline + std::chrono::milliseconds(50) < now) deadline = now;
        while (std::chrono::steady_clock::now() < deadline) {
            if (deadline - std::chrono::steady_clock::now() > std::chrono::milliseconds(2)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            else std::this_thread::yield();
        }
    }
    return result;
}

}
