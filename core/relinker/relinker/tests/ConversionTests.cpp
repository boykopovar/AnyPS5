#include "TestProgress.hpp"
#include <relinker/pipeline/Conversion.hpp>
#include <relinker/guest/GuestImage.hpp>
#include <relinker/analysis/SyscallScanner.hpp>
#include <relinker/analysis/CodeInstructionCollector.hpp>
#include <io/BufferUtils.hpp>
#include <io/NativePath.hpp>
#include <codegen/IAmd64OnlyConverter.hpp>
#include <elfpatcher/windows/WindowsRelocationBuilder.hpp>
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename TAction>
void reject(TAction action) {
    try { action(); } catch (const Domain::RelinkerException&) { return; }
    throw std::runtime_error("Invalid conversion was accepted");
}

std::vector<std::uint8_t> fixture(const std::string& needed = {}) {
    std::vector<std::uint8_t> bytes(0x3000);
    const std::array<std::uint8_t, 7> magic{0x7f, 'E', 'L', 'F', 2, 1, 1};
    std::copy(magic.begin(), magic.end(), bytes.begin());
    Io::WriteU16(bytes, 16, 3);
    Io::WriteU16(bytes, 18, 62);
    Io::WriteU32(bytes, 20, 1);
    Io::WriteU64(bytes, 24, 0x1000);
    Io::WriteU64(bytes, 32, 64);
    Io::WriteU16(bytes, 52, 64);
    Io::WriteU16(bytes, 54, 56);
    Io::WriteU16(bytes, 56, 8);
    Io::WriteU16(bytes, 58, 64);
    const auto header = [&](std::size_t index, unsigned type, unsigned flags, unsigned offset, unsigned size) {
        const auto at = 64 + index * 56;
        Io::WriteU32(bytes, at, type);
        Io::WriteU32(bytes, at + 4, flags);
        Io::WriteU64(bytes, at + 8, offset);
        Io::WriteU64(bytes, at + 16, offset);
        Io::WriteU64(bytes, at + 24, offset);
        Io::WriteU64(bytes, at + 32, size);
        Io::WriteU64(bytes, at + 40, size);
        Io::WriteU64(bytes, at + 48, 8);
    };
    header(0, 1, 5, 0x1000, 8);
    header(1, 1, 6, 0x2000, 0x1000);
    for (std::size_t index = 3; index < 8; ++index) header(index, 0x6fffff01, 0, 0, 0);
    bytes[0x1000] = 0xc3;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> tags{{5, 0x2600}, {10, needed.empty() ? 1 : needed.size() + 2}, {6, 0x2620}, {11, 24}, {7, 0x2700}, {8, 0}, {9, 24}, {4, 0x2680}};
    if (!needed.empty()) {
        std::copy(needed.begin(), needed.end(), bytes.begin() + 0x2601);
        tags.push_back({1, 1});
    }
    tags.push_back({0, 0});
    header(2, 2, 6, 0x2000, tags.size() * 16);
    for (std::size_t index = 0; index < tags.size(); ++index) {
        Io::WriteU64(bytes, 0x2000 + index * 16, tags[index].first);
        Io::WriteU64(bytes, 0x2008 + index * 16, tags[index].second);
    }
    Io::WriteU32(bytes, 0x2680, 1);
    Io::WriteU32(bytes, 0x2684, 1);
    return bytes;
}

Relinker::ConversionInput input() {
    Relinker::ConversionInput result;
    result.Executable = fixture();
    result.InputPath = "virtual/title/custom-name.bin";
    result.OutputPath = "virtual/output/title";
    result.ModuleRoot = "virtual/title";
    return result;
}

