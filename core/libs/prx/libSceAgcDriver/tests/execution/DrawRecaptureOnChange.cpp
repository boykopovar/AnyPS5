#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/SynchronizationStatistics.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t TableOffset = 0x300;
constexpr std::size_t MovedOffset = 0x800;
constexpr std::size_t ShiftOffset = 0x900;
constexpr std::size_t StillOffset = 0xa00;
constexpr std::size_t LabelOffset = 0xc00;
constexpr std::size_t ColorBytes = 65536;
constexpr std::uint32_t RenderTargetTileMode = 0x1b;
constexpr std::size_t FetchBufferTableOffset = 0x000;
constexpr std::size_t FetchAttribTableOffset = 0x100;
constexpr std::size_t StaleSlotsOffset = 0x200;
constexpr std::size_t CurrentSlotsOffset = 0x400;
constexpr std::size_t InstanceSlotsOffset = 0x600;
constexpr std::uint32_t SlotRecords = 32;
constexpr std::uint32_t StaleSlotShift = 16;
constexpr std::uint32_t OutputSlots = 32;
constexpr std::uint32_t Unwritten = 0xdeadbeefu;
constexpr std::uint32_t FetchIndexOffset = 5;
constexpr std::uint32_t FetchVertexOffset = 3;
constexpr std::uint32_t FetchVertices = 3;

alignas(256) constexpr std::array<std::uint32_t, 40> VertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u,
    0xf4080104u, 0xfa000000u, 0xf4080204u, 0xfa000010u, 0xf4080304u, 0xfa000020u,
    0xbf8cc07fu,
    0xe00c2000u, 0x80010a05u, 0xe00c2000u, 0x80020e05u, 0xe00c2000u, 0x80031205u,
    0xbf8c3f70u,
    0x0602030au, 0x0604050bu, 0x0606070cu, 0x0608090du,
    0x0602030eu, 0x0604050fu, 0x06060710u, 0x06080911u,
    0x06020312u, 0x06040513u, 0x06060714u, 0x06080915u,
    0xf80008cfu, 0x04030201u,
    0xbf810000u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 11> PixelRead{0x7e1a0280u, 0xdc308000u, 0x0c02000du, 0xbf8c3f70u, 0x7e10030cu, 0x7e120280u, 0x7e140280u, 0x7e160280u, 0xf800180fu, 0x0b0a0908u, 0xbf810000u};

alignas(256) constexpr std::array<std::uint32_t, 30> FetchVertexCode{
    0x7e020305u, 0x7e040308u,
    0xd70f6a05u, 0x00020a10u,
    0xd70f6a08u, 0x00021011u,
    0xf4080506u, 0xfa000000u,
    0xf4080606u, 0xfa000010u,
    0x02060b08u,
    0xbf8cc07fu,
    0xe0002000u, 0x80050a03u,
    0xe0002000u, 0x80060b03u,
    0xbf8c3f70u,
    0x4a08170au, 0x34080884u,
    0x7e180301u, 0x7e1a0302u, 0x7e1c0305u, 0x7e1e0308u,
    0xe0781000u, 0x80020c04u,
    0x7e200280u, 0x7e2202f2u,
    0xf80008cfu, 0x11101010u,
    0xbf810000u,
};

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(8) Header VertexHeader{};
alignas(8) Header PixelHeader{};

