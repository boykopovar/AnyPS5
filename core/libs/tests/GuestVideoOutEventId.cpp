#include "prx/libSceVideoOut/include/Event.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
#include <stdexcept>
#include <string>

namespace {

constexpr int InvalidEvent = static_cast<int>(0x8029000Du);
constexpr int FlipEvent = 0;
constexpr std::int16_t GraphicsCoreFilter = -14;
constexpr std::int64_t Unwritten = 0x5a5a;

bool Returns(const std::string& what, const std::function<int()>& call, int expected) {
    try {
        const int result = call();
        if (result == expected) return true;
        std::fprintf(stderr, "%s returned 0x%08x, expected 0x%08x\n", what.c_str(), static_cast<unsigned>(result), static_cast<unsigned>(expected));
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s threw: %s\n", what.c_str(), error.what());
    }
    return false;
}

bool Throws(const std::string& what, const std::function<int()>& call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    std::fprintf(stderr, "%s returned instead of throwing\n", what.c_str());
    return false;
}

bool RejectsEventOfAnotherFilter(const std::string& what, const KernelEvent& event) {
    std::int64_t data = Unwritten;
    bool passed = Returns("sceVideoOutGetEventId of " + what, [&] { return sceVideoOutGetEventId(&event); }, InvalidEvent);
    passed = Returns("sceVideoOutGetEventData of " + what, [&] { return sceVideoOutGetEventData(&event, &data); }, InvalidEvent) && passed;
    passed = Returns("sceVideoOutGetEventCount of " + what, [&] { return sceVideoOutGetEventCount(&event); }, InvalidEvent) && passed;
    if (data == Unwritten) return passed;
    std::fprintf(stderr, "sceVideoOutGetEventData of %s wrote 0x%llx\n", what.c_str(), static_cast<unsigned long long>(data));
    return false;
}

}

int main() {
    KernelEvent user{};
    user.filter = EVFILT_USER;
    KernelEvent graphics{};
    graphics.filter = GraphicsCoreFilter;
    graphics.data = 1;
    KernelEvent unknown{};
    unknown.ident = 0x77;
    unknown.filter = EVFILT_VIDEO_OUT;
    KernelEvent wide{};
    wide.ident = 0x100000000;
    wide.filter = EVFILT_VIDEO_OUT;
    KernelEvent flip{};
    flip.ident = FlipEvent;
    flip.filter = EVFILT_VIDEO_OUT;
    flip.data = 0x12343000;

    bool passed = RejectsEventOfAnotherFilter("a user event", user);
    passed = RejectsEventOfAnotherFilter("a graphics core event", graphics) && passed;
    passed = Returns("sceVideoOutGetEventId of a VideoOut event with an unknown id", [&] { return sceVideoOutGetEventId(&unknown); }, InvalidEvent) && passed;
    passed = Returns("sceVideoOutGetEventId of a VideoOut event with an id above 32 bits", [&] { return sceVideoOutGetEventId(&wide); }, InvalidEvent) && passed;

    std::int64_t data = Unwritten;
    passed = Returns("sceVideoOutGetEventId of a flip event", [&] { return sceVideoOutGetEventId(&flip); }, FlipEvent) && passed;
    passed = Returns("sceVideoOutGetEventCount of a flip event", [&] { return sceVideoOutGetEventCount(&flip); }, 3) && passed;
    passed = Returns("sceVideoOutGetEventData of a flip event", [&] { return sceVideoOutGetEventData(&flip, &data); }, 0) && passed;
    if (data != 0x1234) {
        std::fprintf(stderr, "sceVideoOutGetEventData of a flip event wrote 0x%llx, expected 0x1234\n", static_cast<unsigned long long>(data));
        passed = false;
    }

    passed = Throws("sceVideoOutGetEventId of a null event", [] { return sceVideoOutGetEventId(nullptr); }) && passed;
    passed = Throws("sceVideoOutGetEventData of a null event", [&] { return sceVideoOutGetEventData(nullptr, &data); }) && passed;
    passed = Throws("sceVideoOutGetEventData with a null data pointer", [&] { return sceVideoOutGetEventData(&flip, nullptr); }) && passed;
    passed = Throws("sceVideoOutGetEventCount of a null event", [] { return sceVideoOutGetEventCount(nullptr); }) && passed;
    if (!passed) return 1;
    std::printf("guest VideoOut event getter tests passed\n");
    return 0;
}
