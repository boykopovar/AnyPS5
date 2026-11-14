#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <type_traits>

namespace UdatClose {

using IcuRecoveredTest::Require;

static_assert(std::is_same_v<decltype(&udat_close_nid_postfix), void (APS5_VABI*)(UDateFormat*)>);

namespace {

void RequireNonNullThrows(UDateFormat* format) {
    try {
        udat_close_nid_postfix(format);
    } catch (const std::runtime_error&) {
        return;
    }
    Require(false, "non-null UDateFormat handle did not throw");
}

}

void Run() {
    udat_close_nid_postfix(nullptr);
    UDateFormat* nullFormat = nullptr;
    udat_close_nid_postfix(nullFormat);

    alignas(8) std::array<std::uint8_t, 64> storage{};
    storage.fill(0x5a);
    const auto expected = storage;
    RequireNonNullThrows(reinterpret_cast<UDateFormat*>(storage.data()));
    Require(storage == expected, "non-null UDateFormat handle was modified before the throw");
    RequireNonNullThrows(reinterpret_cast<UDateFormat*>(static_cast<std::uintptr_t>(1)));
}

} // namespace UdatClose

namespace UdatFormat {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&udat_format_nid_postfix),
                             std::int32_t (APS5_VABI*)(const UDateFormat*, double, char16_t*, std::int32_t, UFieldPosition*, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;
constexpr char16_t Fill = 0xa55a;

void FailureStatus() {
    std::array<char16_t, 4> buffer;
    buffer.fill(Fill);
    auto* format = reinterpret_cast<const UDateFormat*>(&buffer);
    auto* position = reinterpret_cast<UFieldPosition*>(&buffer);

    for (std::int32_t incoming : {1, 2, 7, 10, 15, 16, std::numeric_limits<std::int32_t>::max()}) {
        std::int32_t status = incoming;
        Require(udat_format_nid_postfix(nullptr, 0.0, nullptr, 0, nullptr, &status) == -1);
        Require(status == incoming);

        status = incoming;
        Require(udat_format_nid_postfix(format, 1700000000000.0, buffer.data(), 4, position, &status) == -1);
        Require(status == incoming);

        status = incoming;
        Require(udat_format_nid_postfix(nullptr, -1.0, nullptr, 1, position, &status) == -1);
        Require(status == incoming);

        status = incoming;
        Require(udat_format_nid_postfix(format, 0.0, buffer.data(), -1, nullptr, &status) == -1);
        Require(status == incoming);
    }
    for (char16_t unit : buffer) Require(unit == Fill);
}

void InvalidBufferArguments() {
    std::array<char16_t, 4> buffer;
    buffer.fill(Fill);
    auto* format = reinterpret_cast<const UDateFormat*>(&buffer);
    auto* position = reinterpret_cast<UFieldPosition*>(&buffer);

    for (std::int32_t incoming : {0, -1, -124, -128, std::numeric_limits<std::int32_t>::min()}) {
        for (std::int32_t badLength : {1, 2, 64, -1, -5, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()}) {
            std::int32_t status = incoming;
            Require(udat_format_nid_postfix(nullptr, 0.0, nullptr, badLength, nullptr, &status) == -1);
            Require(status == IllegalArgument);

            status = incoming;
            Require(udat_format_nid_postfix(format, 123456.78, nullptr, badLength, position, &status) == -1);
            Require(status == IllegalArgument);
        }
        for (std::int32_t negativeLength : {-1, -2, -100, std::numeric_limits<std::int32_t>::min()}) {
            std::int32_t status = incoming;
            Require(udat_format_nid_postfix(nullptr, 0.0, buffer.data(), negativeLength, nullptr, &status) == -1);
            Require(status == IllegalArgument);

            status = incoming;
            Require(udat_format_nid_postfix(format, -98765.0, buffer.data(), negativeLength, position, &status) == -1);
            Require(status == IllegalArgument);
        }
    }
    for (char16_t unit : buffer) Require(unit == Fill);
}

void UnsupportedPathsThrow() {
    std::array<char16_t, 4> buffer;
    buffer.fill(Fill);
    auto* format = reinterpret_cast<const UDateFormat*>(&buffer);
    auto* position = reinterpret_cast<UFieldPosition*>(&buffer);

    RequireThrows([&] { udat_format_nid_postfix(nullptr, 0.0, nullptr, 0, nullptr, nullptr); });
    RequireThrows([&] { udat_format_nid_postfix(format, 0.0, buffer.data(), -1, position, nullptr); });

    for (std::int32_t incoming : {0, -1, -124}) {
        std::int32_t status = incoming;
        RequireThrows([&] { udat_format_nid_postfix(format, 0.0, nullptr, 0, nullptr, &status); });
        Require(status == incoming);

        status = incoming;
        RequireThrows([&] { udat_format_nid_postfix(format, 0.0, buffer.data(), 0, position, &status); });
        Require(status == incoming);

        status = incoming;
        RequireThrows([&] { udat_format_nid_postfix(format, 0.0, buffer.data(), 4, nullptr, &status); });
        Require(status == incoming);
    }
    for (char16_t unit : buffer) Require(unit == Fill);
}

} // namespace

