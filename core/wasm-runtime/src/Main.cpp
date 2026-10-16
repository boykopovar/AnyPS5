#include "Program.hpp"
#include "wasm-runtime/Guest.hpp"
#include <emscripten.h>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

namespace {

EM_JS(double, aps5_game_metadata_size, (), {
    const sections = WebAssembly.Module.customSections(globalThis.aps5GameModule, 'anyps5.images');
    if (sections.length !== 1) throw new Error('the game module has no anyps5.images section');
    return sections[0].byteLength;
});

EM_JS(void, aps5_game_metadata_copy, (void* destination), {
    const bytes = new Uint8Array(WebAssembly.Module.customSections(globalThis.aps5GameModule, 'anyps5.images')[0]);
    new Uint8Array(wasmMemory.buffer).set(bytes, Number(destination));
});

struct ExportCall {
    std::string nid;
    std::vector<WasmRuntime::GuestValue> arguments;
};

struct Options {
    std::vector<ExportCall> calls;
    std::vector<std::string> guestArguments;
};

Options ParseOptions(int argc, char** argv) {
    Options options;
    for (int index = 2; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--call") {
            if (++index >= argc) throw std::runtime_error("--call needs an export NID");
            options.calls.push_back({argv[index], {}});
        } else if (argument == "--") {
            for (++index; index < argc; ++index) options.guestArguments.emplace_back(argv[index]);
        } else if (!options.calls.empty()) {
            options.calls.back().arguments.push_back({WasmRuntime::ValueKind::Integer, static_cast<std::uint64_t>(std::stoll(argument, nullptr, 0))});
        } else {
            throw std::runtime_error("unexpected argument " + argument);
        }
    }
    return options;
}

std::uint64_t PushBytes(std::uint64_t& stack, const void* bytes, std::size_t length) {
    stack -= length;
    std::memcpy(reinterpret_cast<void*>(static_cast<std::uintptr_t>(stack)), bytes, length);
    return stack;
}

std::uint64_t BuildEntryStack(const std::vector<std::string>& arguments) {
    std::uint64_t stack = WasmRuntime::CurrentCpu().gpr[WasmRuntime::Rsp] - 256;
    std::vector<std::uint64_t> argumentPointers;
    for (const auto& argument : arguments) argumentPointers.push_back(PushBytes(stack, argument.c_str(), argument.size() + 1));
    stack &= ~std::uint64_t{15};
    std::vector<std::uint64_t> words;
    words.push_back(argumentPointers.size());
    words.insert(words.end(), argumentPointers.begin(), argumentPointers.end());
    words.push_back(0);
    words.push_back(0);
    words.push_back(0);
    words.push_back(0);
    if (words.size() % 2 != 0) words.push_back(0);
    return PushBytes(stack, words.data(), words.size() * sizeof(std::uint64_t));
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto options = ParseOptions(argc, argv);
        std::vector<std::uint8_t> metadata(static_cast<std::size_t>(aps5_game_metadata_size()));
        aps5_game_metadata_copy(metadata.data());
        auto& program = WasmRuntime::LoadProgram(std::move(metadata));
        WasmRuntime::InitializeImages(program);
        if (!options.calls.empty()) {
            for (const auto& call : options.calls) {
                const auto result = WasmRuntime::CallGuest(WasmRuntime::ExportAddress(program, 0, call.nid), call.arguments);
                std::printf("%s = %" PRId64 "\n", call.nid.c_str(), static_cast<std::int64_t>(result.rax));
            }
            std::fflush(stdout);
            return 0;
        }
        const auto& image = program.metadata.images.front();
        if (!image.entry) throw std::runtime_error(image.name + " has no entry point");
        std::vector<std::string> arguments{"/app0/eboot.bin"};
        arguments.insert(arguments.end(), options.guestArguments.begin(), options.guestArguments.end());
        WasmRuntime::RunGuestEntry(program.bases.front() + *image.entry, BuildEntryStack(arguments));
    } catch (const std::exception& error) {
        std::fprintf(stderr, "anyps5 runtime: %s\n", error.what());
        return 1;
    }
}
