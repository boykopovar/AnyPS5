#include "prx/libSceAgcDriver/Execution/include/WorkerSampler.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <windows.h>

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    const std::string mode = argv[1];
    const bool selected = mode == "selected";
    const bool process = mode == "process";
    const bool workerExpected = selected || mode == "default";
    const auto stem = std::filesystem::temp_directory_path() / ("anyps5-sampler-" + std::to_string(GetCurrentProcessId()));
    auto workerPath = stem;
    workerPath += "-worker.txt";
    auto processPath = stem;
    processPath += "-process.txt";
    _wputenv_s(L"APS5_SAMPLE_WORKER", workerPath.c_str());
    _wputenv_s(L"APS5_SAMPLE_THREADS", process ? processPath.c_str() : L"");
    _putenv_s("APS5_SAMPLE_WORKER_QUEUE", mode == "default" ? "" : "0x50");
    AgcDriver::StartWorkerSampler(selected ? 0x50 : 0);
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    while (std::chrono::steady_clock::now() < until) std::this_thread::yield();
    AgcDriver::StopWorkerSampler();
    const auto sampled = [](const std::filesystem::path& path) {
        std::ifstream file(path);
        std::string marker;
        std::uint64_t samples = 0;
        return (file >> marker >> samples) && marker == "#" && samples != 0;
    };
    auto collapsed = stem;
    collapsed += "-worker.collapsed.txt";
    auto temporary = collapsed;
    temporary += ".tmp";
    const auto published = [&] {
        std::ifstream file(collapsed);
        std::string header;
        unsigned long long samples = 0;
        return std::getline(file, header) && std::sscanf(header.c_str(), "# samples=%llu", &samples) == 1 && samples != 0 &&
            !std::filesystem::exists(temporary);
    };
    const bool passed = sampled(workerPath) == workerExpected && sampled(processPath) == process && published() == workerExpected;
    std::filesystem::remove(workerPath);
    std::filesystem::remove(processPath);
    std::filesystem::remove(collapsed);
    std::filesystem::remove(temporary);
    std::printf("worker sampler %s: %s\n", mode.c_str(), passed ? "passed" : "failed");
    return passed ? 0 : 1;
}
