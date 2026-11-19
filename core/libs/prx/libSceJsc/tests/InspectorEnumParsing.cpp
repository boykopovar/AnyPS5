#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

using Jsc::InspectorAbi::ConsoleMessageLevel;
using Jsc::InspectorAbi::CoordinateSystem;
using Jsc::InspectorAbi::ExecutionContextType;
using Jsc::InspectorAbi::OptionalEnum;
using Jsc::InspectorAbi::ProtocolString;
using Jsc::WtfAbi::StringImpl;
using Jsc::WtfAbi::StringImplFlag8Bit;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Inspector enum parsing check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

template<typename F> bool Throws(F function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

struct Owned8BitString {
    std::string text;
    StringImpl impl{};
    ProtocolString wrapper{};

    explicit Owned8BitString(std::string value) : text(std::move(value)) {
        impl = {1, static_cast<unsigned>(text.size()), text.data(), StringImplFlag8Bit};
        wrapper = {&impl};
    }
};

struct Owned16BitString {
    std::u16string text;
    StringImpl impl{};
    ProtocolString wrapper{};

    explicit Owned16BitString(std::u16string value) : text(std::move(value)) {
        impl = {1, static_cast<unsigned>(text.size()), text.data(), 0};
        wrapper = {&impl};
    }
};

void TestLayout() {
    Require(sizeof(ProtocolString) == 8);
    Require(offsetof(ProtocolString, impl) == 0);
    Require(sizeof(OptionalEnum<ExecutionContextType>) == 8);
    Require(offsetof(OptionalEnum<ExecutionContextType>, hasValue) == 0);
    Require(offsetof(OptionalEnum<ExecutionContextType>, value) == 4);
    Require(sizeof(OptionalEnum<CoordinateSystem>) == 8);
    Require(offsetof(OptionalEnum<CoordinateSystem>, hasValue) == 0);
    Require(offsetof(OptionalEnum<CoordinateSystem>, value) == 4);
    Require(sizeof(OptionalEnum<ConsoleMessageLevel>) == 8);
    Require(offsetof(OptionalEnum<ConsoleMessageLevel>, hasValue) == 0);
    Require(offsetof(OptionalEnum<ConsoleMessageLevel>, value) == 4);
}

void TestExecutionContextType() {
    auto parse = _ZN9Inspector8Protocol16InspectorHelpers24parseEnumValueFromStringINS0_7Runtime20ExecutionContextTypeEEEN3WTF8OptionalIT_EERKNS5_6StringE;

    {
        const Owned8BitString normal("normal");
        const auto r = parse(&normal.wrapper);
        Require(r.hasValue && r.value == ExecutionContextType::Normal && static_cast<std::int32_t>(r.value) == 5);
    }
    {
        const Owned8BitString user("user");
        const auto r = parse(&user.wrapper);
        Require(r.hasValue && r.value == ExecutionContextType::User && static_cast<std::int32_t>(r.value) == 16);
    }
    {
        const Owned8BitString internal("internal");
        const auto r = parse(&internal.wrapper);
        Require(r.hasValue && r.value == ExecutionContextType::Internal && static_cast<std::int32_t>(r.value) == 204);
    }
    {
        const Owned16BitString normal(u"normal");
        const auto r = parse(&normal.wrapper);
        Require(r.hasValue && r.value == ExecutionContextType::Normal);
    }
    {
        const Owned16BitString user(u"user");
        const auto r = parse(&user.wrapper);
        Require(r.hasValue && r.value == ExecutionContextType::User);
    }
    {
        const Owned16BitString internal(u"internal");
        const auto r = parse(&internal.wrapper);
        Require(r.hasValue && r.value == ExecutionContextType::Internal);
    }
    for (const char* invalid : {"", "Normal", "USER", "normal ", " normal", "norm", "internal2", "Viewport", "log"}) {
        const Owned8BitString s(invalid);
        Require(!parse(&s.wrapper).hasValue);
    }
    {
        const Owned16BitString nonAscii(std::u16string{u'u', u's', u'e', static_cast<char16_t>(0x0172)});
        Require(!parse(&nonAscii.wrapper).hasValue);
    }
}

void TestCoordinateSystem() {
    auto parse = _ZN9Inspector8Protocol16InspectorHelpers24parseEnumValueFromStringINS0_4Page16CoordinateSystemEEEN3WTF8OptionalIT_EERKNS5_6StringE;

    {
        const Owned8BitString viewport("Viewport");
        const auto r = parse(&viewport.wrapper);
        Require(r.hasValue && r.value == CoordinateSystem::Viewport && static_cast<std::int32_t>(r.value) == 175);
    }
    {
        const Owned8BitString page("Page");
        const auto r = parse(&page.wrapper);
        Require(r.hasValue && r.value == CoordinateSystem::Page && static_cast<std::int32_t>(r.value) == 176);
    }
    {
        const Owned16BitString viewport(u"Viewport");
        const auto r = parse(&viewport.wrapper);
        Require(r.hasValue && r.value == CoordinateSystem::Viewport);
    }
    {
        const Owned16BitString page(u"Page");
        const auto r = parse(&page.wrapper);
        Require(r.hasValue && r.value == CoordinateSystem::Page);
    }
    for (const char* invalid : {"", "viewport", "page", "PAGE", "Viewport ", "View", "normal", "error"}) {
        const Owned8BitString s(invalid);
        Require(!parse(&s.wrapper).hasValue);
    }
    {
        const Owned16BitString nonAscii(std::u16string{static_cast<char16_t>(0x0150), u'a', u'g', u'e'});
        Require(!parse(&nonAscii.wrapper).hasValue);
    }
}

void TestConsoleMessageLevel() {
    auto parse = _ZN9Inspector8Protocol16InspectorHelpers24parseEnumValueFromStringINS0_7Console14ConsoleMessage5LevelEEEN3WTF8OptionalIT_EERKNS6_6StringE;

    {
        const Owned8BitString log("log");
        const auto r = parse(&log.wrapper);
        Require(r.hasValue && r.value == ConsoleMessageLevel::Log && static_cast<std::int32_t>(r.value) == 70);
    }
    {
        const Owned8BitString info("info");
        const auto r = parse(&info.wrapper);
        Require(r.hasValue && r.value == ConsoleMessageLevel::Info && static_cast<std::int32_t>(r.value) == 71);
    }
    {
        const Owned8BitString warning("warning");
        const auto r = parse(&warning.wrapper);
        Require(r.hasValue && r.value == ConsoleMessageLevel::Warning && static_cast<std::int32_t>(r.value) == 72);
    }
    {
        const Owned8BitString error("error");
        const auto r = parse(&error.wrapper);
        Require(r.hasValue && r.value == ConsoleMessageLevel::Error && static_cast<std::int32_t>(r.value) == 73);
    }
    {
        const Owned8BitString debug("debug");
        const auto r = parse(&debug.wrapper);
        Require(r.hasValue && r.value == ConsoleMessageLevel::Debug && static_cast<std::int32_t>(r.value) == 74);
    }
    {
        const Owned16BitString log(u"log");
        Require(parse(&log.wrapper).value == ConsoleMessageLevel::Log);
    }
    {
        const Owned16BitString info(u"info");
        Require(parse(&info.wrapper).value == ConsoleMessageLevel::Info);
    }
    {
        const Owned16BitString warning(u"warning");
        Require(parse(&warning.wrapper).value == ConsoleMessageLevel::Warning);
    }
    {
        const Owned16BitString error(u"error");
        Require(parse(&error.wrapper).value == ConsoleMessageLevel::Error);
    }
    {
        const Owned16BitString debug(u"debug");
        Require(parse(&debug.wrapper).value == ConsoleMessageLevel::Debug);
    }
    for (const char* invalid : {"", "Log", "WARN", "warnings", "warn", "debug ", "normal", "Page"}) {
        const Owned8BitString s(invalid);
        Require(!parse(&s.wrapper).hasValue);
    }
    {
        const Owned8BitString embeddedNul(std::string("log\0", 4));
        Require(!parse(&embeddedNul.wrapper).hasValue);
    }
    {
        const Owned16BitString nonAscii(std::u16string{u'l', static_cast<char16_t>(0x016F), u'g'});
        Require(!parse(&nonAscii.wrapper).hasValue);
    }
}

void TestNullAndErrors() {
    const ProtocolString nullString{nullptr};
    Require(!_ZN9Inspector8Protocol16InspectorHelpers24parseEnumValueFromStringINS0_7Runtime20ExecutionContextTypeEEEN3WTF8OptionalIT_EERKNS5_6StringE(&nullString).hasValue);
    Require(!_ZN9Inspector8Protocol16InspectorHelpers24parseEnumValueFromStringINS0_4Page16CoordinateSystemEEEN3WTF8OptionalIT_EERKNS5_6StringE(&nullString).hasValue);
    Require(!_ZN9Inspector8Protocol16InspectorHelpers24parseEnumValueFromStringINS0_7Console14ConsoleMessage5LevelEEEN3WTF8OptionalIT_EERKNS6_6StringE(&nullString).hasValue);

    const StringImpl emptyNullData{1, 0, nullptr, StringImplFlag8Bit};
    const ProtocolString emptyString{&emptyNullData};
    Require(!_ZN9Inspector8Protocol16InspectorHelpers24parseEnumValueFromStringINS0_7Runtime20ExecutionContextTypeEEEN3WTF8OptionalIT_EERKNS5_6StringE(&emptyString).hasValue);
    Require(!_ZN9Inspector8Protocol16InspectorHelpers24parseEnumValueFromStringINS0_4Page16CoordinateSystemEEEN3WTF8OptionalIT_EERKNS5_6StringE(&emptyString).hasValue);
    Require(!_ZN9Inspector8Protocol16InspectorHelpers24parseEnumValueFromStringINS0_7Console14ConsoleMessage5LevelEEEN3WTF8OptionalIT_EERKNS6_6StringE(&emptyString).hasValue);

    Require(Throws([] { _ZN9Inspector8Protocol16InspectorHelpers24parseEnumValueFromStringINS0_7Runtime20ExecutionContextTypeEEEN3WTF8OptionalIT_EERKNS5_6StringE(nullptr); }));
    Require(Throws([] { _ZN9Inspector8Protocol16InspectorHelpers24parseEnumValueFromStringINS0_4Page16CoordinateSystemEEEN3WTF8OptionalIT_EERKNS5_6StringE(nullptr); }));
    Require(Throws([] { _ZN9Inspector8Protocol16InspectorHelpers24parseEnumValueFromStringINS0_7Console14ConsoleMessage5LevelEEEN3WTF8OptionalIT_EERKNS6_6StringE(nullptr); }));

    const StringImpl badData{1, 3, nullptr, StringImplFlag8Bit};
    const ProtocolString badString{&badData};
    Require(Throws([&] { _ZN9Inspector8Protocol16InspectorHelpers24parseEnumValueFromStringINS0_7Runtime20ExecutionContextTypeEEEN3WTF8OptionalIT_EERKNS5_6StringE(&badString); }));
    Require(Throws([&] { _ZN9Inspector8Protocol16InspectorHelpers24parseEnumValueFromStringINS0_4Page16CoordinateSystemEEEN3WTF8OptionalIT_EERKNS5_6StringE(&badString); }));
    Require(Throws([&] { _ZN9Inspector8Protocol16InspectorHelpers24parseEnumValueFromStringINS0_7Console14ConsoleMessage5LevelEEEN3WTF8OptionalIT_EERKNS6_6StringE(&badString); }));
}

} // namespace

int main() {
    TestLayout();
    TestExecutionContextType();
    TestCoordinateSystem();
    TestConsoleMessageLevel();
    TestNullAndErrors();
    std::puts("Inspector enum parsing tests passed");
    return 0;
}
