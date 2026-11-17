#include "prx/libfontconfig/libfontconfig.hpp"
#include "Testing.hpp"

#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <limits>
#include <source_location>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static const FcChar8* Text(const char* text) {
    return reinterpret_cast<const FcChar8*>(text);
}

static void Languages() {
    FcLangSetDestroy(nullptr);
    auto* first = FcLangSetCreate();
    auto* second = FcLangSetCreate();
    Require(first && second);
    Require(FcLangSetHasLang(first, Text("en")) == FcLangDifferentLang);
    Require(FcLangSetAdd(first, Text("EN-us")) == 1);
    Require(FcLangSetAdd(first, Text("en-US")) == 1);
    Require(FcLangSetHasLang(first, Text("en-US")) == FcLangEqual);
    Require(FcLangSetHasLang(first, Text("en")) == FcLangDifferentTerritory);
    Require(FcLangSetHasLang(first, Text("en-GB")) == FcLangDifferentTerritory);
    Require(FcLangSetHasLang(first, Text("eng")) == FcLangDifferentLang);
    Require(FcLangSetHasLang(second, Text("en-US")) == FcLangDifferentLang);
    Require(FcLangSetAdd(first, Text("EN")) == 1);
    Require(FcLangSetHasLang(first, Text("en")) == FcLangEqual);
    Require(FcLangSetHasLang(first, Text("en_US")) == FcLangDifferentLang);
    Require(FcLangSetAdd(first, Text("zh-cn")) == 1);
    Require(FcLangSetHasLang(first, Text("zh-tw")) == FcLangDifferentTerritory);
    Require(FcLangSetAdd(first, Text("x-private-long-tag")) == 1);
    Require(FcLangSetHasLang(first, Text("X-PRIVATE-LONG-TAG")) == FcLangEqual);
    Require(FcLangSetHasLang(first, Text("x-another")) == FcLangDifferentTerritory);
    Require(FcLangSetAdd(first, Text("und")) == 1);
    Require(FcLangSetHasLang(first, Text("und")) == FcLangDifferentLang);
    Require(FcLangSetAdd(first, Text("und-Latn")) == 1);
    Require(FcLangSetHasLang(first, Text("und-latn")) == FcLangEqual);
    Require(FcLangSetAdd(first, Text("")) == 1);
    Require(FcLangSetHasLang(first, Text("")) == FcLangEqual);
    FcLangSetDestroy(first);
    Require(FcLangSetAdd(second, Text("fr")) == 1);
    Require(FcLangSetHasLang(second, Text("FR-ca")) == FcLangDifferentTerritory);
    FcLangSetDestroy(second);
}