void Run() {
    FailureStatus();
    InvalidBufferArguments();
    UnsupportedPathsThrow();
}

} // namespace UdatFormat

namespace UdatFormatForFields {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&udat_formatForFields_nid_postfix),
                             std::int32_t (APS5_VABI*)(const void*, double, char16_t*, std::int32_t, void*, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;
constexpr char16_t Sentinel = 0xa55a;

void FailureShortCircuit() {
    int dummyFormat = 0;
    int dummyIterator = 0;
    for (const std::int32_t initialStatus : {1, 2, 7, 15, 16, std::numeric_limits<std::int32_t>::max()}) {
        std::array<char16_t, 4> buffer{Sentinel, Sentinel, Sentinel, Sentinel};
        std::int32_t status = initialStatus;
        Require(udat_formatForFields_nid_postfix(nullptr, 0.0, nullptr, 0, nullptr, &status) == -1);
        Require(status == initialStatus);

        status = initialStatus;
        Require(udat_formatForFields_nid_postfix(&dummyFormat, 1700000000000.0, nullptr, 5, &dummyIterator, &status) == -1);
        Require(status == initialStatus);

        status = initialStatus;
        Require(udat_formatForFields_nid_postfix(&dummyFormat, -12345.5, buffer.data(), -3, &dummyIterator, &status) == -1);
        Require(status == initialStatus);
        for (const char16_t unit : buffer) Require(unit == Sentinel);
    }
}

void IllegalBufferArguments() {
    int dummyFormat = 0;
    int dummyIterator = 0;
    for (const std::int32_t initialStatus : {0, -128, -124, -1, std::numeric_limits<std::int32_t>::min()}) {
        for (const std::int32_t length : {-100, -1, 1, 8, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()}) {
            std::int32_t status = initialStatus;
            Require(udat_formatForFields_nid_postfix(nullptr, 0.0, nullptr, length, nullptr, &status) == -1);
            Require(status == IllegalArgument);

            status = initialStatus;
            Require(udat_formatForFields_nid_postfix(&dummyFormat, 987654.25, nullptr, length, &dummyIterator, &status) == -1);
            Require(status == IllegalArgument);
        }

        for (const std::int32_t length : {-1, -8, std::numeric_limits<std::int32_t>::min()}) {
            std::array<char16_t, 4> buffer{Sentinel, Sentinel, Sentinel, Sentinel};
            std::int32_t status = initialStatus;
            Require(udat_formatForFields_nid_postfix(nullptr, 0.0, buffer.data(), length, nullptr, &status) == -1);
            Require(status == IllegalArgument);
            for (const char16_t unit : buffer) Require(unit == Sentinel);

            status = initialStatus;
            Require(udat_formatForFields_nid_postfix(&dummyFormat, -42.0, buffer.data(), length, &dummyIterator, &status) == -1);
            Require(status == IllegalArgument);
            for (const char16_t unit : buffer) Require(unit == Sentinel);
        }
    }
}

void NullStatusThrows() {
    std::array<char16_t, 2> buffer{Sentinel, Sentinel};
    RequireThrows([&] { udat_formatForFields_nid_postfix(nullptr, 0.0, nullptr, 0, nullptr, nullptr); });
    RequireThrows([&] { udat_formatForFields_nid_postfix(nullptr, 0.0, buffer.data(), -1, nullptr, nullptr); });
    Require(buffer[0] == Sentinel && buffer[1] == Sentinel);
}

} // namespace

void Run() {
    FailureShortCircuit();
    IllegalBufferArguments();
    NullStatusThrows();
}

} // namespace UdatFormatForFields

namespace UdatGetSymbols {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&udat_getSymbols_nid_postfix),
                             std::int32_t (APS5_VABI*)(const void*, std::int32_t, std::int32_t, char16_t*, std::int32_t, std::int32_t*)>);

