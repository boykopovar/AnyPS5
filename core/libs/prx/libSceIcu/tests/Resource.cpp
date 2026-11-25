#include "prx/libSceIcu/libSceIcu.hpp"
#include "uresdata.h"
#include <array>
#include <bit>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static_assert(std::is_same_v<decltype(&res_getPublicType_nid_postfix), std::int32_t (APS5_VABI*)(std::uint32_t)>);
static_assert(std::is_same_v<decltype(&res_read_nid_postfix),
                             void (APS5_VABI*)(Icu::ResourceData*, const Icu::DataInfo*, const void*, std::int32_t, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&res_getStringNoTrace_nid_postfix),
                             const char16_t* (APS5_VABI*)(const Icu::ResourceData*, std::uint32_t, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&res_getAlias_nid_postfix),
                             const char16_t* (APS5_VABI*)(const Icu::ResourceData*, std::uint32_t, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&res_getBinaryNoTrace_nid_postfix),
                             const std::uint8_t* (APS5_VABI*)(const Icu::ResourceData*, std::uint32_t, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&res_getIntVectorNoTrace_nid_postfix),
                             const std::int32_t* (APS5_VABI*)(const Icu::ResourceData*, std::uint32_t, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&res_getResource_nid_postfix),
                             std::uint32_t (APS5_VABI*)(const Icu::ResourceData*, const char*)>);
static_assert(std::is_same_v<decltype(&res_countArrayItems_nid_postfix),
                             std::int32_t (APS5_VABI*)(const Icu::ResourceData*, std::uint32_t)>);
static_assert(std::is_same_v<decltype(&res_getArrayItem_nid_postfix),
                             std::uint32_t (APS5_VABI*)(const Icu::ResourceData*, std::uint32_t, std::int32_t)>);
static_assert(std::is_same_v<decltype(&res_getTableItemByIndex_nid_postfix),
                             std::uint32_t (APS5_VABI*)(const Icu::ResourceData*, std::uint32_t, std::int32_t, const char**)>);
static_assert(std::is_same_v<decltype(&res_getTableItemByKey_nid_postfix),
                             std::uint32_t (APS5_VABI*)(const Icu::ResourceData*, std::uint32_t, std::int32_t*, const char**)>);

static_assert(sizeof(Icu::ResourceData) == sizeof(::ResourceData));
static_assert(alignof(Icu::ResourceData) == alignof(::ResourceData));

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU resource test failed: %s\n", message);
    std::abort();
}

template <typename TAction>
void RequireThrows(TAction action, const char* message) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, message);
}

void CheckPublicType(std::uint32_t resource, std::int32_t expected) {
    const auto actual = res_getPublicType_nid_postfix(resource);
    if (actual != expected) {
        std::fprintf(stderr, "resource 0x%08x: expected %d, got %d\n", resource, expected, actual);
        std::abort();
    }
}

void TestPublicTypes() {
    constexpr std::array<std::int32_t, 16> expectedTypes{0, 1, 2, 3, 2, 2, 0, 7, 8, 8, -1, -1, -1, -1, 14, -1};
    constexpr std::array<std::uint32_t, 6> offsets{0, 1, 0x07ffffff, 0x08000000, 0x0ffffffe, 0x0fffffff};
    std::uint32_t state = 0x62734a91;
    for (std::uint32_t type = 0; type < expectedTypes.size(); ++type) {
        const auto tag = type << 28;
        for (const auto offset : offsets) CheckPublicType(tag | offset, expectedTypes[type]);
        for (unsigned bit = 0; bit < 28; ++bit) {
            CheckPublicType(tag | (std::uint32_t{1} << bit), expectedTypes[type]);
            CheckPublicType(tag | (0x0fffffff ^ (std::uint32_t{1} << bit)), expectedTypes[type]);
        }
        for (unsigned sample = 0; sample < 4096; ++sample) {
            state = state * 1664525u + 1013904223u;
            CheckPublicType(tag | (state & 0x0fffffff), expectedTypes[type]);
        }
    }
    CheckPublicType(std::numeric_limits<std::uint32_t>::max(), -1);
}

struct Pages {
    char* begin = nullptr;
    std::size_t size = 0;

    Pages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        begin = static_cast<char*>(VirtualAlloc(nullptr, size * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        Require(begin != nullptr, "VirtualAlloc");
#else
        size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        auto* mapped = mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED, "mmap");
        begin = static_cast<char*>(mapped);
#endif
    }

    ~Pages() {
#ifdef _WIN32
        VirtualFree(begin, 0, MEM_RELEASE);
#else
        munmap(begin, size * 2);
#endif
    }

    void Protect(std::size_t page, bool writable, bool readable = true) {
#ifdef _WIN32
        DWORD previous = 0;
        const DWORD protection = readable ? (writable ? PAGE_READWRITE : PAGE_READONLY) : PAGE_NOACCESS;
        Require(VirtualProtect(begin + page * size, size, protection, &previous) != 0, "VirtualProtect");
#else
        const int protection = readable ? (writable ? PROT_READ | PROT_WRITE : PROT_READ) : PROT_NONE;
        Require(mprotect(begin + page * size, size, protection) == 0, "mprotect");
#endif
    }
};

