#include "prx/libSceHttp/include/Http.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "SceTypes.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

extern "C" {

extern FileStream* __stdoutp_nid_postfix;
}

namespace {

std::string g_probe;

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "check failed: %s\n", what);
    if (!g_probe.empty()) {
        std::error_code ignored;
        std::filesystem::remove(g_probe, ignored);
    }
    std::exit(1);
}

std::string Capture() {
    Require(freopen_nid_postfix(g_probe.c_str(), "w", __stdoutp_nid_postfix) == __stdoutp_nid_postfix, "freopen");
    sceHttpDbgShowMemoryPoolStat();
    Require(fflush_nid_postfix(__stdoutp_nid_postfix) == 0, "fflush");
    std::ifstream stream(g_probe, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
}

}

int main() {
    g_probe = "anyps5-http-debug-memory-pool-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt";
    const std::string expected = " current      max\n       0        0\n";
    Require(Capture() == expected, "zeros without a live context");

    const int context = sceHttpInit_nid_postfix(0, 0, 0x40000);
    Require(context > 0, "sceHttpInit");
    Require(Capture() == expected, "same text with a live context");
    Require(sceHttpTerm_nid_postfix(context) == 0, "sceHttpTerm");

    Require(fclose_nid_postfix(__stdoutp_nid_postfix) == 0, "fclose");
    Require(std::filesystem::remove(g_probe), "remove the probe");
    return 0;
}
