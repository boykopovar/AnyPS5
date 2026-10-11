#include "VulkanTestDevice.hpp"
#include "SceTypes.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

extern "C" void AgcDriverShutdown_nid_postfix();

struct MemorySizeQueryParams {
    std::uint32_t version;
    std::uint32_t workers;
    std::uint32_t sequences;
    std::uint32_t invocations;
    std::uint32_t dispatches;
    std::uint32_t syncPoints;
    std::uint32_t waits;
    std::uint32_t computeSequences;
    std::uint64_t computeQueueMask;
};

struct ConfigParams {
    std::uint32_t version;
    std::uint32_t threadFlags;
    std::uint32_t stackSize;
    std::uint32_t priority;
    const char* threadName;
    std::uint32_t spinMicroseconds;
    std::uint32_t computeAffinity;
    std::uint32_t computePriority;
    std::uint32_t reserved;
};

using JobEntry = void (APS5_VABI*)(void*, std::uint32_t);

struct Sequence {
    APS5_VABI virtual int Dispatch(JobEntry, void*, std::uint32_t, const char*) = 0;
    APS5_VABI virtual int SyncPoint() = 0;
    APS5_VABI virtual int Wait() = 0;
    APS5_VABI virtual void CompleteDestructor() = 0;
    APS5_VABI virtual void DeletingDestructor() = 0;
    APS5_VABI virtual int DispatchCompute(const void*, void*, std::int32_t, std::int32_t, std::int32_t, const char*) = 0;
    APS5_VABI virtual int DispatchComputeIndirect(const void*, void*, const void*, const char*) = 0;
};

struct SequenceFactory {
    APS5_VABI virtual int Create(Sequence**, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, const char*) = 0;
    APS5_VABI virtual int Destroy(Sequence*) = 0;
    APS5_VABI virtual void CompleteDestructor() = 0;
    APS5_VABI virtual void DeletingDestructor() = 0;
    APS5_VABI virtual int CreateCompute(Sequence**, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, const char*) = 0;
    APS5_VABI virtual int DestroyCompute(Sequence*) = 0;
};

extern "C" {
void APS5_VABI _ZN3sce3Job10JobManagerC1Ev(void*);
std::uint64_t APS5_VABI _ZN3sce3Job10JobManager27calculateRequiredMemorySizeEPKNS1_21MemorySizeQueryParamsE(const MemorySizeQueryParams*);
int APS5_VABI _ZN3sce3Job10JobManager10initializeEPvmjjjmPKNS1_12ConfigParamsE(void*, void*, std::uint64_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint64_t, const ConfigParams*);
int APS5_VABI _ZN3sce3Job10JobManager8shutdownEv(void*);
SequenceFactory* APS5_VABI _ZN3sce3Job10JobManager34getUnifiedSequenceFactoryInterfaceENS1_8PriorityE(void*, std::uint32_t);
}

#define Construct _ZN3sce3Job10JobManagerC1Ev
#define RequiredSize _ZN3sce3Job10JobManager27calculateRequiredMemorySizeEPKNS1_21MemorySizeQueryParamsE
#define Initialize _ZN3sce3Job10JobManager10initializeEPvmjjjmPKNS1_12ConfigParamsE
#define Shutdown _ZN3sce3Job10JobManager8shutdownEv
#define Factories _ZN3sce3Job10JobManager34getUnifiedSequenceFactoryInterfaceENS1_8PriorityE

namespace {

constexpr int InvalidArgument = static_cast<int>(0x81581000u);
constexpr int OutOfRange = static_cast<int>(0x81580000u);
constexpr int NullArgument = static_cast<int>(0x81580001u);
constexpr int NoNodes = static_cast<int>(0x81580002u);
constexpr int LimitExceeded = static_cast<int>(0x81580003u);
constexpr int NoneFree = static_cast<int>(0x81580006u);
constexpr int Invalid = static_cast<int>(0x81580007u);

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

alignas(256) std::array<std::uint32_t, 12> ReaderCode{0xf4040100u, 0xfa000000u, 0xf4080201u, 0xfa000000u, 0xbf8cc07fu, 0xf4000182u, 0xfa000000u, 0xbf8cc07fu, 0x7e000206u, 0xe0700000u, 0x80020000u, 0xbf810000u};
alignas(256) std::array<std::uint32_t, 4> Value{};
alignas(256) std::array<std::uint32_t, 2> Table{};
alignas(256) volatile std::uint32_t Output[4]{};
alignas(256) std::array<std::uint32_t, 4> OutputDescriptor{};
alignas(32) std::array<std::uint32_t, 8> IndirectArguments{1, 1, 1, 0, 0, 0, 0, 0};
alignas(16) std::array<std::uint32_t, 8> Specials{};
alignas(16) std::array<std::uint32_t, 8> SpecialsWithoutDispatch{};
std::array<ShaderRegister, 9> Registers{};
alignas(16) std::array<unsigned char, 0x80> Shader{};
alignas(16) std::array<unsigned char, 0x80> ShaderWithoutDispatch{};

std::uint32_t Low(const volatile void* pointer) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

std::uint32_t High(const volatile void* pointer) {
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer)) >> 32u);
}

