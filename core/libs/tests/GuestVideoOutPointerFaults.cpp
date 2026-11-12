#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "prx/libSceVideoOut/include/Event.hpp"
#include "prx/libSceVideoOut/include/InventoryExports.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

constexpr auto Unmapped = static_cast<std::uintptr_t>(1);
constexpr int InvalidAddress = static_cast<int>(0x80290002u);

template <typename TAction>
bool Throws(TAction&& action) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "VideoOut pointer faults: %s\n", message);
    std::abort();
}

}

int main() {
    VideoOutBufferAttribute2 mapped{};
    mapped.width = 123;
    mapped.option = 1;
    sceVideoOutAddBufferAttributeOption(&mapped, 0x20);
    Require(mapped.option == 0x21 && mapped.width == 123, "AddBufferAttributeOption changed another field");
    sceVideoOutRemoveBufferAttributeOption(&mapped, 1);
    Require(mapped.option == 0x20 && mapped.width == 123, "RemoveBufferAttributeOption changed another field");

    auto* const unmapped = reinterpret_cast<void*>(Unmapped);
    auto* const attribute2 = static_cast<VideoOutBufferAttribute2*>(unmapped);
    auto* const attribute = static_cast<VideoOutBufferAttribute*>(unmapped);
    auto* const event = static_cast<KernelEvent*>(unmapped);
    auto* const data = static_cast<std::int64_t*>(unmapped);
    auto* const options = static_cast<VideoOutOutputOptions*>(unmapped);
    auto* const color = static_cast<VideoOutColorSettings*>(unmapped);
    auto* const conversion = static_cast<VideoOutColorSpaceConversionSettings*>(unmapped);

    Require(Throws([&] { sceVideoOutSetBufferAttribute2(attribute2, 0x8000000000000000ull, 0, 64, 64, 0, 0, 0); }), "SetBufferAttribute2 wrote an unmapped attribute");
    Require(Throws([&] { sceVideoOutAddBufferAttributeOption(attribute2, 0); }), "AddBufferAttributeOption read an unmapped attribute");
    Require(Throws([&] { sceVideoOutRemoveBufferAttributeOption(attribute2, 0); }), "RemoveBufferAttributeOption read an unmapped attribute");
    Require(Throws([&] { sceVideoOutSetBufferAttribute(attribute, 0x80000000u, 0, 0, 64, 64, 64); }), "SetBufferAttribute wrote an unmapped attribute");
    Require(Throws([&] { sceVideoOutGetEventId(event); }), "GetEventId read an unmapped event");
    Require(Throws([&] { sceVideoOutGetEventCount(event); }), "GetEventCount read an unmapped event");
    Require(Throws([&] { sceVideoOutGetEventData(event, data); }), "GetEventData read an unmapped event");
    KernelEvent valid{};
    valid.filter = EVFILT_VIDEO_OUT;
    Require(Throws([&] { sceVideoOutGetEventData(&valid, data); }), "GetEventData wrote an unmapped output");
    Require(Throws([&] { sceVideoOutSysInitMode(unmapped); }), "SysInitMode wrote an unmapped mode");
    Require(sceVideoOutInitializeOutputOptions(options) == InvalidAddress, "InitializeOutputOptions wrote an unmapped options block");
    Require(Throws([&] { sceVideoOutColorSettingsSetGamma(color, 1.0f); }), "ColorSettingsSetGamma wrote an unmapped setting");
    Require(Throws([&] { sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(conversion); }), "the conversion defaults wrote an unmapped setting");
    std::puts("VideoOut pointer fault tests passed");
}
