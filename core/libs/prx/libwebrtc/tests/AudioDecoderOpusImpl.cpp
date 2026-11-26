#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/GuestStringStorage.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <opus.h>
#include <source_location>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "AudioDecoderOpusImpl check failed at line %u\n", location.line());
    std::abort();
}

template<typename TException, typename TAction>
void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const TException&) {
        return;
    }
    Require(false, location);
}

std::unordered_set<void*> allocations;
std::size_t failAfterAllocations = static_cast<std::size_t>(-1);

void* APS5_VABI TestAllocate(std::size_t size) {
    if (failAfterAllocations == 0)
        return nullptr;
    if (failAfterAllocations != static_cast<std::size_t>(-1))
        --failAfterAllocations;
    auto* p = std::malloc(size);
    Require(p != nullptr);
    allocations.insert(p);
    return p;
}

void APS5_VABI TestFree(void* p) {
    if (!p) return;
    Require(allocations.erase(p) == 1);
    std::free(p);
}

void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) {
    auto* p = std::calloc(count, size);
    Require(p != nullptr);
    allocations.insert(p);
    return p;
}

void* APS5_VABI TestReallocate(void* p, std::size_t size) {
    if (p) Require(allocations.erase(p) == 1);
    auto* next = std::realloc(p, size);
    Require(next != nullptr);
    allocations.insert(next);
    return next;
}

void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return TestAllocate(size); }
void* APS5_VABI TestRealign(void* p, std::size_t size, std::size_t) { return TestReallocate(p, size); }
int APS5_VABI TestPosixAlign(void** p, std::size_t, std::size_t size) {
    *p = TestAllocate(size);
    return *p ? 0 : 12;
}

struct FakeFieldTrials {
    void* const* vtable;
    std::string value;
    bool useHeap = false;
    std::string lastKey;
};

void APS5_VABI FakeFieldTrialsD1(FakeFieldTrials*) {}
void APS5_VABI FakeFieldTrialsD0(FakeFieldTrials* self) { delete self; }

WebRtcGuestString* APS5_VABI FakeFieldTrialsLookup(WebRtcGuestString* out, const FakeFieldTrials* self, const char* key, std::size_t keySize) {
    auto* mutableSelf = const_cast<FakeFieldTrials*>(self);
    mutableSelf->lastKey.assign(key, keySize);
    out->reserved = 0;
    const std::size_t len = self->value.size();
    if (!self->useHeap && len <= WebRtcGuestStringInlineCapacity) {
        out->capacity = WebRtcGuestStringInlineCapacity;
        out->size = len;
        std::memcpy(out->buffer, self->value.data(), len);
        out->buffer[len] = '\0';
    } else {
        out->capacity = std::max<std::size_t>(len | 15, 31);
        auto* storage = static_cast<char*>(GuestStringStorage::Allocate(out->capacity + 1));
        std::memcpy(storage, self->value.data(), len);
        storage[len] = '\0';
        out->pointer = storage;
        out->size = len;
    }
    return out;
}

void* const FakeFieldTrialsVtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&FakeFieldTrialsD1),
    reinterpret_cast<void*>(&FakeFieldTrialsD0),
    reinterpret_cast<void*>(&FakeFieldTrialsLookup),
};

struct WebRtcDecodeResult {
    std::size_t numDecodedSamples;
    std::int32_t speechType;
};

struct WebRtcDecodeResultOptional {
    WebRtcDecodeResult value;
    bool engaged;
};

struct WebRtcAudioInt16Buffer {
    std::uint64_t size;
    std::uint64_t capacity;
    std::int16_t* data;
};

struct WebRtcParseResultVector {
    WebRtcAudioDecoderParseResult* begin;
    WebRtcAudioDecoderParseResult* end;
    WebRtcAudioDecoderParseResult* capacityEnd;
};

struct GuardedPage {
    void* base = nullptr;
    std::size_t size = 0;

    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        base = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
        size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        base = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (base == MAP_FAILED)
            base = nullptr;
#endif
        Require(base != nullptr);
    }

    ~GuardedPage() {
        if (!base) return;
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, size);
#endif
    }

    template<typename T> T* As() const { return static_cast<T*>(base); }

    void SetReadOnly() const {
#ifdef _WIN32
        DWORD prev = 0;
        Require(VirtualProtect(base, size, PAGE_READONLY, &prev) != 0);
#else
        Require(mprotect(base, size, PROT_READ) == 0);
#endif
    }

    void SetInaccessible() const {
#ifdef _WIN32
        DWORD prev = 0;
        Require(VirtualProtect(base, size, PAGE_NOACCESS, &prev) != 0);
#else
        Require(mprotect(base, size, PROT_NONE) == 0);
#endif
    }
};

