#include "prx/libharfbuzz/include/Layout.hpp"
#include "FreeTypeGuestOutputFixture.hpp"

#include <hb-ot.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <initializer_list>
#include <limits>
#include <source_location>
#include <span>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "HarfBuzz layout check failed at line %u\n", location.line());
        std::abort();
    }
}

template <typename TFunction, typename TModule>
static TFunction Import(TModule module, const char* name, const char* fallback = nullptr) {
#ifdef _WIN32
    auto address = GetProcAddress(module, name);
    if (!address && fallback) address = GetProcAddress(module, fallback);
#else
    auto address = dlsym(module, name);
    if (!address && fallback) address = dlsym(module, fallback);
#endif
    if (!address) std::fprintf(stderr, "Missing symbol: %s\n", name);
    Require(address != nullptr);
    return std::bit_cast<TFunction>(address);
}

struct Api {
    decltype(&hb_ot_layout_table_get_script_tags_nid_postfix) scripts;
    decltype(&hb_ot_layout_script_get_language_tags_nid_postfix) languages;
    decltype(&hb_ot_layout_language_find_feature_nid_postfix) feature;
};

struct Table {
    std::vector<char> bytes;

    void Word(unsigned value) {
        bytes.push_back(static_cast<char>(value >> 8));
        bytes.push_back(static_cast<char>(value));
    }

    void Tag(hb_tag_t value) {
        Word(value >> 16);
        Word(value & 0xFFFF);
    }

    void Offset(std::size_t position, std::size_t base, std::size_t target) {
        Require(target >= base && target - base <= 0xFFFF);
        const auto value = static_cast<unsigned>(target - base);
        bytes[position] = static_cast<char>(value >> 8);
        bytes[position + 1] = static_cast<char>(value);
    }

    void Language(unsigned required, std::initializer_list<unsigned> features) {
        Word(0);
        Word(required);
        Word(static_cast<unsigned>(features.size()));
        for (const auto feature : features) Word(feature);
    }

    void Script(bool latin) {
        const auto base = bytes.size();
        Word(0);
        Word(latin ? 2 : 0);
        if (latin) {
            Tag(HB_TAG('E', 'N', 'G', ' '));
            Word(0);
            Tag(HB_TAG('T', 'R', 'K', ' '));
            Word(0);
        }
        Offset(base, base, bytes.size());
        if (latin) Language(2, {0, 1});
        else Language(0xFFFF, {2});
        if (latin) {
            Offset(base + 8, base, bytes.size());
            Language(0xFFFF, {1});
            Offset(base + 14, base, bytes.size());
            Language(0xFFFF, {0, 2});
        }
    }
};

static Table LayoutTable(std::array<hb_tag_t, 3> features) {
    Table table;
    table.Word(1);
    table.Word(0);
    table.Word(10);
    table.Word(0);
    table.Word(0);
    table.Word(2);
    table.Tag(HB_TAG('D', 'F', 'L', 'T'));
    table.Word(0);
    table.Tag(HB_TAG('l', 'a', 't', 'n'));
    table.Word(0);
    table.Offset(16, 10, table.bytes.size());
    table.Script(false);
    table.Offset(22, 10, table.bytes.size());
    table.Script(true);
    const auto featureBase = table.bytes.size();
    table.Offset(6, 0, featureBase);
    table.Word(3);
    for (const auto feature : features) {
        table.Tag(feature);
        table.Word(0);
    }
    for (unsigned i = 0; i < features.size(); ++i) {
        table.Offset(featureBase + 6 + i * 6, featureBase, table.bytes.size());
        table.Word(0);
        table.Word(0);
    }
    table.Offset(8, 0, table.bytes.size());
    table.Word(0);
    return table;
}

static hb_face_t* Face(std::span<const char> substitution, std::span<const char> positioning) {
    auto* face = hb_face_builder_create();
    Require(face != hb_face_get_empty());
    for (const auto& [tag, bytes] : std::array<std::pair<hb_tag_t, std::span<const char>>, 2>{{{HB_OT_TAG_GSUB, substitution}, {HB_OT_TAG_GPOS, positioning}}}) {
        auto* blob = hb_blob_create(bytes.data(), static_cast<unsigned>(bytes.size()), HB_MEMORY_MODE_DUPLICATE, nullptr, nullptr);
        Require(hb_face_builder_add_table(face, tag, blob));
        hb_blob_destroy(blob);
    }
    return face;
}