void convert() {
    TestProgress progress;
    const Relinker::Conversion converter(progress);
    for (const auto target : {Relinker::TargetSystem::Windows, Relinker::TargetSystem::Linux}) {
        Relinker::ConversionOptions options;
        options.Target = target;
        options.ToIntel = true;
        auto source = input();
        source.Modules.push_back({"virtual/title/sce_module/plugin.prx", fixture()});
        const auto output = converter.Convert(std::move(source), options);
        require(output.Executable.size() > 64, "No executable returned");
        require(output.Executable.front() == (target == Relinker::TargetSystem::Windows ? 'M' : 0x7f), "Wrong target format");
        require(output.Modules.size() == 1, "Module was not converted");
        require(output.Modules.front().Path.generic_string() == "virtual/output/app0/sce_module/plugin.prx.guest.prx", "Wrong module layout");
        require(output.Modules.front().Bytes.front() == output.Executable.front(), "Wrong module format");
        require(progress.Events.back().Stage == Domain::ProgressStage::Executable && progress.Events.back().Completed == 1, "Conversion did not finish");
        for (const auto& event : progress.Events) require(event.Completed <= event.Total, "Invalid counters");
        if (target == Relinker::TargetSystem::Windows) {
            std::uint64_t previous = 0;
            bool intermediate = false;
            for (const auto& event : progress.Events) {
                if (event.Stage != Domain::ProgressStage::Image) continue;
                require(event.Input == "virtual/title/custom-name.bin", "Lost image input identity");
                require(event.Total == 10000 && event.Completed >= previous, "Image progress moved backwards");
                intermediate |= event.Completed > 0 && event.Completed < event.Total;
                previous = event.Completed;
            }
            require(intermediate && previous == 10000, "Missing image progress");
        }
        progress.Events.clear();
    }
    auto source = input();
    source.Modules.push_back({"virtual/escaped.prx", fixture()});
    reject([&] { converter.Convert(source, {}); });
    source = input();
    source.Modules.push_back({"virtual/title/sce_module/plugin.prx", fixture()});
    source.Modules.push_back(source.Modules.front());
    reject([&] { converter.Convert(source, {}); });
    source = input();
    source.Icon = std::vector<std::uint8_t>{};
    Relinker::ConversionOptions windows;
    windows.Target = Relinker::TargetSystem::Windows;
    reject([&] { converter.Convert(source, windows); });
    source = input();
    source.Executable = {};
    reject([&] { converter.Convert(source, {}); });
    require(progress.Events.back().Completed != 1 || progress.Events.back().Stage != Domain::ProgressStage::Executable, "Failed conversion reported completion");
}

void runtimeLifecyclePolicy() {
    TestProgress progress;
    const Relinker::Conversion converter(progress);
    for (const auto* directory : {"sce_module", "sce_modules", "prx", "Media/Plugins"}) {
        auto source = input();
        const auto parent = source.ModuleRoot / directory;
        auto module = fixture();
        Io::WriteU64(module, 0x2080, 12);
        Io::WriteU64(module, 0x2088, 0x1000);
        Io::WriteU64(module, 208, 160);
        Io::WriteU64(module, 216, 160);
        source.Modules.push_back({parent / "plugin.prx", std::move(module)});
        source.RuntimeDirectories.push_back(parent);
        const auto output = converter.Convert(std::move(source), {});
        const auto& bytes = output.Modules.at(0).Bytes;
        const auto headers = Io::ReadU64(bytes, 32);
        bool initializesOnMapping = false;
        for (std::uint16_t index = 0; index < Io::ReadU16(bytes, 56); ++index) {
            const auto header = headers + index * 56;
            if (Io::ReadU32(bytes, header) != 2) continue;
            const auto dynamic = Io::ReadU64(bytes, header + 8);
            const auto end = dynamic + Io::ReadU64(bytes, header + 32);
            for (auto offset = dynamic; offset < end; offset += 16)
                if (Io::ReadU64(bytes, offset) == 12) initializesOnMapping = Io::ReadU64(bytes, offset + 8) != 0;
        }
        require(initializesOnMapping == (std::string_view(directory) != "Media/Plugins"), "Default SDK directories lost their mapping initializer");
    }
}

