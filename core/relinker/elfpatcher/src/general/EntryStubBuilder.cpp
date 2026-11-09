#include <elfpatcher/general/EntryStubBuilder.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <elfpatcher/general/AddressDistance.hpp>
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
    const std::uint64_t realEntryVaddr,
    const std::optional<std::uint64_t> mainStackEntrySlotVaddr
) const {
    std::vector<std::uint8_t> s;
    const auto appendRel32 = [&](const std::uint64_t target) {
        const std::uint64_t nextVaddr = stubVaddr + s.size() + sizeof(std::int32_t);
        std::int64_t displacement = 0;
        if (!SignedDistance(target, nextVaddr, displacement) || displacement < std::numeric_limits<std::int32_t>::min() || displacement > std::numeric_limits<std::int32_t>::max())
            throw Domain::RelinkerException("Entry stub call exceeds rel32 range");
        const auto rel32 = static_cast<std::uint32_t>(static_cast<std::int32_t>(displacement));
        for (std::size_t shift = 0; shift < 32; shift += 8)
            s.push_back(static_cast<std::uint8_t>((rel32 >> shift) & 0xff));
    };
    _appendBytes(s, kStubOpMovRdiRsp, sizeof(kStubOpMovRdiRsp));
    _appendBytes(s, kStubOpAndRsp0xf0, sizeof(kStubOpAndRsp0xf0));
    _appendBytes(s, kStubOpPushZero, sizeof(kStubOpPushZero));
    _appendBytes(s, kStubOpPushZero, sizeof(kStubOpPushZero));
    _appendBytes(s, kStubOpMovRbpRsp, sizeof(kStubOpMovRbpRsp));
    if (mainStackEntrySlotVaddr) {
        _appendBytes(s, kStubOpLeaRsiRip, sizeof(kStubOpLeaRsiRip));
        appendRel32(realEntryVaddr);
        _appendBytes(s, kStubOpCallRipIndirect, sizeof(kStubOpCallRipIndirect));
        appendRel32(*mainStackEntrySlotVaddr);
    } else {
        _appendBytes(s, kStubOpXorRsiRsi, sizeof(kStubOpXorRsiRsi));
        s.push_back(kStubOpCallRel32);
        appendRel32(realEntryVaddr);
    }
    _appendBytes(s, kStubOpUd2, sizeof(kStubOpUd2));
    return s;
}

}
