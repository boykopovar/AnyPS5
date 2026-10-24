#include "prx/libSceNpTrophy2/include/TitleTrophies.hpp"
#include "prx/libSceNpTrophy2/include/TrophyPackage.hpp"
#include "prx/libSceNpTrophy2/include/TrophyStore.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using Files = std::vector<std::pair<std::string, std::string>>;

constexpr std::int32_t InitialUser = 0x10000000;

void check(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

template <typename TAction>
std::string failure(TAction action) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        return error.what();
    }
    return {};
}

bool contains(const std::string& text, const char* part) {
    return text.find(part) != std::string::npos;
}

void putBigEndian(std::string& data, std::size_t offset, std::uint64_t value, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) data[offset + i] = static_cast<char>((value >> (8 * (size - 1 - i))) & 0xFF);
}

std::string ucp(const Files& files) {
    const std::size_t toc = 0x40;
    std::size_t offset = toc + 0x20 + files.size() * 0x40;
    std::size_t size = offset;
    for (const auto& [name, contents] : files) size += contents.size();
    std::string data(size, '\0');
    putBigEndian(data, 0, 0xB228C60A, 4);
    putBigEndian(data, 4, 1, 4);
    putBigEndian(data, 8, size, 8);
    putBigEndian(data, 0x10, files.size(), 4);
    putBigEndian(data, 0x14, toc, 4);
    std::size_t entry = toc + 0x20;
    for (const auto& [name, contents] : files) {
        data.replace(entry, name.size(), name);
        putBigEndian(data, entry + 0x20, offset, 8);
        putBigEndian(data, entry + 0x28, contents.size(), 8);
        data.replace(offset, contents.size(), contents);
        offset += contents.size();
        entry += 0x40;
    }
    return data;
}

const char TropConf[] = R"({"defaultLanguage":"en-US","trophies":[
    {"id":"0","grade":"P","hidden":false},
    {"id":"1","grade":"G","platinumTrophyId":"0","unlockCondition":{"udsStatId":"10","comparator":"ge","targetValue":"10","progressive":false}},
    {"id":"2","grade":"S","platinumTrophyId":0,"unlockCondition":{"udsStatId":20,"comparator":"gt","targetValue":100}},
    {"id":3,"grade":"B","platinumTrophyId":"0","unlockCondition":{"udsStatId":"30","comparator":"le","targetValue":"5"}},
    {"id":"4","grade":"B","platinumTrophyId":"0","unlockCondition":{"udsStatId":"30","comparator":"lt","targetValue":"3"}},
    {"id":"5","grade":"B","platinumTrophyId":"0","unlockCondition":{"udsStatId":"40","comparator":"ge","targetValue":"5","progressive":true}},
    {"id":"6","grade":"B","groupId":"1","hidden":true,"hasReward":true,"platinumTrophyId":"-1"},
    {"id":"7","grade":"B","groupId":"1","unlockCondition":{"udsStatId":"10","comparator":"eq","targetValue":"-1"}}
]})";

const char TropMetaEnglish[] = R"({"metadata":{"titleMetadata":{"name":"Synthetic Game"},
    "trophyMetadata":[{"id":"0","name":"All of them","detail":"Unlock every trophy"},{"id":"1","name":"Ten","detail":"Reach ten"},
                      {"id":"6","name":"Secret","detail":"A hidden one","reward":"A hat"},{"id":"99","name":"Not a trophy"}],
    "groupMetadata":[{"id":"1","name":"Expansion"},{"id":"9","name":"Unused"}]}})";

const char TropMetaFrench[] = R"({"metadata":{"titleMetadata":{"name":"Jeu synth\u00e9tique"},"trophyMetadata":[{"id":"1","name":"Dix"}]}})";

const char StatsDefinition[] = R"({"statDefinitionArray":[
    {"statId":10,"dataType":"uint64","aggregation":"latest","minValue":"0"},
    {"statId":20,"dataType":"uint64","aggregation":"latest"},
    {"statId":30,"dataType":"uint32","aggregation":"latest"},
    {"statId":"40","dataType":"uint32","aggregation":"latest","maxValue":"50"},
    {"statId":50,"dataType":"uint64","aggregation":"sum"},
    {"statId":60,"dataType":"int32","aggregation":"latest"},
    {"statId":70,"dataType":"uint32","aggregation":"latest","maxValue":"5000000000"},
    {"dataType":"uint32","aggregation":"latest"}
]})";

