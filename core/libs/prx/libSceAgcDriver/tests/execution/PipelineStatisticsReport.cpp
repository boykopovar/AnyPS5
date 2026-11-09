#include "VulkanTestDevice.hpp"
#include "../../tools/ComputePipelineFixture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::Require;

class StderrCapture {
public:
    StderrCapture() : path(std::filesystem::temp_directory_path() / ("aps5_pipeline_statistics_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt")) {
        std::fflush(stderr);
#ifdef _WIN32
        saved = _dup(_fileno(stderr));
        Require(saved >= 0, "cannot save stderr");
        Require(_wfreopen(path.wstring().c_str(), L"wb", stderr) != nullptr, "cannot redirect stderr");
#else
        saved = dup(fileno(stderr));
        Require(saved >= 0, "cannot save stderr");
        Require(std::freopen(path.c_str(), "wb", stderr) != nullptr, "cannot redirect stderr");
#endif
    }

    ~StderrCapture() {
        restore();
        std::error_code error;
        std::filesystem::remove(path, error);
    }

    StderrCapture(const StderrCapture&) = delete;
    StderrCapture& operator=(const StderrCapture&) = delete;

    std::string Finish() {
        restore();
        std::ifstream file(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }

private:
    void restore() {
        if (saved < 0) return;
        std::fflush(stderr);
#ifdef _WIN32
        _dup2(saved, _fileno(stderr));
        _close(saved);
#else
        dup2(saved, fileno(stderr));
        close(saved);
#endif
        saved = -1;
    }

    std::filesystem::path path;
    int saved = -1;
};

std::unique_ptr<AgcDriver::VulkanDevice> OpenDevice() {
    try {
        return OpenVulkanTestDevice();
    } catch (const std::exception& error) {
        if (std::string_view(error.what()).find("APS5_PIPELINE_STATS requires") == std::string_view::npos) throw;
        std::printf("skipped, the device reports no pipeline executable statistics: %s\n", error.what());
        return nullptr;
    }
}

struct StatisticsPipeline {
    explicit StatisticsPipeline(const AgcDriver::Graphics::Context& context) : context(context) {
        const auto spirv = AgcDriver::ComputePipelineFixture().spirv;
        VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        moduleInfo.codeSize = spirv.size() * sizeof(std::uint32_t);
        moduleInfo.pCode = spirv.data();
        Require(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &moduleInfo, nullptr, &module) == VK_SUCCESS, "vkCreateShaderModule");
        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        Require(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &layout) == VK_SUCCESS, "vkCreatePipelineLayout");
        VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pipelineInfo.flags = VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR;
        pipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipelineInfo.stage.module = module;
        pipelineInfo.stage.pName = "main";
        pipelineInfo.layout = layout;
        Require(context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) == VK_SUCCESS, "vkCreateComputePipelines");
    }

    ~StatisticsPipeline() {
        context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
        context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, layout, nullptr);
        context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
    }

    StatisticsPipeline(const StatisticsPipeline&) = delete;
    StatisticsPipeline& operator=(const StatisticsPipeline&) = delete;

    AgcDriver::Graphics::Context context;
    VkShaderModule module = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};

std::vector<std::size_t> ExecutableShape(const AgcDriver::Graphics::Context& context, VkPipeline pipeline) {
    const auto getProperties = context.Function<PFN_vkGetPipelineExecutablePropertiesKHR>("vkGetPipelineExecutablePropertiesKHR");
    const auto getStatistics = context.Function<PFN_vkGetPipelineExecutableStatisticsKHR>("vkGetPipelineExecutableStatisticsKHR");
    VkPipelineInfoKHR pipelineInfo{VK_STRUCTURE_TYPE_PIPELINE_INFO_KHR};
    pipelineInfo.pipeline = pipeline;
    std::uint32_t executables = 0;
    Require(getProperties(context.device, &pipelineInfo, &executables, nullptr) == VK_SUCCESS, "vkGetPipelineExecutablePropertiesKHR count");
    std::vector<std::size_t> shape;
    for (std::uint32_t index = 0; index < executables; ++index) {
        VkPipelineExecutableInfoKHR executable{VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INFO_KHR};
        executable.pipeline = pipeline;
        executable.executableIndex = index;
        std::uint32_t statistics = 0;
        Require(getStatistics(context.device, &executable, &statistics, nullptr) == VK_SUCCESS, "vkGetPipelineExecutableStatisticsKHR count");
        shape.push_back(statistics);
    }
    return shape;
}

std::vector<std::string> StatisticLines(const std::string& text) {
    std::istringstream stream(text);
    std::vector<std::string> lines;
    for (std::string line; std::getline(stream, line);) {
        if (line.find("[pipeline-stats]") == std::string::npos) continue;
        Require(line.starts_with("[pipeline-stats] executable=") && line.find("[pipeline-stats]", 1) == std::string::npos, "a [pipeline-stats] line was split by another report");
        lines.push_back(std::move(line));
    }
    return lines;
}

