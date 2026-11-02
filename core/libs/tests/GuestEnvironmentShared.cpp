#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "guest environment shared: %s\n", message);
        std::abort();
    }
}

const char* Enumerated(const char* name) {
    if (environ_nid_postfix == nullptr) return nullptr;
    const std::string_view key(name);
    for (std::size_t index = 0; index < 2048; ++index) {
        const char* entry = environ_nid_postfix[index];
        if (entry == nullptr) return nullptr;
        const char* separator = std::strchr(entry, '=');
        if (separator != nullptr && std::string_view(entry, separator - entry) == key)
            return separator + 1;
    }
    Require(false, "environment has no null terminator");
    return nullptr;
}

std::size_t Count(const char* name) {
    if (environ_nid_postfix == nullptr) return 0;
    const std::string_view key(name);
    std::size_t count = 0;
    for (std::size_t index = 0; index < 2048; ++index) {
        const char* entry = environ_nid_postfix[index];
        if (entry == nullptr) return count;
        const char* separator = std::strchr(entry, '=');
        if (separator != nullptr && std::string_view(entry, separator - entry) == key)
            ++count;
    }
    Require(false, "environment has no null terminator");
    return 0;
}

void CheckValue(const char* name, const char* expected) {
    const char* value = getenv_nid_postfix(name);
    Require(value != nullptr && std::strcmp(value, expected) == 0, "getenv value mismatch");
    const char* listed = Enumerated(name);
    Require(listed != nullptr && std::strcmp(listed, expected) == 0, "environ value mismatch");
    std::string copy;
    Require(CopyGuestEnvironment_nid_no_patch(name, copy) && copy == expected, "copied value mismatch");
}

void CheckAbsent(const char* name) {
    Require(getenv_nid_postfix(name) == nullptr, "getenv retained an absent variable");
    Require(Enumerated(name) == nullptr, "environ retained an absent variable");
    std::string copy = "unchanged";
    Require(!CopyGuestEnvironment_nid_no_patch(name, copy) && copy == "unchanged", "copy retained an absent variable");
}

void Clear() {
    environ_nid_postfix = nullptr;
    CheckAbsent("ANYPS5_SHARED_ENV_CLEAR_63C9");
}

void PublishedChanges() {
    const char* key = "ANYPS5_SHARED_ENV_63C9";
    Require(setenv_nid_postfix(key, "initial", 1) == 0, "initial setenv failed");
    const char* listed = Enumerated(key);
    Require(listed != nullptr && std::strcmp(listed, "initial") == 0, "setenv did not publish its variable through environ");
    CheckValue(key, "initial");
    Require(Count(key) == 1, "initial setenv published duplicate entries");
    Require(setenv_nid_postfix(key, "ignored", 0) == 0, "nonoverwriting setenv failed");
    CheckValue(key, "initial");
    Require(setenv_nid_postfix(key, "overwritten=with=equals", 1) == 0, "overwriting setenv failed");
    CheckValue(key, "overwritten=with=equals");
    Require(Count(key) == 1, "overwriting setenv published duplicate entries");
    Require(unsetenv_nid_postfix(key) == 0, "unsetenv failed");
    CheckAbsent(key);
}

