#include "prx/libfontconfig/libfontconfig.hpp"

#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <limits>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Require(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "Fontconfig object check failed at line %d\n", line);
    std::abort();
}

#define CHECK(value) Require((value), __LINE__)

static const FcChar8* Text(const char* text) {
    return reinterpret_cast<const FcChar8*>(text);
}

static void Languages() {
    FcLangSetDestroy(nullptr);
    auto* first = FcLangSetCreate();
    auto* second = FcLangSetCreate();
    CHECK(first && second);
    CHECK(FcLangSetHasLang(first, Text("en")) == FcLangDifferentLang);
    CHECK(FcLangSetAdd(first, Text("EN-us")) == 1);
    CHECK(FcLangSetAdd(first, Text("en-US")) == 1);
    CHECK(FcLangSetHasLang(first, Text("en-US")) == FcLangEqual);
    CHECK(FcLangSetHasLang(first, Text("en")) == FcLangDifferentTerritory);
    CHECK(FcLangSetHasLang(first, Text("en-GB")) == FcLangDifferentTerritory);
    CHECK(FcLangSetHasLang(first, Text("eng")) == FcLangDifferentLang);
    CHECK(FcLangSetHasLang(second, Text("en-US")) == FcLangDifferentLang);
    CHECK(FcLangSetAdd(first, Text("EN")) == 1);
    CHECK(FcLangSetHasLang(first, Text("en")) == FcLangEqual);
    CHECK(FcLangSetHasLang(first, Text("en_US")) == FcLangDifferentLang);
    CHECK(FcLangSetAdd(first, Text("zh-cn")) == 1);
    CHECK(FcLangSetHasLang(first, Text("zh-tw")) == FcLangDifferentTerritory);
    CHECK(FcLangSetAdd(first, Text("x-private-long-tag")) == 1);
    CHECK(FcLangSetHasLang(first, Text("X-PRIVATE-LONG-TAG")) == FcLangEqual);
    CHECK(FcLangSetHasLang(first, Text("x-another")) == FcLangDifferentTerritory);
    CHECK(FcLangSetAdd(first, Text("und")) == 1);
    CHECK(FcLangSetHasLang(first, Text("und")) == FcLangDifferentLang);
    CHECK(FcLangSetAdd(first, Text("und-Latn")) == 1);
    CHECK(FcLangSetHasLang(first, Text("und-latn")) == FcLangEqual);
    CHECK(FcLangSetAdd(first, Text("")) == 1);
    CHECK(FcLangSetHasLang(first, Text("")) == FcLangEqual);
    FcLangSetDestroy(first);
    CHECK(FcLangSetAdd(second, Text("fr")) == 1);
    CHECK(FcLangSetHasLang(second, Text("FR-ca")) == FcLangDifferentTerritory);
    FcLangSetDestroy(second);
}

