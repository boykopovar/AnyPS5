#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void check(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("pipelined predicated label order: " + what);
}

alignas(256) std::array<std::uint32_t, 4> Predicate{};
alignas(256) std::array<std::uint32_t, 4> Target{};

void WriteData(std::vector<std::uint32_t>& words, const void* destination, std::uint32_t value, bool predicated) {
    const auto pointer = reinterpret_cast<std::uintptr_t>(destination);
    words.insert(words.end(), {0xc0033700u | (predicated ? 1u : 0u), 0x00100200u, static_cast<std::uint32_t>(pointer), static_cast<std::uint32_t>(pointer >> 32u), value});
}

}

int main() {
#ifdef _WIN32
    _putenv_s("APS5_PIPELINED_DRAWS", "1");
#else
    setenv("APS5_PIPELINED_DRAWS", "1", 1);
#endif
    try {
        Predicate = {1, 0, 0, 0};
        Target = {0, 0, 0, 0};
        const auto predicate = reinterpret_cast<std::uintptr_t>(Predicate.data());
        std::vector<std::uint32_t> words;
        words.insert(words.end(), {0xc0022000u, (4u << 16u) | 0x100u, static_cast<std::uint32_t>(predicate), static_cast<std::uint32_t>(predicate >> 32u)});
        WriteData(words, Target.data(), 1, true);
        WriteData(words, Target.data(), 2, false);
        Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
        check(sceAgcDriverSubmitDcb(&packet) == 0, "submission failed");
        AgcDriverWaitIdle_nid_postfix();
        check(Target[0] == 2, "a predicated WRITE_DATA landed after the unpredicated WRITE_DATA that follows it (target " + std::to_string(Target[0]) + ", expected 2)");
        LibcRunShutdown_nid_postfix();
        std::puts("pipelined predicated label order test passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { LibcRunShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
