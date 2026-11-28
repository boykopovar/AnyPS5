#include "prx/libwebrtc/libwebrtc.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition)
        return;
    std::fprintf(stderr, "libwebrtc CallConfig check failed at line %u\n", location.line());
    std::abort();
}

template<typename TCall>
bool ThrowsInvalid(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

struct ReadOnlyPage {
    void* address = nullptr;
    std::size_t size = 0;

    ReadOnlyPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        address = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        Require(address != nullptr);
        std::memset(address, 0, size);
        DWORD oldProtect = 0;
        Require(VirtualProtect(address, size, PAGE_READONLY, &oldProtect) != 0);
#else
        size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        address = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(address != MAP_FAILED);
        Require(mprotect(address, size, PROT_READ) == 0);
#endif
    }

    ~ReadOnlyPage() {
        if (!address)
            return;
#ifdef _WIN32
        VirtualFree(address, 0, MEM_RELEASE);
#else
        munmap(address, size);
#endif
    }
};

struct FakeOwned {
    const void* const* vtable;
    char tag;
    WebRtcCallConfig* owner;
};

struct FakeStorage {
    const void* const* vtable;
    std::int32_t refCount;
    char tag;
    WebRtcCallConfig* owner;
};

static_assert(offsetof(FakeStorage, vtable) == offsetof(WebRtcRefCountedBase, vtable));
static_assert(offsetof(FakeStorage, refCount) == offsetof(WebRtcRefCountedBase, refCount));

std::string events;

void APS5_VABI UnexpectedCall(void*) {
    std::abort();
}

void APS5_VABI FakeDeleteController(FakeOwned* object) {
    Require(object != nullptr);
    if (object->owner)
        Require(object->owner->perCallNetworkControllerFactory == nullptr);
    events.push_back('C');
    events.push_back(object->tag);
}

std::int32_t APS5_VABI FakeReleaseAudioState(const FakeOwned* object) {
    Require(object != nullptr);
    if (object->owner)
        Require(object->owner->audioState == nullptr);
    events.push_back('A');
    events.push_back(object->tag);
    return 1;
}

void APS5_VABI FakeDeleteStorage(FakeStorage* storage) {
    Require(storage != nullptr);
    Require(storage->refCount == 0);
    if (storage->owner)
        Require(storage->owner->env.storage == nullptr);
    events.push_back('S');
    events.push_back(storage->tag);
}

const void* const ControllerVTable[2] = {
    reinterpret_cast<const void*>(&UnexpectedCall),
    reinterpret_cast<const void*>(&FakeDeleteController),
};

const void* const AudioStateVTable[2] = {
    reinterpret_cast<const void*>(&UnexpectedCall),
    reinterpret_cast<const void*>(&FakeReleaseAudioState),
};

const void* const StorageVTable[2] = {
    reinterpret_cast<const void*>(&UnexpectedCall),
    reinterpret_cast<const void*>(&FakeDeleteStorage),
};

const void* const NullSlot1VTable[2] = {
    reinterpret_cast<const void*>(&UnexpectedCall),
    nullptr,
};