void fixedBase() {
    TestProgress progress;
    const Relinker::Conversion converter(progress);
    auto source = input();
    source.Modules.push_back({"virtual/title/sce_module/plugin.prx", fixture()});
    const auto standard = converter.Convert(source, {});
    Relinker::ConversionOptions lazy;
    lazy.LazyBinding = true;
    const auto independent = converter.Convert(input(), lazy);
    Relinker::ConversionOptions options;
    options.LinuxFixedBase = 0x400000000;
    const auto fixed = converter.Convert(source, options);
    require(Io::ReadU16(standard.Executable, 16) == 2 && Io::ReadU16(fixed.Executable, 16) == 2, "Fixed base did not select ET_EXEC");
    require(Io::ReadU16(independent.Executable, 16) == 3, "Lazy binding did not keep ET_DYN");
    require(standard.LinuxBase == 0x400000 && fixed.LinuxBase == *options.LinuxFixedBase && !independent.LinuxBase, "Conversion reported the wrong Linux base");
    require(Io::ReadU64(fixed.Executable, 24) == Io::ReadU64(standard.Executable, 24) + *options.LinuxFixedBase - 0x400000, "Fixed entry was not rebased above 4 GiB");
    require(fixed.Modules.front().Bytes == standard.Modules.front().Bytes, "Executable fixed base changed a bundled module");
    for (const auto target : {Relinker::TargetSystem::Windows, Relinker::TargetSystem::MacOs}) {
        Relinker::ConversionOptions other;
        other.Target = target;
        require(!converter.Convert(source, other).LinuxBase, "A non-Linux conversion reported a Linux base");
        options.Target = target;
        reject([&] { converter.Convert(source, options); });
    }
}

void scanning() {
    TestProgress progress;
    const auto scanner = Relinker::MakeSyscallScanner(progress);
    std::vector<std::uint8_t> code(150000, 0x90);
    require(scanner->ScanCodeSectionForSyscalls(code, 0, code.size(), "sample.prx").empty(), "Unexpected syscall trap");
    require(std::any_of(progress.Events.begin(), progress.Events.end(), [](const auto& event) { return event.Completed > 0 && event.Completed < event.Total; }), "Only start and finish were reported");
    for (const auto& event : progress.Events) require(event.Input == "sample.prx", "Lost scanner input identity");
    progress.Events.clear();
    Domain::ProgramHeader header{};
    header.Type = 1;
    header.Flags = 1;
    header.FileSize = code.size();
    header.MemorySize = code.size();
    const auto converter = Codegen::MakeAmd64OnlyConverter(Codegen::Amd64OnlyTarget::Intel, [&](auto completed, auto total) { progress.Report({Domain::ProgressStage::Lowering, "sample.prx", completed, total}); });
    require(converter->Convert(code, {header}).Bytes == code, "Progress changed instruction conversion");
    std::uint64_t previous = 0;
    for (const auto& event : progress.Events) {
        require(event.Completed >= previous && event.Completed <= event.Total, "Instruction conversion progress moved backwards");
        previous = event.Completed;
    }
    require(previous == progress.Events.back().Total, "Instruction conversion progress did not finish");
    reject([&] { scanner->ScanCodeSectionForSyscalls(code, 0, (std::uint64_t{1} << 32) + code.size()); });
    auto image = code;
    image.resize(code.size() + 64, 0x90);
    image.back() = 0xc3;
    Io::WriteU64(image, 24, 64);
    header.Offset = 64;
    header.MappedAddress = 64;
    previous = 0;
    std::size_t updates = 0;
    const auto instructions = Relinker::CodeInstructionCollector().Collect(image, {header}, [&](auto completed, auto total) {
        require(completed >= previous && completed <= total, "Image analysis progress moved backwards");
        previous = completed;
        ++updates;
    });
    require(instructions.size() == code.size(), "Progress changed image code analysis");
    require(updates > 10 && previous == 10000, "Missing intermediate image analysis progress");
    code[70000] = 0x0f;
    code[70001] = 0x05;
    progress.Events.clear();
    reject([&] { scanner->ScanCodeSectionForSyscalls(code, 0, code.size()); });
    require(progress.Events.back().Completed < progress.Events.back().Total, "Failed scan reported completion");
}

