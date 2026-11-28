#include "prx/libfontconfig/libfontconfig.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static const FcChar8* Text(const char* text) {
    return reinterpret_cast<const FcChar8*>(text);
}

static void Check(bool value, int line) {
    if (value) return;
    std::fprintf(stderr, "Fontconfig format check failed at line %d\n", line);
    std::abort();
}
#define CHECK(value) Check((value), __LINE__)

static void Expect(FcPattern* pattern, const char* format, const char* expected) {
    auto* result = FcPatternFormat(pattern, Text(format));
    if (!result || std::strcmp(reinterpret_cast<char*>(result), expected)) {
        std::fprintf(stderr, "format [%s]: expected [%s], got [%s]\n", format, expected,
            result ? reinterpret_cast<char*>(result) : "NULL");
        std::abort();
    }
    free_nid_postfix(result);
}

int main() {
    const std::array<void*, 10> heap{};
    ApplicationHeapRegister_nid_no_patch(heap.data());
    auto* pattern = FcPatternCreate();
    CHECK(pattern);
    CHECK(FcPatternAddString(pattern, "family", Text("DejaVu Sans")));
    CHECK(FcPatternAddString(pattern, "family", Text("Other")));
    CHECK(FcPatternAddString(pattern, "style", Text("Book")));
    CHECK(FcPatternAddString(pattern, "file", Text("/fonts/Test.ttf")));
    CHECK(FcPatternAddInteger_nid_postfix(pattern, "index", 2));
    CHECK(FcPatternAddInteger_nid_postfix(pattern, "weight", 80));
    CHECK(FcPatternAddDouble(pattern, "size", 12.5));
    CHECK(FcPatternAddBool_nid_postfix(pattern, "antialias", 1));
    const FcMatrix matrix{1, 2, 3, 4};
    CHECK(FcPatternAddMatrix(pattern, "matrix", &matrix));
    Expect(pattern, "", "");
    Expect(pattern, "plain \\{text\\} %% \\n\\r\\t\\a\\b\\f\\v\\q\\", "plain {text} % \n\r\t\a\b\f\vq");
    Expect(pattern, "%{family} %{style}\\n", "DejaVu Sans,Other Book\n");
    Expect(pattern, "%{family[0]}|%{family[1]}|%{family[2]}", "DejaVu Sans|Other|");
    Expect(pattern, "%{family[]}", "DejaVu Sans");
    Expect(pattern, "%{family[+1]}", "Other");
    Expect(pattern, "%{family[2]:-fallback}|%{missing:-fallback}", "fallback|fallback");
    Expect(pattern, "%{:weight=}|%{:missing=}|%{:missing=:-123}", ":weight=80||:missing=123");
    Expect(pattern, "%{#family}|%{#style}|%{#missing}", "2|1|0");
    Expect(pattern, "%{?family,style,!missing{pass}{fail}}", "pass");
    Expect(pattern, "%{?missing{pass}{fail}}", "fail");
    Expect(pattern, "%{?family,!style{pass}{fail}}", "fail");
    Expect(pattern, "%{?missing{ignored}}x", "x");
    Expect(pattern, "%{?family{yes}}", "yes");
    Expect(pattern, "%{?family{yes}{%{unknown|bad}}}", "yes");
    Expect(pattern, "%{[]family,style{%{family} (%{style})\\n}}", "DejaVu Sans (Book)\nOther ()\n");
    Expect(pattern, "%{[]missing{unused}}end", "end");
    Expect(pattern, "%{[]missing}end", "end");
    Expect(pattern, "%4294967298{{x}}|%-4294967298{{x}}|%4294967296{{x}}", " x|x |x");
    Expect(pattern, "a%{[]missing,absent}b%{[]missing}c", "abc");
    Expect(pattern, "%{family[4294967296]}|%{family[4294967297]}|%{family[4294967298]}", "DejaVu Sans|Other|");
    Expect(pattern, "%{+family,style{%{family}/%{style}/%{weight}}}", "DejaVu Sans,Other/Book/");
    Expect(pattern, "%{-family,style{%{family}/%{style}/%{weight}}}", "//80");
    Expect(pattern, "%{family}", "DejaVu Sans,Other");
    Expect(pattern, "%{+family,style{%{=unparse}}}", "DejaVu Sans,Other:style=Book");
    Expect(pattern, "%{=fcmatch}", "Test.ttf: \"DejaVu Sans\" \"Book\"");
    Expect(nullptr, "%{=fcmatch}", "<unknown filename>: \"<unknown family>\" \"<unknown style>\"");
    Expect(pattern, "%{+family,style,file{%{=fclist}}}", "/fonts/Test.ttf: DejaVu Sans,Other:style=Book");
    Expect(pattern, "%{+family,file,index{%{=fccat}}}", "\"Test.ttf\" 2 \"DejaVu Sans,Other:index=2\"");
    Expect(pattern, "%{=pkgkit}", "font(dejavusans)\nfont(other)\n");
    Expect(pattern, "%{file|basename}|%{file|dirname}", "Test.ttf|/fonts");
    Expect(nullptr, "%{missing:-file|dirname}", ".");
    Expect(nullptr, "%{missing:-/file|dirname}", "");
    Expect(nullptr, "%{missing:-/dir/|basename}", "");
    Expect(nullptr, "%{missing:-a\\\\b.ttf|basename}|%{missing:-a\\\\b.ttf|dirname}", "a\\b.ttf|.");
    auto* backslashes = FcPatternCreate();
    CHECK(backslashes);
    CHECK(FcPatternAddString(backslashes, "file", Text("/fonts\\a/b\\c.ttf")));
    Expect(backslashes, "%{file|basename}|%{file|dirname}", "b\\c.ttf|/fonts\\a");
    FcPatternDestroy_nid_postfix(backslashes);
    Expect(pattern, "%{family|downcase|delete( )}", "dejavusans,other");
    Expect(pattern, "%{family|translate(aeiou,X)}", "DXjXVX SXns,OthXr");
    Expect(pattern, "%{family|escape(_ )}", "DejaVu_ Sans,Other");
    Expect(nullptr, "%{x:-A'B|shescape}", "'A'\\''B'");
    Expect(nullptr, "%{x:-A\"B\\\\C|cescape}", "A\\\"B\\\\C");
    Expect(nullptr, "%{x:-<&>\"'|xmlescape}", "&lt;&amp;&gt;\"'");
    Expect(pattern, "%8{style}|%-8{style}|%2{style}", "    Book|Book    |Book");
    Expect(pattern, "%20{{%{family[1]} %{style}}}", "          Other Book");
    Expect(pattern, "%{size}|%{antialias}|%{matrix}", "12.5|True|1 2 3 4");
    Expect(nullptr, "%{#family}|%{family}|%{family:-none}", "0||none");
    Expect(nullptr, "%{=unparse}", "");
    CHECK(FcPatternAddString(pattern, "custom", Text("Straße ÄÖΣ")));
    Expect(pattern, "%{custom|downcase}", "strasse äöσ");
    CHECK(FcPatternAddString(pattern, "custom2", Text("a-b:c,d=e_f\\g")));
    Expect(pattern, "%{custom2}", "a-b:c,d=e_f\\g");
    auto* charset = FcCharSetCreate();
    CHECK(charset);
    for (auto point : {0x20u, 0x21u, 0x22u, 0x41u, 0x100u}) CHECK(FcCharSetAddChar(charset, point));
    CHECK(FcPatternAddCharSet(pattern, "charset", charset));
    FcCharSetDestroy(charset);
    Expect(pattern, "%{charset}", "20-22 41 100");
    auto* languages = FcLangSetCreate();
    CHECK(languages);
    CHECK(FcLangSetAdd(languages, Text("en")));
    CHECK(FcLangSetAdd(languages, Text("fr")));
    CHECK(FcLangSetAdd(languages, Text("x-private")));
    CHECK(FcPatternAddLangSet(pattern, "lang", languages));
    FcLangSetDestroy(languages);
    Expect(pattern, "%{lang}", "en|fr|x-private");
    Expect(pattern, "%{#lang}", "1");
    Expect(pattern, "%{[]lang{%{lang} }}", "en fr x-private ");
    Expect(pattern, "%{=pkgkit}", "font(dejavusans)\nfont(other)\nfont(:lang=en)\nfont(:lang=fr)\nfont(:lang=x-private)\n");
    for (const auto* invalid : {"%", "%{", "%{}", "%{family[-1]}", "%{family[1}", "%{family", "%{#}",
            "%{family:-}", "%{family:x}", "%{family|unknown}", "%{=unknown}", "%{family|delete()}",
            "%{family|translate(a,)}", "%{family|translate(,b)}", "%{family|escape()}",
            "%{?family{a}", "%{+family{a}", "%{-family{a}", "%{[]family{a}", "%{{a}", "%{[]family a}", "%{[]family}", "%{[]family}end",
            "%{family[2147483648]}", "%{family[4294967295]}", "%{family[9223372036854775807]}", "%{family[99999999999999999999]}"}) {
        auto* result = FcPatternFormat(pattern, Text(invalid));
        if (result) {
            std::fprintf(stderr, "malformed format accepted: %s\n", invalid);
            std::abort();
        }
    }
    const std::string longText(20000, 'x');
    Expect(pattern, longText.c_str(), longText.c_str());
    const std::string longFormat = "%{missing:-" + longText + "}";
    Expect(nullptr, longFormat.c_str(), longText.c_str());
    for (int i = 0; i < 100; ++i) {
        CHECK(!FcPatternFormat(pattern, Text("%{+family{%{family}|bad}")));
        CHECK(!FcPatternFormat(pattern, Text("%{-family{%{style}|bad}")));
    }
    auto* ordered = FcLangSetCreate();
    CHECK(ordered);
    for (const auto* language : {"ff", "fr", "be", "bm", "zz-extra", "aa-extra"})
        CHECK(FcLangSetAdd(ordered, Text(language)));
    CHECK(FcPatternDel_nid_postfix(pattern, "lang"));
    CHECK(FcPatternAddLangSet(pattern, "lang", ordered));
    FcLangSetDestroy(ordered);
    Expect(pattern, "%{lang}", "bm|be|fr|ff|zz-extra|aa-extra");
    Expect(pattern, "%{[]lang{%{lang} }}", "be bm ff fr zz-extra aa-extra ");
    auto* escaped = FcPatternCreate();
    CHECK(escaped);
    CHECK(FcPatternAddString(escaped, "family", Text("a-b:c,d\\e")));
    CHECK(FcPatternAddString(escaped, "style", Text("s=t_u:v,w\\x")));
    Expect(escaped, "%{=unparse}", "a\\-b\\:c\\,d\\\\e:style=s\\=t\\_u\\:v\\,w\\\\x");
    FcPatternDestroy_nid_postfix(escaped);

    const auto expectRejected = [](auto&& fn, int line) {
        errno = 73;
        bool threw = false;
        try {
            fn();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Check(threw && errno == 73, line);
    };
    void* guardedPages = nullptr;
    std::size_t pageSize = 0;
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    pageSize = info.dwPageSize;
    guardedPages = VirtualAlloc(nullptr, pageSize * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    CHECK(guardedPages != nullptr);
    DWORD previous = 0;
    CHECK(VirtualProtect(static_cast<char*>(guardedPages) + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
    const long size = sysconf(_SC_PAGESIZE);
    CHECK(size > 0);
    pageSize = static_cast<std::size_t>(size);
    guardedPages = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK(guardedPages != MAP_FAILED);
    CHECK(mprotect(static_cast<char*>(guardedPages) + pageSize, pageSize, PROT_NONE) == 0);
#endif
    auto* noAccessPage = static_cast<char*>(guardedPages) + pageSize;
    char* validBoundary = noAccessPage - 9;
    std::memcpy(validBoundary, "%{style}", 9);
    Expect(pattern, validBoundary, "Book");
    char* unterminated = noAccessPage - 4;
    std::memcpy(unterminated, "%{st", 4);
    expectRejected([&] { FcPatternFormat(pattern, reinterpret_cast<const FcChar8*>(std::uintptr_t{0x10})); }, __LINE__);
    expectRejected([&] { FcPatternFormat(pattern, reinterpret_cast<const FcChar8*>(noAccessPage)); }, __LINE__);
    expectRejected([&] { FcPatternFormat(pattern, reinterpret_cast<const FcChar8*>(unterminated)); }, __LINE__);
#ifdef _WIN32
    VirtualFree(guardedPages, 0, MEM_RELEASE);
#else
    munmap(guardedPages, pageSize * 2);
#endif

    FcPatternDestroy_nid_postfix(pattern);
    std::puts("Fontconfig format checks passed");
}