Icu::DataInfo MakeValidInfo(std::uint8_t majorVersion, std::uint8_t minorVersion) {
    Icu::DataInfo info{};
    info.size = sizeof(Icu::DataInfo);
    info.isBigEndian = U_IS_BIG_ENDIAN;
    info.charsetFamily = U_CHARSET_FAMILY;
    info.sizeofUChar = U_SIZEOF_UCHAR;
    info.dataFormat[0] = 0x52;
    info.dataFormat[1] = 0x65;
    info.dataFormat[2] = 0x73;
    info.dataFormat[3] = 0x42;
    info.formatVersion[0] = majorVersion;
    info.formatVersion[1] = minorVersion;
    return info;
}

class BundleBuilder {
public:
    explicit BundleBuilder(std::size_t headerWords) : words(headerWords, 0) {}

    std::uint32_t AppendKey(const char* key) {
        const auto byteOffset = static_cast<std::uint32_t>(words.size() * 4 + byteTail.size());
        const std::size_t len = std::strlen(key);
        byteTail.insert(byteTail.end(), reinterpret_cast<const std::uint8_t*>(key),
                        reinterpret_cast<const std::uint8_t*>(key) + len + 1);
        return byteOffset;
    }

    void FinishKeys() {
        while (byteTail.size() % 4 != 0) byteTail.push_back(0xaa);
        for (std::size_t i = 0; i < byteTail.size(); i += 4) {
            std::uint32_t word = 0;
            std::memcpy(&word, byteTail.data() + i, 4);
            words.push_back(word);
        }
        byteTail.clear();
        keysTopWord = static_cast<std::uint32_t>(words.size());
    }

    std::uint32_t AppendV2ImplicitString(const std::u16string& text) {
        const auto offset = static_cast<std::uint32_t>(units16.size());
        for (char16_t ch : text) units16.push_back(static_cast<std::uint16_t>(ch));
        units16.push_back(0);
        return offset;
    }

    std::uint32_t AppendV2ShortString(const std::u16string& text) {
        const auto offset = static_cast<std::uint32_t>(units16.size());
        units16.push_back(static_cast<std::uint16_t>(0xdc00u | text.size()));
        for (char16_t ch : text) units16.push_back(static_cast<std::uint16_t>(ch));
        units16.push_back(0);
        return offset;
    }

    std::uint32_t AppendV2MediumString(const std::u16string& text) {
        const auto offset = static_cast<std::uint32_t>(units16.size());
        const auto len = static_cast<std::uint32_t>(text.size());
        units16.push_back(static_cast<std::uint16_t>(0xdfefu + (len >> 16)));
        units16.push_back(static_cast<std::uint16_t>(len & 0xffffu));
        for (char16_t ch : text) units16.push_back(static_cast<std::uint16_t>(ch));
        units16.push_back(0);
        return offset;
    }

    std::uint32_t AppendV2LongString(const std::u16string& text) {
        const auto offset = static_cast<std::uint32_t>(units16.size());
        const auto len = static_cast<std::uint32_t>(text.size());
        units16.push_back(0xdfffu);
        units16.push_back(static_cast<std::uint16_t>(len >> 16));
        units16.push_back(static_cast<std::uint16_t>(len & 0xffffu));
        for (char16_t ch : text) units16.push_back(static_cast<std::uint16_t>(ch));
        units16.push_back(0);
        return offset;
    }

    std::uint32_t AppendArray16(const std::vector<std::uint16_t>& items) {
        const auto offset = static_cast<std::uint32_t>(units16.size());
        units16.push_back(static_cast<std::uint16_t>(items.size()));
        for (std::uint16_t item : items) units16.push_back(item);
        return offset;
    }

    std::uint32_t AppendTable16(const std::vector<std::pair<std::uint16_t, std::uint16_t>>& entries) {
        const auto offset = static_cast<std::uint32_t>(units16.size());
        units16.push_back(static_cast<std::uint16_t>(entries.size()));
        for (const auto& entry : entries) units16.push_back(entry.first);
        for (const auto& entry : entries) units16.push_back(entry.second);
        return offset;
    }

    void Finish16BitUnits() {
        while (units16.size() % 2 != 0) units16.push_back(0);
        for (std::size_t i = 0; i < units16.size(); i += 2) {
            const std::uint32_t word = static_cast<std::uint32_t>(units16[i]) | (static_cast<std::uint32_t>(units16[i + 1]) << 16);
            words.push_back(word);
        }
        units16.clear();
        top16BitWord = static_cast<std::uint32_t>(words.size());
    }

    std::uint32_t AppendString(const std::u16string& text) {
        const auto offset = static_cast<std::uint32_t>(words.size());
        words.push_back(static_cast<std::uint32_t>(text.size()));
        std::vector<std::uint16_t> chars(text.begin(), text.end());
        chars.push_back(0);
        while (chars.size() % 2 != 0) chars.push_back(0);
        for (std::size_t i = 0; i < chars.size(); i += 2) {
            words.push_back(static_cast<std::uint32_t>(chars[i]) | (static_cast<std::uint32_t>(chars[i + 1]) << 16));
        }
        return URES_MAKE_RESOURCE(URES_STRING, offset);
    }

