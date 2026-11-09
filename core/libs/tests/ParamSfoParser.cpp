#include "prx/libkernel/AppMetadata/include/ParamSfoParser.hpp"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Entry {
    std::string key;
    std::uint16_t format;
    std::vector<std::uint8_t> data;
};

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

Entry Text(const std::string& key, const std::string& value) {
    std::vector<std::uint8_t> data(value.begin(), value.end());
    data.push_back(0);
    return {key, 0x0204, data};
}

Entry Integer(const std::string& key, std::int32_t value) {
    std::vector<std::uint8_t> data(4);
    std::memcpy(data.data(), &value, 4);
    return {key, 0x0404, data};
}

void Put32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    std::memcpy(bytes.data() + offset, &value, 4);
}

std::vector<std::uint8_t> Build(const std::vector<Entry>& entries) {
    std::vector<std::uint8_t> keys;
    std::vector<std::uint8_t> data;
    std::vector<std::uint8_t> bytes(20 + entries.size() * 16);
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const auto entry = 20 + index * 16;
        const auto keyOffset = static_cast<std::uint16_t>(keys.size());
        std::memcpy(bytes.data() + entry, &keyOffset, 2);
        std::memcpy(bytes.data() + entry + 2, &entries[index].format, 2);
        Put32(bytes, entry + 4, static_cast<std::uint32_t>(entries[index].data.size()));
        Put32(bytes, entry + 8, static_cast<std::uint32_t>(entries[index].data.size()));
        Put32(bytes, entry + 12, static_cast<std::uint32_t>(data.size()));
        keys.insert(keys.end(), entries[index].key.begin(), entries[index].key.end());
        keys.push_back(0);
        data.insert(data.end(), entries[index].data.begin(), entries[index].data.end());
    }
    while (keys.size() % 4 != 0) keys.push_back(0);
    std::memcpy(bytes.data(), "\0PSF", 4);
    Put32(bytes, 4, 0x0101);
    Put32(bytes, 8, static_cast<std::uint32_t>(bytes.size()));
    Put32(bytes, 12, static_cast<std::uint32_t>(bytes.size() + keys.size()));
    Put32(bytes, 16, static_cast<std::uint32_t>(entries.size()));
    bytes.insert(bytes.end(), keys.begin(), keys.end());
    bytes.insert(bytes.end(), data.begin(), data.end());
    return bytes;
}

ParsedParamJson Parse(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    {
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        Require(static_cast<bool>(file), "Cannot write param.sfo fixture");
    }
    return parseParamSfo(path);
}

void Rejects(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes, const std::string& expected) {
    try {
        Parse(path, bytes);
    } catch (const std::exception& error) {
        Require(std::string(error.what()).find(expected) != std::string::npos, "Unexpected error for " + expected + ": " + error.what());
        return;
    }
    throw std::runtime_error("Accepted invalid param.sfo: " + expected);
}

std::vector<Entry> Valid() {
    return {Text("CATEGORY", "gd"), Integer("DOWNLOAD_DATA_SIZE", 64), Text("TITLE", "Example"), Text("TITLE_ID", "CUSA00000"),
            Integer("USER_DEFINED_PARAM_1", 23566), Integer("USER_DEFINED_PARAM_3", -7), {"PUBTOOLINFO", 0x0004, {1, 2, 3}}};
}

}

int main() {
    const auto path = std::filesystem::temp_directory_path() / ("anyps5-param-sfo-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".sfo");
    int result = 0;
    try {
        const auto parsed = Parse(path, Build(Valid()));
        Require(parsed.title == "Example" && parsed.titleId == "CUSA00000", "Incorrect title metadata");
        Require(parsed.downloadDataSizeMiB == 64, "Incorrect download data size");
        Require(parsed.userDefinedParams[0] == 23566 && parsed.userDefinedParams[1] == 0 && parsed.userDefinedParams[2] == -7 && parsed.userDefinedParams[3] == 0, "Incorrect user defined params");
        Require(parsed.paramSfoAttribute == 0u, "An absent ATTRIBUTE is not 0");

        auto entries = Valid();
        entries.push_back(Integer("ATTRIBUTE", 0x00800000));
        Require(Parse(path, Build(entries)).paramSfoAttribute == 0x00800000u, "Incorrect ATTRIBUTE");
        entries.back() = Integer("ATTRIBUTE", -1);
        Require(Parse(path, Build(entries)).paramSfoAttribute == 0xFFFFFFFFu, "ATTRIBUTE lost its high bit");
        entries.back() = Text("ATTRIBUTE", "8388608");
        Require(Parse(path, Build(entries)).paramSfoAttribute == 0u, "A text ATTRIBUTE was read as an integer");
        entries.back() = Integer("ATTRIBUTE", 0x00800000);
        entries.back().data.pop_back();
        Rejects(path, Build(entries), "ATTRIBUTE is not a 32-bit integer");

        entries = Valid();
        entries.erase(entries.begin() + 2);
        Rejects(path, Build(entries), "missing TITLE");
        entries = Valid();
        entries.erase(entries.begin() + 3);
        Rejects(path, Build(entries), "missing TITLE_ID");
        entries = Valid();
        entries[1] = Integer("DOWNLOAD_DATA_SIZE", -1);
        Rejects(path, Build(entries), "DOWNLOAD_DATA_SIZE is negative");
        entries = Valid();
        entries[2].data.back() = 'x';
        Rejects(path, Build(entries), "TITLE is not a terminated string");
        entries = Valid();
        entries[4].data.pop_back();
        Rejects(path, Build(entries), "USER_DEFINED_PARAM_1 is not a 32-bit integer");
        entries = Valid();
        entries.push_back(Text("TITLE", "Again"));
        Rejects(path, Build(entries), "repeats TITLE");

        auto bytes = Build(Valid());
        bytes[1] = 'X';
        Rejects(path, bytes, "no PSF header");
        bytes = Build(Valid());
        Put32(bytes, 16, 1000);
        Rejects(path, bytes, "tables are out of bounds");
        bytes = Build(Valid());
        Put32(bytes, 20 + 2 * 16 + 12, 0x10000);
        Rejects(path, bytes, "entry is out of bounds");
        bytes = Build(Valid());
        Put32(bytes, 20 + 4, 0x10000);
        Rejects(path, bytes, "entry is out of bounds");
        bytes = Build(Valid());
        bytes.resize(bytes.size() - 64);
        Rejects(path, bytes, "out of bounds");
        Rejects(path, std::vector<std::uint8_t>(8), "no PSF header");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    std::filesystem::remove(path);
    if (result == 0) std::cout << "param.sfo parser tests passed\n";
    return result;
}
