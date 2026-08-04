#include <elfpatcher/general/EntryStubBuilder.hpp>
#include <elfpatcher/general/ElfConstants.hpp>

namespace Elfpatcher {

namespace {
void _appendBytes(std::vector<std::uint8_t>& s, const std::uint8_t* bytes, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i)
        s.push_back(bytes[i]);
}
}

std::vector<std::uint8_t> EntryStubBuilder::BuildEntryStub(
    const std::uint64_t stubVaddr,
    const std::uint64_t realEntryVaddr
) const {
    std::vector<std::uint8_t> s;
    s.push_back(kStubOpPopRax);
    _appendBytes(s, kStubOpMovRbxRsp, sizeof(kStubOpMovRbxRsp));
    _appendBytes(s, kStubOpSubRsp0x30, sizeof(kStubOpSubRsp0x30));
    _appendBytes(s, kStubOpAndRsp0xf0, sizeof(kStubOpAndRsp0xf0));
    _appendBytes(s, kStubOpMovDwordPtrRsp, sizeof(kStubOpMovDwordPtrRsp));
    _appendBytes(s, kStubOpMovQwordPtrRsp8Rbx, sizeof(kStubOpMovQwordPtrRsp8Rbx));
    _appendBytes(s, kStubOpMovRdiRsp, sizeof(kStubOpMovRdiRsp));
    _appendBytes(s, kStubOpXorRsiRsi, sizeof(kStubOpXorRsiRsi));
    const std::uint64_t jmpInsnVaddr = stubVaddr + s.size();
    const std::uint64_t jmpNextVaddr = jmpInsnVaddr + kStubJmpInstructionSize;
    const auto rel32 = static_cast<std::int32_t>(realEntryVaddr - jmpNextVaddr);
    s.push_back(kStubOpJmpRel32);
    s.push_back(static_cast<std::uint8_t>(rel32 & 0xff));
    s.push_back(static_cast<std::uint8_t>((rel32 >> 8) & 0xff));
    s.push_back(static_cast<std::uint8_t>((rel32 >> 16) & 0xff));
    s.push_back(static_cast<std::uint8_t>((rel32 >> 24) & 0xff));
    return s;
}

}
