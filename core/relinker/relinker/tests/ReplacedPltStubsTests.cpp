#include <relinker/analysis/ReplacedPltStubs.hpp>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;

constexpr Relinker::VirtualAddress Base = 0x10000;
constexpr Relinker::VirtualAddress Got = 0x30000;
constexpr std::size_t Header = 0x100;
constexpr std::size_t Count = 4;

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}

void Displace(Bytes& code, std::size_t at, Relinker::VirtualAddress target, std::size_t next) {
    const auto displacement = static_cast<std::int32_t>(static_cast<std::int64_t>(target - (Base + next)));
    std::memcpy(code.data() + at, &displacement, sizeof(displacement));
}

Bytes Linked(std::size_t index) {
    Bytes code(16);
    const std::size_t at = Header + 16 * (index + 1);
    const auto slot = static_cast<std::uint32_t>(index);
    code[0] = 0xff;
    code[1] = 0x25;
    code[6] = 0x68;
    code[11] = 0xe9;
    const auto got = static_cast<std::int32_t>(static_cast<std::int64_t>(Got + 24 + 8 * index - (Base + at + 6)));
    const auto header = static_cast<std::int32_t>(static_cast<std::int64_t>(Header) - static_cast<std::int64_t>(at + 16));
    std::memcpy(code.data() + 2, &got, sizeof(got));
    std::memcpy(code.data() + 7, &slot, sizeof(slot));
    std::memcpy(code.data() + 12, &header, sizeof(header));
    return code;
}

Bytes Image() {
    Bytes code(0x200, 0x90);
    code[Header] = 0xff;
    code[Header + 1] = 0x35;
    Displace(code, Header + 2, Got + 8, Header + 6);
    code[Header + 6] = 0xff;
    code[Header + 7] = 0x25;
    Displace(code, Header + 8, Got + 16, Header + 12);
    for (std::size_t index = 0; index < Count; ++index) {
        const auto stub = Linked(index);
        std::copy(stub.begin(), stub.end(), code.begin() + static_cast<std::ptrdiff_t>(Header + 16 * (index + 1)));
    }
    return code;
}

void Put(Bytes& code, std::size_t at, const Bytes& bytes) {
    std::copy(bytes.begin(), bytes.end(), code.begin() + static_cast<std::ptrdiff_t>(at));
}

std::vector<Relinker::ReplacedPltStub> Find(const Bytes& code, Relinker::ByteCount relocations = Count * 24) {
    return Relinker::FindReplacedPltStubs(code, Base, Got, relocations);
}

void CheckLinkedTables() {
    Require(Find(Image()).empty(), "a linked PLT was reported");
    Require(Find(Image(), 0).empty(), "a PLT without relocations was reported");
    Require(Find({}).empty(), "an empty segment was reported");
    auto code = Image();
    code[Header + 1] = 0x25;
    Require(Find(code).empty(), "a segment without a PLT header was reported");
    code = Image();
    Put(code, Header + 16, {0xb8, 0, 0, 0, 0, 0xc3});
    Require(Find(code, (Count + 20) * 24).empty(), "a PLT longer than its segment was reported");
    code = Image();
    for (std::size_t index = 0; index < Count; ++index) Put(code, Header + 16 * (index + 1), {0xb8, 0, 0, 0, 0, 0xc3});
    Require(Find(code).empty(), "a table with no linked stub was reported");
}

void CheckReplacedStubs() {
    auto code = Image();
    const Bytes returnZero{0xb8, 0, 0, 0, 0, 0xc3};
    const Bytes whole{0x48, 0xb8, 0x2f, 0x61, 0x70, 0x70, 0x30, 0x2f, 0x00, 0x00, 0x48, 0x89, 0x02, 0x31, 0xc0, 0xc3};
    Bytes jump{0xe9, 0, 0, 0, 0, 0x90};
    const std::size_t jumpStub = Header + 16 * 4;
    const std::size_t helper = 0x180;
    Displace(jump, 1, Base + helper, jumpStub + 5);
    Put(code, Header + 16, returnZero);
    Put(code, Header + 16 * 3, whole);
    Put(code, jumpStub, jump);
    Put(code, helper, {0x31, 0xc9, 0x74, 0x02, 0x31, 0xd2, 0x0f, 0x05, 0xc3, 0x0f, 0x05});
    const auto stubs = Find(code);
    const auto linkedJump = [](std::size_t index) {
        auto stub = Linked(index);
        stub.resize(6);
        return stub;
    };
    Require(stubs.size() == 3, "replaced stub count is wrong");
    Require(stubs[0].Address == Base + Header + 16 && stubs[0].GotEntry == Got + 24 && stubs[0].Found == returnZero
            && stubs[0].Linked == linkedJump(0) && !stubs[0].JumpTarget, "six-byte replacement is wrong");
    Require(stubs[1].Address == Base + Header + 48 && stubs[1].GotEntry == Got + 40 && stubs[1].Found == whole
            && stubs[1].Linked == Linked(2) && !stubs[1].JumpTarget, "whole-stub replacement is wrong");
    Require(stubs[2].Address == Base + jumpStub && stubs[2].GotEntry == Got + 48 && stubs[2].Found == jump
            && stubs[2].Linked == linkedJump(3) && stubs[2].JumpTarget == Base + helper, "jump replacement is wrong");

    Require(Relinker::DescribeReplacedPltStub(stubs[2], "sce_module/libc.prx", "Hc4CaR6JBL0")
            == "Replaced PLT stub: sce_module/libc.prx va=0x10140 ff25eafe0100 -> e93b00000090 (jump to 0x10180); import Hc4CaR6JBL0 is not called through it; "
               "a --patches entry with expected = \"e93b00000090\" and value = \"ff25eafe0100\" restores the linked stub\n", "jump description is wrong");
    Require(Relinker::DescribeReplacedPltStub(stubs[0], {}, {})
            == "Replaced PLT stub: va=0x10110 ff2502ff0100 -> b800000000c3; "
               "a --patches entry with expected = \"b800000000c3\" and value = \"ff2502ff0100\" restores the linked stub\n", "unnamed description is wrong");

    Require(Relinker::DescribeReplacedPltStubReaching(stubs, code, Base, Base + helper + 6)
            == "; it is in the code at 0x10180 that the replaced PLT stub at 0x10140 jumps to", "reached instruction is not explained");
    Require(Relinker::DescribeReplacedPltStubReaching(stubs, code, Base, Base + helper + 9).empty(), "an instruction after the return was explained");
    Require(Relinker::DescribeReplacedPltStubReaching(stubs, code, Base, Base + helper + 5).empty(), "an address inside an instruction was explained");
    Require(Relinker::DescribeReplacedPltStubReaching(stubs, code, Base, Base + helper - 1).empty(), "an address before the target was explained");
    Require(Relinker::DescribeReplacedPltStubReaching(stubs, code, Base, Base + code.size()).empty(), "an address past the segment was explained");
    Require(Relinker::DescribeReplacedPltStubReaching({}, code, Base, Base + helper + 6).empty(), "an instruction was explained without a replaced stub");
}

}

int main() {
    try {
        CheckLinkedTables();
        CheckReplacedStubs();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
