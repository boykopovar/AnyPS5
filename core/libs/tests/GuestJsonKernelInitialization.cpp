#include "prx/libSceJson2/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <new>
#include <stdexcept>

using namespace SceJson2;

struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
static void Require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
static std::uint32_t Sdk = 0x06000000;
extern "C" const void* ApplicationProcessParameters_nid_no_patch() {
    static std::uint32_t words[]{0, 0, 0, 0, 0};
    words[4] = Sdk;
    return words;
}
struct GuestTrace { std::map<void*, std::size_t> live; std::size_t generation = 0; };
static void* APS5_VABI AllocateTracked(void*, std::size_t bytes, void* context) {
    auto& trace = *static_cast<GuestTrace*>(context);
    void* memory = std::malloc(bytes);
    Require(memory != nullptr, "Tracking allocator requires host memory");
    std::memset(memory, 0xa5, bytes);
    Require(trace.live.emplace(memory, ++trace.generation).second, "Guest allocation identities must be independently live");
    return memory;
}
static void APS5_VABI FreeTracked(void*, void* memory, void* context) {
    if (!memory) return;
    Require(static_cast<GuestTrace*>(context)->live.erase(memory) == 1, "Each guest resource must return to its allocator exactly once");
    std::free(memory);
}
static void APS5_VABI NotifyTracked(void*, int, std::size_t, void*) {
    Require(false, "Host kernel failures must not fabricate guest allocator failures");
}
static void* const Vtable[]{nullptr, nullptr, reinterpret_cast<void*>(AllocateTracked), reinterpret_cast<void*>(FreeTracked), reinterpret_cast<void*>(NotifyTracked)};
struct Allocator { void* const* vtable = Vtable; };
enum class Failure { None, AttributeError, AttributeException, TypeError, MutexError, MutexException };
struct KernelTrace {
    Failure failure = Failure::None;
    unsigned target = 0;
    unsigned attempts = 0;
    unsigned attributesCreated = 0;
    unsigned attributesDestroyed = 0;
    unsigned mutexesCreated = 0;
    unsigned mutexesDestroyed = 0;
    std::map<PthreadMutexattr, int> attributes;
    std::map<PthreadMutex, unsigned> mutexes;
};
static KernelTrace Kernel;
static bool At(Failure failure) { return Kernel.failure == failure && Kernel.attempts == Kernel.target; }
extern "C" int APS5_VABI scePthreadMutexattrInit(PthreadMutexattr* attribute) {
    ++Kernel.attempts;
    if (At(Failure::AttributeError)) return static_cast<int>(0x8002000c);
    if (At(Failure::AttributeException)) throw std::bad_alloc();
    *attribute = reinterpret_cast<PthreadMutexattr>(new int(0));
    Require(Kernel.attributes.emplace(*attribute, 0).second, "Kernel attributes must have independent live identities");
    ++Kernel.attributesCreated;
    return 0;
}
extern "C" int APS5_VABI scePthreadMutexattrSettype(PthreadMutexattr* attribute, int type) {
    Require(type == 2 && Kernel.attributes.count(*attribute), "Json2 mutexes must request recursive attributes");
    if (At(Failure::TypeError)) return static_cast<int>(0x80020016);
    Kernel.attributes.at(*attribute) = type;
    return 0;
}
extern "C" int APS5_VABI scePthreadMutexattrDestroy(PthreadMutexattr* attribute) {
    Require(Kernel.attributes.erase(*attribute) == 1, "Kernel attributes must be destroyed exactly once");
    delete reinterpret_cast<int*>(*attribute);
    *attribute = nullptr;
    ++Kernel.attributesDestroyed;
    return 0;
}
extern "C" int APS5_VABI scePthreadMutexInit(PthreadMutex* mutex, const PthreadMutexattr* attribute, const char* name) {
    Require(Kernel.attributes.at(*attribute) == 2 && std::strcmp(name, "SceJson_internal") == 0 && !*mutex, "Mutex creation must receive recursive attributes and initialized handle storage");
    if (At(Failure::MutexError)) return static_cast<int>(0x8002000c);
    if (At(Failure::MutexException)) throw std::bad_alloc();
    *mutex = reinterpret_cast<PthreadMutex>(new std::recursive_mutex);
    Require(Kernel.mutexes.emplace(*mutex, 0).second, "Kernel mutexes must have independent live identities");
    ++Kernel.mutexesCreated;
    return 0;
}
extern "C" int APS5_VABI scePthreadMutexDestroy(PthreadMutex* mutex) {
    Require(Kernel.mutexes.count(*mutex) && Kernel.mutexes.at(*mutex) == 0, "Only a live unlocked kernel mutex may be destroyed");
    Kernel.mutexes.erase(*mutex);
    delete reinterpret_cast<std::recursive_mutex*>(*mutex);
    *mutex = reinterpret_cast<PthreadMutex>(std::uintptr_t{2});
    ++Kernel.mutexesDestroyed;
    return 0;
}
extern "C" int APS5_VABI scePthreadMutexLock(PthreadMutex* mutex) {
    Require(Kernel.mutexes.count(*mutex), "Only a live kernel mutex may be locked");
    reinterpret_cast<std::recursive_mutex*>(*mutex)->lock();
    ++Kernel.mutexes.at(*mutex);
    return 0;
}
extern "C" int APS5_VABI scePthreadMutexUnlock(PthreadMutex* mutex) {
    Require(Kernel.mutexes.count(*mutex) && Kernel.mutexes.at(*mutex), "Kernel unlocking must balance an owned lock");
    --Kernel.mutexes.at(*mutex);
    reinterpret_cast<std::recursive_mutex*>(*mutex)->unlock();
    return 0;
}
static void Check(Failure failure, unsigned target) {
    Kernel = {}; Kernel.failure = failure; Kernel.target = target;
    GuestTrace guest; Allocator allocator; Parameter parameter{&allocator, &guest, 8192, 2, {}};
    std::uint8_t initializer = 0; _ZN3sce4Json11InitializerC1Ev(&initializer);
    bool failed = false;
    try {
        Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, reinterpret_cast<const InitParameter2*>(&parameter)) == 0 && failure == Failure::None, "A failed kernel setup must not report successful initialization");
    } catch (const std::bad_alloc&) {
        Require(failure == Failure::AttributeException || failure == Failure::MutexException, "Host allocation exceptions must retain their actual type");
        failed = true;
    } catch (const std::runtime_error& error) {
        Require((failure == Failure::AttributeError || failure == Failure::TypeError || failure == Failure::MutexError) && std::strstr(error.what(), "CreateKernelMutex"), "Unverified kernel status failures must remain explicitly unsupported");
        failed = true;
    }
    const unsigned mutexCount = Sdk < 0x06000000 ? 1u : 2u;
    if (failure == Failure::None) {
        Require(!failed && Kernel.mutexes.size() == mutexCount && Kernel.attributes.empty(), "Healthy initialization must own the SDK-specific recursive mutex resources");
        Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0, "Terminate healthy resources");
    } else {
        Require(failed && initializer == 0 && Kernel.attempts == target, "Kernel setup failure must leave the initializer inactive at the requested attempt");
    }
    Require(guest.live.empty() && Kernel.attributes.empty() && Kernel.mutexes.empty(), "Every kernel setup failure must roll back guest handles, attributes and previously created mutexes");
    Require(Kernel.attributesCreated == Kernel.attributesDestroyed && Kernel.mutexesCreated == Kernel.mutexesDestroyed, "Rollback must balance each kernel resource exactly once");
    const auto generation = guest.generation;
    Kernel.failure = Failure::None;
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, reinterpret_cast<const InitParameter2*>(&parameter)) == 0 && initializer == 1 && guest.generation > generation && Kernel.mutexes.size() == mutexCount, "Retry must admit a fresh resource generation using the same initializer");
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && initializer == 0 && guest.live.empty() && Kernel.attributes.empty() && Kernel.mutexes.empty(), "Retry teardown must reclaim complete guest and kernel ownership");
    Require(Kernel.attributesCreated == Kernel.attributesDestroyed && Kernel.mutexesCreated == Kernel.mutexesDestroyed, "Retry must preserve balanced kernel ownership");
}
int main() {
    for (auto sdk : {0x05000000u, 0x06000000u}) {
        Sdk = sdk;
        Check(Failure::None, 0);
        for (auto failure : {Failure::AttributeError, Failure::AttributeException, Failure::TypeError, Failure::MutexError, Failure::MutexException})
            for (unsigned target = 1; target <= (sdk < 0x06000000 ? 1u : 2u); ++target) Check(failure, target);
    }
    return 0;
}
