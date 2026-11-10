#include "prx/libSceAjm.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static_assert(sizeof(WVorbisHeaderPacketParameter) == 0x48);

namespace {

constexpr int InvalidParameter = static_cast<int>(0x8A6E0001);
constexpr int InvalidSetup = static_cast<int>(0x8A6E0004);

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

WVorbisHeaderPacketParameter Valid(const void* setup, std::size_t setupSize, void* output, std::size_t capacity) {
    return {sizeof(WVorbisHeaderPacketParameter), 2, 48000, 16000, 8, 11, 0, setup, setupSize, output, capacity, 0};
}

void TestValidPacket() {
    constexpr std::uint8_t Setup[] {5, 'v', 'o', 'r', 'b', 'i', 's', 9, 0, 0xAA, 0xBB, 0xCC};
    std::array<std::uint8_t, 128> output {};
    output.fill(0xEE);
    auto param = Valid(Setup, sizeof(Setup), output.data(), output.size());
    Require(sceAjmDecWVorbisCreateHeaderPacket(&param) == 0, "a valid header packet is built");
    Require(param.outputSize == 56 + sizeof(Setup), "the written size covers the table, both headers and the setup packet");
    constexpr std::uint8_t Table[] {0, 0, 30, 0, 16, 0, 9, 0, 3, 0};
    Require(std::memcmp(output.data(), Table, sizeof(Table)) == 0, "the length table is written");
    constexpr std::uint8_t Golden[] {
        0, 0, 30, 0, 16, 0, 9, 0, 3, 0,
        1, 'v', 'o', 'r', 'b', 'i', 's',
        0, 0, 0, 0,
        2,
        0x80, 0xBB, 0x00, 0x00,
        0, 0, 0, 0,
        0x00, 0xF4, 0x01, 0x00,
        0, 0, 0, 0,
        0xB8,
        1,
        3, 'v', 'o', 'r', 'b', 'i', 's',
        0, 0, 0, 0,
        0, 0, 0, 0,
        1,
        5, 'v', 'o', 'r', 'b', 'i', 's', 9, 0, 0xAA, 0xBB, 0xCC};
    Require(std::memcmp(output.data(), Golden, sizeof(Golden)) == 0, "the packet matches the expected header layout");
    Require(std::memcmp(output.data() + 56, Setup, sizeof(Setup)) == 0, "the setup packet follows unchanged");
    Require(output[56 + sizeof(Setup)] == 0xEE, "nothing is written past the packet");
}

void TestNullParameter() {
    std::array<std::uint8_t, 12> setup {};
    setup.fill(0xEE);
    std::array<std::uint8_t, 128> output {};
    output.fill(0xEE);
    auto param = Valid(setup.data(), setup.size(), output.data(), output.size());
    Require(sceAjmDecWVorbisCreateHeaderPacket(nullptr) == InvalidParameter, "a null parameter is refused");
    Require(param.outputSize == 0 && output[127] == 0xEE, "the refused call leaves the caller buffers untouched");
}

void TestDanglingParameter() {
    std::array<std::uint8_t, 12> setup {};
    setup.fill(0xEE);
    std::array<std::uint8_t, 128> output {};
    output.fill(0xEE);
    Require(sceAjmDecWVorbisCreateHeaderPacket(reinterpret_cast<WVorbisHeaderPacketParameter*>(0x10)) == InvalidParameter, "a dangling parameter is refused");
    Require(output[127] == 0xEE, "the dangling call leaves the caller buffers untouched");
}

void TestDanglingSetup() {
    std::array<std::uint8_t, 12> setup {};
    constexpr std::uint8_t Content[] {5, 'v', 'o', 'r', 'b', 'i', 's', 9, 0, 0xAA, 0xBB, 0xCC};
    std::memcpy(setup.data(), Content, sizeof(Content));
    std::array<std::uint8_t, 128> output {};
    output.fill(0xEE);
    auto param = Valid(setup.data(), setup.size(), output.data(), output.size());
    param.setupPacket = reinterpret_cast<const void*>(0x10);
    Require(sceAjmDecWVorbisCreateHeaderPacket(&param) == InvalidParameter, "a dangling setup packet is refused");
    Require(param.outputSize == 0 && output[56 + setup.size()] == 0xEE, "the refused call leaves the caller buffers untouched");
}

void TestOversizedSetupSize() {
    constexpr std::size_t Huge = 1ULL << 60;
    std::array<std::uint8_t, 128> output {};
    output.fill(0xEE);
    auto param = Valid(reinterpret_cast<const void*>(0x10), Huge, output.data(), Huge);
    Require(sceAjmDecWVorbisCreateHeaderPacket(&param) == InvalidSetup, "an oversized setup size is refused before any guest memory is read");
    Require(param.outputSize == 0 && output[127] == 0xEE, "the refused call leaves the caller buffers untouched");
}

void TestDanglingOutput() {
    std::array<std::uint8_t, 12> setup {};
    constexpr std::uint8_t Content[] {5, 'v', 'o', 'r', 'b', 'i', 's', 9, 0, 0xAA, 0xBB, 0xCC};
    std::memcpy(setup.data(), Content, sizeof(Content));
    auto param = Valid(setup.data(), setup.size(), reinterpret_cast<void*>(0x10), 128);
    Require(sceAjmDecWVorbisCreateHeaderPacket(&param) == InvalidParameter, "a dangling output buffer is refused");
    Require(param.outputSize == 0 && setup[11] == 0xCC, "the refused call leaves the caller buffers untouched");
}

}