void Register(Header& header, const std::uint32_t* code, std::size_t bytes, std::uint8_t type) {
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(Header);
    header.shader.shader_size = static_cast<std::uint32_t>(bytes);
    header.shader.code = code;
    header.shader.type = type;
    header.shader.user_data = &header.userData;
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

struct FetchHeader {
    Shader shader{};
    ShaderUserData userData{};
    std::array<std::uint16_t, ShaderRegs::AGC_DIRECT_RESOURCE_TYPE_COUNT> direct{};
    std::array<ShaderSemantic, 2> semantics{};
};

alignas(8) FetchHeader FetchVertexHeader{};

void RegisterFetchVertex() {
    auto& header = FetchVertexHeader;
    header.direct.fill(ShaderRegs::AGC_ILLEGAL_DIRECT_OFFSET);
    header.direct[static_cast<std::size_t>(ShaderRegs::AgcDirectResourceType::PtrVertexBufferTable)] = 4;
    header.direct[static_cast<std::size_t>(ShaderRegs::AgcDirectResourceType::PtrVertexAttribDescTable)] = 6;
    header.userData.direct_resource_offset = header.direct.data();
    header.userData.direct_resource_count = static_cast<std::uint16_t>(header.direct.size());
    header.semantics[0].semantic = 0;
    header.semantics[0].hardware_mapping = 10;
    header.semantics[0].size_in_elements = 1;
    header.semantics[1].semantic = 1;
    header.semantics[1].hardware_mapping = 11;
    header.semantics[1].size_in_elements = 1;
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(FetchHeader);
    header.shader.shader_size = static_cast<std::uint32_t>(sizeof(FetchVertexCode));
    header.shader.code = FetchVertexCode.data();
    header.shader.type = 2;
    header.shader.user_data = &header.userData;
    header.shader.input_semantics = header.semantics.data();
    header.shader.num_input_semantics = static_cast<std::uint32_t>(header.semantics.size());
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        constexpr std::uintptr_t alignment = 65536;
        void* mapped = mmap(nullptr, BlockBytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + BlockBytes != begin + BlockBytes + alignment) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + alignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
        }
#endif
        Require(block != nullptr, "draw recapture: cannot allocate a guest block");
        std::memset(block, 0, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint64_t At(std::size_t offset) const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block + offset)); }
    std::uint8_t* Bytes(std::size_t offset) const { return block + offset; }

private:
    std::uint8_t* block = nullptr;
};

std::array<std::uint32_t, 4> VertexDescriptor(std::uint64_t address) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), 3u, 0x0104dfacu};
}

void WriteVertices(const GuestBlock& block, std::size_t offset, float x) {
    for (std::size_t vertex = 0; vertex < 3; ++vertex) {
        const std::array<float, 4> value{x, 0.0f, 0.0f, 0.0f};
        std::memcpy(block.Bytes(offset + vertex * 16u), value.data(), sizeof(value));
    }
}

void WriteDescriptor(const GuestBlock& block, std::size_t slot, std::array<std::uint32_t, 4> descriptor) {
    std::memcpy(block.Bytes(TableOffset + slot * 16u), descriptor.data(), sizeof(descriptor));
}

const GuestBlock* TableBlock = nullptr;
std::atomic<bool> TableChangeArmed{false};
std::atomic<bool> WaitArmed{false};
std::atomic<int> TableChanges{0};
std::array<std::uint32_t, 7> WaitPacket{};

void ArmWait(const GuestBlock& block, std::size_t offset, std::uint32_t reference) {
    const auto label = block.At(offset);
    WaitPacket = {0xc0053c00u, 0x13u, static_cast<std::uint32_t>(label), static_cast<std::uint32_t>(label >> 32u), reference, 0xffffffffu, 10u};
    WaitArmed.store(true);
}

void SetLabel(const GuestBlock& block, std::size_t offset, std::uint32_t value) {
    std::memcpy(block.Bytes(offset), &value, sizeof(value));
}

void ChangeTableBeforeCheck() {
    if (WaitArmed.exchange(false)) AgcDriver::DriverDetail::Driver::NotePendingWaitForTest(WaitPacket);
    if (!TableChangeArmed.exchange(false)) return;
    WriteDescriptor(*TableBlock, 0, VertexDescriptor(TableBlock->At(MovedOffset)));
    TableChanges.fetch_add(1);
}

constexpr std::size_t AddSlotZeroWord = 24;
constexpr std::size_t AddSlotOneWord = 28;
static_assert(VertexCode[AddSlotZeroWord] == 0x0602030au && VertexCode[AddSlotOneWord] == 0x0602030eu, "the patched words are v_add_f32 v1, v10, v1 and v_add_f32 v1, v14, v1");