void ReplacedEnvironment() {
    char first[] = "ANYPS5_SHARED_EXTERNAL_63C9=first";
    char second[] = "ANYPS5_SHARED_NEIGHBOR_63C9=neighbor";
    char* replacement[] = {first, second, nullptr};
    environ_nid_postfix = replacement;
    CheckValue("ANYPS5_SHARED_EXTERNAL_63C9", "first");
    CheckValue("ANYPS5_SHARED_NEIGHBOR_63C9", "neighbor");
    std::memcpy(std::strchr(first, '=') + 1, "other", 5);
    CheckValue("ANYPS5_SHARED_EXTERNAL_63C9", "other");
    Require(setenv_nid_postfix("ANYPS5_SHARED_EXTERNAL_63C9", "ignored", 0) == 0, "replacement overwrite zero failed");
    CheckValue("ANYPS5_SHARED_EXTERNAL_63C9", "other");
    Require(setenv_nid_postfix("ANYPS5_SHARED_EXTERNAL_63C9", "owned", 1) == 0, "replacement overwrite failed");
    CheckValue("ANYPS5_SHARED_EXTERNAL_63C9", "owned");
    CheckValue("ANYPS5_SHARED_NEIGHBOR_63C9", "neighbor");
    Require(unsetenv_nid_postfix("ANYPS5_SHARED_NEIGHBOR_63C9") == 0, "replacement removal failed");
    CheckAbsent("ANYPS5_SHARED_NEIGHBOR_63C9");
    Clear();
    CheckAbsent("ANYPS5_SHARED_EXTERNAL_63C9");
    Require(setenv_nid_postfix("ANYPS5_SHARED_AFTER_NULL_63C9", "new", 1) == 0, "setenv after null failed");
    CheckValue("ANYPS5_SHARED_AFTER_NULL_63C9", "new");
    char* empty[] = {nullptr};
    environ_nid_postfix = empty;
    CheckAbsent("ANYPS5_SHARED_AFTER_NULL_63C9");
    Require(setenv_nid_postfix("ANYPS5_SHARED_AFTER_EMPTY_63C9", "new", 1) == 0, "setenv after empty failed");
    CheckValue("ANYPS5_SHARED_AFTER_EMPTY_63C9", "new");
    Clear();
}

void OffsetEnvironment() {
    const auto nameOf = [](const char* entry) { return std::string(entry, std::strchr(entry, '=') - entry); };
    for (const char* name : {"ANYPS5_SHARED_OFFSET_A_63C9", "ANYPS5_SHARED_OFFSET_B_63C9", "ANYPS5_SHARED_OFFSET_C_63C9"})
        Require(setenv_nid_postfix(name, "kept", 1) == 0, "offset setup failed");
    Require(environ_nid_postfix != nullptr && environ_nid_postfix[0] != nullptr && environ_nid_postfix[1] != nullptr &&
        environ_nid_postfix[2] != nullptr && environ_nid_postfix[3] == nullptr, "offset setup has an unexpected array");
    const std::string firstSkipped = nameOf(environ_nid_postfix[0]);
    const std::string firstKept = nameOf(environ_nid_postfix[1]);
    const std::string secondKept = nameOf(environ_nid_postfix[2]);
    environ_nid_postfix = environ_nid_postfix + 1;
    Require(setenv_nid_postfix("ANYPS5_SHARED_OFFSET_ADDED_63C9", "kept", 1) == 0, "setenv with an offset array failed");
    CheckAbsent(firstSkipped.c_str());
    CheckValue(firstKept.c_str(), "kept");
    CheckValue(secondKept.c_str(), "kept");
    CheckValue("ANYPS5_SHARED_OFFSET_ADDED_63C9", "kept");
    Require(environ_nid_postfix != nullptr && environ_nid_postfix[0] != nullptr && environ_nid_postfix[1] != nullptr &&
        environ_nid_postfix[2] != nullptr && environ_nid_postfix[3] == nullptr, "setenv lost offset array entries");
    const std::string secondSkipped = nameOf(environ_nid_postfix[0]);
    const std::string removed = nameOf(environ_nid_postfix[1]);
    const std::string survivor = nameOf(environ_nid_postfix[2]);
    environ_nid_postfix = environ_nid_postfix + 1;
    Require(unsetenv_nid_postfix(removed.c_str()) == 0, "unsetenv with an offset array failed");
    CheckAbsent(firstSkipped.c_str());
    CheckAbsent(secondSkipped.c_str());
    CheckAbsent(removed.c_str());
    CheckValue(survivor.c_str(), "kept");
    Require(environ_nid_postfix != nullptr && environ_nid_postfix[0] != nullptr && environ_nid_postfix[1] == nullptr,
        "unsetenv lost the offset array survivor");
    Clear();
}

