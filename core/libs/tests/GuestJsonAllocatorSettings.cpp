#include "prx/libSceJson2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "JsonTestAllocator.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace SceJson2;

using Allocate = void* (APS5_VABI*)(std::size_t, void*);
using Free = void (APS5_VABI*)(void*, void*);
using Notify = void (APS5_VABI*)(int, std::size_t, void*);
struct Callbacks { Allocate allocate; Free free; Notify notify; };
struct Parameter {
    void* allocator;
    void* context;
    std::size_t bufferSize;
    std::int32_t mode;
    std::uint32_t reserved[3];
};
static_assert(sizeof(Parameter) == 40);
static void Require(bool valid, const char* message) {
    if (!valid) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
struct Context { std::size_t size = 0; int error = 0; void* memory = nullptr; int calls = 0; };
static void* APS5_VABI AllocateMemory(std::size_t size, void* context) {
    auto& trace = *static_cast<Context*>(context);
    trace.size = size;
    ++trace.calls;
    trace.memory = std::malloc(size);
    return trace.memory;
}
static void APS5_VABI FreeMemory(void* memory, void* context) {
    auto& trace = *static_cast<Context*>(context);
    Require(memory == trace.memory, "The adapter must forward the allocation to the matching context");
    ++trace.calls;
    std::free(memory);
    trace.memory = nullptr;
}
static void APS5_VABI NotifyError(int error, std::size_t size, void* context) {
    auto& trace = *static_cast<Context*>(context);
    trace.error = error;
    trace.size = size;
    ++trace.calls;
}
static void* APS5_VABI AllocateReplacement(std::size_t size, void* context) {
    return AllocateMemory(size + 1, context);
}
static void Exercise(Parameter& parameter, Context& trace, bool replacement) {
    auto** table = *reinterpret_cast<void***>(parameter.allocator);
    const auto allocate = reinterpret_cast<void* (APS5_VABI*)(void*, std::size_t, void*)>(table[2]);
    const auto free = reinterpret_cast<void (APS5_VABI*)(void*, void*, void*)>(table[3]);
    const auto notify = reinterpret_cast<void (APS5_VABI*)(void*, int, std::size_t, void*)>(table[4]);
    const int before = trace.calls;
    void* memory = allocate(parameter.allocator, 127, parameter.context);
    Require(memory && trace.size == (replacement ? 128 : 127), "The RTTI adapter must dispatch allocation with the supplied size and context");
    free(parameter.allocator, memory, parameter.context);
    notify(parameter.allocator, static_cast<int>(0x80848102), 8193, parameter.context);
    Require(trace.calls == before + 3 && trace.error == static_cast<int>(0x80848102) && trace.size == 8193,
            "The RTTI adapter must forward free and notification exactly once");
}
int main() {
    Context first, second;
    Callbacks callbacks{AllocateMemory, FreeMemory, NotifyError};
    Parameter parameter{nullptr, nullptr, 8192, 1, {11, 12, 13}};
    _ZN3sce4Json18InitParameterRtti216setAllocatorRttiEPNS0_14AllocParamRttiEPv(reinterpret_cast<InitParameter2*>(&parameter), &callbacks, &first);
    Require(parameter.allocator && parameter.allocator != &callbacks && parameter.context == &first &&
                parameter.bufferSize == 8192 && parameter.mode == 1 && parameter.reserved[2] == 13,
            "The setter must install an adapter and preserve unrelated settings");
    callbacks = {};
    Exercise(parameter, first, false);
    const Parameter saved = parameter;
    for (Callbacks invalid : {Callbacks{}, Callbacks{AllocateMemory, nullptr, NotifyError},
                              Callbacks{nullptr, FreeMemory, NotifyError}}) {
        _ZN3sce4Json18InitParameterRtti216setAllocatorRttiEPNS0_14AllocParamRttiEPv(reinterpret_cast<InitParameter2*>(&parameter), &invalid, &second);
        Require(std::memcmp(&parameter, &saved, sizeof(saved)) == 0, "Invalid callbacks must preserve the parameter block");
        alignas(8) std::array<std::byte, 40> legacy;
        legacy.fill(std::byte{0xa5});
        _ZN3sce4Json17InitParameterRttiC1EPNS0_14AllocParamRttiEPvm(reinterpret_cast<InitParameterRtti*>(legacy.data()), &invalid, &second, 4096);
        for (std::size_t i = 0; i < legacy.size(); ++i)
            Require(legacy[i] == (i < 24 ? std::byte{} : std::byte{0xa5}), "Rejected legacy construction must clear only its 24-byte base");
    }
    _ZN3sce4Json18InitParameterRtti216setAllocatorRttiEPNS0_14AllocParamRttiEPv(reinterpret_cast<InitParameter2*>(&parameter), nullptr, &second);
    Require(std::memcmp(&parameter, &saved, sizeof(saved)) == 0, "A null RTTI setter argument must preserve settings");
    Callbacks replacement{AllocateReplacement, FreeMemory, NotifyError};
    Parameter newer{};
    _ZN3sce4Json18InitParameterRtti216setAllocatorRttiEPNS0_14AllocParamRttiEPv(reinterpret_cast<InitParameter2*>(&newer), &replacement, &second);
    Require(newer.allocator == parameter.allocator, "RTTI parameter blocks must share the module adapter");
    Exercise(parameter, first, true);
    Exercise(newer, second, true);
    JsonTest::Allocator allocator;
    Parameter regular{&allocator, nullptr, 0, 2, {}};
    std::uint8_t initializer = 0;
    _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, reinterpret_cast<const InitParameter2*>(&regular)) == 0, "Initialize the module");
    callbacks = {AllocateMemory, FreeMemory, NotifyError};
    _ZN3sce4Json18InitParameterRtti216setAllocatorRttiEPNS0_14AllocParamRttiEPv(reinterpret_cast<InitParameter2*>(&parameter), &callbacks, &second);
    Require(std::memcmp(&parameter, &saved, sizeof(saved)) == 0, "An active module must refuse RTTI reconfiguration");
    alignas(8) std::array<std::byte, 40> refused;
    refused.fill(std::byte{0xa5});
    _ZN3sce4Json17InitParameterRttiC1EPNS0_14AllocParamRttiEPvm(reinterpret_cast<InitParameterRtti*>(refused.data()), reinterpret_cast<void*>(std::uintptr_t{1}), &second, 4096);
    for (std::size_t i = 0; i < refused.size(); ++i)
        Require(refused[i] == (i < 24 ? std::byte{} : std::byte{0xa5}), "An active module must reject legacy configuration before reading callbacks");
    Exercise(parameter, first, true);
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0, "Terminate the module");
    alignas(8) std::array<std::byte, 40> legacy;
    legacy.fill(std::byte{0xa5});
    _ZN3sce4Json17InitParameterRttiC1EPNS0_14AllocParamRttiEPvm(reinterpret_cast<InitParameterRtti*>(legacy.data()), &callbacks, &first, 4096);
    Parameter stored{};
    std::memcpy(&stored, legacy.data(), 24);
    Require(stored.allocator == parameter.allocator && stored.context == &first && stored.bufferSize == 4096,
            "Legacy construction must install the adapter, context and buffer size");
    for (std::size_t i = 24; i < legacy.size(); ++i)
        Require(legacy[i] == std::byte{0xa5}, "Legacy construction must preserve trailing memory");
    Exercise(stored, first, false);
    callbacks.notify = nullptr;
    _ZN3sce4Json18InitParameterRtti216setAllocatorRttiEPNS0_14AllocParamRttiEPv(reinterpret_cast<InitParameter2*>(&parameter), &callbacks, &first);
    auto** table = *reinterpret_cast<void***>(parameter.allocator);
    const auto notify = reinterpret_cast<void (APS5_VABI*)(void*, int, std::size_t, void*)>(table[4]);
    const int before = first.calls;
    notify(parameter.allocator, static_cast<int>(0x80848102), 512, parameter.context);
    Require(first.calls == before, "An optional null notifier must not dispatch a callback");
    return 0;
}
