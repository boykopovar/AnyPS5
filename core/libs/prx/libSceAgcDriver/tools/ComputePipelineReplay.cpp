#include "ComputePipelineFixture.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace {

struct Options {
    std::filesystem::path source;
    std::filesystem::path cacheIn;
    std::filesystem::path cacheOut;
    std::filesystem::path output;
    std::filesystem::path fixtureDirectory;
    bool fixture = false;
    bool inspect = false;
};

Options Parse(int argc, char** argv) {
    Options result;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        const auto value = [&](std::filesystem::path& destination) {
            if (!destination.empty() || ++index == argc || *argv[index] == '\0') throw std::runtime_error("missing or repeated option: " + std::string(argument));
            destination = argv[index];
        };
        if (argument == "--cache-in") value(result.cacheIn);
        else if (argument == "--cache-out") value(result.cacheOut);
        else if (argument == "--output") value(result.output);
        else if (argument == "--write-fixture") value(result.fixtureDirectory);
        else if (argument == "--fixture" && !result.fixture) result.fixture = true;
        else if (argument == "--inspect" && !result.inspect) result.inspect = true;
        else if (argument.starts_with("--") || !result.source.empty()) throw std::runtime_error("unknown or repeated argument: " + std::string(argument));
        else result.source = argument;
    }
    const auto sources = static_cast<int>(!result.source.empty()) + static_cast<int>(result.fixture) + static_cast<int>(!result.fixtureDirectory.empty());
    if (sources != 1) throw std::runtime_error("select a capture file, --fixture, or --write-fixture DIRECTORY");
    if (!result.fixtureDirectory.empty() && (!result.cacheIn.empty() || !result.cacheOut.empty() || !result.output.empty() || result.inspect)) throw std::runtime_error("--write-fixture cannot be combined with replay options");
    for (const auto& path : {result.cacheOut, result.output}) {
        if (!path.empty() && std::filesystem::exists(path)) throw std::runtime_error("output already exists: " + path.string());
    }
    if (!result.cacheOut.empty() && !result.output.empty() && std::filesystem::absolute(result.cacheOut).lexically_normal() == std::filesystem::absolute(result.output).lexically_normal()) throw std::runtime_error("cache and report must use different paths");
    return result;
}

std::vector<std::byte> ReadCache(const std::filesystem::path& path) {
    if (path.empty()) return {};
    const auto bytes = std::filesystem::file_size(path);
    if (bytes == 0 || bytes > 64u * 1024u * 1024u) throw std::runtime_error("imported pipeline cache must contain 1 byte to 64 MiB");
    std::vector<std::byte> result(static_cast<std::size_t>(bytes));
    std::ifstream input(path, std::ios::binary);
    input.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(result.size()));
    if (!input || input.peek() != std::char_traits<char>::eof()) throw std::runtime_error("could not read complete pipeline cache: " + path.string());
    return result;
}

