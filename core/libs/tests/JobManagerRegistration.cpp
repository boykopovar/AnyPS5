#include "SceTypes.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

struct ConfigParams;
using JobEntry = void (APS5_VABI*)(void*, std::uint32_t);

struct Sequence {
    APS5_VABI virtual int Dispatch(JobEntry, void*, std::uint32_t, const char*) = 0;
    APS5_VABI virtual int SyncPoint() = 0;
    APS5_VABI virtual int Wait() = 0;
};

struct SequenceFactory {
    APS5_VABI virtual int Create(Sequence**, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, const char*) = 0;
    APS5_VABI virtual int Destroy(Sequence*) = 0;
};

extern "C" {
void APS5_VABI _ZN3sce3Job10JobManagerC1Ev(void*);
void APS5_VABI _ZN3sce3Job10JobManagerD1Ev(void*);
int APS5_VABI _ZN3sce3Job10JobManager10initializeEPvmjjjmPKNS1_12ConfigParamsE(void*, void*, std::uint64_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint64_t, const ConfigParams*);
int APS5_VABI _ZN3sce3Job10JobManager10initializeEPvmjjPKNS1_12ConfigParamsE(void*, void*, std::uint64_t, std::uint32_t, std::uint32_t, const ConfigParams*);
int APS5_VABI _ZN3sce3Job10JobManager8shutdownEv(void*);
SequenceFactory* APS5_VABI _ZN3sce3Job10JobManager27getSequenceFactoryInterfaceENS1_8PriorityE(void*, std::uint32_t);
}

namespace {

alignas(256) std::array<std::uint8_t, 0x4000> workArea{};
alignas(64) std::array<std::uint8_t, 0x100> manager{};
std::uint32_t registrationCalls = 0;
std::uint32_t unregistrationCalls = 0;
std::uint32_t assignedId = 0;
bool writeId = true;
int registrationResult = 0;
std::atomic<std::uint32_t> jobs{0};

void Require(bool condition) {
    if (!condition) std::abort();
}

int Initialize(bool unified, void* memory) {
    if (unified) return _ZN3sce3Job10JobManager10initializeEPvmjjjmPKNS1_12ConfigParamsE(manager.data(), memory, workArea.size(), 1, 1, 0, 0, nullptr);
    return _ZN3sce3Job10JobManager10initializeEPvmjjPKNS1_12ConfigParamsE(manager.data(), memory, workArea.size(), 1, 1, nullptr);
}

void APS5_VABI Count(void*, std::uint32_t) {
    ++jobs;
}

}

extern "C" {

int APS5_VABI _sceUlobjmgrRegisterObject_nid_postfix(std::uint64_t object, std::int32_t kind, std::uint32_t* id) {
    Require(object == reinterpret_cast<std::uintptr_t>(workArea.data()) && kind == 0x21 && id != nullptr);
    Require(workArea[0] == 0x4a && workArea[1] == 0x4d);
    Require(_ZN3sce3Job10JobManager27getSequenceFactoryInterfaceENS1_8PriorityE(manager.data(), 0) != nullptr);
    ++registrationCalls;
    if (writeId) *id = assignedId;
    return registrationResult;
}

int APS5_VABI _sceUlobjmgrUnregisterObject_nid_postfix(std::uint32_t id) {
    Require(id == (writeId ? assignedId : 0));
    Require(_ZN3sce3Job10JobManager27getSequenceFactoryInterfaceENS1_8PriorityE(manager.data(), 0) != nullptr);
    ++unregistrationCalls;
    return 37;
}

}

int main() {
    for (const bool unified : {false, true}) {
        for (const auto [result, assignsId] : std::array<std::pair<int, bool>, 5>{{{0, true}, {22, true}, {static_cast<int>(0x8002000cu), true}, {22, false}, {static_cast<int>(0x8002000cu), false}}}) {
            for (const bool destructor : {false, true}) {
                _ZN3sce3Job10JobManagerC1Ev(manager.data());
                const auto before = registrationCalls;
                Require(Initialize(unified, nullptr) == static_cast<int>(0x81581001u));
                Require(registrationCalls == before);
                registrationResult = result;
                writeId = assignsId;
                assignedId = 101 + before;
                Require(Initialize(unified, workArea.data()) == result);
                Require(registrationCalls == before + 1 && unregistrationCalls == before);
                Require(Initialize(unified, workArea.data()) == static_cast<int>(0x81581020u));
                Require(registrationCalls == before + 1);
                auto* factory = _ZN3sce3Job10JobManager27getSequenceFactoryInterfaceENS1_8PriorityE(manager.data(), 0);
                Sequence* sequence = nullptr;
                Require(factory->Create(&sequence, 3, 1, 0, 1, nullptr) == 0);
                jobs = 0;
                Require(sequence->Dispatch(Count, nullptr, 3, nullptr) == 0);
                Require(factory->Destroy(sequence) == 0 && jobs == 3);
                if (destructor) _ZN3sce3Job10JobManagerD1Ev(manager.data());
                else Require(_ZN3sce3Job10JobManager8shutdownEv(manager.data()) == 0);
                Require(unregistrationCalls == before + 1);
                Require(_ZN3sce3Job10JobManager8shutdownEv(manager.data()) == static_cast<int>(0x81581010u));
                _ZN3sce3Job10JobManagerD1Ev(manager.data());
                Require(unregistrationCalls == before + 1);
            }
        }
    }
    std::puts("job manager registration tests passed");
}