template<typename T>
T Slot(void* const* vtable, std::size_t index) {
    return reinterpret_cast<T>(const_cast<void*>(vtable[index]));
}

using DestroyFn = void(APS5_VABI*)(WebRtcAudioDecoderOpusImpl*);
using ParsePayloadFn = WebRtcParseResultVector*(APS5_VABI*)(WebRtcParseResultVector*, WebRtcAudioDecoderOpusImpl*, WebRtcBuffer*, std::uint32_t);
using HasDecodePlcFn = bool(APS5_VABI*)(const WebRtcAudioDecoderOpusImpl*);
using DecodePlcFn = std::size_t(APS5_VABI*)(WebRtcAudioDecoderOpusImpl*, std::size_t, std::int16_t*);
using GeneratePlcFn = void(APS5_VABI*)(WebRtcAudioDecoderOpusImpl*, std::size_t, WebRtcInt16Buffer*);
using ResetFn = void(APS5_VABI*)(WebRtcAudioDecoderOpusImpl*);
using ErrorCodeFn = int(APS5_VABI*)(WebRtcAudioDecoderOpusImpl*);
using PacketDurationFn = int(APS5_VABI*)(const WebRtcAudioDecoderOpusImpl*, const std::uint8_t*, std::size_t);
using PacketHasFecFn = bool(APS5_VABI*)(const WebRtcAudioDecoderOpusImpl*, const std::uint8_t*, std::size_t);
using SampleRateHzFn = int(APS5_VABI*)(const WebRtcAudioDecoderOpusImpl*);
using ChannelsFn = std::size_t(APS5_VABI*)(const WebRtcAudioDecoderOpusImpl*);
using DecodeInternalFn = int(APS5_VABI*)(WebRtcAudioDecoderOpusImpl*, const std::uint8_t*, std::size_t, int, std::int16_t*, std::int32_t*);

using FrameDestroyFn = void(APS5_VABI*)(void*);
using FrameDurationFn = std::size_t(APS5_VABI*)(const void*);
using FrameIsDtxFn = bool(APS5_VABI*)(const void*);
using FrameDecodeFn = WebRtcDecodeResultOptional*(APS5_VABI*)(WebRtcDecodeResultOptional*, const void*, std::int16_t*, std::size_t);

std::vector<std::uint8_t> EncodeTonePacket(int sampleRateHz, int channels, int frameSamplesPerChannel, int application, bool withFec = false, int phaseOffset = 0) {
    int err = OPUS_OK;
    OpusEncoder* enc = opus_encoder_create(sampleRateHz, channels, application, &err);
    Require(err == OPUS_OK && enc != nullptr);
    opus_encoder_ctl(enc, OPUS_SET_BITRATE(64000));
    if (withFec) {
        opus_encoder_ctl(enc, OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_WIDEBAND));
        opus_encoder_ctl(enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
        opus_encoder_ctl(enc, OPUS_SET_INBAND_FEC(1));
        opus_encoder_ctl(enc, OPUS_SET_PACKET_LOSS_PERC(25));
    }
    std::vector<std::int16_t> pcm(static_cast<std::size_t>(frameSamplesPerChannel) * channels);
    std::vector<std::uint8_t> out(4000);
    int len = 0;
    const int frames = withFec ? 3 : 1;
    for (int f = 0; f < frames; ++f) {
        for (int i = 0; i < frameSamplesPerChannel; ++i) {
            const double angle = 6.28318530718 * 440.0 * static_cast<double>((f + phaseOffset) * frameSamplesPerChannel + i) / static_cast<double>(sampleRateHz);
            const auto sample = static_cast<std::int16_t>(12000.0 * std::sin(angle));
            for (int c = 0; c < channels; ++c)
                pcm[static_cast<std::size_t>(i) * channels + c] = sample;
        }
        len = opus_encode(enc, pcm.data(), frameSamplesPerChannel, out.data(), static_cast<opus_int32>(out.size()));
        Require(len > 0);
    }
    out.resize(static_cast<std::size_t>(len));
    opus_encoder_destroy(enc);
    return out;
}

