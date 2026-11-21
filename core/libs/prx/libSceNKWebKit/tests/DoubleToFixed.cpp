#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <algorithm>
#include <array>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using NKWebKit::WtfAbi::DoubleConversionStringBuilder;
using NKWebKit::WtfAbi::DoubleToStringConverter;
constexpr auto toFixed = _ZNK3WTF17double_conversion23DoubleToStringConverter7ToFixedEdiPNS0_13StringBuilderE;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "DoubleToFixed check failed at line %u\n", location.line());
        std::abort();
    }
}

template<typename Exception, typename Function>
void ExpectThrows(Function function, std::source_location location = std::source_location::current()) {
    try {
        function();
    } catch (const Exception&) {
        return;
    }
    Require(false, location);
}

std::string Format(const DoubleToStringConverter* converter, double value, int digits, bool expectedSuccess = true) {
    std::array<char, 256> buffer;
    buffer.fill('#');
    DoubleConversionStringBuilder builder{{buffer.data(), static_cast<int>(buffer.size())}, 3};
    Require(toFixed(converter, value, digits, &builder) == expectedSuccess);
    Require(std::string_view(buffer.data(), 3) == "###");
    Require(builder.position >= 3 && builder.position < static_cast<int>(buffer.size()));
    Require(std::all_of(buffer.begin() + builder.position, buffer.end(), [](char ch) { return ch == '#'; }));
    if (!expectedSuccess)
        Require(builder.position == 3);
    return std::string(buffer.data() + 3, static_cast<std::size_t>(builder.position - 3));
}

