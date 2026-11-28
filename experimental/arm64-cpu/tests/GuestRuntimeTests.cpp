#include "GuestRuntime.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <vector>
using anyps5::experimental::GuestRuntime;
static void u32(std::vector<std::uint8_t>& out, std::uint32_t n) {
    for (unsigned i = 0; i < 4; ++i)
        out.push_back(static_cast<std::uint8_t>(n >> (8 * i)));
}
static std::vector<std::uint8_t> image(std::vector<std::uint8_t> code,
                                        std::vector<std::uint8_t> data) {
    std::vector<std::uint8_t> result{'A','G','6','4'};
    u32(result, 1);
    u32(result, static_cast<std::uint32_t>(code.size()));
    u32(result, static_cast<std::uint32_t>(data.size()));
    u32(result, 0);
    result.insert(result.end(), code.begin(), code.end());
    result.insert(result.end(), data.begin(), data.end());
    return result;
}
static bool rejects(const std::vector<std::uint8_t>& bytes) {
    try { (void)GuestRuntime::load(bytes); }
    catch (const std::exception&) { return true; }
    return false;
}
int main() {
    // mov edi,4096; mov eax,[rdi]; add eax,2; ret.
    const auto valid = image({0xBF,0x00,0x10,0x00,0x00,
                              0x8B,0x07,0x83,0xC0,0x02,0xC3},
                             {40,0,0,0});
    auto runtime = GuestRuntime::load(valid);
    if (runtime.run() != 42) return EXIT_FAILURE;
    if (runtime.cpu().readMemory32(GuestRuntime::dataBase) != 40)
        return EXIT_FAILURE;
    // Read outside mapped data (but inside allocated memory) must fail.
    try { (void)runtime.cpu().readMemory32(GuestRuntime::dataBase + 4); }
    catch (const std::runtime_error&) { goto checked_guard; }
    return EXIT_FAILURE;
checked_guard:
    // Guest stack remains separately mapped.
    const auto stackTop = runtime.cpu().registerValue(4);
    runtime.cpu().writeMemory64(stackTop - 8, 1234);
    if (runtime.cpu().readMemory64(stackTop - 8) != 1234)
        return EXIT_FAILURE;
    // Malformed headers and invalid entry/section boundaries are rejected.
    auto bad = valid;
    bad[0] = 'X';
    if (!rejects(bad)) return EXIT_FAILURE;
    bad = valid;
    bad[4] = 2; // unsupported version
    if (!rejects(bad)) return EXIT_FAILURE;
    bad = valid;
    bad[8] = 255; // code size larger than payload
    if (!rejects(bad)) return EXIT_FAILURE;
    bad = valid;
    bad[16] = 255; // out-of-range entry
    if (!rejects(bad)) return EXIT_FAILURE;
    // Entry may select a later function in the code image.
    bad = image({0xB8,0x01,0,0,0,0xC3,0xB8,0x2A,0,0,0,0xC3}, {0,0,0,0});
    bad[16] = 6;
    if (GuestRuntime::load(bad).run() != 42) return EXIT_FAILURE;
    bad = valid;
    bad.pop_back(); // truncated initialized data
    if (!rejects(bad)) return EXIT_FAILURE;
    // AG64 v2: mov edi,<relocated-data-address>; mov eax,[rdi]; ret.
    // The relocation patches the immediate at code offset 1.
    std::vector<std::uint8_t> relocated{'A','G','6','4'};
    u32(relocated, 2); u32(relocated, 8); u32(relocated, 4);
    u32(relocated, 0); u32(relocated, 1);
    relocated.insert(relocated.end(), {0xBF,0,0,0,0,0x8B,0x07,0xC3});
    relocated.insert(relocated.end(), {77,0,0,0});
    u32(relocated, 1); u32(relocated, 0);
    if (GuestRuntime::load(relocated).run() != 77) return EXIT_FAILURE;
    bad = relocated;
    bad[bad.size() - 8] = 255; // invalid code patch offset
    if (!rejects(bad)) return EXIT_FAILURE;
    bad = relocated;
    bad[bad.size() - 4] = 255; // invalid target data offset
    if (!rejects(bad)) return EXIT_FAILURE;
    bad = relocated;
    bad.pop_back(); // truncated relocation table
    if (!rejects(bad)) return EXIT_FAILURE;
    // Multi-function execution: call helper, which reads relocated data.
    // call +1; ret; mov edi,<data>; mov eax,[rdi]; ret
    std::vector<std::uint8_t> multi{'A','G','6','4'};
    u32(multi, 2); u32(multi, 14); u32(multi, 4);
    u32(multi, 0); u32(multi, 1);
    multi.insert(multi.end(), {0xE8,0x01,0,0,0,0xC3,
                               0xBF,0,0,0,0,0x8B,0x07,0xC3});
    multi.insert(multi.end(), {91,0,0,0});
    u32(multi, 7); u32(multi, 0);
    if (GuestRuntime::load(multi).run() != 91) return EXIT_FAILURE;
    // Reject duplicate relocation writes to the same immediate.
    bad = multi;
    bad[20] = 2; // two relocation records
    u32(bad, 7); u32(bad, 0);
    if (!rejects(bad)) return EXIT_FAILURE;
    // Reject overlapping, non-identical relocation patches.
    bad[bad.size() - 8] = 8;
    if (!rejects(bad)) return EXIT_FAILURE;
    std::puts("AG64 image loader, data/stack isolation and malformed input tests passed");
    return EXIT_SUCCESS;
}