static void CheckQueries(const Api& api, hb_face_t* guest, hb_face_t* native) {
    constexpr std::array<unsigned, 6> offsets{0, 1, 2, 3, 65535, std::numeric_limits<unsigned>::max()};
    constexpr std::array<unsigned, 5> capacities{0, 1, 2, 7, std::numeric_limits<unsigned>::max()};
    constexpr std::array<hb_tag_t, 4> tables{HB_OT_TAG_GSUB, HB_OT_TAG_GPOS, HB_TAG('G', 'D', 'E', 'F'), 0};
    for (const auto table : tables) {
        for (const auto offset : offsets) {
            Require(api.scripts(guest, table, offset, nullptr, nullptr) == hb_ot_layout_table_get_script_tags(native, table, offset, nullptr, nullptr));
            for (const auto capacity : capacities) {
                std::array<hb_tag_t, 9> actual, expected;
                actual.fill(0xA5A5A5A5);
                expected = actual;
                unsigned actualCount = capacity, expectedCount = capacity;
                Require(api.scripts(guest, table, offset, &actualCount, actual.data() + 1) == hb_ot_layout_table_get_script_tags(native, table, offset, &expectedCount, expected.data() + 1));
                Require(actualCount == expectedCount && actual == expected);
                for (const auto script : std::array<unsigned, 4>{0, 1, 2, std::numeric_limits<unsigned>::max()}) {
                    actual.fill(0xA5A5A5A5);
                    expected = actual;
                    actualCount = capacity;
                    expectedCount = capacity;
                    Require(api.languages(guest, table, script, offset, &actualCount, actual.data() + 1) == hb_ot_layout_script_get_language_tags(native, table, script, offset, &expectedCount, expected.data() + 1));
                    Require(actualCount == expectedCount && actual == expected);
                    Require(api.languages(guest, table, script, offset, nullptr, nullptr) == hb_ot_layout_script_get_language_tags(native, table, script, offset, nullptr, nullptr));
                }
            }
        }
        for (const auto script : std::array<unsigned, 4>{0, 1, 2, std::numeric_limits<unsigned>::max()}) {
            for (const auto language : std::array<unsigned, 5>{0, 1, 2, HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX, std::numeric_limits<unsigned>::max()}) {
                for (const auto feature : std::array<hb_tag_t, 8>{HB_TAG('k', 'e', 'r', 'n'), HB_TAG('l', 'i', 'g', 'a'), HB_TAG('r', 'l', 'i', 'g'), HB_TAG('d', 'i', 's', 't'), HB_TAG('m', 'a', 'r', 'k'), HB_TAG('z', 'z', 'z', 'z'), 0, 0xFFFFFFFF}) {
                    std::array<unsigned, 3> actual{0xA5A5A5A5, 0xA5A5A5A5, 0xA5A5A5A5}, expected = actual;
                    Require(api.feature(guest, table, script, language, feature, actual.data() + 1) == hb_ot_layout_language_find_feature(native, table, script, language, feature, expected.data() + 1));
                    Require(actual == expected);
                    Require(api.feature(guest, table, script, language, feature, nullptr) == hb_ot_layout_language_find_feature(native, table, script, language, feature, nullptr));
                }
            }
        }
        for (const auto offset : offsets) {
            std::array<unsigned, 8> actual{}, expected{};
            actual.fill(0xA5A5A5A5);
            actual[1] = 2;
            expected = actual;
            Require(api.scripts(guest, table, offset, actual.data() + 1, actual.data() + 1) == hb_ot_layout_table_get_script_tags(native, table, offset, expected.data() + 1, expected.data() + 1));
            Require(actual == expected);
            actual.fill(0xA5A5A5A5);
            actual[1] = 2;
            expected = actual;
            Require(api.languages(guest, table, 1, offset, actual.data() + 1, actual.data() + 1) == hb_ot_layout_script_get_language_tags(native, table, 1, offset, expected.data() + 1, expected.data() + 1));
            Require(actual == expected);
        }
        unsigned zero = 0;
        Require(api.scripts(guest, table, 0, &zero, nullptr) == hb_ot_layout_table_get_script_tags(native, table, 0, nullptr, nullptr));
        Require(zero == 0);
    }
}

