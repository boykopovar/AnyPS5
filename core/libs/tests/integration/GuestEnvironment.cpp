#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>

extern "C" {
char* APS5_VABI getenv_nid_postfix(const char*);
int APS5_VABI setenv_nid_postfix(const char*, const char*, int);
int APS5_VABI unsetenv_nid_postfix(const char*);
int APS5_VABI putenv_nid_postfix(char*);
int* APS5_VABI __error_nid_postfix();
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr const char* key = "ANYPS5_GUEST_ENV_TEST_4C27";
constexpr const char* lowercaseKey = "anyps5_guest_env_test_4c27";
constexpr const char* inherited = "inherited";
constexpr int einval = 22;

bool SetHostVariable() {
#ifdef _WIN32
    return _putenv_s(key, inherited) == 0;
#else
    return ::setenv(key, inherited, 1) == 0;
#endif
}

class GuestVariableFixture {
public:
    GuestVariableFixture() = default;

    ~GuestVariableFixture() {
        setenv_nid_postfix(key, inherited, 1);
        unsetenv_nid_postfix(lowercaseKey);
    }

    GuestVariableFixture(const GuestVariableFixture&) = delete;
    GuestVariableFixture& operator=(const GuestVariableFixture&) = delete;

    char* PutBorrowed() {
        std::memcpy(borrowed.data(), "ANYPS5_GUEST_ENV_TEST_4C27=one", sizeof("ANYPS5_GUEST_ENV_TEST_4C27=one"));
        RequireEqual(putenv_nid_postfix(borrowed.data()), 0, "putenv");
        return std::strchr(borrowed.data(), '=') + 1;
    }

private:
    std::array<char, 64> borrowed{};
};

std::string_view GuestValue(const char* name) {
    const char* value = getenv_nid_postfix(name);
    Require(value != nullptr, std::string("guest variable is set: ") + name);
    return value;
}

const Case inheritedValue{"Getenv_VariableSetInHostBeforeFirstUse_ReturnsHostValue", [] {
    const GuestVariableFixture fixture;
    RequireEqual(GuestValue(key), std::string_view(inherited), "inherited value");
}};

const Case noOverwrite{"Setenv_ExistingVariableWithoutOverwrite_KeepsValue", [] {
    const GuestVariableFixture fixture;
    RequireEqual(setenv_nid_postfix(key, "kept out", 0), 0, "setenv");
    RequireEqual(GuestValue(key), std::string_view(inherited), "value");
}};

const Case copiesValue{"Setenv_Overwrite_StoresCopyOfValue", [] {
    const GuestVariableFixture fixture;
    char value[] = "copied";
    RequireEqual(setenv_nid_postfix(key, value, 1), 0, "setenv");
    value[0] = 'X';
    RequireEqual(GuestValue(key), std::string_view("copied"), "value");
}};

const Case hostUntouched{"Setenv_Overwrite_LeavesHostEnvironmentUnchanged", [] {
    const GuestVariableFixture fixture;
    RequireEqual(setenv_nid_postfix(key, "copied", 1), 0, "setenv");
    const char* host = std::getenv(key);
    Require(host != nullptr, "host variable is set");
    RequireEqual(std::string_view(host), std::string_view(inherited), "host value");
}};

const Case emptyValue{"Setenv_EmptyValue_StoresEmptyString", [] {
    const GuestVariableFixture fixture;
    RequireEqual(setenv_nid_postfix(key, "", 1), 0, "setenv");
    RequireEqual(GuestValue(key), std::string_view(""), "value");
}};

const Case putenvBorrows{"Putenv_String_ReturnsPointerIntoCallerString", [] {
    GuestVariableFixture fixture;
    const char* position = fixture.PutBorrowed();
    Require(getenv_nid_postfix(key) == position, "getenv points into the putenv string");
}};

const Case putenvAliases{"Putenv_CallerModifiesString_ChangeIsVisible", [] {
    GuestVariableFixture fixture;
    char* position = fixture.PutBorrowed();
    std::memcpy(position, "two", 3);
    RequireEqual(GuestValue(key), std::string_view("two"), "value");
}};

const Case caseSensitive{"Setenv_LowercaseName_CreatesSeparateVariable", [] {
    GuestVariableFixture fixture;
    char* position = fixture.PutBorrowed();
    std::memcpy(position, "two", 3);
    RequireEqual(setenv_nid_postfix(lowercaseKey, "lower", 1), 0, "setenv lowercase");
    RequireEqual(GuestValue(key), std::string_view("two"), "uppercase value");
    RequireEqual(unsetenv_nid_postfix(lowercaseKey), 0, "unsetenv lowercase");
}};

const Case nameWithEquals{"Setenv_NameContainingEquals_FailsWithEinval", [] {
    const GuestVariableFixture fixture;
    RequireEqual(setenv_nid_postfix("bad=name", "x", 1), -1, "setenv");
    RequireEqual(*__error_nid_postfix(), einval, "errno");
}};

const Case unsetEmpty{"Unsetenv_EmptyName_Fails", [] {
    const GuestVariableFixture fixture;
    RequireEqual(unsetenv_nid_postfix(""), -1, "unsetenv");
}};

const Case putenvNoSeparator{"Putenv_StringWithoutSeparator_Fails", [] {
    const GuestVariableFixture fixture;
    char invalid[] = "no-separator";
    RequireEqual(putenv_nid_postfix(invalid), -1, "putenv");
}};

const Case unsetExisting{"Unsetenv_ExistingVariable_RemovesIt", [] {
    const GuestVariableFixture fixture;
    RequireEqual(unsetenv_nid_postfix(key), 0, "unsetenv");
    Require(getenv_nid_postfix(key) == nullptr, "variable removed");
}};

const Case unsetMissing{"Unsetenv_MissingVariable_Succeeds", [] {
    const GuestVariableFixture fixture;
    RequireEqual(unsetenv_nid_postfix(key), 0, "first unsetenv");
    RequireEqual(unsetenv_nid_postfix(key), 0, "second unsetenv");
}};

} // namespace

int main(int argc, char** argv) {
    if (!SetHostVariable()) {
        std::cerr << "cannot set the host variable " << key << '\n';
        return 1;
    }
    return Testing::Run(argc, argv);
}
