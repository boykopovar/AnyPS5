#include "prx/libSceAgcDriver/Execution/include/TraceFile.hpp"
#include <algorithm>
#include <barrier>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

}

int main() {
    try {
        constexpr int threadCount = 8;
        constexpr int linesPerThread = 120;
        constexpr std::size_t lineBytes = 4096;
        std::vector<std::string> expected;
        expected.reserve(threadCount * linesPerThread);
        for (int thread = 0; thread < threadCount; ++thread) {
            for (int index = 0; index < linesPerThread; ++index) {
                std::string line = "T" + std::to_string(thread) + "-N" + std::to_string(index) + ":";
                line.append(lineBytes - line.size() - 1, static_cast<char>('A' + thread));
                line.push_back('\n');
                expected.push_back(std::move(line));
            }
        }
        const auto path = std::filesystem::temp_directory_path() / ("aps5-trace-append-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".jsonl");
        std::barrier start(threadCount);
        std::vector<std::thread> workers;
        workers.reserve(threadCount);
        for (int thread = 0; thread < threadCount; ++thread) {
            workers.emplace_back([&, thread] {
                start.arrive_and_wait();
                for (int index = 0; index < linesPerThread; ++index) {
                    AgcDriver::TraceFile::AppendLine(path, expected[thread * linesPerThread + index]);
                }
            });
        }
        for (auto& worker : workers) worker.join();
        std::ifstream input(path, std::ios::binary);
        const std::string written((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        input.close();
        std::filesystem::remove(path);
        Check(written.size() == expected.size() * lineBytes, "concurrent trace appends changed the output size");
        std::vector<std::string> actual;
        actual.reserve(expected.size());
        for (std::size_t offset = 0; offset < written.size(); offset += lineBytes) actual.push_back(written.substr(offset, lineBytes));
        std::sort(actual.begin(), actual.end());
        std::sort(expected.begin(), expected.end());
        Check(actual == expected, "concurrent trace appends changed, lost, or duplicated records");
        std::cout << "trace file append tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