void TestFieldTrialsAndLifecycle() {
    FakeFieldTrials off {FakeFieldTrialsVtable + 2, "Disabled"};
    WebRtcAudioDecoderOpusImpl dec{};
    _ZN6webrtc20AudioDecoderOpusImplC1ERKNS_15FieldTrialsViewEmi_nid_postfix(&dec, reinterpret_cast<const WebRtcFieldTrialsView*>(&off), 1, 16000);
    Require(off.lastKey == "WebRTC-Audio-OpusGeneratePlc");
    Require(dec.vtable != nullptr && dec.decState != nullptr);
    Require(dec.channels == 1 && dec.sampleRateHz == 16000 && !dec.generatePlc);
    Require(Slot<ChannelsFn>(dec.vtable, 12)(&dec) == 1);
    Require(Slot<SampleRateHzFn>(dec.vtable, 11)(&dec) == 16000);
    Require(!Slot<HasDecodePlcFn>(dec.vtable, 3)(&dec));
    Require(Slot<DecodePlcFn>(dec.vtable, 4)(&dec, 1, nullptr) == 0);
    Require(Slot<ErrorCodeFn>(dec.vtable, 7)(&dec) == 0);
    WebRtcAudioInt16Buffer plc{};
    Slot<GeneratePlcFn>(dec.vtable, 5)(&dec, 320, reinterpret_cast<WebRtcInt16Buffer*>(&plc));
    Require(plc.size == 0 && plc.data == nullptr);
    Slot<DestroyFn>(dec.vtable, 0)(&dec);

    FakeFieldTrials onHeap {FakeFieldTrialsVtable + 2, "Enabled-With-Long-Trial-Parameter", true};
    auto* heapDec = static_cast<WebRtcAudioDecoderOpusImpl*>(_Znwm_nid_postfix(sizeof(WebRtcAudioDecoderOpusImpl)));
    _ZN6webrtc20AudioDecoderOpusImplC1ERKNS_15FieldTrialsViewEmi_nid_postfix(heapDec, reinterpret_cast<const WebRtcFieldTrialsView*>(&onHeap), 2, 48000);
    Require(heapDec->generatePlc && heapDec->channels == 2 && heapDec->sampleRateHz == 48000);
    Slot<GeneratePlcFn>(heapDec->vtable, 5)(heapDec, 960, reinterpret_cast<WebRtcInt16Buffer*>(&plc));
    Require(plc.size == 960 * 2 && plc.capacity >= 960 * 2 && plc.data != nullptr);
    _ZdaPv_nid_postfix(plc.data);
    Slot<DestroyFn>(heapDec->vtable, 1)(heapDec);
    Require(allocations.empty());
}

void TestDecodeAndPacketMetadata() {
    FakeFieldTrials trials {FakeFieldTrialsVtable + 2, "Enabled"};
    WebRtcAudioDecoderOpusImpl dec{};
    _ZN6webrtc20AudioDecoderOpusImplC1ERKNS_15FieldTrialsViewEmi_nid_postfix(&dec, reinterpret_cast<const WebRtcFieldTrialsView*>(&trials), 2, 48000);
    const auto stereoPkt = EncodeTonePacket(48000, 2, 960, OPUS_APPLICATION_AUDIO);
    const auto monoPkt = EncodeTonePacket(48000, 1, 960, OPUS_APPLICATION_AUDIO);

    auto* baseDec = reinterpret_cast<WebRtcAudioDecoder*>(&dec);
    Require(Slot<PacketDurationFn>(dec.vtable, 8)(&dec, nullptr, 0) == 960);
    Require(Slot<PacketDurationFn>(dec.vtable, 8)(&dec, stereoPkt.data(), stereoPkt.size()) == 960);
    Require(!Slot<PacketHasFecFn>(dec.vtable, 10)(&dec, stereoPkt.data(), stereoPkt.size()));
    std::uint8_t badToc = 0xff;
    Require(Slot<PacketDurationFn>(dec.vtable, 8)(&dec, &badToc, 0) == 960);

    std::vector<std::int16_t> out(960 * 2);
    std::int32_t speechType = 0;
    const int decodedStereo = _ZN6webrtc12AudioDecoder6DecodeEPKhmimPsPNS0_10SpeechTypeE_nid_postfix(baseDec, stereoPkt.data(), stereoPkt.size(), 48000, out.size() * sizeof(std::int16_t), out.data(), &speechType);
    Require(decodedStereo == 960 * 2 && speechType == 1);

    const int decodedMono = _ZN6webrtc12AudioDecoder6DecodeEPKhmimPsPNS0_10SpeechTypeE_nid_postfix(baseDec, monoPkt.data(), monoPkt.size(), 48000, out.size() * sizeof(std::int16_t), out.data(), &speechType);
    Require(decodedMono == 960 * 2 && speechType == 1);
    for (int i = 0; i < 960; ++i)
        Require(out[static_cast<std::size_t>(i) * 2] == out[static_cast<std::size_t>(i) * 2 + 1]);

    const std::array<std::uint8_t, 1> dtx {0};
    std::int32_t dtxType = 0;
    const int dtxSamples = _ZN6webrtc12AudioDecoder6DecodeEPKhmimPsPNS0_10SpeechTypeE_nid_postfix(baseDec, dtx.data(), dtx.size(), 48000, out.size() * sizeof(std::int16_t), out.data(), &dtxType);
    Require(dtxSamples >= 0 && dtxType == 2);

    Slot<ResetFn>(dec.vtable, 6)(&dec);
    std::int32_t postResetType = 0;
    const int plcAfterReset = _ZN6webrtc12AudioDecoder6DecodeEPKhmimPsPNS0_10SpeechTypeE_nid_postfix(baseDec, nullptr, 0, 48000, out.size() * sizeof(std::int16_t), out.data(), &postResetType);
    Require(plcAfterReset == 960 * 2 && postResetType == 1);

    _ZN6webrtc20AudioDecoderOpusImplD1Ev_nid_postfix(&dec);
    Require(allocations.empty());
}