    std::uint32_t AppendAlias(const std::u16string& text) {
        const std::uint32_t stringRes = AppendString(text);
        return URES_MAKE_RESOURCE(URES_ALIAS, RES_GET_OFFSET(stringRes));
    }

    std::uint32_t AppendBinary(const std::vector<std::uint8_t>& bytes) {
        const auto offset = static_cast<std::uint32_t>(words.size());
        words.push_back(static_cast<std::uint32_t>(bytes.size()));
        std::vector<std::uint8_t> padded = bytes;
        while (padded.size() % 4 != 0) padded.push_back(0);
        for (std::size_t i = 0; i < padded.size(); i += 4) {
            std::uint32_t word = 0;
            std::memcpy(&word, padded.data() + i, 4);
            words.push_back(word);
        }
        return URES_MAKE_RESOURCE(URES_BINARY, offset);
    }

    std::uint32_t AppendIntVector(const std::vector<std::int32_t>& values) {
        const auto offset = static_cast<std::uint32_t>(words.size());
        words.push_back(static_cast<std::uint32_t>(values.size()));
        for (std::int32_t value : values) words.push_back(static_cast<std::uint32_t>(value));
        return URES_MAKE_RESOURCE(URES_INT_VECTOR, offset);
    }

    std::uint32_t AppendArray(const std::vector<std::uint32_t>& items) {
        const auto offset = static_cast<std::uint32_t>(words.size());
        words.push_back(static_cast<std::uint32_t>(items.size()));
        for (std::uint32_t item : items) words.push_back(item);
        return URES_MAKE_RESOURCE(URES_ARRAY, offset);
    }

    std::uint32_t AppendTable16Word(const std::vector<std::pair<std::uint16_t, std::uint32_t>>& entries) {
        const auto offset = static_cast<std::uint32_t>(words.size());
        std::vector<std::uint16_t> header;
        header.push_back(static_cast<std::uint16_t>(entries.size()));
        for (const auto& entry : entries) header.push_back(entry.first);
        if ((entries.size() & 1u) == 0u) header.push_back(0);
        for (std::size_t i = 0; i < header.size(); i += 2) {
            words.push_back(static_cast<std::uint32_t>(header[i]) | (static_cast<std::uint32_t>(header[i + 1]) << 16));
        }
        for (const auto& entry : entries) words.push_back(entry.second);
        return URES_MAKE_RESOURCE(URES_TABLE, offset);
    }

    std::uint32_t AppendTable32(const std::vector<std::pair<std::int32_t, std::uint32_t>>& entries) {
        const auto offset = static_cast<std::uint32_t>(words.size());
        words.push_back(static_cast<std::uint32_t>(entries.size()));
        for (const auto& entry : entries) words.push_back(static_cast<std::uint32_t>(entry.first));
        for (const auto& entry : entries) words.push_back(entry.second);
        return URES_MAKE_RESOURCE(URES_TABLE32, offset);
    }

    std::vector<std::uint32_t> words;
    std::uint32_t keysTopWord = 0;
    std::uint32_t top16BitWord = 0;

private:
    std::vector<std::uint8_t> byteTail;
    std::vector<std::uint16_t> units16;
};

