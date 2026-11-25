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
    std::fprintf(stderr, "libwebrtc PeerConnectionFactoryDependencies check failed at line %u\n", location.line());
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

struct FakeObject {
    const void* const* vtable;
    char tag;
    WebRtcPeerConnectionFactoryDependencies* owner;
    void* WebRtcPeerConnectionFactoryDependencies::* field;
};

std::string events;

void APS5_VABI UnexpectedCall(void*) {
    std::abort();
}

void APS5_VABI FakeDelete(FakeObject* object) {
    Require(object != nullptr);
    if (object->owner && object->field)
        Require(object->owner->*(object->field) == nullptr);
    events.push_back('D');
    events.push_back(object->tag);
}

std::int32_t APS5_VABI FakeRelease(const FakeObject* object) {
    Require(object != nullptr);
    if (object->owner && object->field)
        Require(object->owner->*(object->field) == nullptr);
    events.push_back('R');
    events.push_back(object->tag);
    return 1;
}

const void* const Slot1DeleteVTable[2] = {
    reinterpret_cast<const void*>(&UnexpectedCall),
    reinterpret_cast<const void*>(&FakeDelete),
};

const void* const Slot2DeleteVTable[3] = {
    reinterpret_cast<const void*>(&UnexpectedCall),
    reinterpret_cast<const void*>(&UnexpectedCall),
    reinterpret_cast<const void*>(&FakeDelete),
};

const void* const Slot6DeleteVTable[7] = {
    reinterpret_cast<const void*>(&UnexpectedCall),
    reinterpret_cast<const void*>(&UnexpectedCall),
    reinterpret_cast<const void*>(&UnexpectedCall),
    reinterpret_cast<const void*>(&UnexpectedCall),
    reinterpret_cast<const void*>(&UnexpectedCall),
    reinterpret_cast<const void*>(&UnexpectedCall),
    reinterpret_cast<const void*>(&FakeDelete),
};

const void* const ReleaseVTable[2] = {
    reinterpret_cast<const void*>(&UnexpectedCall),
    reinterpret_cast<const void*>(&FakeRelease),
};

const void* const NullSlot6VTable[7] = {
    reinterpret_cast<const void*>(&UnexpectedCall),
    reinterpret_cast<const void*>(&UnexpectedCall),
    reinterpret_cast<const void*>(&UnexpectedCall),
    reinterpret_cast<const void*>(&UnexpectedCall),
    reinterpret_cast<const void*>(&UnexpectedCall),
    reinterpret_cast<const void*>(&UnexpectedCall),
    nullptr,
};

const void* const NullSlot2VTable[3] = {
    reinterpret_cast<const void*>(&UnexpectedCall),
    reinterpret_cast<const void*>(&UnexpectedCall),
    nullptr,
};

const void* const NullSlot1VTable[2] = {
    reinterpret_cast<const void*>(&UnexpectedCall),
    nullptr,
};

bool AllManagedNull(const WebRtcPeerConnectionFactoryDependencies& dependencies) {
    return dependencies.packetSocketFactory == nullptr &&
        dependencies.taskQueueFactory == nullptr &&
        dependencies.eventLogFactory == nullptr &&
        dependencies.fecControllerFactory == nullptr &&
        dependencies.networkStatePredictorFactory == nullptr &&
        dependencies.networkControllerFactory == nullptr &&
        dependencies.networkManager == nullptr &&
        dependencies.networkMonitorFactory == nullptr &&
        dependencies.netEqFactory == nullptr &&
        dependencies.sctpFactory == nullptr &&
        dependencies.trials == nullptr &&
        dependencies.transportControllerSendFactory == nullptr &&
        dependencies.decodeMetronome == nullptr &&
        dependencies.encodeMetronome == nullptr &&
        dependencies.adm == nullptr &&
        dependencies.audioEncoderFactory == nullptr &&
        dependencies.audioDecoderFactory == nullptr &&
        dependencies.audioMixer == nullptr &&
        dependencies.audioProcessing == nullptr &&
        dependencies.audioProcessingBuilder == nullptr &&
        dependencies.audioFrameProcessor == nullptr &&
        dependencies.videoEncoderFactory == nullptr &&
        dependencies.videoDecoderFactory == nullptr &&
        dependencies.mediaFactory == nullptr;
}