void ReplacedEnvironmentKeepsOwnedStrings() {
    const char* ownerName = "ANYPS5_SHARED_REPLACED_OWNER_63C9";
    const char* directName = "ANYPS5_SHARED_REPLACED_DIRECT_63C9";
    Require(setenv_nid_postfix(ownerName, "ANYPS5_SHARED_REPLACED_INNER_63C9=retained", 1) == 0, "replacement owner setup failed");
    Require(setenv_nid_postfix(directName, "direct", 1) == 0, "replacement direct setup failed");
    char* const alias = getenv_nid_postfix(ownerName);
    char* const direct = getenv_nid_postfix(directName);
    Require(alias != nullptr && direct != nullptr, "replacement owned values are missing");
    char other[] = "ANYPS5_SHARED_REPLACED_OTHER_63C9=before";
    char* replacement[] = {direct - std::strlen(directName) - 1, alias, other, nullptr};
    environ_nid_postfix = replacement;
    CheckAbsent(ownerName);
    CheckValue(directName, "direct");
    CheckValue("ANYPS5_SHARED_REPLACED_INNER_63C9", "retained");
    Require(setenv_nid_postfix(ownerName, "short", 1) == 0, "replacement owner overwrite failed");
    CheckValue(ownerName, "short");
    CheckValue("ANYPS5_SHARED_REPLACED_INNER_63C9", "retained");
    Require(unsetenv_nid_postfix(ownerName) == 0, "replacement owner removal failed");
    Require(setenv_nid_postfix("ANYPS5_SHARED_REPLACED_OTHER_63C9", "after", 1) == 0, "setenv with owned replacement entries failed");
    CheckValue(directName, "direct");
    CheckValue("ANYPS5_SHARED_REPLACED_INNER_63C9", "retained");
    CheckValue("ANYPS5_SHARED_REPLACED_OTHER_63C9", "after");
    Require(unsetenv_nid_postfix("ANYPS5_SHARED_REPLACED_OTHER_63C9") == 0, "unsetenv with owned replacement entries failed");
    Require(setenv_nid_postfix("ANYPS5_SHARED_REPLACED_NEW_63C9", "allocation after replacement", 1) == 0, "post-replacement allocation failed");
    CheckAbsent(ownerName);
    CheckAbsent("ANYPS5_SHARED_REPLACED_OTHER_63C9");
    CheckValue(directName, "direct");
    CheckValue("ANYPS5_SHARED_REPLACED_INNER_63C9", "retained");
    Clear();
}

void BorrowedEnvironment() {
    char borrowed[] = "ANYPS5_SHARED_BORROW_A_63C9=one";
    Require(putenv_nid_postfix(borrowed) == 0, "putenv failed");
    Require(getenv_nid_postfix("ANYPS5_SHARED_BORROW_A_63C9") == std::strchr(borrowed, '=') + 1, "putenv copied its input");
    CheckValue("ANYPS5_SHARED_BORROW_A_63C9", "one");
    Require(environ_nid_postfix != nullptr && environ_nid_postfix[0] == borrowed, "environ lost the borrowed entry");
    std::memcpy(std::strchr(borrowed, '=') + 1, "two", 3);
    CheckValue("ANYPS5_SHARED_BORROW_A_63C9", "two");
    const auto renamed = std::string_view(borrowed).find("_A_");
    Require(renamed != std::string_view::npos, "borrowed key has no rename position");
    borrowed[renamed + 1] = 'B';
    CheckAbsent("ANYPS5_SHARED_BORROW_A_63C9");
    CheckValue("ANYPS5_SHARED_BORROW_B_63C9", "two");
    Require(unsetenv_nid_postfix("ANYPS5_SHARED_BORROW_B_63C9") == 0, "borrowed removal failed");
    CheckAbsent("ANYPS5_SHARED_BORROW_B_63C9");
    Clear();
}

