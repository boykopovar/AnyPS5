#include "prx/libwebrtc/libwebrtc.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
    std::fprintf(stderr, "libwebrtc PeerConnectionDependencies check failed at line %u\n", location.line());
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
    WebRtcPeerConnectionDependencies* owner;
    void* WebRtcPeerConnectionDependencies::* field;
};

std::string destroyed;

void APS5_VABI FakeCompleteDestructor(FakeOwned*) {
    std::abort();
}

void APS5_VABI FakeDeletingDestructor(FakeOwned* object) {
    Require(object != nullptr);
    if (object->owner && object->field)
        Require(object->owner->*(object->field) == nullptr);
    destroyed.push_back(object->tag);
}

const void* const ValidVTable[2] = {
    reinterpret_cast<const void*>(&FakeCompleteDestructor),
    reinterpret_cast<const void*>(&FakeDeletingDestructor),
};

const void* const NullDeletingVTable[2] = {
    reinterpret_cast<const void*>(&FakeCompleteDestructor),
    nullptr,
};

void TestObserverConstructor() {
    WebRtcPeerConnectionDependencies dependencies{};
    std::memset(&dependencies, 0xCC, sizeof(dependencies));
    void* const sentinelObserver = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1234));
    errno = EINVAL;
    _ZN6webrtc26PeerConnectionDependenciesC1EPNS_22PeerConnectionObserverE_nid_postfix(&dependencies, sentinelObserver);
    Require(errno == EINVAL);
    Require(dependencies.observer == sentinelObserver);
    Require(dependencies.allocator == nullptr);
    Require(dependencies.asyncDnsResolverFactory == nullptr);
    Require(dependencies.iceTransportFactory == nullptr);
    Require(dependencies.certGenerator == nullptr);
    Require(dependencies.tlsCertVerifier == nullptr);
    Require(dependencies.videoBitrateAllocatorFactory == nullptr);
    Require(dependencies.networkControllerFactory == nullptr);
    Require(dependencies.trials == nullptr);

    std::memset(&dependencies, 0x55, sizeof(dependencies));
    _ZN6webrtc26PeerConnectionDependenciesC1EPNS_22PeerConnectionObserverE_nid_postfix(&dependencies, nullptr);
    Require(dependencies.observer == nullptr);
    Require(dependencies.allocator == nullptr);
    Require(dependencies.trials == nullptr);

    errno = EAGAIN;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc26PeerConnectionDependenciesC1EPNS_22PeerConnectionObserverE_nid_postfix(nullptr, sentinelObserver);
    }));
    Require(errno == EAGAIN);

    auto* const unmapped = reinterpret_cast<WebRtcPeerConnectionDependencies*>(static_cast<std::uintptr_t>(0x10));
    errno = EBUSY;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc26PeerConnectionDependenciesC1EPNS_22PeerConnectionObserverE_nid_postfix(unmapped, sentinelObserver);
    }));
    Require(errno == EBUSY);

    ReadOnlyPage readOnly;
    errno = ENOENT;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc26PeerConnectionDependenciesC1EPNS_22PeerConnectionObserverE_nid_postfix(
            static_cast<WebRtcPeerConnectionDependencies*>(readOnly.address), sentinelObserver);
    }));
    Require(errno == ENOENT);
}