static void CheckLiterals(const Api& api, hb_face_t* face) {
    unsigned count = 6;
    std::array<hb_tag_t, 6> tags{};
    Require(api.scripts(face, HB_OT_TAG_GSUB, 0, &count, tags.data()) == 2);
    Require(count == 2 && tags[0] == HB_TAG('D', 'F', 'L', 'T') && tags[1] == HB_TAG('l', 'a', 't', 'n') && tags[2] == 0);
    count = 6;
    Require(api.languages(face, HB_OT_TAG_GSUB, 1, 0, &count, tags.data()) == 2);
    Require(count == 2 && tags[0] == HB_TAG('E', 'N', 'G', ' ') && tags[1] == HB_TAG('T', 'R', 'K', ' '));
    count = 1;
    Require(api.languages(face, HB_OT_TAG_GPOS, 1, 1, &count, tags.data()) == 2 && count == 1 && tags[0] == HB_TAG('T', 'R', 'K', ' '));
    unsigned index = 0xA5A5A5A5;
    Require(api.feature(face, HB_OT_TAG_GSUB, 1, 0, HB_TAG('l', 'i', 'g', 'a'), &index) == 1 && index == 1);
    Require(api.feature(face, HB_OT_TAG_GSUB, 0, HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX, HB_TAG('r', 'l', 'i', 'g'), &index) == 1 && index == 2);
    Require(api.feature(face, HB_OT_TAG_GSUB, 1, HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX, HB_TAG('r', 'l', 'i', 'g'), &index) == 0 && index == HB_OT_LAYOUT_NO_FEATURE_INDEX);
    Require(api.feature(face, HB_OT_TAG_GPOS, 1, 1, HB_TAG('m', 'a', 'r', 'k'), &index) == 1 && index == 2);
    Require(api.feature(face, HB_OT_TAG_GSUB, 1, 1, HB_TAG('m', 'a', 'r', 'k'), &index) == 0 && index == HB_OT_LAYOUT_NO_FEATURE_INDEX);
}

#ifdef _WIN32
static void CheckProtected(const Api& api, hb_face_t* face, const FreeTypeTest::KernelApi& kernel) {
    FreeTypeTest::GuestOutput output(kernel);
    for (const unsigned kind : {0u, 1u, 2u, 3u}) {
        for (const bool alias : {false, true}) {
            if (kind >= 2 && alias) continue;
            for (const bool hostReadOnly : {false, true}) {
                for (unsigned mask = 1; mask < 4; ++mask) {
                    if ((kind >= 2 || alias) && mask != 1) continue;
                    output.Reset();
                    const std::size_t countOffset = 64;
                    const std::size_t tagsOffset = alias ? countOffset : FreeTypeTest::GuestOutput::Page + 64;
                    auto* count = output.Record<unsigned>(countOffset);
                    auto* tags = output.Record<hb_tag_t>(tagsOffset);
                    *count = 2;
                    const auto before = output.Snapshot();
                    auto expected = before;
                    auto* expectedCount = reinterpret_cast<unsigned*>(expected.data() + countOffset);
                    auto* expectedTags = reinterpret_cast<hb_tag_t*>(expected.data() + tagsOffset);
                    const hb_tag_t feature = kind == 2 ? HB_TAG('l', 'i', 'g', 'a') : HB_TAG('z', 'z', 'z', 'z');
                    const unsigned expectedResult = kind == 0 ? hb_ot_layout_table_get_script_tags(face, HB_OT_TAG_GSUB, 0, expectedCount, expectedTags)
                        : kind == 1 ? hb_ot_layout_script_get_language_tags(face, HB_OT_TAG_GSUB, 1, 0, expectedCount, expectedTags)
                        : static_cast<unsigned>(hb_ot_layout_language_find_feature(face, HB_OT_TAG_GSUB, 1, 0, feature, expectedCount));
                    const auto query = [&] {
                        if (kind == 0) return api.scripts(face, HB_OT_TAG_GSUB, 0, count, tags);
                        if (kind == 1) return api.languages(face, HB_OT_TAG_GSUB, 1, 0, count, tags);
                        return static_cast<unsigned>(api.feature(face, HB_OT_TAG_GSUB, 1, 0, feature, count));
                    };
                    for (unsigned page = 0; page < 2; ++page) {
                        if (!(mask & (1u << page))) continue;
                        output.Protect(page, false);
                        if (!hostReadOnly) output.PermitNativeWrite(page);
                    }
                    try { query(); }
                    catch (const std::exception&) {}
                    output.Unchanged(before);
                    output.Protect(0, true);
                    output.Protect(1, true);
                    Require(query() == expectedResult && output.Snapshot() == expected);
                }
            }
        }
    }
    output.Reset();
    auto* count = output.Record<unsigned>(64);
    auto* tags = output.Record<hb_tag_t>(FreeTypeTest::GuestOutput::Page + 64);
    *count = 0;
    output.Protect(1, false);
    Require(api.scripts(face, HB_OT_TAG_GSUB, 0, count, tags) == 2 && *count == 0);
    *count = 2;
    Require(api.scripts(face, HB_OT_TAG_GSUB, 2, count, tags) == 2 && *count == 0);
    *count = 0;
    Require(api.languages(face, HB_OT_TAG_GSUB, 1, 0, count, tags) == 2 && *count == 0);
    output.Protect(1, true);
}
#endif