const GuestBlock* CodeBlock = nullptr;
std::atomic<bool> CodePatchArmed{false};
std::atomic<int> CodePatches{0};

void ChangeCodeBeforeCheck() {
    if (!CodePatchArmed.exchange(false)) return;
    std::memcpy(CodeBlock->Bytes(AddSlotOneWord * sizeof(std::uint32_t)), &VertexCode[AddSlotZeroWord], sizeof(std::uint32_t));
    CodePatches.fetch_add(1);
}

std::array<std::uint32_t, 4> SlotDescriptor(std::uint64_t address) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), SlotRecords, 0x01014facu};
}

std::array<std::uint32_t, 4> OutputDescriptor(std::uint64_t address) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), OutputSlots * 16u, 0x31016facu};
}

void WriteFetchDescriptor(const GuestBlock& block, std::size_t index, std::array<std::uint32_t, 4> descriptor) {
    std::memcpy(block.Bytes(FetchBufferTableOffset + index * 16u), descriptor.data(), sizeof(descriptor));
}

void WriteSlots(const GuestBlock& block, std::size_t offset, std::uint32_t shift) {
    for (std::uint32_t record = 0; record < SlotRecords; ++record) {
        const std::uint32_t slot = record + shift;
        std::memcpy(block.Bytes(offset + record * sizeof(slot)), &slot, sizeof(slot));
    }
}

void PrepareFetchTables(const GuestBlock& fetch) {
    WriteSlots(fetch, StaleSlotsOffset, StaleSlotShift);
    WriteSlots(fetch, CurrentSlotsOffset, 0);
    std::memset(fetch.Bytes(InstanceSlotsOffset), 0, SlotRecords * sizeof(std::uint32_t));
    const std::array<std::uint32_t, 2> attributes{0u, 1u | (1u << 26u)};
    std::memcpy(fetch.Bytes(FetchAttribTableOffset), attributes.data(), sizeof(attributes));
    WriteFetchDescriptor(fetch, 0, SlotDescriptor(fetch.At(CurrentSlotsOffset)));
    WriteFetchDescriptor(fetch, 1, SlotDescriptor(fetch.At(InstanceSlotsOffset)));
}

const GuestBlock* FetchBlock = nullptr;
std::atomic<bool> FetchChangeArmed{false};
std::atomic<bool> FetchCheckedOnFinisher{false};
std::atomic<int> FetchChanges{0};