void TestFormatting() {
    const auto* ecma = _ZN3WTF17double_conversion23DoubleToStringConverter19EcmaScriptConverterEv();
    Require(Format(ecma, 3.1415, 3) == "3.142");
    Require(Format(ecma, 1234.56789, 4) == "1234.5679");
    Require(Format(ecma, 0.1, 30) == "0.100000000000000005551115123126");
    Require(Format(ecma, 0.1, 100) == "0.1000000000000000055511151231257827021181583404541015625" + std::string(45, '0'));
    Require(Format(ecma, std::nextafter(1e21, 0.0), 0) == "999999999999999868928");
    Require(Format(ecma, -std::nextafter(1e21, 0.0), 2) == "-999999999999999868928.00");

    for (int digits = 0; digits <= 100; ++digits) {
        const auto suffix = digits ? "." + std::string(digits, '0') : "";
        Require(Format(ecma, 0.0, digits) == "0" + suffix);
        Require(Format(ecma, -0.0, digits) == "0" + suffix);
        Require(Format(ecma, std::numeric_limits<double>::denorm_min(), digits) == "0" + suffix);
        Require(Format(ecma, -std::numeric_limits<double>::denorm_min(), digits) == "-0" + suffix);
        Require(Format(ecma, std::numeric_limits<double>::min(), digits) == "0" + suffix);
        Require(Format(ecma, 12.0, digits) == "12" + suffix);
    }

    const int previousRounding = std::fegetround();
    for (int rounding : {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
        Require(std::fesetround(rounding) == 0);
        for (int integer = 0; integer < 32; ++integer) {
            const double midpoint = integer + 0.5;
            Require(Format(ecma, midpoint, 0) == std::to_string(integer + 1));
            Require(Format(ecma, -midpoint, 0) == "-" + std::to_string(integer + 1));
            Require(Format(ecma, std::nextafter(midpoint, 0.0), 0) == std::to_string(integer));
            Require(Format(ecma, std::nextafter(midpoint, 100.0), 0) == std::to_string(integer + 1));
        }
        Require(Format(ecma, 1.25, 1) == "1.3");
        Require(Format(ecma, -1.25, 1) == "-1.3");
        Require(Format(ecma, 0.125, 2) == "0.13");
        Require(Format(ecma, 9.5, 0) == "10");
        Require(Format(ecma, 99.96875, 1) == "100.0");
        Require(Format(ecma, 1.005, 2) == "1.00");
    }
    Require(std::fesetround(previousRounding) == 0);

    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    Require(Format(ecma, infinity, 101) == "Infinity");
    Require(Format(ecma, -infinity, -1) == "-Infinity");
    Require(Format(ecma, -nan, -1) == "NaN");
    Require(Format(ecma, 1e21, 0, false).empty());
    Require(Format(ecma, -1e21, 0, false).empty());
    Require(Format(ecma, std::numeric_limits<double>::max(), 0, false).empty());
    Require(Format(ecma, 1.0, 101, false).empty());
    Require(Format(ecma, 1.0, std::numeric_limits<int>::max(), false).empty());

    DoubleToStringConverter custom = *ecma;
    custom.flags = 0;
    custom.infinitySymbol = "inf";
    custom.nanSymbol = "nan";
    Require(Format(&custom, -0.0, 2) == "-0.00");
    Require(Format(&custom, -infinity, 0) == "-inf");
    Require(Format(&custom, nan, 0) == "nan");
    custom.flags = 2;
    Require(Format(&custom, 12.5, 0) == "13.");
    Require(Format(&custom, -0.0, 0) == "-0.");
    custom.flags = 6;
    Require(Format(&custom, 12.5, 0) == "13.0");
    Require(Format(&custom, 12.5, 2) == "12.50");
    custom.infinitySymbol = nullptr;
    custom.nanSymbol = nullptr;
    Require(Format(&custom, infinity, 0, false).empty());
    Require(Format(&custom, nan, 0, false).empty());
    Require(!toFixed(&custom, infinity, 0, nullptr));
    custom.infinitySymbol = "";
    Require(Format(&custom, infinity, 0).empty());
    Require(Format(&custom, -infinity, 0) == "-");
}

void TestBuffers() {
    const auto* ecma = _ZN3WTF17double_conversion23DoubleToStringConverter19EcmaScriptConverterEv();
    std::array<char, 8> buffer;
    buffer.fill('#');
    DoubleConversionStringBuilder builder{{buffer.data(), 5}, 0};
    Require(toFixed(ecma, 1.25, 2, &builder));
    Require(builder.position == 4 && std::string_view(buffer.data(), buffer.size()) == "1.25####");
    buffer.fill('#');
    builder.position = 0;
    builder.buffer.length = 4;
    ExpectThrows<std::out_of_range>([&] { toFixed(ecma, 1.25, 2, &builder); });
    Require(builder.position == 0 && buffer[0] == '#');
    for (int position : {-1, 4, std::numeric_limits<int>::max()}) {
        builder.position = position;
        ExpectThrows<std::invalid_argument>([&] { toFixed(ecma, 1.0, 0, &builder); });
    }
    builder.position = 0;
    ExpectThrows<std::invalid_argument>([&] { toFixed(ecma, 1.0, -1, &builder); });
    ExpectThrows<std::invalid_argument>([&] { toFixed(ecma, 1.0, std::numeric_limits<int>::min(), &builder); });
    ExpectThrows<std::invalid_argument>([&] { toFixed(nullptr, 1.0, 0, &builder); });
    ExpectThrows<std::invalid_argument>([&] { toFixed(ecma, 1.0, 0, nullptr); });
    const auto invalid = std::uintptr_t{0x10};
    ExpectThrows<std::invalid_argument>([&] { toFixed(reinterpret_cast<const DoubleToStringConverter*>(invalid), 1.0, 0, &builder); });
    ExpectThrows<std::invalid_argument>([&] { toFixed(ecma, 1.0, 0, reinterpret_cast<DoubleConversionStringBuilder*>(invalid)); });
    builder.buffer.start = reinterpret_cast<char*>(invalid);
    ExpectThrows<std::invalid_argument>([&] { toFixed(ecma, 1.0, 0, &builder); });
    Require(builder.position == 0);
    Require(!toFixed(nullptr, 1.0, 101, nullptr));
    Require(!toFixed(nullptr, 1e21, 0, nullptr));

#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const auto pageSize = static_cast<std::size_t>(info.dwPageSize);
    auto* pages = static_cast<char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(pages != nullptr);
    DWORD previous = 0;
    Require(VirtualProtect(pages + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
    const auto pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    auto* pages = static_cast<char*>(mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(pages != MAP_FAILED);
    Require(mprotect(pages + pageSize, pageSize, PROT_NONE) == 0);
#endif
    DoubleToStringConverter custom = *ecma;
    std::memcpy(pages + pageSize - 4, "Inf", 4);
    custom.infinitySymbol = pages + pageSize - 4;
    Require(Format(&custom, -std::numeric_limits<double>::infinity(), 0) == "-Inf");
    pages[pageSize - 1] = 'x';
    ExpectThrows<std::invalid_argument>([&] { Format(&custom, std::numeric_limits<double>::infinity(), 0); });
    builder = {{buffer.data(), 3}, 0};
    ExpectThrows<std::out_of_range>([&] { toFixed(&custom, std::numeric_limits<double>::infinity(), 0, &builder); });
    Require(builder.position == 0 && buffer[0] == '#');

    builder = {{buffer.data(), static_cast<int>(buffer.size())}, 0};
    std::memcpy(pages, &builder, sizeof(builder));
#ifdef _WIN32
    Require(VirtualProtect(pages, pageSize, PAGE_READONLY, &previous) != 0);
#else
    Require(mprotect(pages, pageSize, PROT_READ) == 0);
#endif
    ExpectThrows<std::invalid_argument>([&] { toFixed(ecma, 1.25, 2, reinterpret_cast<DoubleConversionStringBuilder*>(pages)); });
    Require(buffer[0] == '#');
    builder.buffer.start = pages;
    ExpectThrows<std::invalid_argument>([&] { toFixed(ecma, 1.25, 2, &builder); });
    Require(builder.position == 0);
#ifdef _WIN32
    Require(VirtualFree(pages, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(pages, pageSize * 2) == 0);
#endif
}

}  // namespace

int main() {
    TestFormatting();
    TestBuffers();
}
