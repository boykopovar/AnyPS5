#include <elfpatcher/stub/EntryStubBuilder.hpp>

namespace Elfpatcher {

std::vector<std::uint8_t> EntryStubBuilder::BuildEntryStub(
    std::uint64_t stubVaddr,
    std::uint64_t realEntryVaddr
) const {
    std::vector<std::uint8_t> s;
    s.push_back(0x58);
    s.push_back(0x48); s.push_back(0x89); s.push_back(0xe3);
    s.push_back(0x48); s.push_back(0x83); s.push_back(0xec); s.push_back(0x30);
    s.push_back(0x48); s.push_back(0x83); s.push_back(0xe4); s.push_back(0xf0);
    s.push_back(0x89); s.push_back(0x04); s.push_back(0x24);
    s.push_back(0x48); s.push_back(0x89); s.push_back(0x5c); s.push_back(0x24); s.push_back(0x08);
    s.push_back(0x48); s.push_back(0x89); s.push_back(0xe7);
    s.push_back(0x48); s.push_back(0x31); s.push_back(0xf6);
    const std::uint64_t jmpInsnVaddr = stubVaddr + s.size();
    const std::uint64_t jmpNextVaddr = jmpInsnVaddr + 5;
    const std::int32_t rel32 = static_cast<std::int32_t>(realEntryVaddr - jmpNextVaddr);
    s.push_back(0xe9);
    s.push_back(static_cast<std::uint8_t>(rel32 & 0xff));
    s.push_back(static_cast<std::uint8_t>((rel32 >> 8) & 0xff));
    s.push_back(static_cast<std::uint8_t>((rel32 >> 16) & 0xff));
    s.push_back(static_cast<std::uint8_t>((rel32 >> 24) & 0xff));
    return s;
}

}
