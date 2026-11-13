#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include "prx/libkernel/libkernel.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string_view>

namespace {

using IcuRecoveredTest::Require;

void OpenMapped() {
    constexpr std::size_t page = 0x4000;
    constexpr std::size_t bytes = page * 3;
    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, bytes, 0, 0, &physical) == 0, "allocate pattern fixture memory");
    void* address = nullptr;
    Require(sceKernelMapDirectMemory(&address, bytes, 3, 0, physical, 0) == 0, "map pattern fixture memory");
    auto* data = static_cast<std::uint8_t*>(address);
    std::fill_n(data, bytes, 0xa5);
    Require(sceKernelMprotect(data, page, 0) == 0, "protect pattern fixture first guard");
    Require(sceKernelMprotect(data + page * 2, page, 0) == 0, "protect pattern fixture last guard");
    auto* locale = reinterpret_cast<char*>(data + page + 64);
    auto* status = reinterpret_cast<std::int32_t*>(data + page + 96);
    auto* skeleton = reinterpret_cast<char16_t*>(data + page + 128);
    auto* output = reinterpret_cast<char16_t*>(data + page + 256);
    std::memcpy(locale, "en_US", 6);
    std::memcpy(skeleton, u"yMd", 8);
    *status = 0;
    std::fputs("pattern fixture: valid mapped udatpg_open\n", stderr);
    auto* handle = udatpg_open_nid_postfix(locale, status);
    Require(handle != nullptr && *status <= 0, "valid mapped locale opens a usable pattern generator");
    const auto count = udatpg_getBestPattern_nid_postfix(handle, skeleton, 3, output + 1, 32, status);
    Require(count == 5 && *status <= 0, "mapped yMd pattern has its required length");
    Require(std::u16string_view(output + 1, 5) == u"M/d/y" && output[6] == 0, "mapped en_US yMd pattern and terminator");
    Require(output[0] == 0xa5a5 && output[7] == 0xa5a5 && output[33] == 0xa5a5, "mapped pattern output guards remain intact");
    udatpg_close_nid_postfix(handle);
    udatpg_close_nid_postfix(nullptr);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(data), bytes) == 0, "unmap pattern fixture memory");
    Require(sceKernelReleaseDirectMemory(physical, bytes) == 0, "release pattern fixture memory");
}

} // namespace

int main() {
    try {
        OpenMapped();
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "pattern fixture failure: %s\n", error.what());
        return 1;
    }
}
