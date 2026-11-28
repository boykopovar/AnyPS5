#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include "ucmndata.h"
#include "ulocimp.h"
#include "unicode/udata.h"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

constexpr std::size_t HeaderWords = 7;

struct TableEntry {
    const char* key;
    std::vector<std::pair<const char*, std::u16string_view>> items;
};

void Append16(std::vector<std::uint8_t>& payload, std::uint16_t value) {
    const auto position = payload.size();
    payload.resize(position + sizeof(value));
    std::memcpy(payload.data() + position, &value, sizeof(value));
}

void Append32(std::vector<std::uint8_t>& payload, std::uint32_t value) {
    const auto position = payload.size();
    payload.resize(position + sizeof(value));
    std::memcpy(payload.data() + position, &value, sizeof(value));
}

void Align(std::vector<std::uint8_t>& payload) {
    while (payload.size() % 4 != 0) payload.push_back(0);
}

std::uint16_t AddKey(std::vector<std::uint8_t>& payload, std::string_view key) {
    const auto offset = static_cast<std::uint16_t>(payload.size());
    payload.insert(payload.end(), key.begin(), key.end());
    payload.push_back(0);
    return offset;
}

std::uint32_t AppendString(std::vector<std::uint8_t>& payload, std::u16string_view text) {
    Align(payload);
    const auto offset = static_cast<std::uint32_t>(payload.size() / sizeof(std::uint32_t));
    Append32(payload, static_cast<std::uint32_t>(text.size()));
    for (const char16_t unit : text) Append16(payload, unit);
    Append16(payload, 0);
    Align(payload);
    return offset;
}

std::uint32_t AppendTable(std::vector<std::uint8_t>& payload,
                          const std::vector<std::pair<std::uint16_t, std::uint32_t>>& items) {
    Align(payload);
    const auto offset = static_cast<std::uint32_t>(payload.size() / sizeof(std::uint32_t));
    Append16(payload, static_cast<std::uint16_t>(items.size()));
    for (const auto& item : items) Append16(payload, item.first);
    if ((items.size() & 1u) == 0) Append16(payload, 0);
    for (const auto& item : items) Append32(payload, item.second);
    return (std::uint32_t{2} << 28) | offset;
}

std::vector<std::uint32_t> FinishBundle(std::vector<std::uint8_t>& payload, std::uint32_t root, std::uint32_t keysTop,
                                        std::uint32_t bundleTop);

std::vector<std::uint32_t> BuildBundle(const std::vector<TableEntry>& entries) {
    std::vector<std::uint8_t> payload(HeaderWords * sizeof(std::uint32_t), 0);
    std::vector<std::uint16_t> topKeys;
    for (const auto& entry : entries) topKeys.push_back(AddKey(payload, entry.key));
    std::vector<std::vector<std::uint16_t>> itemKeys;
    for (const auto& entry : entries) {
        std::vector<std::uint16_t> keys;
        for (const auto& item : entry.items) keys.push_back(AddKey(payload, item.first));
        itemKeys.push_back(std::move(keys));
    }
    Align(payload);
    const auto keysTop = static_cast<std::uint32_t>(payload.size() / sizeof(std::uint32_t));

    std::vector<std::uint32_t> tables;
    for (std::size_t index = 0; index < entries.size(); ++index) {
        std::vector<std::pair<std::uint16_t, std::uint32_t>> items;
        for (std::size_t item = 0; item < entries[index].items.size(); ++item) {
            items.emplace_back(itemKeys[index][item], AppendString(payload, entries[index].items[item].second));
        }
        tables.push_back(AppendTable(payload, items));
    }
    const auto root = AppendTable(payload, [&] {
        std::vector<std::pair<std::uint16_t, std::uint32_t>> items;
        for (std::size_t index = 0; index < entries.size(); ++index) items.emplace_back(topKeys[index], tables[index]);
        return items;
    }());
    Align(payload);
    const auto bundleTop = static_cast<std::uint32_t>(payload.size() / sizeof(std::uint32_t));

    return FinishBundle(payload, root, keysTop, bundleTop);
}

std::vector<std::uint32_t> FinishBundle(std::vector<std::uint8_t>& payload, std::uint32_t root, std::uint32_t keysTop,
                                        std::uint32_t bundleTop) {
    const std::array<std::uint32_t, HeaderWords> header{root, 6, keysTop, bundleTop, bundleTop, 2, 0};
    std::memcpy(payload.data(), header.data(), sizeof(header));
    const DataHeader resourceHeader{
        sizeof(DataHeader), 0xda, 0x27,
        {sizeof(UDataInfo), 0, U_IS_BIG_ENDIAN, U_CHARSET_FAMILY, U_SIZEOF_UCHAR, 0,
         {'R', 'e', 's', 'B'}, {1, 2, 0, 0}, {}}};
    std::vector<std::uint32_t> bundle((sizeof(resourceHeader) + payload.size() + sizeof(std::uint32_t) - 1) /
                                          sizeof(std::uint32_t),
                                      0);
    std::memcpy(bundle.data(), &resourceHeader, sizeof(resourceHeader));
    std::memcpy(reinterpret_cast<std::uint8_t*>(bundle.data()) + sizeof(resourceHeader), payload.data(), payload.size());
    return bundle;
}

