#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <cctype>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "guest_fios2_diagnostics check failed at line %d\n", line);
        std::abort();
    }
}

#define REQUIRE(condition) Require((condition), __LINE__)

void CheckTimestampFormat(const char* text, std::size_t length) {
    const char* dot = nullptr;
    for (std::size_t index = 0; index < length; ++index) {
        if (text[index] == '.') {
            REQUIRE(dot == nullptr);
            dot = text + index;
        } else {
            REQUIRE(std::isdigit(static_cast<unsigned char>(text[index])) != 0);
        }
    }
    REQUIRE(dot != nullptr && dot != text);
    REQUIRE(static_cast<std::size_t>((text + length) - (dot + 1)) == 6);
}

void CheckTraceTimestamp() {
    void* mapping = nullptr;
    REQUIRE(sceKernelMapFlexibleMemory(&mapping, 2 * PS5_PAGE_SIZE, 3, 0) == 0 && mapping != nullptr);
    auto* boundary = static_cast<char*>(mapping) + PS5_PAGE_SIZE;
    REQUIRE(sceKernelMunmap(reinterpret_cast<std::uint64_t>(boundary), PS5_PAGE_SIZE) == 0);

    auto* smallTail = boundary - 24;
    std::memset(smallTail, 0xa5, 24);
    errno = 71;
    REQUIRE(sceFiosTraceTimestamp(smallTail, 32) == SCE_FIOS_OK && errno == 71);
    const auto* nul = static_cast<const char*>(std::memchr(smallTail, '\0', 24));
    REQUIRE(nul != nullptr);
    const std::size_t len = static_cast<std::size_t>(nul - smallTail);
    REQUIRE(len > 0 && len < 24 && smallTail[len] == '\0');
    CheckTimestampFormat(smallTail, len);
    for (std::size_t index = len + 1; index < 24; ++index) {
        REQUIRE(static_cast<unsigned char>(smallTail[index]) == 0xa5);
    }

    std::array<char, 8> truncBuf{};
    truncBuf.fill('\x5a');
    errno = 71;
    REQUIRE(sceFiosTraceTimestamp(truncBuf.data(), 5) == SCE_FIOS_OK && errno == 71);
    REQUIRE(std::strlen(truncBuf.data()) == 4 && truncBuf[4] == '\0' && truncBuf[5] == '\x5a');

    truncBuf.fill('\x5a');
    errno = 71;
    REQUIRE(sceFiosTraceTimestamp(truncBuf.data(), 1) == SCE_FIOS_OK && errno == 71);
    REQUIRE(truncBuf[0] == '\0' && truncBuf[1] == '\x5a');

    errno = 71;
    REQUIRE(sceFiosTraceTimestamp(boundary - 4, 16) == SCE_FIOS_ERROR_BAD_PTR && errno == 71);
    errno = 71;
    REQUIRE(sceFiosTraceTimestamp(boundary, 16) == SCE_FIOS_ERROR_BAD_PTR && errno == 71);
    errno = 71;
    REQUIRE(sceFiosTraceTimestamp(nullptr, 16) == SCE_FIOS_ERROR_BAD_PTR && errno == 71);
    errno = 71;
    REQUIRE(sceFiosTraceTimestamp(truncBuf.data(), 0) == SCE_FIOS_ERROR_BAD_PTR && errno == 71);

    REQUIRE(sceKernelMunmap(reinterpret_cast<std::uint64_t>(mapping), PS5_PAGE_SIZE) == 0);
}

void CheckCopyDump() {
    void* mapping = nullptr;
    REQUIRE(sceKernelMapFlexibleMemory(&mapping, 2 * PS5_PAGE_SIZE, 3, 0) == 0 && mapping != nullptr);
    auto* boundary = static_cast<char*>(mapping) + PS5_PAGE_SIZE;
    REQUIRE(sceKernelMunmap(reinterpret_cast<std::uint64_t>(boundary), PS5_PAGE_SIZE) == 0);

    std::array<char, 32> buf{};
    buf.fill('\x5a');
    errno = 71;
    REQUIRE(sceFiosDebugDumpError(SCE_FIOS_OK, buf.data(), buf.size()) == buf.data() && errno == 71);
    REQUIRE(std::strcmp(buf.data(), "SCE_FIOS_OK") == 0);
    for (std::size_t index = std::strlen("SCE_FIOS_OK") + 1; index < buf.size(); ++index) {
        REQUIRE(buf[index] == '\x5a');
    }

    std::array<char, 8> truncBuf{};
    truncBuf.fill('\x5a');
    errno = 71;
    REQUIRE(sceFiosDebugDumpError(SCE_FIOS_OK, truncBuf.data(), 5) == truncBuf.data() && errno == 71);
    REQUIRE(std::strcmp(truncBuf.data(), "SCE_") == 0 && truncBuf[5] == '\x5a');

    truncBuf[0] = '\x5a';
    errno = 71;
    REQUIRE(sceFiosDebugDumpError(SCE_FIOS_OK, truncBuf.data(), 0) == truncBuf.data() && errno == 71);
    REQUIRE(truncBuf[0] == '\x5a');

    errno = 71;
    REQUIRE(sceFiosDebugDumpError(SCE_FIOS_OK, boundary - 4, 5) == nullptr && errno == 71);
    errno = 71;
    REQUIRE(sceFiosDebugDumpError(SCE_FIOS_OK, boundary, 16) == nullptr && errno == 71);

    REQUIRE(sceKernelMunmap(reinterpret_cast<std::uint64_t>(mapping), PS5_PAGE_SIZE) == 0);
}

}

int main() {
    std::uint32_t params[16] = {};
    params[0] = (sizeof(params) << 1) | 0u;
    params[3] = 0x80000;
    REQUIRE(sceFiosInitialize(reinterpret_cast<const SceFiosParams*>(params)) == SCE_FIOS_OK);
    CheckTraceTimestamp();
    CheckCopyDump();
    sceFiosTerminate();
    std::puts("guest_fios2_diagnostics ok");
    return 0;
}
