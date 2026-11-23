#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

using NKWebKit::JscAbi::ConfigFile;
using NKWebKit::JscAbi::ConfigFilePathMax;

namespace {

const auto Construct = &_ZN3JSC10ConfigFileC1EPKc;
const auto ConstructComplete = &_ZN3JSC10ConfigFileC2EPKc;
const auto Parse = &_ZN3JSC10ConfigFile5parseEv;
const auto SetProcessName = &_ZN3JSC10ConfigFile14setProcessNameEPKc;
const auto SetParentProcessName = &_ZN3JSC10ConfigFile20setParentProcessNameEPKc;

constexpr const char* guestRoot = "/nkwebkit-jscconfig";

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "libSceNKWebKit ConfigFile check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

void* APS5_VABI TestAllocate(std::size_t size) { return std::malloc(size ? size : 1); }
void APS5_VABI TestFree(void* pointer) { std::free(pointer); }
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { return std::calloc(count ? count : 1, size ? size : 1); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size ? size : 1); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return TestAllocate(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return TestReallocate(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t alignment, std::size_t size) {
    *pointer = TestAlign(alignment, size);
    return *pointer ? 0 : 12;
}

template <typename Action>
bool Throws(const Action& action, const std::type_info& type) {
    try {
        action();
    } catch (const std::exception& error) {
        return typeid(error) == type;
    }
    return false;
}

template <typename Action>
bool ThrowsInvalidArgument(const Action& action) {
    return Throws(action, typeid(std::invalid_argument));
}

template <typename Action>
bool ThrowsLogicError(const Action& action) {
    return Throws(action, typeid(std::logic_error));
}

std::string GuestPath(const std::string& name) {
    return std::string(guestRoot) + "/" + name;
}

void WriteFile(const std::filesystem::path& root, const std::string& name, const std::string& content) {
    std::ofstream file(root / name, std::ios::binary);
    file << content;
    Require(static_cast<bool>(file));
}

ConfigFile EmptyObject() {
    ConfigFile configFile;
    std::memset(&configFile, 0xAA, sizeof configFile);
    return configFile;
}

ConfigFile Constructed(const std::string& guestName) {
    ConfigFile configFile = EmptyObject();
    Construct(&configFile, guestName.c_str());
    return configFile;
}

std::string Field(const char* text) {
    return std::string(text);
}

void TestConstruction() {
    const ConfigFile named = Constructed("/nkwebkit-jscconfig/first.txt");
    Require(Field(named.filename) == "/nkwebkit-jscconfig/first.txt");
    Require(named.configDirectory[0] == '\0');
    Require(named.filename[ConfigFilePathMax] == '\0');

    ConfigFile complete = EmptyObject();
    ConstructComplete(&complete, "/nkwebkit-jscconfig/second.txt");
    Require(Field(complete.filename) == "/nkwebkit-jscconfig/second.txt");
    Require(complete.configDirectory[0] == '\0');

    ConfigFile absent = EmptyObject();
    Construct(&absent, nullptr);
    Require(absent.filename[0] == '\0');
    Require(absent.configDirectory[0] == '\0');
    Require(absent.filename[ConfigFilePathMax] == '\0');

    ConfigFile empty = EmptyObject();
    Construct(&empty, "");
    Require(empty.filename[0] == '\0');
    Require(empty.configDirectory[0] == '\0');

    const std::string longName(1200, 'f');
    const ConfigFile truncated = Constructed(longName);
    Require(std::strlen(truncated.filename) == ConfigFilePathMax);
    Require(std::strncmp(truncated.filename, longName.c_str(), ConfigFilePathMax) == 0);
}

void TestRefusals() {
    Require(ThrowsInvalidArgument([] { Construct(nullptr, "/nkwebkit-jscconfig/first.txt"); }));
    Require(ThrowsInvalidArgument([] { ConstructComplete(nullptr, "/nkwebkit-jscconfig/first.txt"); }));
    Require(ThrowsInvalidArgument([] { Parse(nullptr); }));
    Require(ThrowsInvalidArgument([] { SetProcessName(nullptr); }));
    Require(ThrowsInvalidArgument([] { SetParentProcessName(nullptr); }));

    ConfigFile unterminated;
    std::memset(&unterminated, 0xFF, sizeof unterminated);
    Require(ThrowsInvalidArgument([&] { Parse(&unterminated); }));
}

void TestMissingFile() {
    ConfigFile configFile = Constructed(GuestPath("absent.txt"));
    Parse(&configFile);
    Require(Field(configFile.filename) == GuestPath("absent.txt"));
    Require(Field(configFile.configDirectory) == guestRoot);

    ConfigFile empty = EmptyObject();
    Construct(&empty, "");
    Parse(&empty);
    Require(empty.filename[0] == '\0');
    Require(empty.configDirectory[0] == '\0');
}

void TestProcessNamePredicates(const std::filesystem::path& root) {
    SetProcessName("gamma");
    SetParentProcessName("ps5");

    WriteFile(root, "match-process.txt", "# a comment\n\n   \n  processName == \"gamma\" { }\n");
    ConfigFile matching = Constructed(GuestPath("match-process.txt"));
    Parse(&matching);

    WriteFile(root, "skip-process.txt",
        "processName == \"delta\" {\n"
        "  jscOptions {\n"
        "    useJIT = true\n"
        "  }\n"
        "}\n");
    ConfigFile skipped = Constructed(GuestPath("skip-process.txt"));
    Parse(&skipped);

    WriteFile(root, "apply-process.txt",
        "processName == \"gamma\" {\n"
        "  jscOptions {\n"
        "    useJIT = true\n"
        "  }\n"
        "}\n");
    ConfigFile applied = Constructed(GuestPath("apply-process.txt"));
    Require(ThrowsLogicError([&] { Parse(&applied); }));

    WriteFile(root, "match-chain.txt",
        "parentProcessName == \"ps5\" && processName == \"gamma\" && build == \"Release\" { }\n"
        "parentProcessName == \"ps5\" && processName == \"delta\" { jscOptions { useJIT = true } }\n"
        "build == \"Debug\" { jscOptions { useJIT = true } }\n");
    ConfigFile chain = Constructed(GuestPath("match-chain.txt"));
    Parse(&chain);

    WriteFile(root, "trailing-comment.txt", "processName == \"gamma\" { } # trailing comment\n");
    ConfigFile trailing = Constructed(GuestPath("trailing-comment.txt"));
    Parse(&trailing);

    WriteFile(root, "two-blocks.txt", "processName == \"gamma\" { }\nprocessName == \"delta\" { }\n");
    ConfigFile blocks = Constructed(GuestPath("two-blocks.txt"));
    Parse(&blocks);
}

void TestPredicateChains(const std::filesystem::path& root) {
    SetProcessName("gamma");
    SetParentProcessName("ps5");

    WriteFile(root, "false-first-chain.txt",
        "processName == \"delta\" && build == \"Release\" { logFile = \"out.log\" }\n"
        "parentProcessName == \"ps5\" && processName == \"gamma\" && build == \"Release\" {\n"
        "  jscOptions { useJIT = true }\n"
        "}\n");
    ConfigFile falseFirst = Constructed(GuestPath("false-first-chain.txt"));
    bool appliedOptions = false;
    try {
        Parse(&falseFirst);
    } catch (const std::logic_error& error) {
        appliedOptions = std::strcmp(error.what(), "JSC::ConfigFile::parse: JSC::Options::setOptions is not implemented") == 0;
    }
    Require(appliedOptions);

    WriteFile(root, "malformed-chain.txt",
        "processName == \"delta\" && build == parentProcessName == \"ps5\" { }\n"
        "jscOptions { useJIT = true }\n");
    ConfigFile malformed = Constructed(GuestPath("malformed-chain.txt"));
    Require(!ThrowsLogicError([&] { Parse(&malformed); }));
}

void TestRegexPredicates(const std::filesystem::path& root) {
    SetProcessName("gamma");

    WriteFile(root, "regex-match.txt", "processName =~ /^gam/ { }\n");
    ConfigFile matching = Constructed(GuestPath("regex-match.txt"));
    Parse(&matching);

    WriteFile(root, "regex-ignore-case.txt", "processName =~ /^GAM/i { jscOptions { useJIT = true } }\n");
    ConfigFile ignoringCase = Constructed(GuestPath("regex-ignore-case.txt"));
    Require(ThrowsLogicError([&] { Parse(&ignoringCase); }));

    WriteFile(root, "regex-mismatch.txt", "processName =~ /^GAM/ { jscOptions { useJIT = true } }\n");
    ConfigFile caseSensitive = Constructed(GuestPath("regex-mismatch.txt"));
    Parse(&caseSensitive);

    WriteFile(root, "regex-invalid.txt", "processName =~ /(/ { jscOptions { useJIT = true } }\n");
    ConfigFile invalid = Constructed(GuestPath("regex-invalid.txt"));
    Parse(&invalid);
}

void TestOptions(const std::filesystem::path& root) {
    SetProcessName("gamma");

    WriteFile(root, "options.txt", "jscOptions {\n  useJIT = true\n  maxPerThreadStackUsage = 1\n}\n");
    ConfigFile options = Constructed(GuestPath("options.txt"));
    Require(ThrowsLogicError([&] { Parse(&options); }));

    WriteFile(root, "options-spacing.txt", "jscOptions {a=b\n  c = d }\n");
    ConfigFile spacing = Constructed(GuestPath("options-spacing.txt"));
    Require(ThrowsLogicError([&] { Parse(&spacing); }));

    WriteFile(root, "options-missing-value.txt", "jscOptions { useJIT }\n");
    ConfigFile missingValue = Constructed(GuestPath("options-missing-value.txt"));
    Parse(&missingValue);

    WriteFile(root, "options-empty-value.txt", "jscOptions { useJIT = }\n");
    ConfigFile emptyValue = Constructed(GuestPath("options-empty-value.txt"));
    Parse(&emptyValue);
}

void TestLogFile(const std::filesystem::path& root) {
    SetProcessName("gamma");

    WriteFile(root, "log-relative.txt", "logFile = \"out.log\"\n");
    ConfigFile relative = Constructed(GuestPath("log-relative.txt"));
    Require(ThrowsLogicError([&] { Parse(&relative); }));

    WriteFile(root, "log-absolute.txt", "logFile = \"/nkwebkit-jscconfig/out.log\"\n");
    ConfigFile absolute = Constructed(GuestPath("log-absolute.txt"));
    Require(ThrowsLogicError([&] { Parse(&absolute); }));

    WriteFile(root, "log-skipped.txt", "processName == \"delta\" { logFile = \"out.log\" }\n");
    ConfigFile skipped = Constructed(GuestPath("log-skipped.txt"));
    Parse(&skipped);

    WriteFile(root, "log-unquoted.txt", "logFile = out.log\n");
    ConfigFile unquoted = Constructed(GuestPath("log-unquoted.txt"));
    Parse(&unquoted);
}

void TestMalformedStatements(const std::filesystem::path& root) {
    WriteFile(root, "unknown-statement.txt", "nonsense\n");
    ConfigFile unknown = Constructed(GuestPath("unknown-statement.txt"));
    Parse(&unknown);

    WriteFile(root, "bare-block.txt", "{ }\n");
    ConfigFile bare = Constructed(GuestPath("bare-block.txt"));
    Parse(&bare);

    WriteFile(root, "unterminated-block.txt", "processName == \"gamma\" {\n");
    ConfigFile unterminated = Constructed(GuestPath("unterminated-block.txt"));
    Parse(&unterminated);

    WriteFile(root, "unknown-predicate.txt", "build == \"Release\" && nonsense == \"x\" { }\n");
    ConfigFile predicate = Constructed(GuestPath("unknown-predicate.txt"));
    Parse(&predicate);
}

void TestProcessNameStorage(const std::filesystem::path& root) {
    SetProcessName(std::string(200, 'a').c_str());
    SetProcessName("ab");
    WriteFile(root, "padded-name.txt", "processName == \"ab\" { }\nprocessName == \"" + std::string(200, 'a') + "\" { jscOptions { useJIT = true } }\n");
    ConfigFile configFile = Constructed(GuestPath("padded-name.txt"));
    Parse(&configFile);

    SetParentProcessName("first");
    SetParentProcessName("second");
    WriteFile(root, "parent-name.txt", "parentProcessName == \"second\" { }\nparentProcessName == \"first\" { jscOptions { useJIT = true } }\n");
    ConfigFile parent = Constructed(GuestPath("parent-name.txt"));
    Parse(&parent);
}

void TestCanonicalPaths(const std::filesystem::path& root) {
    const std::string name = "relative-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt";
    WriteFile(root, name, "processName == \"gamma\" { }\n");
    const auto hostDirectory = std::filesystem::current_path();
    char saved[ConfigFilePathMax + 1] = {0};
    Require(getcwd_nid_postfix(saved, sizeof saved) != nullptr);
    Require(chdir_nid_postfix(guestRoot) == 0);
    Require(std::filesystem::current_path() == hostDirectory);

    ConfigFile configFile = EmptyObject();
    Construct(&configFile, name.c_str());
    Parse(&configFile);

    char directory[ConfigFilePathMax + 1] = {0};
    Require(getcwd_nid_postfix(directory, sizeof directory) != nullptr);
    Require(Field(configFile.filename) == Field(directory) + "/" + name);
    Require(Field(configFile.configDirectory) == Field(directory));

    Require(chdir_nid_postfix(saved) == 0);
    char restored[ConfigFilePathMax + 1] = {0};
    Require(getcwd_nid_postfix(restored, sizeof restored) != nullptr);
    Require(Field(restored) == Field(saved));
    Require(std::filesystem::current_path() == hostDirectory);
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());

    const auto root = std::filesystem::current_path() / ("nkwebkit-jscconfig-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    AddPathAlias_nid_no_patch(guestRoot, reinterpret_cast<const char*>(root.u8string().c_str()));

    TestConstruction();
    TestRefusals();
    TestMissingFile();
    TestProcessNamePredicates(root);
    TestPredicateChains(root);
    TestRegexPredicates(root);
    TestOptions(root);
    TestLogFile(root);
    TestMalformedStatements(root);
    TestProcessNameStorage(root);
    TestCanonicalPaths(root);

    RemovePathAlias_nid_no_patch(guestRoot);
    std::filesystem::remove_all(root);
    std::puts("libSceNKWebKit ConfigFile tests passed");
    return 0;
}