void TestFormat2BundleAndDifferential() {
    BundleBuilder builder(1 + 7);
    const std::uint32_t keyAlias = builder.AppendKey("alias");
    const std::uint32_t keyArr = builder.AppendKey("arr");
    const std::uint32_t keyArr16 = builder.AppendKey("arr16");
    const std::uint32_t keyBin = builder.AppendKey("bin");
    const std::uint32_t keyInt = builder.AppendKey("int");
    const std::uint32_t keyVec = builder.AppendKey("ivec");
    const std::uint32_t keyStr = builder.AppendKey("str");
    const std::uint32_t keyStrV2 = builder.AppendKey("strv2");
    const std::uint32_t keyTbl16 = builder.AppendKey("tbl16");
    const std::uint32_t keyTbl32 = builder.AppendKey("tbl32");
    const std::uint32_t keyZEmpty = builder.AppendKey("zempty");
    builder.FinishKeys();

    const std::uint32_t v2ImplicitOff = builder.AppendV2ImplicitString(u"hello");
    const std::uint32_t v2ShortOff = builder.AppendV2ShortString(u"abc");
    const std::uint32_t v2MedOff = builder.AppendV2MediumString(std::u16string(20, u'm'));
    const std::uint32_t v2LongOff = builder.AppendV2LongString(std::u16string(4, u'L'));
    const std::uint32_t arr16Off = builder.AppendArray16({
        static_cast<std::uint16_t>(v2ImplicitOff),
        static_cast<std::uint16_t>(v2ShortOff),
    });
    const std::uint32_t tbl16Off = builder.AppendTable16({
        {static_cast<std::uint16_t>(keyAlias), static_cast<std::uint16_t>(v2MedOff)},
        {static_cast<std::uint16_t>(keyStr), static_cast<std::uint16_t>(v2LongOff)},
    });
    builder.Finish16BitUnits();

    const std::uint32_t resAlias = builder.AppendAlias(u"/other/path");
    const std::uint32_t resBin = builder.AppendBinary({0xde, 0xad, 0xbe, 0xef, 0x42});
    const std::uint32_t resVec = builder.AppendIntVector({-7, 0, 123456, INT32_MIN});
    const std::uint32_t resStr = builder.AppendString(u"wide-v1");
    const std::uint32_t resInt = URES_MAKE_RESOURCE(URES_INT, 0x0ffffffdu);
    const std::uint32_t resArr = builder.AppendArray({resStr, resInt, URES_MAKE_EMPTY_RESOURCE(URES_STRING)});
    const std::uint32_t resTbl32 = builder.AppendTable32({
        {static_cast<std::int32_t>(keyArr), resArr},
        {static_cast<std::int32_t>(keyInt), resInt},
    });
    const std::uint32_t rootRes = builder.AppendTable16Word({
        {static_cast<std::uint16_t>(keyAlias), resAlias},
        {static_cast<std::uint16_t>(keyArr), resArr},
        {static_cast<std::uint16_t>(keyArr16), URES_MAKE_RESOURCE(URES_ARRAY16, arr16Off)},
        {static_cast<std::uint16_t>(keyBin), resBin},
        {static_cast<std::uint16_t>(keyInt), resInt},
        {static_cast<std::uint16_t>(keyVec), resVec},
        {static_cast<std::uint16_t>(keyStr), resStr},
        {static_cast<std::uint16_t>(keyStrV2), URES_MAKE_RESOURCE(URES_STRING_V2, v2ShortOff)},
        {static_cast<std::uint16_t>(keyTbl16), URES_MAKE_RESOURCE(URES_TABLE16, tbl16Off)},
        {static_cast<std::uint16_t>(keyTbl32), resTbl32},
        {static_cast<std::uint16_t>(keyZEmpty), URES_MAKE_EMPTY_RESOURCE(URES_TABLE)},
    });

    const auto totalWords = static_cast<std::uint32_t>(builder.words.size());
    builder.words[0] = rootRes;
    builder.words[1 + URES_INDEX_LENGTH] = 7;
    builder.words[1 + URES_INDEX_KEYS_TOP] = builder.keysTopWord;
    builder.words[1 + URES_INDEX_RESOURCES_TOP] = totalWords;
    builder.words[1 + URES_INDEX_BUNDLE_TOP] = totalWords;
    builder.words[1 + URES_INDEX_MAX_TABLE_LENGTH] = 11;
    builder.words[1 + URES_INDEX_ATTRIBUTES] = 0;
    builder.words[1 + URES_INDEX_16BIT_TOP] = builder.top16BitWord;

    const Icu::DataInfo guestInfo = MakeValidInfo(2, 0);
    UDataInfo nativeInfo{};
    std::memcpy(&nativeInfo, &guestInfo, sizeof(nativeInfo));

    Icu::ResourceData guestData{};
    ::ResourceData nativeData{};
    std::int32_t guestErr = 0;
    UErrorCode nativeErr = U_ZERO_ERROR;
    const auto byteLength = static_cast<std::int32_t>(builder.words.size() * sizeof(std::uint32_t));
    res_read_nid_postfix(&guestData, &guestInfo, builder.words.data(), byteLength, &guestErr);
    res_read(&nativeData, &nativeInfo, builder.words.data(), byteLength, &nativeErr);
    Require(guestErr == U_ZERO_ERROR && nativeErr == U_ZERO_ERROR, "res_read success");
    Require(std::memcmp(&guestData, &nativeData, sizeof(guestData)) == 0, "res_read ResourceData matches ICU");

    Require(res_getResource_nid_postfix(&guestData, "str") == res_getResource(&nativeData, "str"), "res_getResource str");
    Require(res_getResource_nid_postfix(&guestData, "missing") == RES_BOGUS, "res_getResource missing");
    Require(res_getResource_nid_postfix(&guestData, nullptr) == RES_BOGUS, "res_getResource null key");

    std::int32_t gLen = -1;
    std::int32_t nLen = -1;
    const char16_t* gs = res_getStringNoTrace_nid_postfix(&guestData, resStr, &gLen);
    const char16_t* ns = res_getStringNoTrace(&nativeData, resStr, &nLen);
    Require(gs == ns && gLen == 7 && gLen == nLen, "res_getStringNoTrace v1");

    const std::uint32_t resV2Med = URES_MAKE_RESOURCE(URES_STRING_V2, v2MedOff);
    gs = res_getStringNoTrace_nid_postfix(&guestData, resV2Med, &gLen);
    ns = res_getStringNoTrace(&nativeData, resV2Med, &nLen);
    Require(gs == ns && gLen == 20 && gLen == nLen, "res_getStringNoTrace v2 medium");

    const std::uint32_t resV2Long = URES_MAKE_RESOURCE(URES_STRING_V2, v2LongOff);
    gs = res_getStringNoTrace_nid_postfix(&guestData, resV2Long, &gLen);
    ns = res_getStringNoTrace(&nativeData, resV2Long, &nLen);
    Require(gs == ns && gLen == 4 && gLen == nLen, "res_getStringNoTrace v2 long");

    gs = res_getStringNoTrace_nid_postfix(nullptr, 0, &gLen);
    ns = res_getStringNoTrace(nullptr, 0, &nLen);
    Require(gs != nullptr && gLen == 0 && gLen == nLen && *gs == 0, "res_getStringNoTrace empty");
    Require(res_getStringNoTrace_nid_postfix(nullptr, resInt, &gLen) == nullptr && gLen == 0, "res_getStringNoTrace mismatch");
    Require(res_getStringNoTrace_nid_postfix(&guestData, resStr, nullptr) == ns, "res_getStringNoTrace null length");

    const char16_t* ga = res_getAlias_nid_postfix(&guestData, resAlias, &gLen);
    const char16_t* na = res_getAlias(&nativeData, resAlias, &nLen);
    Require(ga == na && gLen == 11 && gLen == nLen, "res_getAlias match");
    ga = res_getAlias_nid_postfix(nullptr, URES_MAKE_EMPTY_RESOURCE(URES_ALIAS), &gLen);
    Require(ga != nullptr && gLen == 0 && *ga == 0, "res_getAlias empty");
    Require(res_getAlias_nid_postfix(nullptr, resInt, &gLen) == nullptr && gLen == 0, "res_getAlias mismatch");

    const std::uint8_t* gb = res_getBinaryNoTrace_nid_postfix(&guestData, resBin, &gLen);
    const std::uint8_t* nb = res_getBinaryNoTrace(&nativeData, resBin, &nLen);
    Require(gb == nb && gLen == 5 && gLen == nLen && gb[4] == 0x42, "res_getBinaryNoTrace match");
    Require(res_getBinaryNoTrace_nid_postfix(nullptr, URES_MAKE_EMPTY_RESOURCE(URES_BINARY), &gLen) != nullptr && gLen == 0, "res_getBinaryNoTrace empty");
    Require(res_getBinaryNoTrace_nid_postfix(nullptr, resInt, &gLen) == nullptr && gLen == 0, "res_getBinaryNoTrace mismatch");

    const std::int32_t* gv = res_getIntVectorNoTrace_nid_postfix(&guestData, resVec, &gLen);
    const std::int32_t* nv = res_getIntVectorNoTrace(&nativeData, resVec, &nLen);
    Require(gv == nv && gLen == 4 && gLen == nLen && gv[0] == -7 && gv[3] == INT32_MIN, "res_getIntVectorNoTrace match");
    Require(res_getIntVectorNoTrace_nid_postfix(nullptr, URES_MAKE_EMPTY_RESOURCE(URES_INT_VECTOR), &gLen) != nullptr && gLen == 0, "res_getIntVectorNoTrace empty");
    Require(res_getIntVectorNoTrace_nid_postfix(nullptr, resInt, &gLen) == nullptr && gLen == 0, "res_getIntVectorNoTrace mismatch");

    Require(res_countArrayItems_nid_postfix(&guestData, resArr) == 3, "res_countArrayItems ARRAY");
    Require(res_countArrayItems_nid_postfix(&guestData, URES_MAKE_RESOURCE(URES_ARRAY16, arr16Off)) == 2, "res_countArrayItems ARRAY16");
    Require(res_countArrayItems_nid_postfix(&guestData, rootRes) == 11, "res_countArrayItems TABLE");
    Require(res_countArrayItems_nid_postfix(&guestData, URES_MAKE_RESOURCE(URES_TABLE16, tbl16Off)) == 2, "res_countArrayItems TABLE16");
    Require(res_countArrayItems_nid_postfix(&guestData, resTbl32) == 2, "res_countArrayItems TABLE32");
    Require(res_countArrayItems_nid_postfix(nullptr, resInt) == 1, "res_countArrayItems scalar");
    Require(res_countArrayItems_nid_postfix(nullptr, URES_MAKE_EMPTY_RESOURCE(URES_ARRAY)) == 0, "res_countArrayItems empty");
    Require(res_countArrayItems_nid_postfix(nullptr, RES_BOGUS) == 0, "res_countArrayItems bogus");

    Require(res_getArrayItem_nid_postfix(&guestData, resArr, 0) == resStr, "res_getArrayItem ARRAY[0]");
    Require(res_getArrayItem_nid_postfix(&guestData, resArr, 1) == resInt, "res_getArrayItem ARRAY[1]");
    Require(res_getArrayItem_nid_postfix(&guestData, resArr, 3) == RES_BOGUS, "res_getArrayItem oob");
    Require(res_getArrayItem_nid_postfix(nullptr, resArr, -1) == RES_BOGUS, "res_getArrayItem negative");
    Require(res_getArrayItem_nid_postfix(&guestData, URES_MAKE_RESOURCE(URES_ARRAY16, arr16Off), 1) ==
                res_getArrayItem(&nativeData, URES_MAKE_RESOURCE(URES_ARRAY16, arr16Off), 1),
            "res_getArrayItem ARRAY16");

    const char* gKey = nullptr;
    const char* nKey = nullptr;
    Require(res_getTableItemByIndex_nid_postfix(&guestData, rootRes, 3, &gKey) ==
                    res_getTableItemByIndex(&nativeData, rootRes, 3, &nKey) &&
                gKey == nKey && std::string(gKey) == "bin",
            "res_getTableItemByIndex TABLE");
    Require(res_getTableItemByIndex_nid_postfix(&guestData, URES_MAKE_RESOURCE(URES_TABLE16, tbl16Off), 1, &gKey) ==
                    res_getTableItemByIndex(&nativeData, URES_MAKE_RESOURCE(URES_TABLE16, tbl16Off), 1, &nKey) &&
                gKey == nKey && std::string(gKey) == "str",
            "res_getTableItemByIndex TABLE16");
    Require(res_getTableItemByIndex_nid_postfix(&guestData, resTbl32, 0, &gKey) ==
                    res_getTableItemByIndex(&nativeData, resTbl32, 0, &nKey) &&
                gKey == nKey && std::string(gKey) == "arr",
            "res_getTableItemByIndex TABLE32");
    Require(res_getTableItemByIndex_nid_postfix(nullptr, rootRes, -1, &gKey) == RES_BOGUS, "res_getTableItemByIndex negative");
    Require(res_getTableItemByIndex_nid_postfix(&guestData, rootRes, 99, &gKey) == RES_BOGUS, "res_getTableItemByIndex oob");

    std::string lookupBuf = "tbl32";
    const char* lookupKey = lookupBuf.c_str();
    const char* nativeLookupKey = lookupBuf.c_str();
    std::int32_t gIdx = -9;
    std::int32_t nIdx = -9;
    Require(res_getTableItemByKey_nid_postfix(&guestData, rootRes, &gIdx, &lookupKey) ==
                    res_getTableItemByKey(&nativeData, rootRes, &nIdx, &nativeLookupKey) &&
                gIdx == nIdx && gIdx == 9 && lookupKey == nativeLookupKey && std::string(lookupKey) == "tbl32",
            "res_getTableItemByKey found");

    const char* missingKey = "zzz";
    gIdx = 5;
    Require(res_getTableItemByKey_nid_postfix(&guestData, rootRes, &gIdx, &missingKey) == RES_BOGUS && gIdx == -1,
            "res_getTableItemByKey missing sets -1");

    const char* emptyKey = nullptr;
    Require(res_getTableItemByKey_nid_postfix(nullptr, rootRes, nullptr, &emptyKey) == RES_BOGUS,
            "res_getTableItemByKey null *key");
    Require(res_getTableItemByKey_nid_postfix(nullptr, rootRes, nullptr, nullptr) == RES_BOGUS,
            "res_getTableItemByKey null key");
    const char* nonTableKey = "x";
    gIdx = 42;
    Require(res_getTableItemByKey_nid_postfix(nullptr, URES_MAKE_EMPTY_RESOURCE(URES_TABLE), &gIdx, &nonTableKey) == RES_BOGUS &&
                gIdx == 42,
            "res_getTableItemByKey empty table keeps index");
}

