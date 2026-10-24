#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

extern "C" {
void APS5_VABI sceLibcInternalBacktraceForGame_nid_postfix(const char* heapName);
void APS5_VABI sceLibcInternalHeapErrorReportForGame_nid_postfix(void* heap, void* block, std::uint32_t error);
}

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stdout, "%s\n", message);
    std::abort();
}

int main() {
    const auto path = std::filesystem::temp_directory_path() / ("anyps5-heap-report-" + std::to_string(std::rand()) + ".txt");
#ifdef _WIN32
    Require(_wfreopen(path.wstring().c_str(), L"w", stderr) != nullptr, "stderr redirection failed");
#else
    Require(std::freopen(path.c_str(), "w", stderr) != nullptr, "stderr redirection failed");
#endif

    char heapName[] = "SceLibcHeap";
    sceLibcInternalBacktraceForGame_nid_postfix(heapName);
    sceLibcInternalBacktraceForGame_nid_postfix(nullptr);
    int heap = 0;
    int block = 0;
    sceLibcInternalHeapErrorReportForGame_nid_postfix(&heap, &block, 0x1234u);
    std::fflush(stderr);
    std::fclose(stderr);

    std::ifstream file(path);
    std::stringstream text;
    text << file.rdbuf();
    file.close();
    std::filesystem::remove(path);
    const std::string report = text.str();
    Require(report.find("[SceLibcHeap] backtrace requested from guest 0x") != std::string::npos, "heap name not reported");
    Require(report.find("[] backtrace requested from guest 0x") != std::string::npos, "null heap name not reported");
    Require(report.find("[libc] heap error 0x1234: heap ") != std::string::npos, "heap error not reported");
    Require(std::string(heapName) == "SceLibcHeap", "heap name modified");
}
