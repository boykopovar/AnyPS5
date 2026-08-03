#include <relinker/ICallSiteResolver.hpp>
#include <memory>

namespace Relinker {

class CallSiteResolver : public ICallSiteResolver {
public:
    std::vector<Offset> ResolveCallSites(
        const std::vector<std::uint8_t>& TextSection,
        Offset TextSectionVAddr,
        Address TargetGotOrPltAddress,
        Size TargetGotOrPltSize) override;
};

std::vector<Offset> CallSiteResolver::ResolveCallSites(
    const std::vector<std::uint8_t>& TextSection,
    Offset TextSectionVAddr,
    Address TargetGotOrPltAddress,
    Size TargetGotOrPltSize)
{
    std::vector<Offset> sites;

    if (TextSection.size() < 6)
        return sites;

    const std::size_t limit = TextSection.size() - 5;

    for (std::size_t i = 0; i <= limit; ++i) {
        std::uint8_t b0 = TextSection[i];
        std::uint8_t b1 = TextSection[i + 1];

        bool isRipRelative = false;
        std::size_t dispOffset = 0;
        std::size_t instrSize = 0;

        if (b0 == 0xFF && (b1 == 0x15 || b1 == 0x25)) {
            isRipRelative = true;
            dispOffset = i + 2;
            instrSize = 6;
        } else if (b0 == 0x48 && b1 == 0x8B && i + 6 < TextSection.size()) {
            std::uint8_t modrm = TextSection[i + 2];
            if ((modrm & 0xC7) == 0x05) {
                isRipRelative = true;
                dispOffset = i + 3;
                instrSize = 7;
            }
        }

        if (!isRipRelative)
            continue;

        if (dispOffset + 3 >= TextSection.size())
            continue;

        std::int32_t disp =
            static_cast<std::int32_t>(
                static_cast<std::uint32_t>(TextSection[dispOffset]) |
                (static_cast<std::uint32_t>(TextSection[dispOffset + 1]) << 8) |
                (static_cast<std::uint32_t>(TextSection[dispOffset + 2]) << 16) |
                (static_cast<std::uint32_t>(TextSection[dispOffset + 3]) << 24));

        Address nextInstrVAddr = TextSectionVAddr + i + instrSize;
        Address resolvedAddr = static_cast<Address>(static_cast<std::int64_t>(nextInstrVAddr) + disp);

        if (resolvedAddr >= TargetGotOrPltAddress &&
            resolvedAddr < TargetGotOrPltAddress + TargetGotOrPltSize)
        {
            sites.push_back(TextSectionVAddr + i);
        }
    }

    return sites;
}

std::shared_ptr<ICallSiteResolver> MakeCallSiteResolver() {
    return std::make_shared<CallSiteResolver>();
}

}