void TestFormat10AndFormat30Pool() {
    std::vector<std::uint32_t> v10(16, 0);
    std::memcpy(v10.data() + 4, "k1\0\0", 4);
    v10[5] = 1;
    v10[6] = static_cast<std::uint32_t>('Z');
    v10[8] = (16u << 16) | 1u;
    v10[9] = URES_MAKE_RESOURCE(URES_STRING, 5);
    v10[0] = URES_MAKE_RESOURCE(URES_TABLE, 8);
    const Icu::DataInfo info10 = MakeValidInfo(1, 0);
    Icu::ResourceData data10{};
    std::int32_t err = 0;
    res_read_nid_postfix(&data10, &info10, v10.data(), static_cast<std::int32_t>(v10.size() * 4), &err);
    Require(err == U_ZERO_ERROR && data10.localKeyLimit == 0x10000, "res_read format 1.0");
    const std::uint32_t got = res_getResource_nid_postfix(&data10, "k1");
    std::int32_t len = 0;
    const char16_t* str = res_getStringNoTrace_nid_postfix(&data10, got, &len);
    Require(str != nullptr && len == 1 && str[0] == u'Z', "format 1.0 string lookup");

    BundleBuilder b3(1 + 8);
    const std::uint32_t localKey = b3.AppendKey("local");
    b3.FinishKeys();
    const std::uint32_t localStrOff = b3.AppendV2ImplicitString(u"loc");
    const std::uint32_t tbl16Off = b3.AppendTable16({
        {static_cast<std::uint16_t>(localKey), static_cast<std::uint16_t>(1 + localStrOff)},
    });
    b3.Finish16BitUnits();
    b3.words[0] = URES_MAKE_RESOURCE(URES_TABLE16, tbl16Off);
    b3.words[1 + URES_INDEX_LENGTH] = (1u << 8) | 8u;
    b3.words[1 + URES_INDEX_KEYS_TOP] = b3.keysTopWord;
    b3.words[1 + URES_INDEX_RESOURCES_TOP] = static_cast<std::uint32_t>(b3.words.size());
    b3.words[1 + URES_INDEX_BUNDLE_TOP] = static_cast<std::uint32_t>(b3.words.size());
    b3.words[1 + URES_INDEX_MAX_TABLE_LENGTH] = 1;
    b3.words[1 + URES_INDEX_ATTRIBUTES] = URES_ATT_USES_POOL_BUNDLE | (1u << 16);
    b3.words[1 + URES_INDEX_16BIT_TOP] = b3.top16BitWord;
    b3.words[1 + URES_INDEX_POOL_CHECKSUM] = 0;

    const Icu::DataInfo info30 = MakeValidInfo(3, 0);
    Icu::ResourceData data30{};
    err = 0;
    res_read_nid_postfix(&data30, &info30, b3.words.data(), static_cast<std::int32_t>(b3.words.size() * 4), &err);
    Require(err == U_ZERO_ERROR && data30.usesPoolBundle == 1 && data30.poolStringIndexLimit == 1 &&
                data30.poolStringIndex16Limit == 1,
            "res_read format 3.0 pool metadata");
    const std::array<std::uint16_t, 5> poolStrings{'p', 'o', 'o', 'l', 0};
    data30.poolBundleStrings = poolStrings.data();
    const char* poolKey = "pkey";
    data30.poolBundleKeys = poolKey;

    const std::uint32_t poolStrRes = URES_MAKE_RESOURCE(URES_STRING_V2, 0);
    const char16_t* ps = res_getStringNoTrace_nid_postfix(&data30, poolStrRes, &len);
    Require(ps == reinterpret_cast<const char16_t*>(poolStrings.data()) && len == 4, "format 3.0 pool string");
    const std::uint32_t locRes = res_getResource_nid_postfix(&data30, "local");
    ps = res_getStringNoTrace_nid_postfix(&data30, locRes, &len);
    Require(ps != nullptr && len == 3 && ps[0] == u'l', "format 3.0 local string after pool limit");
}

