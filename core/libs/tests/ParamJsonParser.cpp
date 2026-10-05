#include "prx/libkernel/AppMetadata/include/ParamJsonParser.hpp"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

static void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

static ParsedParamJson ParseSize(const std::filesystem::path& path, const std::string& size) {
    {
        std::ofstream file(path);
        file << R"({"titleId":"PPSA00000","localizedParameters":{"en-US":{"titleName":"Example"}})";
        if (!size.empty()) file << ",\"downloadDataSize\":" << size;
        file << '}';
        Require(static_cast<bool>(file), "Cannot write param.json fixture");
    }
    return parseParamJson(path);
}

int main() {
    const auto path = std::filesystem::temp_directory_path() / ("anyps5-param-json-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");
    int result = 0;
    try {
        const std::pair<const char*, std::uint64_t> sizes[] = {
            {"", 0}, {"0", 0}, {"-0", 0}, {"4096", 4096},
            {"9007199254740993", 9007199254740993ull},
            {"18446744073709551614", std::numeric_limits<std::uint64_t>::max() - 1},
            {"18446744073709551615", std::numeric_limits<std::uint64_t>::max()},
            {"1e3", 1000}, {"1.25", 1}
        };
        for (const auto& [text, expected] : sizes) {
            const auto parsed = ParseSize(path, text);
            Require(parsed.downloadDataSizeMiB == expected, std::string("Incorrect download size: ") + text);
            Require(parsed.titleId == "PPSA00000" && parsed.title == "Example", "Title metadata changed");
        }
        for (const auto* text : {"18446744073709551616", "18446744073709551617", "1.8446744073709552e19",
                                 "1e40", "-1", "-0.5", "true", "null", "\"4096\""}) {
            bool rejected = false;
            try { ParseSize(path, text); } catch (const std::exception&) { rejected = true; }
            Require(rejected, std::string("Accepted invalid download size: ") + text);
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    std::filesystem::remove(path);
    return result;
}
