#include "prx/libkernel/AppMetadata/include/ParamJsonParser.hpp"
#include <chrono>
#include <clocale>
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

static void CheckNumericLocale(const std::filesystem::path& path) {
    struct RestoreLocale {
        std::string previous = std::setlocale(LC_NUMERIC, nullptr);
        ~RestoreLocale() { std::setlocale(LC_NUMERIC, previous.c_str()); }
    } restore;
    bool commaLocale = false;
    for (const auto* name : {"French", "French_France.1252", "fr_FR.UTF-8", "de_DE.UTF-8"}) {
        if (std::setlocale(LC_NUMERIC, name) != nullptr && std::string(std::localeconv()->decimal_point) == ",") {
            commaLocale = true;
            break;
        }
    }
    if (!commaLocale) {
#ifdef _WIN32
        throw std::runtime_error("No Windows comma-decimal locale available for regression");
#else
        std::cout << "Comma-decimal locale regression unavailable on this host\n";
        return;
#endif
    }
    for (const auto& [number, expected] : {std::pair{"1.25e3", std::uint64_t{1250}},
            std::pair{"0.5e3", std::uint64_t{500}}, std::pair{"12.75e2", std::uint64_t{1275}},
            std::pair{"1.25", std::uint64_t{1}}}) {
        Require(ParseSize(path, number).downloadDataSizeMiB == expected,
            std::string("JSON number depends on host locale: ") + number);
    }
    bool rejected = false;
    try { ParseSize(path, "-0.5"); } catch (const std::exception&) { rejected = true; }
    Require(rejected, "Negative fractional size was accepted under comma locale");
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

static void CheckContentVersion(const std::filesystem::path& path) {
    Require(ParseText(path, R"({"titleId":"PPSA00000","contentVersion":"01.002.000","localizedParameters":{"en-US":{"titleName":"Example"}}})").contentVersion == "01.002.000", "Incorrect content version");
    Require(ParseText(path, R"({"titleId":"PPSA00000","localizedParameters":{"en-US":{"titleName":"Example"}}})").contentVersion.empty(), "A missing content version was not empty");
    Require(ParseText(path, R"({"titleId":"PPSA00000","contentVersion":1,"localizedParameters":{"en-US":{"titleName":"Example"}}})").contentVersion.empty(), "A content version that is not a string was kept");
}

static void CheckNesting(const std::filesystem::path& path) {
    const std::string prefix = R"({"titleId":"PPSA00000","localizedParameters":{"en-US":{"titleName":"Example"}},"extra":)";
    const auto nested = [&](std::size_t arrays) {
        return prefix + std::string(arrays, '[') + std::string(arrays, ']') + "}";
    };
    Require(ParseText(path, nested(511)).title == "Example", "Nesting within the limit rejected");
    for (const std::size_t arrays : {std::size_t{512}, std::size_t{513}, std::size_t{100000}}) {
        bool rejected = false;
        try {
            ParseText(path, nested(arrays));
        } catch (const std::runtime_error& error) {
            rejected = std::string(error.what()).find("nested too deeply") != std::string::npos;
        }
        Require(rejected, "Excessive nesting not rejected: " + std::to_string(arrays));
    }
}

static void CheckFlexibleMemorySize(const std::filesystem::path& path) {
    const std::string prefix = R"({"titleId":"PPSA00000","localizedParameters":{"en-US":{"titleName":"Example"}})";
    Require(!ParseText(path, prefix + "}").flexibleMemorySizeBytes, "Absent kernel changed the default budget");
    Require(!ParseText(path, prefix + R"(,"kernel":{}})").flexibleMemorySizeBytes, "Absent flexible size changed the default budget");
    Require(!ParseText(path, prefix + R"(,"kernel":{"flexibleMemorySize":0}})").flexibleMemorySizeBytes, "A zero flexible size changed the default budget");
    for (const auto bytes : {std::uint64_t{16384}, std::uint64_t{838860800}, std::uint64_t{18446744073709535232ull}}) {
        const auto parsed = ParseText(path, prefix + R"(,"kernel":{"flexibleMemorySize":)" + std::to_string(bytes) + "}}");
        Require(parsed.flexibleMemorySizeBytes == bytes, "Incorrect flexible memory size in bytes");
    }
    for (const auto* value : {"-0", "-16384", "1", "16385", "838860800.0", "8e8", "true", "null", "\"838860800\"", "18446744073709551616"}) {
        bool rejected = false;
        try { ParseText(path, prefix + R"(,"kernel":{"flexibleMemorySize":)" + value + "}}"); }
        catch (const std::runtime_error&) { rejected = true; }
        Require(rejected, std::string("Accepted invalid flexible memory size: ") + value);
    }
    for (const auto* value : {"null", "true", "[]", "1", "\"kernel\""}) {
        bool rejected = false;
        try { ParseText(path, prefix + R"(,"kernel":)" + value + "}"); }
        catch (const std::runtime_error&) { rejected = true; }
        Require(rejected, "Accepted a non-object kernel");
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

static void CheckOutputPermission(const std::filesystem::path& path) {
    const std::string prefix = R"({"titleId":"PPSA99007","localizedParameters":{"en-US":{"titleName":"Example"}})";
    Require(ParseText(path, prefix + "}").attribute3 == 0, "Absent output permission is not zero");
    Require(!ParseText(path, prefix + ",\"attribute\":8388608}").paramSfoAttribute, "param.json gave a param.sfo attribute");
    for (const auto& [text, expected] : {std::pair{"0", 0u}, {"524352", 0x80040u}, {"4294967295", 0xFFFFFFFFu}}) {
        Require(ParseText(path, prefix + ",\"attribute3\":" + text + "}").attribute3 == expected, "Incorrect output permission");
    }
    for (const auto* text : {"-1", "0.5", "1e3", "4294967296", "true", "null", "\"524352\""}) {
        bool rejected = false;
        try { ParseText(path, prefix + ",\"attribute3\":" + text + "}"); } catch (const std::exception&) { rejected = true; }
        Require(rejected, "Accepted invalid attribute3");
    }
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

static void CheckUtf8(const std::filesystem::path& path) {
    const std::string prefix = R"({"titleId":"PPSA00000","localizedParameters":{"en-US":{"titleName":")";
    const std::string suffix = R"("}}})";
    for (const auto& title : {std::string("ASCII"), std::string("\xC2\x80"), std::string("\xE3\x82\xB2\xE3\x83\xBC\xE3\x83\xA0"),
                              std::string("\xF0\x9F\x8E\xAE"), std::string("\xF4\x8F\xBF\xBF")}) {
        Require(ParseText(path, prefix + title + suffix).title == title, "Valid UTF-8 title changed");
    }
    Require(ParseText(path, prefix + R"(\uD83C\uDFAE)" + suffix).title == "\xF0\x9F\x8E\xAE", "Escaped title changed");
    for (const auto& title : {std::string("\x80"), std::string("\xC0\xAF"), std::string("\xC1\xBF"), std::string("\xC2"),
                              std::string("\xE0\x80\xAF"), std::string("\xED\xA0\x80"), std::string("\xE3\x82"),
                              std::string("\xF0\x80\x80\xAF"), std::string("\xF4\x90\x80\x80"), std::string("\xF5\x80\x80\x80"),
                              std::string("\xFE"), std::string("\xFF"), std::string("\xC2x"), std::string("\xE3\x82x")}) {
        bool rejected = false;
        try { ParseText(path, prefix + title + suffix); } catch (const std::exception&) { rejected = true; }
        Require(rejected, "Accepted malformed UTF-8 title");
    }
    bool rejected = false;
    try { ParseText(path, std::string("{\"") + "\x80" + R"(":0,)" + prefix.substr(1) + "Valid" + suffix); }
    catch (const std::exception&) { rejected = true; }
    Require(rejected, "Accepted malformed UTF-8 object key");
}

int main() {
    const auto path = std::filesystem::temp_directory_path() / ("anyps5-param-json-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");
    int result = 0;
    try {
        CheckNumericLocale(path);
        CheckUtf8(path);
        CheckFlexibleMemorySize(path);
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
        CheckContentVersion(path);
        CheckOutputPermission(path);
        CheckNesting(path);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    std::filesystem::remove(path);
    return result;
}
