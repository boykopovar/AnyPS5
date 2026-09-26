#include <elfpatcher/windows/WindowsTlsTemplateBuilder.hpp>
#include <algorithm>

namespace Elfpatcher::Windows {

std::vector<std::uint8_t> WindowsTlsTemplateBuilder::Build(const Domain::ProgramHeader& tls, const WindowsLoadImage& image, const std::vector<PeSection>& sections, const std::uint32_t templateRva, std::vector<std::uint32_t>& relocations) const {
    if (tls.FileSize == 0) return {};
    const auto sourceRva = image.GetRva(tls.MappedAddress, tls.FileSize);
    const auto sourceEnd = static_cast<std::uint64_t>(sourceRva) + tls.FileSize;
    std::vector<std::uint8_t> result;
    result.reserve(static_cast<std::size_t>(tls.FileSize));
    while (result.size() < tls.FileSize) {
        const auto currentRva = static_cast<std::uint64_t>(sourceRva) + result.size();
        const auto section = std::find_if(sections.begin(), sections.end(), [&](const auto& candidate) {
            return currentRva >= candidate.Rva && currentRva - candidate.Rva < candidate.Data.size();
        });
        if (section == sections.end()) throw Domain::RelinkerException("TLS template is outside PE section data", currentRva);
        const auto offset = static_cast<std::size_t>(currentRva - section->Rva);
        const auto size = std::min<std::uint64_t>(tls.FileSize - result.size(), section->Data.size() - offset);
        result.insert(result.end(), section->Data.begin() + offset, section->Data.begin() + offset + size);
    }
    const auto originalCount = relocations.size();
    for (std::size_t index = 0; index < originalCount; ++index) {
        const auto target = static_cast<std::uint64_t>(relocations[index]);
        if (target >= sourceEnd || target + 8 <= sourceRva) continue;
        if (target < sourceRva || sourceEnd - target < 8) throw Domain::RelinkerException("PE relocation crosses the TLS template boundary", target);
        relocations.push_back(CheckedRva(static_cast<std::uint64_t>(templateRva) + target - sourceRva));
    }
    return result;
}

}