static void Patterns() {
    FcPatternDestroy(nullptr);
    Require(FcPatternDuplicate(nullptr) == nullptr);
    auto* pattern = FcPatternCreate();
    auto* empty = FcPatternCreate();
    Require(pattern && empty);
    Require(FcPatternEqual(pattern, pattern) == 1);
    Require(FcPatternEqual(pattern, empty) == 0);
    Require(FcPatternEqual(nullptr, nullptr) == 1);
    FcValue value{};
    value.type = FcTypeRange;
    value.u.i = 123;
    Require(FcPatternGet_nid_postfix(nullptr, "family", 0, &value) == FcResultNoMatch);
    Require(FcPatternGet_nid_postfix(pattern, "missing", 0, &value) == FcResultNoMatch);
    Require(value.type == FcTypeRange && value.u.i == 123);
    char object[] = "family";
    FcChar8 original[] = "Example";
    Require(FcPatternAddString(pattern, object, original));
    object[0] = 'X';
    original[0] = 'Y';
    FcChar8* string = nullptr;
    Require(FcPatternGetString(pattern, "family", 0, &string) == FcResultMatch);
    Require(std::strcmp(reinterpret_cast<const char*>(string), "Example") == 0);
    const auto* savedString = string;
    Require(FcPatternAddString(pattern, "family", Text("Second")));
    Require(std::strcmp(reinterpret_cast<const char*>(savedString), "Example") == 0);
    Require(FcPatternGetString(pattern, "family", 1, &string) == FcResultMatch);
    Require(std::strcmp(reinterpret_cast<const char*>(string), "Second") == 0);
    Require(FcPatternGet_nid_postfix(pattern, "family", -1, &value) == FcResultNoId);
    Require(FcPatternGet_nid_postfix(pattern, "family", INT_MAX, &value) == FcResultNoId);
    Require(value.type == FcTypeRange && value.u.i == 123);
    int integer = 0x12345678;
    Require(FcPatternGetInteger(pattern, "family", 0, &integer) == FcResultTypeMismatch);
    Require(integer == 0x12345678);
    Require(FcPatternAddInteger_nid_postfix(pattern, "family", 4) == 0);
    Require(FcPatternAddString(pattern, "family", nullptr) == 0);
    Require(FcPatternAddMatrix(pattern, "matrix", nullptr) == 0);
    Require(FcPatternAddInteger_nid_postfix(pattern, "slant", INT_MIN));
    Require(FcPatternAddDouble(pattern, "slant", -3.75));
    Require(FcPatternGetInteger(pattern, "slant", 0, &integer) == FcResultMatch && integer == INT_MIN);
    Require(FcPatternGetInteger(pattern, "slant", 1, &integer) == FcResultTypeMismatch);
    Require(integer == INT_MIN);
    Require(FcPatternAddDouble(pattern, "size", 2147483647.75));
    Require(FcPatternGetInteger(pattern, "size", 0, &integer) == FcResultTypeMismatch);
    Require(integer == INT_MIN);
    Require(FcPatternAddBool_nid_postfix(pattern, "antialias", 7));
    Require(FcPatternGetBool(pattern, "antialias", 0, &integer) == FcResultMatch && integer == 7);
    Require(FcPatternAddInteger_nid_postfix(pattern, "antialias", 1) == 0);
    Require(FcPatternAddString(pattern, "lang", Text("en")));
    Require(FcPatternAddBool_nid_postfix(pattern, "lang", 1) == 0);
    Require(FcPatternAddInteger_nid_postfix(pattern, "custom", 42));
    Require(FcPatternAddString(pattern, "custom", Text("mixed")));
    Require(FcPatternGet_nid_postfix(pattern, "custom", 0, &value) == FcResultMatch && value.type == FcTypeInteger && value.u.i == 42);
    Require(FcPatternGet_nid_postfix(pattern, "custom", 1, &value) == FcResultMatch && value.type == FcTypeString);
    Require(FcPatternAddString(pattern, "Family", Text("case-sensitive object name")));
    Require(FcPatternAddInteger_nid_postfix(pattern, "Family", 5));
    FcMatrix input{1, 2, 3, 4};
    Require(FcPatternAddMatrix(pattern, "matrix", &input));
    input.xx = 99;
    FcMatrix* matrix = nullptr;
    Require(FcPatternGetMatrix(pattern, "matrix", 0, &matrix) == FcResultMatch);
    Require(matrix->xx == 1 && matrix->xy == 2 && matrix->yx == 3 && matrix->yy == 4);
    Require(FcPatternAddMatrix(pattern, "matrix", matrix));
    Require(matrix->xx == 1);
    auto* copy = FcPatternDuplicate(pattern);
    Require(copy && copy != pattern && FcPatternEqual(pattern, copy));
    FcChar8* copyString = nullptr;
    Require(FcPatternGetString(copy, "family", 0, &copyString) == FcResultMatch);
    Require(copyString != savedString);
    Require(FcPatternGetMatrix(copy, "matrix", 0, &matrix) == FcResultMatch);
    matrix->xx = 9;
    Require(!FcPatternEqual(pattern, copy));
    matrix->xx = 1;
    Require(FcPatternEqual(pattern, copy));
    Require(!FcPatternRemove(pattern, "family", -1));
    Require(!FcPatternRemove(pattern, "family", 2));
    Require(FcPatternRemove(pattern, "family", 0));
    Require(FcPatternGetString(pattern, "family", 0, &string) == FcResultMatch);
    Require(std::strcmp(reinterpret_cast<const char*>(string), "Second") == 0);
    Require(FcPatternRemove(pattern, "family", 0));
    Require(FcPatternGet_nid_postfix(pattern, "family", 0, &value) == FcResultNoMatch);
    Require(!FcPatternDel_nid_postfix(pattern, "family"));
    Require(FcPatternDel_nid_postfix(pattern, "custom"));
    Require(!FcPatternDel_nid_postfix(pattern, "custom"));
    FcPatternReference(copy);
    FcPatternDestroy(copy);
    Require(FcPatternGetInteger(copy, "slant", 0, &integer) == FcResultMatch && integer == INT_MIN);
    Require(FcPatternGet_nid_postfix(copy, "absent", 0, nullptr) == FcResultNoMatch);
    Require(FcPatternGet_nid_postfix(copy, "slant", 2, nullptr) == FcResultNoId);
    Require(FcPatternGetString(copy, "slant", 0, nullptr) == FcResultTypeMismatch);
    FcPatternDestroy(copy);
    FcPatternDestroy(pattern);
    FcPatternDestroy(empty);
}

