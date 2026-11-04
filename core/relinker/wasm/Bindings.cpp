#include <relinker/pipeline/Conversion.hpp>
#include <io/NativePath.hpp>
#include <emscripten/bind.h>
#include <stdexcept>

namespace {

class BrowserProgress final : public Domain::IProgress {
public:
    explicit BrowserProgress(emscripten::val callback) : callback(std::move(callback)) {}

    void Report(const Domain::ProgressEvent& event) override {
        auto report = emscripten::val::object();
        report.set("stage", static_cast<unsigned>(event.Stage));
        report.set("input", event.Input);
        report.set("completed", static_cast<double>(event.Completed));
        report.set("total", static_cast<double>(event.Total));
        callback(report);
    }

    void Message(std::string_view message) override {
        auto report = emscripten::val::object();
        report.set("message", std::string(message));
        callback(report);
    }

private:
    emscripten::val callback;
};

std::vector<std::uint8_t> ReadBytes(const emscripten::val& bytes, std::size_t& remaining) {
    if (!bytes.instanceof(emscripten::val::global("Uint8Array"))) throw std::invalid_argument("Expected Uint8Array input");
    const auto length = bytes["length"].as<std::size_t>();
    if (length > remaining) throw std::invalid_argument("Executable, modules and icon exceed the 256 MiB conversion limit");
    remaining -= length;
    std::vector<std::uint8_t> result(length);
    if (length != 0) emscripten::val(emscripten::typed_memory_view(length, result.data())).call<void>("set", bytes);
    return result;
}

emscripten::val CopyBytes(const std::vector<std::uint8_t>& bytes) {
    return emscripten::val::global("Uint8Array").new_(emscripten::val(emscripten::typed_memory_view(bytes.size(), bytes.data())));
}

emscripten::val Convert(const emscripten::val& request, const emscripten::val& callback) {
    auto result = emscripten::val::object();
    try {
        const auto target = request["target"].as<std::string>();
        if (target != "windows" && target != "linux") throw std::invalid_argument("Unknown target system");
        const auto name = request["name"].as<std::string>();
        if (name.empty() || name.find_first_of("/\\") != std::string::npos || name == "." || name == "..") throw std::invalid_argument("Invalid executable name");
        std::size_t remaining = 256 * 1024 * 1024;
        Relinker::ConversionInput input;
        input.InputPath = Io::NativePath("/input/" + name);
        input.OutputPath = Io::NativePath("/output/" + request["outputName"].as<std::string>());
        if (input.OutputPath.parent_path() != "/output") throw std::invalid_argument("Invalid output name");
        input.ModuleRoot = "/input";
        input.Executable = ReadBytes(request["executable"], remaining);
        const auto modules = request["modules"];
        const auto count = modules["length"].as<unsigned>();
        if (count > 4096) throw std::invalid_argument("Too many modules");
        for (unsigned index = 0; index < count; ++index) {
            const auto module = modules[index];
            const auto path = Io::NativePath(module["name"].as<std::string>());
            if (path.is_absolute() || path.lexically_normal() != path || path.begin() == path.end() ||
                (*path.begin() != "sce_module" && *path.begin() != "sce_modules" && *path.begin() != "prx")) throw std::invalid_argument("Invalid module path");
            input.Modules.push_back({Io::NativePath("/input") / path, ReadBytes(module["bytes"], remaining)});
        }
        if (!request["icon"].isNull()) input.Icon = ReadBytes(request["icon"], remaining);
        Relinker::ConversionOptions options;
        options.Target = target == "windows" ? Relinker::TargetSystem::Windows : Relinker::TargetSystem::Linux;
        options.WindowsGui = false;
        options.WindowsDiagnostics = target == "windows";
        options.ToIntel = request["toIntel"].as<bool>();
        BrowserProgress progress(callback);
        auto output = Relinker::Conversion(progress).Convert(std::move(input), options);
        result.set("executable", CopyBytes(output.Executable));
        result.set("diagnostics", output.WindowsDiagnostics);
        auto artifacts = emscripten::val::array();
        for (const auto& module : output.Modules) {
            auto artifact = emscripten::val::object();
            artifact.set("name", Io::Utf8Path(module.Path.lexically_relative("/output")));
            artifact.set("bytes", CopyBytes(module.Bytes));
            artifacts.call<void>("push", artifact);
        }
        result.set("modules", artifacts);
        result.set("ok", true);
    } catch (const std::exception& error) {
        result.set("ok", false);
        result.set("error", std::string(error.what()));
    }
    return result;
}

}

EMSCRIPTEN_BINDINGS(relinker) {
    emscripten::function("Convert", &Convert);
}
