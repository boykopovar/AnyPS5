#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <array>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <source_location>
#include <stdexcept>
#include <type_traits>

namespace UdatpgClose {

static_assert(std::is_same_v<decltype(&udatpg_close_nid_postfix), void (APS5_VABI*)(UDateTimePatternGenerator*)>);

namespace {

void CheckNonNullThrows() {
    alignas(8) std::array<std::uint8_t, 16> storage{};
    storage.fill(0xa5);
    bool threw = false;
    try {
        udatpg_close_nid_postfix(reinterpret_cast<UDateTimePatternGenerator*>(storage.data()));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    if (!threw) {
        std::fputs("non-null UDateTimePatternGenerator close did not throw\n", stderr);
        std::abort();
    }
    for (const auto byte : storage) {
        if (byte != 0xa5) {
            std::fputs("non-null UDateTimePatternGenerator close modified storage\n", stderr);
            std::abort();
        }
    }
}

} // namespace

void Run() {
    udatpg_close_nid_postfix(nullptr);
    udatpg_close_nid_postfix(nullptr);
    CheckNonNullThrows();
}

} // namespace UdatpgClose

namespace UdatpgGetBestPattern {

using IcuRecoveredTest::Require;

namespace {

void PreExistingFailure() {
    constexpr std::int32_t IllegalArgument = 1;
    constexpr std::int32_t BufferOverflow = 15;
    char16_t buffer[8] = {u'x', u'y', u'z', u'w', 0, 0, 0, 0};

    std::int32_t status = IllegalArgument;
    Require(udatpg_getBestPattern_nid_postfix(nullptr, nullptr, 5, buffer, 8, &status) == 0);
    Require(status == IllegalArgument);
    Require(buffer[0] == u'x' && buffer[1] == u'y' && buffer[2] == u'z' && buffer[3] == u'w');

    status = BufferOverflow;
    Require(udatpg_getBestPattern_nid_postfix(nullptr, nullptr, -1, buffer, 8, &status) == 0);
    Require(status == BufferOverflow);
    Require(buffer[0] == u'x' && buffer[1] == u'y');

    status = 100;
    Require(udatpg_getBestPattern_nid_postfix(nullptr, nullptr, 0, buffer, 8, &status) == 0);
    Require(status == 100);
    Require(buffer[0] == u'x');
}

void InvalidArguments() {
    constexpr std::int32_t IllegalArgument = 1;
    constexpr std::int32_t NotTerminated = -124;
    char16_t buffer[8] = {u'k', u'e', u'e', u'p', 0, 0, 0, 0};

    std::int32_t status = 0;
    Require(udatpg_getBestPattern_nid_postfix(nullptr, nullptr, 4, buffer, 8, &status) == 0);
    Require(status == IllegalArgument);
    Require(buffer[0] == u'k' && buffer[1] == u'e' && buffer[2] == u'e' && buffer[3] == u'p');

    status = 0;
    Require(udatpg_getBestPattern_nid_postfix(nullptr, nullptr, 1, buffer, 8, &status) == 0);
    Require(status == IllegalArgument);
    Require(buffer[0] == u'k');

    status = 0;
    Require(udatpg_getBestPattern_nid_postfix(nullptr, nullptr, -1, buffer, 8, &status) == 0);
    Require(status == IllegalArgument);
    Require(buffer[0] == u'k');

    status = 0;
    Require(udatpg_getBestPattern_nid_postfix(nullptr, nullptr, -10, buffer, 8, &status) == 0);
    Require(status == IllegalArgument);
    Require(buffer[0] == u'k');

    status = NotTerminated;
    Require(udatpg_getBestPattern_nid_postfix(nullptr, nullptr, 2, buffer, 8, &status) == 0);
    Require(status == IllegalArgument);
    Require(buffer[0] == u'k');

    status = -1;
    Require(udatpg_getBestPattern_nid_postfix(nullptr, nullptr, -1, buffer, 8, &status) == 0);
    Require(status == IllegalArgument);
    Require(buffer[0] == u'k');
}

}

void Run() {
    PreExistingFailure();
    InvalidArguments();
}

} // namespace UdatpgGetBestPattern

namespace UdatpgGetBestPatternWithOptions {

using IcuRecoveredTest::Require;

static_assert(std::is_same_v<
    decltype(&udatpg_getBestPatternWithOptions_nid_postfix),
    std::int32_t (APS5_VABI*)(
        UDateTimePatternGenerator*,
        const char16_t*,
        std::int32_t,
        UDateTimePatternMatchOptions,
        char16_t*,
        std::int32_t,
        std::int32_t*)>);

namespace {

void TestIncomingFailure() {
    constexpr char16_t Fill = 0xa55a;
    for (std::int32_t incoming : {1, 7, 15, 16}) {
        std::int32_t status = incoming;
        std::array<char16_t, 8> buffer;
        buffer.fill(Fill);
        const auto result = udatpg_getBestPatternWithOptions_nid_postfix(
            nullptr, nullptr, 0, UDATPG_MATCH_NO_OPTIONS, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status);
        Require(result == 0);
        Require(status == incoming);
        for (char16_t unit : buffer) {
            Require(unit == Fill);
        }
    }

    for (std::int32_t incoming : {1, 7, 15, 16}) {
        std::int32_t status = incoming;
        std::array<char16_t, 8> buffer;
        buffer.fill(Fill);
        const auto result = udatpg_getBestPatternWithOptions_nid_postfix(
            nullptr, nullptr, 10, UDATPG_MATCH_NO_OPTIONS, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status);
        Require(result == 0);
        Require(status == incoming);
        for (char16_t unit : buffer) {
            Require(unit == Fill);
        }
    }
}

void TestNullSkeletonNonzeroLength() {
    constexpr char16_t Fill = 0xa55a;
    constexpr std::int32_t IllegalArgument = 1;

    for (std::int32_t length : {1, 2, 100, -1, -5}) {
        std::int32_t status = 0;
        std::array<char16_t, 8> buffer;
        buffer.fill(Fill);
        const auto result = udatpg_getBestPatternWithOptions_nid_postfix(
            nullptr, nullptr, length, UDATPG_MATCH_NO_OPTIONS, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status);
        Require(result == 0);
        Require(status == IllegalArgument);
        for (char16_t unit : buffer) {
            Require(unit == Fill);
        }
    }

    {
        std::int32_t status = -124;
        std::array<char16_t, 8> buffer;
        buffer.fill(Fill);
        const auto result = udatpg_getBestPatternWithOptions_nid_postfix(
            nullptr, nullptr, 4, UDATPG_MATCH_NO_OPTIONS, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status);
        Require(result == 0);
        Require(status == IllegalArgument);
        for (char16_t unit : buffer) {
            Require(unit == Fill);
        }
    }

    {
        std::int32_t status = 0;
        const auto result = udatpg_getBestPatternWithOptions_nid_postfix(
            nullptr, nullptr, 1, UDATPG_MATCH_NO_OPTIONS, nullptr, 0, &status);
        Require(result == 0);
        Require(status == IllegalArgument);
    }
}

}

void Run() {
    TestIncomingFailure();
    TestNullSkeletonNonzeroLength();
}

} // namespace UdatpgGetBestPatternWithOptions

namespace UdatpgGetFieldDisplayName {

using IcuRecoveredTest::Require;

static_assert(std::is_same_v<decltype(&udatpg_getFieldDisplayName_nid_postfix),
                             std::int32_t (APS5_VABI*)(const UDateTimePatternGenerator*,
                                                      std::int32_t,
                                                      std::int32_t,
                                                      char16_t*,
                                                      std::int32_t,
                                                      std::int32_t*)>);

namespace {

void CheckPreexistingFailure() {
    std::array<char16_t, 8> buffer = {u'a', u'b', u'c', u'd', 0, 0, 0, 0};
    std::int32_t status = 5;
    const auto ret = udatpg_getFieldDisplayName_nid_postfix(nullptr, 0, 0, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status);
    Require(ret == -1);
    Require(status == 5);
    Require(buffer[0] == u'a' && buffer[1] == u'b' && buffer[2] == u'c' && buffer[3] == u'd');

    status = 1;
    const auto retNull = udatpg_getFieldDisplayName_nid_postfix(nullptr, 1, 2, nullptr, -10, &status);
    Require(retNull == -1);
    Require(status == 1);
}

void CheckInvalidArgument() {
    std::int32_t status = 0;
    const auto ret1 = udatpg_getFieldDisplayName_nid_postfix(nullptr, 0, 0, nullptr, 1, &status);
    Require(ret1 == -1);
    Require(status == 1);

    status = 0;
    const auto ret2 = udatpg_getFieldDisplayName_nid_postfix(nullptr, 0, 0, nullptr, -1, &status);
    Require(ret2 == -1);
    Require(status == 1);

    std::array<char16_t, 4> buffer = {u'x', u'y', u'z', 0};
    status = 0;
    const auto ret3 = udatpg_getFieldDisplayName_nid_postfix(nullptr, 0, 0, buffer.data(), -1, &status);
    Require(ret3 == -1);
    Require(status == 1);
    Require(buffer[0] == u'x' && buffer[1] == u'y' && buffer[2] == u'z');

    status = -124;
    const auto ret4 = udatpg_getFieldDisplayName_nid_postfix(nullptr, 0, 0, nullptr, 4, &status);
    Require(ret4 == -1);
    Require(status == 1);
}

}

void Run() {
    CheckPreexistingFailure();
    CheckInvalidArgument();
}

} // namespace UdatpgGetFieldDisplayName

namespace UdatpgGetSkeleton {

using IcuRecoveredTest::Require;

static_assert(std::is_same_v<decltype(&udatpg_getSkeleton_nid_postfix),
              std::int32_t (APS5_VABI*)(UDateTimePatternGenerator*, const char16_t*, std::int32_t, char16_t*, std::int32_t, std::int32_t*)>);

namespace {

void PreexistingError() {
    constexpr std::int32_t IllegalArgument = 1;
    constexpr std::int32_t BufferOverflow = 15;
    constexpr std::int32_t CustomError = 42;

    for (std::int32_t error : {IllegalArgument, BufferOverflow, CustomError}) {
        std::int32_t status = error;
        std::array<char16_t, 8> buffer = {u'x', u'y', u'z', 0};
        const std::int32_t result = udatpg_getSkeleton_nid_postfix(nullptr, nullptr, 0, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status);
        Require(result == 0);
        Require(status == error);
        Require(buffer[0] == u'x' && buffer[1] == u'y' && buffer[2] == u'z' && buffer[3] == 0);

        status = error;
        const char16_t pattern[] = u"yyyyMMdd";
        const std::int32_t withPattern = udatpg_getSkeleton_nid_postfix(nullptr, pattern, 8, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status);
        Require(withPattern == 0);
        Require(status == error);
        Require(buffer[0] == u'x' && buffer[1] == u'y' && buffer[2] == u'z' && buffer[3] == 0);

        status = error;
        const std::int32_t invalidArgs = udatpg_getSkeleton_nid_postfix(nullptr, nullptr, 5, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status);
        Require(invalidArgs == 0);
        Require(status == error);
        Require(buffer[0] == u'x' && buffer[1] == u'y' && buffer[2] == u'z' && buffer[3] == 0);
    }
}

void NullPatternNonzeroLength() {
    constexpr std::int32_t IllegalArgument = 1;
    constexpr std::int32_t NotTerminated = -124;

    for (std::int32_t length : {1, 2, 10, -1, -5}) {
        std::int32_t status = 0;
        std::array<char16_t, 4> buffer = {u'a', u'b', u'c', 0};
        const std::int32_t result = udatpg_getSkeleton_nid_postfix(nullptr, nullptr, length, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status);
        Require(result == 0);
        Require(status == IllegalArgument);
        Require(buffer[0] == u'a' && buffer[1] == u'b' && buffer[2] == u'c' && buffer[3] == 0);
    }

    for (std::int32_t warning : {-1, -5, NotTerminated}) {
        std::int32_t status = warning;
        std::array<char16_t, 4> buffer = {u'a', u'b', u'c', 0};
        const std::int32_t result = udatpg_getSkeleton_nid_postfix(nullptr, nullptr, 3, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status);
        Require(result == 0);
        Require(status == IllegalArgument);
        Require(buffer[0] == u'a' && buffer[1] == u'b' && buffer[2] == u'c' && buffer[3] == 0);
    }
}

} // namespace

void Run() {
    PreexistingError();
    NullPatternNonzeroLength();
}

} // namespace UdatpgGetSkeleton

namespace UdatpgOpen {

using IcuRecoveredTest::Require;

static_assert(std::is_same_v<decltype(&udatpg_open_nid_postfix), UDateTimePatternGenerator* (APS5_VABI*)(const char*, std::int32_t*)>);

namespace {

void TestPreexistingFailure() {
    for (const std::int32_t error : {1, 2, 7, 15, 1000, INT32_MAX}) {
        std::int32_t status = error;
        Require(udatpg_open_nid_postfix(nullptr, &status) == nullptr);
        Require(status == error);

        status = error;
        Require(udatpg_open_nid_postfix("", &status) == nullptr);
        Require(status == error);

        status = error;
        Require(udatpg_open_nid_postfix("en_US", &status) == nullptr);
        Require(status == error);

        status = error;
        Require(udatpg_open_nid_postfix("root", &status) == nullptr);
        Require(status == error);

        status = error;
        Require(udatpg_open_nid_postfix("de_DE@calendar=buddhist", &status) == nullptr);
        Require(status == error);
    }
}

}

void Run() {
    TestPreexistingFailure();
}

} // namespace UdatpgOpen

void CheckIcuDateTimePatternGenerator() {
    UdatpgClose::Run();
    UdatpgGetBestPattern::Run();
    UdatpgGetBestPatternWithOptions::Run();
    UdatpgGetFieldDisplayName::Run();
    UdatpgGetSkeleton::Run();
    UdatpgOpen::Run();
}
