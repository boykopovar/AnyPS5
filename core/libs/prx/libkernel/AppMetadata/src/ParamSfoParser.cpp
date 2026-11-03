#include "prx/libkernel/AppMetadata/include/ParamSfoParser.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::uint16_t SfoUtf8 = 0x0204;
constexpr std::uint16_t SfoInteger = 0x0404;
constexpr std::size_t HeaderSize = 20;
constexpr std::size_t EntrySize = 16;

std::uint16_t readU16(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    std::uint16_t value;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

std::uint32_t readU32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    std::uint32_t value;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

}

ParsedParamJson parseParamSfo(const std::filesystem::path& paramSfoPath) {
    std::ifstream file(paramSfoPath, std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("failed to open param.sfo");
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (bytes.size() < HeaderSize || std::memcmp(bytes.data(), "\0PSF", 4) != 0) throw std::runtime_error("param.sfo has no PSF header");
    const std::uint64_t keyTable = readU32(bytes, 8);
    const std::uint64_t dataTable = readU32(bytes, 12);
    const std::uint64_t count = readU32(bytes, 16);
    if (count > (bytes.size() - HeaderSize) / EntrySize) throw std::runtime_error("param.sfo tables are out of bounds");
    std::set<std::string> keys;
    std::map<std::string, std::string> strings;
    std::map<std::string, std::int32_t> integers;
    for (std::uint64_t index = 0; index < count; ++index) {
        const auto entry = HeaderSize + index * EntrySize;
        const auto keyOffset = keyTable + readU16(bytes, entry);
        const auto format = readU16(bytes, entry + 2);
        const std::uint64_t length = readU32(bytes, entry + 4);
        const auto dataOffset = dataTable + readU32(bytes, entry + 12);
        if (keyOffset >= bytes.size() || dataOffset > bytes.size() || length > bytes.size() - dataOffset) throw std::runtime_error("param.sfo entry is out of bounds");
        const auto keyEnd = std::find(bytes.begin() + static_cast<std::ptrdiff_t>(keyOffset), bytes.end(), std::uint8_t{0});
        if (keyEnd == bytes.end()) throw std::runtime_error("param.sfo key is not terminated");
        const std::string key(bytes.begin() + static_cast<std::ptrdiff_t>(keyOffset), keyEnd);
        if (!keys.insert(key).second) throw std::runtime_error("param.sfo repeats " + key);
        if (format == SfoUtf8) {
            const auto begin = bytes.begin() + static_cast<std::ptrdiff_t>(dataOffset);
            const auto end = std::find(begin, begin + static_cast<std::ptrdiff_t>(length), std::uint8_t{0});
            if (end == begin + static_cast<std::ptrdiff_t>(length)) throw std::runtime_error("param.sfo " + key + " is not a terminated string");
            strings.emplace(key, std::string(begin, end));
        } else if (format == SfoInteger) {
            if (length != 4) throw std::runtime_error("param.sfo " + key + " is not a 32-bit integer");
            integers.emplace(key, static_cast<std::int32_t>(readU32(bytes, dataOffset)));
        }
    }
    const auto text = [&](const char* key) {
        const auto found = strings.find(key);
        if (found == strings.end() || found->second.empty()) throw std::runtime_error(std::string("param.sfo is missing ") + key);
        return found->second;
    };
    ParsedParamJson result;
    result.title = text("TITLE");
    result.titleId = text("TITLE_ID");
    if (const auto size = integers.find("DOWNLOAD_DATA_SIZE"); size != integers.end()) {
        if (size->second < 0) throw std::runtime_error("param.sfo DOWNLOAD_DATA_SIZE is negative");
        result.downloadDataSizeMiB = static_cast<std::uint64_t>(size->second);
    }
    for (int index = 0; index < 4; ++index) {
        const auto param = integers.find("USER_DEFINED_PARAM_" + std::to_string(index + 1));
        if (param != integers.end()) result.userDefinedParams[index] = param->second;
    }
    const auto attribute = integers.find("ATTRIBUTE");
    result.paramSfoAttribute = attribute != integers.end() ? static_cast<std::uint32_t>(attribute->second) : 0u;
    return result;
}