namespace {

void NullFormatterReturnsMinusOne() {
    constexpr std::array<char16_t, 4> initial{u'A', u'B', u'C', u'D'};
    for (std::int32_t type : {0, 1, 2, 7, 27, -1, 100, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()}) {
        for (std::int32_t index : {0, 1, -1, 12, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()}) {
            for (std::int32_t length : {0, 4, -1, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()}) {
                for (std::int32_t incoming : {0, -124, -1, 1, 7, 15, 16}) {
                    auto buffer = initial;
                    auto status = incoming;
                    Require(udat_getSymbols_nid_postfix(nullptr, type, index, buffer.data(), length, &status) == -1);
                    Require(status == incoming);
                    Require(buffer == initial);

                    status = incoming;
                    Require(udat_getSymbols_nid_postfix(nullptr, type, index, nullptr, length, &status) == -1);
                    Require(status == incoming);
                }
                auto buffer = initial;
                Require(udat_getSymbols_nid_postfix(nullptr, type, index, buffer.data(), length, nullptr) == -1);
                Require(buffer == initial);
                Require(udat_getSymbols_nid_postfix(nullptr, type, index, nullptr, length, nullptr) == -1);
            }
        }
    }
}

void NonNullFormatterThrows() {
    alignas(8) std::array<std::uint8_t, 64> storage{};
    std::array<char16_t, 4> buffer{u'X', u'Y', u'Z', u'W'};
    for (std::int32_t incoming : {0, -124, 1, 15}) {
        auto status = incoming;
        RequireThrows([&] { udat_getSymbols_nid_postfix(storage.data(), 0, 0, buffer.data(), 4, &status); });
        Require(status == incoming);
        Require(buffer[0] == u'X' && buffer[1] == u'Y' && buffer[2] == u'Z' && buffer[3] == u'W');
    }
    RequireThrows([&] { udat_getSymbols_nid_postfix(storage.data(), 7, -1, nullptr, 0, nullptr); });
}

}

void Run() {
    NullFormatterReturnsMinusOne();
    NonNullFormatterThrows();
}

} // namespace UdatGetSymbols

namespace UdatOpen {

using IcuRecoveredTest::Require;

static_assert(std::is_same_v<decltype(&udat_open_nid_postfix),
                             UDateFormat* (APS5_VABI*)(std::int32_t, std::int32_t, const char*, const char16_t*, std::int32_t,
                                                       const char16_t*, std::int32_t, std::int32_t*)>);

namespace {

template<typename TException, typename TAction>
void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const TException&) {
        return;
    }
    Require(false, location);
}

void FailureShortCircuit() {
    const char* badLocale = reinterpret_cast<const char*>(std::uintptr_t{1});
    const char16_t* badTz = reinterpret_cast<const char16_t*>(std::uintptr_t{1});
    const char16_t* badPattern = reinterpret_cast<const char16_t*>(std::uintptr_t{1});
    for (std::int32_t incoming : {1, 2, 7, 15, 16, 65536, std::numeric_limits<std::int32_t>::max()}) {
        for (std::int32_t timeStyle : {-2, -1, 0, 1, 2, 3, 128}) {
            for (std::int32_t dateStyle : {-2, -1, 0, 1, 2, 3, 128}) {
                std::int32_t status = incoming;
                Require(udat_open_nid_postfix(timeStyle, dateStyle, nullptr, nullptr, 0, nullptr, 0, &status) == nullptr);
                Require(status == incoming);
                Require(udat_open_nid_postfix(timeStyle, dateStyle, badLocale, badTz, -5, badPattern, -9, &status) == nullptr);
                Require(status == incoming);
            }
        }
    }
}

void NonFailureThrows() {
    const char16_t zone[] = u"UTC";
    const char16_t pattern[] = u"yyyy-MM-dd";
    RequireThrows<std::invalid_argument>([&] {
        udat_open_nid_postfix(0, 0, "en_US", zone, 3, pattern, 10, nullptr);
    });
    for (std::int32_t incoming : {std::numeric_limits<std::int32_t>::min(), -128, -124, -1, 0}) {
        for (std::int32_t timeStyle : {-2, 0, 2}) {
            std::int32_t status = incoming;
            RequireThrows<std::exception>([&] {
                udat_open_nid_postfix(timeStyle, 0, "en_US", zone, 3, pattern, 10, &status);
            });
            Require(status == incoming);
            RequireThrows<std::exception>([&] {
                udat_open_nid_postfix(timeStyle, 0, nullptr, nullptr, -1, nullptr, -1, &status);
            });
            Require(status == incoming);
        }
    }
}

} // namespace

void Run() {
    FailureShortCircuit();
    NonFailureThrows();
}

} // namespace UdatOpen

