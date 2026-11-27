#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTrace.hpp"
#include "execution/VulkanTestDevice.hpp"
#include <SDL_stdinc.h>
#include "zlib.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

void Check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::string ReadGzip(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    Check(bytes.size() > 18, "the GPU trace gzip file is truncated");
    std::size_t size = 0;
    for (unsigned index = 0; index < 4; ++index) size |= static_cast<std::size_t>(bytes[bytes.size() - 4 + index]) << (8 * index);
    z_stream stream{};
    Check(inflateInit2(&stream, -MAX_WBITS) == Z_OK, "inflateInit2 failed");
    std::string inflated(size, 0);
    stream.next_in = const_cast<Bytef*>(bytes.data() + 10);
    stream.avail_in = static_cast<uInt>(bytes.size() - 18);
    stream.next_out = reinterpret_cast<Bytef*>(inflated.data());
    stream.avail_out = static_cast<uInt>(inflated.size());
    const auto status = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);
    Check(status == Z_STREAM_END && stream.avail_out == 0, "the GPU trace does not inflate to its recorded size");
    return inflated;
}

int Run(int argc, char** argv) {
    Check(argc == 2, "expected the requested GPU trace capacity");
    const auto capacity = AgcDriver::TraceThread::ParseCapacity(argv[1]);
    Check(SDL_setenv("APS5_TRACE_EVENTS", argv[1], 1) == 0 && SDL_setenv("APS5_TRACE_FRAMES", "0:1", 1) == 0, "could not configure the GPU trace window");
    const auto count = capacity == 1 ? 2 : capacity;
    const auto directory = std::filesystem::temp_directory_path() / ("aps5-gpu-capacity-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    const auto utf8 = directory.u8string();
    const std::string path(utf8.begin(), utf8.end());
#ifdef _WIN32
    Check(_wputenv_s(L"APS5_TRACE_DIR", directory.c_str()) == 0, "could not set the GPU trace directory");
#else
    Check(SDL_setenv("APS5_TRACE_DIR", path.c_str(), 1) == 0, "could not set the GPU trace directory");
#endif
    auto device = OpenVulkanTestDevice();
    if (!device) {
        std::filesystem::remove_all(directory);
        return VulkanTestSkipped;
    }
    double gpu = 0;
    std::chrono::steady_clock::time_point cpu;
    if (!device->CalibrateClock(gpu, cpu)) {
        std::cout << "skipped, calibrated GPU timestamps are unavailable\n";
        std::filesystem::remove_all(directory);
        return VulkanTestSkipped;
    }
    AgcDriver::PerformanceTrace::Present(1, 1, device.get());
    Check(device->CalibrateClock(gpu, cpu), "GPU clock calibration disappeared");
    for (std::size_t index = 0; index < count; ++index) AgcDriver::PerformanceTrace::GpuRange("gpu-capacity-probe", gpu, gpu + 1000, index + 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    AgcDriver::PerformanceTrace::Present(1, 1, device.get());
    AgcDriver::PerformanceTrace::StopOutput();
    std::string trace;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().extension() == ".gz") trace = ReadGzip(entry.path());
    }
    Check(!trace.empty(), "the runtime did not write a GPU trace capture");
    constexpr std::string_view name = "\"name\":\"gpu-capacity-probe\"";
    std::size_t retained = 0;
    for (auto at = trace.find(name); at != std::string::npos; at = trace.find(name, at + name.size())) ++retained;
    std::ifstream summaryFile(directory / "aps5-summary.jsonl");
    const std::string summary((std::istreambuf_iterator<char>(summaryFile)), std::istreambuf_iterator<char>());
    summaryFile.close();
    std::cout << "requested=" << capacity << " submitted=" << count << " retained=" << retained << '\n' << summary;
    Check(retained == std::min(capacity, count), "the runtime GPU trace ignored the requested capacity; capture: " + path);
    if (count > capacity) {
        Check(summary.find("\"gpu\":{\"events_lost\":" + std::to_string(count - capacity) + ",") != std::string::npos, "the GPU trace did not report its exact lost range count");
    } else {
        Check(summary.find("\"gpu\":{\"events_lost\":") == std::string::npos, "the GPU trace lost ranges inside the requested capacity");
    }
    std::filesystem::remove_all(directory);
    return 0;
}

}

int main(int argc, char** argv) {
    int result = 1;
    try {
        result = Run(argc, argv);
    } catch (const std::exception& error) {
        AgcDriver::PerformanceTrace::StopOutput();
        std::cerr << error.what() << '\n';
    }
    try {
        AgcDriverShutdown_nid_postfix();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    return result;
}