std::vector<std::uint32_t> BuildNestedBundle() {
    std::vector<std::uint8_t> payload(HeaderWords * sizeof(std::uint32_t), 0);
    const auto labels = AddKey(payload, "labels");
    const auto sub = AddKey(payload, "sub");
    const auto yes = AddKey(payload, "yes");
    Align(payload);
    const auto keysTop = static_cast<std::uint32_t>(payload.size() / sizeof(std::uint32_t));
    const auto value = AppendString(payload, u"nested-value");
    const auto subTable = AppendTable(payload, {{yes, value}});
    const auto labelsTable = AppendTable(payload, {{sub, subTable}});
    const auto root = AppendTable(payload, {{labels, labelsTable}});
    Align(payload);
    const auto bundleTop = static_cast<std::uint32_t>(payload.size() / sizeof(std::uint32_t));
    return FinishBundle(payload, root, keysTop, bundleTop);
}

struct BundleEntry {
    const char* name;
    const void* data;
};

struct DataPackage {
    DataHeader header;
    std::uint32_t count;
    std::uint32_t reserved;
    BundleEntry entries[6];
};

static_assert(offsetof(DataPackage, count) == 24);
static_assert(offsetof(DataPackage, entries) == 32);

struct FallbackCase {
    const char* locale;
    const char* tableKey;
    const char* subTableKey;
    const char* itemKey;
    std::u16string_view expected;
    UErrorCode expectedStatus;
    std::int32_t expectedNullLength = -1;
};