static void IntegerGetters() {
    auto* pattern = FcPatternCreate();
    Require(pattern);
    const int integers[] = {INT_MIN, -1, 0, 1, INT_MAX};
    for (int i = 0; i < static_cast<int>(std::size(integers)); ++i) {
        Require(FcPatternAddInteger_nid_postfix(pattern, "integer", integers[i]));
        int output = 0x12345678;
        Require(FcPatternGetInteger(pattern, "integer", i, &output) == FcResultMatch);
        Require(output == integers[i]);
    }
    const double doubles[] = {
        0.0, -0.0, 42.0, -3.75, 2147483647.75, -2147483649.0,
        std::numeric_limits<double>::max(), std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()
    };
    for (int i = 0; i < static_cast<int>(std::size(doubles)); ++i) {
        Require(FcPatternAddDouble(pattern, "double", doubles[i]));
        FcValue value{};
        Require(FcPatternGet_nid_postfix(pattern, "double", i, &value) == FcResultMatch);
        Require(value.type == FcTypeDouble);
        int output = 0x12345678;
        Require(FcPatternGetInteger(pattern, "double", i, &output) == FcResultTypeMismatch);
        Require(output == 0x12345678);
    }
    int output = 0x12345678;
    Require(FcPatternGetInteger(pattern, "missing", 0, &output) == FcResultNoMatch);
    Require(output == 0x12345678);
    Require(FcPatternGetInteger(pattern, "double", -1, &output) == FcResultNoId);
    Require(output == 0x12345678);
    Require(FcPatternGetInteger(pattern, "double", static_cast<int>(std::size(doubles)), &output) == FcResultNoId);
    Require(output == 0x12345678);
    FcPatternDestroy(pattern);
}

