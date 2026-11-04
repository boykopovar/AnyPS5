#include "prx/libc/include/Api.hpp"
#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <clocale>
#include <cctype>
#include <string>

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
    return 0;
}
