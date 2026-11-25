#include "prx/libSceIcu/libSceIcu.hpp"
#include "unicode/utypes.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("icu collator availability: " + message);
}

constexpr std::size_t HeaderSize = 32;
constexpr std::uint32_t TableType = 2;
constexpr std::uint32_t TypeShift = 28;

void Put16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

void Put32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

void PutHeader(std::vector<std::uint8_t>& bytes, std::size_t offset, std::string_view format) {
    Put16(bytes, offset, HeaderSize);
    bytes[offset + 2] = 0xda;
    bytes[offset + 3] = 0x27;
    Put16(bytes, offset + 4, 20);
    bytes[offset + 8] = U_IS_BIG_ENDIAN;
    bytes[offset + 9] = U_CHARSET_FAMILY;
    bytes[offset + 10] = U_SIZEOF_UCHAR;
    std::memcpy(bytes.data() + offset + 12, format.data(), 4);
    bytes[offset + 16] = 1;
}

std::size_t PutKey(std::vector<std::uint8_t>& bytes, std::size_t root, std::size_t& cursor, std::string_view key) {
    const auto offset = cursor - root;
    std::memcpy(bytes.data() + cursor, key.data(), key.size());
    cursor += key.size() + 1;
    return offset;
}

std::vector<std::uint8_t> CollationIndex(const std::vector<std::string_view>& locales) {
    const std::string name = U_ICUDATA_NAME "/coll/res_index.res";
    std::vector<std::uint8_t> bytes(1024);
    PutHeader(bytes, 0, "CmnD");
    const std::size_t toc = HeaderSize;
    const std::size_t item = (toc + 12 + name.size() + 1 + 15) / 16 * 16;
    Put32(bytes, toc, 1);
    Put32(bytes, toc + 4, 12);
    Put32(bytes, toc + 8, static_cast<std::uint32_t>(item - toc));
    std::memcpy(bytes.data() + toc + 12, name.data(), name.size());
    PutHeader(bytes, item, "ResB");
    const std::size_t root = item + HeaderSize;
    std::size_t cursor = root + 4;
    const auto installedKey = PutKey(bytes, root, cursor, "InstalledLocales");
    std::vector<std::size_t> keys;
    for (const auto locale : locales) keys.push_back(PutKey(bytes, root, cursor, locale));
    cursor = (cursor + 3) / 4 * 4;
    const auto installed = cursor;
    Put16(bytes, cursor, static_cast<std::uint16_t>(locales.size()));
    cursor += 2;
    for (const auto key : keys) {
        Put16(bytes, cursor, static_cast<std::uint16_t>(key));
        cursor += 2;
    }
    cursor = (cursor + 3) / 4 * 4 + 4 * locales.size();
    const auto top = cursor;
    Put16(bytes, cursor, 1);
    Put16(bytes, cursor + 2, static_cast<std::uint16_t>(installedKey));
    Put32(bytes, cursor + 4, (TableType << TypeShift) | static_cast<std::uint32_t>((installed - root) / 4));
    Put32(bytes, root, (TableType << TypeShift) | static_cast<std::uint32_t>((top - root) / 4));
    Require(cursor + 8 <= bytes.size(), "the test data does not fit its buffer");
    return bytes;
}

void CheckBundled() {
    const auto count = ucol_countAvailable_nid_postfix();
    Require(count == 145, "ucol_countAvailable returned " + std::to_string(count) + ", expected the 145 locales of the bundled collation data");
    std::string_view first;
    std::string_view previous;
    bool german = false;
    bool swedish = false;
    for (std::int32_t index = 0; index != count; ++index) {
        const char* locale = ucol_getAvailable_nid_postfix(index);
        Require(locale != nullptr, "ucol_getAvailable returned null for index " + std::to_string(index));
        Require(ucol_getAvailable_nid_postfix(index) == locale, "ucol_getAvailable returned another pointer for the same index");
        const std::string_view name(locale);
        Require(index == 0 || previous < name, "ucol_getAvailable returned " + std::string(name) + " out of order");
        if (index == 0) first = name;
        german = german || name == "de";
        swedish = swedish || name == "sv";
        previous = name;
    }
    Require(first == "af" && previous == "zu", "the bundled collation locales do not run from af to zu");
    Require(german && swedish, "the bundled collation locales lack de or sv");
    Require(ucol_getAvailable_nid_postfix(count) == nullptr, "ucol_getAvailable returned a locale at the count");
    Require(ucol_getAvailable_nid_postfix(std::numeric_limits<std::int32_t>::max()) == nullptr, "ucol_getAvailable returned a locale past the count");
    for (const auto index : {std::numeric_limits<std::int32_t>::min(), -1}) {
        bool thrown = false;
        try {
            ucol_getAvailable_nid_postfix(index);
        } catch (const std::exception&) {
            thrown = true;
        }
        Require(thrown, "ucol_getAvailable accepted the negative index " + std::to_string(index));
    }
    Require(ucol_countAvailable_nid_postfix() == count, "ucol_countAvailable changed between calls");
}

void CheckWithData() {
    alignas(16) static std::uint8_t storage[1024];
    const auto image = CollationIndex({"de", "en_US", "sr_Latn_RS"});
    std::memcpy(storage, image.data(), image.size());
    std::int32_t status = U_ZERO_ERROR;
    udata_setCommonData_nid_postfix(storage, &status);
    Require(status == U_ZERO_ERROR, "udata_setCommonData refused the test data with status " + std::to_string(status));
    Require(ucol_countAvailable_nid_postfix() == 3, "ucol_countAvailable returned " + std::to_string(ucol_countAvailable_nid_postfix()) + ", expected 3");
    const char* const expected[] = {"de", "en_US", "sr_Latn_RS"};
    for (std::int32_t index = 0; index != 3; ++index) {
        const char* locale = ucol_getAvailable_nid_postfix(index);
        Require(locale != nullptr, "ucol_getAvailable returned null for index " + std::to_string(index));
        Require(std::string_view(locale) == expected[index], "ucol_getAvailable returned " + std::string(locale) + " for index " + std::to_string(index));
        Require(ucol_getAvailable_nid_postfix(index) == locale, "ucol_getAvailable returned another pointer for the same index");
    }
    Require(ucol_getAvailable_nid_postfix(3) == nullptr, "ucol_getAvailable returned a locale at the count");
    Require(ucol_getAvailable_nid_postfix(std::numeric_limits<std::int32_t>::max()) == nullptr, "ucol_getAvailable returned a locale past the count");
    bool thrown = false;
    try {
        ucol_getAvailable_nid_postfix(-1);
    } catch (const std::exception&) {
        thrown = true;
    }
    Require(thrown, "ucol_getAvailable accepted a negative index");
}

}

int main(int argumentCount, char** arguments) {
    Require(argumentCount == 2, "expected one argument: bundled or data");
    const std::string_view mode = arguments[1];
    if (mode == "bundled") CheckBundled();
    else if (mode == "data") CheckWithData();
    else Require(false, "unknown mode " + std::string(mode));
    return 0;
}
