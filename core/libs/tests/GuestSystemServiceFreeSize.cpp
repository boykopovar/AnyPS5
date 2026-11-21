#include "prx/libSceSystemService/include/SystemService.hpp"
#include "prx/libSceSystemService/LanguageLocale.hpp"

#include <cstdint>
#include <cerrno>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

void Require(bool value) { if (!value) std::abort(); }

void TestPosixLanguageCSentinel() {
    const auto resolveWithEnv = [](const std::unordered_map<std::string, std::string>& env, const std::vector<std::string>& expectedLocales, int expectedLanguage, bool expectNotice) {
        const auto envLookup = [&env](const char* name) -> const char* {
            const auto it = env.find(name);
            return it != env.end() ? it->second.c_str() : nullptr;
        };
        const std::vector<std::string> locales = SystemServiceLocale::PosixHostLocales(envLookup);
        Require(locales == expectedLocales);
        std::vector<std::string> notices;
        const int language = SystemServiceLocale::ResolveHostLanguage(
            [&locales]() { return locales; },
            [&notices](const std::string& message) { notices.push_back(message); }
        );
        Require(language == expectedLanguage);
        if (expectNotice) {
            Require(notices.size() == 1 && !notices.front().empty());
        } else {
            Require(notices.empty());
        }
    };

    resolveWithEnv({{"LANG", "fr_FR.UTF-8"}, {"LANGUAGE", "POSIX.UTF-8:ja"}}, {"POSIX.UTF-8"}, SystemServiceLocale::ConsoleLanguage::EnglishUs, true);
    resolveWithEnv({{"LANG", "fr_FR.UTF-8"}, {"LANGUAGE", "C@euro:ja"}}, {"C@euro"}, SystemServiceLocale::ConsoleLanguage::EnglishUs, true);
    resolveWithEnv({{"LANG", "fr_FR.UTF-8"}, {"LANGUAGE", "POSIX@euro:ja"}}, {"POSIX@euro"}, SystemServiceLocale::ConsoleLanguage::EnglishUs, true);
    resolveWithEnv({{"LANG", "fr_FR.UTF-8"}, {"LANGUAGE", "he:POSIX.UTF-8:ja"}}, {"he", "POSIX.UTF-8"}, SystemServiceLocale::ConsoleLanguage::EnglishUs, true);
    resolveWithEnv({{"LANG", "fr_FR.UTF-8"}, {"LANGUAGE", "posix.utf8:de"}}, {"posix.utf8"}, SystemServiceLocale::ConsoleLanguage::EnglishUs, true);

    resolveWithEnv({{"LANG", "fr_FR.UTF-8"}, {"LANGUAGE", "de:POSIX.UTF-8:ja"}}, {"de", "POSIX.UTF-8"}, SystemServiceLocale::ConsoleLanguage::German, false);
    resolveWithEnv({{"LANG", "fr_FR.UTF-8"}, {"LANGUAGE", ".UTF-8:ja"}}, {".UTF-8", "ja", "fr_FR.UTF-8"}, SystemServiceLocale::ConsoleLanguage::Japanese, false);
    resolveWithEnv({{"LANG", "fr_FR.UTF-8"}, {"LANGUAGE", "ca_ES:ja"}}, {"ca_ES", "ja", "fr_FR.UTF-8"}, SystemServiceLocale::ConsoleLanguage::Japanese, false);
}

}

int main() {
    std::uint64_t freeSize = 0;
    std::uint64_t totalSize = 0;
    Require(sceShellCoreUtilGetFreeSizeOfUserPartition(&freeSize, &totalSize) == 0);
    errno = 71;
    Require(sceShellCoreUtilGetFreeSizeOfUserPartition(reinterpret_cast<std::uint64_t*>(0x10), &totalSize) == static_cast<int>(0x80e30005u));
    Require(sceShellCoreUtilGetFreeSizeOfUserPartition(&freeSize, reinterpret_cast<std::uint64_t*>(0x10)) == static_cast<int>(0x80e30005u));
    Require(errno == 71);
    Require(freeSize != 0 && totalSize != 0 && freeSize <= totalSize);
    Require(sceShellCoreUtilGetFreeSizeOfAvContentsTmp() != 0);
    TestPosixLanguageCSentinel();
    return 0;
}
