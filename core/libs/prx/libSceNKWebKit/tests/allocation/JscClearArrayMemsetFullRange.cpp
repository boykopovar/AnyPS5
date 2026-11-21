#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

constexpr std::uintptr_t FakeGuestBase = std::uintptr_t{1} << 40;
constexpr std::uint64_t PositiveNanBits = 0x7FF8000000000000ULL;

std::uintptr_t expectedDestination = 0;
std::uintptr_t rangeEnd = 0;
std::uint64_t bytesWritten = 0;
std::uint64_t copyCalls = 0;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "clearArrayMemset full range test failed at line %u\n", location.line());
        std::abort();
    }
}

void ClearFakeRange(unsigned int count) {
    expectedDestination = FakeGuestBase;
    rangeEnd = FakeGuestBase + std::uintptr_t{count} * sizeof(double);
    bytesWritten = 0;
    copyCalls = 0;
    _ZN3JSC16clearArrayMemsetEPdj(reinterpret_cast<double*>(FakeGuestBase), count);
    Check(expectedDestination == rangeEnd);
    Check(bytesWritten == std::uint64_t{count} * sizeof(double));
    Check(copyCalls == (std::uint64_t{count} + 511) / 512);
}

}  // namespace

extern "C" bool GuestCopyOut_nid_no_patch(void* destination, const void* source, std::size_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(destination);
    Check(address == expectedDestination);
    Check(bytes != 0 && bytes % sizeof(double) == 0 && bytes <= rangeEnd - address);
    const auto* values = static_cast<const unsigned char*>(source);
    std::uint64_t first = 0;
    std::uint64_t last = 0;
    std::memcpy(&first, values, sizeof(first));
    std::memcpy(&last, values + bytes - sizeof(last), sizeof(last));
    Check(first == PositiveNanBits && last == PositiveNanBits);
    expectedDestination = address + bytes;
    bytesWritten += bytes;
    ++copyCalls;
    return true;
}

int main() {
    ClearFakeRange(0);
    ClearFakeRange(1);
    ClearFakeRange(512);
    ClearFakeRange(1500);
    ClearFakeRange(UINT_MAX - 511);
    ClearFakeRange(UINT_MAX - 510);
    ClearFakeRange(UINT_MAX);
    return 0;
}
