#include <elfpatcher/general/EntryStubBuilder.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <domain/Types.hpp>
#include <limits>

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
    _appendBytes(s, kStubOpMovRdiRsp, sizeof(kStubOpMovRdiRsp));
    _appendBytes(s, kStubOpAndRsp0xf0, sizeof(kStubOpAndRsp0xf0));
    _appendBytes(s, kStubOpPushZero, sizeof(kStubOpPushZero));
    _appendBytes(s, kStubOpPushZero, sizeof(kStubOpPushZero));
    _appendBytes(s, kStubOpMovRbpRsp, sizeof(kStubOpMovRbpRsp));
    _appendBytes(s, kStubOpXorRsiRsi, sizeof(kStubOpXorRsiRsi));
    const std::uint64_t callInsnVaddr = stubVaddr + s.size();
    const std::uint64_t callNextVaddr = callInsnVaddr + kStubCallInstructionSize;
    const auto displacement = static_cast<std::int64_t>(realEntryVaddr - callNextVaddr);
    if (displacement < std::numeric_limits<std::int32_t>::min() || displacement > std::numeric_limits<std::int32_t>::max())
        throw Domain::RelinkerException("Entry stub call exceeds rel32 range");
    const auto rel32 = static_cast<std::int32_t>(displacement);
    s.push_back(kStubOpCallRel32);
    s.push_back(static_cast<std::uint8_t>(rel32 & 0xff));
    s.push_back(static_cast<std::uint8_t>((rel32 >> 8) & 0xff));
    s.push_back(static_cast<std::uint8_t>((rel32 >> 16) & 0xff));
    s.push_back(static_cast<std::uint8_t>((rel32 >> 24) & 0xff));
    _appendBytes(s, kStubOpUd2, sizeof(kStubOpUd2));
    return s;
}

}
