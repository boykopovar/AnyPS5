#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceJobManager/include/Api.hpp"
#include "VulkanTestDevice.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

extern "C" void AgcDriverShutdown_nid_postfix();

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
alignas(16) std::array<std::uint32_t, 8> SpecialsDefaultModifier{};
std::array<ShaderRegister, 9> Registers{};
alignas(16) std::array<unsigned char, 0x80> Shader{};
alignas(16) std::array<unsigned char, 0x80> ShaderDefaultModifier{};

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
    for (auto* shader : {&Shader, &ShaderDefaultModifier}) {
        const std::uint32_t* specials = shader == &Shader ? Specials.data() : SpecialsDefaultModifier.data();
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
    Require(SizeFor(1, 1, false) == 0x29fd8 && SizeFor(1, 1, true) == 0x2a3c8, "size with one compute sequence");
    Require(SizeFor(2, 3, false) == 0x52de0 && SizeFor(2, 3, true) == 0x531d0, "size with two compute sequences");
    Require(SizeFor(3, 7, false) == 0x7bb28, "size with three compute sequences");
    Require(SizeFor(1, 3, true) == 0x53188, "size with two queues for one compute sequence");
    Require(SizeFor(2, 1, true) == 0, "size with fewer queues than compute sequences");
}

void RunCase() {
    const std::uint64_t size = SizeFor(1, 2, true);
    void* memory = ::operator new((size + 255) & ~std::uint64_t{255}, std::align_val_t{256});
    Require(memory != nullptr, "allocation failed");
    alignas(64) static unsigned char manager[0x100];
    Construct(manager);
    const ConfigParams badHandler{1, 0x00021fff, 0x00010000, 0x2bc, nullptr, 0, 0x1fff, 0x50, 0};
    const ConfigParams noAffinity{1, 0x00021fff, 0x00010000, 0x2bc, nullptr, 0, 0, 0x100, 0};
    const ConfigParams wideAffinity{1, 0x00021fff, 0x00010000, 0x2bc, nullptr, 0, 0x2000, 0x100, 0};
    Require(Initialize(manager, memory, size, 2, 1, 1, 2, &badHandler) == InvalidArgument, "a handler priority below 0x100 was accepted");
    Require(Initialize(manager, memory, size, 2, 1, 1, 2, &noAffinity) == InvalidArgument, "an empty handler affinity was accepted");
    Require(Initialize(manager, memory, size, 2, 1, 1, 2, &wideAffinity) == InvalidArgument, "a handler affinity beyond 13 cores was accepted");
    Require(Initialize(manager, memory, size, 2, 1, 0xffffffffu, 2, nullptr) == InvalidArgument, "a sequence count that wraps was accepted");
    Require(Initialize(manager, memory, size, 2, 1, 1, 2, nullptr) == 0, "initialize with one compute sequence failed");
    ISequenceFactory* factory = Factories(manager, 0);
    ISequence* sequence = nullptr;
    ISequence* other = nullptr;
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
    Require(sequence->DispatchCompute(ShaderDefaultModifier.data(), Table.data(), 1, 1, 1, nullptr) == 0, "the dispatch with a default modifier failed");
    Require(sequence->Wait() == 0, "the wait after a default modifier dispatch failed");
    Require(Output[0] == 0x33330003u, "a default modifier dispatch did not run");

    Value[0] = 0x33330004u;
    Require(sequence->DispatchComputeIndirect(ShaderDefaultModifier.data(), Table.data(), IndirectArguments.data(), nullptr) == 0, "the indirect dispatch with a default modifier failed");
    Require(sequence->Wait() == 0, "the wait after an indirect default modifier dispatch failed");
    Require(Output[0] == 0x33330004u, "an indirect default modifier dispatch did not run");

    Value[0] = 0x33330005u;
    Require(sequence->Dispatch(BlockingJob, nullptr, 1, "blocking") == 0, "the blocking job was not dispatched");
    Require(sequence->SyncPoint() == 0, "the sync point failed");
    Require(sequence->DispatchCompute(Shader.data(), Table.data(), 1, 1, 1, nullptr) == 0, "the compute dispatch behind a sync point failed");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    Require(Output[0] == 0x33330004u, "a compute dispatch ran before the CPU job ahead of its sync point finished");
    Released = true;
    Require(sequence->SyncPoint() == 0, "the second sync point failed");
    Require(sequence->Dispatch(ReadOutputJob, nullptr, 1, "reader") == 0, "the reading job was not dispatched");
    Require(sequence->Wait() == 0, "the wait after the ordered dispatches failed");
    Require(Output[0] == 0x33330005u, "the compute dispatch behind a sync point did not run");
    Require(SeenByJob == 0x33330005u, "a CPU job behind a sync point ran before the compute dispatch ahead of it finished");

    Value[0] = 0x44440004u;
    Require(sequence->DispatchCompute(ShaderDefaultModifier.data(), Table.data(), 1, 1, 1, nullptr) == 0, "the leading dispatch failed");
    Require(sequence->SyncPoint() == 0, "the sync point between compute dispatches failed");
    Require(sequence->DispatchCompute(Shader.data(), Table.data(), 1, 1, 1, "behind") == 0, "the compute dispatch behind a compute dispatch failed");
    Require(sequence->Wait() == 0, "the wait after chained compute dispatches failed");
    Require(Output[0] == 0x44440004u, "a compute dispatch behind a sync point and a compute dispatch did not run");

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
    ::operator delete(memory, std::align_val_t{256});
}