void TestConstructor() {
    FakeStorage storage{StorageVTable, 1, '1', nullptr};
    WebRtcEnvironment env{
        &storage,
        reinterpret_cast<const void*>(static_cast<std::uintptr_t>(0x1010)),
        reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1020)),
        reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1030)),
        reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1040)),
    };
    auto* const taskQueue = reinterpret_cast<WebRtcTaskQueueBase*>(static_cast<std::uintptr_t>(0x2000));

    WebRtcCallConfig config{};
    std::memset(&config, 0xCC, sizeof(config));
    errno = EINVAL;
    _ZN6webrtc10CallConfigC1ERKNS_11EnvironmentEPNS_13TaskQueueBaseE_nid_postfix(&config, &env, taskQueue);
    Require(errno == EINVAL);
    Require(storage.refCount == 2);
    Require(config.env.storage == &storage);
    Require(config.env.fieldTrials == env.fieldTrials);
    Require(config.env.clock == env.clock);
    Require(config.env.taskQueueFactory == env.taskQueueFactory);
    Require(config.env.eventLog == env.eventLog);
    Require(config.bitrateConfig.minBitrateBps == 0);
    Require(config.bitrateConfig.startBitrateBps == 300000);
    Require(config.bitrateConfig.maxBitrateBps == -1);
    Require(config.audioState == nullptr);
    Require(config.audioProcessing == nullptr);
    Require(config.fecControllerFactory == nullptr);
    Require(config.networkStatePredictorFactory == nullptr);
    Require(config.perCallNetworkControllerFactory == nullptr);
    Require(config.networkControllerFactory == nullptr);
    Require(config.netEqFactory == nullptr);
    Require(config.networkTaskQueue == taskQueue);
    Require(config.rtpTransportControllerSendFactory == nullptr);
    Require(config.decodeMetronome == nullptr);
    Require(config.encodeMetronome == nullptr);
    Require(!config.pacerBurstInterval.engaged);
    Require(!config.enableSendPacketBatching);

    events.clear();
    _ZN6webrtc10CallConfigD1Ev_nid_postfix(&config);
    Require(storage.refCount == 1);
    Require(events.empty());

    WebRtcEnvironment nullStorageEnv{nullptr, env.fieldTrials, env.clock, env.taskQueueFactory, env.eventLog};
    std::memset(&config, 0x55, sizeof(config));
    _ZN6webrtc10CallConfigC1ERKNS_11EnvironmentEPNS_13TaskQueueBaseE_nid_postfix(&config, &nullStorageEnv, nullptr);
    Require(config.env.storage == nullptr);
    Require(config.networkTaskQueue == nullptr);
    Require(config.bitrateConfig.startBitrateBps == 300000);
    _ZN6webrtc10CallConfigD1Ev_nid_postfix(&config);

    errno = EAGAIN;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc10CallConfigC1ERKNS_11EnvironmentEPNS_13TaskQueueBaseE_nid_postfix(nullptr, &env, taskQueue);
    }));
    Require(errno == EAGAIN);
    Require(storage.refCount == 1);

    Require(ThrowsInvalid([&] {
        _ZN6webrtc10CallConfigC1ERKNS_11EnvironmentEPNS_13TaskQueueBaseE_nid_postfix(&config, nullptr, taskQueue);
    }));

    Require(ThrowsInvalid([&] {
        _ZN6webrtc10CallConfigC1ERKNS_11EnvironmentEPNS_13TaskQueueBaseE_nid_postfix(&config, &config.env, taskQueue);
    }));

    ReadOnlyPage readOnly;
    errno = ENOENT;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc10CallConfigC1ERKNS_11EnvironmentEPNS_13TaskQueueBaseE_nid_postfix(
            static_cast<WebRtcCallConfig*>(readOnly.address), &env, taskQueue);
    }));
    Require(errno == ENOENT);
    Require(storage.refCount == 1);

    WebRtcEnvironment readOnlyStorageEnv = env;
    readOnlyStorageEnv.storage = readOnly.address;
    WebRtcCallConfig untouched{};
    std::memset(&untouched, 0x77, sizeof(untouched));
    WebRtcCallConfig snapshot = untouched;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc10CallConfigC1ERKNS_11EnvironmentEPNS_13TaskQueueBaseE_nid_postfix(&untouched, &readOnlyStorageEnv, taskQueue);
    }));
    Require(std::memcmp(&untouched, &snapshot, sizeof(untouched)) == 0);

    FakeStorage negativeStorage{StorageVTable, -1, 'n', nullptr};
    WebRtcEnvironment negativeEnv = env;
    negativeEnv.storage = &negativeStorage;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc10CallConfigC1ERKNS_11EnvironmentEPNS_13TaskQueueBaseE_nid_postfix(&untouched, &negativeEnv, taskQueue);
    }));
    Require(std::memcmp(&untouched, &snapshot, sizeof(untouched)) == 0);

    FakeStorage maxStorage{StorageVTable, std::numeric_limits<std::int32_t>::max(), 'm', nullptr};
    WebRtcEnvironment maxEnv = env;
    maxEnv.storage = &maxStorage;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc10CallConfigC1ERKNS_11EnvironmentEPNS_13TaskQueueBaseE_nid_postfix(&untouched, &maxEnv, taskQueue);
    }));
    Require(std::memcmp(&untouched, &snapshot, sizeof(untouched)) == 0);

    alignas(8) std::array<std::uint8_t, sizeof(FakeStorage) + 4> misaligned{};
    WebRtcEnvironment misalignedEnv = env;
    misalignedEnv.storage = misaligned.data() + 1;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc10CallConfigC1ERKNS_11EnvironmentEPNS_13TaskQueueBaseE_nid_postfix(&untouched, &misalignedEnv, taskQueue);
    }));
    Require(std::memcmp(&untouched, &snapshot, sizeof(untouched)) == 0);
}