const char StatsExtraction[] = R"({"statsExtractionRuleArray":[
    {"condition":{"eventName":"COLLECT"},"action":{"input":"$.counter","output":{"statId":10}}},
    {"condition":{"eventName":"COLLECT"},"action":{"input":"$.total","output":{"statId":20}}},
    {"condition":{"eventName":"TIME"},"action":{"input":"$.seconds","output":{"statId":30}}},
    {"condition":{"eventName":"STEPS"},"action":{"input":"$.player.steps","output":{"statId":"40"}}},
    {"condition":{"eventName":"SUM"},"action":{"input":"$.counter","output":{"statId":50}}},
    {"condition":{"eventName":"FILTERED","property":{"path":"$.level"}},"action":{"input":"$.counter","output":{"statId":10}}},
    {"condition":{"eventName":"COUNT"},"action":{"output":{"statId":10}}},
    {"condition":{"eventName":"BRACKETS"},"action":{"input":"$['counter']","output":{"statId":10}}},
    {"bad":1}
]})";

std::string trophyPackage() {
    return ucp({{"tropconf.json", TropConf}, {"tropmeta_en-US.json", TropMetaEnglish}, {"tropmeta_fr-FR.json", TropMetaFrench}, {"trop0001.png", "not an image"}});
}

std::string statPackage() {
    return ucp({{"stats_definition.json", StatsDefinition}, {"stats_extraction.json", StatsExtraction}});
}

void testContainer() {
    const auto data = ucp({{"a.json", "{}"}, {"b.bin", std::string("\0\1\2", 3)}, {"a.json", "second"}});
    const auto files = NpTrophy2::ReadUcp(data);
    check(files.size() == 2 && files.at("a.json") == "{}" && files.at("b.bin") == std::string_view("\0\1\2", 3), "the container files were not read, or a repeated name replaced the first");
    check(NpTrophy2::ReadUcp(ucp({})).empty(), "an empty container was not read");
    const std::string longName(0x20, 'n');
    const auto named = NpTrophy2::ReadUcp(ucp({{longName, "x"}}));
    check(named.size() == 1 && named.begin()->first == longName, "a name that fills its 32 bytes was not read whole");
    auto padded = data + "trailing";
    check(NpTrophy2::ReadUcp(padded).size() == 2, "bytes after the container size were not ignored");

    for (std::size_t length = 0; length < data.size(); ++length) {
        check(!failure([&] { NpTrophy2::ReadUcp(std::string_view(data).substr(0, length)); }).empty(), "a truncated container was read");
    }
    check(contains(failure([&] { NpTrophy2::ReadUcp(std::string_view(data).substr(0, 0x3F)); }), "fewer than the 64-byte header"), "a short header was not named");
    auto corrupt = data;
    corrupt[0] = 0;
    check(contains(failure([&] { NpTrophy2::ReadUcp(corrupt); }), "magic is 0x28c60a"), "a wrong magic was not named");
    corrupt = data;
    putBigEndian(corrupt, 4, 2, 4);
    check(contains(failure([&] { NpTrophy2::ReadUcp(corrupt); }), "version is 2"), "a wrong version was not named");
    corrupt = data;
    putBigEndian(corrupt, 8, 0x3F, 8);
    check(contains(failure([&] { NpTrophy2::ReadUcp(corrupt); }), "its size 0x3f"), "a size inside the header was accepted");
    corrupt = data;
    putBigEndian(corrupt, 8, data.size() + 1, 8);
    check(contains(failure([&] { NpTrophy2::ReadUcp(corrupt); }), "is not from the header size"), "a size past the file was accepted");
    corrupt = data;
    putBigEndian(corrupt, 0x10, 4097, 4);
    check(contains(failure([&] { NpTrophy2::ReadUcp(corrupt); }), "more than 4096"), "too many files were accepted");
    corrupt = data;
    putBigEndian(corrupt, 0x10, 40, 4);
    check(contains(failure([&] { NpTrophy2::ReadUcp(corrupt); }), "table of 40 files"), "a table past the size was accepted");
    corrupt = data;
    putBigEndian(corrupt, 0x14, data.size() + 1, 4);
    check(contains(failure([&] { NpTrophy2::ReadUcp(corrupt); }), "ends past its size"), "a table offset past the size was accepted");
    corrupt = data;
    putBigEndian(corrupt, 0x60 + 0x20, data.size() + 1, 8);
    check(contains(failure([&] { NpTrophy2::ReadUcp(corrupt); }), "its file \"a.json\""), "a file offset past the size was accepted");
    corrupt = data;
    putBigEndian(corrupt, 0x60 + 0x28, ~std::uint64_t{0}, 8);
    check(contains(failure([&] { NpTrophy2::ReadUcp(corrupt); }), "ends past its size"), "a file length past the size was accepted");
}