void CapacityCase() {
    std::array<ShaderRegister, 255> registers;
    registers.fill(Registers[2]);
    std::copy(Registers.begin(), Registers.end(), registers.begin());
    auto shader = Shader;
    const ShaderRegister* data = registers.data();
    std::memcpy(shader.data() + 0x20, &data, sizeof(data));
    shader[0x5c] = static_cast<unsigned char>(registers.size());
    const std::string marker(65535, 'm');
    const MemorySizeQueryParams query{1, 1, 0, 0, 2, 0, 1, 1, 1};
    const auto size = RequiredSize(&query);
    const auto allocationSize = (size + 255) & ~std::uint64_t{255};
    auto* memory = static_cast<std::uint8_t*>(::operator new(allocationSize, std::align_val_t{256}));
    std::memset(memory, 0, allocationSize);
    std::memset(memory + size, 0xa5, allocationSize - size);
    alignas(64) unsigned char manager[0x100]{};
    Construct(manager);
    Require(Initialize(manager, memory, size, 1, 0, 1, 1, nullptr) == 0, "capacity manager initialization failed");
    auto* factory = Factories(manager, 0);
    ISequence* sequence = nullptr;
    Require(factory->CreateCompute(&sequence, 0, 2, 0, 1, nullptr) == 0, "capacity sequence creation failed");
    for (std::uint32_t round = 0; round < 2; ++round) {
        Value[0] = 0xabcd0000u + round;
        for (int dispatch = 0; dispatch < 2; ++dispatch)
            Require(sequence->DispatchCompute(shader.data(), Table.data(), 1, 1, 1, marker.c_str()) == 0, "maximum register and marker dispatch failed");
        Require(sequence->Wait() == 0, "maximum packet wait failed");
        Require(Output[0] == Value[0], "maximum packet dispatch did not finish");
        const auto* words = reinterpret_cast<const std::uint32_t*>(memory);
        std::uint32_t markers = 0;
        for (std::size_t offset = 0; offset + 0x4001 <= size / 4; ++offset) {
            if (words[offset] != 0xffff102cu) continue;
            Require(std::memcmp(words + offset + 1, marker.c_str(), marker.size() + 1) == 0, "maximum marker was truncated");
            ++markers;
        }
        Require(markers == 2, "the maximum markers were not stored in the command area");
    }
    Require(factory->DestroyCompute(sequence) == 0, "capacity sequence destruction failed");
    Require(Shutdown(manager) == 0, "capacity manager shutdown failed");
    for (auto offset = size; offset < allocationSize; ++offset)
        Require(memory[offset] == 0xa5, "maximum packets escaped the supplied work area");
    ::operator delete(memory, std::align_val_t{256});
}