void TestMoveConstructor() {
    void* const observer = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x2000));
    WebRtcPeerConnectionDependencies source{};
    _ZN6webrtc26PeerConnectionDependenciesC1EPNS_22PeerConnectionObserverE_nid_postfix(&source, observer);
    FakeOwned objects[8]{};
    for (std::size_t i = 0; i < 8; ++i)
        objects[i] = {ValidVTable, static_cast<char>('0' + i), nullptr, nullptr};
    source.allocator = &objects[0];
    source.asyncDnsResolverFactory = &objects[1];
    source.iceTransportFactory = &objects[2];
    source.certGenerator = &objects[3];
    source.tlsCertVerifier = &objects[4];
    source.videoBitrateAllocatorFactory = &objects[5];
    source.networkControllerFactory = &objects[6];
    source.trials = &objects[7];

    WebRtcPeerConnectionDependencies destination{};
    std::memset(&destination, 0xAA, sizeof(destination));
    errno = EACCES;
    _ZN6webrtc26PeerConnectionDependenciesC1EOS0__nid_postfix(&destination, &source);
    Require(errno == EACCES);
    Require(destination.observer == observer);
    Require(destination.allocator == &objects[0]);
    Require(destination.asyncDnsResolverFactory == &objects[1]);
    Require(destination.iceTransportFactory == &objects[2]);
    Require(destination.certGenerator == &objects[3]);
    Require(destination.tlsCertVerifier == &objects[4]);
    Require(destination.videoBitrateAllocatorFactory == &objects[5]);
    Require(destination.networkControllerFactory == &objects[6]);
    Require(destination.trials == &objects[7]);

    Require(source.observer == observer);
    Require(source.allocator == nullptr);
    Require(source.asyncDnsResolverFactory == nullptr);
    Require(source.iceTransportFactory == nullptr);
    Require(source.certGenerator == nullptr);
    Require(source.tlsCertVerifier == nullptr);
    Require(source.videoBitrateAllocatorFactory == nullptr);
    Require(source.networkControllerFactory == nullptr);
    Require(source.trials == nullptr);

    destroyed.clear();
    _ZN6webrtc26PeerConnectionDependenciesD1Ev_nid_postfix(&source);
    Require(destroyed.empty());

    _ZN6webrtc26PeerConnectionDependenciesD1Ev_nid_postfix(&destination);
    Require(destroyed == "76543210");

    WebRtcPeerConnectionDependencies intact{};
    _ZN6webrtc26PeerConnectionDependenciesC1EPNS_22PeerConnectionObserverE_nid_postfix(&intact, observer);
    intact.allocator = &objects[0];
    intact.trials = &objects[7];

    errno = EPIPE;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc26PeerConnectionDependenciesC1EOS0__nid_postfix(nullptr, &intact);
    }));
    Require(errno == EPIPE);
    Require(intact.allocator == &objects[0]);
    Require(intact.trials == &objects[7]);

    Require(ThrowsInvalid([&] {
        _ZN6webrtc26PeerConnectionDependenciesC1EOS0__nid_postfix(&intact, nullptr);
    }));
    Require(intact.allocator == &objects[0]);

    Require(ThrowsInvalid([&] {
        _ZN6webrtc26PeerConnectionDependenciesC1EOS0__nid_postfix(&intact, &intact);
    }));
    Require(intact.allocator == &objects[0]);

    alignas(WebRtcPeerConnectionDependencies) std::array<std::uint8_t, sizeof(WebRtcPeerConnectionDependencies) + 16> overlap{};
    auto* const first = reinterpret_cast<WebRtcPeerConnectionDependencies*>(overlap.data());
    auto* const second = reinterpret_cast<WebRtcPeerConnectionDependencies*>(overlap.data() + 8);
    Require(ThrowsInvalid([&] {
        _ZN6webrtc26PeerConnectionDependenciesC1EOS0__nid_postfix(first, second);
    }));
    Require(ThrowsInvalid([&] {
        _ZN6webrtc26PeerConnectionDependenciesC1EOS0__nid_postfix(second, first);
    }));

    ReadOnlyPage readOnly;
    auto* const readOnlyDeps = static_cast<WebRtcPeerConnectionDependencies*>(readOnly.address);
    errno = EXDEV;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc26PeerConnectionDependenciesC1EOS0__nid_postfix(readOnlyDeps, &intact);
    }));
    Require(errno == EXDEV);
    Require(intact.allocator == &objects[0]);
    Require(intact.trials == &objects[7]);

    WebRtcPeerConnectionDependencies untouched{};
    std::memset(&untouched, 0x77, sizeof(untouched));
    WebRtcPeerConnectionDependencies snapshot = untouched;
    errno = ENOMEM;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc26PeerConnectionDependenciesC1EOS0__nid_postfix(&untouched, readOnlyDeps);
    }));
    Require(errno == ENOMEM);
    Require(std::memcmp(&untouched, &snapshot, sizeof(untouched)) == 0);

    auto* const unmapped = reinterpret_cast<WebRtcPeerConnectionDependencies*>(static_cast<std::uintptr_t>(0x10));
    Require(ThrowsInvalid([&] {
        _ZN6webrtc26PeerConnectionDependenciesC1EOS0__nid_postfix(&untouched, unmapped);
    }));
    Require(std::memcmp(&untouched, &snapshot, sizeof(untouched)) == 0);
}