void BorrowedOwnedSubstring() {
    Require(setenv_nid_postfix("ANYPS5_SHARED_OUTER_63C9", "ANYPS5_SHARED_INNER_63C9=kept", 1) == 0, "outer setenv failed");
    char* const alias = getenv_nid_postfix("ANYPS5_SHARED_OUTER_63C9");
    Require(alias != nullptr && putenv_nid_postfix(alias) == 0, "interior putenv failed");
    Require(setenv_nid_postfix("ANYPS5_SHARED_OUTER_63C9", "short", 1) == 0, "borrowed owner overwrite failed");
    CheckValue("ANYPS5_SHARED_OUTER_63C9", "short");
    CheckValue("ANYPS5_SHARED_INNER_63C9", "kept");
    Require(unsetenv_nid_postfix("ANYPS5_SHARED_OUTER_63C9") == 0, "outer unsetenv failed");
    CheckAbsent("ANYPS5_SHARED_OUTER_63C9");
    CheckValue("ANYPS5_SHARED_INNER_63C9", "kept");
    Require(environ_nid_postfix != nullptr && environ_nid_postfix[0] == alias, "interior putenv pointer was not retained");
    Require(setenv_nid_postfix("ANYPS5_SHARED_OTHER_63C9", "allocation after owner removal", 1) == 0, "post-owner setenv failed");
    CheckValue("ANYPS5_SHARED_INNER_63C9", "kept");
    Require(unsetenv_nid_postfix("ANYPS5_SHARED_INNER_63C9") == 0, "interior unsetenv failed");
    Require(setenv_nid_postfix("ANYPS5_SHARED_OUTER_63C9", "ANYPS5_SHARED_INNER_63C9=again", 1) == 0, "owner re-add failed");
    Require(putenv_nid_postfix(getenv_nid_postfix("ANYPS5_SHARED_OUTER_63C9")) == 0, "interior re-add failed");
    CheckValue("ANYPS5_SHARED_INNER_63C9", "again");
    Require(unsetenv_nid_postfix("ANYPS5_SHARED_INNER_63C9") == 0, "re-added alias removal failed");
    Require(unsetenv_nid_postfix("ANYPS5_SHARED_OUTER_63C9") == 0, "re-added owner removal failed");
    Clear();
}

void AliasedSetenvArguments() {
    const char* self = "ANYPS5_SHARED_SELF_63C9";
    Require(setenv_nid_postfix(self, self, 1) == 0, "self-named setenv failed");
    const char* name = getenv_nid_postfix(self);
    Require(name != nullptr && setenv_nid_postfix(name, "updated", 1) == 0, "setenv with aliased name failed");
    CheckValue(self, "updated");
    Require(setenv_nid_postfix(self, "prefix-kept", 1) == 0, "aliased-value setup failed");
    const char* value = getenv_nid_postfix(self);
    Require(value != nullptr && setenv_nid_postfix(self, value + 7, 1) == 0, "setenv with aliased value failed");
    CheckValue(self, "kept");
    Require(setenv_nid_postfix(self, self, 1) == 0, "combined alias setup failed");
    name = getenv_nid_postfix(self);
    Require(name != nullptr && setenv_nid_postfix(name, name, 1) == 0, "setenv with shared name and value failed");
    CheckValue(self, self);
    Clear();
}