static void Patterns() {
    FcPatternDestroy_nid_postfix(nullptr);
    CHECK(FcPatternDuplicate_nid_postfix(nullptr) == nullptr);
    auto* pattern = FcPatternCreate();
    auto* empty = FcPatternCreate();
    CHECK(pattern && empty);
    CHECK(FcPatternEqual(pattern, pattern) == 1);
    CHECK(FcPatternEqual(pattern, empty) == 0);
    CHECK(FcPatternEqual(nullptr, nullptr) == 1);
    FcValue value{};
    value.type = FcTypeRange;
    value.u.i = 123;
    CHECK(FcPatternGet_nid_postfix(nullptr, "family", 0, &value) == FcResultNoMatch);
    CHECK(FcPatternGet_nid_postfix(pattern, "missing", 0, &value) == FcResultNoMatch);
    CHECK(value.type == FcTypeRange && value.u.i == 123);
    char object[] = "family";
    FcChar8 original[] = "Example";
    CHECK(FcPatternAddString(pattern, object, original));
    object[0] = 'X';
    original[0] = 'Y';
    FcChar8* string = nullptr;
    CHECK(FcPatternGetString_nid_postfix(pattern, "family", 0, &string) == FcResultMatch);
    CHECK(std::strcmp(reinterpret_cast<const char*>(string), "Example") == 0);
    const auto* savedString = string;
    CHECK(FcPatternAddString(pattern, "family", Text("Second")));
    CHECK(std::strcmp(reinterpret_cast<const char*>(savedString), "Example") == 0);
    CHECK(FcPatternGetString_nid_postfix(pattern, "family", 1, &string) == FcResultMatch);
    CHECK(std::strcmp(reinterpret_cast<const char*>(string), "Second") == 0);
    CHECK(FcPatternGet_nid_postfix(pattern, "family", -1, &value) == FcResultNoId);
    CHECK(FcPatternGet_nid_postfix(pattern, "family", INT_MAX, &value) == FcResultNoId);
    CHECK(value.type == FcTypeRange && value.u.i == 123);
    int integer = 0x12345678;
    CHECK(FcPatternGetInteger_nid_postfix(pattern, "family", 0, &integer) == FcResultTypeMismatch);
    CHECK(integer == 0x12345678);
    CHECK(FcPatternAddInteger_nid_postfix(pattern, "family", 4) == 0);
    CHECK(FcPatternAddString(pattern, "family", nullptr) == 0);
    CHECK(FcPatternAddMatrix(pattern, "matrix", nullptr) == 0);
    CHECK(FcPatternAddInteger_nid_postfix(pattern, "slant", INT_MIN));
    CHECK(FcPatternAddDouble(pattern, "slant", -3.75));
    CHECK(FcPatternGetInteger_nid_postfix(pattern, "slant", 0, &integer) == FcResultMatch && integer == INT_MIN);
    CHECK(FcPatternGetInteger_nid_postfix(pattern, "slant", 1, &integer) == FcResultTypeMismatch);
    CHECK(integer == INT_MIN);
    CHECK(FcPatternAddDouble(pattern, "size", 2147483647.75));
    CHECK(FcPatternGetInteger_nid_postfix(pattern, "size", 0, &integer) == FcResultTypeMismatch);
    CHECK(integer == INT_MIN);
    CHECK(FcPatternAddBool_nid_postfix(pattern, "antialias", 7));
    CHECK(FcPatternGetBool_nid_postfix(pattern, "antialias", 0, &integer) == FcResultMatch && integer == 7);
    CHECK(FcPatternAddInteger_nid_postfix(pattern, "antialias", 1) == 0);
    CHECK(FcPatternAddString(pattern, "lang", Text("en")));
    CHECK(FcPatternAddBool_nid_postfix(pattern, "lang", 1) == 0);
    CHECK(FcPatternAddInteger_nid_postfix(pattern, "custom", 42));
    CHECK(FcPatternAddString(pattern, "custom", Text("mixed")));
    CHECK(FcPatternGet_nid_postfix(pattern, "custom", 0, &value) == FcResultMatch && value.type == FcTypeInteger && value.u.i == 42);
    CHECK(FcPatternGet_nid_postfix(pattern, "custom", 1, &value) == FcResultMatch && value.type == FcTypeString);
    CHECK(FcPatternAddString(pattern, "Family", Text("case-sensitive object name")));
    CHECK(FcPatternAddInteger_nid_postfix(pattern, "Family", 5));
    FcMatrix input{1, 2, 3, 4};
    CHECK(FcPatternAddMatrix(pattern, "matrix", &input));
    input.xx = 99;
    FcMatrix* matrix = nullptr;
    CHECK(FcPatternGetMatrix(pattern, "matrix", 0, &matrix) == FcResultMatch);
    CHECK(matrix->xx == 1 && matrix->xy == 2 && matrix->yx == 3 && matrix->yy == 4);
    CHECK(FcPatternAddMatrix(pattern, "matrix", matrix));
    CHECK(matrix->xx == 1);
    auto* copy = FcPatternDuplicate_nid_postfix(pattern);
    CHECK(copy && copy != pattern && FcPatternEqual(pattern, copy));
    FcChar8* copyString = nullptr;
    CHECK(FcPatternGetString_nid_postfix(copy, "family", 0, &copyString) == FcResultMatch);
    CHECK(copyString != savedString);
    CHECK(FcPatternGetMatrix(copy, "matrix", 0, &matrix) == FcResultMatch);
    matrix->xx = 9;
    CHECK(!FcPatternEqual(pattern, copy));
    matrix->xx = 1;
    CHECK(FcPatternEqual(pattern, copy));
    CHECK(!FcPatternRemove(pattern, "family", -1));
    CHECK(!FcPatternRemove(pattern, "family", 2));
    CHECK(FcPatternRemove(pattern, "family", 0));
    CHECK(FcPatternGetString_nid_postfix(pattern, "family", 0, &string) == FcResultMatch);
    CHECK(std::strcmp(reinterpret_cast<const char*>(string), "Second") == 0);
    CHECK(FcPatternRemove(pattern, "family", 0));
    CHECK(FcPatternGet_nid_postfix(pattern, "family", 0, &value) == FcResultNoMatch);
    CHECK(!FcPatternDel_nid_postfix(pattern, "family"));
    CHECK(FcPatternDel_nid_postfix(pattern, "custom"));
    CHECK(!FcPatternDel_nid_postfix(pattern, "custom"));
    FcPatternReference(copy);
    FcPatternDestroy_nid_postfix(copy);
    CHECK(FcPatternGetInteger_nid_postfix(copy, "slant", 0, &integer) == FcResultMatch && integer == INT_MIN);
    CHECK(FcPatternGet_nid_postfix(copy, "absent", 0, nullptr) == FcResultNoMatch);
    CHECK(FcPatternGet_nid_postfix(copy, "slant", 2, nullptr) == FcResultNoId);
    CHECK(FcPatternGetString_nid_postfix(copy, "slant", 0, nullptr) == FcResultTypeMismatch);
    FcPatternDestroy_nid_postfix(copy);
    FcPatternDestroy_nid_postfix(pattern);
    FcPatternDestroy_nid_postfix(empty);
}

