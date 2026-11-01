#ifndef CORE_LIBS_PRX_LIBKERNEL_SYSTEM_INCLUDE_MACACTIVITY_HPP
#define CORE_LIBS_PRX_LIBKERNEL_SYSTEM_INCLUDE_MACACTIVITY_HPP

#include "prx/libkernel/System/include/HostPerformance.hpp"
#include <cstdint>
#include <string>

namespace HostPerformance {

constexpr std::uint64_t ActivityIdleSystemSleepDisabled = 1ULL << 20;
constexpr std::uint64_t ActivityUserInitiated = 0x00FFFFFFULL | ActivityIdleSystemSleepDisabled;
constexpr std::uint64_t ActivityLatencyCritical = 0xFF00000000ULL;

struct ObjcLibraries {
    std::string Runtime = "/usr/lib/libobjc.A.dylib";
    std::string Foundation = "/System/Library/Frameworks/Foundation.framework/Foundation";
};

class MacActivity final : public IBackend {
public:
    explicit MacActivity(ObjcLibraries libraries = {});
    ~MacActivity() override;
    MacActivity(const MacActivity&) = delete;
    MacActivity& operator=(const MacActivity&) = delete;

    const char* Name() const override;
    BeginResult Begin() override;
    void End() override;

private:
    using GetClass = void* (*)(const char*);
    using RegisterSelector = void* (*)(const char*);
    using PoolPush = void* (*)();
    using PoolPop = void (*)(void*);
    using SendObject = void* (*)(void*, void*);
    using SendVoid = void (*)(void*, void*);
    using SendString = void* (*)(void*, void*, const char*);
    using SendBegin = void* (*)(void*, void*, std::uint64_t, void*);
    using SendEnd = void (*)(void*, void*, void*);

    ObjcLibraries libraries;
    void* runtime = nullptr;
    void* foundation = nullptr;
    GetClass getClass = nullptr;
    RegisterSelector registerSelector = nullptr;
    PoolPush poolPush = nullptr;
    PoolPop poolPop = nullptr;
    void* send = nullptr;
    void* processInfo = nullptr;
    void* token = nullptr;

    BeginResult Load();
};

}

#endif
