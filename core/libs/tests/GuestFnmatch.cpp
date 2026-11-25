#include "prx/libc/include/Api.hpp"
#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <clocale>
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

struct Case {
    const char* pattern;
    const char* string;
    int flags;
    int result;
};
static void Require(bool value) {
    if (!value) {
        std::fputs("Guest byte case check failed\n", stderr);
        std::abort();
    }
}

static int FoldCByte(int byte) {
    return byte >= 'A' && byte <= 'Z' ? byte + 'a' - 'A' : byte;
}

static void CheckCaseInsensitiveBytePairs() {
    for (int a = 0; a < 256; ++a) {
        for (int b = 0; b < 256; ++b) {
            char left[] = {static_cast<char>(a), 0};
            char right[] = {static_cast<char>(b), 0};
            const int expected = FoldCByte(a) - FoldCByte(b);
            const int sign = (expected > 0) - (expected < 0);
            Require(strcasecmp_nid_postfix(left, right) == sign);
            Require(strncasecmp_nid_postfix(left, right, 0) == 0);
            Require(strncasecmp_nid_postfix(left, right, 1) == sign);
            Require(strncasecmp_nid_postfix(left, right, 2) == sign);
            const char* match = b == 0 || expected == 0 ? left : nullptr;
            Require(strcasestr_nid_postfix(left, right) == match);
            const char pattern[] = {'\\', static_cast<char>(a), 0};
            Require(fnmatch_nid_postfix(a == 0 ? pattern + 1 : pattern, right, 0x10) == (expected == 0 ? 0 : 1));
            Require(static_cast<unsigned char>(left[0]) == a && left[1] == 0);
            Require(static_cast<unsigned char>(right[0]) == b && right[1] == 0);
        }
    }
    const char left = 'A', right = 'a';
    Require(strncasecmp_nid_postfix(&left, &right, 0) == 0);
    Require(strncasecmp_nid_postfix(&left, &right, 1) == 0);
    const char repeated[] = "AaAaBAaAaC";
    Require(strcasestr_nid_postfix(repeated, "aaac") == repeated + 6);
    Require(strcasestr_nid_postfix(repeated, "aAa") == repeated);
    Require(strcasestr_nid_postfix(repeated, "aaad") == nullptr);
    Require(strcasecmp_nid_postfix("AbC", "aBcD") == -1);
    Require(strncasecmp_nid_postfix("AbC", "aBcD", 3) == 0);
    Require(strncasecmp_nid_postfix("AbC", "aBcD", 4) == -1);
}

static void CheckCaseInsensitiveHostLocale() {
    const char* original = std::setlocale(LC_CTYPE, nullptr);
    Require(original != nullptr);
    struct RestoreLocale {
        std::string name;
        ~RestoreLocale() { Require(std::setlocale(LC_CTYPE, name.c_str()) != nullptr); }
    } restore{original};
    Require(std::setlocale(LC_CTYPE, "C") != nullptr);
    CheckCaseInsensitiveBytePairs();
    bool exercised = false;
    for (const char* name : {".1252", "English_United States.1252", "en_US.ISO8859-1", "de_DE.ISO8859-1",
                             "en_US.iso88591", "tr_TR.ISO8859-9", "Turkish_Turkey.1254", "tr_TR.UTF-8"}) {
        if (!std::setlocale(LC_CTYPE, name)) continue;
        bool changed = false;
        for (int byte = 0; byte < 256; ++byte) changed |= std::tolower(byte) != FoldCByte(byte);
        if (!changed) continue;
        std::fprintf(stderr, "Checking guest byte comparisons with host LC_CTYPE=%s\n", name);
        CheckCaseInsensitiveBytePairs();
        exercised = true;
    }
    if (!exercised) std::fputs("No installed locale changes host byte case folding; host-locale regression skipped\n", stderr);
}

