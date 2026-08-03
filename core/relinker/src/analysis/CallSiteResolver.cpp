#include <relinker/analysis/CallSiteResolver.hpp>
#include <memory>

namespace Relinker {

class CallSiteResolver : public ICallSiteResolver {
public:
    std::vector<FileByteOffset> ResolveCallSites(
        const std::vector<std::uint8_t>& textSection,
        FileByteOffset textSectionVAddr,
        VirtualAddress targetGotOrPltAddress,
        ByteCount targetGotOrPltSize) override;
};

std::vector<FileByteOffset> CallSiteResolver::ResolveCallSites(
    const std::vector<std::uint8_t>& textSection,
    const FileByteOffset textSectionVAddr,
    const VirtualAddress targetGotOrPltAddress,
    const ByteCount targetGotOrPltSize)
{
    std::vector<FileByteOffset> sites;

    if (textSection.size() < 6)
        return sites;

    const std::size_t limit = textSection.size() - 5;

    for (std::size_t i = 0; i <= limit; ++i) {
        std::uint8_t b0 = textSection[i];
        std::uint8_t b1 = textSection[i + 1];

        bool isRipRelative = false;
        std::size_t dispOffset = 0;
        std::size_t instrSize = 0;

        if (b0 == 0xFF && (b1 == 0x15 || b1 == 0x25)) {
            isRipRelative = true;
            dispOffset = i + 2;
            instrSize = 6;
        } else if (b0 == 0x48 && b1 == 0x8B && i + 6 < textSection.size()) {
            std::uint8_t modrm = textSection[i + 2];
            if ((modrm & 0xC7) == 0x05) {
                isRipRelative = true;
                dispOffset = i + 3;
                instrSize = 7;
            }
        }

        if (!isRipRelative)
            continue;

        if (dispOffset + 3 >= textSection.size())
            continue;

        const auto disp =
            static_cast<std::int32_t>(
                static_cast<std::uint32_t>(textSection[dispOffset]) |
                (static_cast<std::uint32_t>(textSection[dispOffset + 1]) << 8) |
                (static_cast<std::uint32_t>(textSection[dispOffset + 2]) << 16) |
                (static_cast<std::uint32_t>(textSection[dispOffset + 3]) << 24));

        const VirtualAddress nextInstrVAddr = textSectionVAddr + i + instrSize;
        auto resolvedAddr = static_cast<VirtualAddress>(static_cast<std::int64_t>(nextInstrVAddr) + disp);

        if (resolvedAddr >= targetGotOrPltAddress &&
            resolvedAddr < targetGotOrPltAddress + targetGotOrPltSize)
        {
            sites.push_back(textSectionVAddr + i);
        }
    }

    return sites;
}

std::shared_ptr<ICallSiteResolver> MakeCallSiteResolver() {
    return std::make_shared<CallSiteResolver>();
}

}