void ChangeFetchBufferBeforeCheck() {
    if (!FetchChangeArmed.exchange(false)) return;
    FetchCheckedOnFinisher.store(AgcDriver::DriverDetail::DrawFinisher::OnFinisherThread());
    WriteFetchDescriptor(*FetchBlock, 0, SlotDescriptor(FetchBlock->At(CurrentSlotsOffset)));
    FetchChanges.fetch_add(1);
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

using RegisterWrites = std::vector<std::pair<std::uint32_t, std::uint32_t>>;

void SubmitDrawWith(std::uint64_t vertex, const RegisterWrites& userConfig, const RegisterWrites& vertexRegisters, const GuestBlock& data, const GuestBlock& target) {
    const auto color = target.At(0);
    const auto pixel = reinterpret_cast<std::uintptr_t>(PixelRead.data());
    const auto pointer = data.At(0);
    const float scale = 32.0f;
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000},
        {0x1b6, 0}, {0x207, 0}, {0x200, 0}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4}, {0x31c, 0x28028},
        {0x31b, 0}, {0x31d, 0}, {0x3b0, (63u << 14u) | 3u},
        {0x3b8, 0x9000000u | (RenderTargetTileMode << 14u)}, {0x1e0, 0},
        {0xc, 0}, {0xd, 0x40040},
        {0x81, 0x80000000}, {0x82, 0x40040},
        {0x90, 0x80000000}, {0x91, 0x40040},
        {0x94, 0x80000000}, {0x95, 0x40040},
        {0x318, static_cast<std::uint32_t>(color >> 8u)}, {0x390, static_cast<std::uint32_t>(color >> 40u)},
        {0x10f, std::bit_cast<std::uint32_t>(scale)}, {0x110, std::bit_cast<std::uint32_t>(scale)},
        {0x111, std::bit_cast<std::uint32_t>(-2.0f)}, {0x112, std::bit_cast<std::uint32_t>(2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
        {0x2fe, 0}, {0x302, 0}, {0x306, 0}, {0x30a, 0},
    };
    std::vector<std::uint32_t> commands;
    for (const auto& [offset, value] : userConfig) Append(commands, 0x79, {offset, value});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    Append(commands, 0x76, {0xc8, static_cast<std::uint32_t>(vertex >> 8u)});
    Append(commands, 0x76, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)});
    for (const auto& [offset, value] : vertexRegisters) Append(commands, 0x76, {offset, value});
    for (const auto& [offset, value] : std::initializer_list<std::pair<std::uint32_t, std::uint32_t>>{{0x00b, 8u << 1u}, {0x00c, 0}, {0x00d, 0}}) Append(commands, 0x76, {offset, value});
    Append(commands, 0x76, {0x008, static_cast<std::uint32_t>(pixel >> 8u), static_cast<std::uint32_t>(pixel >> 40u)});
    Append(commands, 0x76, {0x00e, static_cast<std::uint32_t>(pointer), static_cast<std::uint32_t>(pointer >> 32u)});
    Append(commands, 0x2d, {3, 2});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "draw recapture: the draw submission failed");
    AgcDriverWaitIdle_nid_postfix();
}

void SubmitDraw(const GuestBlock& vertices, const GuestBlock& data, const GuestBlock& target, std::uint64_t vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data())) {
    const auto table = vertices.At(TableOffset);
    SubmitDrawWith(vertex, {{0x242, 4}}, {{0x8b, 2u << 1u}, {0x8c, static_cast<std::uint32_t>(table)}, {0x8d, static_cast<std::uint32_t>(table >> 32u)}}, data, target);
}

void SubmitFetchDraw(const GuestBlock& fetch, const GuestBlock& output, const GuestBlock& data, const GuestBlock& target) {
    const auto stored = OutputDescriptor(output.At(0));
    const auto buffers = fetch.At(FetchBufferTableOffset);
    const auto attributes = fetch.At(FetchAttribTableOffset);
    const std::array<std::uint32_t, 10> user{stored[0], stored[1], stored[2], stored[3], static_cast<std::uint32_t>(buffers), static_cast<std::uint32_t>(buffers >> 32u), static_cast<std::uint32_t>(attributes), static_cast<std::uint32_t>(attributes >> 32u), FetchVertexOffset, 0u};
    RegisterWrites registers{{0x8b, static_cast<std::uint32_t>(user.size()) << 1u}};
    for (std::uint32_t word = 0; word < user.size(); ++word) registers.emplace_back(0x8cu + word, user[word]);
    SubmitDrawWith(reinterpret_cast<std::uintptr_t>(FetchVertexCode.data()), {{0x242, 4}, {0x24a, FetchIndexOffset}}, registers, data, target);
}

unsigned Brightest(const GuestBlock& target) {
    AgcDriver::GuestMemory::FlushGpuWrites(static_cast<std::uintptr_t>(target.At(0)), ColorBytes);
    unsigned brightest = 0;
    for (std::size_t i = 0; i < ColorBytes; ++i) brightest = std::max(brightest, static_cast<unsigned>(target.Bytes(0)[i]));
    return brightest;
}

void Expect(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("draw recapture: " + what);
}

