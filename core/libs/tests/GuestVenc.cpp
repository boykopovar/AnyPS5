#include "prx/libSceVenc/libSceVenc.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {

constexpr int VencError(std::uint32_t code) {
    return static_cast<int>(0x80C70000u | code);
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "VENC: %s\n", message);
    std::abort();
}

struct PointerBlock {
    std::uint64_t words[8]{};

    void Set(std::size_t offset, const void* pointer) {
        std::memcpy(reinterpret_cast<std::uint8_t*>(words) + offset, &pointer, sizeof(pointer));
    }

    void SetWord(std::size_t offset, std::uint64_t value) {
        std::memcpy(reinterpret_cast<std::uint8_t*>(words) + offset, &value, sizeof(value));
    }
};

using SetConfigFunction = decltype(&sceVencSetConfig);

void CheckSetConfig(SetConfigFunction function, std::uint32_t nullConfigCode, const char* name) {
    std::uint8_t config[16]{};
    std::uint8_t encoder[16]{};
    if (function(nullptr, config) != VencError(0x00) || function(nullptr, nullptr) != VencError(0x00) ||
        function(encoder, nullptr) != VencError(nullConfigCode)) {
        std::fprintf(stderr, "VENC: %s\n", name);
        std::abort();
    }
}

void CheckEncoderParameters() {
    std::uint8_t target[16]{};
    PointerBlock memory;
    memory.Set(0x8, target);
    memory.Set(0x10, target);
    memory.Set(0x18, target);
    PointerBlock parameters;
    void* encoder = target;

    Require(sceVencCreateEncoder(nullptr, nullptr, nullptr) == VencError(0x00), "Create: null output first");
    Require(sceVencCreateEncoder(nullptr, &memory, &encoder) == VencError(0x01), "Create: null parameters");
    Require(encoder == nullptr, "Create: output cleared before the parameter checks");
    encoder = target;
    Require(sceVencCreateEncoder(&parameters, &memory, &encoder) == VencError(0x04), "Create: parameter +8");
    Require(encoder == nullptr, "Create: output cleared");
    parameters.Set(0x8, target);
    Require(sceVencCreateEncoder(&parameters, &memory, &encoder) == VencError(0x05), "Create: parameter +0x10");
    parameters.Set(0x10, target);
    Require(sceVencCreateEncoder(&parameters, nullptr, &encoder) == VencError(0x10), "Create: null memory");
    for (const std::size_t offset : {std::size_t{0x8}, std::size_t{0x10}, std::size_t{0x18}}) {
        PointerBlock incomplete = memory;
        incomplete.Set(offset, nullptr);
        Require(sceVencCreateEncoder(&parameters, &incomplete, &encoder) == VencError(0x10), "Create: memory field");
    }
    errno = 71;
    Require(sceVencCreateEncoder(nullptr, nullptr, reinterpret_cast<void**>(0x10)) == VencError(0x00), "Create: unmapped output");
    Require(errno == 71, "Create: errno preserved on unmapped output");
    errno = 71;
    Require(sceVencCreateEncoder(reinterpret_cast<const void*>(0x10), &memory, &encoder) == VencError(0x04), "Create: unmapped parameters");
    Require(errno == 71, "Create: errno preserved on unmapped parameters");
    errno = 71;
    Require(sceVencCreateEncoder(&parameters, reinterpret_cast<const void*>(0x10), &encoder) == VencError(0x10), "Create: unmapped memory");
    Require(errno == 71, "Create: errno preserved on unmapped memory");
    errno = 71;
    Require(sceVencQueryPreset2(reinterpret_cast<const void*>(0x10), nullptr, nullptr, nullptr) == VencError(0x04), "QueryPreset2: unmapped preset");
    Require(errno == 71, "QueryPreset2: errno preserved");
    errno = 71;
    Require(sceVencMapMemory(&encoder, reinterpret_cast<const void*>(0x10)) == VencError(0x10), "MapMemory: unmapped region");
    Require(errno == 71, "MapMemory: errno preserved");

    PointerBlock header;
    PointerBlock query;
    Require(sceVencQueryHeader(nullptr, &header) == VencError(0x01), "QueryHeader: null parameters");
    Require(sceVencQueryHeader(&query, &header) == VencError(0x04), "QueryHeader: parameter +8");
    query.Set(0x8, target);
    Require(sceVencQueryHeader(&query, nullptr) == VencError(0x0F), "QueryHeader: null header");

    PointerBlock nested;
    PointerBlock sizes;
    PointerBlock memoryQuery;
    Require(sceVencQueryMemorySize(nullptr, &sizes) == VencError(0x01), "QueryMemorySize: null parameters");
    Require(sceVencQueryMemorySize(&memoryQuery, &sizes) == VencError(0x04), "QueryMemorySize: parameter +8");
    memoryQuery.Set(0x8, &nested);
    Require(sceVencQueryMemorySize(&memoryQuery, &sizes) == VencError(0x05), "QueryMemorySize: parameter +0x10");
    memoryQuery.Set(0x10, target);
    Require(sceVencQueryMemorySize(&memoryQuery, &sizes) == VencError(0x06), "QueryMemorySize: parameter +0x18");
    memoryQuery.Set(0x18, target);
    nested.SetWord(0x20, 2);
    Require(sceVencQueryMemorySize(&memoryQuery, &sizes) == VencError(0x07), "QueryMemorySize: mode 2 needs +0x28");

    PointerBlock getHeader;
    PointerBlock output;
    Require(sceVencGetHeader(nullptr, nullptr, nullptr, &memory, &output) == VencError(0x01), "GetHeader: null parameters");
    Require(sceVencGetHeader(&getHeader, nullptr, nullptr, &memory, &output) == VencError(0x04), "GetHeader: +8");
    getHeader.Set(0x8, target);
    Require(sceVencGetHeader(&getHeader, nullptr, nullptr, &memory, &output) == VencError(0x05), "GetHeader: +0x10");
    getHeader.Set(0x10, target);
    Require(sceVencGetHeader(&getHeader, nullptr, nullptr, &memory, &output) == VencError(0x06), "GetHeader: +0x18");
    getHeader.Set(0x18, target);
    Require(sceVencGetHeader(&getHeader, nullptr, nullptr, nullptr, &output) == VencError(0x10), "GetHeader: null memory");
    for (const std::size_t offset : {std::size_t{0x8}, std::size_t{0x10}, std::size_t{0x18}}) {
        PointerBlock incomplete = memory;
        incomplete.Set(offset, nullptr);
        Require(sceVencGetHeader(&getHeader, nullptr, nullptr, &incomplete, &output) == VencError(0x10),
            "GetHeader: memory field");
    }
    Require(sceVencGetHeader(&getHeader, nullptr, nullptr, &memory, nullptr) == VencError(0x0F), "GetHeader: null header");

    PointerBlock preset;
    Require(sceVencQueryPreset2(nullptr, nullptr, nullptr, nullptr) == VencError(0x04), "QueryPreset2: null preset");
    Require(sceVencQueryPreset(nullptr, nullptr, nullptr) == VencError(0x04), "QueryPreset: null preset");
    for (const std::uint64_t size : {std::uint64_t{0}, std::uint64_t{0x2F}, std::uint64_t{0x39}, ~std::uint64_t{0}}) {
        preset.SetWord(0, size);
        Require(sceVencQueryPreset2(&preset, nullptr, nullptr, nullptr) == VencError(0x04), "QueryPreset2: size");
        Require(sceVencQueryPreset(&preset, nullptr, nullptr) == VencError(0x04), "QueryPreset: size");
    }
}