static void Equality() {
    const struct { const char* first; const char* second; bool equal; } pairs[] = {
        {"Example", "eXAMPLE", true},
        {"Stra\xc3\x9f" "e", "STRASSE", true},
        {"\xce\xa3\xcf\x82\xcf\x83", "\xcf\x83\xcf\x83\xcf\x83", true},
        {"\xe2\x84\xaa", "k", true},
        {"\xc4\xb0", "i\xcc\x87", true},
        {"\xef\xac\x83", "ffi", true},
        {"\xc4\xb1", "i", false},
        {"\xe1\xb2\x88", "", true},
        {"\xe1\xb2\x88", "\xea\x99\x8b", false},
        {"\xc1\x81", "a", true},
        {"\xff" "A", "\xff" "a", true},
        {"\xc3", "\xc3", true},
        {"a b", "ab", false}
    };
    for (const auto& pair : pairs) {
        auto* first = FcPatternCreate();
        auto* second = FcPatternCreate();
        Require(first && second);
        Require(FcPatternAddString(first, "family", Text(pair.first)));
        Require(FcPatternAddString(second, "family", Text(pair.second)));
        Require((FcPatternEqual(first, second) != 0) == pair.equal);
        Require((FcPatternEqual(second, first) != 0) == pair.equal);
        FcPatternDestroy(first);
        FcPatternDestroy(second);
    }
    auto* first = FcPatternCreate();
    auto* second = FcPatternCreate();
    Require(first && second);
    Require(FcPatternAddInteger_nid_postfix(first, "custom", 42));
    Require(FcPatternAddDouble(second, "custom", 42));
    Require(FcPatternEqual(first, second));
    Require(FcPatternAddDouble(first, "nan", std::numeric_limits<double>::quiet_NaN()));
    Require(FcPatternAddDouble(second, "nan", std::numeric_limits<double>::quiet_NaN()));
    Require(!FcPatternEqual(first, second));
    Require(FcPatternEqual(first, first));
    FcPatternDestroy(first);
    FcPatternDestroy(second);
}

static void FontSets() {
    auto* pattern = FcPatternCreate();
    auto* set = FcFontSetCreate();
    Require(pattern && set);
    Require(FcPatternAddInteger_nid_postfix(pattern, "weight", 80));
    Require(FcFontSetAdd(set, pattern));
    Require(set->nfont == 1);
    Require(set->fonts[0] != pattern);
    Require(FcPatternEqual(set->fonts[0], pattern));
    Require(FcPatternDel_nid_postfix(pattern, "weight"));
    Require(FcPatternAddInteger_nid_postfix(pattern, "weight", 200));
    Require(FcFontSetAdd(set, pattern));
    Require(set->nfont == 2);
    Require(set->fonts[1] != pattern && set->fonts[1] != set->fonts[0]);
    FcPatternDestroy(pattern);
    int weight = 0;
    Require(FcPatternGetInteger(set->fonts[0], "weight", 0, &weight) == FcResultMatch);
    Require(weight == 80);
    Require(FcPatternGetInteger(set->fonts[1], "weight", 0, &weight) == FcResultMatch);
    Require(weight == 200);
    FcFontSetDestroy(set);
}

static void FreeTypeFaceGetter() {
    int sentinel = 0;
    void* face = &sentinel;
    Require(FcPatternGetFTFace(nullptr, "ftface", 0, &face) == FcResultNoMatch && face == &sentinel);
    auto* pattern = FcPatternCreate();
    Require(pattern);
    Require(FcPatternGetFTFace(pattern, "ftface", 0, &face) == FcResultNoMatch && face == &sentinel);
    Require(FcPatternAddString(pattern, "family", Text("Sans")) == 1);
    Require(FcPatternAddInteger_nid_postfix(pattern, "weight", 80) == 1);
    Require(FcPatternGetFTFace(pattern, "family", 0, &face) == FcResultTypeMismatch && face == &sentinel);
    Require(FcPatternGetFTFace(pattern, "weight", 0, &face) == FcResultTypeMismatch && face == &sentinel);
    Require(FcPatternGetFTFace(pattern, "family", 0, nullptr) == FcResultTypeMismatch);
    Require(FcPatternGetFTFace(pattern, "family", 1, &face) == FcResultNoId && face == &sentinel);
    Require(FcPatternGetFTFace(pattern, "family", -1, &face) == FcResultNoId && face == &sentinel);
    FcPatternDestroy(pattern);
}

