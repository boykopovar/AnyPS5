#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace Jsc::InspectorAbi {

struct ProtocolString {
    const Jsc::WtfAbi::StringImpl* impl;
};

static_assert(sizeof(ProtocolString) == 8);
static_assert(offsetof(ProtocolString, impl) == 0);
static_assert(std::is_trivially_copyable_v<ProtocolString>);

enum class ExecutionContextType : std::int32_t {
    Normal = 5,
    User = 16,
    Internal = 204,
};

enum class CoordinateSystem : std::int32_t {
    Viewport = 175,
    Page = 176,
};

enum class ConsoleMessageLevel : std::int32_t {
    Log = 70,
    Info = 71,
    Warning = 72,
    Error = 73,
    Debug = 74,
};

template<typename TEnum>
struct OptionalEnum {
    bool hasValue{};
    TEnum value{};

    OptionalEnum() = default;
    OptionalEnum(bool engaged, TEnum enumValue) : hasValue(engaged), value(enumValue) {}
    OptionalEnum(const OptionalEnum& other) : hasValue(other.hasValue), value(other.value) {}
    OptionalEnum& operator=(const OptionalEnum& other) = default;
};

static_assert(sizeof(OptionalEnum<ExecutionContextType>) == 8);
static_assert(alignof(OptionalEnum<ExecutionContextType>) == 4);
static_assert(offsetof(OptionalEnum<ExecutionContextType>, hasValue) == 0);
static_assert(offsetof(OptionalEnum<ExecutionContextType>, value) == 4);
static_assert(!std::is_trivially_copyable_v<OptionalEnum<ExecutionContextType>>);

static_assert(sizeof(OptionalEnum<CoordinateSystem>) == 8);
static_assert(alignof(OptionalEnum<CoordinateSystem>) == 4);
static_assert(offsetof(OptionalEnum<CoordinateSystem>, hasValue) == 0);
static_assert(offsetof(OptionalEnum<CoordinateSystem>, value) == 4);
static_assert(!std::is_trivially_copyable_v<OptionalEnum<CoordinateSystem>>);

static_assert(sizeof(OptionalEnum<ConsoleMessageLevel>) == 8);
static_assert(alignof(OptionalEnum<ConsoleMessageLevel>) == 4);
static_assert(offsetof(OptionalEnum<ConsoleMessageLevel>, hasValue) == 0);
static_assert(offsetof(OptionalEnum<ConsoleMessageLevel>, value) == 4);
static_assert(!std::is_trivially_copyable_v<OptionalEnum<ConsoleMessageLevel>>);

} // namespace Jsc::InspectorAbi

extern "C" {

Jsc::InspectorAbi::OptionalEnum<Jsc::InspectorAbi::ExecutionContextType> APS5_VABI _ZN9Inspector8Protocol16InspectorHelpers24parseEnumValueFromStringINS0_7Runtime20ExecutionContextTypeEEEN3WTF8OptionalIT_EERKNS5_6StringE(const Jsc::InspectorAbi::ProtocolString* protocolString);
Jsc::InspectorAbi::OptionalEnum<Jsc::InspectorAbi::CoordinateSystem> APS5_VABI _ZN9Inspector8Protocol16InspectorHelpers24parseEnumValueFromStringINS0_4Page16CoordinateSystemEEEN3WTF8OptionalIT_EERKNS5_6StringE(const Jsc::InspectorAbi::ProtocolString* protocolString);
Jsc::InspectorAbi::OptionalEnum<Jsc::InspectorAbi::ConsoleMessageLevel> APS5_VABI _ZN9Inspector8Protocol16InspectorHelpers24parseEnumValueFromStringINS0_7Console14ConsoleMessage5LevelEEEN3WTF8OptionalIT_EERKNS6_6StringE(const Jsc::InspectorAbi::ProtocolString* protocolString);

}