void TestSetupSizeAtCapBoundary() {
    constexpr std::size_t Cap = 2 * 0xFFFF;
    std::vector<std::uint8_t> setup(Cap);
    setup[7] = 0xFF;
    setup[8] = 0xFF;
    std::vector<std::uint8_t> output(56 + Cap);
    auto param = Valid(setup.data(), setup.size(), output.data(), output.size());
    Require(sceAjmDecWVorbisCreateHeaderPacket(&param) == 0, "a setup size of exactly the cap is accepted");
    Require(param.outputSize == 56 + Cap, "the written size covers the table, both headers and the full-cap packet");

    std::array<std::uint8_t, 128> small {};
    small.fill(0xEE);
    auto over = Valid(reinterpret_cast<const void*>(0x10), Cap + 1, small.data(), small.size());
    Require(sceAjmDecWVorbisCreateHeaderPacket(&over) == InvalidSetup, "a setup size one byte past the cap is refused");
    Require(over.outputSize == 0 && small[127] == 0xEE, "the over-cap call leaves the caller buffers untouched");
}

void TestReadableNonWritableParameterInput() {
    constexpr std::uint8_t Setup[] {5, 'v', 'o', 'r', 'b', 'i', 's', 9, 0, 0xAA, 0xBB, 0xCC};
    std::array<std::uint8_t, 128> output {};
    output.fill(0xEE);
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const std::size_t page = si.dwPageSize;
    auto* mem = static_cast<unsigned char*>(VirtualAlloc(nullptr, 2 * page, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    Require(mem != nullptr, "allocate guard pages");
#else
    const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    auto* mem = static_cast<unsigned char*>(mmap(nullptr, 2 * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(mem != MAP_FAILED, "allocate guard pages");
#endif
    constexpr std::size_t inputSize = offsetof(WVorbisHeaderPacketParameter, outputSize);
    auto* param = reinterpret_cast<WVorbisHeaderPacketParameter*>(mem + page - inputSize);
    *param = Valid(Setup, sizeof(Setup), output.data(), output.size());
#ifdef _WIN32
    DWORD previous;
    Require(VirtualProtect(mem, page, PAGE_READONLY, &previous) != 0, "protect input page read-only");
#else
    Require(mprotect(mem, page, PROT_READ) == 0, "protect input page read-only");
#endif
    Require(sceAjmDecWVorbisCreateHeaderPacket(param) == 0, "a parameter with read-only inputs and writable outputSize succeeds");
    Require(param->outputSize == 56 + sizeof(Setup), "outputSize is written successfully");
    Require(output[0] == 0 && output[1] == 0, "output packet is written");
#ifdef _WIN32
    Require(VirtualFree(mem, 0, MEM_RELEASE) != 0, "release guard pages");
#else
    Require(munmap(mem, 2 * page) == 0, "release guard pages");
#endif
}

void TestDanglingOutputRestoresOutputSize() {
    constexpr std::uint8_t Setup[] {5, 'v', 'o', 'r', 'b', 'i', 's', 9, 0, 0xAA, 0xBB, 0xCC};
    auto param = Valid(Setup, sizeof(Setup), reinterpret_cast<void*>(0x10), 128);
    param.outputSize = 0xAA55;
    Require(sceAjmDecWVorbisCreateHeaderPacket(&param) == InvalidParameter, "a dangling output buffer is refused");
    Require(param.outputSize == 0xAA55, "outputSize is restored when the output write fails");
}

int main() {
    TestValidPacket();
    TestNullParameter();
    TestDanglingParameter();
    TestDanglingSetup();
    TestOversizedSetupSize();
    TestSetupSizeAtCapBoundary();
    TestDanglingOutput();
    TestDanglingOutputRestoresOutputSize();
    TestReadableNonWritableParameterInput();
    std::puts("guest ajm wvorbis header packet pointers: PASS");
    return 0;
}
