#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>


static std::string ReadAll(const std::filesystem::path& path) {
    std::ifstream file(path);
    std::stringstream text;
    text << file.rdbuf();
    return text.str();
}

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stdout, "%s\n", message);
    std::abort();
}

int main() {
#ifdef _WIN32
    const unsigned long pid = static_cast<unsigned long>(_getpid());
#else
    const unsigned long pid = static_cast<unsigned long>(getpid());
#endif
    const auto path = std::filesystem::temp_directory_path() / ("anyps5-heap-report-" + std::to_string(pid) + ".txt");
    const auto outputPath = std::filesystem::temp_directory_path() / ("anyps5-heap-report-out-" + std::to_string(pid) + ".txt");
#ifdef _WIN32
    Require(_wfreopen(path.wstring().c_str(), L"w", stderr) != nullptr, "stderr redirection failed");
    Require(_wfreopen(outputPath.wstring().c_str(), L"w", stdout) != nullptr, "stdout redirection failed");
#else
    Require(std::freopen(path.c_str(), "w", stderr) != nullptr, "stderr redirection failed");
    Require(std::freopen(outputPath.c_str(), "w", stdout) != nullptr, "stdout redirection failed");
#endif

    char heapName[] = "SceLibcHeap";
    sceLibcInternalBacktraceForGame_nid_postfix(heapName);
    sceLibcInternalBacktraceForGame_nid_postfix(nullptr);
    alignas(16) char heap[0x400] = {};
    std::strcpy(heap + 0x3B0, "SceLibcHeap");
    const std::uint32_t number = 3;
    std::memcpy(heap + 0x3F4, &number, sizeof(number));
    int block = 0;
    sceLibcInternalHeapErrorReportForGame_nid_postfix(heap, &block, 1);
    std::memset(heap + 0x3F4, 0, sizeof(number));
    sceLibcInternalHeapErrorReportForGame_nid_postfix(heap, &block, 2);
    bool threw = false;
    try {
        sceLibcInternalHeapErrorReportForGame_nid_postfix(nullptr, &block, 1);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "null heap accepted");
    std::fflush(stdout);
    const bool closed = _Stdout_nid_postfix.Close() == 0;
    bool closedThrew = false;
    try {
        sceLibcInternalHeapErrorReportForGame_nid_postfix(heap, &block, 1);
    } catch (const std::exception&) {
        closedThrew = true;
    }
    if (!closed || closedThrew) {
        std::fputs("heap error report on a closed standard output\n", stderr);
        std::abort();
    }
    std::fflush(stderr);
    std::fclose(stderr);

    const std::string report = ReadAll(path);
    const std::string output = ReadAll(outputPath);
    std::filesystem::remove(path);
    std::filesystem::remove(outputPath);
    const std::string first = "[SceLibc] A heap error is detected.\n[SceLibc] -- Mspace Name --\n[SceLibc] SceLibcHeap ( 3)\n"
        "[SceLibc] -- Error reason --\n[SceLibc] segment out of range.\n[SceLibc] pid:<0x";
    const std::string second = "[SceLibc] A heap error is detected.\n[SceLibc] -- Mspace Name --\n[SceLibc] SceLibcHeap\n[SceLibc] pid:<0x";
    Require(output.rfind(first, 0) == 0, "segment error report differs");
    Require(output.find(second, first.size()) != std::string::npos, "unnamed reason report differs");
    Require(output.find("reason", output.find(second)) == std::string::npos, "reason printed for error 2");
    Require(report.find("[SceLibcHeap] backtrace requested from guest 0x") != std::string::npos, "heap name not reported");
    Require(report.find("[] backtrace requested from guest 0x") != std::string::npos, "null heap name not reported");
    Require(report.find("[libc] heap error 0x1: heap ") != std::string::npos, "host diagnostic missing");
    char pidText[32];
    std::snprintf(pidText, sizeof(pidText), "pid:<0x%lx> tid:<0x", pid);
    Require(output.find(pidText) != std::string::npos, "pid differs");
    Require(std::string(heapName) == "SceLibcHeap", "heap name modified");
}
