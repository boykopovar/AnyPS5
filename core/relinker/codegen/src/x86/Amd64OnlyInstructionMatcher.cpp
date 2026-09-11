#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <codegen/x86/DecodedInstruction.hpp>
#include <codegen/x86/Amd64OnlySubstitutionTable.hpp>
#include <memory>

namespace Codegen {

namespace {

std::vector<std::uint8_t> _bytesOf(const Amd64OnlySubstitutionTable::Entry& e) {
    return std::vector<std::uint8_t>(e.Bytes, e.Bytes + e.Size);
}

class Amd64OnlyInstructionMatcher : public IAmd64OnlyInstructionMatcher {
public:
    [[nodiscard]] std::optional<Amd64OnlyMatch> Match(
        const std::uint8_t* data,
        std::size_t length
    ) const override;
};

std::optional<Amd64OnlyMatch> Amd64OnlyInstructionMatcher::Match(
    const std::uint8_t* data,
    std::size_t length
) const {
    using namespace Amd64OnlySubstitutionTable;
    const DecodedInstruction instr{data, length};

    if (instr.IsMonitorx()) {
        return Amd64OnlyMatch{kMonitorx.Name, length, _bytesOf(kMonitorx)};
    }

    if (instr.IsMwaitx()) {
        return Amd64OnlyMatch{kMwaitx.Name, length, _bytesOf(kMwaitx)};
    }

    if (instr.IsClzero()) {
        return Amd64OnlyMatch{kClzero.Name, length, _bytesOf(kClzero)};
    }

    if (instr.IsRdpru()) {
        return Amd64OnlyMatch{kRdpru.Name, length, _bytesOf(kRdpru)};
    }

    if (instr.IsMcommit()) {
        return Amd64OnlyMatch{kMcommit.Name, length, _bytesOf(kMcommit)};
    }

    return std::nullopt;
}

}

std::unique_ptr<IAmd64OnlyInstructionMatcher> MakeAmd64OnlyInstructionMatcher() {
    return std::make_unique<Amd64OnlyInstructionMatcher>();
}

}