class FailingProgress final : public Domain::IProgress {
public:
    std::string InputPath;
    void Report(const Domain::ProgressEvent&) override {
        Domain::RelinkerException error("diagnostic context", 0x1234);
        error.InputPath = InputPath;
        throw error;
    }
    void Message(std::string_view) override {}
};

void diagnosticContext() {
    for (const bool specific : {false, true}) {
        FailingProgress progress;
        if (specific) progress.InputPath = "virtual/title/sce_module/plugin.prx";
        const auto source = input();
        try { Relinker::Conversion(progress).Convert(source, {}); }
        catch (const Domain::RelinkerException& error) {
            require(error.InputPath == (specific ? progress.InputPath : Io::Utf8Path(source.InputPath)), "Wrong diagnostic input path");
            require(error.FailureOffset == 0x1234, "Diagnostic offset changed");
            require(std::string(error.what()) == "diagnostic context", "Diagnostic message changed");
            continue;
        }
        throw std::runtime_error("Diagnostic failure was swallowed");
    }
}

class CancellingProgress final : public Domain::IProgress {
public:
    Domain::ProgressStage Stage = Domain::ProgressStage::Decode;
    void Report(const Domain::ProgressEvent& event) override {
        if (event.Stage == Stage && event.Completed > 0) throw std::runtime_error("cancelled");
    }
    void Message(std::string_view) override {}
};

void cancellation() {
    for (const auto stage : {Domain::ProgressStage::Decode, Domain::ProgressStage::Image}) {
        CancellingProgress progress;
        progress.Stage = stage;
        Relinker::ConversionOptions options;
        options.Target = Relinker::TargetSystem::Windows;
        try { Relinker::Conversion(progress).Convert(input(), options); }
        catch (const std::runtime_error& error) {
            require(std::string(error.what()) == "cancelled", "Callback exception was replaced");
            continue;
        }
        throw std::runtime_error("Callback exception was swallowed");
    }
}

void baseRelocations() {
    Elfpatcher::Windows::WindowsRelocationBuilder builder;
    const auto relocs = builder.BuildBaseRelocations({0x10000, 0x10000, 0x10008});
    require(relocs.size() == 12, "Base relocation duplicates were not deduplicated");
    require(Io::ReadU32(relocs, 0) == 0x10000, "Wrong base relocation page RVA");
    require(Io::ReadU32(relocs, 4) == 12, "Wrong base relocation block size");
    require(Io::ReadU16(relocs, 8) == 0xa000, "Wrong first relocation entry");
    require(Io::ReadU16(relocs, 10) == 0xa008, "Wrong second relocation entry");
    const auto spanning = builder.BuildBaseRelocations({0x10ffc, 0x10fff});
    require(spanning.size() == 12 && Io::ReadU32(spanning, 0) == 0x10000, "A page-spanning base relocation was not placed in its first page");
    require(Io::ReadU16(spanning, 8) == 0xaffc && Io::ReadU16(spanning, 10) == 0xafff, "Wrong page-spanning base relocation entries");
}

std::size_t fileOffset(const std::vector<std::uint8_t>& pe, std::uint32_t address) {
    const auto header = Io::ReadU32(pe, 0x3c);
    const auto sections = header + 24 + Io::ReadU16(pe, header + 20);
    for (std::size_t index = 0; index < Io::ReadU16(pe, header + 6); ++index) {
        const auto at = sections + index * 40;
        const auto start = Io::ReadU32(pe, at + 12);
        if (address >= start && address - start < Io::ReadU32(pe, at + 16)) return Io::ReadU32(pe, at + 20) + address - start;
    }
    throw std::runtime_error("Unmapped PE address");
}