void CheckShape(const std::vector<std::string>& report, const std::vector<std::size_t>& shape) {
    Require(!shape.empty(), "the pipeline reports no executables");
    std::size_t line = 0;
    for (std::size_t executable = 0; executable < shape.size(); ++executable) {
        const auto prefix = "[pipeline-stats] executable=" + std::to_string(executable) + " ";
        Require(line < report.size() && report[line].starts_with(prefix + "stages=0x"), "an executable's header is missing from the reference report");
        ++line;
        for (std::size_t statistic = 0; statistic < shape[executable]; ++statistic, ++line) {
            Require(line < report.size() && report[line].starts_with(prefix) && !report[line].starts_with(prefix + "stages="), "an executable's statistics in the reference report differ from the queried ones");
        }
    }
    Require(line == report.size(), "the reference report has lines past its executables");
}

std::size_t CountReports(const std::vector<std::string>& lines, const std::vector<std::string>& reference) {
    Require(!reference.empty(), "the reference report is empty");
    for (std::size_t line = 0; line < lines.size(); ++line) {
        Require(lines[line] == reference[line % reference.size()], "a pipeline statistics report was interleaved with another or lost a line");
    }
    Require(lines.size() % reference.size() == 0, "the last pipeline statistics report is incomplete");
    return lines.size() / reference.size();
}

template<typename TCheck>
bool Rejects(TCheck check) {
    try {
        check();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

void CheckParser() {
    const std::vector<std::string> reference = {
        "[pipeline-stats] executable=0 stages=0x1 subgroup=32 name=VS",
        "[pipeline-stats] executable=0 Register Count=24 (registers)",
        "[pipeline-stats] executable=0 Binary Size=4096 (bytes)",
        "[pipeline-stats] executable=0 Stack Size=0 (bytes)",
        "[pipeline-stats] executable=1 stages=0x10 subgroup=32 name=FS",
        "[pipeline-stats] executable=1 Output Colors=1 (colors)",
    };
    std::string report;
    for (const auto& line : reference) report += line + "\n";
    CheckShape(StatisticLines(report), {3, 1});
    Require(Rejects([&] { CheckShape(StatisticLines(report), {1, 3}); }), "the parser accepted a report against another executable shape");
    Require(CountReports(StatisticLines("[gpu] other output\n" + report + report + report), reference) == 3, "the parser did not count three intact two-executable reports");
    const auto split = report.substr(0, report.find(" (registers)")) + reference[0] + "\n (registers)\n" + report.substr(report.find(" (registers)") + 13);
    Require(Rejects([&] { CountReports(StatisticLines(split + report), reference); }), "the parser accepted a split line");
    auto missing = reference;
    missing.erase(missing.begin() + 2);
    std::string lost;
    for (const auto& line : missing) lost += line + "\n";
    Require(Rejects([&] { CountReports(StatisticLines(lost + report), reference); }), "the parser accepted a report that lost a line");
    std::string interleaved;
    for (std::size_t line = 0; line < reference.size(); ++line) interleaved += reference[line] + "\n" + reference[line] + "\n";
    Require(Rejects([&] { CountReports(StatisticLines(interleaved), reference); }), "the parser accepted two interleaved reports");
    Require(Rejects([&] { CountReports(StatisticLines(report + reference[0] + "\n"), reference); }), "the parser accepted an incomplete last report");
}

}

int main() {
    try {
        CheckParser();
        std::puts("pipeline statistics: the parser accepts intact two-executable reports and rejects split, lost, interleaved and incomplete ones");
#ifdef _WIN32
        Require(_putenv_s("APS5_PIPELINE_STATS", "1") == 0, "cannot set APS5_PIPELINE_STATS");
#else
        Require(setenv("APS5_PIPELINE_STATS", "1", 1) == 0, "cannot set APS5_PIPELINE_STATS");
#endif
        const auto device = OpenDevice();
        if (!device) return VulkanTestSkipped;
        const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        Require(context.pipelineExecutableInfo, "APS5_PIPELINE_STATS=1 did not enable pipeline executable statistics");
        const StatisticsPipeline pipeline(context);
        std::vector<std::string> reference;
        {
            StderrCapture output;
            AgcDriver::Graphics::LogPipelineStatistics_nid_no_patch(context, pipeline.pipeline);
            reference = StatisticLines(output.Finish());
        }
        const auto shape = ExecutableShape(context, pipeline.pipeline);
        CheckShape(reference, shape);
        constexpr unsigned threads = 8;
        constexpr unsigned reportsPerThread = 64;
        std::vector<std::exception_ptr> failures(threads);
        StderrCapture output;
        {
            std::vector<std::jthread> workers;
            for (unsigned thread = 0; thread < threads; ++thread) {
                workers.emplace_back([&, thread] {
                    try {
                        for (unsigned report = 0; report < reportsPerThread; ++report) AgcDriver::Graphics::LogPipelineStatistics_nid_no_patch(context, pipeline.pipeline);
                    } catch (...) {
                        failures[thread] = std::current_exception();
                    }
                });
            }
        }
        const auto text = output.Finish();
        for (const auto& failure : failures) {
            if (failure) std::rethrow_exception(failure);
        }
        const auto reports = CountReports(StatisticLines(text), reference);
        Require(reports == threads * reportsPerThread, "a pipeline statistics report is missing");
        std::printf("pipeline statistics: %zu reports of %zu executables and %zu lines each from %u threads, each in one piece\n", reports, shape.size(), reference.size(), threads);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "pipeline statistics report test: %s\n", error.what());
        return 1;
    }
}