void testJson() {
    const auto value = NpTrophy2::ParseJson("\xEF\xBB\xBF {\"a\": [1, -2.5e+3, \"x\\u00e9\\ud83d\\ude00\\n\", true, false, null, {}], \"a\": 7, \"b\": \"12\"} ");
    check(value.kind == NpTrophy2::Json::Kind::Object && value.members.size() == 3, "an object was not parsed");
    check(value.Find("a") != nullptr && value.Find("a")->text == "7", "a repeated key did not keep its last value");
    const auto& list = value.members[0].second.items;
    check(list.size() == 7 && list[0].text == "1" && list[1].text == "-2.5e+3" && list[2].text == "x\xC3\xA9\xF0\x9F\x98\x80\n", "array values or escapes were not parsed");
    check(list[3].IsTrue() && !list[4].IsTrue() && list[5].kind == NpTrophy2::Json::Kind::Null && list[6].kind == NpTrophy2::Json::Kind::Object, "literals were not parsed");
    check(value.Find("b")->Unsigned() == std::optional<std::uint64_t>(12) && list[0].Unsigned() == std::optional<std::uint64_t>(1), "numbers and numeric strings are not unsigned");
    check(!list[1].Unsigned() && !list[3].Unsigned() && !NpTrophy2::ParseJson("\"\"").Unsigned() && !NpTrophy2::ParseJson("\" 1\"").Unsigned(), "a value that is not an unsigned integer was read as one");
    check(NpTrophy2::ParseJson("18446744073709551615").Unsigned() == std::optional<std::uint64_t>(UINT64_MAX) && !NpTrophy2::ParseJson("18446744073709551616").Unsigned(), "the unsigned range is wrong");
    check(NpTrophy2::ParseJson("2147483647").Id() == std::optional<std::int32_t>(INT32_MAX) && !NpTrophy2::ParseJson("2147483648").Id() && !NpTrophy2::ParseJson("-1").Id(), "the id range is wrong");
    std::string deep(64, '[');
    deep += std::string(64, ']');
    check(NpTrophy2::ParseJson(deep).kind == NpTrophy2::Json::Kind::Array, "64 levels of nesting were refused");
    for (const char* text : {"", "{", "[1,]", "{\"a\":1,}", "{\"a\" 1}", "{a:1}", "01", "1.", "-", "1e", "+1", "\"\\x\"", "\"\\u12g4\"", "\"\\ud800\"", "\"\\udc00\"",
                             "\"\\ud800\\u0041\"", "\"a\nb\"", "\"open", "tru", "nul", "{} {}", "[1] x"}) {
        check(!failure([text] { NpTrophy2::ParseJson(text); }).empty(), "malformed JSON was parsed");
    }
    const auto deeper = "[" + deep + "]";
    check(contains(failure([&deeper] { NpTrophy2::ParseJson(deeper); }), "deeper than 64"), "deep nesting was not refused");
    std::string most = "[";
    for (std::size_t value = 2; value < 1048576; ++value) most += "0,";
    most += "0]";
    check(NpTrophy2::ParseJson(most).items.size() == 1048575, "a document of 1048576 values was refused");
    const auto more = "[0," + most.substr(1);
    check(contains(failure([&more] { NpTrophy2::ParseJson(more); }), "holds more than 1048576 values"), "a document of more than 1048576 values was parsed");
}