void TestDestructor() {
    WebRtcPeerConnectionDependencies dependencies{};
    void* const sentinelObserver = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1234));
    _ZN6webrtc26PeerConnectionDependenciesC1EPNS_22PeerConnectionObserverE_nid_postfix(&dependencies, sentinelObserver);

    FakeOwned objects[8] = {
        {ValidVTable, 'A', &dependencies, &WebRtcPeerConnectionDependencies::allocator},
        {ValidVTable, 'B', &dependencies, &WebRtcPeerConnectionDependencies::asyncDnsResolverFactory},
        {ValidVTable, 'C', &dependencies, &WebRtcPeerConnectionDependencies::iceTransportFactory},
        {ValidVTable, 'D', &dependencies, &WebRtcPeerConnectionDependencies::certGenerator},
        {ValidVTable, 'E', &dependencies, &WebRtcPeerConnectionDependencies::tlsCertVerifier},
        {ValidVTable, 'F', &dependencies, &WebRtcPeerConnectionDependencies::videoBitrateAllocatorFactory},
        {ValidVTable, 'G', &dependencies, &WebRtcPeerConnectionDependencies::networkControllerFactory},
        {ValidVTable, 'H', &dependencies, &WebRtcPeerConnectionDependencies::trials},
    };
    dependencies.allocator = &objects[0];
    dependencies.asyncDnsResolverFactory = &objects[1];
    dependencies.iceTransportFactory = &objects[2];
    dependencies.certGenerator = &objects[3];
    dependencies.tlsCertVerifier = &objects[4];
    dependencies.videoBitrateAllocatorFactory = &objects[5];
    dependencies.networkControllerFactory = &objects[6];
    dependencies.trials = &objects[7];

    destroyed.clear();
    errno = ERANGE;
    _ZN6webrtc26PeerConnectionDependenciesD1Ev_nid_postfix(&dependencies);
    Require(errno == ERANGE);
    Require(destroyed == "HGFEDCBA");
    Require(dependencies.observer == sentinelObserver);
    Require(dependencies.allocator == nullptr);
    Require(dependencies.trials == nullptr);

    _ZN6webrtc26PeerConnectionDependenciesC1EPNS_22PeerConnectionObserverE_nid_postfix(&dependencies, nullptr);
    dependencies.iceTransportFactory = &objects[2];
    dependencies.networkControllerFactory = &objects[6];
    destroyed.clear();
    _ZN6webrtc26PeerConnectionDependenciesD1Ev_nid_postfix(&dependencies);
    Require(destroyed == "GC");

    FakeOwned nullVTable{nullptr, 'X', nullptr, nullptr};
    FakeOwned unmappedVTable{reinterpret_cast<const void* const*>(static_cast<std::uintptr_t>(0x10)), 'Y', nullptr, nullptr};
    FakeOwned nullSlot{NullDeletingVTable, 'Z', nullptr, nullptr};

    _ZN6webrtc26PeerConnectionDependenciesC1EPNS_22PeerConnectionObserverE_nid_postfix(&dependencies, sentinelObserver);
    dependencies.trials = &objects[7];
    dependencies.allocator = &nullVTable;
    destroyed.clear();
    errno = EDOM;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc26PeerConnectionDependenciesD1Ev_nid_postfix(&dependencies);
    }));
    Require(errno == EDOM);
    Require(destroyed.empty());
    Require(dependencies.trials == &objects[7]);
    Require(dependencies.allocator == &nullVTable);

    dependencies.allocator = &unmappedVTable;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc26PeerConnectionDependenciesD1Ev_nid_postfix(&dependencies);
    }));
    Require(destroyed.empty());

    dependencies.allocator = &nullSlot;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc26PeerConnectionDependenciesD1Ev_nid_postfix(&dependencies);
    }));
    Require(destroyed.empty());

    dependencies.allocator = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x10));
    Require(ThrowsInvalid([&] {
        _ZN6webrtc26PeerConnectionDependenciesD1Ev_nid_postfix(&dependencies);
    }));
    Require(destroyed.empty());

    errno = EEXIST;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc26PeerConnectionDependenciesD1Ev_nid_postfix(nullptr);
    }));
    Require(errno == EEXIST);

    auto* const unmapped = reinterpret_cast<WebRtcPeerConnectionDependencies*>(static_cast<std::uintptr_t>(0x10));
    Require(ThrowsInvalid([&] {
        _ZN6webrtc26PeerConnectionDependenciesD1Ev_nid_postfix(unmapped);
    }));

    ReadOnlyPage readOnly;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc26PeerConnectionDependenciesD1Ev_nid_postfix(
            static_cast<WebRtcPeerConnectionDependencies*>(readOnly.address));
    }));
}

}

int main() {
    TestObserverConstructor();
    TestMoveConstructor();
    TestDestructor();
    return 0;
}
