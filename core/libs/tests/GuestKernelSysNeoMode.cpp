#include "prx/libkernel_sys/libkernel_sys.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::int32_t SupportNeoMode = 0x00800000;
constexpr std::int32_t SupportPsVrAndEnterCross = 0x00004002;

struct Entry {
    std::string key;
    std::uint16_t format;
    std::vector<std::uint8_t> data;
};

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "KERNEL SYS NEO MODE: %s\n", message);
        std::abort();
    }
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

std::filesystem::path Metadata(const std::filesystem::path& directory) {
    const auto metadata = directory / "app0" / "sce_sys";
    std::filesystem::create_directories(metadata);
    return metadata;
}

void WriteParamSfo(const std::filesystem::path& directory, const std::vector<Entry>& extra) {
    std::vector<Entry> entries{Text("TITLE", "Example"), Text("TITLE_ID", "CUSA00000")};
    entries.insert(entries.end(), extra.begin(), extra.end());
    const auto bytes = Build(entries);
    std::ofstream file(Metadata(directory) / "param.sfo", std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(file), "cannot write the param.sfo fixture");
}

void WriteParamJson(const std::filesystem::path& directory) {
    std::ofstream file(Metadata(directory) / "param.json");
    file << R"({"titleId":"PPSA00000","localizedParameters":{"en-US":{"titleName":"Example"}}})";
    Require(static_cast<bool>(file), "cannot write the param.json fixture");
}

bool Throws(std::string_view expected) {
    try {
        sceKernelIsNeoMode();
    } catch (const std::runtime_error& error) {
        return std::string_view(error.what()) == expected;
    }
    return false;
}

int RunSelf(const char* program, std::string_view mode, const std::filesystem::path& directory) {
    const std::string arguments = "\"" + std::string(program) + "\" " + std::string(mode) + " \"" + directory.string() + "\"";
#ifdef _WIN32
    const std::string command = "\"" + arguments + "\"";
#else
    const std::string command = arguments;
#endif
    return std::system(command.c_str());
}

}

int main(int argc, char** argv) {
    if (argc == 3) {
        std::filesystem::current_path(argv[2]);
        const std::string_view mode = argv[1];
        if (mode == "--neo") {
            Require(sceKernelIsNeoMode() == 1, "a title with the Neo-mode bit is not in Neo mode");
            Require(sceKernelIsNeoMode() == 1, "a second call changed the answer");
            return 0;
        }
        if (mode == "--base") {
            Require(sceKernelIsNeoMode() == 0, "a title without the Neo-mode bit is in Neo mode");
            Require(sceKernelIsNeoMode() == 0, "a second call changed the answer");
            return 0;
        }
        if (mode == "--param-json") {
            Require(Throws("sceKernelIsNeoMode not implemented"), "a title with param.json did not throw");
            return 0;
        }
        if (mode == "--no-metadata") {
            Require(Throws("param.json or param.sfo not found"), "a title without metadata did not throw");
            return 0;
        }
        return 1;
    }

    const auto root = std::filesystem::temp_directory_path() / ("guest_kernel_sys_neo_mode_" + std::to_string(getpid_nid_postfix()));
    std::filesystem::remove_all(root);
    const struct {
        const char* name;
        const char* mode;
        bool paramSfo;
        std::vector<Entry> attribute;
        bool paramJson;
        const char* failure;
    } cases[] = {
        {"neo", "--neo", true, {Integer("ATTRIBUTE", SupportNeoMode)}, false, "ATTRIBUTE 0x00800000"},
        {"all", "--neo", true, {Integer("ATTRIBUTE", -1)}, false, "ATTRIBUTE 0xffffffff"},
        {"other", "--base", true, {Integer("ATTRIBUTE", SupportPsVrAndEnterCross)}, false, "ATTRIBUTE without bit 23"},
        {"absent", "--base", true, {}, false, "param.sfo without ATTRIBUTE"},
        {"json", "--param-json", false, {}, true, "param.json"},
        {"both", "--param-json", true, {Integer("ATTRIBUTE", SupportNeoMode)}, true, "param.json beside a Neo-mode param.sfo"},
        {"none", "--no-metadata", false, {}, false, "no metadata"},
    };
    for (const auto& test : cases) {
        const auto directory = root / test.name;
        std::filesystem::create_directories(directory);
        if (test.paramSfo) WriteParamSfo(directory, test.attribute);
        if (test.paramJson) WriteParamJson(directory);
        if (RunSelf(argv[0], test.mode, directory) != 0) {
            std::fprintf(stderr, "KERNEL SYS NEO MODE: case failed: %s\n", test.failure);
            std::filesystem::remove_all(root);
            return 1;
        }
    }
    std::filesystem::remove_all(root);
    std::puts("sceKernelIsNeoMode tests passed");
    return 0;
}
