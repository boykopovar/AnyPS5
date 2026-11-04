#ifndef CORE_LIBS_PRX_LIBSCENPUNIVERSALDATASYSTEM_INCLUDE_UDSEVENT_HPP
#define CORE_LIBS_PRX_LIBSCENPUNIVERSALDATASYSTEM_INCLUDE_UDSEVENT_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

struct NpUniversalDataSystemEventPropertyObject;
struct NpUniversalDataSystemEventPropertyArray;
struct NpTrophy2Event;

namespace Uds {

using Binary = std::vector<std::uint8_t>;
using Value = std::variant<std::int32_t, std::uint32_t, std::int64_t, std::uint64_t, float, double, bool, std::string, Binary,
                           std::shared_ptr<NpUniversalDataSystemEventPropertyObject>, std::shared_ptr<NpUniversalDataSystemEventPropertyArray>>;

Value Clone(const Value& value);

}

struct NpUniversalDataSystemEventPropertyObject {
    std::vector<std::pair<std::string, Uds::Value>> properties;

    Uds::Value& Set(std::string_view key, Uds::Value value);
    const Uds::Value* Find(std::string_view key) const;
    NpUniversalDataSystemEventPropertyObject Clone() const;
};

struct NpUniversalDataSystemEventPropertyArray {
    std::vector<Uds::Value> values;

    Uds::Value& Add(Uds::Value value);
    NpUniversalDataSystemEventPropertyArray Clone() const;
};

struct NpUniversalDataSystemEvent {
    std::string name;
    NpUniversalDataSystemEventPropertyObject properties;
};

namespace Uds {

inline constexpr std::string_view UnlockTrophyEvent = "_UnlockTrophy";
inline constexpr std::string_view UpdateTrophyProgressEvent = "_UpdateTrophyProgress";
inline constexpr std::string_view TrophyIdProperty = "_trophy_id";
inline constexpr std::string_view EventNameKey = "eventName";
inline constexpr std::string_view PropertiesKey = "properties";

enum class TrophyEventKind { None, Unlock, Progress, Malformed };

struct TrophyEvent {
    TrophyEventKind kind = TrophyEventKind::None;
    std::int32_t trophyId = -1;
    std::string problem;
};

TrophyEvent RecognizeTrophyEvent(const NpUniversalDataSystemEvent& event);
std::string Describe(const Value& value);
std::string Describe(const NpUniversalDataSystemEventPropertyObject& object);
std::string Describe(const NpUniversalDataSystemEvent& event);
std::size_t MessagePackSize(const NpUniversalDataSystemEvent& event);
std::string DebugString(const NpUniversalDataSystemEvent& event);
NpTrophy2Event StatEvent(const NpUniversalDataSystemEvent& event);

}

#endif
