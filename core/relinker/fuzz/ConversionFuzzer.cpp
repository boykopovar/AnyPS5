#include <relinker/pipeline/Conversion.hpp>
#include <io/NativePath.hpp>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <string_view>

namespace {

class SilentProgress final : public Domain::IProgress {
public:
    void Report(const Domain::ProgressEvent&) override {}
    void Message(std::string_view) override {}
};

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size < 1) return 0;
    const std::uint8_t selector = data[0];
    Relinker::ConversionInput input;
    input.InputPath = Io::NativePath("/input/eboot.bin");
    input.OutputPath = Io::NativePath("/output/eboot");
    input.ModuleRoot = "/input";
    input.Executable.assign(data + 1, data + size);
    Relinker::ConversionOptions options;
    switch (selector % 3) {
    case 0: options.Target = Relinker::TargetSystem::Linux; break;
    case 1: options.Target = Relinker::TargetSystem::Windows; break;
    default: options.Target = Relinker::TargetSystem::MacOs; break;
    }
    options.ToIntel = (selector & 0x04) != 0;
    options.UnusedFilterLevel = (selector >> 3) % 3;
    SilentProgress progress;
    try {
        Relinker::Conversion(progress).Convert(std::move(input), options);
    } catch (const std::exception&) {
    }
    return 0;
}