void ChangedTableIsBound(const GuestBlock& vertices, const GuestBlock& data, const GuestBlock& target) {
    std::memset(target.Bytes(0), 0, ColorBytes);
    WriteDescriptor(vertices, 0, {});
    const auto retries = AgcDriver::DriverDetail::captureChangeRetries.load();
    ArmWait(vertices, LabelOffset, 1);
    TableChangeArmed.store(true);
    SubmitDraw(vertices, data, target);
    const auto brightest = Brightest(target);
    const auto redone = AgcDriver::DriverDetail::captureChangeRetries.load() - retries;
    std::printf("a draw behind an unsatisfied wait whose descriptor table changed from a zero V# to a valid one after its capture: the table changed %d time(s), brightest color byte %u (%s), %llu packet(s) prepared again\n", TableChanges.load(), brightest, brightest == 255u ? "the valid V# was bound" : "the zero V# captured before the change was bound", static_cast<unsigned long long>(redone));
    Expect(TableChanges.load() == 1, "the check seam did not run before the draw was recorded");
    Expect(brightest == 255u, "the draw bound the zero V# captured before the table changed, not the valid V# in memory when it was recorded");
    Expect(redone == 1, "the draw was prepared again " + std::to_string(redone) + " times, not once");
}

void WaitedUnchangedTableIsCompared(const GuestBlock& vertices, const GuestBlock& data, const GuestBlock& target) {
    std::memset(target.Bytes(0), 0, ColorBytes);
    const auto retries = AgcDriver::DriverDetail::captureChangeRetries.load();
    const auto checks = AgcDriver::DriverDetail::captureChecks.load();
    ArmWait(vertices, LabelOffset, 1);
    SubmitDraw(vertices, data, target);
    const auto brightest = Brightest(target);
    const auto redone = AgcDriver::DriverDetail::captureChangeRetries.load() - retries;
    const auto compared = AgcDriver::DriverDetail::captureChecks.load() - checks;
    std::printf("the same draw behind the unsatisfied wait with its table unchanged: brightest color byte %u, %llu capture check(s) run, %llu packet(s) prepared again\n", brightest, static_cast<unsigned long long>(compared), static_cast<unsigned long long>(redone));
    Expect(brightest == 255u, "the draw behind an unsatisfied wait with an unchanged valid table did not draw");
    Expect(compared == 1, "a draw behind an unsatisfied wait ran " + std::to_string(compared) + " capture checks, not one");
    Expect(redone == 0, "a draw whose captured memory did not change was prepared again");
}

void UnchangedTableIsNotRedone(const GuestBlock& vertices, const GuestBlock& data, const GuestBlock& target) {
    std::memset(target.Bytes(0), 0, ColorBytes);
    SetLabel(vertices, LabelOffset, 1);
    const auto retries = AgcDriver::DriverDetail::captureChangeRetries.load();
    const auto checks = AgcDriver::DriverDetail::captureChecks.load();
    const auto skipped = AgcDriver::DriverDetail::captureChecksSkipped.load();
    SubmitDraw(vertices, data, target);
    const auto brightest = Brightest(target);
    const auto redone = AgcDriver::DriverDetail::captureChangeRetries.load() - retries;
    const auto compared = AgcDriver::DriverDetail::captureChecks.load() - checks;
    const auto passed = AgcDriver::DriverDetail::captureChecksSkipped.load() - skipped;
    std::printf("the same draw after memory satisfied the wait, table unchanged: brightest color byte %u, %llu capture check(s) run, %llu skipped, %llu packet(s) prepared again\n", brightest, static_cast<unsigned long long>(compared), static_cast<unsigned long long>(passed), static_cast<unsigned long long>(redone));
    Expect(brightest == 255u, "the draw with an unchanged valid table did not draw");
    Expect(compared == 0, "a draw with no unsatisfied wait compared its captured memory");
    Expect(passed >= 1, "a draw with no unsatisfied wait did not skip its capture check");
    Expect(redone == 0, "a draw whose captured memory did not change was prepared again");
}