static void IntegerGetters() {
    auto* pattern = FcPatternCreate();
    CHECK(pattern);
    const int integers[] = {INT_MIN, -1, 0, 1, INT_MAX};
    for (int i = 0; i < static_cast<int>(std::size(integers)); ++i) {
        CHECK(FcPatternAddInteger_nid_postfix(pattern, "integer", integers[i]));
        int output = 0x12345678;
        CHECK(FcPatternGetInteger_nid_postfix(pattern, "integer", i, &output) == FcResultMatch);
        CHECK(output == integers[i]);
    }
    const double doubles[] = {
        0.0, -0.0, 42.0, -3.75, 2147483647.75, -2147483649.0,
        std::numeric_limits<double>::max(), std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()
    };
    for (int i = 0; i < static_cast<int>(std::size(doubles)); ++i) {
        CHECK(FcPatternAddDouble(pattern, "double", doubles[i]));
        FcValue value{};
        CHECK(FcPatternGet_nid_postfix(pattern, "double", i, &value) == FcResultMatch);
        CHECK(value.type == FcTypeDouble);
        int output = 0x12345678;
        CHECK(FcPatternGetInteger_nid_postfix(pattern, "double", i, &output) == FcResultTypeMismatch);
        CHECK(output == 0x12345678);
    }
    int output = 0x12345678;
    CHECK(FcPatternGetInteger_nid_postfix(pattern, "missing", 0, &output) == FcResultNoMatch);
    CHECK(output == 0x12345678);
    CHECK(FcPatternGetInteger_nid_postfix(pattern, "double", -1, &output) == FcResultNoId);
    CHECK(output == 0x12345678);
    CHECK(FcPatternGetInteger_nid_postfix(pattern, "double", static_cast<int>(std::size(doubles)), &output) == FcResultNoId);
    CHECK(output == 0x12345678);
    FcPatternDestroy_nid_postfix(pattern);
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
        CHECK(first && second);
        CHECK(FcPatternAddString(first, "family", Text(pair.first)));
        CHECK(FcPatternAddString(second, "family", Text(pair.second)));
        CHECK((FcPatternEqual(first, second) != 0) == pair.equal);
        CHECK((FcPatternEqual(second, first) != 0) == pair.equal);
        FcPatternDestroy_nid_postfix(first);
        FcPatternDestroy_nid_postfix(second);
    }
    auto* first = FcPatternCreate();
    auto* second = FcPatternCreate();
    CHECK(first && second);
    CHECK(FcPatternAddInteger_nid_postfix(first, "custom", 42));
    CHECK(FcPatternAddDouble(second, "custom", 42));
    CHECK(FcPatternEqual(first, second));
    CHECK(FcPatternAddDouble(first, "nan", std::numeric_limits<double>::quiet_NaN()));
    CHECK(FcPatternAddDouble(second, "nan", std::numeric_limits<double>::quiet_NaN()));
    CHECK(!FcPatternEqual(first, second));
    CHECK(FcPatternEqual(first, first));
    FcPatternDestroy_nid_postfix(first);
    FcPatternDestroy_nid_postfix(second);
}