void TestParsePayloadAndOpusFrame() {
    FakeFieldTrials trials {FakeFieldTrialsVtable + 2, ""};
    WebRtcAudioDecoderOpusImpl dec{};
    _ZN6webrtc20AudioDecoderOpusImplC1ERKNS_15FieldTrialsViewEmi_nid_postfix(&dec, reinterpret_cast<const WebRtcFieldTrialsView*>(&trials), 1, 16000);

    std::vector<std::uint8_t> fecPkt = EncodeTonePacket(16000, 1, 320, OPUS_APPLICATION_VOIP, true);
    if (!Slot<PacketHasFecFn>(dec.vtable, 10)(&dec, fecPkt.data(), fecPkt.size())) {
        fecPkt = std::vector<std::uint8_t>{0x00, 0x80};
    }

    const auto pkt = EncodeTonePacket(16000, 1, 320, OPUS_APPLICATION_VOIP, false);
    auto* bufData = static_cast<std::uint8_t*>(_Znam_nid_postfix(pkt.size()));
    std::memcpy(bufData, pkt.data(), pkt.size());
    WebRtcBuffer payload {pkt.size(), pkt.size(), bufData};
    WebRtcParseResultVector parsed{};
    Slot<ParsePayloadFn>(dec.vtable, 2)(&parsed, &dec, &payload, 48000);
    Require(payload.data == nullptr && payload.size == 0 && payload.capacity == 0);
    Require(parsed.begin != nullptr && (parsed.end - parsed.begin) == 1);
    Require(parsed.begin[0].timestamp == 48000 && parsed.begin[0].priority == 0 && parsed.begin[0].frame != nullptr);

    void* const* frameVtable = *reinterpret_cast<void* const* const*>(parsed.begin[0].frame);
    Require(Slot<FrameDurationFn>(frameVtable, 2)(parsed.begin[0].frame) == 320);
    Require(!Slot<FrameIsDtxFn>(frameVtable, 3)(parsed.begin[0].frame));
    std::vector<std::int16_t> out(320);
    WebRtcDecodeResultOptional res{};
    Slot<FrameDecodeFn>(frameVtable, 4)(&res, parsed.begin[0].frame, out.data(), out.size());
    Require(res.engaged && res.value.numDecodedSamples == 320 && res.value.speechType == 1);

    _ZN6webrtc12AudioDecoder11ParseResultD1Ev_nid_postfix(&parsed.begin[0]);
    _ZdlPv_nid_postfix(parsed.begin);

    const auto redPkt = EncodeTonePacket(16000, 1, 320, OPUS_APPLICATION_VOIP, true, 5);
    const bool hasFec = Slot<PacketHasFecFn>(dec.vtable, 10)(&dec, redPkt.data(), redPkt.size());
    Require(Slot<PacketDurationFn>(dec.vtable, 9)(&dec, redPkt.data(), redPkt.size()) == 320);
    if (hasFec) {
        auto* redData = static_cast<std::uint8_t*>(_Znam_nid_postfix(redPkt.size()));
        std::memcpy(redData, redPkt.data(), redPkt.size());
        WebRtcBuffer redBuf {redPkt.size(), redPkt.size(), redData};
        WebRtcParseResultVector fecParsed{};
        Slot<ParsePayloadFn>(dec.vtable, 2)(&fecParsed, &dec, &redBuf, 64000);
        Require((fecParsed.end - fecParsed.begin) == 2);
        Require(fecParsed.begin[0].timestamp == 64000 - 320 && fecParsed.begin[0].priority == 1);
        Require(fecParsed.begin[1].timestamp == 64000 && fecParsed.begin[1].priority == 0);
        void* const* fecVtable = *reinterpret_cast<void* const* const*>(fecParsed.begin[0].frame);
        WebRtcDecodeResultOptional fecRes{};
        Slot<FrameDecodeFn>(fecVtable, 4)(&fecRes, fecParsed.begin[0].frame, out.data(), out.size());
        Require(fecRes.engaged && fecRes.value.numDecodedSamples == 320);
        _ZN6webrtc12AudioDecoder11ParseResultD1Ev_nid_postfix(&fecParsed.begin[0]);
        _ZN6webrtc12AudioDecoder11ParseResultD1Ev_nid_postfix(&fecParsed.begin[1]);
        _ZdlPv_nid_postfix(fecParsed.begin);
    }

    _ZN6webrtc20AudioDecoderOpusImplD1Ev_nid_postfix(&dec);
    Require(allocations.empty());
}