void ChangedRawCodeIsRecompiled(const GuestBlock& vertices, const GuestBlock& data, const GuestBlock& target, const GuestBlock& code) {
    std::memset(target.Bytes(0), 0, ColorBytes);
    std::memcpy(code.Bytes(0), VertexCode.data(), sizeof(VertexCode));
    WriteDescriptor(vertices, 0, VertexDescriptor(vertices.At(StillOffset)));
    const auto retries = AgcDriver::DriverDetail::captureChangeRetries.load();
    CodePatchArmed.store(true);
    SubmitDraw(vertices, data, target, code.At(0));
    const auto brightest = Brightest(target);
    const auto redone = AgcDriver::DriverDetail::captureChangeRetries.load() - retries;
    std::printf("an unregistered vertex program whose code stopped adding the +8 x offset after its capture: the code changed %d time(s), brightest color byte %u (%s), %llu packet(s) prepared again\n", CodePatches.load(), brightest, brightest == 255u ? "the code in memory ran" : "the code captured before the change ran", static_cast<unsigned long long>(redone));
    Expect(CodePatches.load() == 1, "the check seam did not run before the unregistered program's draw was recorded");
    Expect(AgcDriver::DriverDetail::RawGraphicsDrawnIn(code.At(0), code.At(0) + sizeof(std::uint32_t)), "the vertex program was not captured as unregistered code");
    Expect(brightest == 255u, "the draw ran the unregistered vertex code captured before it changed, not the code in memory when it was recorded");
    Expect(redone == 1, "the draw of the changed unregistered program was prepared again " + std::to_string(redone) + " times, not once");
}

using FetchOutput = std::array<std::uint32_t, OutputSlots * 4u>;

void ClearFetchOutput(const GuestBlock& output) {
    FetchOutput words;
    words.fill(Unwritten);
    std::memcpy(output.Bytes(0), words.data(), sizeof(words));
}

FetchOutput ReadFetchOutput(const GuestBlock& output) {
    AgcDriver::GuestMemory::FlushGpuWrites(static_cast<std::uintptr_t>(output.At(0)), sizeof(FetchOutput));
    FetchOutput words;
    std::memcpy(words.data(), output.Bytes(0), sizeof(words));
    return words;
}

FetchOutput FoldedOnce() {
    FetchOutput words;
    words.fill(Unwritten);
    for (std::uint32_t vertex = 0; vertex < FetchVertices; ++vertex) {
        const auto raw = FetchIndexOffset + vertex;
        auto* entry = words.data() + static_cast<std::size_t>(raw + FetchVertexOffset) * 4u;
        entry[0] = raw;
        entry[1] = 0u;
        entry[2] = raw + FetchVertexOffset;
        entry[3] = 0u;
    }
    return words;
}

std::string WrittenSlots(const FetchOutput& words) {
    std::string text;
    for (std::uint32_t slot = 0; slot < OutputSlots; ++slot) {
        const auto* entry = words.data() + static_cast<std::size_t>(slot) * 4u;
        if (std::all_of(entry, entry + 4, [](std::uint32_t word) { return word == Unwritten; })) continue;
        char line[112];
        std::snprintf(line, sizeof(line), " [slot %u: v5 %u v8 %u, after the fetch adds %u %u]", slot, entry[0], entry[1], entry[2], entry[3]);
        text += line;
    }
    return text.empty() ? std::string(" nothing") : text;
}