void TestDefaultConstructor() {
    WebRtcPeerConnectionFactoryDependencies dependencies{};
    std::memset(&dependencies, 0xCC, sizeof(dependencies));
    errno = EINVAL;
    _ZN6webrtc33PeerConnectionFactoryDependenciesC1Ev_nid_postfix(&dependencies);
    Require(errno == EINVAL);
    Require(dependencies.networkThread == nullptr);
    Require(dependencies.workerThread == nullptr);
    Require(dependencies.signalingThread == nullptr);
    Require(dependencies.socketFactory == nullptr);
    Require(AllManagedNull(dependencies));

    errno = EAGAIN;
    Require(ThrowsInvalid([] {
        _ZN6webrtc33PeerConnectionFactoryDependenciesC1Ev_nid_postfix(nullptr);
    }));
    Require(errno == EAGAIN);

    auto* const unmapped = reinterpret_cast<WebRtcPeerConnectionFactoryDependencies*>(static_cast<std::uintptr_t>(0x10));
    errno = EBUSY;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc33PeerConnectionFactoryDependenciesC1Ev_nid_postfix(unmapped);
    }));
    Require(errno == EBUSY);

    ReadOnlyPage readOnly;
    errno = ENOENT;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc33PeerConnectionFactoryDependenciesC1Ev_nid_postfix(
            static_cast<WebRtcPeerConnectionFactoryDependencies*>(readOnly.address));
    }));
    Require(errno == ENOENT);
}