void TestValidationAndGuardedPointers() {
    FakeFieldTrials trials {FakeFieldTrialsVtable + 2, ""};
    WebRtcAudioDecoderOpusImpl dec{};
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc20AudioDecoderOpusImplC1ERKNS_15FieldTrialsViewEmi_nid_postfix(nullptr, reinterpret_cast<const WebRtcFieldTrialsView*>(&trials), 1, 48000); });
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc20AudioDecoderOpusImplC1ERKNS_15FieldTrialsViewEmi_nid_postfix(&dec, nullptr, 1, 48000); });
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc20AudioDecoderOpusImplC1ERKNS_15FieldTrialsViewEmi_nid_postfix(&dec, reinterpret_cast<const WebRtcFieldTrialsView*>(&trials), 3, 48000); });
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc20AudioDecoderOpusImplC1ERKNS_15FieldTrialsViewEmi_nid_postfix(&dec, reinterpret_cast<const WebRtcFieldTrialsView*>(&trials), 1, 32000); });
    RequireThrows<std::invalid_argument>([] { _ZN6webrtc20AudioDecoderOpusImplD1Ev_nid_postfix(nullptr); });

    GuardedPage roDec;
    roDec.SetReadOnly();
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc20AudioDecoderOpusImplC1ERKNS_15FieldTrialsViewEmi_nid_postfix(roDec.As<WebRtcAudioDecoderOpusImpl>(), reinterpret_cast<const WebRtcFieldTrialsView*>(&trials), 1, 48000); });

    GuardedPage noTrials;
    noTrials.SetInaccessible();
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc20AudioDecoderOpusImplC1ERKNS_15FieldTrialsViewEmi_nid_postfix(&dec, noTrials.As<const WebRtcFieldTrialsView>(), 1, 48000); });

    _ZN6webrtc20AudioDecoderOpusImplC1ERKNS_15FieldTrialsViewEmi_nid_postfix(&dec, reinterpret_cast<const WebRtcFieldTrialsView*>(&trials), 1, 48000);
    GuardedPage noEncoded;
    noEncoded.SetInaccessible();
    std::int16_t pcm[960];
    std::int32_t speechType = 0;
    RequireThrows<std::invalid_argument>([&] { Slot<DecodeInternalFn>(dec.vtable, 13)(&dec, noEncoded.As<const std::uint8_t>(), 4, 48000, pcm, &speechType); });
    RequireThrows<std::invalid_argument>([&] { Slot<DecodeInternalFn>(dec.vtable, 13)(&dec, nullptr, 0, 16000, pcm, &speechType); });

    GuardedPage roOut;
    roOut.SetReadOnly();
    const auto mono48k = EncodeTonePacket(48000, 1, 960, OPUS_APPLICATION_AUDIO);
    RequireThrows<std::invalid_argument>([&] { Slot<DecodeInternalFn>(dec.vtable, 13)(&dec, mono48k.data(), mono48k.size(), 48000, roOut.As<std::int16_t>(), &speechType); });

    std::array<std::int16_t, 160> smallOut{};
    smallOut.fill(0x1234);
    auto* baseDec = reinterpret_cast<WebRtcAudioDecoder*>(&dec);
    Require(_ZN6webrtc12AudioDecoder6DecodeEPKhmimPsPNS0_10SpeechTypeE_nid_postfix(baseDec, mono48k.data(), mono48k.size(), 48000, smallOut.size() * sizeof(std::int16_t), smallOut.data(), &speechType) == -1);
    Require(smallOut[0] == 0x1234);

    const std::array<std::uint8_t, 2> malformedCode3 {0xff, 0x00};
    Require(Slot<DecodeInternalFn>(dec.vtable, 13)(&dec, malformedCode3.data(), malformedCode3.size(), 48000, smallOut.data(), &speechType) == -1);
    Require(smallOut[0] == 0x1234);

    auto* parseBuf = static_cast<std::uint8_t*>(_Znam_nid_postfix(mono48k.size()));
    std::memcpy(parseBuf, mono48k.data(), mono48k.size());
    WebRtcBuffer payload {mono48k.size(), mono48k.size(), parseBuf};
    WebRtcParseResultVector parsed{};
    Slot<ParsePayloadFn>(dec.vtable, 2)(&parsed, &dec, &payload, 96000);
    void* const* frameVtable = *reinterpret_cast<void* const* const*>(parsed.begin[0].frame);
    WebRtcDecodeResultOptional smallRes{};
    Slot<FrameDecodeFn>(frameVtable, 4)(&smallRes, parsed.begin[0].frame, smallOut.data(), smallOut.size());
    Require(!smallRes.engaged);
    Require(smallOut[0] == 0x1234);
    _ZN6webrtc12AudioDecoder11ParseResultD1Ev_nid_postfix(&parsed.begin[0]);
    _ZdlPv_nid_postfix(parsed.begin);

    _ZN6webrtc20AudioDecoderOpusImplD1Ev_nid_postfix(&dec);
    Require(allocations.empty());
}