void FetchOffsetIsFoldedOnceOnRecapture(const GuestBlock& fetch, const GuestBlock& output, const GuestBlock& data, const GuestBlock& target) {
    FetchBlock = &fetch;
    AgcDriver::DriverDetail::Driver::SetCaptureCheckHook(&ChangeFetchBufferBeforeCheck);
    WriteFetchDescriptor(fetch, 0, SlotDescriptor(fetch.At(CurrentSlotsOffset)));
    ClearFetchOutput(output);
    auto retries = AgcDriver::DriverDetail::captureChangeRetries.load();
    SubmitFetchDraw(fetch, output, data, target);
    const auto control = ReadFetchOutput(output);
    const auto controlRedone = AgcDriver::DriverDetail::captureChangeRetries.load() - retries;
    std::printf("a draw whose program folds s16 = %u into its first vertex (GE_INDX_OFFSET %u), with its vertex V# unchanged: wrote%s, %llu packet(s) prepared again\n", FetchVertexOffset, FetchIndexOffset, WrittenSlots(control).c_str(), static_cast<unsigned long long>(controlRedone));
    Expect(controlRedone == 0, "the control draw, whose captured memory did not change, was prepared again");
    Expect(control == FoldedOnce(), "the control draw did not fetch and store slots GE_INDX_OFFSET + s16 + vertex with v5 = GE_INDX_OFFSET + vertex, so s16 was not folded into its first vertex exactly once and this case cannot see a second fold");

    WriteFetchDescriptor(fetch, 0, SlotDescriptor(fetch.At(StaleSlotsOffset)));
    ClearFetchOutput(output);
    retries = AgcDriver::DriverDetail::captureChangeRetries.load();
    FetchChangeArmed.store(true);
    SubmitFetchDraw(fetch, output, data, target);
    const auto changed = ReadFetchOutput(output);
    const auto redone = AgcDriver::DriverDetail::captureChangeRetries.load() - retries;
    std::printf("the same draw with its vertex V# moved after its capture (checked on the %s): the V# changed %d time(s), wrote%s, %llu packet(s) prepared again\n", FetchCheckedOnFinisher.load() ? "draw finisher" : "queue worker", FetchChanges.load(), WrittenSlots(changed).c_str(), static_cast<unsigned long long>(redone));
    Expect(FetchChanges.load() == 1, "the check seam did not run before the fetching draw was recorded");
    if (AgcDriver::DriverDetail::DrawFinisher::Enabled()) Expect(FetchCheckedOnFinisher.load(), "the fetching draw was checked on the queue worker, not the draw finisher, so restartFinishedDraw was not exercised");
    Expect(changed == control, "the recaptured draw differs from the control: slots " + std::to_string(FetchVertexOffset) + " further on mean the retry folded s16 into the first vertex again, slots " + std::to_string(StaleSlotShift) + " further on mean it bound the V# captured before the change");
    Expect(redone == 1, "the draw was prepared again " + std::to_string(redone) + " times, not once");
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        Register(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
        Register(PixelHeader, PixelRead.data(), sizeof(PixelRead), 1);
        RegisterFetchVertex();
        GuestBlock vertices;
        GuestBlock data;
        GuestBlock target;
        GuestBlock code;
        GuestBlock fetch;
        GuestBlock output;
        WriteVertices(vertices, MovedOffset, -8.0f);
        WriteVertices(vertices, ShiftOffset, 8.0f);
        WriteVertices(vertices, StillOffset, 0.0f);
        WriteDescriptor(vertices, 1, VertexDescriptor(vertices.At(ShiftOffset)));
        WriteDescriptor(vertices, 2, VertexDescriptor(vertices.At(StillOffset)));
        const float one = 1.0f;
        std::memcpy(data.Bytes(0), &one, sizeof(one));
        PrepareFetchTables(fetch);
        TableBlock = &vertices;
        CodeBlock = &code;
        AgcDriver::DriverDetail::Driver::SetCaptureCheckHook(&ChangeTableBeforeCheck);
        ChangedTableIsBound(vertices, data, target);
        WaitedUnchangedTableIsCompared(vertices, data, target);
        UnchangedTableIsNotRedone(vertices, data, target);
        AgcDriver::DriverDetail::Driver::SetCaptureCheckHook(&ChangeCodeBeforeCheck);
        ChangedRawCodeIsRecompiled(vertices, data, target, code);
        FetchOffsetIsFoldedOnceOnRecapture(fetch, output, data, target);
        AgcDriver::DriverDetail::Driver::SetCaptureCheckHook(nullptr);
        AgcDriverShutdown_nid_postfix();
        std::puts("draw recapture: a draw whose captured descriptor table or unregistered program code changed before it was recorded uses the memory in place when it is recorded, folding its vertex offset once");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { AgcDriverShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
