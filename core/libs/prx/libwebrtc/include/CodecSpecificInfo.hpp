#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcInlinedVectorHeap {
    void* data;
    std::uint64_t capacity;
};

template<std::size_t TInlineBytes>
struct WebRtcInlinedVector {
    std::uint64_t metadata;
    union {
        WebRtcInlinedVectorHeap allocated;
        std::uint8_t inlined[TInlineBytes];
    };
};

struct WebRtcBoolVector {
    std::uint64_t* begin;
    std::uint64_t size;
    std::uint64_t capacity;
};

struct WebRtcFrameDependencyTemplate {
    std::int32_t spatialId;
    std::int32_t temporalId;
    WebRtcInlinedVector<40> decodeTargetIndications;
    WebRtcInlinedVector<16> frameDiffs;
    WebRtcInlinedVector<16> chainDiffs;
};

struct WebRtcGenericFrameInfo {
    WebRtcFrameDependencyTemplate frameDependency;
    WebRtcInlinedVector<64> encoderBuffers;
    WebRtcBoolVector partOfChain;
    std::uint64_t activeDecodeTargets;
};

struct WebRtcFrameDependencyStructure {
    std::int32_t structureId;
    std::int32_t numDecodeTargets;
    std::int32_t numChains;
    WebRtcInlinedVector<40> decodeTargetProtectedByChain;
    WebRtcInlinedVector<32> resolutions;
    WebRtcFrameDependencyTemplate* templatesBegin;
    WebRtcFrameDependencyTemplate* templatesEnd;
    WebRtcFrameDependencyTemplate* templatesCapacity;
};

struct WebRtcFrameInstrumentationSyncData {
    std::int32_t sequenceIndex;
    bool communicateUpperBits;
};

struct WebRtcFrameInstrumentationData {
    std::int32_t sequenceIndex;
    bool communicateUpperBits;
    double stdDev;
    std::int32_t lumaErrorThreshold;
    std::int32_t chromaErrorThreshold;
    double* sampleValuesBegin;
    double* sampleValuesEnd;
    double* sampleValuesCapacity;
};

struct WebRtcFrameInstrumentationVariant {
    union {
        WebRtcFrameInstrumentationSyncData sync;
        WebRtcFrameInstrumentationData data;
    };
    std::uint32_t index;
};

inline constexpr std::uint32_t WebRtcFrameInstrumentationSyncIndex = 0;
inline constexpr std::uint32_t WebRtcFrameInstrumentationDataIndex = 1;
inline constexpr std::uint32_t WebRtcFrameInstrumentationValueless = 0xffffffffu;
inline constexpr std::size_t WebRtcCodecSpecificUnionSize = 1624;

struct WebRtcCodecSpecificInfo {
    std::int32_t codecType;
    alignas(8) std::uint8_t codecSpecific[WebRtcCodecSpecificUnionSize];
    bool endOfPicture;
    WebRtcGenericFrameInfo genericFrameInfo;
    bool hasGenericFrameInfo;
    WebRtcFrameDependencyStructure templateStructure;
    bool hasTemplateStructure;
    std::uint8_t templateStructurePadding[7];
    std::uint8_t scalabilityMode;
    bool hasScalabilityMode;
    WebRtcFrameInstrumentationVariant frameInstrumentationData;
    bool hasFrameInstrumentationData;
};

static_assert(sizeof(WebRtcInlinedVector<40>) == 48 && sizeof(WebRtcInlinedVector<16>) == 24);
static_assert(sizeof(WebRtcInlinedVector<64>) == 72 && sizeof(WebRtcInlinedVector<32>) == 40);
static_assert(offsetof(WebRtcInlinedVector<16>, allocated) == 8 && offsetof(WebRtcInlinedVectorHeap, capacity) == 8);
static_assert(sizeof(WebRtcBoolVector) == 24);
static_assert(sizeof(WebRtcFrameDependencyTemplate) == 104);
static_assert(offsetof(WebRtcFrameDependencyTemplate, decodeTargetIndications) == 8);
static_assert(offsetof(WebRtcFrameDependencyTemplate, frameDiffs) == 56);
static_assert(offsetof(WebRtcFrameDependencyTemplate, chainDiffs) == 80);
static_assert(sizeof(WebRtcGenericFrameInfo) == 208);
static_assert(offsetof(WebRtcGenericFrameInfo, encoderBuffers) == 104);
static_assert(offsetof(WebRtcGenericFrameInfo, partOfChain) == 176);
static_assert(offsetof(WebRtcGenericFrameInfo, activeDecodeTargets) == 200);
static_assert(sizeof(WebRtcFrameDependencyStructure) == 128);
static_assert(offsetof(WebRtcFrameDependencyStructure, decodeTargetProtectedByChain) == 16);
static_assert(offsetof(WebRtcFrameDependencyStructure, resolutions) == 64);
static_assert(offsetof(WebRtcFrameDependencyStructure, templatesBegin) == 104);
static_assert(sizeof(WebRtcFrameInstrumentationData) == 48);
static_assert(offsetof(WebRtcFrameInstrumentationData, stdDev) == 8);
static_assert(offsetof(WebRtcFrameInstrumentationData, lumaErrorThreshold) == 16);
static_assert(offsetof(WebRtcFrameInstrumentationData, chromaErrorThreshold) == 20);
static_assert(offsetof(WebRtcFrameInstrumentationData, sampleValuesBegin) == 24);
static_assert(sizeof(WebRtcFrameInstrumentationVariant) == 56);
static_assert(offsetof(WebRtcFrameInstrumentationVariant, index) == 48);
static_assert(sizeof(WebRtcCodecSpecificInfo) == 2064 && alignof(WebRtcCodecSpecificInfo) == 8);
static_assert(offsetof(WebRtcCodecSpecificInfo, codecType) == 0);
static_assert(offsetof(WebRtcCodecSpecificInfo, codecSpecific) == 8);
static_assert(offsetof(WebRtcCodecSpecificInfo, endOfPicture) == 1632);
static_assert(offsetof(WebRtcCodecSpecificInfo, genericFrameInfo) == 1640);
static_assert(offsetof(WebRtcCodecSpecificInfo, hasGenericFrameInfo) == 1848);
static_assert(offsetof(WebRtcCodecSpecificInfo, templateStructure) == 1856);
static_assert(offsetof(WebRtcCodecSpecificInfo, hasTemplateStructure) == 1984);
static_assert(offsetof(WebRtcCodecSpecificInfo, scalabilityMode) == 1992);
static_assert(offsetof(WebRtcCodecSpecificInfo, hasScalabilityMode) == 1993);
static_assert(offsetof(WebRtcCodecSpecificInfo, frameInstrumentationData) == 2000);
static_assert(offsetof(WebRtcCodecSpecificInfo, hasFrameInstrumentationData) == 2056);
