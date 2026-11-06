#include <codegen/x86/StubBodyBuilder.hpp>
#include <span>
#include <elfpatcher/general/TrampolineWriter.hpp>
#include <elfpatcher/general/AddressDistance.hpp>
#include <codegen/x86/Amd64OnlySubstitutionTable.hpp>
#include <domain/Types.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <limits>

namespace Elfpatcher {

void AppendTrampoline(std::vector<std::uint8_t>& bytes, const Codegen::TrampolineSite& site, const std::uint64_t imageEnd, const std::function<std::uint64_t(std::uint64_t)>& addressOfOffset) {
    using namespace Codegen::Amd64OnlySubstitutionTable;
    if (site.Length < kJmpRel32.Size || site.OriginalBytes.size() != site.Length || site.Body.size() < kJmpRel32.Size || site.ReturnBranchOffset > site.Body.size() - kJmpRel32.Size || site.Body[site.ReturnBranchOffset] != kJmpRel32.Bytes[0])
        throw Domain::RelinkerException("Invalid AMD-only trampoline site", site.Offset);
    if (site.Offset > imageEnd || site.Length > imageEnd - site.Offset)
        throw Domain::RelinkerException("AMD-only instruction is outside the original image", site.Offset);
    if (!std::equal(site.OriginalBytes.begin(), site.OriginalBytes.end(), bytes.begin() + static_cast<std::ptrdiff_t>(site.Offset)))
        throw Domain::RelinkerException("AMD-only site bytes changed before patching", site.Offset);

    bytes.resize(Io::AlignUp(bytes.size(), kStubAlignment), kTrapFill);
    const auto bodyOffset = static_cast<std::uint64_t>(bytes.size());
    const auto bodyAddress = addressOfOffset(bodyOffset);
    bytes.insert(bytes.end(), site.Body.begin(), site.Body.end());

    const auto inRange = [](const std::int64_t displacement) {
        return displacement >= std::numeric_limits<std::int32_t>::min() && displacement <= std::numeric_limits<std::int32_t>::max();
    };
    std::int64_t returnDisplacement = 0, jumpDisplacement = 0;
    if (!SignedDistance(site.Address + site.Length, bodyAddress + site.ReturnBranchOffset + kJmpRel32.Size, returnDisplacement) || !SignedDistance(bodyAddress, site.Address + kJmpRel32.Size, jumpDisplacement) || !inRange(returnDisplacement) || !inRange(jumpDisplacement))
        throw Domain::RelinkerException("AMD-only stub exceeds rel32 range", site.Offset);
    Io::WriteU32(bytes, static_cast<std::size_t>(bodyOffset + site.ReturnBranchOffset + 1), static_cast<std::uint32_t>(returnDisplacement));

    Codegen::ApplyStubRelocations(std::span<std::uint8_t>(bytes.data() + bodyOffset, site.ReturnBranchOffset), site.Relocations, site.Address, bodyAddress, site.Offset);

    for (const auto& fixup : site.Fixups) {
        if (fixup.BodyOffset + 4 > site.Body.size())
            throw Domain::RelinkerException("Relocated operand is outside the stub body", site.Offset);
        const auto operandAddress = bodyAddress + fixup.BodyOffset + 4;
        std::int64_t relocated = 0;
        if (!SignedDistance(fixup.Target, operandAddress, relocated) || !inRange(relocated))
            throw Domain::RelinkerException("Relocated operand exceeds rel32 range", site.Offset);
        Io::WriteU32(bytes, static_cast<std::size_t>(bodyOffset + fixup.BodyOffset), static_cast<std::uint32_t>(relocated));
    }

    for (const auto& branch : site.Incoming) {
        if (branch.DisplacementOffset + 4 > bytes.size())
            throw Domain::RelinkerException("Incoming branch is outside the image", branch.DisplacementOffset);
        std::int64_t entry = 0;
        if (!SignedDistance(bodyAddress + branch.BodyOffset, branch.BranchEnd, entry) || !inRange(entry))
            throw Domain::RelinkerException("Incoming branch cannot reach the stub", branch.DisplacementOffset);
        Io::WriteU32(bytes, static_cast<std::size_t>(branch.DisplacementOffset), static_cast<std::uint32_t>(entry));
    }

    std::fill_n(bytes.begin() + static_cast<std::ptrdiff_t>(site.Offset), site.Length, kNop1.Bytes[0]);
    bytes[static_cast<std::size_t>(site.Offset)] = kJmpRel32.Bytes[0];
    Io::WriteU32(bytes, static_cast<std::size_t>(site.Offset + 1), static_cast<std::uint32_t>(jumpDisplacement));
}

}
