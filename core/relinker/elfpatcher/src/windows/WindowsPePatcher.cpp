#include <elfpatcher/windows/WindowsElfPatcher.hpp>
#include <elfpatcher/windows/WindowsEntryStubBuilder.hpp>
#include <elfpatcher/windows/WindowsLoadImage.hpp>
#include <elfpatcher/windows/WindowsPeWriter.hpp>
#include <elfpatcher/windows/WindowsRelocationBuilder.hpp>
#include <elfpatcher/windows/WindowsTlsBuilder.hpp>
#include <io/BufferUtils.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <utility>

namespace Elfpatcher::Windows {

namespace {

void writeDiagnosticsImports(const std::vector<PeImport>& imports) {
    const std::filesystem::path path = std::filesystem::absolute("windows-diagnostics-imports.txt");
    std::ofstream stream(path, std::ios::trunc);
    if (!stream)
        throw Domain::RelinkerException("Cannot open windows-diagnostics-imports.txt for writing");
    for (const auto& import : imports)
        stream << import.Name << '\n';
    if (!stream)
        throw Domain::RelinkerException("Cannot write windows-diagnostics-imports.txt");
    stream.close();
    std::cout << "Wrote Windows import diagnostics to " << path.string() << '\n';
}

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

std::vector<std::uint8_t> WindowsPePatcher::Patch(const std::vector<std::uint8_t>& sourceElf, const std::vector<Domain::ProgramHeader>& originalHeaders, const Domain::SysVDynamicSection& dynamicSection, const std::uint64_t originalPltGotVaddr, const std::string& runPath, const bool lazyBinding, const bool dependencyDiagnostics) {
    WindowsLoadImage image(sourceElf, originalHeaders);
    if (originalPltGotVaddr != 0)
        image.GetRva(originalPltGotVaddr, 8);
    const WindowsRelocationBuilder relocationBuilder;
    auto relocations = relocationBuilder.Apply(image, dynamicSection);
    auto sections = image.BuildSections();
    std::array<PeDirectory, 16> directories{};
    auto nextRva = image.GetEndRva();
    bool hasProcessParameters = false;
    for (const auto& header : originalHeaders) {
        if (header.Type != 0x61000001) continue;
        if (hasProcessParameters || header.FileSize < 0x40) throw Domain::RelinkerException("Invalid process parameter segment");
        hasProcessParameters = true;
        std::vector<std::uint8_t> metadata(8);
        Io::WriteU32(metadata, 0, image.GetRva(header.MappedAddress, header.FileSize));
        Io::WriteU32(metadata, 4, CheckedRva(header.FileSize));
        sections.push_back({".procpar", nextRva, SectionRead | 0x40u, std::move(metadata)});
        nextRva = AlignRva(nextRva + sections.back().Data.size());
    }
    for (const auto& header : originalHeaders) {
        if (header.Type != 0x6474e550) continue;
        std::vector<std::uint8_t> metadata(4);
        Io::WriteU32(metadata, 0, image.GetRva(header.MappedAddress, header.FileSize));
        sections.push_back({".ehmeta", nextRva, SectionRead | 0x40u, std::move(metadata)});
        nextRva = AlignRva(nextRva + sections.back().Data.size());
    }
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
    auto libraries = importBuilder.ReadLibraries(dynamicSection);
    std::vector<std::string> guestPaths;
    for (std::size_t index = 0; index < dynamicSection.GuestModules.size(); ++index) {
        const auto& module = dynamicSection.GuestModules[index];
        guestPaths.push_back(module.Path);
        for (const auto& import : module.Imports) relocations.Imports.push_back({import.Name, import.TargetRva, import.Addend, static_cast<std::int32_t>(index), import.RelocationType});
    }
    libraries.insert(libraries.begin(), guestPaths.begin(), guestPaths.end());
    if (dependencyDiagnostics)
        writeDiagnosticsImports(relocations.Imports);
    auto entry = WindowsEntryStubBuilder().Build(nextRva, image.GetEntryRva(), nativeImports, libraries, relocations.Imports, runPath, lazyBinding, dependencyDiagnostics, dynamicSection.GuestModules);
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
