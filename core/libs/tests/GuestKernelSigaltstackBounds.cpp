#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <thread>

static void Require(bool condition) {
    if (!condition) std::abort();
}

static GuestStack Query() {
    GuestStack current{};
    std::memset(&current, 0xa5, sizeof(current));
    Require(sigaltstack_nid_postfix(nullptr, &current) == 0);
    return current;
}

static void CheckInitialState() {
    const GuestStack current = Query();
    Require(current.sp == nullptr);
    Require(current.size == 0);
    Require(current.flags == 4);
    Require(sigaltstack_nid_postfix(nullptr, nullptr) == 0);
    const GuestStack afterNull = Query();
    Require(afterNull.sp == nullptr);
    Require(afterNull.size == 0);
    Require(afterNull.flags == 4);
}

static void CheckFaultRejection() {
    const auto unreadableStack = reinterpret_cast<const GuestStack*>(std::uintptr_t{0x10});
    const auto unreadablePrev = reinterpret_cast<GuestStack*>(std::uintptr_t{0x10});
    *__error_nid_postfix() = 0;
    Require(sigaltstack_nid_postfix(unreadableStack, nullptr) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sigaltstack_nid_postfix(nullptr, unreadablePrev) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sigaltstack_nid_postfix(unreadableStack, unreadablePrev) == -1 && *__error_nid_postfix() == 14);

    GuestStack untouched{};
    std::memset(&untouched, 0x5a, sizeof(untouched));
    GuestStack previous = untouched;
    *__error_nid_postfix() = 0;
    Require(sigaltstack_nid_postfix(unreadableStack, &previous) == -1 && *__error_nid_postfix() == 14);
    Require(std::memcmp(&previous, &untouched, sizeof(previous)) == 0);

    constexpr std::size_t page = 0x4000;
    void* readOnlyMem = nullptr;
    Require(sceKernelMapFlexibleMemory(&readOnlyMem, page, 3, 0) == 0 && readOnlyMem != nullptr);
    Require(sceKernelMprotect(readOnlyMem, page, 1) == 0);
    const auto readOnlyPrev = static_cast<GuestStack*>(readOnlyMem);
    *__error_nid_postfix() = 0;
    Require(sigaltstack_nid_postfix(nullptr, readOnlyPrev) == -1 && *__error_nid_postfix() == 14);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(readOnlyMem), page) == 0);

    void* protNoneMem = nullptr;
    Require(sceKernelMapFlexibleMemory(&protNoneMem, page, 3, 0) == 0 && protNoneMem != nullptr);
    Require(sceKernelMprotect(protNoneMem, page, 0) == 0);
    const auto protNoneStack = static_cast<const GuestStack*>(protNoneMem);
    const auto protNonePrev = static_cast<GuestStack*>(protNoneMem);
    *__error_nid_postfix() = 0;
    Require(sigaltstack_nid_postfix(protNoneStack, nullptr) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sigaltstack_nid_postfix(nullptr, protNonePrev) == -1 && *__error_nid_postfix() == 14);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(protNoneMem), page) == 0);

    const GuestStack current = Query();
    Require(current.sp == nullptr && current.size == 0 && current.flags == 4);
}

static void CheckFlagBounds(char* area, std::size_t areaSize) {
    constexpr int ssOnStack = 1;
    constexpr int ssDisable = 4;
    constexpr int errorInval = 22;

    const int invalidFlags[] = {
        ssOnStack,
        ssOnStack | ssDisable,
        2,
        8,
        16,
        32,
        64,
        128,
        256,
        0x1000,
        0x8000,
        -1,
        ~ssDisable
    };

    for (int flag : invalidFlags) {
        const GuestStack invalidStack{area, areaSize, flag};
        GuestStack untouched{};
        std::memset(&untouched, 0x5a, sizeof(untouched));
        GuestStack previous = untouched;
        *__error_nid_postfix() = 0;
        Require(sigaltstack_nid_postfix(&invalidStack, &previous) == -1 && *__error_nid_postfix() == errorInval);
        Require(std::memcmp(&previous, &untouched, sizeof(previous)) == 0);
    }

    const GuestStack onStackZeroSize{nullptr, 0, ssOnStack};
    *__error_nid_postfix() = 0;
    Require(sigaltstack_nid_postfix(&onStackZeroSize, nullptr) == -1 && *__error_nid_postfix() == errorInval);

    const GuestStack onStackSubMinSize{area, 100, ssOnStack};
    *__error_nid_postfix() = 0;
    Require(sigaltstack_nid_postfix(&onStackSubMinSize, nullptr) == -1 && *__error_nid_postfix() == errorInval);

    const GuestStack onStackAndDisable{area, 0, ssOnStack | ssDisable};
    *__error_nid_postfix() = 0;
    Require(sigaltstack_nid_postfix(&onStackAndDisable, nullptr) == -1 && *__error_nid_postfix() == errorInval);

    const GuestStack current = Query();
    Require(current.sp == nullptr && current.size == 0 && current.flags == ssDisable);
}