static void FontSets() {
    auto* pattern = FcPatternCreate();
    auto* set = FcFontSetCreate();
    CHECK(pattern && set);
    CHECK(FcPatternAddInteger_nid_postfix(pattern, "weight", 80));
    CHECK(FcFontSetAdd(set, pattern));
    CHECK(set->nfont == 1);
    CHECK(set->fonts[0] != pattern);
    CHECK(FcPatternEqual(set->fonts[0], pattern));
    CHECK(FcPatternDel_nid_postfix(pattern, "weight"));
    CHECK(FcPatternAddInteger_nid_postfix(pattern, "weight", 200));
    CHECK(FcFontSetAdd(set, pattern));
    CHECK(set->nfont == 2);
    CHECK(set->fonts[1] != pattern && set->fonts[1] != set->fonts[0]);
    FcPatternDestroy_nid_postfix(pattern);
    int weight = 0;
    CHECK(FcPatternGetInteger_nid_postfix(set->fonts[0], "weight", 0, &weight) == FcResultMatch);
    CHECK(weight == 80);
    CHECK(FcPatternGetInteger_nid_postfix(set->fonts[1], "weight", 0, &weight) == FcResultMatch);
    CHECK(weight == 200);
    FcFontSetDestroy(set);
}

static void FreeTypeFaceGetter() {
    int sentinel = 0;
    void* face = &sentinel;
    CHECK(FcPatternGetFTFace_nid_postfix(nullptr, "ftface", 0, &face) == FcResultNoMatch && face == &sentinel);
    auto* pattern = FcPatternCreate();
    CHECK(pattern);
    CHECK(FcPatternGetFTFace_nid_postfix(pattern, "ftface", 0, &face) == FcResultNoMatch && face == &sentinel);
    CHECK(FcPatternAddString(pattern, "family", Text("Sans")) == 1);
    CHECK(FcPatternAddInteger_nid_postfix(pattern, "weight", 80) == 1);
    CHECK(FcPatternGetFTFace_nid_postfix(pattern, "family", 0, &face) == FcResultTypeMismatch && face == &sentinel);
    CHECK(FcPatternGetFTFace_nid_postfix(pattern, "weight", 0, &face) == FcResultTypeMismatch && face == &sentinel);
    CHECK(FcPatternGetFTFace_nid_postfix(pattern, "family", 0, nullptr) == FcResultTypeMismatch);
    CHECK(FcPatternGetFTFace_nid_postfix(pattern, "family", 1, &face) == FcResultNoId && face == &sentinel);
    CHECK(FcPatternGetFTFace_nid_postfix(pattern, "family", -1, &face) == FcResultNoId && face == &sentinel);
    FcPatternDestroy_nid_postfix(pattern);
}

