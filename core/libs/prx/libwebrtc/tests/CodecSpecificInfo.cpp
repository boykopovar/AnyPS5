#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
#include <unordered_map>

namespace {

std::unordered_map<void*, std::size_t> allocations;
std::size_t allocated = 0;
std::size_t freed = 0;

void Require(bool condition) {
    if (!condition) throw std::runtime_error("CodecSpecificInfo assertion failed");
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    auto* pointer = std::malloc(size);
    if (pointer) {
        allocations.emplace(pointer, size);
        ++allocated;
        std::memset(pointer, 0xa5, size);
    }
    return pointer;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    Require(allocations.erase(pointer) == 1);
    ++freed;
    std::free(pointer);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t size) { return Allocate_nid_no_patch(size); }
void* APS5_VABI Realign_nid_no_patch(void* pointer, std::size_t size, std::size_t) { return Reallocate_nid_no_patch(pointer, size); }
int APS5_VABI PosixAlign_nid_no_patch(void** pointer, std::size_t alignment, std::size_t size) {
    *pointer = Align_nid_no_patch(alignment, size);
    return *pointer ? 0 : 12;
}

template<std::size_t TInlineBytes>
void Allocate(WebRtcInlinedVector<TInlineBytes>& vector, std::size_t size, std::size_t bytes) {
    vector.metadata = (size << 1) | 1;
    vector.allocated.data = _Znwm_nid_postfix(bytes);
    vector.allocated.capacity = size;
}

template<std::size_t TInlineBytes>
void Inline(WebRtcInlinedVector<TInlineBytes>& vector, std::size_t size) {
    vector.metadata = size << 1;
    std::memset(vector.inlined, 0x5a, sizeof(vector.inlined));
}

void Construct(WebRtcCodecSpecificInfo& info) {
    std::memset(&info, 0xa5, sizeof(info));
    _ZN6webrtc17CodecSpecificInfoC1Ev_nid_postfix(&info);
}

void TestDefault() {
    WebRtcCodecSpecificInfo info;
    Construct(info);
    Require(info.codecType == 0 && info.endOfPicture);
    for (auto byte : info.codecSpecific) Require(byte == 0);
    Require(!info.hasGenericFrameInfo && !info.hasTemplateStructure);
    Require(!info.hasScalabilityMode && !info.hasFrameInstrumentationData);
    _ZN6webrtc17CodecSpecificInfoD1Ev_nid_postfix(&info);
    Require(allocated == 0 && freed == 0);
    bool threw = false;
    try { _ZN6webrtc17CodecSpecificInfoC1Ev_nid_postfix(nullptr); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
}

void FillGenericFrameInfo(WebRtcGenericFrameInfo& info) {
    Allocate(info.frameDependency.decodeTargetIndications, 12, 12 * 4);
    Inline(info.frameDependency.frameDiffs, 2);
    Allocate(info.frameDependency.chainDiffs, 5, 5 * 4);
    Allocate(info.encoderBuffers, 9, 9 * 8);
    info.partOfChain.begin = static_cast<std::uint64_t*>(_Znwm_nid_postfix(8));
    info.partOfChain.size = 3;
    info.partOfChain.capacity = 1;
    info.activeDecodeTargets = 0xffffffffu;
}

void FillTemplateStructure(WebRtcFrameDependencyStructure& structure) {
    Allocate(structure.decodeTargetProtectedByChain, 11, 11 * 4);
    Inline(structure.resolutions, 1);
    structure.templatesBegin = static_cast<WebRtcFrameDependencyTemplate*>(_Znwm_nid_postfix(3 * sizeof(WebRtcFrameDependencyTemplate)));
    structure.templatesEnd = structure.templatesBegin + 2;
    structure.templatesCapacity = structure.templatesBegin + 3;
    auto& first = structure.templatesBegin[0];
    Inline(first.decodeTargetIndications, 3);
    Allocate(first.frameDiffs, 6, 6 * 4);
    Inline(first.chainDiffs, 0);
    auto& second = structure.templatesBegin[1];
    Inline(second.decodeTargetIndications, 1);
    Inline(second.frameDiffs, 1);
    Inline(second.chainDiffs, 1);
}

void FillInstrumentationData(WebRtcFrameInstrumentationVariant& variant) {
    variant.index = WebRtcFrameInstrumentationDataIndex;
    variant.data.sequenceIndex = 7;
    variant.data.sampleValuesBegin = static_cast<double*>(_Znwm_nid_postfix(4 * sizeof(double)));
    variant.data.sampleValuesEnd = variant.data.sampleValuesBegin + 3;
    variant.data.sampleValuesCapacity = variant.data.sampleValuesBegin + 4;
}

void ExpectThrow(WebRtcCodecSpecificInfo* info) {
    bool threw = false;
    try { _ZN6webrtc17CodecSpecificInfoD1Ev_nid_postfix(info); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
}

void TestDestructor() {
    WebRtcCodecSpecificInfo info;
    Construct(info);
    info.hasGenericFrameInfo = true;
    FillGenericFrameInfo(info.genericFrameInfo);
    info.hasTemplateStructure = true;
    FillTemplateStructure(info.templateStructure);
    info.hasScalabilityMode = true;
    info.scalabilityMode = 3;
    info.hasFrameInstrumentationData = true;
    FillInstrumentationData(info.frameInstrumentationData);
    Require(allocated == 8);
    _ZN6webrtc17CodecSpecificInfoD1Ev_nid_postfix(&info);
    Require(freed == 8 && allocations.empty());

    for (auto index : {WebRtcFrameInstrumentationSyncIndex, WebRtcFrameInstrumentationValueless}) {
        Construct(info);
        info.hasFrameInstrumentationData = true;
        info.frameInstrumentationData.index = index;
        info.frameInstrumentationData.sync = {3, true};
        _ZN6webrtc17CodecSpecificInfoD1Ev_nid_postfix(&info);
        Require(freed == 8);
    }

    Construct(info);
    info.hasFrameInstrumentationData = true;
    FillInstrumentationData(info.frameInstrumentationData);
    info.hasTemplateStructure = true;
    FillTemplateStructure(info.templateStructure);
    const auto before = allocations.size();
    info.frameInstrumentationData.index = 2;
    ExpectThrow(&info);
    info.frameInstrumentationData.index = WebRtcFrameInstrumentationDataIndex;
    auto* end = info.templateStructure.templatesEnd;
    info.templateStructure.templatesEnd = info.templateStructure.templatesCapacity + 1;
    ExpectThrow(&info);
    info.templateStructure.templatesEnd = end;
    auto* samples = info.frameInstrumentationData.data.sampleValuesEnd;
    info.frameInstrumentationData.data.sampleValuesEnd = info.frameInstrumentationData.data.sampleValuesBegin - 1;
    ExpectThrow(&info);
    info.frameInstrumentationData.data.sampleValuesEnd = samples;
    Require(allocations.size() == before);
    _ZN6webrtc17CodecSpecificInfoD1Ev_nid_postfix(&info);
    Require(allocations.empty());

    Construct(info);
    info.hasTemplateStructure = true;
    std::memset(&info.templateStructure, 0, sizeof(info.templateStructure));
    info.hasGenericFrameInfo = true;
    std::memset(&info.genericFrameInfo, 0, sizeof(info.genericFrameInfo));
    _ZN6webrtc17CodecSpecificInfoD1Ev_nid_postfix(&info);
    ExpectThrow(nullptr);
}

}

int main(int argc, char** argv) {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    try {
        Require(argc == 2);
        if (std::strcmp(argv[1], "default") == 0) TestDefault();
        else if (std::strcmp(argv[1], "destructor") == 0) TestDestructor();
        else Require(false);
        Require(allocations.empty() && allocated == freed);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "CodecSpecificInfo exception: %s\n", error.what());
        return 1;
    }
}