void TestMoveConstructor() {
    void* const raw0 = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1100));
    void* const raw1 = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1200));
    void* const raw2 = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1300));
    void* const raw3 = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1400));

    WebRtcPeerConnectionFactoryDependencies source{};
    _ZN6webrtc33PeerConnectionFactoryDependenciesC1Ev_nid_postfix(&source);
    source.networkThread = raw0;
    source.workerThread = raw1;
    source.signalingThread = raw2;
    source.socketFactory = raw3;

    FakeObject packetSocket{Slot1DeleteVTable, 'a', nullptr, nullptr};
    FakeObject predictor{Slot2DeleteVTable, 'e', nullptr, nullptr};
    FakeObject monitor{Slot2DeleteVTable, 'h', nullptr, nullptr};
    FakeObject adm{ReleaseVTable, 'o', nullptr, nullptr};
    FakeObject videoEncoder{Slot6DeleteVTable, 'v', nullptr, nullptr};
    FakeObject media{Slot1DeleteVTable, 'x', nullptr, nullptr};

    source.packetSocketFactory = &packetSocket;
    source.networkStatePredictorFactory = &predictor;
    source.networkMonitorFactory = &monitor;
    source.adm = &adm;
    source.videoEncoderFactory = &videoEncoder;
    source.mediaFactory = &media;

    WebRtcPeerConnectionFactoryDependencies destination{};
    std::memset(&destination, 0xAA, sizeof(destination));
    errno = EACCES;
    _ZN6webrtc33PeerConnectionFactoryDependenciesC1EOS0__nid_postfix(&destination, &source);
    Require(errno == EACCES);
    Require(destination.networkThread == raw0);
    Require(destination.workerThread == raw1);
    Require(destination.signalingThread == raw2);
    Require(destination.socketFactory == raw3);
    Require(destination.packetSocketFactory == &packetSocket);
    Require(destination.networkStatePredictorFactory == &predictor);
    Require(destination.networkMonitorFactory == &monitor);
    Require(destination.adm == &adm);
    Require(destination.videoEncoderFactory == &videoEncoder);
    Require(destination.mediaFactory == &media);

    Require(source.networkThread == raw0);
    Require(source.workerThread == raw1);
    Require(source.signalingThread == raw2);
    Require(source.socketFactory == raw3);
    Require(AllManagedNull(source));

    events.clear();
    _ZN6webrtc33PeerConnectionFactoryDependenciesD1Ev_nid_postfix(&source);
    Require(events.empty());

    _ZN6webrtc33PeerConnectionFactoryDependenciesD1Ev_nid_postfix(&destination);
    Require(events == "DxDvRoDhDeDa");

    WebRtcPeerConnectionFactoryDependencies intact{};
    _ZN6webrtc33PeerConnectionFactoryDependenciesC1Ev_nid_postfix(&intact);
    intact.packetSocketFactory = &packetSocket;
    intact.mediaFactory = &media;

    errno = EPIPE;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc33PeerConnectionFactoryDependenciesC1EOS0__nid_postfix(nullptr, &intact);
    }));
    Require(errno == EPIPE);
    Require(intact.packetSocketFactory == &packetSocket);
    Require(intact.mediaFactory == &media);

    Require(ThrowsInvalid([&] {
        _ZN6webrtc33PeerConnectionFactoryDependenciesC1EOS0__nid_postfix(&intact, nullptr);
    }));
    Require(ThrowsInvalid([&] {
        _ZN6webrtc33PeerConnectionFactoryDependenciesC1EOS0__nid_postfix(&intact, &intact);
    }));

    alignas(WebRtcPeerConnectionFactoryDependencies) std::array<std::uint8_t, sizeof(WebRtcPeerConnectionFactoryDependencies) + 16> overlap{};
    auto* const first = reinterpret_cast<WebRtcPeerConnectionFactoryDependencies*>(overlap.data());
    auto* const second = reinterpret_cast<WebRtcPeerConnectionFactoryDependencies*>(overlap.data() + 8);
    Require(ThrowsInvalid([&] {
        _ZN6webrtc33PeerConnectionFactoryDependenciesC1EOS0__nid_postfix(first, second);
    }));
    Require(ThrowsInvalid([&] {
        _ZN6webrtc33PeerConnectionFactoryDependenciesC1EOS0__nid_postfix(second, first);
    }));

    ReadOnlyPage readOnly;
    auto* const readOnlyDeps = static_cast<WebRtcPeerConnectionFactoryDependencies*>(readOnly.address);
    errno = EXDEV;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc33PeerConnectionFactoryDependenciesC1EOS0__nid_postfix(readOnlyDeps, &intact);
    }));
    Require(errno == EXDEV);
    Require(intact.packetSocketFactory == &packetSocket);
    Require(intact.mediaFactory == &media);

    WebRtcPeerConnectionFactoryDependencies untouched{};
    std::memset(&untouched, 0x77, sizeof(untouched));
    WebRtcPeerConnectionFactoryDependencies snapshot = untouched;
    errno = ENOMEM;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc33PeerConnectionFactoryDependenciesC1EOS0__nid_postfix(&untouched, readOnlyDeps);
    }));
    Require(errno == ENOMEM);
    Require(std::memcmp(&untouched, &snapshot, sizeof(untouched)) == 0);

    auto* const unmapped = reinterpret_cast<WebRtcPeerConnectionFactoryDependencies*>(static_cast<std::uintptr_t>(0x10));
    Require(ThrowsInvalid([&] {
        _ZN6webrtc33PeerConnectionFactoryDependenciesC1EOS0__nid_postfix(&untouched, unmapped);
    }));
    Require(std::memcmp(&untouched, &snapshot, sizeof(untouched)) == 0);
}