static void CharSetValues() {
    auto* pattern = FcPatternCreate();
    auto* other = FcPatternCreate();
    auto* first = FcCharSetCreate();
    auto* second = FcCharSetCreate();
    Require(pattern && other && first && second);
    Require(FcCharSetAddChar(first, 'A'));
    Require(FcCharSetAddChar(second, 'A'));
    Require(!FcPatternAddCharSet(pattern, "family", first));
    Require(!FcPatternAddCharSet(pattern, "charset", nullptr));
    Require(FcPatternAddCharSet(pattern, "charset", first));
    Require(FcPatternAddCharSet(other, "charset", second));
    Require(FcPatternEqual(pattern, other));

    auto* borrowed = second;
    Require(FcPatternGetCharSet(pattern, "absent", 0, &borrowed) == FcResultNoMatch && borrowed == second);
    Require(FcPatternGetCharSet(pattern, "charset", -1, &borrowed) == FcResultNoId && borrowed == second);
    Require(FcPatternGetCharSet(pattern, "charset", INT_MAX, &borrowed) == FcResultNoId && borrowed == second);
    Require(FcPatternAddString(pattern, "family", Text("example")));
    Require(FcPatternGetCharSet(pattern, "family", 0, &borrowed) == FcResultTypeMismatch && borrowed == second);
    Require(FcPatternDel_nid_postfix(pattern, "family"));
    Require(FcPatternGetCharSet(pattern, "charset", 0, &borrowed) == FcResultMatch && borrowed == first);
    FcValue value{};
    Require(FcPatternGet_nid_postfix(pattern, "charset", 0, &value) == FcResultMatch && value.type == FcTypeCharSet && value.u.c == borrowed);
    FcCharSetDestroy(first);
    Require(FcCharSetAddChar(borrowed, 0x1f680));
    Require(!FcPatternEqual(pattern, other));
    Require(FcCharSetSubtractCount(borrowed, second) == 1);

    auto* copy = FcPatternDuplicate(pattern);
    auto* select = FcObjectSetBuild("charset", "charset", static_cast<const char*>(nullptr));
    Require(copy && select && FcPatternEqual(pattern, copy));
    auto* filtered = FcPatternFilter(pattern, select);
    FcObjectSetDestroy(select);
    Require(filtered);
    Require(FcPatternRemove(pattern, "charset", 0));
    FcPatternDestroy(pattern);
    FcPatternDestroy(copy);
    Require(FcPatternRemove(filtered, "charset", 0));
    auto* retained = second;
    Require(FcPatternGetCharSet(filtered, "charset", 0, &retained) == FcResultMatch && retained == borrowed);
    Require(FcCharSetSubtractCount(retained, second) == 1);
    Require(FcCharSetAddChar(second, 0x1f680));
    Require(FcPatternEqual(filtered, other));
    FcCharSetDestroy(second);
    FcPatternDestroy(other);
    FcPatternDestroy(filtered);
}