void TestErrorsAndGuestSafety() {
    const auto* unmapped = reinterpret_cast<const void*>(std::uintptr_t{0x10});
    Icu::ResourceData data{};
    data.rootRes = 1;
    Icu::DataInfo info = MakeValidInfo(2, 0);
    std::int32_t err = U_ILLEGAL_ARGUMENT_ERROR;

    errno = 55;
    res_read_nid_postfix(&data, static_cast<const Icu::DataInfo*>(unmapped), unmapped, 4, &err);
    Require(err == U_ILLEGAL_ARGUMENT_ERROR && data.rootRes == 0 && errno == 55,
            "res_read prefailed zeroes ResourceData and skips info/bytes");

    err = 0;
    Icu::DataInfo smallInfo = info;
    smallInfo.size = 4;
    res_read_nid_postfix(&data, &smallInfo, unmapped, 4, &err);
    Require(err == U_INVALID_FORMAT_ERROR, "res_read rejects small DataInfo without reading bytes");

    err = 0;
    Icu::DataInfo badMagic = info;
    badMagic.dataFormat[0] = 'X';
    res_read_nid_postfix(&data, &badMagic, unmapped, 4, &err);
    Require(err == U_INVALID_FORMAT_ERROR, "res_read rejects non-ResB format");

    err = 0;
    const std::uint32_t nonTableRoot = URES_MAKE_RESOURCE(URES_INT, 1);
    res_read_nid_postfix(&data, &info, &nonTableRoot, 4, &err);
    Require(err == U_INVALID_FORMAT_ERROR, "res_read rejects truncated / non-table root");

    errno = 55;
    RequireThrows([&] { res_read_nid_postfix(nullptr, &info, &nonTableRoot, 4, &err); }, "res_read null data");
    RequireThrows([&] { res_read_nid_postfix(&data, &info, &nonTableRoot, 4, nullptr); }, "res_read null error");
    RequireThrows([&] { res_read_nid_postfix(&data, &info, &nonTableRoot, 4, static_cast<std::int32_t*>(const_cast<void*>(unmapped))); },
                  "res_read unreadable error");
    err = 0;
    RequireThrows([&] { res_read_nid_postfix(&data, nullptr, &nonTableRoot, 4, &err); }, "res_read null info");
    RequireThrows([&] { res_read_nid_postfix(&data, &info, nullptr, 4, &err); }, "res_read null bytes");
    RequireThrows([&] { res_read_nid_postfix(&data, &info, unmapped, 4, &err); }, "res_read unreadable bytes");
    Require(errno == 55, "errno preserved on throws");

    std::vector<std::uint32_t> v10(16, 0);
    std::memcpy(v10.data() + 4, "k1\0\0", 4);
    v10[5] = 1;
    v10[6] = static_cast<std::uint32_t>('Z');
    v10[8] = (16u << 16) | 1u;
    v10[9] = URES_MAKE_RESOURCE(URES_STRING, 5);
    v10[0] = URES_MAKE_RESOURCE(URES_TABLE, 8);
    const Icu::DataInfo info10 = MakeValidInfo(1, 0);
    res_read_nid_postfix(&data, &info10, v10.data(), static_cast<std::int32_t>(v10.size() * 4), &err);
    Require(err == 0, "v10 setup");

    Pages pages;
    std::memcpy(pages.begin, &err, sizeof(err));
    const std::int32_t zero = 0;
    std::memcpy(pages.begin + 16, &zero, sizeof(zero));
    const char* readOnlyMissing = "missing";
    const char* readOnlyFound = "k1";
    std::memcpy(pages.begin + 32, &readOnlyMissing, sizeof(readOnlyMissing));
    std::memcpy(pages.begin + 40, &readOnlyFound, sizeof(readOnlyFound));
    pages.Protect(0, false);

    auto* roZero = reinterpret_cast<std::int32_t*>(pages.begin + 16);
    auto* roData = reinterpret_cast<Icu::ResourceData*>(pages.begin + 64);
    RequireThrows([&] { res_read_nid_postfix(roData, &info, &nonTableRoot, 4, roZero); }, "res_read unwritable data");
    RequireThrows([&] { res_read_nid_postfix(&data, &badMagic, unmapped, 4, roZero); }, "res_read unwritable status");

    auto* roLen = reinterpret_cast<std::int32_t*>(pages.begin + 16);
    RequireThrows([&] { res_getStringNoTrace_nid_postfix(nullptr, 0, roLen); }, "res_getStringNoTrace unwritable length");
    RequireThrows([&] { res_getAlias_nid_postfix(nullptr, URES_MAKE_EMPTY_RESOURCE(URES_ALIAS), roLen); },
                  "res_getAlias unwritable length");
    RequireThrows([&] { res_getBinaryNoTrace_nid_postfix(nullptr, URES_MAKE_EMPTY_RESOURCE(URES_BINARY), roLen); },
                  "res_getBinaryNoTrace unwritable length");
    RequireThrows([&] { res_getIntVectorNoTrace_nid_postfix(nullptr, URES_MAKE_EMPTY_RESOURCE(URES_INT_VECTOR), roLen); },
                  "res_getIntVectorNoTrace unwritable length");

    std::int32_t idx = 7;
    auto* roMissingPtr = reinterpret_cast<const char**>(pages.begin + 32);
    auto* roFoundPtr = reinterpret_cast<const char**>(pages.begin + 40);
    Require(res_getTableItemByKey_nid_postfix(&data, data.rootRes, &idx, roMissingPtr) == RES_BOGUS && idx == -1,
            "res_getTableItemByKey read-only missing key");
    idx = 7;
    RequireThrows([&] { res_getTableItemByKey_nid_postfix(&data, data.rootRes, &idx, roFoundPtr); },
                  "res_getTableItemByKey unwritable found key throws");
    Require(idx == 7, "res_getTableItemByKey preserves index on unwritable key throw");
    RequireThrows([&] { res_getTableItemByKey_nid_postfix(&data, data.rootRes, roZero, roFoundPtr); },
                  "res_getTableItemByKey unwritable index throws");
    RequireThrows([&] { res_getTableItemByIndex_nid_postfix(&data, data.rootRes, 0, roFoundPtr); },
                  "res_getTableItemByIndex unwritable key throws");
    Require(res_getTableItemByIndex_nid_postfix(&data, data.rootRes, 5, roFoundPtr) == RES_BOGUS,
            "res_getTableItemByIndex oob skips read-only key");

    const std::uint32_t v1Str = URES_MAKE_RESOURCE(URES_STRING, 1);
    RequireThrows([&] { res_getStringNoTrace_nid_postfix(nullptr, v1Str, nullptr); }, "res_getStringNoTrace null data");
    RequireThrows([&] { res_getStringNoTrace_nid_postfix(static_cast<const Icu::ResourceData*>(unmapped), v1Str, nullptr); },
                  "res_getStringNoTrace unreadable data");
    RequireThrows([&] { res_getResource_nid_postfix(nullptr, "k"); }, "res_getResource null data");
    RequireThrows([&] { res_countArrayItems_nid_postfix(nullptr, URES_MAKE_RESOURCE(URES_ARRAY, 1)); },
                  "res_countArrayItems null data");
    RequireThrows([&] { res_getArrayItem_nid_postfix(nullptr, URES_MAKE_RESOURCE(URES_ARRAY, 1), 0); },
                  "res_getArrayItem null data");
    RequireThrows([&] { res_getTableItemByIndex_nid_postfix(nullptr, URES_MAKE_RESOURCE(URES_TABLE, 1), 0, nullptr); },
                  "res_getTableItemByIndex null data");
    const char* badPtr = static_cast<const char*>(unmapped);
    RequireThrows([&] { res_getTableItemByKey_nid_postfix(nullptr, URES_MAKE_RESOURCE(URES_TABLE, 1), &idx, &badPtr); },
                  "res_getTableItemByKey null data");
}

} // namespace

int main() {
    TestPublicTypes();
    TestFormat2BundleAndDifferential();
    TestFormat10AndFormat30Pool();
    TestErrorsAndGuestSafety();
    std::puts("ICU resource data tests passed");
    return 0;
}