static void CharSetValues() {
    auto* pattern = FcPatternCreate();
    auto* other = FcPatternCreate();
    auto* first = FcCharSetCreate();
    auto* second = FcCharSetCreate();
    CHECK(pattern && other && first && second);
    CHECK(FcCharSetAddChar(first, 'A'));
    CHECK(FcCharSetAddChar(second, 'A'));
    CHECK(!FcPatternAddCharSet(pattern, "family", first));
    CHECK(!FcPatternAddCharSet(pattern, "charset", nullptr));
    CHECK(FcPatternAddCharSet(pattern, "charset", first));
    CHECK(FcPatternAddCharSet(other, "charset", second));
    CHECK(FcPatternEqual(pattern, other));

    auto* borrowed = second;
    CHECK(FcPatternGetCharSet(pattern, "absent", 0, &borrowed) == FcResultNoMatch && borrowed == second);
    CHECK(FcPatternGetCharSet(pattern, "charset", -1, &borrowed) == FcResultNoId && borrowed == second);
    CHECK(FcPatternGetCharSet(pattern, "charset", INT_MAX, &borrowed) == FcResultNoId && borrowed == second);
    CHECK(FcPatternAddString(pattern, "family", Text("example")));
    CHECK(FcPatternGetCharSet(pattern, "family", 0, &borrowed) == FcResultTypeMismatch && borrowed == second);
    CHECK(FcPatternDel_nid_postfix(pattern, "family"));
    CHECK(FcPatternGetCharSet(pattern, "charset", 0, &borrowed) == FcResultMatch && borrowed == first);
    FcValue value{};
    CHECK(FcPatternGet_nid_postfix(pattern, "charset", 0, &value) == FcResultMatch && value.type == FcTypeCharSet && value.u.c == borrowed);
    FcCharSetDestroy(first);
    CHECK(FcCharSetAddChar(borrowed, 0x1f680));
    CHECK(!FcPatternEqual(pattern, other));
    CHECK(FcCharSetSubtractCount(borrowed, second) == 1);

    auto* copy = FcPatternDuplicate_nid_postfix(pattern);
    auto* select = FcObjectSetBuild("charset", "charset", static_cast<const char*>(nullptr));
    CHECK(copy && select && FcPatternEqual(pattern, copy));
    auto* filtered = FcPatternFilter(pattern, select);
    FcObjectSetDestroy(select);
    CHECK(filtered);
    CHECK(FcPatternRemove(pattern, "charset", 0));
    FcPatternDestroy_nid_postfix(pattern);
    FcPatternDestroy_nid_postfix(copy);
    CHECK(FcPatternRemove(filtered, "charset", 0));
    auto* retained = second;
    CHECK(FcPatternGetCharSet(filtered, "charset", 0, &retained) == FcResultMatch && retained == borrowed);
    CHECK(FcCharSetSubtractCount(retained, second) == 1);
    CHECK(FcCharSetAddChar(second, 0x1f680));
    CHECK(FcPatternEqual(filtered, other));
    FcCharSetDestroy(second);
    FcPatternDestroy_nid_postfix(other);
    FcPatternDestroy_nid_postfix(filtered);
}

