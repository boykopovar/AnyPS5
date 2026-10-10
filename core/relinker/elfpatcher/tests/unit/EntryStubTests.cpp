#include <Testing/Test.hpp>
#include <elfpatcher/general/EntryStubBuilder.hpp>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

constexpr std::uint64_t StubVaddr = 0xb32db98;
constexpr std::uint64_t EntryVaddr = 0x80;
constexpr std::size_t CallOffset = 17;
constexpr std::size_t CallSize = 5;

std::vector<std::uint8_t> buildStub() {
    return Elfpatcher::EntryStubBuilder().BuildEntryStub(StubVaddr, EntryVaddr);
}

bool bytesAt(const std::vector<std::uint8_t>& stub, std::size_t offset, const std::vector<std::uint8_t>& expected) {
    return offset + expected.size() <= stub.size() && std::memcmp(stub.data() + offset, expected.data(), expected.size()) == 0;
}

const Testing::Case passesStackPointer{"EntryStub_Prologue_PassesInitialStackPointerInRdi", [] {
    const auto stub = buildStub();

    Testing::Require(bytesAt(stub, 0, {0x48, 0x89, 0xe7}), "the stub does not pass the initial stack pointer in rdi");
}};

const Testing::Case alignsStack{"EntryStub_Prologue_AlignsStackTo16Bytes", [] {
    const auto stub = buildStub();

    Testing::Require(bytesAt(stub, 3, {0x48, 0x83, 0xe4, 0xf0}), "the stub does not align the stack to 16 bytes");
}};

const Testing::Case pushesZeroFrame{"EntryStub_Prologue_PushesZeroFrameRecord", [] {
    const auto stub = buildStub();

    Testing::Require(bytesAt(stub, 7, {0x6a, 0x00, 0x6a, 0x00}), "the stub does not push a zero frame record");
}};

const Testing::Case pointsRbpAtZeroFrame{"EntryStub_Prologue_PointsRbpAtZeroFrameRecord", [] {
    const auto stub = buildStub();

    Testing::Require(bytesAt(stub, 11, {0x48, 0x89, 0xe5}), "the stub does not point rbp at the zero frame record");
}};

const Testing::Case clearsRsi{"EntryStub_Prologue_ClearsRsi", [] {
    const auto stub = buildStub();

    Testing::Require(bytesAt(stub, 14, {0x48, 0x31, 0xf6}), "the stub does not clear rsi");
}};

const Testing::Case callsEntryPoint{"EntryStub_Call_TargetsEntryPoint", [] {
    const auto stub = buildStub();

    Testing::Require(CallOffset + CallSize <= stub.size() && stub[CallOffset] == 0xe8, "the stub does not call the entry point");
    std::int32_t rel32 = 0;
    std::memcpy(&rel32, stub.data() + CallOffset + 1, sizeof(rel32));
    Testing::RequireEqual(StubVaddr + CallOffset + CallSize + static_cast<std::int64_t>(rel32), EntryVaddr, "the stub calls the wrong address");
}};

const Testing::Case endsWithUd2{"EntryStub_AfterCall_EndsWithUd2", [] {
    const auto stub = buildStub();

    Testing::Require(bytesAt(stub, CallOffset + CallSize, {0x0f, 0x0b}), "the stub does not end with ud2 after the call");
    Testing::RequireEqual(stub.size(), CallOffset + CallSize + 2, "the stub has trailing bytes after ud2");
}};

} // namespace