int main() {
    CheckCaseInsensitiveHostLocale();
    static const Case cases[] = {
        {"*.txt", "notes.txt", 0x00, 0},
        {"*.txt", "notes.TXT", 0x00, 1},
        {"*.txt", "notes.TXT", 0x10, 0},
        {"a?c", "abc", 0x00, 0},
        {"a?c", "ac", 0x00, 1},
        {"a[bc]d", "acd", 0x00, 0},
        {"a[!bc]d", "acd", 0x00, 1},
        {"a[^bc]d", "aed", 0x00, 0},
        {"a[a-c]d", "abd", 0x00, 0},
        {"a[a-c]d", "add", 0x00, 1},
        {"a[]]d", "a]d", 0x00, 0},
        {"a[a-]d", "a-d", 0x00, 0},
        {"[A-C]x", "bx", 0x10, 0},
        {"*", "dir/file", 0x00, 0},
        {"*", "dir/file", 0x02, 1},
        {"dir/*", "dir/file", 0x02, 0},
        {"*/file", "dir/file", 0x02, 0},
        {"dir?file", "dir/file", 0x02, 1},
        {"dir[/]file", "dir/file", 0x02, 1},
        {"*", ".hidden", 0x04, 1},
        {".*", ".hidden", 0x04, 0},
        {"?hidden", ".hidden", 0x04, 1},
        {"dir/*", "dir/.hidden", 0x06, 1},
        {"dir/*", "dir/.hidden", 0x04, 0},
        {"dir/.*", "dir/.hidden", 0x06, 0},
        {"dir", "dir/sub/file", 0x08, 0},
        {"dir/*", "dir/sub/file", 0x0a, 0},
        {"dir", "dirx", 0x08, 1},
        {"\\*", "*", 0x00, 0},
        {"\\*", "x", 0x00, 1},
        {"\\*", "\\*", 0x01, 0},
        {"a\\", "a", 0x00, 1},
        {"a\\", "a\\", 0x01, 0},
        {"a*b*c", "axxbyyc", 0x00, 0},
        {"a*b*c", "axxbyy", 0x00, 1},
        {"*a*", "bbb", 0x00, 1},
        {"**a", "ba", 0x00, 0},
        {"", "", 0x00, 0},
        {"", "x", 0x00, 1},
        {"x", "", 0x00, 1},
        {"[", "[", 0x00, 0},
        {"[a", "[a", 0x00, 0},
        {"a[", "a[", 0x00, 0},
        {"[!]x", "x", 0x00, 1},
        {"*/", "dir/", 0x02, 0},
        {"*/", "dir", 0x02, 1},
        {"[[:digit:]]", "7", 0x00, 1},
        {"[[:digit:]]", "d]", 0x00, 0},
        {"[[:digit:]]", ":]", 0x00, 0},
        {"a[[:alpha:]]", "ab", 0x00, 1},
    };
    for (const Case& entry : cases) {
        if (fnmatch_nid_postfix(entry.pattern, entry.string, entry.flags) != entry.result) {
            std::fprintf(stderr, "fnmatch(\"%s\", \"%s\", 0x%x) != %d\n", entry.pattern, entry.string, entry.flags, entry.result);
            std::abort();
        }
    }
    bool threw = false;
    try {
        fnmatch_nid_postfix(nullptr, "x", 0);

    } catch (const std::invalid_argument&) {
        threw = true;
    }
    if (!threw) std::abort();
    const char text[] = "Hello World";
    Require(strcasestr_nid_postfix(text, "world") == text + 6);
    Require(strcasestr_nid_postfix(text, "") == text);
    Require(strcasestr_nid_postfix("", "x") == nullptr);
    const char* badText = reinterpret_cast<const char*>(std::uintptr_t(1));
    const char* badNeedle = reinterpret_cast<const char*>(~std::uintptr_t(0xFFF));
    errno = EINVAL;
    Require(strcasestr_nid_postfix(badText, "world") == nullptr);
    Require(errno == EINVAL);
    errno = EINVAL;
    Require(strcasestr_nid_postfix(badText, "") == nullptr);
    Require(errno == EINVAL);
    errno = EINVAL;
    Require(strcasestr_nid_postfix(text, badNeedle) == nullptr);
    Require(errno == EINVAL);
    Require(strcasestr_nid_postfix(nullptr, "world") == nullptr);
    Require(strcasestr_nid_postfix("Hello", nullptr) == nullptr);

    struct MappedPages {
        char* data = nullptr;
        std::size_t pageSize = 0;
        std::size_t pageCount = 0;

        explicit MappedPages(std::size_t count) : pageCount(count) {
#ifdef _WIN32
            SYSTEM_INFO info{};
            GetSystemInfo(&info);
            pageSize = info.dwPageSize != 0 ? static_cast<std::size_t>(info.dwPageSize) : static_cast<std::size_t>(4096);
            data = static_cast<char*>(VirtualAlloc(nullptr, pageSize * pageCount, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Require(data != nullptr);
#else
            const long value = ::sysconf(_SC_PAGESIZE);
            Require(value > 0);
            pageSize = static_cast<std::size_t>(value);
            void* mapping = ::mmap(nullptr, pageSize * pageCount, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            Require(mapping != MAP_FAILED);
            data = static_cast<char*>(mapping);
#endif
        }

        ~MappedPages() {
            if (data == nullptr) return;
#ifdef _WIN32
            VirtualFree(data, 0, MEM_RELEASE);
#else
            ::munmap(data, pageSize * pageCount);
#endif
        }

        void ProtectNoAccess(std::size_t pageIndex) {
            Require(pageIndex < pageCount);
#ifdef _WIN32
            DWORD previous = 0;
            Require(VirtualProtect(data + pageIndex * pageSize, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
            Require(::mprotect(data + pageIndex * pageSize, pageSize, PROT_NONE) == 0);
#endif
        }

        MappedPages(const MappedPages&) = delete;
        MappedPages& operator=(const MappedPages&) = delete;
    };

    std::string longText(18000, 'a');
    const char longToken[] = "ToKeN_AfTeR_16K";
    std::memcpy(longText.data() + 17000, longToken, sizeof(longToken) - 1);
    longText[17500] = 'Z';
    Require(strcasestr_nid_postfix(longText.c_str(), "token_after_16k") == longText.c_str() + 17000);
    Require(strcasestr_nid_postfix(longText.c_str(), "z") == longText.c_str() + 17500);
    Require(strcasestr_nid_postfix(longText.c_str(), "not_present_in_long_text") == nullptr);

    {
        MappedPages boundary(2);
        std::memset(boundary.data, 'x', boundary.pageSize * 2);
        const char cross[] = "CrOsSpAgE";
        const std::size_t crossOffset = boundary.pageSize - 4;
        std::memcpy(boundary.data + crossOffset, cross, sizeof(cross) - 1);
        boundary.data[boundary.pageSize + 64] = '\0';
        Require(strcasestr_nid_postfix(boundary.data, "crosspage") == boundary.data + crossOffset);
        Require(strcasestr_nid_postfix(boundary.data + 17, "CROSSPAGE") == boundary.data + crossOffset);

        std::memset(boundary.data, 'y', boundary.pageSize + 32);
        std::memcpy(boundary.data + boundary.pageSize - 4, "aaaa", 4);
        std::memcpy(boundary.data + boundary.pageSize, "aB", 2);
        boundary.data[boundary.pageSize + 2] = '\0';
        Require(strcasestr_nid_postfix(boundary.data, "AAAB") == boundary.data + boundary.pageSize - 2);
    }

    {
        MappedPages multiPage(3);
        const std::size_t longNeedleLen = std::max<std::size_t>(5000, multiPage.pageSize + 100);
        const std::size_t needleStart = multiPage.pageSize - 50;
        Require(needleStart + longNeedleLen + 1 <= multiPage.pageSize * 3);
        std::memset(multiPage.data + needleStart, 'b', longNeedleLen);
        std::memcpy(multiPage.data + needleStart, "HeAd", 4);
        std::memcpy(multiPage.data + needleStart + longNeedleLen / 2, "MiDdLe", 6);
        std::memcpy(multiPage.data + needleStart + longNeedleLen - 4, "TaIl", 4);
        multiPage.data[needleStart + longNeedleLen] = '\0';
        const char* pageSpannedNeedle = multiPage.data + needleStart;

        std::string nearMatch(pageSpannedNeedle, longNeedleLen);
        nearMatch.back() = 'x';
        std::string swappedCase(pageSpannedNeedle, longNeedleLen);
        for (char& ch : swappedCase) {
            if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
            else if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
        }
        std::string longHaystack = std::string(500, 'q') + nearMatch + std::string(300, 'r') + swappedCase + std::string(200, 's');
        const std::size_t expectedOffset = 500 + nearMatch.size() + 300;
        Require(strcasestr_nid_postfix(longHaystack.c_str(), pageSpannedNeedle) == longHaystack.c_str() + expectedOffset);

        multiPage.data[needleStart + longNeedleLen - 1] = 'z';
        Require(strcasestr_nid_postfix(longHaystack.c_str(), pageSpannedNeedle) == nullptr);
    }

    {
        MappedPages guarded(2);
        std::memset(guarded.data, 'a', guarded.pageSize);
        const char tailToken[] = "TaRgEt";
        const std::size_t tailOffset = guarded.pageSize - (sizeof(tailToken) - 1);
        std::memcpy(guarded.data + tailOffset, tailToken, sizeof(tailToken) - 1);
        guarded.ProtectNoAccess(1);

        errno = EINVAL;
        Require(strcasestr_nid_postfix(guarded.data, "target") == guarded.data + tailOffset);
        Require(errno == EINVAL);

        errno = EINVAL;
        Require(strcasestr_nid_postfix(guarded.data, "missing") == nullptr);
        Require(errno == EINVAL);

        errno = EINVAL;
        Require(strcasestr_nid_postfix(guarded.data, "targetx") == nullptr);
        Require(errno == EINVAL);

        errno = EINVAL;
        Require(strcasestr_nid_postfix(guarded.data + guarded.pageSize, "") == nullptr);
        Require(errno == EINVAL);

        errno = EINVAL;
        Require(strcasestr_nid_postfix("abcdef", guarded.data + guarded.pageSize - 3) == nullptr);
        Require(errno == EINVAL);

        guarded.data[guarded.pageSize - 1] = '\0';
        errno = EINVAL;
        Require(strcasestr_nid_postfix(guarded.data, "missing") == nullptr);
        Require(errno == EINVAL);
        Require(strcasestr_nid_postfix(guarded.data + guarded.pageSize - 1, "") == guarded.data + guarded.pageSize - 1);

        const auto expectFnmatchInvalid = [](const char* pattern, const char* text, int flags) {
            errno = EBUSY;
            bool rejected = false;
            try {
                fnmatch_nid_postfix(pattern, text, flags);
            } catch (const std::invalid_argument&) {
                rejected = true;
            }
            Require(rejected && errno == EBUSY);
        };

        expectFnmatchInvalid(nullptr, "x", 0);
        expectFnmatchInvalid("x", nullptr, 0);
        expectFnmatchInvalid(badText, "notes.txt", 0);
        expectFnmatchInvalid("*.txt", badText, 0);
        expectFnmatchInvalid(badNeedle, "notes.txt", 0);
        expectFnmatchInvalid("*.txt", badNeedle, 0);
        expectFnmatchInvalid(guarded.data + guarded.pageSize, "notes.txt", 0);
        expectFnmatchInvalid("*.txt", guarded.data + guarded.pageSize, 0);

        char* boundaryPattern = guarded.data + guarded.pageSize - 6;
        std::memcpy(boundaryPattern, "*.txt", 6);
        Require(fnmatch_nid_postfix(boundaryPattern, "notes.txt", 0) == 0);
        char* boundaryString = guarded.data + guarded.pageSize - 10;
        std::memcpy(boundaryString, "notes.TXT", 10);
        Require(fnmatch_nid_postfix("*.txt", boundaryString, 0x10) == 0);

        char* unterminated = guarded.data + guarded.pageSize - 4;
        std::memcpy(unterminated, "*.tx", 4);
        expectFnmatchInvalid(unterminated, "notes.txt", 0);
        expectFnmatchInvalid("*.txt", unterminated, 0);

        const std::string longString(6000, 'm');
        const std::string longPattern = "*" + longString;
        Require(fnmatch_nid_postfix(longPattern.c_str(), longString.c_str(), 0) == 0);
        Require(fnmatch_nid_postfix(longPattern.c_str(), "other", 0) == 1);
        Require(fnmatch_nid_postfix(longString.c_str(), longString.c_str(), 0) == 0);
    }
    return 0;
}
