#pragma once

#include <concepts>
#include <exception>
#include <filesystem>
#include <ostream>
#include <source_location>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace Testing {

using CaseBody = void (*)();

class Case {
public:
    Case(std::string_view name, CaseBody body);
};

class Failure : public std::exception {
public:
    Failure(std::string message, std::source_location location);
    const char* what() const noexcept override;
    const std::source_location& Location() const noexcept;

private:
    std::string message;
    std::source_location location;
};

class Skipped : public std::exception {
public:
    explicit Skipped(std::string reason);
    const char* what() const noexcept override;

private:
    std::string reason;
};

[[noreturn]] void Fail(std::string_view message, std::source_location location = std::source_location::current());
[[noreturn]] void Skip(std::string_view reason);

void Require(bool condition, std::string_view message, std::source_location location = std::source_location::current());

template<typename TValue>
concept Printable = requires(std::ostream& stream, const TValue& value) { stream << value; };

template<typename TValue>
std::string Describe(const TValue& value) {
    if constexpr (std::is_same_v<TValue, bool>) {
        return value ? "true" : "false";
    } else if constexpr (std::is_enum_v<TValue>) {
        return std::to_string(static_cast<std::underlying_type_t<TValue>>(value));
    } else if constexpr (std::is_integral_v<TValue> && sizeof(TValue) == 1) {
        return std::to_string(static_cast<int>(value));
    } else if constexpr (Printable<TValue>) {
        std::ostringstream stream;
        stream << value;
        return stream.str();
    } else {
        return "<value>";
    }
}

template<typename TActual, typename TExpected>
void RequireEqual(const TActual& actual, const TExpected& expected, std::string_view message,
                  std::source_location location = std::source_location::current()) {
    if (actual == expected) return;
    Fail(std::string(message) + ": expected " + Describe(expected) + ", got " + Describe(actual), location);
}

bool IsFrameworkSignal(const std::exception& error) noexcept;

template<typename TError>
constexpr bool ShadowsFrameworkSignals = !std::is_same_v<TError, Failure> && !std::is_same_v<TError, Skipped> &&
    (std::is_base_of_v<TError, Failure> || std::is_base_of_v<TError, Skipped>);

template<typename TError, typename TResult, typename TOperation, typename TExtract>
TResult CatchExpected(const TOperation& operation, const TExtract& extract, std::string_view message, std::source_location location) {
    try {
        try {
            operation();
        } catch (const TError& error) {
            if constexpr (ShadowsFrameworkSignals<TError>) {
                if (IsFrameworkSignal(error)) throw;
            }
            return extract(error);
        }
    } catch (const std::exception& error) {
        if (IsFrameworkSignal(error)) throw;
        Fail(std::string(message) + ": threw an unexpected exception: " + error.what(), location);
    }
    Fail(std::string(message) + ": did not throw", location);
}

template<typename TError, typename TOperation>
TError RequireThrows(const TOperation& operation, std::string_view message,
                     std::source_location location = std::source_location::current()) {
    return CatchExpected<TError, TError>(operation, [](const TError& error) { return error; }, message, location);
}

template<typename TError, typename TOperation>
std::string RequireThrowsMessage(const TOperation& operation, std::string_view message,
                                 std::source_location location = std::source_location::current()) {
    return CatchExpected<TError, std::string>(operation, [](const TError& error) { return std::string(error.what()); }, message, location);
}

template<typename TError, typename TOperation>
void RequireThrowsWithMessage(const TOperation& operation, std::string_view expectedMessage, std::string_view message,
                              std::source_location location = std::source_location::current()) {
    RequireEqual(RequireThrowsMessage<TError>(operation, message, location), std::string(expectedMessage), message, location);
}

template<typename TError, typename TOperation>
void RequireThrowsContaining(const TOperation& operation, std::string_view expectedText, std::string_view message,
                             std::source_location location = std::source_location::current()) {
    const auto text = RequireThrowsMessage<TError>(operation, message, location);
    if (text.find(expectedText) == std::string::npos) {
        Fail(std::string(message) + ": message \"" + text + "\" does not contain \"" + std::string(expectedText) + "\"", location);
    }
}

class TemporaryDirectory {
public:
    TemporaryDirectory();
    ~TemporaryDirectory();
    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    const std::filesystem::path& Path() const noexcept;

private:
    std::filesystem::path path;
};

const std::vector<std::string>& Arguments();
const std::string& RequireArgument(std::size_t index, std::string_view name);

int Run(int argc, char** argv);

} // namespace Testing