int main(int argc, char** argv) {
    Require(argc == 4);
    const bool patched = std::strcmp(argv[2], "patched") == 0;
    Require(patched || std::strcmp(argv[2], "unpatched") == 0);
#ifdef _WIN32
    auto module = LoadLibraryW(std::filesystem::path(argv[1]).c_str());
    auto kernel = LoadLibraryW(std::filesystem::path(argv[3]).c_str());
    Require(kernel != nullptr);
    if (!module) std::fprintf(stderr, "LoadLibraryW failed: %lu\n", GetLastError());
#else
    auto module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!module) std::fprintf(stderr, "%s\n", dlerror());
#endif
    Require(module != nullptr);
    Api api{};
#define LoadExport(field, name, nid) api.field = Import<decltype(api.field)>(module, patched ? nid : #name "_nid_postfix", patched ? nullptr : #name)
    LoadExport(scripts, hb_ot_layout_table_get_script_tags, "wf6CfVJj9PQ");
    LoadExport(languages, hb_ot_layout_script_get_language_tags, "HH-sRSNiSaU");
    LoadExport(feature, hb_ot_layout_language_find_feature, "qos3JIrKrDg");
#undef LoadExport
#ifdef _WIN32
    const FreeTypeTest::KernelApi kernelApi{
        Import<decltype(FreeTypeTest::KernelApi::allocate)>(kernel, patched ? "rTXw65xmLIA" : "sceKernelAllocateDirectMemory"),
        Import<decltype(FreeTypeTest::KernelApi::map)>(kernel, patched ? "L-Q3LEjIbgA" : "sceKernelMapDirectMemory"),
        Import<decltype(FreeTypeTest::KernelApi::protect)>(kernel, patched ? "vSMAm3cxYTY" : "sceKernelMprotect"),
        Import<decltype(FreeTypeTest::KernelApi::unmap)>(kernel, patched ? "cQke9UuBQOk" : "sceKernelMunmap"),
        Import<decltype(FreeTypeTest::KernelApi::release)>(kernel, patched ? "MBuItvba6z8" : "sceKernelReleaseDirectMemory")
    };
#endif
    try {
    const auto substitution = LayoutTable({HB_TAG('k', 'e', 'r', 'n'), HB_TAG('l', 'i', 'g', 'a'), HB_TAG('r', 'l', 'i', 'g')});
    const auto positioning = LayoutTable({HB_TAG('d', 'i', 's', 't'), HB_TAG('k', 'e', 'r', 'n'), HB_TAG('m', 'a', 'r', 'k')});
    auto* guest = Face(substitution.bytes, positioning.bytes);
    auto* native = Face(substitution.bytes, positioning.bytes);
    CheckLiterals(api, guest);
    CheckQueries(api, guest, native);
#ifdef _WIN32
    CheckProtected(api, guest, kernelApi);
#endif
    hb_face_destroy(guest);
    hb_face_destroy(native);
    for (const auto length : std::array<std::size_t, 8>{0, 1, 4, 9, 10, 11, 21, 23}) {
        const auto malformed = std::span(substitution.bytes).first(length);
        guest = Face(malformed, {});
        native = Face(malformed, {});
        CheckQueries(api, guest, native);
        hb_face_destroy(guest);
        hb_face_destroy(native);
    }
    CheckQueries(api, hb_face_get_empty(), hb_face_get_empty());
    } catch (const std::exception& error) {
        std::fprintf(stderr, "HarfBuzz layout query unexpectedly threw: %s\n", error.what());
        Require(false);
    }
#ifdef _WIN32
    Require(FreeLibrary(module) != 0 && FreeLibrary(kernel) != 0);
#else
    Require(dlclose(module) == 0);
#endif
}