static void LangSetValues() {
    auto* pattern = FcPatternCreate();
    auto* languages = FcLangSetCreate();
    Require(pattern && languages);
    Require(FcLangSetAdd(languages, Text("en")));
    Require(FcLangSetAdd(languages, Text("x-private-font")));
    Require(!FcPatternAddLangSet(pattern, "family", languages));
    Require(!FcPatternAddLangSet(pattern, "lang", nullptr));
    Require(FcPatternAddString(pattern, "lang", Text("en")));
    Require(FcPatternAddLangSet(pattern, "lang", languages));
    auto* borrowed = languages;
    Require(FcPatternGetLangSet(pattern, "absent", 0, &borrowed) == FcResultNoMatch && borrowed == languages);
    Require(FcPatternGetLangSet(pattern, "lang", 0, &borrowed) == FcResultTypeMismatch && borrowed == languages);
    Require(FcPatternGetLangSet(pattern, "lang", 2, &borrowed) == FcResultNoId && borrowed == languages);
    Require(FcPatternGetLangSet(pattern, "lang", 1, &borrowed) == FcResultMatch && borrowed != languages);
    FcValue value{};
    Require(FcPatternGet_nid_postfix(pattern, "lang", 1, &value) == FcResultMatch && value.type == FcTypeLangSet && value.u.l == borrowed);
    Require(FcLangSetAdd(languages, Text("fr")));
    Require(FcLangSetHasLang(borrowed, Text("fr")) == FcLangDifferentLang);
    FcLangSetDestroy(languages);
    Require(FcLangSetHasLang(borrowed, Text("en")) == FcLangEqual);
    Require(FcLangSetHasLang(borrowed, Text("x-private-font")) == FcLangEqual);

    auto* copy = FcPatternFilter(pattern, nullptr);
    auto* select = FcObjectSetBuild("lang", "lang", static_cast<const char*>(nullptr));
    Require(copy && copy != pattern && select && FcPatternEqual(pattern, copy));
    auto* filtered = FcPatternFilter(pattern, select);
    FcObjectSetDestroy(select);
    Require(filtered);
    FcLangSet* copyLanguages = nullptr;
    FcLangSet* selectedFirst = nullptr;
    FcLangSet* selectedSecond = nullptr;
    Require(FcPatternGetLangSet(copy, "lang", 1, &copyLanguages) == FcResultMatch && copyLanguages != borrowed);
    Require(FcPatternGetLangSet(filtered, "lang", 1, &selectedFirst) == FcResultMatch && selectedFirst != borrowed);
    Require(FcPatternGetLangSet(filtered, "lang", 3, &selectedSecond) == FcResultMatch && selectedSecond != selectedFirst);
    Require(FcLangSetAdd(borrowed, Text("de")));
    Require(!FcPatternEqual(pattern, copy));
    Require(FcLangSetHasLang(copyLanguages, Text("de")) == FcLangDifferentLang);
    Require(FcLangSetAdd(copyLanguages, Text("de")));
    Require(FcPatternEqual(pattern, copy));
    Require(FcLangSetAdd(selectedFirst, Text("it")));
    Require(FcLangSetHasLang(selectedSecond, Text("it")) == FcLangDifferentLang);
    Require(FcPatternDel_nid_postfix(pattern, "lang"));
    FcPatternDestroy(pattern);
    FcPatternDestroy(copy);
    Require(FcLangSetHasLang(selectedFirst, Text("it")) == FcLangEqual);
    Require(FcLangSetHasLang(selectedSecond, Text("x-private-font")) == FcLangEqual);
    FcPatternDestroy(filtered);
}

static void PatternFiltering() {
    auto* source = FcPatternCreate();
    Require(source);
    Require(FcPatternAddString(source, "family", Text("First")));
    Require(FcPatternAddString(source, "family", Text("Second")));
    Require(FcPatternAddInteger_nid_postfix(source, "weight", 180));
    Require(FcPatternAddDouble(source, "size", 12.5));
    auto* select = FcObjectSetBuild("family", "missing", "family", "size", static_cast<const char*>(nullptr));
    auto* selectNone = FcObjectSetBuild(nullptr);
    Require(select && selectNone);
    auto* filtered = FcPatternFilter(source, select);
    auto* empty = FcPatternFilter(source, selectNone);
    FcObjectSetDestroy(select);
    FcObjectSetDestroy(selectNone);
    Require(filtered && empty && !FcPatternEqual(source, filtered));
    FcPatternDestroy(source);
    FcValue value{};
    Require(FcPatternGet_nid_postfix(empty, "family", 0, &value) == FcResultNoMatch);
    Require(FcPatternGet_nid_postfix(filtered, "missing", 0, &value) == FcResultNoMatch);
    Require(FcPatternGet_nid_postfix(filtered, "weight", 0, &value) == FcResultNoMatch);
    Require(FcPatternGet_nid_postfix(filtered, "size", 0, &value) == FcResultMatch && value.type == FcTypeDouble && value.u.d == 12.5);
    const char* expected[] = {"First", "Second", "First", "Second"};
    for (int index = 0; index < static_cast<int>(std::size(expected)); ++index) {
        FcChar8* text = nullptr;
        Require(FcPatternGetString(filtered, "family", index, &text) == FcResultMatch);
        Require(std::strcmp(reinterpret_cast<const char*>(text), expected[index]) == 0);
    }
    Require(FcPatternGet_nid_postfix(filtered, "family", 4, &value) == FcResultNoId);
    FcPatternDestroy(empty);
    FcPatternDestroy(filtered);
}

