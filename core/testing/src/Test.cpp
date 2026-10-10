#include <Testing/Test.hpp>

#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

namespace Testing {
namespace {

struct RegisteredCase {
    std::string name;
    CaseBody body;
};

enum class Outcome { Passed, Failed, Skipped };

constexpr int SkipExitCode = 77;

std::vector<std::string>& PositionalArguments() {
    static std::vector<std::string> arguments;
    return arguments;
}

std::vector<RegisteredCase>& Registry() {
    static std::vector<RegisteredCase> cases;
    return cases;
}

std::string Where(const std::source_location& location) {
    return std::string(location.file_name()) + ":" + std::to_string(location.line());
}

Outcome RunCase(const RegisteredCase& testCase) {
    try {
        testCase.body();
        std::cout << "[ PASSED  ] " << testCase.name << '\n';
        return Outcome::Passed;
    } catch (const Failure& failure) {
        std::cout << "[ FAILED  ] " << testCase.name << '\n'
                  << "  " << Where(failure.Location()) << ": " << failure.what() << '\n';
    } catch (const Skipped& skipped) {
        std::cout << "[ SKIPPED ] " << testCase.name << '\n'
                  << "skipped, " << skipped.what() << '\n';
        return Outcome::Skipped;
    } catch (const std::exception& error) {
        std::cout << "[ FAILED  ] " << testCase.name << '\n'
                  << "  unhandled exception: " << error.what() << '\n';
    } catch (...) {
        std::cout << "[ FAILED  ] " << testCase.name << '\n'
                  << "  unhandled non-standard exception\n";
    }
    return Outcome::Failed;
}

int Usage(const char* program) {
    std::cerr << "usage: " << program << " [--list] [--case <name>]...\n";
    return 2;
}

} // namespace

Case::Case(std::string_view name, CaseBody body) {
    for (const auto& existing : Registry()) {
        if (existing.name == name) throw std::logic_error("duplicate test case: " + std::string(name));
    }
    Registry().push_back({std::string(name), body});
}

Failure::Failure(std::string message, std::source_location location)
    : message(std::move(message)), location(location) {}

const char* Failure::what() const noexcept {
    return message.c_str();
}

const std::source_location& Failure::Location() const noexcept {
    return location;
}

Skipped::Skipped(std::string reason) : reason(std::move(reason)) {}

const char* Skipped::what() const noexcept {
    return reason.c_str();
}

bool IsFrameworkSignal(const std::exception& error) noexcept {
    return dynamic_cast<const Failure*>(&error) != nullptr || dynamic_cast<const Skipped*>(&error) != nullptr;
}

void Fail(std::string_view message, std::source_location location) {
    throw Failure(std::string(message), location);
}

void Skip(std::string_view reason) {
    throw Skipped(std::string(reason));
}

void Require(bool condition, std::string_view message, std::source_location location) {
    if (!condition) Fail(message, location);
}

TemporaryDirectory::TemporaryDirectory() {
    static std::atomic<unsigned> counter{0};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto base = std::filesystem::temp_directory_path();
    for (int attempt = 0; attempt < 100; ++attempt) {
        auto candidate = base / ("anyps5-test-" + std::to_string(stamp) + "-" + std::to_string(counter++));
        if (std::filesystem::create_directory(candidate)) {
            path = std::move(candidate);
            return;
        }
    }
    throw std::runtime_error("cannot create a temporary test directory under " + base.string());
}

TemporaryDirectory::~TemporaryDirectory() {
    std::error_code ignored;
    std::filesystem::remove_all(path, ignored);
}

const std::filesystem::path& TemporaryDirectory::Path() const noexcept {
    return path;
}

const std::vector<std::string>& Arguments() {
    return PositionalArguments();
}

const std::string& RequireArgument(std::size_t index, std::string_view name) {
    if (index >= PositionalArguments().size()) {
        throw std::invalid_argument("missing command-line argument " + std::to_string(index + 1) + ": " + std::string(name));
    }
    return PositionalArguments()[index];
}

int Run(int argc, char** argv) {
    std::vector<std::string_view> selected;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--list") {
            for (const auto& testCase : Registry()) std::cout << testCase.name << '\n';
            return 0;
        }
        if (argument == "--case" && index + 1 < argc) {
            selected.emplace_back(argv[++index]);
            continue;
        }
        if (argument.starts_with("--")) return Usage(argv[0]);
        PositionalArguments().emplace_back(argument);
    }
    for (const auto name : selected) {
        bool known = false;
        for (const auto& testCase : Registry()) known = known || testCase.name == name;
        if (!known) {
            std::cerr << "unknown test case: " << name << '\n';
            return 2;
        }
    }
    if (Registry().empty()) {
        std::cerr << "no test cases registered\n";
        return 1;
    }
    int passed = 0;
    int failed = 0;
    int skipped = 0;
    for (const auto& testCase : Registry()) {
        bool run = selected.empty();
        for (const auto name : selected) run = run || testCase.name == name;
        if (!run) continue;
        switch (RunCase(testCase)) {
            case Outcome::Passed: ++passed; break;
            case Outcome::Failed: ++failed; break;
            case Outcome::Skipped: ++skipped; break;
        }
    }
    std::cout << passed << " passed, " << failed << " failed, " << skipped << " skipped\n";
    if (failed > 0) return 1;
    if (passed == 0 && skipped > 0) return SkipExitCode;
    return 0;
}

} // namespace Testing