void testPackage() {
    const auto package = NpTrophy2::ParseTrophyPackage(trophyPackage(), "en-US");
    check(package.language == "en-US" && package.title == "Synthetic Game" && package.trophies.size() == 8, "the package was not read");
    const auto& platinum = package.trophies.at(0);
    check(platinum.grade == NpTrophy2::GradePlatinum && platinum.name == "All of them" && platinum.detail == "Unlock every trophy" && !platinum.Condition(), "the platinum was not read");
    const auto& gold = package.trophies.at(1);
    check(gold.grade == NpTrophy2::GradeGold && gold.name == "Ten" && gold.platinumId == std::optional<std::int32_t>(0) && gold.groupId == NpTrophy2::BaseGroup, "trophy 1 was not read");
    const auto condition = gold.Condition();
    check(condition && condition->statId == 10 && condition->target == 10 && condition->comparator == NpTrophy2::Comparator::GreaterEqual && !gold.progressive, "trophy 1's condition was not read");
    check(package.trophies.at(2).grade == NpTrophy2::GradeSilver && package.trophies.at(3).grade == NpTrophy2::GradeBronze, "grades were not read");
    check(package.trophies.at(2).Condition()->comparator == NpTrophy2::Comparator::Greater && package.trophies.at(3).Condition()->comparator == NpTrophy2::Comparator::LessEqual &&
              package.trophies.at(4).Condition()->comparator == NpTrophy2::Comparator::Less,
          "comparators were not read");
    const auto& progressive = package.trophies.at(5);
    check(progressive.progressive && NpTrophy2::ProgressTarget(progressive) == std::optional<std::uint64_t>(5) && !NpTrophy2::ProgressTarget(gold), "the progressive target was not read");
    const auto& hidden = package.trophies.at(6);
    check(hidden.hidden && hidden.hasReward && hidden.groupId == 1 && hidden.name == "Secret" && hidden.reward == "A hat" && !hidden.platinumId, "trophy 6 was not read");
    check(!package.trophies.at(7).Condition() && package.trophies.at(7).name.empty(), "an unusable condition was kept");
    check(package.skipped.size() == 1 && contains(package.skipped[0], "trophy 7 never unlocks through stats") && contains(package.skipped[0], "\"-1\"") && contains(package.skipped[0], "\"eq\""),
          "the unusable condition was not reported");
    check(package.groups.size() == 2 && package.groups.at(NpTrophy2::BaseGroup) == "Synthetic Game" && package.groups.at(1) == "Expansion", "groups were not read");

    const auto french = NpTrophy2::ParseTrophyPackage(trophyPackage(), "fr-FR");
    check(french.language == "fr-FR" && french.title == "Jeu synth\xC3\xA9tique" && french.trophies.at(1).name == "Dix" && french.trophies.at(6).name.empty(), "the system language was not used");
    check(NpTrophy2::ParseTrophyPackage(trophyPackage(), "de-DE").language == "en-US", "English was not the fallback");
    std::string conf = TropConf;
    conf.replace(conf.find("en-US"), 5, "ja-JP");
    check(NpTrophy2::ParseTrophyPackage(ucp({{"tropconf.json", conf}, {"tropmeta_ja-JP.json", TropMetaEnglish}}), "de-DE").language == "ja-JP", "the default language was not the last fallback");
    const auto withoutTexts = ucp({{"tropconf.json", conf}});
    check(contains(failure([&withoutTexts] { NpTrophy2::ParseTrophyPackage(withoutTexts, "de-DE"); }), "no tropmeta_<language>.json for de-DE, en-US or its defaultLanguage \"ja-JP\""),
          "a package without texts was read");
    const auto missingTexts = [](const std::string& defaultLanguage, const char* language) {
        std::string text = TropConf;
        text.replace(text.find("en-US"), 5, defaultLanguage);
        const auto data = ucp({{"tropconf.json", text}});
        return failure([&data, language] { NpTrophy2::ParseTrophyPackage(data, language); });
    };
    check(missingTexts("en-US", "en-US") == "it has no tropmeta_<language>.json for en-US", "a missing text named en-US more than once");
    check(missingTexts("ja-JP", "en-US") == "it has no tropmeta_<language>.json for en-US or its defaultLanguage \"ja-JP\"", "a missing text in English named English twice");
    check(missingTexts("en-US", "de-DE") == "it has no tropmeta_<language>.json for de-DE or en-US", "a missing text with an English default named English twice");

    const auto broken = [](const std::string& replaced, const std::string& replacement) {
        std::string text = TropConf;
        text.replace(text.find(replaced), replaced.size(), replacement);
        const auto data = ucp({{"tropconf.json", text}, {"tropmeta_en-US.json", TropMetaEnglish}});
        return failure([&data] { NpTrophy2::ParseTrophyPackage(data, "en-US"); });
    };
    check(contains(broken(R"("grade":"G")", R"("grade":"X")"), "trophy 1 has no grade"), "a wrong grade was accepted");
    check(contains(broken(R"("grade":"G")", R"("grade":"GS")"), "trophy 1 has no grade"), "a two-letter grade was accepted");
    check(contains(broken(R"({"id":"4",)", R"({"id":"3",)"), "lists trophy 3 twice"), "a repeated trophy was accepted");
    check(contains(broken(R"({"id":"4",)", R"({"id":"-4",)"), "trophies[4] has no id"), "a negative id was accepted");
    check(contains(broken(R"("groupId":"1","hidden")", R"("groupId":"x","hidden")"), "trophy 6 has groupId \"x\""), "a wrong group was accepted");
    check(contains(broken(R"("udsStatId":"10")", R"("udsStatId":"-10")"), "udsStatId \"-10\""), "a negative stat id was accepted");
    check(contains(broken(R"("trophies":[)", R"("trophies":0,"other":[)"), "has no trophies array"), "trophies that are not an array were accepted");
    const auto withoutConf = ucp({{"tropmeta_en-US.json", TropMetaEnglish}});
    check(contains(failure([&withoutConf] { NpTrophy2::ParseTrophyPackage(withoutConf, "en-US"); }), "no tropconf.json"), "a package without tropconf.json was read");
    const auto brokenConf = ucp({{"tropconf.json", "{\"trophies\":["}, {"tropmeta_en-US.json", TropMetaEnglish}});
    check(contains(failure([&brokenConf] { NpTrophy2::ParseTrophyPackage(brokenConf, "en-US"); }), "tropconf.json is not JSON"), "a tropconf.json that is not JSON was read");
    const auto withoutTitle = ucp({{"tropconf.json", TropConf}, {"tropmeta_en-US.json", "{\"metadata\":{}}"}});
    check(contains(failure([&withoutTitle] { NpTrophy2::ParseTrophyPackage(withoutTitle, "en-US"); }), "has no titleMetadata"), "texts without a title were read");
    std::string groups = R"({"defaultLanguage":"en-US","trophies":[)";
    for (int id = 0; id <= 50; ++id) groups += (id == 0 ? "" : ",") + std::string(R"({"id":)") + std::to_string(id) + R"(,"grade":"B","groupId":)" + std::to_string(id) + "}";
    groups += "]}";
    const auto manyGroups = ucp({{"tropconf.json", groups}, {"tropmeta_en-US.json", TropMetaEnglish}});
    check(contains(failure([&manyGroups] { NpTrophy2::ParseTrophyPackage(manyGroups, "en-US"); }), "more than 50 groups"), "more than 50 groups were accepted");
    std::string padded = TropConf;
    std::string padding = "\"padding\":[";
    for (std::size_t value = 0; value < 1048576; ++value) padding += "0,";
    padded.insert(1, padding + "0],");
    const auto oversized = ucp({{"tropconf.json", padded}, {"tropmeta_en-US.json", TropMetaEnglish}});
    check(contains(failure([&oversized] { NpTrophy2::ParseTrophyPackage(oversized, "en-US"); }), "tropconf.json is not JSON: the document holds more than 1048576 values"),
          "a tropconf.json of more than 1048576 values was read");
    check(contains(failure([] { NpTrophy2::ParseTrophyPackage("not a package", "en-US"); }), "fewer than the 64-byte header"), "a package that is not a container was read");
    check(NpTrophy2::LanguageTag(1) == "en-US" && NpTrophy2::LanguageTag(0) == "ja-JP" && NpTrophy2::LanguageTag(2) == "fr-FR" && NpTrophy2::LanguageTag(29) == "id-ID", "language tags are wrong");
    bool thrown = false;
    try {
        NpTrophy2::LanguageTag(31);
    } catch (const std::out_of_range&) {
        thrown = true;
    }
    check(thrown, "language 31 has a tag");
}

