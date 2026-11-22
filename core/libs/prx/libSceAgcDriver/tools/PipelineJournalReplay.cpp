#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineJournal.hpp"
#include "ShaderDiskCache.hpp"
#include "RuntimeAbi.hpp"
#include <charconv>
#include <cstdlib>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <syncstream>

namespace {

struct Header {
    std::uint32_t magic, format, abi, reserved;
    std::uint64_t source, device;
};
static_assert(sizeof(Header) == 32);

unsigned Bound(const char* value) {
    unsigned result{};
    const std::string_view text(value);
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || result > 4096) throw std::runtime_error("bound must be 0 to 4096");
    return result;
}

std::string Quote(std::string_view text) {
    std::ostringstream result;
    result << '"';
    for (const unsigned char character : text) {
        if (character == '\\' || character == '"') result << '\\' << character;
        else if (character < 0x20u) result << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<unsigned>(character);
        else result << character;
    }
    result << '"';
    return result.str();
}

PFN_vkGetPhysicalDeviceProperties PhysicalPropertiesQuery() {
#ifdef _WIN32
    const auto loader = LoadLibraryW(L"vulkan-1.dll");
    return loader ? reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(GetProcAddress(loader, "vkGetPhysicalDeviceProperties")) : nullptr;
#else
    const auto loader = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    return loader ? reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(dlsym(loader, "vkGetPhysicalDeviceProperties")) : nullptr;
#endif
}

void Environment(const char* name, const char* value) {
#ifdef _WIN32
    if (_putenv_s(name, value) != 0) throw std::runtime_error("could not set environment");
#else
    if (setenv(name, value, 1) != 0) throw std::runtime_error("could not set environment");
#endif
}

}

int main(int argc, char** argv) {
    try {
        if (argc != 6) throw std::runtime_error("usage: agc_pipeline_journal_replay JOURNAL NEW_CACHE_ROOT GRAPHICS COMPUTE THREADS");
        const auto graphics = Bound(argv[3]);
        const auto compute = Bound(argv[4]);
        const auto threads = Bound(argv[5]);
        if (threads == 0 || threads > 32) throw std::runtime_error("threads must be 1 to 32");
        const std::filesystem::path source(argv[1]), root(argv[2]);
        if (std::filesystem::exists(root)) throw std::runtime_error("private cache root already exists");
        if (std::filesystem::file_size(source) > 1024ull * 1024ull * 1024ull) throw std::runtime_error("journal exceeds 1 GiB input limit");
        Header header{};
        std::ifstream input(source, std::ios::binary);
        input.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (!input || header.magic != 0x4c4a5041u || header.abi != ShaderRecompiler::RuntimeAbi::Version) throw std::runtime_error("journal header/runtime ABI mismatch");
        if (header.source != ShaderRecompiler::ShaderDiskCache::SourceVersion()) throw std::runtime_error("journal source version mismatch");
        if (!std::filesystem::create_directories(root)) throw std::runtime_error("private cache root already exists");
#ifdef _WIN32
        if (_wputenv_s(L"ANYPS5_SHADER_CACHE_DIR", root.wstring().c_str()) != 0) throw std::runtime_error("could not set private shader cache directory");
#else
        Environment("ANYPS5_SHADER_CACHE_DIR", root.string().c_str());
#endif
        Environment("ANYPS5_NO_SHADER_CACHE", "0");
        Environment("APS5_NO_PIPELINE_JOURNAL", "0");
        Environment("APS5_PIPELINE_JOURNAL_ENTRIES", argv[3]);
        Environment("APS5_PIPELINE_JOURNAL_COMPUTE_ENTRIES", argv[4]);
        Environment("APS5_PIPELINE_JOURNAL_THREADS", argv[5]);
        const auto properties = PhysicalPropertiesQuery();
        const auto begin = std::chrono::steady_clock::now();
        auto device = std::make_unique<AgcDriver::VulkanDevice>();
        if (!properties) throw std::runtime_error("physical properties query unavailable");
        VkPhysicalDeviceProperties physical{};
        properties(device->graphicsContext().physical, &physical);
        if (header.device != AgcDriver::Graphics::PipelineJournalDeviceFingerprint(physical)) throw std::runtime_error("journal device fingerprint mismatch");
        const auto directory = ShaderRecompiler::ShaderDiskCache::EntryDirectory();
        if (directory.empty()) throw std::runtime_error("private cache disabled");
        std::filesystem::create_directories(directory);
        std::filesystem::copy_file(source, directory / "pipelines.journal");
        {
            auto replay = AgcDriver::Graphics::StartPipelineJournalReplay(device->graphicsContext(), header.device,
                {[&](const ShaderRecompiler::RecompileResult& shader, bool push) {
                    const auto hash = shader.spirv.ContentHash();
                    const auto identity = shader.Identity();
                    const auto write = [&](const char* event, double elapsed, const char* status) {
                        std::osyncstream line(std::cerr);
                        line << "{\"event\":" << Quote(event) << ",\"identity_low\":" << identity.low
                             << ",\"identity_high\":" << identity.high << ",\"hash_low\":" << hash.low
                             << ",\"hash_high\":" << hash.high << ",\"words\":" << shader.spirv.size()
                             << ",\"push_constants\":" << (push ? "true" : "false")
                             << ",\"elapsed_ms\":" << elapsed << ",\"status\":" << Quote(status) << "}\n";
                        line.emit();
                        std::cerr.flush();
                    };
                    const auto started = std::chrono::steady_clock::now();
                    write("compute_begin", 0, "started");
                    try {
                        const auto built = device->ReplayComputePipeline(shader, push);
                        write("compute_end", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count(), built ? "built" : "present");
                        return built;
                    } catch (...) {
                        write("compute_end", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count(), "failed");
                        throw;
                    }
                }});
            if (!replay) throw std::runtime_error("journal replay unavailable");
            replay->Wait();
        }
        const auto totals = AgcDriver::Graphics::PipelineJournalTotals();
        const double wall = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
        const bool useful = totals.replayedGraphics + totals.replayedCompute > 0;
        const bool okay = useful && totals.replayedGraphics <= graphics && totals.replayedCompute <= compute && totals.replayRejected == 0 && totals.replayFailed == 0 && totals.droppedRecords == 0;
        std::cout << "{\"ok\":" << (okay ? "true" : "false") << ",\"device\":" << Quote(device->DeviceName()) << ",\"driver\":" << Quote(device->DriverInfo())
                  << ",\"loaded_recipes\":" << totals.loadedRecipes << ",\"loaded_modules\":" << totals.loadedModules
                  << ",\"graphics\":" << totals.replayedGraphics << ",\"compute\":" << totals.replayedCompute
                  << ",\"present\":" << totals.replayPresent << ",\"skipped\":" << totals.replaySkipped
                  << ",\"rejected\":" << totals.replayRejected << ",\"failed\":" << totals.replayFailed
                  << ",\"dropped\":" << totals.droppedRecords << ",\"load_ms\":" << totals.loadMs
                  << ",\"replay_ms\":" << totals.replayMs << ",\"device_and_replay_ms\":" << wall << "}\n";
        if (!okay) std::cerr << "replay failed or produced no useful pipelines\n";
        return okay ? 0 : 2;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
