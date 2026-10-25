#include <elfpatcher/windows/WindowsElfPatcher.hpp>
#include <elfpatcher/windows/WindowsEntryStubBuilder.hpp>
#include <elfpatcher/windows/WindowsIconResourceBuilder.hpp>
#include <elfpatcher/windows/WindowsLoadImage.hpp>
#include <elfpatcher/windows/WindowsPeWriter.hpp>
#include <elfpatcher/windows/WindowsRelocationBuilder.hpp>
#include <elfpatcher/windows/WindowsTlsBuilder.hpp>
#include <elfpatcher/windows/WindowsTrampolineBuilder.hpp>
#include <io/BufferUtils.hpp>
#include <filesystem>


#include <utility>

namespace Elfpatcher::Windows {

namespace {

void writeGotStub(std::vector<PeSection>& sections, const std::uint32_t targetRva, const std::uint32_t stubRva, const std::uint64_t imageBase) {
    for (auto& section : sections) {
        if (!section.Writable(targetRva, 8))
            continue;
        Io::WriteU64(section.Data, targetRva - section.Rva, imageBase + stubRva);
        return;
    }
    throw Domain::RelinkerException("Lazy import GOT slot is not contained in any section", targetRva);
}

}

WindowsPePatcher::WindowsPePatcher(const bool windowsGui, std::optional<std::vector<std::uint8_t>> icon, std::filesystem::path iconName, std::function<void(std::uint64_t, std::uint64_t)> progress, const std::optional<std::uint64_t> fixedBase, const bool traceImports, std::function<void(std::string_view)> message) : progress(std::move(progress)), windowsGui(windowsGui), icon(std::move(icon)), iconName(std::move(iconName)), fixedBase(fixedBase), traceImports(traceImports), message(std::move(message)) {
    if (fixedBase && (*fixedBase < 0x20000 || *fixedBase > 0x7fffffff0000 || (*fixedBase & 0xffff) != 0))
        throw Domain::RelinkerException("Invalid fixed Windows guest image base");
}

const std::string& WindowsPePatcher::GetDiagnostics() const { return diagnostics; }

std::vector<std::uint8_t> WindowsPePatcher::Patch(std::vector<std::uint8_t> sourceElf, const std::vector<Domain::ProgramHeader>& originalHeaders, const Domain::SysVDynamicSection& dynamicSection, const std::uint64_t originalPltGotVaddr, const std::string& runPath, const bool lazyBinding, const bool dependencyDiagnostics, const std::vector<Codegen::TrampolineSite>& trampolines) {
    const auto report = [&](std::uint64_t completed) { if (progress) progress(completed, 10000); };
    report(0);
    diagnostics.clear();
    WindowsLoadImage image(sourceElf, originalHeaders, true, fixedBase.value_or(ExecutableBase) - LoadRva);
    if (originalPltGotVaddr != 0)
        image.GetRva(originalPltGotVaddr, 8);
    report(500);
    const WindowsRelocationBuilder relocationBuilder;
    auto relocations = relocationBuilder.Apply(image, dynamicSection, originalHeaders);
    report(1000);
    auto sections = image.BuildSections();
    report(1500);
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
    directories[9] = WindowsTlsBuilder().Build(sourceElf, originalHeaders, image, sections, relocations.BaseRelocations, nextRva, nullptr, [&](auto completed, auto total) { report(1500 + 6500 * completed / total); }, message);
    report(8000);
    WindowsTrampolineBuilder().Build(trampolines, image, sections, nextRva);
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
        for (const auto& import : module.Imports) {
            relocations.Imports.push_back({import.Name, import.TargetRva, import.Addend, static_cast<std::int32_t>(index), import.RelocationType, import.Library});
            relocations.Imports.back().Function = import.Function;
            relocations.Imports.back().Weak = import.Weak;
        }
    }
    libraries.insert(libraries.begin(), guestPaths.begin(), guestPaths.end());
    if (dependencyDiagnostics)
        for (const auto& import : relocations.Imports) diagnostics += import.Name + '\n';
    report(8500);
    auto entry = WindowsEntryStubBuilder().Build(nextRva, image.GetEntryRva(), nativeImports, libraries, relocations.Imports, runPath, lazyBinding, dependencyDiagnostics, dynamicSection.GuestModules, dynamicSection.UnmatchedImportModules, traceImports);
    report(9000);
    directories[3] = entry.ExceptionDirectory;
    const auto entryRva = entry.Code.Rva;
    nextRva = AlignRva(entry.Code.Rva + entry.Code.Data.size());
    sections.push_back(std::move(nativeImports.Section));
    sections.push_back(std::move(entry.Data));
    sections.push_back(std::move(entry.Code));
    for (const auto& lazyStub : entry.LazyStubs) {
        writeGotStub(sections, lazyStub.TargetRva, lazyStub.StubRva, image.GetImageBase());
        relocations.BaseRelocations.push_back(lazyStub.TargetRva);
    }
    auto relocationData = fixedBase ? std::vector<std::uint8_t>{} : relocationBuilder.BuildBaseRelocations(relocations.BaseRelocations);
    if (!relocationData.empty()) {
        directories[5] = {nextRva, CheckedRva(relocationData.size())};
        sections.push_back({".reloc", nextRva, SectionRead | 0x02000040u, std::move(relocationData)});
        nextRva = AlignRva(nextRva + sections.back().Data.size());
    }
    directories[2] = icon ? WindowsIconResourceBuilder().Build(*icon, iconName, sections, nextRva) : PeDirectory{};
    report(9500);
    auto result = WindowsPeWriter().Write(sections, entryRva, directories, windowsGui, image.GetImageBase(), fixedBase.has_value(), false);
    report(10000);
    return result;
}

}