namespace UdatToPattern {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&udat_toPattern_nid_postfix),
                             std::int32_t (APS5_VABI*)(const UDateFormat*, std::int8_t, char16_t*, std::int32_t, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t NotTerminated = -124;

void PreexistingFailure() {
    std::uintptr_t dummyFmt = 1;
    const auto* nonNullFmt = reinterpret_cast<const UDateFormat*>(&dummyFmt);
    char16_t buffer[4] = {u'a', u'b', u'c', u'd'};

    for (std::int32_t incoming : {IllegalArgument, 2, 7, 15, 16, std::numeric_limits<std::int32_t>::max()}) {
        for (std::int8_t localized : {std::int8_t{0}, std::int8_t{1}, std::int8_t{-1}, std::int8_t{42}}) {
            std::int32_t status = incoming;
            Require(udat_toPattern_nid_postfix(nullptr, localized, nullptr, 0, &status) == -1);
            Require(status == incoming);

            status = incoming;
            Require(udat_toPattern_nid_postfix(nullptr, localized, nullptr, 5, &status) == -1);
            Require(status == incoming);

            status = incoming;
            Require(udat_toPattern_nid_postfix(nullptr, localized, nullptr, -1, &status) == -1);
            Require(status == incoming);

            status = incoming;
            Require(udat_toPattern_nid_postfix(nullptr, localized, buffer, -1, &status) == -1);
            Require(status == incoming);
            Require(buffer[0] == u'a' && buffer[1] == u'b' && buffer[2] == u'c' && buffer[3] == u'd');

            status = incoming;
            Require(udat_toPattern_nid_postfix(nonNullFmt, localized, buffer, 4, &status) == -1);
            Require(status == incoming);
            Require(buffer[0] == u'a' && buffer[1] == u'b' && buffer[2] == u'c' && buffer[3] == u'd');

            status = incoming;
            Require(udat_toPattern_nid_postfix(nonNullFmt, localized, nullptr, 3, &status) == -1);
            Require(status == incoming);
        }
    }
}

void BufferArguments() {
    std::uintptr_t dummyFmt = 1;
    const auto* nonNullFmt = reinterpret_cast<const UDateFormat*>(&dummyFmt);
    char16_t buffer[4] = {u'w', u'x', u'y', u'z'};

    for (std::int32_t incoming : {0, -1, NotTerminated, -128, std::numeric_limits<std::int32_t>::min()}) {
        for (std::int8_t localized : {std::int8_t{0}, std::int8_t{1}, std::int8_t{-1}}) {
            for (std::int32_t badLength : {-1, -2, 1, 8, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()}) {
                std::int32_t status = incoming;
                Require(udat_toPattern_nid_postfix(nullptr, localized, nullptr, badLength, &status) == -1);
                Require(status == IllegalArgument);

                status = incoming;
                Require(udat_toPattern_nid_postfix(nonNullFmt, localized, nullptr, badLength, &status) == -1);
                Require(status == IllegalArgument);
            }

            for (std::int32_t negativeLength : {-1, -5, std::numeric_limits<std::int32_t>::min()}) {
                std::int32_t status = incoming;
                Require(udat_toPattern_nid_postfix(nullptr, localized, buffer, negativeLength, &status) == -1);
                Require(status == IllegalArgument);
                Require(buffer[0] == u'w' && buffer[1] == u'x' && buffer[2] == u'y' && buffer[3] == u'z');

                status = incoming;
                Require(udat_toPattern_nid_postfix(nonNullFmt, localized, buffer, negativeLength, &status) == -1);
                Require(status == IllegalArgument);
                Require(buffer[0] == u'w' && buffer[1] == u'x' && buffer[2] == u'y' && buffer[3] == u'z');
            }
        }
    }
}

void NullFormatHandle() {
    char16_t buffer[4] = {u'm', u'n', u'o', u'p'};

    for (std::int32_t incoming : {0, -1, NotTerminated, -128, std::numeric_limits<std::int32_t>::min()}) {
        for (std::int8_t localized : {std::int8_t{0}, std::int8_t{1}, std::int8_t{-1}, std::int8_t{127}}) {
            std::int32_t status = incoming;
            Require(udat_toPattern_nid_postfix(nullptr, localized, nullptr, 0, &status) == -1);
            Require(status == IllegalArgument);

            for (std::int32_t length : {0, 1, 4, 32, std::numeric_limits<std::int32_t>::max()}) {
                status = incoming;
                Require(udat_toPattern_nid_postfix(nullptr, localized, buffer, length, &status) == -1);
                Require(status == IllegalArgument);
                Require(buffer[0] == u'm' && buffer[1] == u'n' && buffer[2] == u'o' && buffer[3] == u'p');
            }
        }
    }
}

void NullStatusThrows() {
    char16_t buffer[2] = {u'p', u'q'};
    RequireThrows([&] { udat_toPattern_nid_postfix(nullptr, 0, nullptr, 0, nullptr); });
    RequireThrows([&] { udat_toPattern_nid_postfix(nullptr, 1, buffer, 2, nullptr); });
    Require(buffer[0] == u'p' && buffer[1] == u'q');
}

} // namespace

void Run() {
    PreexistingFailure();
    BufferArguments();
    NullFormatHandle();
    NullStatusThrows();
}

} // namespace UdatToPattern

void CheckIcuDateFormat() {
    UdatClose::Run();
    UdatFormat::Run();
    UdatFormatForFields::Run();
    UdatGetSymbols::Run();
    UdatOpen::Run();
    UdatToPattern::Run();
}