void CheckEncoderCalls() {
    std::uint8_t encoder[16]{};
    std::uint8_t argument[16]{};
    Require(sceVencDeleteEncoder(nullptr) == VencError(0x00), "Delete");
    Require(sceVencStopEncode(nullptr) == VencError(0x00), "Stop");
    Require(sceVencStartEncode(nullptr, argument) == VencError(0x00), "Start");
    Require(sceVencGetConfig(nullptr, argument) == VencError(0x00), "GetConfig");
    Require(libSceVencUnknown_2FTI1YJYNBw_nid_no_patch(nullptr, argument) == VencError(0x00), "2FTI1YJYNBw");

    PointerBlock frame;
    Require(sceVencSetDecodedFrame(nullptr, &frame) == VencError(0x00), "SetDecodedFrame: null encoder");
    Require(sceVencSetDecodedFrame(encoder, &frame) == VencError(0x11), "SetDecodedFrame: frame +8");

    PointerBlock region;
    for (const auto map : {sceVencMapMemory, sceVencUnmapMemory}) {
        Require(map(nullptr, &region) == VencError(0x00), "Map: null encoder");
        Require(map(encoder, nullptr) == VencError(0x10), "Map: null region");
        for (const std::uint64_t size : {std::uint64_t{0}, std::uint64_t{0x1F}, std::uint64_t{0x21}, ~std::uint64_t{0}}) {
            region.SetWord(0, size);
            Require(map(encoder, &region) == VencError(0x10), "Map: region size");
        }
    }

    CheckSetConfig(sceVencSetConfig, 0x03, "SetConfig");
    CheckSetConfig(sceVencSetCodecConfig, 0x05, "SetCodecConfig");
    CheckSetConfig(sceVencSetPictureConfig, 0x06, "SetPictureConfig");
    CheckSetConfig(sceVencSetRateControlConfig, 0x08, "SetRateControlConfig");
    CheckSetConfig(sceVencSetImagePasteConfig, 0x09, "SetImagePasteConfig");
    CheckSetConfig(sceVencSetNormalPrivacyProtectionConfig, 0x0A, "SetNormalPrivacyProtectionConfig");
    CheckSetConfig(libSceVencUnknown_B5iyzWMm_plus_DY_nid_no_patch, 0x0B, "B5iyzWMm+DY");
    CheckSetConfig(libSceVencUnknown_Vm5uPoERWmg_nid_no_patch, 0x0C, "Vm5uPoERWmg");
    CheckSetConfig(libSceVencUnknown_6nGfjRPqCUM_nid_no_patch, 0x0D, "6nGfjRPqCUM");
    CheckSetConfig(sceVencSetReferenceFrameInvalidationConfig, 0x0E, "SetReferenceFrameInvalidationConfig");
    CheckSetConfig(libSceVencUnknown_JoBLKJB_minus_Nck_nid_no_patch, 0x13, "JoBLKJB-Nck");
    CheckSetConfig(libSceVencUnknown_766UkIIwtxU_nid_no_patch, 0x14, "766UkIIwtxU");
}

void CheckSocSupport() {
    const bool socSupports = static_cast<std::uint32_t>(sceKernelGetMainSocId()) >= 0x840F80u;
    for (const int type : {100, 200}) {
        Require(libSceVencUnknown_39_plus_oWg0ZObY_nid_no_patch(type) == socSupports, "39+oWg0ZObY: types 100 and 200");
    }
    for (const int type : {0, 1, 99, 101, 150, 199, 201, -100}) {
        Require(!libSceVencUnknown_39_plus_oWg0ZObY_nid_no_patch(type), "39+oWg0ZObY: other types");
    }
}

}

int main() {
    CheckEncoderParameters();
    CheckEncoderCalls();
    CheckSocSupport();
    std::puts("Venc tests passed");
    return 0;
}