void testProgressAndPlatinum() {
    const auto package = NpTrophy2::ParseTrophyPackage(trophyPackage(), "en-US");
    std::set<std::int32_t> unlocked;
    const NpTrophy2::UnlockedQuery isUnlocked = [&unlocked](std::int32_t id) { return unlocked.contains(id); };
    const auto& progressive = package.trophies.at(5);
    check(NpTrophy2::TrophyProgress(progressive, std::nullopt, false) == 0 && NpTrophy2::TrophyProgress(progressive, 3, false) == 3 && NpTrophy2::TrophyProgress(progressive, 9, false) == 5 &&
              NpTrophy2::TrophyProgress(progressive, 1, true) == 5,
          "progress is not the stat up to the target");
    bool thrown = false;
    try {
        NpTrophy2::TrophyProgress(package.trophies.at(1), 3, false);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    check(thrown, "a trophy that is not progressive reported progress");

    unlocked = {1, 2, 3, 4};
    check(!NpTrophy2::PlatinumToUnlock(package, 4, isUnlocked), "the platinum unlocked before trophy 5");
    unlocked.insert(5);
    check(NpTrophy2::PlatinumToUnlock(package, 5, isUnlocked) == std::optional<std::int32_t>(0), "the platinum did not unlock after the last trophy that names it");
    check(!NpTrophy2::PlatinumToUnlock(package, 6, isUnlocked) && !NpTrophy2::PlatinumToUnlock(package, 0, isUnlocked) && !NpTrophy2::PlatinumToUnlock(package, 42, isUnlocked),
          "a trophy that names no platinum unlocked one");
    unlocked.insert(0);
    check(!NpTrophy2::PlatinumToUnlock(package, 5, isUnlocked), "an unlocked platinum was unlocked again");

    unlocked = {1, 6};
    const auto all = NpTrophy2::CountTrophies(package, std::nullopt, isUnlocked);
    check(all.trophies == 8 && all.unlocked == 2 && all.grades[NpTrophy2::GradePlatinum] == 1 && all.grades[NpTrophy2::GradeGold] == 1 && all.grades[NpTrophy2::GradeSilver] == 1 &&
              all.grades[NpTrophy2::GradeBronze] == 5 && all.unlockedGrades[NpTrophy2::GradeGold] == 1 && all.unlockedGrades[NpTrophy2::GradeBronze] == 1,
          "the game counts are wrong");
    check(all.Percentage() == (6 + 1) * 100 / (6 + 2 + 5), "the progress percentage does not weigh gold 6, silver 2 and bronze 1");
    const auto expansion = NpTrophy2::CountTrophies(package, 1, isUnlocked);
    check(expansion.trophies == 2 && expansion.unlocked == 1 && expansion.Percentage() == 50, "the group counts are wrong");
    check(NpTrophy2::CountTrophies(package, 5, isUnlocked).trophies == 0 && NpTrophy2::CountTrophies(package, 5, isUnlocked).Percentage() == 0, "an empty group has trophies");
}

void testStatRules() {
    const auto rules = NpTrophy2::ParseStatRules(statPackage());
    check(rules.stats.size() == 4 && rules.stats.at(10).type == NpTrophy2::StatType::UInt64 && rules.stats.at(30).type == NpTrophy2::StatType::UInt32 && rules.stats.at(30).maximum == UINT32_MAX &&
              rules.stats.at(40).maximum == 50 && rules.stats.at(10).minimum == 0,
          "stat definitions were not read");
    check(rules.events.size() == 3 && rules.events.at("COLLECT").size() == 2 && rules.events.at("COLLECT")[1].input == "total" && rules.events.at("COLLECT")[1].statId == 20 &&
              rules.events.at("STEPS")[0].input == "player.steps" && rules.events.at("TIME")[0].statId == 30,
          "extraction rules were not read");
    std::set<std::string> skipped(rules.skipped.begin(), rules.skipped.end());
    const auto reported = [&skipped](const char* part) {
        for (const auto& line : skipped) {
            if (contains(line, part)) return true;
        }
        return false;
    };
    check(rules.skipped.size() == 9, "unsupported definitions and rules were not all reported");
    check(reported("stat 50 has aggregation \"sum\"") && reported("stat 60 has dataType \"int32\"") && reported("stat 70 has maxValue \"5000000000\"") && reported("statDefinitionArray[7] has no unsigned statId"),
          "unsupported definitions were not reported");
    check(reported("for SUM writes stat 50") && reported("filters FILTERED") && reported("for COUNT reads no input") && reported("for BRACKETS reads \"$['counter']\"") && reported("[8] has no condition"),
          "unsupported rules were not reported");
    for (const char* input : {"$", "$.", "$..a", "$.a.", "$.a-b", "counter", "$.a b"}) {
        const std::string extraction = std::string(R"({"statsExtractionRuleArray":[{"condition":{"eventName":"E"},"action":{"input":")") + input + R"(","output":{"statId":10}}}]})";
        check(NpTrophy2::ParseStatRules(ucp({{"stats_definition.json", StatsDefinition}, {"stats_extraction.json", extraction}})).events.empty(), "an unsupported input path was accepted");
    }
    const auto withoutDefinitions = ucp({{"stats_extraction.json", StatsExtraction}});
    check(contains(failure([&withoutDefinitions] { NpTrophy2::ParseStatRules(withoutDefinitions); }), "no stats_definition.json"), "rules without definitions were read");
    const auto withoutRules = ucp({{"stats_definition.json", StatsDefinition}, {"stats_extraction.json", "{}"}});
    check(contains(failure([&withoutRules] { NpTrophy2::ParseStatRules(withoutRules); }), "has no statsExtractionRuleArray"), "rules without an extraction array were read");
}

void testConditions() {
    const auto package = NpTrophy2::ParseTrophyPackage(trophyPackage(), "en-US");
    std::set<std::int32_t> unlocked;
    const NpTrophy2::UnlockedQuery isUnlocked = [&unlocked](std::int32_t id) { return unlocked.contains(id); };
    const auto reached = [&](std::uint64_t stat, std::uint64_t value) { return NpTrophy2::ReachedTrophies(package, {{stat, value}}, isUnlocked); };
    check(reached(10, 9).empty() && reached(10, 10) == std::vector<std::int32_t>{1} && reached(10, 11) == std::vector<std::int32_t>{1}, "ge did not unlock at its target");
    check(reached(20, 100).empty() && reached(20, 101) == std::vector<std::int32_t>{2}, "gt did not unlock past its target");
    check(reached(30, 6).empty() && reached(30, 5) == std::vector<std::int32_t>{3} && reached(30, 3) == std::vector<std::int32_t>{3} && reached(30, 2) == (std::vector<std::int32_t>{3, 4}),
          "le and lt did not unlock at and below their targets");
    check(reached(40, 4).empty() && reached(40, 5) == std::vector<std::int32_t>{5}, "the progressive trophy did not unlock at its target");
    check(reached(99, 1000).empty() && NpTrophy2::ReachedTrophies(package, {}, isUnlocked).empty(), "a stat without a trophy, or no stat, unlocked a trophy");
    unlocked.insert(1);
    check(reached(10, 10).empty(), "an unlocked trophy was reached again");
    check(NpTrophy2::ReachedTrophies(package, {{10, 10}, {20, 101}, {30, 0}, {40, 9}}, isUnlocked) == (std::vector<std::int32_t>{2, 3, 4, 5}), "one set of stats did not reach every trophy");
}

void testStatStore() {
    const std::filesystem::path directory("stats");
    {
        NpTrophy2::StatStore stats(directory);
        check(!stats.Value(InitialUser, 0, 10) && stats.Values(InitialUser, 0).empty(), "a new store has stats");
        stats.Set(InitialUser, 0, 10, 7);
        stats.Set(InitialUser, 0, 18446744073709551615ull, 1);
        stats.Set(InitialUser, 0, 10, 9);
        stats.Set(InitialUser + 1, 0, 10, 3);
        stats.Set(InitialUser, 1, 10, 4);
        check(stats.RecordPath(InitialUser, 0) == directory / "268435456_0.stats.txt", "the stats are not stored next to the unlocks");
    }
    NpTrophy2::StatStore restarted(directory);
    check(restarted.Value(InitialUser, 0, 10) == std::optional<std::uint64_t>(9) && restarted.Value(InitialUser + 1, 0, 10) == std::optional<std::uint64_t>(3) &&
              restarted.Value(InitialUser, 1, 10) == std::optional<std::uint64_t>(4) && restarted.Values(InitialUser, 0).size() == 2,
          "stats did not survive a restart");
    for (const char* text : {"10\n", "10 x\n", "x 5\n", "-1 5\n", "10 5\n10 6\n", "10 5 7\n", "\n", "10 -5\n"}) {
        std::filesystem::remove_all("damaged");
        std::filesystem::create_directories("damaged");
        std::FILE* file = std::fopen("damaged/268435456_0.stats.txt", "wb");
        check(file != nullptr, "a damaged stat record was not created");
        std::fputs(text, file);
        std::fclose(file);
        NpTrophy2::StatStore damaged("damaged");
        check(contains(failure([&damaged] { damaged.Values(InitialUser, 0); }), "stat record"), "a damaged stat record was read");
    }
}

NpTrophy2Event event(const char* name, std::vector<NpTrophy2EventProperty> properties) {
    return {name, std::move(properties)};
}

void testApplyStatRules() {
    const auto rules = NpTrophy2::ParseStatRules(statPackage());
    NpTrophy2::StatStore stats("applied");
    const auto none = NpTrophy2::ApplyStatRules(rules, event("UNKNOWN", {{"counter", 5, "5"}}), stats, InitialUser, 0);
    check(none.updates.empty() && none.problems.empty() && stats.Values(InitialUser, 0).empty(), "an unknown event changed a stat");
    const auto collect = NpTrophy2::ApplyStatRules(rules, event("COLLECT", {{"counter", 12, "12"}, {"total", 300, "300"}, {"other", 1, "1"}}), stats, InitialUser, 0);
    check(collect.problems.empty() && collect.updates.size() == 2 && collect.updates[0].statId == 10 && collect.updates[0].value == 12 && collect.updates[0].input == "counter" &&
              collect.updates[1].statId == 20 && collect.updates[1].value == 300,
          "an event did not update every stat it feeds");
    check(stats.Value(InitialUser, 0, 10) == std::optional<std::uint64_t>(12) && stats.Value(InitialUser, 0, 20) == std::optional<std::uint64_t>(300), "the updated stats were not stored");
    const auto missing = NpTrophy2::ApplyStatRules(rules, event("COLLECT", {{"counter", 13, "13"}}), stats, InitialUser, 0);
    check(missing.updates.size() == 1 && missing.problems.size() == 1 && missing.problems[0].rule == "COLLECT $.total for stat 20" && missing.problems[0].reason == "the event has no such property" &&
              stats.Value(InitialUser, 0, 20) == std::optional<std::uint64_t>(300),
          "a missing property changed its stat");
    const auto wrong = NpTrophy2::ApplyStatRules(rules, event("TIME", {{"seconds", std::nullopt, "-3"}}), stats, InitialUser, 0);
    check(wrong.updates.empty() && wrong.problems.size() == 1 && wrong.problems[0].reason == "-3 is not an unsigned integer" && !stats.Value(InitialUser, 0, 30), "a negative value changed a stat");
    const auto wide = NpTrophy2::ApplyStatRules(rules, event("TIME", {{"seconds", 4294967296ull, "4294967296"}}), stats, InitialUser, 0);
    check(wide.updates.empty() && wide.problems.size() == 1 && wide.problems[0].reason == "4294967296 is not from 0 to 4294967295", "a uint32 stat took a value it cannot hold");
    const auto bounded = NpTrophy2::ApplyStatRules(rules, event("STEPS", {{"player.steps", 51, "51"}}), stats, InitialUser, 0);
    check(bounded.updates.empty() && bounded.problems[0].reason == "51 is not from 0 to 50", "a value above maxValue changed a stat");
    const auto nested = NpTrophy2::ApplyStatRules(rules, event("STEPS", {{"steps", 2, "2"}, {"player.steps", 50, "50"}}), stats, InitialUser, 0);
    check(nested.updates.size() == 1 && nested.updates[0].statId == 40 && stats.Value(InitialUser, 0, 40) == std::optional<std::uint64_t>(50), "a nested property was not read");
    const auto latest = NpTrophy2::ApplyStatRules(rules, event("COLLECT", {{"counter", 2, "2"}, {"total", 1, "1"}}), stats, InitialUser, 0);
    check(latest.updates.size() == 2 && stats.Value(InitialUser, 0, 10) == std::optional<std::uint64_t>(2), "a stat did not keep the latest value");
}

}

int main() {
    const auto previous = std::filesystem::current_path();
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-trophy-package-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root);
    std::filesystem::current_path(root);
    testContainer();
    testJson();
    testPackage();
    testProgressAndPlatinum();
    testStatRules();
    testConditions();
    testStatStore();
    testApplyStatRules();
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(root);
    std::puts("Trophy package tests passed");
    return 0;
}