static void LangSetValues() {
    auto* pattern = FcPatternCreate();
    auto* languages = FcLangSetCreate();
    CHECK(pattern && languages);
    CHECK(FcLangSetAdd(languages, Text("en")));
    CHECK(FcLangSetAdd(languages, Text("x-private-font")));
    CHECK(!FcPatternAddLangSet(pattern, "family", languages));
    CHECK(!FcPatternAddLangSet(pattern, "lang", nullptr));
    CHECK(FcPatternAddString(pattern, "lang", Text("en")));
    CHECK(FcPatternAddLangSet(pattern, "lang", languages));
    auto* borrowed = languages;
    CHECK(FcPatternGetLangSet(pattern, "absent", 0, &borrowed) == FcResultNoMatch && borrowed == languages);
    CHECK(FcPatternGetLangSet(pattern, "lang", 0, &borrowed) == FcResultTypeMismatch && borrowed == languages);
    CHECK(FcPatternGetLangSet(pattern, "lang", 2, &borrowed) == FcResultNoId && borrowed == languages);
    CHECK(FcPatternGetLangSet(pattern, "lang", 1, &borrowed) == FcResultMatch && borrowed != languages);
    FcValue value{};
    CHECK(FcPatternGet_nid_postfix(pattern, "lang", 1, &value) == FcResultMatch && value.type == FcTypeLangSet && value.u.l == borrowed);
    CHECK(FcLangSetAdd(languages, Text("fr")));
    CHECK(FcLangSetHasLang(borrowed, Text("fr")) == FcLangDifferentLang);
    FcLangSetDestroy(languages);
    CHECK(FcLangSetHasLang(borrowed, Text("en")) == FcLangEqual);
    CHECK(FcLangSetHasLang(borrowed, Text("x-private-font")) == FcLangEqual);

    auto* copy = FcPatternFilter(pattern, nullptr);
    auto* select = FcObjectSetBuild("lang", "lang", static_cast<const char*>(nullptr));
    CHECK(copy && copy != pattern && select && FcPatternEqual(pattern, copy));
    auto* filtered = FcPatternFilter(pattern, select);
    FcObjectSetDestroy(select);
    CHECK(filtered);
    FcLangSet* copyLanguages = nullptr;
    FcLangSet* selectedFirst = nullptr;
    FcLangSet* selectedSecond = nullptr;
    CHECK(FcPatternGetLangSet(copy, "lang", 1, &copyLanguages) == FcResultMatch && copyLanguages != borrowed);
    CHECK(FcPatternGetLangSet(filtered, "lang", 1, &selectedFirst) == FcResultMatch && selectedFirst != borrowed);
    CHECK(FcPatternGetLangSet(filtered, "lang", 3, &selectedSecond) == FcResultMatch && selectedSecond != selectedFirst);
    CHECK(FcLangSetAdd(borrowed, Text("de")));
    CHECK(!FcPatternEqual(pattern, copy));
    CHECK(FcLangSetHasLang(copyLanguages, Text("de")) == FcLangDifferentLang);
    CHECK(FcLangSetAdd(copyLanguages, Text("de")));
    CHECK(FcPatternEqual(pattern, copy));
    CHECK(FcLangSetAdd(selectedFirst, Text("it")));
    CHECK(FcLangSetHasLang(selectedSecond, Text("it")) == FcLangDifferentLang);
    CHECK(FcPatternDel_nid_postfix(pattern, "lang"));
    FcPatternDestroy_nid_postfix(pattern);
    FcPatternDestroy_nid_postfix(copy);
    CHECK(FcLangSetHasLang(selectedFirst, Text("it")) == FcLangEqual);
    CHECK(FcLangSetHasLang(selectedSecond, Text("x-private-font")) == FcLangEqual);
    FcPatternDestroy_nid_postfix(filtered);
}

static void PatternFiltering() {
    auto* source = FcPatternCreate();
    CHECK(source);
    CHECK(FcPatternAddString(source, "family", Text("First")));
    CHECK(FcPatternAddString(source, "family", Text("Second")));
    CHECK(FcPatternAddInteger_nid_postfix(source, "weight", 180));
    CHECK(FcPatternAddDouble(source, "size", 12.5));
    auto* select = FcObjectSetBuild("family", "missing", "family", "size", static_cast<const char*>(nullptr));
    auto* selectNone = FcObjectSetBuild(nullptr);
    CHECK(select && selectNone);
    auto* filtered = FcPatternFilter(source, select);
    auto* empty = FcPatternFilter(source, selectNone);
    FcObjectSetDestroy(select);
    FcObjectSetDestroy(selectNone);
    CHECK(filtered && empty && !FcPatternEqual(source, filtered));
    FcPatternDestroy_nid_postfix(source);
    FcValue value{};
    CHECK(FcPatternGet_nid_postfix(empty, "family", 0, &value) == FcResultNoMatch);
    CHECK(FcPatternGet_nid_postfix(filtered, "missing", 0, &value) == FcResultNoMatch);
    CHECK(FcPatternGet_nid_postfix(filtered, "weight", 0, &value) == FcResultNoMatch);
    CHECK(FcPatternGet_nid_postfix(filtered, "size", 0, &value) == FcResultMatch && value.type == FcTypeDouble && value.u.d == 12.5);
    const char* expected[] = {"First", "Second", "First", "Second"};
    for (int index = 0; index < static_cast<int>(std::size(expected)); ++index) {
        FcChar8* text = nullptr;
        CHECK(FcPatternGetString_nid_postfix(filtered, "family", index, &text) == FcResultMatch);
        CHECK(std::strcmp(reinterpret_cast<const char*>(text), expected[index]) == 0);
    }
    CHECK(FcPatternGet_nid_postfix(filtered, "family", 4, &value) == FcResultNoId);
    FcPatternDestroy_nid_postfix(empty);
    FcPatternDestroy_nid_postfix(filtered);
}