bool exports(const std::vector<std::uint8_t>& pe, std::string_view name) {
    const auto address = Io::ReadU32(pe, Io::ReadU32(pe, 0x3c) + 24 + 112);
    if (address == 0) return false;
    const auto directory = fileOffset(pe, address);
    const auto names = fileOffset(pe, Io::ReadU32(pe, directory + 32));
    for (std::size_t index = 0; index < Io::ReadU32(pe, directory + 24); ++index) {
        if (reinterpret_cast<const char*>(pe.data() + fileOffset(pe, Io::ReadU32(pe, names + index * 4))) == name) return true;
    }
    return false;
}

void runtimeDirectories() {
    TestProgress progress;
    const Relinker::Conversion converter(progress);
    Relinker::ConversionOptions options;
    options.Target = Relinker::TargetSystem::Windows;
    for (const auto* directory : {"sce_module", "sce_modules", "prx", "Media/Modules"}) {
        auto source = input();
        const auto parent = source.ModuleRoot / directory;
        source.Executable = fixture("needed.prx");
        source.Modules.push_back({parent / "needed.prx", fixture()});
        source.RuntimeDirectories.push_back(parent);
        const auto output = converter.Convert(std::move(source), options);
        require(output.Modules.size() == 1, "Module was not converted");
        require(exports(output.Modules.front().Bytes, Relinker::GuestInitializeExport) == (std::string_view(directory) == "Media/Modules"), "Wrong start of a runtime module the executable needs");
    }
}

void relocationValidation() {
    TestProgress progress;
    const Relinker::Conversion converter(progress);
    {
        auto source = input();
        Io::WriteU64(source.Executable, 0x2058, 24);
        Io::WriteU64(source.Executable, 0x2700, 0x2000);
        Io::WriteU64(source.Executable, 0x2708, (static_cast<std::uint64_t>(0x9999) << 32) | 0x99);
        Io::WriteU64(source.Executable, 0x2710, 0);
        try {
            converter.Convert(source, {});
            throw std::runtime_error("Unsupported relocation type was accepted");
        } catch (const Domain::RelinkerException& error) {
            require(std::string(error.what()).find("Unsupported relocation type 0x99") != std::string::npos, "Wrong error for unsupported relocation");
            require(error.FailureOffset == 0x2700, "Wrong failure offset for unsupported relocation");
        }
    }
    {
        auto source = input();
        const std::vector<std::pair<std::uint64_t, std::uint64_t>> tags{
            {5, 0x2600}, {10, 1}, {6, 0x2620}, {11, 24}, {7, 0x2700}, {8, 0}, {9, 24}, {4, 0x2680},
            {3, 0x2000}, {20, 7}, {23, 0x2780}, {2, 24}, {0, 0}
        };
        const auto at = 64 + 2 * 56;
        Io::WriteU64(source.Executable, at + 32, tags.size() * 16);
        Io::WriteU64(source.Executable, at + 40, tags.size() * 16);
        for (std::size_t index = 0; index < tags.size(); ++index) {
            Io::WriteU64(source.Executable, 0x2000 + index * 16, tags[index].first);
            Io::WriteU64(source.Executable, 0x2008 + index * 16, tags[index].second);
        }
        Io::WriteU64(source.Executable, 0x2780, 0x2000);
        Io::WriteU64(source.Executable, 0x2788, (static_cast<std::uint64_t>(1) << 32) | 8);
        Io::WriteU64(source.Executable, 0x2790, 0);
        try {
            converter.Convert(source, {});
            throw std::runtime_error("Unsupported PLT relocation type was accepted");
        } catch (const Domain::RelinkerException& error) {
            require(std::string(error.what()) == "Unsupported PLT relocation type", "Wrong error for unsupported PLT relocation");
            require(error.FailureOffset == 0x2780, "Wrong failure offset for unsupported PLT relocation");
        }
    }
}

}

int main() {
    try {
        convert();
        runtimeLifecyclePolicy();
        fixedBase();
        scanning();
        diagnosticContext();
        cancellation();
        baseRelocations();
        runtimeDirectories();
        relocationValidation();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