void BuildShader() {
    Table = {Low(Value.data()), High(Value.data())};
    OutputDescriptor = {Low(Output), High(Output) & 0xffffu, sizeof(Output), 0x31016facu};
    const auto code = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(ReaderCode.data()));
    Registers = {{{0x20c, static_cast<std::uint32_t>(code >> 8u)}, {0x20d, static_cast<std::uint32_t>(code >> 40u) & 0xffu}, {0x207, 1}, {0x208, 1}, {0x209, 1}, {0x212, 0}, {0x213, 4u << 1u},
                  {0x242, Low(OutputDescriptor.data())}, {0x243, High(OutputDescriptor.data())}}};
    Specials[4] = 1;
    const ShaderRegister* registers = Registers.data();
    for (auto* shader : {&Shader, &ShaderWithoutDispatch}) {
        const std::uint32_t* specials = shader == &Shader ? Specials.data() : SpecialsWithoutDispatch.data();
        std::memcpy(shader->data() + 0x20, &registers, sizeof(registers));
        std::memcpy(shader->data() + 0x28, &specials, sizeof(specials));
        (*shader)[0x5c] = static_cast<unsigned char>(Registers.size());
    }
}

std::uint64_t SizeFor(std::uint32_t computeSequences, std::uint64_t mask, bool limits) {
    const MemorySizeQueryParams query{1, 2, 1, limits ? 100u : 0u, limits ? 8u : 0u, limits ? 4u : 0u, limits ? 2u : 0u, computeSequences, mask};
    return RequiredSize(&query);
}

std::atomic<bool> Released{false};
std::atomic<std::uint32_t> SeenByJob{0};

void APS5_VABI BlockingJob(void*, std::uint32_t) {
    while (!Released) std::this_thread::yield();
}

void APS5_VABI ReadOutputJob(void*, std::uint32_t) {
    SeenByJob = Output[0];
}

void APS5_VABI Nothing(void*, std::uint32_t) {
}

void SizeCase() {
    Require(SizeFor(0, 0, false) == 0x10d0 && SizeFor(0, 0, true) == 0x14c0, "size without compute sequences");
    Require(SizeFor(1, 1, false) == 0x24fd8 && SizeFor(1, 1, true) == 0x253c8, "size with one compute sequence");
    Require(SizeFor(2, 3, false) == 0x48e20 && SizeFor(2, 3, true) == 0x49210, "size with two compute sequences");
    Require(SizeFor(3, 7, false) == 0x6cb68, "size with three compute sequences");
    Require(SizeFor(1, 3, true) == 0x491c8, "size with two queues for one compute sequence");
    Require(SizeFor(2, 1, true) == 0, "size with fewer queues than compute sequences");
}

