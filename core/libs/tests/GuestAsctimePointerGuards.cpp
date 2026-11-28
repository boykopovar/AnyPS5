#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestTime.hpp"
#include "prx/libSceIme/include/ImeFilterText.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "asctime pointer guard failed: %s\n", message);
        std::abort();
    }
}

class Pages {
    unsigned char* allocation;
    std::size_t size;
public:
    Pages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        allocation = static_cast<unsigned char*>(VirtualAlloc(nullptr, size * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(allocation != nullptr, "allocate pages");
        DWORD previous;
        Require(VirtualProtect(allocation + size, size, PAGE_NOACCESS, &previous) != 0, "protect page");
#else
        const long page = sysconf(_SC_PAGESIZE);
        Require(page > 0, "page size");
        size = static_cast<std::size_t>(page);
        allocation = static_cast<unsigned char*>(mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(allocation != MAP_FAILED, "allocate pages");
        Require(mprotect(allocation + size, size, PROT_NONE) == 0, "protect page");
#endif
        std::memset(allocation, 0xa5, size);
        char value = 0;
        Require(!GuestCopyIn_nid_no_patch(&value, allocation + size, 1), "qualify unreadable page");
        Require(!GuestCopyOut_nid_no_patch(allocation + size, &value, 1), "qualify unwritable page");
    }
    ~Pages() {
#ifdef _WIN32
        Require(VirtualFree(allocation, 0, MEM_RELEASE) != 0, "release pages");
#else
        Require(munmap(allocation, size * 2) == 0, "release pages");
#endif
    }
    unsigned char* End() { return allocation + size; }
    void ReadOnly() {
#ifdef _WIN32
        DWORD previous;
        Require(VirtualProtect(allocation, size, PAGE_READONLY, &previous) != 0, "read-only page");
#else
        Require(mprotect(allocation, size, PROT_READ) == 0, "read-only page");
#endif
    }
};

GuestTm Time() {
    GuestTm result{};
    result.tm_year = 126;
    result.tm_mon = 9;
    result.tm_mday = 2;
    result.tm_hour = 13;
    result.tm_min = 5;
    result.tm_sec = 9;
    result.tm_wday = 5;
    return result;
}

void FilterTextPointerGuards() {
    char16_t overlap[10] = {u'a', u'1', u'b', u'2', u'c', u'3', 0, u'#', u'#', u'#'};
    std::int32_t out_len = -1;
    const ime_filter::FilterParam ascii_param{0, 1, 0, 0};
    errno = 73;
    *__error_nid_postfix() = 73;
    Require(ime_filter::FilterText(overlap + 1, &out_len, 8, &ascii_param, overlap, 6) == 0, "filter overlap success");
    Require(out_len == 6 && overlap[0] == u'a', "filter overlap length and prefix");
    Require(std::u16string_view(overlap + 1) == u"a1b2c3" && overlap[8] == u'#', "filter overlap output");
    Require(errno == 73 && *__error_nid_postfix() == 73, "filter overlap errno");

    Require(!ime_filter::InCodepageA(0x10000u), "filter InCodepageA 0x10000");
    Require(!ime_filter::InCodepageA(0xffffffffu), "filter InCodepageA 0xffffffff");

    Pages pages;
    std::array<char16_t, 16> dst;
    const char16_t valid_src[] = u"abc";
    auto check_fault = [&](auto&& fn, const char* msg) {
        dst.fill(u'#');
        out_len = -12345;
        errno = 73;
        *__error_nid_postfix() = 73;
        Require(fn() == ime_filter::kErrorInvalidAddress, msg);
        Require(errno == 73 && *__error_nid_postfix() == 73, msg);
        for (char16_t ch : dst) Require(ch == u'#', msg);
        Require(out_len == -12345, msg);
    };

    check_fault([&] {
        return ime_filter::FilterText(dst.data(), &out_len, static_cast<std::uint32_t>(dst.size()),
                                      reinterpret_cast<const ime_filter::FilterParam*>(pages.End()), valid_src, 3);
    }, "filter unmapped param");

    std::memcpy(pages.End() - sizeof(ime_filter::FilterParam) / 2, &ascii_param, sizeof(ime_filter::FilterParam) / 2);
    check_fault([&] {
        return ime_filter::FilterText(dst.data(), &out_len, static_cast<std::uint32_t>(dst.size()),
                                      reinterpret_cast<const ime_filter::FilterParam*>(pages.End() - sizeof(ime_filter::FilterParam) / 2),
                                      valid_src, 3);
    }, "filter straddling param");

    check_fault([&] {
        return ime_filter::FilterText(dst.data(), &out_len, static_cast<std::uint32_t>(dst.size()),
                                      &ascii_param, reinterpret_cast<const char16_t*>(pages.End()), 4);
    }, "filter unmapped src");

    const char16_t partial_src[2] = {u'a', u'b'};
    char16_t* straddle_src = reinterpret_cast<char16_t*>(pages.End()) - 2;
    std::memcpy(straddle_src, partial_src, sizeof(partial_src));
    check_fault([&] {
        return ime_filter::FilterText(dst.data(), &out_len, static_cast<std::uint32_t>(dst.size()),
                                      &ascii_param, straddle_src, 4);
    }, "filter straddling src");

    std::memset(pages.End() - 4, 0xa5, 4);
    check_fault([&] {
        return ime_filter::FilterText(reinterpret_cast<char16_t*>(pages.End()) - 2, &out_len, 8,
                                      &ascii_param, valid_src, 3);
    }, "filter straddling dst");
    for (int i = -4; i < 0; ++i) Require(pages.End()[i] == 0xa5, "filter straddling dst no partial write");

    check_fault([&] {
        return ime_filter::FilterText(dst.data(), reinterpret_cast<std::int32_t*>(pages.End()),
                                      static_cast<std::uint32_t>(dst.size()), &ascii_param, valid_src, 3);
    }, "filter unmapped out_length");

    std::memset(pages.End() - 2, 0xa5, 2);
    check_fault([&] {
        return ime_filter::FilterText(dst.data(), reinterpret_cast<std::int32_t*>(pages.End() - 2),
                                      static_cast<std::uint32_t>(dst.size()), &ascii_param, valid_src, 3);
    }, "filter straddling out_length");
    for (int i = -2; i < 0; ++i) Require(pages.End()[i] == 0xa5, "filter straddling out_length no partial write");

    Pages ro_pages;
    const std::int32_t ro_canary = -12345;
    std::memcpy(ro_pages.End() - sizeof(ro_canary), &ro_canary, sizeof(ro_canary));
    ro_pages.ReadOnly();

    check_fault([&] {
        return ime_filter::FilterText(reinterpret_cast<char16_t*>(ro_pages.End() - 16), &out_len, 8,
                                      &ascii_param, valid_src, 3);
    }, "filter read-only dst");

    check_fault([&] {
        return ime_filter::FilterText(dst.data(), reinterpret_cast<std::int32_t*>(ro_pages.End() - sizeof(ro_canary)),
                                      static_cast<std::uint32_t>(dst.size()), &ascii_param, valid_src, 3);
    }, "filter read-only out_length");

    const char16_t exact_src[4] = {u'x', u'y', u'z', 0};
    char16_t* flush_src = reinterpret_cast<char16_t*>(pages.End()) - 4;
    std::memcpy(flush_src, exact_src, sizeof(exact_src));
    dst.fill(u'#');
    out_len = -12345;
    errno = 73;
    *__error_nid_postfix() = 73;
    Require(ime_filter::FilterText(dst.data(), &out_len, static_cast<std::uint32_t>(dst.size()),
                                   &ascii_param, flush_src, 1000) == 0,
            "filter flush NUL src");
    Require(out_len == 3 && std::u16string_view(dst.data()) == u"xyz" && dst[4] == u'#', "filter flush NUL output");
    Require(errno == 73 && *__error_nid_postfix() == 73, "filter flush NUL errno");

    dst.fill(u'#');
    out_len = -12345;
    Require(ime_filter::FilterText(dst.data(), &out_len, 1, &ascii_param, valid_src, 3) == ime_filter::kErrorInvalidParam,
            "filter capacity < 2");
    Require(dst[0] == u'#' && out_len == -12345, "filter capacity < 2 canary");

    const ime_filter::FilterParam bad_type{0, 5, 0, 0};
    Require(ime_filter::FilterText(dst.data(), &out_len, static_cast<std::uint32_t>(dst.size()),
                                   &bad_type, valid_src, 3) == ime_filter::kErrorInvalidParam,
            "filter invalid type");
    Require(dst[0] == u'#' && out_len == -12345, "filter invalid type canary");

    Require(ime_filter::FilterText(dst.data(), &out_len, 0x40000000u, &ascii_param, valid_src, 2) == 0,
            "filter huge capacity short src");
    Require(out_len == 2 && std::u16string_view(dst.data()) == u"ab" && dst[3] == u'#',
            "filter huge capacity short src output");

    std::array<char16_t, 516> long_filtered_src;
    long_filtered_src.fill(char16_t{0x3042});
    long_filtered_src[512] = u'a';
    long_filtered_src[513] = u'b';
    long_filtered_src[514] = u'c';
    long_filtered_src[515] = 0;
    dst.fill(u'#');
    out_len = -12345;
    Require(ime_filter::FilterText(dst.data(), &out_len, 4, &ascii_param, long_filtered_src.data(), 515) == 0,
            "filter long filtered prefix reaches kept suffix");
    Require(out_len == 3 && std::u16string_view(dst.data()) == u"abc" && dst[4] == u'#',
            "filter long filtered prefix output");

    char16_t* const guard_prefix_src = reinterpret_cast<char16_t*>(pages.End()) - 64;
    for (int idx = 0; idx < 64; ++idx) guard_prefix_src[idx] = char16_t{0x3042};
    check_fault([&] {
        return ime_filter::FilterText(dst.data(), &out_len, 8, &ascii_param, guard_prefix_src, 1000);
    }, "filter long filtered prefix hitting guard page");
}

void Valid() {
    auto time = Time();
    std::array<char, 64> output;
    output.fill('#');
    *__error_nid_postfix() = 73;
    Require(asctime_r_nid_postfix(&time, output.data()) == output.data(), "valid return pointer");
    Require(std::strcmp(output.data(), "Fri Oct  2 13:05:09 2026\n") == 0, "valid formatting");
    Require(*__error_nid_postfix() == 73 && output[26] == '#', "valid errno and canary");
    Require(std::strcmp(asctime_nid_postfix(&time), output.data()) == 0, "thread-local alias");
    output.fill('#');
    time.tm_wday = 100;
    Require(asctime_r_nid_postfix(&time, output.data()) == output.data(), "empty name return");
    const auto used = std::strlen(output.data()) + 1;
    for (std::size_t index = used; index < output.size(); ++index)
        Require(output[index] == '#', "short formatted output preserves tail");
    *__error_nid_postfix() = 73;
    Require(asctime_r_nid_postfix(nullptr, output.data()) == output.data(), "null time return");
    Require(std::strcmp(output.data(), "??? ??? ?? ??:??:?? ????\n") == 0 && *__error_nid_postfix() == 22, "null time policy");
    Pages pages;
    bool threw = false;
    try { asctime_r_nid_postfix(reinterpret_cast<const GuestTm*>(pages.End()), nullptr); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw, "null output precedes unreadable time");
    time = Time();
    std::memcpy(pages.End() - 32, &time, 28);
    Require(asctime_r_nid_postfix(reinterpret_cast<const GuestTm*>(pages.End() - 32), output.data()) == output.data(), "only formatter fields are read");
    Require(std::strcmp(output.data(), "Fri Oct  2 13:05:09 2026\n") == 0, "boundary input formatting");
    FilterTextPointerGuards();
}

}

int main(int argc, char** argv) {
    Require(argc == 2, "test mode");
    const std::string_view mode = argv[1];
    if (mode == "valid") {
        Valid();
        return 0;
    }
    Pages pages;
    auto time = Time();
    std::array<char, 32> output;
    output.fill('#');
    if (mode == "read") {
        std::memcpy(pages.End() - 24, &time, 24);
        std::fputs("asctime read guard qualified=1\n", stderr);
        std::fflush(stderr);
        *__error_nid_postfix() = 73;
        Require(asctime_r_nid_postfix(reinterpret_cast<const GuestTm*>(pages.End() - 24), output.data()) == nullptr, "unreadable input return");
        Require(*__error_nid_postfix() == 14, "unreadable input errno");
        for (char value : output) Require(value == '#', "unreadable input leaves output untouched");
    } else if (mode == "write" || mode == "readonly") {
        char* destination = reinterpret_cast<char*>(pages.End() - (mode == "write" ? 25 : 32));
        if (mode == "readonly") pages.ReadOnly();
        const std::array<char, 26> probe{};
        Require(!GuestCopyOut_nid_no_patch(destination, probe.data(), probe.size()), "qualify output span");
        std::fputs("asctime write guard qualified=1\n", stderr);
        std::fflush(stderr);
        *__error_nid_postfix() = 73;
        Require(asctime_r_nid_postfix(&time, destination) == nullptr, "unwritable output return");
        Require(*__error_nid_postfix() == 14, "unwritable output errno");
    } else {
        Require(false, "unknown mode");
    }
}