template <typename Fn>
static void ExpectThrowsPreservingErrno(Fn&& fn, int line) {
    errno = 71;
    bool threw = false;
    try {
        fn();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw && errno == 71, line);
}

#define EXPECT_REJECTED(...) ExpectThrowsPreservingErrno([&] { __VA_ARGS__; }, __LINE__)

static void GuestPointerChecks() {
    auto* pattern = FcPatternCreate();
    CHECK(pattern != nullptr);
    CHECK(FcPatternAddInteger_nid_postfix(pattern, "weight", 80));
    CHECK(FcPatternAddBool_nid_postfix(pattern, "scalable", 1));
    CHECK(FcPatternAddString(pattern, "family", Text("Mono")));
    const FcMatrix matrix{1.0, 0.0, 0.0, 1.0};
    CHECK(FcPatternAddMatrix(pattern, "matrix", &matrix));
    auto* charset = FcCharSetCreate();
    CHECK(charset != nullptr && FcCharSetAddChar(charset, 'A'));
    CHECK(FcPatternAddCharSet(pattern, "charset", charset));
    FcCharSetDestroy(charset);
    auto* langset = FcLangSetCreate();
    CHECK(langset != nullptr && FcLangSetAdd(langset, Text("en")));
    CHECK(FcPatternAddLangSet(pattern, "lang", langset));
    FcLangSetDestroy(langset);

    EXPECT_REJECTED(FcPatternAddMatrix(pattern, "matrix", reinterpret_cast<const FcMatrix*>(std::uintptr_t{0x10})));

    void* roPage = nullptr;
    std::size_t pageSize = 0;
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    pageSize = info.dwPageSize;
    roPage = VirtualAlloc(nullptr, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    CHECK(roPage != nullptr);
    DWORD previous = 0;
    CHECK(VirtualProtect(roPage, pageSize, PAGE_READONLY, &previous) != 0);
#else
    const long size = sysconf(_SC_PAGESIZE);
    CHECK(size > 0);
    pageSize = static_cast<std::size_t>(size);
    roPage = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK(roPage != MAP_FAILED);
    CHECK(mprotect(roPage, pageSize, PROT_READ) == 0);
#endif
    EXPECT_REJECTED(FcPatternGet_nid_postfix(pattern, "weight", 0, reinterpret_cast<FcValue*>(std::uintptr_t{0x10})));
    EXPECT_REJECTED(FcPatternGet_nid_postfix(pattern, "weight", 0, static_cast<FcValue*>(roPage)));
    EXPECT_REJECTED(FcPatternGetInteger_nid_postfix(pattern, "weight", 0, reinterpret_cast<int*>(std::uintptr_t{0x10})));
    EXPECT_REJECTED(FcPatternGetInteger_nid_postfix(pattern, "weight", 0, static_cast<int*>(roPage)));
    EXPECT_REJECTED(FcPatternGetBool_nid_postfix(pattern, "scalable", 0, static_cast<FcBool*>(roPage)));
    EXPECT_REJECTED(FcPatternGetString_nid_postfix(pattern, "family", 0, static_cast<FcChar8**>(roPage)));
    EXPECT_REJECTED(FcPatternGetMatrix(pattern, "matrix", 0, static_cast<FcMatrix**>(roPage)));
    EXPECT_REJECTED(FcPatternGetCharSet(pattern, "charset", 0, static_cast<FcCharSet**>(roPage)));
    EXPECT_REJECTED(FcPatternGetLangSet(pattern, "lang", 0, static_cast<FcLangSet**>(roPage)));
#ifdef _WIN32
    VirtualFree(roPage, 0, MEM_RELEASE);
#else
    munmap(roPage, pageSize);
#endif
    FcPatternDestroy_nid_postfix(pattern);
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
