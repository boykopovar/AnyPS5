#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <unordered_map>

namespace {

std::unordered_map<void*, std::size_t> allocations;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc EchoCanceller3Factory check failed at line %u\n", location.line());
    std::abort();
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    auto* pointer = std::malloc(size);
    if (pointer) {
        allocations.emplace(pointer, size);
        std::memset(pointer, 0xa5, size);
    }
    return pointer;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    Require(allocations.erase(pointer) == 1);
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

template<typename TException, typename TAction>
void RequireThrows(TAction action, const char* text = "", std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const TException& error) {
        Require(std::strstr(error.what(), text) != nullptr, location);
        return;
    }
    Require(false, location);
}

const void* Entry(const WebRtcEchoCanceller3Factory* factory, std::size_t slot) {
    return static_cast<const void* const*>(factory->vtable)[slot];
}

template<std::size_t TSlot>
void CallDestructor(WebRtcEchoCanceller3Factory* factory) {
    using Function = void(APS5_VABI*)(WebRtcEchoCanceller3Factory*);
    reinterpret_cast<Function>(const_cast<void*>(Entry(factory, TSlot)))(factory);
}

WebRtcEchoCanceller3Config Source() {
    WebRtcEchoCanceller3Config config;
    _ZN6webrtc20EchoCanceller3ConfigC1Ev_nid_postfix(&config);
    for (std::size_t index = 0; index < sizeof(config.bytes); index += 37)
        config.bytes[index] = static_cast<unsigned char>(index * 7 + 1);
    return config;
}

void TestConstructor() {
    const WebRtcEchoCanceller3Config config = Source();
    WebRtcEchoCanceller3Factory factory;
    std::memset(&factory, 0xcc, sizeof(factory));
    _ZN6webrtc21EchoCanceller3FactoryC1ERKNS_20EchoCanceller3ConfigE_nid_postfix(&factory, &config);
    Require(factory.vtable != nullptr);
    Require(std::memcmp(factory.config.bytes, config.bytes, sizeof(config.bytes)) == 0);
    WebRtcEchoCanceller3Config copied;
    _ZN6webrtc20EchoCanceller3ConfigC1ERKS0__nid_postfix(&copied, &config);
    Require(std::memcmp(factory.config.bytes, copied.bytes, sizeof(copied.bytes)) == 0);
    WebRtcEchoCanceller3Factory second;
    _ZN6webrtc21EchoCanceller3FactoryC1ERKNS_20EchoCanceller3ConfigE_nid_postfix(&second, &config);
    Require(second.vtable == factory.vtable);
    CallDestructor<0>(&factory);
    Require(std::memcmp(factory.config.bytes, config.bytes, sizeof(config.bytes)) == 0);
    Require(allocations.empty());
}

void TestCreate() {
    const WebRtcEchoCanceller3Config config = Source();
    WebRtcEchoCanceller3Factory factory;
    _ZN6webrtc21EchoCanceller3FactoryC1ERKNS_20EchoCanceller3ConfigE_nid_postfix(&factory, &config);
    void* result = nullptr;
    const unsigned char environment[24] = {};
    using CreateWithEnvironment = void*(APS5_VABI*)(void**, WebRtcEchoCanceller3Factory*, const void*, int, int, int);
    using Create = void*(APS5_VABI*)(void**, WebRtcEchoCanceller3Factory*, int, int, int);
    RequireThrows<std::runtime_error>([&] {
        reinterpret_cast<CreateWithEnvironment>(const_cast<void*>(Entry(&factory, 2)))(&result, &factory, environment, 48000, 2, 1);
    }, "EchoCanceller3Factory::Create(const Environment&");
    RequireThrows<std::runtime_error>([&] {
        reinterpret_cast<Create>(const_cast<void*>(Entry(&factory, 3)))(&result, &factory, 48000, 2, 1);
    }, "EchoCanceller3Factory::Create(int, int, int)");
    Require(result == nullptr && allocations.empty());
}

void TestDelete() {
    const WebRtcEchoCanceller3Config config = Source();
    auto* factory = static_cast<WebRtcEchoCanceller3Factory*>(_Znwm_nid_postfix(sizeof(WebRtcEchoCanceller3Factory)));
    Require(allocations.size() == 1);
    _ZN6webrtc21EchoCanceller3FactoryC1ERKNS_20EchoCanceller3ConfigE_nid_postfix(factory, &config);
    Require(std::memcmp(factory->config.bytes, config.bytes, sizeof(config.bytes)) == 0);
    CallDestructor<1>(factory);
    Require(allocations.empty());

    WebRtcEchoCanceller3Factory local;
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc21EchoCanceller3FactoryC1ERKNS_20EchoCanceller3ConfigE_nid_postfix(nullptr, &config); });
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc21EchoCanceller3FactoryC1ERKNS_20EchoCanceller3ConfigE_nid_postfix(&local, nullptr); });
}

}

int main(int argc, char** argv) {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    try {
        Require(argc == 2);
        if (std::strcmp(argv[1], "constructor") == 0) TestConstructor();
        else if (std::strcmp(argv[1], "create") == 0) TestCreate();
        else if (std::strcmp(argv[1], "delete") == 0) TestDelete();
        else Require(false);
        Require(allocations.empty());
    } catch (const std::exception& error) {
        std::fprintf(stderr, "EchoCanceller3Factory exception: %s\n", error.what());
        return 1;
    }
}