void CheckFallback() {
    static_assert(std::is_same_v<decltype(&uloc_getTableStringWithFallback_nid_postfix),
                                 const char16_t* (APS5_VABI*)(const char*, const char*, const char*, const char*,
                                                              const char*, std::int32_t*, std::int32_t*)>);

    const std::vector<std::uint32_t> root = BuildBundle({
        {"Countries", {{"US", u"root-country"}}}, {"layout", {{"ltr", u"left-to-right"}}}});
    const std::vector<std::uint32_t> de = BuildBundle({
        {"Countries", {{"RS", u"replacement"}}}, {"labels", {{"none", u"\u2205\u2205\u2205"}, {"yes", u"ja"}}}});
    const std::vector<std::uint32_t> sr = BuildBundle({{"Countries", {{"RS", u"serbia"}}}});
    const std::vector<std::uint32_t> deXx = BuildNestedBundle();
    const std::vector<std::uint32_t> en = BuildBundle({{"layout", {{"rtl", u"right-to-left"}}}});
    const std::vector<std::uint32_t> ptBr = BuildBundle({{"Countries", {{"Fallback", u"de"}}}});
    const DataPackage package{
        {{sizeof(DataHeader), 0xda, 0x27}, {sizeof(UDataInfo), 0, U_IS_BIG_ENDIAN, U_CHARSET_FAMILY,
                                          U_SIZEOF_UCHAR, 0, {'T', 'o', 'C', 'P'}, {1, 0, 0, 0}, {}}},
        6, 0,
        {{U_ICUDATA_NAME "/de.res", de.data()}, {U_ICUDATA_NAME "/de_XX.res", deXx.data()},
         {U_ICUDATA_NAME "/en.res", en.data()}, {U_ICUDATA_NAME "/pt_BR.res", ptBr.data()},
         {U_ICUDATA_NAME "/root.res", root.data()}, {U_ICUDATA_NAME "/sr.res", sr.data()}}};
    std::int32_t status = 0;
    udata_setCommonData_nid_postfix(&package, &status);
    Require(status == 0, "guest udata_setCommonData accepts the synthetic bundles");
    UErrorCode hostStatus = U_ZERO_ERROR;
    udata_setCommonData(&package, &hostStatus);
    Require(hostStatus == U_ZERO_ERROR, "host udata_setCommonData accepts the synthetic bundles");

    std::int32_t directCode = 0;
    std::int32_t directLength = -1;
    const char16_t* direct = uloc_getTableStringWithFallback_nid_postfix(
        nullptr, "de", "labels", nullptr, "yes", &directLength, &directCode);
    Require(direct != nullptr && directLength == 2 && direct[0] == u'j' && direct[1] == u'a',
            "direct table item is borrowed from the resource bundle");
    Require(directCode == 0, "direct lookup keeps a clear status");

    std::int32_t replacedCode = 0;
    std::int32_t replacedLength = -1;
    const char16_t* replaced = uloc_getTableStringWithFallback_nid_postfix(
        nullptr, "sr", "Countries", nullptr, "YU", &replacedLength, &replacedCode);
    Require(replaced != nullptr &&
                std::u16string_view(replaced, static_cast<std::size_t>(replacedLength)) == u"serbia",
            "deprecated country code is replaced before lookup");
    Require(replacedCode == 0, "deprecated replacement keeps a clear status");

    constexpr FallbackCase cases[] = {
        {"de", "labels", nullptr, "yes", u"ja", U_ZERO_ERROR},
        {"de_XX", "labels", "sub", "yes", u"nested-value", U_ZERO_ERROR},
        {"de_XX", "labels", nullptr, "yes", u"ja", U_ZERO_ERROR},
        {"sr", "Countries", nullptr, "YU", u"serbia", U_ZERO_ERROR},
        {"pt_BR", "Countries", nullptr, "YU", u"replacement", U_ZERO_ERROR},
        {"de", "labels", nullptr, "missing", {}, U_MISSING_RESOURCE_ERROR},
        {"de", "labels", nullptr, "none", {}, U_MISSING_RESOURCE_ERROR, 0},
        {"de_ZZ", "labels", nullptr, "yes", u"ja", U_USING_FALLBACK_WARNING},
        {"en", "layout", nullptr, "ltr", u"left-to-right", U_ZERO_ERROR},
    };
    for (const auto& item : cases) {
        const bool expectNull = item.expected.data() == nullptr;
        std::int32_t guestCode = 0;
        std::int32_t guestLength = -1;
        const char16_t* guest = uloc_getTableStringWithFallback_nid_postfix(
            nullptr, item.locale, item.tableKey, item.subTableKey, item.itemKey, &guestLength, &guestCode);
        if (expectNull) {
            Require(guest == nullptr, "expected fallback miss returns null");
            Require(guestCode == static_cast<std::int32_t>(item.expectedStatus), "expected fallback miss status");
            Require(guestLength == item.expectedNullLength, "expected fallback miss length");
        } else {
            Require(guest != nullptr, "expected fallback hit returns a string");
            Require(guestCode == static_cast<std::int32_t>(item.expectedStatus), "expected fallback hit status");
            Require(guestLength == static_cast<std::int32_t>(item.expected.size()), "expected fallback hit length");
            Require(std::u16string_view(guest, static_cast<std::size_t>(guestLength)) == item.expected,
                    "expected fallback hit value");
        }

        UErrorCode hostCode = U_ZERO_ERROR;
        std::int32_t hostLength = -1;
        const char16_t* host = uloc_getTableStringWithFallback(nullptr, item.locale, item.tableKey,
                                                                 item.subTableKey, item.itemKey, &hostLength,
                                                                 &hostCode);
        Require((host == nullptr) == (guest == nullptr), "fallback result matches native ICU");
        Require(static_cast<std::int32_t>(hostCode) == guestCode, "fallback status matches native ICU");
        Require(hostLength == guestLength, "fallback length matches native ICU");
        if (host) Require(host == guest, "fallback returns the native ICU string");
    }

    std::int32_t nullLenCode = 0;
    const char16_t* nullLen = uloc_getTableStringWithFallback_nid_postfix(
        nullptr, "de", "labels", nullptr, "yes", nullptr, &nullLenCode);
    UErrorCode hostNullLenCode = U_ZERO_ERROR;
    const char16_t* hostNullLen = uloc_getTableStringWithFallback(
        nullptr, "de", "labels", nullptr, "yes", nullptr, &hostNullLenCode);
    Require(nullLen != nullptr && std::u16string_view(nullLen) == u"ja",
            "null length returns the table item");
    Require(nullLenCode == 0, "null length keeps a clear status");
    Require(static_cast<std::int32_t>(hostNullLenCode) == nullLenCode,
            "null length status matches native ICU");
    Require((hostNullLen == nullptr) == (nullLen == nullptr),
            "null length result matches native ICU");
    if (hostNullLen) Require(hostNullLen == nullLen, "null length returns the native ICU string");

    for (const std::int32_t failure : {1, 2, 15, 0x7fffffff}) {
        std::int32_t code = failure;
        std::int32_t length = 77;
        Require(uloc_getTableStringWithFallback_nid_postfix(nullptr, "de", "labels", nullptr, "yes", &length,
                                                            &code) == nullptr,
                "prefailed status returns null");
        Require(code == failure && length == 77, "prefailed status and length stay unchanged");
    }

    const auto* bad = reinterpret_cast<const char*>(0x10);
    auto* badLength = reinterpret_cast<std::int32_t*>(0x10);
    std::int32_t code = 0;
    RequireThrows([&] { uloc_getTableStringWithFallback_nid_postfix(nullptr, "de", bad, nullptr, "yes", nullptr, &code); });
    RequireThrows([&] { uloc_getTableStringWithFallback_nid_postfix(nullptr, "de", "labels", nullptr, bad, nullptr, &code); });
    errno = 71;
    RequireThrows([&] {
        uloc_getTableStringWithFallback_nid_postfix(nullptr, "de", "labels", nullptr, "yes", badLength, &code);
    });
    Require(errno == 71, "unwritable length keeps errno");
    RequireThrows([&] {
        uloc_getTableStringWithFallback_nid_postfix(nullptr, "de", "labels", nullptr, "yes", nullptr, nullptr);
    });
    RequireThrows([&] { uloc_getTableStringWithFallback_nid_postfix(nullptr, "de", nullptr, nullptr, "yes", nullptr, &code); });
    Require(code == 0, "throwing calls keep the status untouched");
}

}

int main() {
    CheckFallback();
    return 0;
}