void RunCase() {
    const std::uint64_t size = SizeFor(1, 1, true);
    void* memory = std::aligned_alloc(256, (size + 255) & ~std::uint64_t{255});
    Require(memory != nullptr, "allocation failed");
    alignas(64) static unsigned char manager[0x100];
    Construct(manager);
    const ConfigParams badHandler{1, 0x00021fff, 0x00010000, 0x2bc, nullptr, 0, 0x1fff, 0x50, 0};
    Require(Initialize(manager, memory, size, 2, 1, 1, 1, &badHandler) == InvalidArgument, "a handler priority below 0x100 was accepted");
    Require(Initialize(manager, memory, size, 2, 1, 1, 1, nullptr) == 0, "initialize with one compute sequence failed");
    SequenceFactory* factory = Factories(manager, 0);
    Sequence* sequence = nullptr;
    Sequence* other = nullptr;
    Require(factory->CreateCompute(&sequence, 100, 4, 4, 2, "compute") == 0, "the compute sequence was not created");
    Require(factory->CreateCompute(&other, 1, 1, 1, 1, "second") == NoneFree, "a second compute sequence was created");
    Require(factory->Create(&other, 1, 1, 1, 1, "cpu") == 0, "the CPU sequence was not created");
    Require(factory->DestroyCompute(other) == Invalid, "a CPU sequence was destroyed as a compute sequence");
    Require(factory->Destroy(sequence) == Invalid, "a compute sequence was destroyed as a CPU sequence");
    Require(factory->Destroy(other) == 0, "the CPU sequence was not destroyed");

    Require(sequence->DispatchCompute(nullptr, Table.data(), 1, 1, 1, nullptr) == NullArgument, "a null shader was accepted");
    Require(sequence->DispatchCompute(Shader.data(), Table.data(), 0, 1, 1, nullptr) == OutOfRange, "a zero group count was accepted");
    Require(sequence->DispatchCompute(Shader.data(), Table.data(), 1, 1, -1, nullptr) == OutOfRange, "a negative group count was accepted");
    Require(sequence->DispatchComputeIndirect(Shader.data(), Table.data(), nullptr, nullptr) == NullArgument, "null indirect arguments were accepted");
    Require(sequence->DispatchComputeIndirect(Shader.data(), Table.data(), reinterpret_cast<const char*>(IndirectArguments.data()) + 4, nullptr) == NullArgument, "misaligned indirect arguments were accepted");

    Value[0] = 0x11110001u;
    Require(sequence->DispatchCompute(Shader.data(), Table.data(), 1, 1, 1, "direct") == 0, "the compute dispatch failed");
    Require(sequence->Wait() == 0, "the wait after a compute dispatch failed");
    Require(Output[0] == 0x11110001u, "the compute dispatch had not run when the wait returned: " + std::to_string(Output[0]));

    Value[0] = 0x22220002u;
    Require(sequence->DispatchComputeIndirect(Shader.data(), Table.data(), IndirectArguments.data(), nullptr) == 0, "the indirect compute dispatch failed");
    Require(sequence->Wait() == 0, "the wait after an indirect compute dispatch failed");
    Require(Output[0] == 0x22220002u, "the indirect compute dispatch had not run when the wait returned: " + std::to_string(Output[0]));

    Value[0] = 0x33330003u;
    Require(sequence->DispatchCompute(ShaderWithoutDispatch.data(), Table.data(), 1, 1, 1, nullptr) == 0, "the dispatch of a shader without the dispatch bit failed");
    Require(sequence->Wait() == 0, "the wait after a shader without the dispatch bit failed");
    Require(Output[0] == 0x22220002u, "a shader without the dispatch bit ran");

    Require(sequence->Dispatch(BlockingJob, nullptr, 1, "blocking") == 0, "the blocking job was not dispatched");
    Require(sequence->SyncPoint() == 0, "the sync point failed");
    Require(sequence->DispatchCompute(Shader.data(), Table.data(), 1, 1, 1, nullptr) == 0, "the compute dispatch behind a sync point failed");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    Require(Output[0] == 0x22220002u, "a compute dispatch ran before the CPU job ahead of its sync point finished");
    Released = true;
    Require(sequence->SyncPoint() == 0, "the second sync point failed");
    Require(sequence->Dispatch(ReadOutputJob, nullptr, 1, "reader") == 0, "the reading job was not dispatched");
    Require(sequence->Wait() == 0, "the wait after the ordered dispatches failed");
    Require(Output[0] == 0x33330003u, "the compute dispatch behind a sync point did not run");
    Require(SeenByJob == 0x33330003u, "a CPU job behind a sync point ran before the compute dispatch ahead of it finished");

    for (int index = 0; index < 4; ++index) Require(sequence->DispatchCompute(Shader.data(), Table.data(), 1, 1, 1, nullptr) == 0, "a compute dispatch within the limit failed");
    Require(sequence->DispatchCompute(Shader.data(), Table.data(), 1, 1, 1, nullptr) == LimitExceeded, "a compute dispatch past the dispatch limit was accepted");
    Require(sequence->DispatchCompute(Shader.data(), Table.data(), 1, 1, 1, nullptr) == Invalid, "a latched compute sequence accepted a dispatch");
    Require(factory->DestroyCompute(sequence) == 0, "the compute sequence was not destroyed");

    Require(factory->Create(&other, 0x7fff, 0x3ff, 0xff, 1, "nodes") == 0, "the node counting sequence was not created");
    int accepted = 0;
    int result = 0;
    while (accepted < 0x3ff && (result = other->Dispatch(Nothing, nullptr, 1, nullptr)) == 0) ++accepted;
    Require(result == NoNodes && accepted == 43, "the node pool of a compute configuration holds " + std::to_string(accepted) + " dispatches");
    Require(Shutdown(manager) == 0, "shutdown failed");
    std::free(memory);
}

}

int main() {
    try {
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        device.reset();
        BuildShader();
        SizeCase();
        RunCase();
        AgcDriverShutdown_nid_postfix();
        std::cout << "job manager compute test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try {
            AgcDriverShutdown_nid_postfix();
        } catch (const std::exception& shutdownError) {
            std::cerr << shutdownError.what() << '\n';
        }
        return 1;
    }
}
