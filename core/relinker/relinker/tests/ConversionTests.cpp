#include "TestProgress.hpp"
#include <relinker/pipeline/Conversion.hpp>
#include <relinker/analysis/SyscallScanner.hpp>
#include <relinker/analysis/CodeInstructionCollector.hpp>
#include <io/BufferUtils.hpp>
#include <io/NativePath.hpp>
#include <codegen/IAmd64OnlyConverter.hpp>
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename TAction>
void reject(TAction action) {
    try { action(); } catch (const Domain::RelinkerException&) { return; }
    throw std::runtime_error("Invalid conversion was accepted");
}

std::vector<std::uint8_t> fixture() {
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
    const std::array<std::pair<std::uint64_t, std::uint64_t>, 9> tags{{{5, 0x2600}, {10, 1}, {6, 0x2620}, {11, 24}, {7, 0x2700}, {8, 0}, {9, 24}, {4, 0x2680}, {0, 0}}};
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

void fixedBase() {
    TestProgress progress;
    const Relinker::Conversion converter(progress);
    auto source = input();
    source.Modules.push_back({"virtual/title/sce_module/plugin.prx", fixture()});
    const auto plain = converter.Convert(source, {});
    Relinker::ConversionOptions options;
    options.LinuxFixedBase = 0x400000000;
    const auto fixed = converter.Convert(source, options);
    require(Io::ReadU16(plain.Executable, 16) == 3 && Io::ReadU16(fixed.Executable, 16) == 2, "Fixed base did not select ET_EXEC");
    require(Io::ReadU64(fixed.Executable, 24) == Io::ReadU64(plain.Executable, 24) + *options.LinuxFixedBase, "Fixed entry was not rebased above 4 GiB");
    require(fixed.Modules.front().Bytes == plain.Modules.front().Bytes, "Executable fixed base changed a bundled module");
    for (const auto target : {Relinker::TargetSystem::Windows, Relinker::TargetSystem::MacOs}) {
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
    std::vector<std::uint8_t> wrappingImage(0x1000);
    Io::WriteU64(wrappingImage, 24, 0x2000);
    wrappingImage[0x200] = 0xc3;
    wrappingImage[0x208] = 0xc3;
    Io::WriteU64(wrappingImage, 0x3c0, 0x2008);
    Io::WriteU64(wrappingImage, 0x700, 0x2008);
    Domain::ProgramHeader execHeader{};
    execHeader.Type = 1;
    execHeader.Flags = 1;
    execHeader.Offset = 0x200;
    execHeader.MappedAddress = 0x2000;
    execHeader.FileSize = 0x100;
    execHeader.MemorySize = 0x100;
    Domain::ProgramHeader dynamicHeader{};
    dynamicHeader.Type = 2;
    dynamicHeader.Flags = 6;
    dynamicHeader.Offset = 0x100;
    dynamicHeader.MappedAddress = 0x1000;
    dynamicHeader.FileSize = 48;
    dynamicHeader.MemorySize = 48;
    Domain::ProgramHeader wrappingDataHeader{};
    wrappingDataHeader.Type = 1;
    wrappingDataHeader.Flags = 0;
    wrappingDataHeader.Offset = ~std::uint64_t{0} - 63;
    wrappingDataHeader.MappedAddress = 0x8000;
    wrappingDataHeader.FileSize = 0x410;
    wrappingDataHeader.MemorySize = 0x410;
    auto controlDataHeader = wrappingDataHeader;
    controlDataHeader.Offset = 0x300;
    for (const auto [addrTag, sizeTag] : {std::pair{25u, 27u}, std::pair{26u, 28u}, std::pair{32u, 33u}}) {
        Io::WriteU64(wrappingImage, 0x100, addrTag);
        Io::WriteU64(wrappingImage, 0x108, 0x8400);
        Io::WriteU64(wrappingImage, 0x110, sizeTag);
        Io::WriteU64(wrappingImage, 0x118, 8);
        Io::WriteU64(wrappingImage, 0x120, 0);
        Io::WriteU64(wrappingImage, 0x128, 0);
        reject([&] { Relinker::CodeInstructionCollector().Collect(wrappingImage, {execHeader, dynamicHeader, wrappingDataHeader}); });
        const auto roots = Relinker::CodeInstructionCollector().Collect(wrappingImage, {execHeader, dynamicHeader, controlDataHeader});
        require(roots == std::set<Domain::VirtualAddress>{0x2000, 0x2008}, "Valid initializer array roots were not collected");
        Io::WriteU64(wrappingImage, 0x3c0, 0);
        reject([&] { Relinker::CodeInstructionCollector().Collect(wrappingImage, {execHeader, dynamicHeader, wrappingDataHeader}); });
        Io::WriteU64(wrappingImage, 0x3c0, 0x2008);
    }
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

}

int main() {
    try {
        convert();
        fixedBase();
        scanning();
        diagnosticContext();
        cancellation();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
