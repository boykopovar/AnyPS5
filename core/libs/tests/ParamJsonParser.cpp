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

static ParsedParamJson ParseText(const std::filesystem::path& path, const std::string& text) {
    {
        std::ofstream file(path, std::ios::binary);
        file << text;
        Require(static_cast<bool>(file), "Cannot write param.json fixture");
    }
    return parseParamJson(path);
}

static void CheckUserDefinedParams(const std::filesystem::path& path) {
    const auto parsed = ParseText(path, R"({"titleId":"PPSA23566","localizedParameters":{"en-US":{"titleName":"Example"}},)"
                                        R"("userDefinedParam1":23566,"userDefinedParam3":-7,"userDefinedParam4":2147483647})");
    Require(parsed.userDefinedParams[0] == 23566 && parsed.userDefinedParams[1] == 0 && parsed.userDefinedParams[2] == -7 &&
            parsed.userDefinedParams[3] == 2147483647, "Incorrect user defined params");
    for (const auto* text : {"2147483648", "1.5", "1e3", "\"1\"", "true"}) {
        bool rejected = false;
        try {
            ParseText(path, std::string(R"({"titleId":"PPSA00000","localizedParameters":{"en-US":{"titleName":"Example"}},"userDefinedParam2":)") + text + "}");
        } catch (const std::exception&) {
            rejected = true;
        }
        Require(rejected, std::string("Accepted invalid user defined param: ") + text);
    }
}

static void CheckLanguages(const std::filesystem::path& path) {
    const auto nested = ParseText(path, "{\r\n  \"localizedParameters\": {\r\n    \"defaultLanguage\": \"en-GB\",\r\n"
                                        "    \"en-GB\": { \"titleName\": \"British\" }\r\n  },\r\n  \"titleId\": \"PPSA00001\"\r\n}\r\n");
    Require(nested.title == "British" && nested.titleId == "PPSA00001", "Nested defaultLanguage without en-US");
    const auto selected = ParseText(path, R"({"titleId":"PPSA00002","localizedParameters":{"defaultLanguage":"fr-FR",)"
                                          R"("en-US":{"titleName":"American"},"fr-FR":{"titleName":"French"}}})");
    Require(selected.title == "French", "Nested defaultLanguage not selected");
    const auto american = ParseText(path, R"({"titleId":"PPSA00003","localizedParameters":{"defaultLanguage":"de-DE",)"
                                          R"("ja-JP":{"titleName":"Japanese"},"en-US":{"titleName":"American"}}})");
    Require(american.title == "American", "Missing defaultLanguage entry does not fall back to en-US");
    const auto root = ParseText(path, R"({"titleId":"PPSA00004","defaultLanguage":"ja-JP","localizedParameters":{)"
                                      R"("en-US":{"titleName":"American"},"ja-JP":{"titleName":"Japanese"}}})");
    Require(root.title == "Japanese", "Root defaultLanguage not selected");
    bool rejected = false;
    try {
        ParseText(path, R"({"titleId":"PPSA00005","localizedParameters":{"defaultLanguage":"en-GB"}})");
    } catch (const std::exception&) {
        rejected = true;
    }
    Require(rejected, "Accepted localizedParameters without a language entry");
}

static void CheckNumberSyntax(const std::filesystem::path& path) {
    const std::string prefix = R"({"titleId":"PPSA00000","localizedParameters":{"en-US":{"titleName":"Example"}})";
    const std::pair<const char*, const char*> fields[] = {
        {R"(,"number":)", "}"}, {R"(,"values":[)", "]}"}, {R"(,"values":[{"number":)", "}]}"}
    };
    for (const auto& [before, after] : fields) {
        for (const auto* text : {"0", "-0", "0.25", "-0.25", "0e3", "0E+3", "-0e-3",
                                 "10", "-10", "1e3", "1.25", "1e01", "1e+003", "1e-003"}) {
            const auto parsed = ParseText(path, prefix + before + text + after);
            Require(parsed.titleId == "PPSA00000" && parsed.title == "Example", "Title metadata changed");
        }
        for (const auto* text : {"00", "01", "-00", "-01", "00.25", "01.25", "-01.25", "00e3", "01e3", "-01e3"}) {
            bool rejected = false;
            try { ParseText(path, prefix + before + text + after); } catch (const std::exception&) { rejected = true; }
            Require(rejected, std::string("Accepted leading-zero number: ") + text);
        }
    }
}

int main() {
    const auto path = std::filesystem::temp_directory_path() / ("anyps5-param-json-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");
    int result = 0;
    try {
        CheckNumberSyntax(path);
        const std::pair<const char*, std::uint64_t> sizes[] = {
            {"", 0}, {"0", 0}, {"-0", 0}, {"0.25", 0}, {"0e3", 0}, {"0E+3", 0}, {"-0e-3", 0}, {"4096", 4096},
            {"9007199254740993", 9007199254740993ull},
            {"18446744073709551614", std::numeric_limits<std::uint64_t>::max() - 1},
            {"18446744073709551615", std::numeric_limits<std::uint64_t>::max()},
            {"1e3", 1000}, {"1.25", 1}, {"1e01", 10}, {"1e+003", 1000}, {"1e-003", 0}
        };
        for (const auto& [text, expected] : sizes) {
            const auto parsed = ParseSize(path, text);
            Require(parsed.downloadDataSizeMiB == expected, std::string("Incorrect download size: ") + text);
            Require(parsed.titleId == "PPSA00000" && parsed.title == "Example", "Title metadata changed");
        }
        for (const auto* text : {"18446744073709551616", "18446744073709551617", "1.8446744073709552e19",
                                 "1e40", "-1", "-0.5", "true", "null", "\"4096\"", "00", "01", "00.25", "01e3", "01.25"}) {
            bool rejected = false;
            try { ParseSize(path, text); } catch (const std::exception&) { rejected = true; }
            Require(rejected, std::string("Accepted invalid download size: ") + text);
        }
        CheckLanguages(path);
        CheckUserDefinedParams(path);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    std::filesystem::remove(path);
    return result;
}