void StorageCase(bool directMemory) {
    constexpr std::uint64_t mask = (1u << 1) | (1u << 5) | (1u << 8);
    const MemorySizeQueryParams query{1, 2, 0, 0, 32, 8, 4, 2, mask};
    const auto size = RequiredSize(&query);
    const auto allocationSize = (size + 0xffff) & ~std::uint64_t{0xffff};
    std::int64_t physical = 0;
    void* memory = nullptr;
    if (directMemory) {
        Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, allocationSize, 0x10000, 0, &physical) == 0, "direct work area allocation failed");
        Require(sceKernelMapDirectMemory(&memory, allocationSize, 0x33, 0, physical, 0x10000) == 0, "direct work area mapping failed");
    } else {
        memory = ::operator new(allocationSize, std::align_val_t{256});
    }
    {
        const GuestArena::HostWrite destination(memory, allocationSize);
        std::memset(memory, 0, allocationSize);
        std::memset(static_cast<std::uint8_t*>(memory) + size, 0xa5, allocationSize - size);
    }
    alignas(64) unsigned char manager[0x100]{};
    Construct(manager);
    Require(Initialize(manager, memory, size, 2, 0, 2, mask, nullptr) == 0, "storage manager initialization failed");
    auto* factory = Factories(manager, 0);
    std::array<ISequence*, 2> sequences{};
    for (auto& sequence : sequences) Require(factory->CreateCompute(&sequence, 0, 16, 4, 2, nullptr) == 0, "storage sequence creation failed");
    for (std::uint32_t round = 0; round < 3; ++round) {
        for (auto* sequence : sequences) {
            Value[0] = 0x56780000u + round;
            for (int dispatch = 0; dispatch < 8; ++dispatch)
                Require(sequence->DispatchCompute(Shader.data(), Table.data(), 1, 1, 1, "batch") == 0, "batched dispatch failed");
            Require(sequence->Wait() == 0, "batched wait failed");
            Require(Output[0] == Value[0], "batched work was not visible after wait");
            Require(sequence->Wait() == 0, "empty wait after a batch failed");
        }
        const auto* words = static_cast<const std::uint32_t*>(memory);
        const auto base = reinterpret_cast<std::uintptr_t>(memory);
        std::vector<std::uintptr_t> labels;
        for (std::size_t index = 0; index + 8 <= size / 4; ++index) {
            if (words[index] != 0xc0064900u || (words[index + 2] & 0xe7000000u) != 0x22000000u) continue;
            const auto address = words[index + 3] | (std::uint64_t{words[index + 4]} << 32);
            Require(address >= base && address + 8 <= base + size && (address & 7) == 0, "a completion label is outside the caller's work area");
            Require(words[index + 5] == 8 * (round + 1) && words[index + 6] == 0, "a completion did not publish the batched 32-bit count");
            Require(*reinterpret_cast<const volatile std::uint64_t*>(address) == words[index + 5], "a published batch was not retired");
            labels.push_back(address);
        }
        Require(labels.size() == 2 && labels[0] != labels[1], "each queue must store one batch completion in the caller's work area");
    }
    for (auto* sequence : sequences) Require(factory->DestroyCompute(sequence) == 0, "storage sequence destruction failed");
    for (auto& sequence : sequences) {
        Require(factory->CreateCompute(&sequence, 0, 16, 4, 2, nullptr) == 0, "storage sequence reuse failed");
        Require(sequence->DispatchCompute(Shader.data(), Table.data(), 1, 1, 1, nullptr) == 0, "dispatch after sequence reuse failed");
        Require(factory->DestroyCompute(sequence) == 0, "destroy did not wait for reused sequence work");
    }
    Require(Shutdown(manager) == 0, "storage manager shutdown failed");
    const auto* bytes = static_cast<const std::uint8_t*>(memory);
    for (auto offset = size; offset < allocationSize; ++offset)
        Require(bytes[offset] == 0xa5, "the manager wrote beyond its supplied work area");
    if (directMemory) Require(sceKernelReleaseDirectMemory(physical, allocationSize) == 0, "direct work area release failed");
    else ::operator delete(memory, std::align_val_t{256});
}

}

int RunJobManagerCase(const char* name) {
    try {
        Require(std::strcmp(name, "compute") == 0 || std::strcmp(name, "size") == 0 || std::strcmp(name, "execution") == 0 || std::strcmp(name, "storage") == 0 || std::strcmp(name, "capacity") == 0, "unknown compute fixture");
#ifndef ANYPS5_JOB_MANAGER_REFERENCE
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        device.reset();
#endif
        BuildShader();
        if (std::strcmp(name, "compute") == 0 || std::strcmp(name, "size") == 0) SizeCase();
        if (std::strcmp(name, "compute") == 0 || std::strcmp(name, "execution") == 0) RunCase();
        if (std::strcmp(name, "compute") == 0 || std::strcmp(name, "capacity") == 0) CapacityCase();
        if (std::strcmp(name, "compute") == 0 || std::strcmp(name, "storage") == 0) {
            StorageCase(false);
            StorageCase(true);
        }
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

#ifndef ANYPS5_JOB_MANAGER_REFERENCE
int main(int argc, char** argv) {
    if (argc > 2) return 2;
    return RunJobManagerCase(argc == 2 ? argv[1] : "compute");
}
#endif
