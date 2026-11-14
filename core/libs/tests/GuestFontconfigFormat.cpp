#include "prx/libfontconfig/libfontconfig.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

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
    CHECK(FcPatternAddInteger(pattern, "index", 2));
    CHECK(FcPatternAddInteger(pattern, "weight", 80));
    CHECK(FcPatternAddDouble(pattern, "size", 12.5));
    CHECK(FcPatternAddBool(pattern, "antialias", 1));
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
            "%{?family{a}", "%{+family{a}", "%{-family{a}", "%{[]family{a}", "%{{a}", "%{[]family a}"}) {
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
    CHECK(FcPatternDel(pattern, "lang"));
    CHECK(FcPatternAddLangSet(pattern, "lang", ordered));
    FcLangSetDestroy(ordered);
    Expect(pattern, "%{lang}", "bm|be|fr|ff|zz-extra|aa-extra");
    Expect(pattern, "%{[]lang{%{lang} }}", "be bm ff fr zz-extra aa-extra ");
    auto* escaped = FcPatternCreate();
    CHECK(escaped);
    CHECK(FcPatternAddString(escaped, "family", Text("a-b:c,d\\e")));
    CHECK(FcPatternAddString(escaped, "style", Text("s=t_u:v,w\\x")));
    Expect(escaped, "%{=unparse}", "a\\-b\\:c\\,d\\\\e:style=s\\=t\\_u\\:v\\,w\\\\x");
    FcPatternDestroy(escaped);
    FcPatternDestroy(pattern);
    std::puts("Fontconfig format checks passed");
}
