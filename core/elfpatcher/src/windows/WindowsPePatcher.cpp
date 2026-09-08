#include <elfpatcher/windows/WindowsElfPatcher.hpp>
#include "WindowsEntryStubBuilder.hpp"
#include "WindowsLoadImage.hpp"
#include "WindowsPeWriter.hpp"
#include "WindowsRelocationBuilder.hpp"
#include <utility>

namespace Elfpatcher::Windows {

std::vector<std::uint8_t> WindowsPePatcher::Patch(const std::vector<std::uint8_t>& sourceElf, const std::vector<Domain::ProgramHeader>& originalHeaders, const Domain::SysVDynamicSection& dynamicSection, const std::uint64_t originalPltGotVaddr, const std::string& runPath) {
    WindowsLoadImage image(sourceElf, originalHeaders);
    if (originalPltGotVaddr != 0)
        image.GetRva(originalPltGotVaddr, 8);
    const WindowsRelocationBuilder relocationBuilder;
    const auto relocations = relocationBuilder.Apply(image, dynamicSection);
    auto sections = image.BuildSections();
    std::array<PeDirectory, 16> directories{};
    auto nextRva = image.GetEndRva();
    auto relocationData = relocationBuilder.BuildBaseRelocations(relocations.BaseRelocations);
    if (!relocationData.empty()) {
        directories[5] = {nextRva, CheckedRva(relocationData.size())};
        sections.push_back({".reloc", nextRva, SectionRead | 0x02000040u, std::move(relocationData)});
        nextRva = AlignRva(nextRva + sections.back().Data.size());
    }
    const WindowsImportBuilder importBuilder;
    auto nativeImports = importBuilder.Build(nextRva);
    directories[1] = nativeImports.Directory;
    directories[12] = nativeImports.AddressTable;
    nextRva = AlignRva(nextRva + nativeImports.Section.Data.size());
    const auto libraries = importBuilder.ReadLibraries(dynamicSection);
    auto entry = WindowsEntryStubBuilder().Build(nextRva, image.GetEntryRva(), nativeImports, libraries, relocations.Imports, runPath);
    directories[3] = entry.ExceptionDirectory;
    const auto entryRva = entry.Code.Rva;
    sections.push_back(std::move(nativeImports.Section));
    sections.push_back(std::move(entry.Data));
    sections.push_back(std::move(entry.Code));
    return WindowsPeWriter().Write(sections, entryRva, directories);
}

}