template <typename Fn>
static void ExpectThrowsPreservingErrno(Fn&& fn, const std::source_location& location = std::source_location::current()) {
    errno = 71;
    bool threw = false;
    try {
        fn();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw && errno == 71, location);
}

#define EXPECT_REJECTED(...) ExpectThrowsPreservingErrno([&] { __VA_ARGS__; })

static void GuestPointerChecks() {
    auto* pattern = FcPatternCreate();
    Require(pattern != nullptr);
    Require(FcPatternAddInteger_nid_postfix(pattern, "weight", 80));
    Require(FcPatternAddBool_nid_postfix(pattern, "scalable", 1));
    Require(FcPatternAddString(pattern, "family", Text("Mono")));
    const FcMatrix matrix{1.0, 0.0, 0.0, 1.0};
    Require(FcPatternAddMatrix(pattern, "matrix", &matrix));
    auto* charset = FcCharSetCreate();
    Require(charset != nullptr && FcCharSetAddChar(charset, 'A'));
    Require(FcPatternAddCharSet(pattern, "charset", charset));
    FcCharSetDestroy(charset);
    auto* langset = FcLangSetCreate();
    Require(langset != nullptr && FcLangSetAdd(langset, Text("en")));
    Require(FcPatternAddLangSet(pattern, "lang", langset));
    FcLangSetDestroy(langset);

    EXPECT_REJECTED(FcPatternAddMatrix(pattern, "matrix", reinterpret_cast<const FcMatrix*>(std::uintptr_t{0x10})));

    void* roPage = nullptr;
    std::size_t pageSize = 0;
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    pageSize = info.dwPageSize;
    roPage = VirtualAlloc(nullptr, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    Require(roPage != nullptr);
    DWORD previous = 0;
    Require(VirtualProtect(roPage, pageSize, PAGE_READONLY, &previous) != 0);
#else
    const long size = sysconf(_SC_PAGESIZE);
    Require(size > 0);
    pageSize = static_cast<std::size_t>(size);
    roPage = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(roPage != MAP_FAILED);
    Require(mprotect(roPage, pageSize, PROT_READ) == 0);
#endif
    EXPECT_REJECTED(FcPatternGet_nid_postfix(pattern, "weight", 0, reinterpret_cast<FcValue*>(std::uintptr_t{0x10})));
    EXPECT_REJECTED(FcPatternGet_nid_postfix(pattern, "weight", 0, static_cast<FcValue*>(roPage)));
    EXPECT_REJECTED(FcPatternGetInteger(pattern, "weight", 0, reinterpret_cast<int*>(std::uintptr_t{0x10})));
    EXPECT_REJECTED(FcPatternGetInteger(pattern, "weight", 0, static_cast<int*>(roPage)));
    EXPECT_REJECTED(FcPatternGetBool(pattern, "scalable", 0, static_cast<FcBool*>(roPage)));
    EXPECT_REJECTED(FcPatternGetString(pattern, "family", 0, static_cast<FcChar8**>(roPage)));
    EXPECT_REJECTED(FcPatternGetMatrix(pattern, "matrix", 0, static_cast<FcMatrix**>(roPage)));
    EXPECT_REJECTED(FcPatternGetCharSet(pattern, "charset", 0, static_cast<FcCharSet**>(roPage)));
    EXPECT_REJECTED(FcPatternGetLangSet(pattern, "lang", 0, static_cast<FcLangSet**>(roPage)));
#ifdef _WIN32
    VirtualFree(roPage, 0, MEM_RELEASE);
#else
    munmap(roPage, pageSize);
#endif
    FcPatternDestroy(pattern);
}

int main() {
    CharSetValues();
    LangSetValues();
    PatternFiltering();
    Languages();
    Patterns();
    IntegerGetters();
    Equality();
    FontSets();
    FreeTypeFaceGetter();
    GuestPointerChecks();
}
