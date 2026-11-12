#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
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
bool failAllocation = false;

void Require(bool condition) {
    if (!condition) throw std::runtime_error("VideoSinkWants assertion failed");
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (failAllocation) throw std::bad_alloc();
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

void TestDefault() {
    WebRtcVideoSinkWants wants;
    std::memset(&wants, 0xa5, sizeof(wants));
    _ZN3rtc14VideoSinkWantsC1Ev_nid_postfix(&wants);
    Require(!wants.rotationApplied && !wants.blackFrames && !wants.isActive);
    Require(wants.maxPixelCount == 2147483647 && wants.maxFramerateFps == 2147483647);
    Require(!wants.hasTargetPixelCount && !wants.hasRequestedResolution && !wants.hasAggregates);
    Require(wants.resolutionAlignment == 1);
    Require(!wants.resolutionsBegin && !wants.resolutionsEnd && !wants.resolutionsCapacity);
    _ZN3rtc14VideoSinkWantsD1Ev_nid_postfix(&wants);
    bool threw = false;
    try { _ZN3rtc14VideoSinkWantsC1Ev_nid_postfix(nullptr); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
}

void TestCopy() {
    for (bool engaged : {false, true}) {
        WebRtcVideoSinkWants source{};
        source.rotationApplied = true;
        source.blackFrames = true;
        source.isActive = true;
        source.maxPixelCount = 123456;
        source.maxFramerateFps = 37;
        source.resolutionAlignment = 8;
        source.hasTargetPixelCount = engaged;
        source.targetPixelCount = 120000;
        source.hasRequestedResolution = engaged;
        source.requestedResolution = {641, 359};
        source.hasAggregates = engaged;
        source.anyActiveWithoutRequestedResolution = true;
        source.resolutionsBegin = static_cast<WebRtcVideoSinkFrameSize*>(_Znwm_nid_postfix(3 * sizeof(WebRtcVideoSinkFrameSize)));
        source.resolutionsEnd = source.resolutionsBegin + 2;
        source.resolutionsCapacity = source.resolutionsBegin + 3;
        source.resolutionsBegin[0] = {1920, 1080};
        source.resolutionsBegin[1] = {854, 480};
        WebRtcVideoSinkWants copy;
        std::memset(&copy, 0xa5, sizeof(copy));
        const auto before = allocated;
        _ZN3rtc14VideoSinkWantsC1ERKS0__nid_postfix(&copy, &source);
        Require(allocated == before + 1 && copy.resolutionsBegin != source.resolutionsBegin);
        _ZN3rtc14VideoSinkWantsD1Ev_nid_postfix(&source);
        Require(copy.rotationApplied && copy.blackFrames && copy.isActive);
        Require(copy.maxPixelCount == 123456 && copy.maxFramerateFps == 37 && copy.resolutionAlignment == 8);
        Require(copy.hasTargetPixelCount == engaged && copy.hasRequestedResolution == engaged && copy.hasAggregates == engaged);
        if (engaged) {
            Require(copy.targetPixelCount == 120000);
            Require(copy.requestedResolution.width == 641 && copy.requestedResolution.height == 359);
            Require(copy.anyActiveWithoutRequestedResolution);
        }
        Require(copy.resolutionsEnd - copy.resolutionsBegin == 2 && copy.resolutionsCapacity >= copy.resolutionsEnd);
        Require(copy.resolutionsBegin[0].width == 1920 && copy.resolutionsBegin[0].height == 1080);
        Require(copy.resolutionsBegin[1].width == 854 && copy.resolutionsBegin[1].height == 480);
        _ZN3rtc14VideoSinkWantsD1Ev_nid_postfix(&copy);
    }
    WebRtcVideoSinkWants source{}, copy;
    _ZN3rtc14VideoSinkWantsC1ERKS0__nid_postfix(&copy, &source);
    Require(!copy.resolutionsBegin && !copy.resolutionsEnd && !copy.resolutionsCapacity);
    _ZN3rtc14VideoSinkWantsD1Ev_nid_postfix(&copy);
    source.resolutionsBegin = static_cast<WebRtcVideoSinkFrameSize*>(_Znwm_nid_postfix(8));
    source.resolutionsEnd = source.resolutionsCapacity = source.resolutionsBegin + 1;
    failAllocation = true;
    bool threw = false;
    try { _ZN3rtc14VideoSinkWantsC1ERKS0__nid_postfix(&copy, &source); }
    catch (const std::bad_alloc&) { threw = true; }
    failAllocation = false;
    Require(threw && allocations.size() == 1);
    _ZN3rtc14VideoSinkWantsD1Ev_nid_postfix(&source);
    for (int i = 0; i < 3; ++i) {
        threw = false;
        try { _ZN3rtc14VideoSinkWantsC1ERKS0__nid_postfix(i == 0 ? nullptr : &copy, i == 1 ? nullptr : &copy); }
        catch (const std::invalid_argument&) { threw = true; }
        Require(threw);
    }
}

void TestDestructor() {
    WebRtcVideoSinkWants wants{};
    wants.resolutionsBegin = static_cast<WebRtcVideoSinkFrameSize*>(_Znwm_nid_postfix(8));
    wants.resolutionsEnd = wants.resolutionsBegin;
    wants.resolutionsCapacity = wants.resolutionsBegin + 1;
    const auto before = freed;
    _ZN3rtc14VideoSinkWantsD1Ev_nid_postfix(&wants);
    Require(freed == before + 1);
    bool threw = false;
    try { _ZN3rtc14VideoSinkWantsD1Ev_nid_postfix(nullptr); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
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
        else if (std::strcmp(argv[1], "copy") == 0) TestCopy();
        else if (std::strcmp(argv[1], "destructor") == 0) TestDestructor();
        else Require(false);
        Require(allocations.empty() && allocated == freed);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "VideoSinkWants exception: %s\n", error.what());
        return 1;
    }
}
