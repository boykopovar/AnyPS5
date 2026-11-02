#ifndef CORE_LIBS_PRX_LIBSCENPTROPHY2_INCLUDE_TROPHYPACKAGE_HPP
#define CORE_LIBS_PRX_LIBSCENPTROPHY2_INCLUDE_TROPHYPACKAGE_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace NpTrophy2 {

inline constexpr std::uint32_t UcpMagic = 0xB228C60Au;
inline constexpr std::uint32_t UcpVersion = 1;
inline constexpr std::uint64_t UcpHeaderSize = 0x40;
inline constexpr std::uint64_t UcpTocHeaderSize = 0x20;
inline constexpr std::uint64_t UcpEntrySize = 0x40;
inline constexpr std::size_t UcpNameSize = 0x20;
inline constexpr std::uint64_t UcpMaxEntries = 4096;

using UcpFiles = std::map<std::string, std::string_view, std::less<>>;

UcpFiles ReadUcp(std::string_view data);

class Json {
public:
    enum class Kind { Null, Bool, Number, String, Array, Object };

    Kind kind = Kind::Null;
    bool boolean = false;
    std::string text;
    std::vector<Json> items;
    std::vector<std::pair<std::string, Json>> members;

    const Json* Find(std::string_view key) const;
    bool IsTrue() const;
    std::optional<std::uint64_t> Unsigned() const;
    std::optional<std::int32_t> Id() const;
};

Json ParseJson(std::string_view text);

inline constexpr std::int32_t GradePlatinum = 1;
inline constexpr std::int32_t GradeGold = 2;
inline constexpr std::int32_t GradeSilver = 3;
inline constexpr std::int32_t GradeBronze = 4;
inline constexpr std::int32_t BaseGroup = -1;
inline constexpr std::size_t MaxTrophies = 1000;
inline constexpr std::size_t MaxGroups = 50;

enum class Comparator { None, GreaterEqual, Greater, LessEqual, Less };

struct UnlockCondition {
    std::uint64_t statId = 0;
    std::uint64_t target = 0;
    Comparator comparator = Comparator::None;

    bool Reached(std::uint64_t value) const;
};

struct TrophyDefinition {
    std::int32_t id = 0;
    std::int32_t grade = GradeBronze;
    std::int32_t groupId = BaseGroup;
    bool hidden = false;
    bool hasReward = false;
    bool progressive = false;
    std::optional<std::int32_t> platinumId;
    std::optional<std::uint64_t> statId;
    std::optional<std::uint64_t> target;
    Comparator comparator = Comparator::None;
    std::string name;
    std::string detail;
    std::string reward;

    std::optional<UnlockCondition> Condition() const;
};

struct TrophyPackage {
    std::string language;
    std::string title;
    std::map<std::int32_t, TrophyDefinition> trophies;
    std::map<std::int32_t, std::string> groups;
    std::vector<std::string> skipped;
};

TrophyPackage ParseTrophyPackage(std::string_view ucp, std::string_view language);

enum class StatType { UInt32, UInt64, Int32 };

enum class StatAggregation { Latest, Max };

struct StatDefinition {
    StatType type = StatType::UInt64;
    StatAggregation aggregation = StatAggregation::Latest;
    std::uint64_t minimum = 0;
    std::uint64_t maximum = UINT64_MAX;
    std::uint64_t initial = 0;
};

struct StatFilter {
    std::string path;
    std::uint64_t value = 0;
};

struct StatRule {
    std::string input;
    std::uint64_t statId = 0;
    std::optional<StatFilter> filter;
};

struct StatRules {
    std::map<std::uint64_t, StatDefinition> stats;
    std::map<std::string, std::vector<StatRule>, std::less<>> events;
    std::vector<std::string> skipped;
};

StatRules ParseStatRules(std::string_view ucp);

std::string_view LanguageTag(int consoleLanguage);

}

#endif
