#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string_view>

namespace AgcDriver::DriverDetail {

double TraceMs() {
    static const auto origin = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - origin).count();
}

std::uint64_t SkippedShader() {
    static const std::uint64_t skipped = [] { const char* text = std::getenv("APS5_SKIP_SHADER"); return text ? std::strtoull(text, nullptr, 16) : 0ull; }();
    return skipped;
}

bool SkipRefusedDraws() {
    static const bool skip = std::getenv("APS5_SKIP_REFUSED_DRAWS") != nullptr;
    return skip;
}

void ReportRefusedDraw(const RefusedDraw& refused) {
    static std::mutex reportedMutex;
    static std::set<std::uint64_t> reported;
    {
        std::lock_guard lock(reportedMutex);
        if (!reported.insert(refused.address).second) return;
    }
    const std::string_view what = refused.what();
    const auto line = what.substr(0, what.find('\n'));
    std::fprintf(stderr, "[gpu] APS5_SKIP_REFUSED_DRAWS: skipping the draws that use shader 0x%llx: %.*s\n", static_cast<unsigned long long>(refused.address), static_cast<int>(line.size()), line.data());
}

void require(bool condition, const char* reason) {
    if (!condition) {
        throw std::runtime_error(std::string("AGC driver: ") + reason);
    }
}

std::uint32_t readRegister(const Registers& registers, std::uint32_t offset) {
    const auto it = registers.find(offset);
    require(it != registers.end(), "required shader register has not been written");
    return it->second;
}

std::uint32_t readUserData(const Registers& shader, std::uint32_t offset) {
    const auto it = shader.find(offset);
    return it == shader.end() ? 0u : it->second;
}

}