void TestParsePayloadAllocationFailure() {
    FakeFieldTrials trials {FakeFieldTrialsVtable + 2, ""};
    WebRtcAudioDecoderOpusImpl dec{};
    _ZN6webrtc20AudioDecoderOpusImplC1ERKNS_15FieldTrialsViewEmi_nid_postfix(&dec, reinterpret_cast<const WebRtcFieldTrialsView*>(&trials), 1, 16000);
    const auto redPkt = EncodeTonePacket(16000, 1, 320, OPUS_APPLICATION_VOIP, true, 5);
    const bool hasFec = Slot<PacketHasFecFn>(dec.vtable, 10)(&dec, redPkt.data(), redPkt.size());
    const std::size_t allocCount = hasFec ? 4 : 2;
    auto* redData = static_cast<std::uint8_t*>(_Znam_nid_postfix(redPkt.size()));
    std::memcpy(redData, redPkt.data(), redPkt.size());
    WebRtcBuffer payload {redPkt.size(), redPkt.size(), redData};
    WebRtcParseResultVector parsed{};
    for (std::size_t failAt = 0; failAt < allocCount; ++failAt) {
        failAfterAllocations = failAt;
        RequireThrows<std::exception>([&] { Slot<ParsePayloadFn>(dec.vtable, 2)(&parsed, &dec, &payload, 64000); });
        failAfterAllocations = static_cast<std::size_t>(-1);
        Require(payload.data == redData && payload.size == redPkt.size() && payload.capacity == redPkt.size());
        Require(parsed.begin == nullptr && parsed.end == nullptr);
    }
    _ZdaPv_nid_postfix(redData);
    _ZN6webrtc20AudioDecoderOpusImplD1Ev_nid_postfix(&dec);
    Require(allocations.empty());
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestFieldTrialsAndLifecycle();
    TestDecodeAndPacketMetadata();
    TestParsePayloadAndOpusFrame();
    TestValidationAndGuardedPointers();
    TestParsePayloadAllocationFailure();
    std::puts("libwebrtc AudioDecoderOpusImpl tests passed");
    return 0;
}