void TestDestructor() {
    WebRtcPeerConnectionFactoryDependencies dependencies{};
    _ZN6webrtc33PeerConnectionFactoryDependenciesC1Ev_nid_postfix(&dependencies);
    void* const sentinel0 = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1010));
    void* const sentinel1 = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1020));
    void* const sentinel2 = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1030));
    void* const sentinel3 = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1040));
    dependencies.networkThread = sentinel0;
    dependencies.workerThread = sentinel1;
    dependencies.signalingThread = sentinel2;
    dependencies.socketFactory = sentinel3;

    FakeObject objects[24] = {
        {Slot1DeleteVTable, 'a', &dependencies, &WebRtcPeerConnectionFactoryDependencies::packetSocketFactory},
        {Slot1DeleteVTable, 'b', &dependencies, &WebRtcPeerConnectionFactoryDependencies::taskQueueFactory},
        {Slot1DeleteVTable, 'c', &dependencies, &WebRtcPeerConnectionFactoryDependencies::eventLogFactory},
        {Slot1DeleteVTable, 'd', &dependencies, &WebRtcPeerConnectionFactoryDependencies::fecControllerFactory},
        {Slot2DeleteVTable, 'e', &dependencies, &WebRtcPeerConnectionFactoryDependencies::networkStatePredictorFactory},
        {Slot1DeleteVTable, 'f', &dependencies, &WebRtcPeerConnectionFactoryDependencies::networkControllerFactory},
        {Slot1DeleteVTable, 'g', &dependencies, &WebRtcPeerConnectionFactoryDependencies::networkManager},
        {Slot2DeleteVTable, 'h', &dependencies, &WebRtcPeerConnectionFactoryDependencies::networkMonitorFactory},
        {Slot1DeleteVTable, 'i', &dependencies, &WebRtcPeerConnectionFactoryDependencies::netEqFactory},
        {Slot1DeleteVTable, 'j', &dependencies, &WebRtcPeerConnectionFactoryDependencies::sctpFactory},
        {Slot1DeleteVTable, 'k', &dependencies, &WebRtcPeerConnectionFactoryDependencies::trials},
        {Slot1DeleteVTable, 'l', &dependencies, &WebRtcPeerConnectionFactoryDependencies::transportControllerSendFactory},
        {Slot1DeleteVTable, 'm', &dependencies, &WebRtcPeerConnectionFactoryDependencies::decodeMetronome},
        {Slot1DeleteVTable, 'n', &dependencies, &WebRtcPeerConnectionFactoryDependencies::encodeMetronome},
        {ReleaseVTable, 'o', &dependencies, &WebRtcPeerConnectionFactoryDependencies::adm},
        {ReleaseVTable, 'p', &dependencies, &WebRtcPeerConnectionFactoryDependencies::audioEncoderFactory},
        {ReleaseVTable, 'q', &dependencies, &WebRtcPeerConnectionFactoryDependencies::audioDecoderFactory},
        {ReleaseVTable, 'r', &dependencies, &WebRtcPeerConnectionFactoryDependencies::audioMixer},
        {ReleaseVTable, 's', &dependencies, &WebRtcPeerConnectionFactoryDependencies::audioProcessing},
        {Slot1DeleteVTable, 't', &dependencies, &WebRtcPeerConnectionFactoryDependencies::audioProcessingBuilder},
        {Slot1DeleteVTable, 'u', &dependencies, &WebRtcPeerConnectionFactoryDependencies::audioFrameProcessor},
        {Slot6DeleteVTable, 'v', &dependencies, &WebRtcPeerConnectionFactoryDependencies::videoEncoderFactory},
        {Slot1DeleteVTable, 'w', &dependencies, &WebRtcPeerConnectionFactoryDependencies::videoDecoderFactory},
        {Slot1DeleteVTable, 'x', &dependencies, &WebRtcPeerConnectionFactoryDependencies::mediaFactory},
    };
    dependencies.packetSocketFactory = &objects[0];
    dependencies.taskQueueFactory = &objects[1];
    dependencies.eventLogFactory = &objects[2];
    dependencies.fecControllerFactory = &objects[3];
    dependencies.networkStatePredictorFactory = &objects[4];
    dependencies.networkControllerFactory = &objects[5];
    dependencies.networkManager = &objects[6];
    dependencies.networkMonitorFactory = &objects[7];
    dependencies.netEqFactory = &objects[8];
    dependencies.sctpFactory = &objects[9];
    dependencies.trials = &objects[10];
    dependencies.transportControllerSendFactory = &objects[11];
    dependencies.decodeMetronome = &objects[12];
    dependencies.encodeMetronome = &objects[13];
    dependencies.adm = &objects[14];
    dependencies.audioEncoderFactory = &objects[15];
    dependencies.audioDecoderFactory = &objects[16];
    dependencies.audioMixer = &objects[17];
    dependencies.audioProcessing = &objects[18];
    dependencies.audioProcessingBuilder = &objects[19];
    dependencies.audioFrameProcessor = &objects[20];
    dependencies.videoEncoderFactory = &objects[21];
    dependencies.videoDecoderFactory = &objects[22];
    dependencies.mediaFactory = &objects[23];

    events.clear();
    errno = ERANGE;
    _ZN6webrtc33PeerConnectionFactoryDependenciesD1Ev_nid_postfix(&dependencies);
    Require(errno == ERANGE);
    Require(events == "DxDwDvDuDtRsRrRqRpRoDnDmDlDkDjDiDhDgDfDeDdDcDbDa");
    Require(dependencies.networkThread == sentinel0);
    Require(dependencies.workerThread == sentinel1);
    Require(dependencies.signalingThread == sentinel2);
    Require(dependencies.socketFactory == sentinel3);
    Require(AllManagedNull(dependencies));

    FakeObject nullVTable{nullptr, '0', nullptr, nullptr};
    FakeObject badSlot1{NullSlot1VTable, '1', nullptr, nullptr};
    FakeObject badSlot2{NullSlot2VTable, '2', nullptr, nullptr};
    FakeObject badSlot6{NullSlot6VTable, '6', nullptr, nullptr};

    _ZN6webrtc33PeerConnectionFactoryDependenciesC1Ev_nid_postfix(&dependencies);
    dependencies.mediaFactory = &objects[23];
    dependencies.videoEncoderFactory = &badSlot6;
    events.clear();
    errno = EDOM;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc33PeerConnectionFactoryDependenciesD1Ev_nid_postfix(&dependencies);
    }));
    Require(errno == EDOM);
    Require(events.empty());
    Require(dependencies.mediaFactory == &objects[23]);

    dependencies.videoEncoderFactory = nullptr;
    dependencies.networkMonitorFactory = &badSlot2;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc33PeerConnectionFactoryDependenciesD1Ev_nid_postfix(&dependencies);
    }));
    Require(events.empty());

    dependencies.networkMonitorFactory = nullptr;
    dependencies.adm = &badSlot1;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc33PeerConnectionFactoryDependenciesD1Ev_nid_postfix(&dependencies);
    }));
    Require(events.empty());

    dependencies.adm = &nullVTable;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc33PeerConnectionFactoryDependenciesD1Ev_nid_postfix(&dependencies);
    }));
    Require(events.empty());

    dependencies.adm = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x10));
    Require(ThrowsInvalid([&] {
        _ZN6webrtc33PeerConnectionFactoryDependenciesD1Ev_nid_postfix(&dependencies);
    }));
    Require(events.empty());

    errno = EEXIST;
    Require(ThrowsInvalid([] {
        _ZN6webrtc33PeerConnectionFactoryDependenciesD1Ev_nid_postfix(nullptr);
    }));
    Require(errno == EEXIST);

    auto* const unmapped = reinterpret_cast<WebRtcPeerConnectionFactoryDependencies*>(static_cast<std::uintptr_t>(0x10));
    Require(ThrowsInvalid([&] {
        _ZN6webrtc33PeerConnectionFactoryDependenciesD1Ev_nid_postfix(unmapped);
    }));

    ReadOnlyPage readOnly;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc33PeerConnectionFactoryDependenciesD1Ev_nid_postfix(
            static_cast<WebRtcPeerConnectionFactoryDependencies*>(readOnly.address));
    }));
}

}

int main() {
    TestDefaultConstructor();
    TestMoveConstructor();
    TestDestructor();
    return 0;
}