void TestDestructor() {
    WebRtcCallConfig config{};
    FakeStorage storage{StorageVTable, 0, 's', &config};
    WebRtcEnvironment env{
        &storage,
        reinterpret_cast<const void*>(static_cast<std::uintptr_t>(0x3010)),
        reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x3020)),
        reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x3030)),
        reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x3040)),
    };
    _ZN6webrtc10CallConfigC1ERKNS_11EnvironmentEPNS_13TaskQueueBaseE_nid_postfix(&config, &env, nullptr);
    Require(storage.refCount == 1);

    FakeOwned controller{ControllerVTable, 'c', &config};
    FakeOwned audioState{AudioStateVTable, 'a', &config};
    config.perCallNetworkControllerFactory = &controller;
    config.audioState = &audioState;
    config.audioProcessing = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x4010));
    config.fecControllerFactory = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x4020));
    config.networkStatePredictorFactory = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x4030));
    config.networkControllerFactory = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x4040));
    config.netEqFactory = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x4050));
    config.rtpTransportControllerSendFactory = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x4060));
    config.decodeMetronome = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x4070));
    config.encodeMetronome = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x4080));

    events.clear();
    errno = ERANGE;
    _ZN6webrtc10CallConfigD1Ev_nid_postfix(&config);
    Require(errno == ERANGE);
    Require(events == "CcAaSs");
    Require(config.perCallNetworkControllerFactory == nullptr);
    Require(config.audioState == nullptr);
    Require(config.env.storage == nullptr);

    FakeStorage sharedStorage{StorageVTable, 1, 'q', &config};
    env.storage = &sharedStorage;
    _ZN6webrtc10CallConfigC1ERKNS_11EnvironmentEPNS_13TaskQueueBaseE_nid_postfix(&config, &env, nullptr);
    Require(sharedStorage.refCount == 2);
    config.perCallNetworkControllerFactory = &controller;
    config.audioState = &audioState;
    events.clear();
    _ZN6webrtc10CallConfigD1Ev_nid_postfix(&config);
    Require(events == "CcAa");
    Require(sharedStorage.refCount == 1);

    FakeStorage badZeroStorage{StorageVTable, 0, 'z', nullptr};
    FakeStorage badNullSlotStorage{NullSlot1VTable, 1, 'v', nullptr};
    FakeOwned badAudioState{NullSlot1VTable, 'x', nullptr};
    FakeOwned badController{nullptr, 'y', nullptr};

    WebRtcEnvironment nullEnv{nullptr, nullptr, nullptr, nullptr, nullptr};
    _ZN6webrtc10CallConfigC1ERKNS_11EnvironmentEPNS_13TaskQueueBaseE_nid_postfix(&config, &nullEnv, nullptr);
    config.perCallNetworkControllerFactory = &controller;
    config.audioState = &audioState;
    config.env.storage = &badZeroStorage;
    events.clear();
    errno = EDOM;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc10CallConfigD1Ev_nid_postfix(&config);
    }));
    Require(errno == EDOM);
    Require(events.empty());
    Require(config.perCallNetworkControllerFactory == &controller);
    Require(config.audioState == &audioState);

    config.env.storage = &badNullSlotStorage;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc10CallConfigD1Ev_nid_postfix(&config);
    }));
    Require(events.empty());

    config.env.storage = nullptr;
    config.audioState = &badAudioState;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc10CallConfigD1Ev_nid_postfix(&config);
    }));
    Require(events.empty());

    config.audioState = nullptr;
    config.perCallNetworkControllerFactory = &badController;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc10CallConfigD1Ev_nid_postfix(&config);
    }));
    Require(events.empty());

    errno = EEXIST;
    Require(ThrowsInvalid([] {
        _ZN6webrtc10CallConfigD1Ev_nid_postfix(nullptr);
    }));
    Require(errno == EEXIST);

    auto* const unmapped = reinterpret_cast<WebRtcCallConfig*>(static_cast<std::uintptr_t>(0x10));
    Require(ThrowsInvalid([&] {
        _ZN6webrtc10CallConfigD1Ev_nid_postfix(unmapped);
    }));

    ReadOnlyPage readOnly;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc10CallConfigD1Ev_nid_postfix(static_cast<WebRtcCallConfig*>(readOnly.address));
    }));
}

}

int main() {
    TestConstructor();
    TestDestructor();
    return 0;
}