void WriteBytes(const std::filesystem::path& path, std::span<const std::byte> bytes) {
    if (std::filesystem::exists(path)) throw std::runtime_error("output already exists: " + path.string());
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    output.close();
    if (!output) throw std::runtime_error("could not write complete output: " + path.string());
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

std::string Uuid(std::span<const std::uint8_t> bytes) {
    std::ostringstream result;
    for (const auto byte : bytes) result << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(byte);
    return result.str();
}

template<typename TDevice>
void WriteDevice(std::ostream& output, const TDevice& device) {
    output << "{\"name\":" << Quote(device.deviceName)
           << ",\"vendor_id\":" << device.vendorId << ",\"device_id\":" << device.deviceId
           << ",\"driver_version_raw\":" << device.driverVersion << ",\"api_version_raw\":" << device.apiVersion
           << ",\"pipeline_cache_uuid\":" << Quote(Uuid(device.pipelineCacheUuid)) << '}';
}

std::string Report(const Options& options, const AgcDriver::ComputePipelineCapture& capture, const AgcDriver::ComputePipelineReplayResult& replay) {
    std::ostringstream output;
    output << std::setprecision(12)
           << "{\n  \"schema_version\":1,\n  \"source\":" << Quote(options.fixture ? "synthetic-no-op-compute" : options.source.string())
           << ",\n  \"capture_identity\":" << Quote(capture.Identity())
           << ",\n  \"capture_identity_algorithm\":\"fnv1a64\""
           << ",\n  \"spirv_words\":" << capture.spirv.size()
           << ",\n  \"descriptor_sets\":" << capture.sets.size()
           << ",\n  \"push_constant_ranges\":" << capture.pushConstants.size()
           << ",\n  \"specialization_constants\":" << capture.specialization.size()
           << ",\n  \"required_subgroup_size\":" << capture.requiredSubgroupSize
           << ",\n  \"pipeline_flags\":" << capture.flags
           << ",\n  \"replay_pipeline_flags\":" << (capture.flags | (options.inspect ? static_cast<VkPipelineCreateFlags>(VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR) : 0u))
           << ",\n  \"stage_flags\":" << capture.stageFlags
           << ",\n  \"captured_device\":";
    WriteDevice(output, capture);
    output << ",\n  \"replay_device\":";
    WriteDevice(output, replay);
    output << ",\n  \"application_cache_policy\":" << Quote(options.cacheIn.empty() ? "fresh" : "imported")
           << ",\n  \"cache_import_path\":" << Quote(options.cacheIn.string())
           << ",\n  \"cache_import_bytes\":" << replay.initialCacheBytes
           << ",\n  \"cache_export_path\":" << Quote(options.cacheOut.string())
           << ",\n  \"cache_export_bytes\":" << (options.cacheOut.empty() ? 0u : replay.cache.size())
           << ",\n  \"driver_cache_state\":\"uncontrolled\""
           << ",\n  \"dispatch_count\":0,\n  \"pipeline_statistics\":" << (options.inspect ? "true" : "false")
           << ",\n  \"mode\":" << Quote(options.inspect ? "inspection" : "benchmark");
    if (options.inspect) {
        output << ",\n  \"milliseconds\":null\n}\n";
    } else {
        output << ",\n  \"milliseconds\":{\"shader_module\":" << replay.moduleMilliseconds
               << ",\"layout\":" << replay.layoutMilliseconds << ",\"pipeline_create\":" << replay.pipelineMilliseconds
               << ",\"cache\":" << replay.cacheMilliseconds << ",\"replay_total\":" << replay.totalMilliseconds << "}\n}\n";
    }
    return output.str();
}

}

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << "agc_compute_pipeline_replay CAPTURE|--fixture [--cache-in FILE] [--cache-out FILE] [--output FILE] [--inspect]\n"
                     "agc_compute_pipeline_replay --write-fixture DIRECTORY\n"
                     "--inspect requires APS5_PIPELINE_STATS=1, prints executable counters to stderr and omits benchmark timings.\n"
                     "Compiles one compute pipeline without dispatch. Fresh application cache does not clear driver caches.\n";
        return 0;
    }
    try {
        const auto options = Parse(argc, argv);
        if (!options.fixtureDirectory.empty()) {
            std::cout << AgcDriver::ComputePipelineFixture().Write(options.fixtureDirectory).string() << '\n';
            return 0;
        }
        const auto* statistics = std::getenv("APS5_PIPELINE_STATS");
        const bool statisticsEnabled = statistics != nullptr && std::string_view(statistics) != "0" && *statistics != '\0';
        if (options.inspect && !statisticsEnabled) throw std::runtime_error("--inspect requires APS5_PIPELINE_STATS=1");
        if (!options.inspect && statisticsEnabled) throw std::runtime_error("unset APS5_PIPELINE_STATS before benchmarking");
        const auto capture = options.fixture ? AgcDriver::ComputePipelineFixture() : AgcDriver::ComputePipelineCapture::Read(options.source);
        if (!options.inspect && (capture.flags & VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR) != 0) throw std::runtime_error("capture requests pipeline statistics; capture again with APS5_PIPELINE_STATS disabled");
        const auto cache = ReadCache(options.cacheIn);
        AgcDriver::VulkanDevice device(AgcDriver::VulkanDevice::CompileOnly{});
        const auto replay = [&] {
            if (!options.inspect) return device.ReplayComputePipeline(capture, cache);
            auto inspection = capture;
            inspection.flags |= VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR;
            return device.ReplayComputePipeline(inspection, cache);
        }();
        if (!options.cacheOut.empty()) WriteBytes(options.cacheOut, replay.cache);
        const auto report = Report(options, capture, replay);
        if (!options.output.empty()) WriteBytes(options.output, std::as_bytes(std::span(report.data(), report.size())));
        std::cout << report;
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "compute pipeline replay: " << error.what() << '\n';
        return 1;
    }
}