void DuplicateEnvironment() {
    char first[] = "ANYPS5_SHARED_DUPLICATE_63C9=first";
    char second[] = "ANYPS5_SHARED_DUPLICATE_63C9=second";
    char neighbor[] = "ANYPS5_SHARED_DUP_NEIGHBOR_63C9=kept";
    char* replacement[] = {first, second, neighbor, nullptr};
    environ_nid_postfix = replacement;
    CheckValue("ANYPS5_SHARED_DUPLICATE_63C9", "first");
    Require(unsetenv_nid_postfix("ANYPS5_SHARED_DUPLICATE_63C9") == 0, "duplicate removal failed");
    CheckAbsent("ANYPS5_SHARED_DUPLICATE_63C9");
    Require(Count("ANYPS5_SHARED_DUPLICATE_63C9") == 0, "unsetenv did not remove every duplicate");
    CheckValue("ANYPS5_SHARED_DUP_NEIGHBOR_63C9", "kept");
    environ_nid_postfix = replacement;
    Require(setenv_nid_postfix("ANYPS5_SHARED_DUPLICATE_63C9", "updated", 1) == 0, "duplicate overwrite failed");
    CheckValue("ANYPS5_SHARED_DUPLICATE_63C9", "updated");
    Require(Count("ANYPS5_SHARED_DUPLICATE_63C9") == 1, "overwritten duplicates were not consolidated");
    CheckValue("ANYPS5_SHARED_DUP_NEIGHBOR_63C9", "kept");
    Clear();
}

void GrowthKeepsUnrelatedValue() {
    const char* stableName = "ANYPS5_SHARED_STABLE_63C9";
    Require(setenv_nid_postfix(stableName, "stable", 1) == 0, "stable setenv failed");
    char* const stable = getenv_nid_postfix(stableName);
    for (int index = 0; index < 256; ++index) {
        const std::string name = "ANYPS5_SHARED_GROWTH_63C9_" + std::to_string(index);
        Require(setenv_nid_postfix(name.c_str(), std::to_string(index).c_str(), 1) == 0, "growth setenv failed");
    }
    for (int index = 0; index < 256; ++index) {
        const std::string name = "ANYPS5_SHARED_GROWTH_63C9_" + std::to_string(index);
        CheckValue(name.c_str(), std::to_string(index).c_str());
        Require(Count(name.c_str()) == 1, "growth produced duplicates");
        Require(unsetenv_nid_postfix(name.c_str()) == 0, "growth unsetenv failed");
    }
    Require(getenv_nid_postfix(stableName) == stable && std::strcmp(stable, "stable") == 0, "unrelated changes invalidated getenv storage");
    CheckValue(stableName, "stable");
    Clear();
}

void HostEnvironmentIsolated() {
#ifdef _WIN32
    Require(_putenv_s("HOME", "anyps5-host-home-63c9") == 0, "host HOME setup failed");
    Require(_putenv_s("PATH", "anyps5-host-path-63c9") == 0, "host PATH setup failed");
#else
    Require(::setenv("HOME", "anyps5-host-home-63c9", 1) == 0, "host HOME setup failed");
    Require(::setenv("PATH", "anyps5-host-path-63c9", 1) == 0, "host PATH setup failed");
#endif
    CheckAbsent("HOME");
    CheckAbsent("PATH");
    Require(setenv_nid_postfix("HOME", "/guest/home", 1) == 0, "guest HOME setup failed");
    Require(setenv_nid_postfix("PATH", "/app0/bin", 1) == 0, "guest PATH setup failed");
    CheckValue("HOME", "/guest/home");
    CheckValue("PATH", "/app0/bin");
    Require(std::strcmp(std::getenv("HOME"), "anyps5-host-home-63c9") == 0, "guest changed host HOME");
    Require(std::strcmp(std::getenv("PATH"), "anyps5-host-path-63c9") == 0, "guest changed host PATH");
    Clear();
    CheckAbsent("HOME");
    CheckAbsent("PATH");
}

}

int main() {
    PublishedChanges();
    ReplacedEnvironment();
    OffsetEnvironment();
    ReplacedEnvironmentKeepsOwnedStrings();
    BorrowedEnvironment();
    BorrowedOwnedSubstring();
    AliasedSetenvArguments();
    DuplicateEnvironment();
    GrowthKeepsUnrelatedValue();
    HostEnvironmentIsolated();
    std::puts("guest environment shared checks passed");
}
