#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/PalSleepDisablerExports.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <unordered_set>

namespace {

using NKWebKit::PalAbi::SleepDisabler;
using NKWebKit::PalAbi::SleepDisablerType;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "PAL SleepDisabler check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

std::unordered_set<void*> guestAllocations;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* pointer = std::malloc(size);
    if (pointer) guestAllocations.insert(pointer);
    return pointer;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    Require(guestAllocations.erase(pointer) == 1);
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

template<typename Function> bool Throws(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void TestConstructAndDelete() {
    for (int variant = 0; variant < 2; ++variant) {
        for (SleepDisablerType type : {SleepDisablerType::System, SleepDisablerType::Display}) {
            auto* disabler = static_cast<SleepDisabler*>(_Znwm_nid_postfix(sizeof(SleepDisabler)));
            Require(guestAllocations.count(disabler) == 1);
            std::memset(disabler, 0xa5, sizeof(*disabler));
            (variant ? _ZN3PAL13SleepDisablerC2EPKcNS0_4TypeE : _ZN3PAL13SleepDisablerC1EPKcNS0_4TypeE)(disabler, "reason", type);
            Require(disabler->type == type);
            Require(disabler->vptr != nullptr);
            disabler->vptr->completeDestructor(disabler);
            Require(guestAllocations.count(disabler) == 1);
            disabler->vptr->deletingDestructor(disabler);
            Require(guestAllocations.count(disabler) == 0);
        }
    }
    auto* direct = static_cast<SleepDisabler*>(_Znwm_nid_postfix(sizeof(SleepDisabler)));
    _ZN3PAL13SleepDisablerC1EPKcNS0_4TypeE(direct, nullptr, SleepDisablerType::Display);
    Require(direct->type == SleepDisablerType::Display);
    _ZN3PAL13SleepDisablerD0Ev(direct);
    Require(guestAllocations.empty());
}

void TestNullObjects() {
    Require(Throws([] { _ZN3PAL13SleepDisablerC1EPKcNS0_4TypeE(nullptr, "reason", SleepDisablerType::System); }));
    Require(Throws([] { _ZN3PAL13SleepDisablerC2EPKcNS0_4TypeE(nullptr, "reason", SleepDisablerType::System); }));
    Require(Throws([] { _ZN3PAL13SleepDisablerD0Ev(nullptr); }));
}

void TestClientSlot() {
    void** slot = _ZN7WebCore19sleepDisablerClientEv();
    Require(slot != nullptr);
    Require(slot == _ZN7WebCore19sleepDisablerClientEv());
    Require(*slot == nullptr);
    int client = 0;
    *slot = &client;
    Require(*_ZN7WebCore19sleepDisablerClientEv() == &client);
    *slot = nullptr;
}

}  // namespace

int main() {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    TestConstructAndDelete();
    TestNullObjects();
    TestClientSlot();
    return 0;
}
