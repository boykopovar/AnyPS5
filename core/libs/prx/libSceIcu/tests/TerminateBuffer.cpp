#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>

namespace {

constexpr std::int32_t NotTerminated = -124;
constexpr std::int32_t Overflow = 15;
constexpr std::int32_t Minimum = std::numeric_limits<std::int32_t>::min();
constexpr std::int32_t Maximum = std::numeric_limits<std::int32_t>::max();

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU termination check failed at line %u\n", location.line());
    std::abort();
}

enum class Outcome { Write, Warning, Failure, Unchanged };

struct Case {
    std::int32_t capacity;
    std::int32_t length;
    Outcome outcome;
};

constexpr Case cases[] {
    {4, 0, Outcome::Write}, {4, 1, Outcome::Write}, {4, 3, Outcome::Write},
    {1, 0, Outcome::Write}, {Maximum, 3, Outcome::Write},
    {0, 0, Outcome::Warning}, {3, 3, Outcome::Warning}, {Maximum, Maximum, Outcome::Warning},
    {0, 1, Outcome::Failure}, {2, 3, Outcome::Failure}, {-1, 0, Outcome::Failure},
    {Minimum, 0, Outcome::Failure}, {0, Maximum, Outcome::Failure},
    {-1, -1, Outcome::Unchanged}, {0, -1, Outcome::Unchanged},
    {Maximum, Minimum, Outcome::Unchanged}
};

constexpr std::int32_t statuses[] {0, NotTerminated, -128, -1, Minimum, 1, Overflow, Maximum};

std::int32_t Expected(std::int32_t status, Outcome outcome) {
    if (status > 0) return status;
    switch (outcome) {
    case Outcome::Write: return status == NotTerminated ? 0 : status;
    case Outcome::Warning: return NotTerminated;
    case Outcome::Failure: return Overflow;
    case Outcome::Unchanged: return status;
    }
    std::abort();
}

template<typename T, auto TTerminate>
void Check() {
    for (const auto& test : cases) {
        for (const auto initial : statuses) {
            std::array<T, 12> buffer;
            for (std::size_t index = 0; index < buffer.size(); ++index)
                buffer[index] = static_cast<T>(0x41 + index);
            auto expected = buffer;
            if (initial <= 0 && test.outcome == Outcome::Write)
                expected[4 + static_cast<std::size_t>(test.length)] = 0;
            struct { std::int32_t before, value, after; } status {0x12345678, initial, 0x23456789};
            Require(TTerminate(buffer.data() + 4, test.capacity, test.length, &status.value) == test.length);
            Require(buffer == expected);
            Require(status.value == Expected(initial, test.outcome));
            Require(status.before == 0x12345678 && status.after == 0x23456789);

            const auto before = buffer;
            Require(TTerminate(buffer.data() + 4, test.capacity, test.length, nullptr) == test.length);
            Require(buffer == before);

            status.value = initial;
            if (initial <= 0 && test.outcome == Outcome::Write) {
                try {
                    TTerminate(nullptr, test.capacity, test.length, &status.value);
                    Require(false);
                } catch (const std::invalid_argument&) {
                    Require(status.value == initial);
                }
            } else {
                Require(TTerminate(nullptr, test.capacity, test.length, &status.value) == test.length);
                Require(status.value == Expected(initial, test.outcome));
            }
            Require(status.before == 0x12345678 && status.after == 0x23456789);
        }
        Require(TTerminate(nullptr, test.capacity, test.length, nullptr) == test.length);
    }
}

}

int main() {
    Check<char, u_terminateChars_nid_postfix>();
    Check<std::int32_t, u_terminateUChar32s_nid_postfix>();
    return 0;
}