static void CheckMinimumStackSize(char* area, std::size_t areaSize) {
    constexpr std::size_t minSignalStackSize = 2048;
    constexpr int errorNoMem = 12;

    const std::size_t subMinSizes[] = {0, 1, 2, 16, 64, 256, 512, 1024, 2047};
    for (std::size_t size : subMinSizes) {
        const GuestStack badSize{area, size, 0};
        GuestStack untouched{};
        std::memset(&untouched, 0x5a, sizeof(untouched));
        GuestStack previous = untouched;
        *__error_nid_postfix() = 0;
        Require(sigaltstack_nid_postfix(&badSize, &previous) == -1 && *__error_nid_postfix() == errorNoMem);
        Require(std::memcmp(&previous, &untouched, sizeof(previous)) == 0);
    }

    const GuestStack current = Query();
    Require(current.sp == nullptr && current.size == 0 && current.flags == 4);

    GuestStack previous{};
    const GuestStack exactMin{area, minSignalStackSize, 0};
    *__error_nid_postfix() = 0;
    Require(sigaltstack_nid_postfix(&exactMin, &previous) == 0);
    Require(previous.sp == nullptr && previous.size == 0 && previous.flags == 4);

    GuestStack installed = Query();
    Require(installed.sp == area && installed.size == minSignalStackSize && installed.flags == 0);

    const GuestStack minPlusOne{area, minSignalStackSize + 1, 0};
    Require(sigaltstack_nid_postfix(&minPlusOne, &previous) == 0);
    Require(previous.sp == area && previous.size == minSignalStackSize && previous.flags == 0);

    installed = Query();
    Require(installed.sp == area && installed.size == minSignalStackSize + 1 && installed.flags == 0);

    const GuestStack fullArea{area, areaSize, 0};
    Require(sigaltstack_nid_postfix(&fullArea, &previous) == 0);
    Require(previous.sp == area && previous.size == minSignalStackSize + 1 && previous.flags == 0);

    installed = Query();
    Require(installed.sp == area && installed.size == areaSize && installed.flags == 0);
}

static void CheckDisableTransitions(char* area, std::size_t areaSize) {
    constexpr int ssDisable = 4;
    constexpr int errorNoMem = 12;

    GuestStack previous{};
    const GuestStack disableStack{nullptr, 0, ssDisable};
    Require(sigaltstack_nid_postfix(&disableStack, &previous) == 0);
    Require(previous.sp == area && previous.size == areaSize && previous.flags == 0);

    GuestStack current = Query();
    Require(current.sp == area && current.size == areaSize && current.flags == ssDisable);

    const GuestStack smallUnderflow{area, 512, 0};
    *__error_nid_postfix() = 0;
    Require(sigaltstack_nid_postfix(&smallUnderflow, &previous) == -1 && *__error_nid_postfix() == errorNoMem);
    current = Query();
    Require(current.sp == area && current.size == areaSize && current.flags == ssDisable);

    const GuestStack disableWithSubMinSize{area, 100, ssDisable};
    Require(sigaltstack_nid_postfix(&disableWithSubMinSize, &previous) == 0);
    Require(previous.sp == area && previous.size == areaSize && previous.flags == ssDisable);
    current = Query();
    Require(current.sp == area && current.size == areaSize && current.flags == ssDisable);

    const GuestStack reenable{area, areaSize, 0};
    Require(sigaltstack_nid_postfix(&reenable, &previous) == 0);
    Require(previous.sp == area && previous.size == areaSize && previous.flags == ssDisable);
    current = Query();
    Require(current.sp == area && current.size == areaSize && current.flags == 0);

    const GuestStack disableWithSp{area, areaSize, ssDisable};
    Require(sigaltstack_nid_postfix(&disableWithSp, &previous) == 0);
    Require(previous.sp == area && previous.size == areaSize && previous.flags == 0);
    current = Query();
    Require(current.sp == area && current.size == areaSize && current.flags == ssDisable);
}

static void CheckThreadIsolation() {
    alignas(16) static char mainArea[4096];
    const GuestStack mainStack{mainArea, sizeof(mainArea), 0};
    Require(sigaltstack_nid_postfix(&mainStack, nullptr) == 0);

    std::thread([] {
        const GuestStack threadInitial = Query();
        Require(threadInitial.sp == nullptr && threadInitial.size == 0 && threadInitial.flags == 4);

        alignas(16) static char childArea[4096];
        const GuestStack childStack{childArea, sizeof(childArea), 0};
        GuestStack childPrev{};
        Require(sigaltstack_nid_postfix(&childStack, &childPrev) == 0);
        Require(childPrev.sp == nullptr && childPrev.size == 0 && childPrev.flags == 4);

        const GuestStack childCurrent = Query();
        Require(childCurrent.sp == childArea && childCurrent.size == sizeof(childArea) && childCurrent.flags == 0);

        const GuestStack childDisable{nullptr, 0, 4};
        Require(sigaltstack_nid_postfix(&childDisable, nullptr) == 0);
        const GuestStack childDisabled = Query();
        Require(childDisabled.flags == 4);
    }).join();

    const GuestStack mainCurrent = Query();
    Require(mainCurrent.sp == mainArea && mainCurrent.size == sizeof(mainArea) && mainCurrent.flags == 0);

    const GuestStack disableMain{nullptr, 0, 4};
    Require(sigaltstack_nid_postfix(&disableMain, nullptr) == 0);
    const GuestStack mainDisabled = Query();
    Require(mainDisabled.flags == 4);
}

int main() {
    alignas(16) static char stackBuffer[8192];
    CheckInitialState();
    CheckFaultRejection();
    CheckFlagBounds(stackBuffer, sizeof(stackBuffer));
    CheckMinimumStackSize(stackBuffer, sizeof(stackBuffer));
    CheckDisableTransitions(stackBuffer, sizeof(stackBuffer));
    CheckThreadIsolation();
    return 0;
}
