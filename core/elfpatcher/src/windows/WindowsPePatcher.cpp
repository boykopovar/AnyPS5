#include <elfpatcher/windows/WindowsElfPatcher.hpp>
#include "WindowsEntryStubBuilder.hpp"
#include "WindowsLoadImage.hpp"
#include "WindowsPeWriter.hpp"
#include "WindowsRelocationBuilder.hpp"
#include "WindowsTlsBuilder.hpp"
#include <io/BufferUtils.hpp>
#include <utility>

namespace Elfpatcher::Windows {

namespace {

void writeGotStub(std::vector<PeSection>& sections, const std::uint32_t targetRva, const std::uint32_t stubRva) {
    for (auto& section : sections) {
        if (targetRva < section.Rva || targetRva - section.Rva > section.Data.size() - 4)
            continue;
        Io::WriteU32(section.Data, targetRva - section.Rva, stubRva);
        return;
    }
    throw Domain::RelinkerException("Lazy import GOT slot is not contained in any section", targetRva);
}

}

std::vector<std::uint8_t> WindowsPePatcher::Patch(const std::vector<std::uint8_t>& sourceElf, const std::vector<Domain::ProgramHeader>& originalHeaders, const Domain::SysVDynamicSection& dynamicSection, const std::uint64_t originalPltGotVaddr, const std::string& runPath, const bool lazyBinding) {
    WindowsLoadImage image(sourceElf, originalHeaders);
    if (originalPltGotVaddr != 0)
        image.GetRva(originalPltGotVaddr, 8);
    const WindowsRelocationBuilder relocationBuilder;
    auto relocations = relocationBuilder.Apply(image, dynamicSection);
    auto sections = image.BuildSections();
    std::array<PeDirectory, 16> directories{};
    auto nextRva = image.GetEndRva();
    directories[9] = WindowsTlsBuilder().Build(sourceElf, originalHeaders, image, sections, relocations.BaseRelocations, nextRva);
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
    auto entry = WindowsEntryStubBuilder().Build(nextRva, image.GetEntryRva(), nativeImports, libraries, relocations.Imports, runPath, lazyBinding);
    directories[3] = entry.ExceptionDirectory;
    const auto entryRva = entry.Code.Rva;
    sections.push_back(std::move(nativeImports.Section));
    sections.push_back(std::move(entry.Data));
    sections.push_back(std::move(entry.Code));
    for (const auto& lazyStub : entry.LazyStubs)
        writeGotStub(sections, lazyStub.TargetRva, lazyStub.StubRva);
    return WindowsPeWriter().Write(sections, entryRva, directories);
}

}
