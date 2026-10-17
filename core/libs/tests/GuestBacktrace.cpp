#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" {
int APS5_VABI sceLibcBacktraceGetBufferSize_nid_postfix(int, int*, std::size_t*);
int APS5_VABI sceLibcBacktraceSelf_nid_postfix(int, void*, std::size_t, int*);
}

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stdout, "%s\n", message);
    std::abort();
}

int main() {
    Require(sceLibcBacktraceGetBufferSize_nid_postfix(8, nullptr, nullptr) == 1, "null size outputs accepted");
    Require(sceLibcBacktraceSelf_nid_postfix(8, nullptr, 0, nullptr) == 1, "null self outputs accepted");

    int expectedCount = 0;
    std::size_t expectedSize = 0;
    Require(sceLibcBacktraceGetBufferSize_nid_postfix(8, &expectedCount, &expectedSize) == 0, "buffer size failed");
    Require(expectedCount > 0 && expectedSize >= expectedCount * 0x18, "buffer size was not populated");

    std::vector<std::uint8_t> buffer(expectedSize);
    int count = 0;
    Require(sceLibcBacktraceSelf_nid_postfix(8, buffer.data(), buffer.size(), &count) == 0, "self backtrace failed");
    Require(count > 0 && count <= 8, "self count was invalid");

    std::size_t offset = 0;
    for (int index = 0; index < count; ++index) {
        Require(offset + 0x18 <= buffer.size(), "backtrace record header exceeded buffer");
        std::uintptr_t next = 0;
        std::memcpy(&next, buffer.data() + offset, sizeof(next));
        const auto* text = reinterpret_cast<const char*>(buffer.data() + offset + 0x10);
        const auto length = std::strlen(text);
        Require(length > 0 && length <= 255, "backtrace record text was invalid");
        const auto size = 0x18 + ((length + 7) & ~std::size_t(7));
        Require(offset + size <= buffer.size(), "backtrace record exceeded buffer");
        if (index + 1 == count) Require(next == 0, "last backtrace record was linked");
        else Require(next == reinterpret_cast<std::uintptr_t>(buffer.data() + offset + size), "backtrace records were not linked");
        offset += size;
    }
}
