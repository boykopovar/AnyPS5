#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cctype>
#include <clocale>
#include <cstdio>
#include <string>

extern "C" {
int APS5_VABI isupper_nid_postfix(int);
int APS5_VABI islower_nid_postfix(int);
int APS5_VABI isalpha_nid_postfix(int);
int APS5_VABI isdigit_nid_postfix(int);
int APS5_VABI isalnum_nid_postfix(int);
int APS5_VABI isspace_nid_postfix(int);
int APS5_VABI isblank_nid_postfix(int);
int APS5_VABI iscntrl_nid_postfix(int);
int APS5_VABI isprint_nid_postfix(int);
int APS5_VABI isgraph_nid_postfix(int);
int APS5_VABI ispunct_nid_postfix(int);
int APS5_VABI isxdigit_nid_postfix(int);
int APS5_VABI toupper_nid_postfix(int);
int APS5_VABI tolower_nid_postfix(int);
const short* APS5_VABI _Getptolower_nid_postfix();
const short* APS5_VABI _Getptoupper_nid_postfix();
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

using GuestCharacterFunction = int (APS5_VABI *)(int);
using HostCharacterFunction = int (*)(int);

class CLocaleFixture {
public:
    CLocaleFixture() {
        const char* current = std::setlocale(LC_CTYPE, nullptr);
        previous = current != nullptr ? current : "C";
        Require(std::setlocale(LC_CTYPE, "C") != nullptr, "select the C locale");
    }

    ~CLocaleFixture() {
        std::setlocale(LC_CTYPE, previous.c_str());
    }

    CLocaleFixture(const CLocaleFixture&) = delete;
    CLocaleFixture& operator=(const CLocaleFixture&) = delete;

private:
    std::string previous;
};

std::string Label(const char* name, int character) {
    return std::string(name) + "(" + std::to_string(character) + ")";
}

void RequireClassifierMatches(GuestCharacterFunction guest, HostCharacterFunction host, const char* name) {
    const CLocaleFixture locale;
    for (int character = EOF; character <= 255; ++character) {
        RequireEqual(guest(character) != 0, host(character) != 0, Label(name, character));
    }
}

void RequireConversionMatches(GuestCharacterFunction guest, HostCharacterFunction host, const char* name) {
    const CLocaleFixture locale;
    for (int character = EOF; character <= 255; ++character) {
        RequireEqual(guest(character), host(character), Label(name, character));
    }
}

void RequireConversionMatchesTable(GuestCharacterFunction guest, const short* table, const char* name) {
    const CLocaleFixture locale;
    for (int character = EOF; character <= 255; ++character) {
        RequireEqual(guest(character), static_cast<int>(table[character]), Label(name, character));
    }
}

const Case isupperCase{"Isupper_AllBytesAndEof_MatchesHostCLocale", [] {
    RequireClassifierMatches(isupper_nid_postfix, [](int c) { return std::isupper(c); }, "isupper");
}};

const Case islowerCase{"Islower_AllBytesAndEof_MatchesHostCLocale", [] {
    RequireClassifierMatches(islower_nid_postfix, [](int c) { return std::islower(c); }, "islower");
}};

const Case isalphaCase{"Isalpha_AllBytesAndEof_MatchesHostCLocale", [] {
    RequireClassifierMatches(isalpha_nid_postfix, [](int c) { return std::isalpha(c); }, "isalpha");
}};

const Case isdigitCase{"Isdigit_AllBytesAndEof_MatchesHostCLocale", [] {
    RequireClassifierMatches(isdigit_nid_postfix, [](int c) { return std::isdigit(c); }, "isdigit");
}};

const Case isalnumCase{"Isalnum_AllBytesAndEof_MatchesHostCLocale", [] {
    RequireClassifierMatches(isalnum_nid_postfix, [](int c) { return std::isalnum(c); }, "isalnum");
}};

const Case isspaceCase{"Isspace_AllBytesAndEof_MatchesHostCLocale", [] {
    RequireClassifierMatches(isspace_nid_postfix, [](int c) { return std::isspace(c); }, "isspace");
}};

const Case isblankCase{"Isblank_AllBytesAndEof_MatchesHostCLocale", [] {
    RequireClassifierMatches(isblank_nid_postfix, [](int c) { return std::isblank(c); }, "isblank");
}};

const Case iscntrlCase{"Iscntrl_AllBytesAndEof_MatchesHostCLocale", [] {
    RequireClassifierMatches(iscntrl_nid_postfix, [](int c) { return std::iscntrl(c); }, "iscntrl");
}};

const Case isprintCase{"Isprint_AllBytesAndEof_MatchesHostCLocale", [] {
    RequireClassifierMatches(isprint_nid_postfix, [](int c) { return std::isprint(c); }, "isprint");
}};

const Case isgraphCase{"Isgraph_AllBytesAndEof_MatchesHostCLocale", [] {
    RequireClassifierMatches(isgraph_nid_postfix, [](int c) { return std::isgraph(c); }, "isgraph");
}};

const Case ispunctCase{"Ispunct_AllBytesAndEof_MatchesHostCLocale", [] {
    RequireClassifierMatches(ispunct_nid_postfix, [](int c) { return std::ispunct(c); }, "ispunct");
}};

const Case isxdigitCase{"Isxdigit_AllBytesAndEof_MatchesHostCLocale", [] {
    RequireClassifierMatches(isxdigit_nid_postfix, [](int c) { return std::isxdigit(c); }, "isxdigit");
}};

const Case toupperCase{"Toupper_AllBytesAndEof_MatchesHostCLocale", [] {
    RequireConversionMatches(toupper_nid_postfix, [](int c) { return std::toupper(c); }, "toupper");
}};

const Case tolowerCase{"Tolower_AllBytesAndEof_MatchesHostCLocale", [] {
    RequireConversionMatches(tolower_nid_postfix, [](int c) { return std::tolower(c); }, "tolower");
}};

const Case toupperTable{"Toupper_AllBytesAndEof_MatchesGetptoupperTable", [] {
    RequireConversionMatchesTable(toupper_nid_postfix, _Getptoupper_nid_postfix(), "toupper");
}};

const Case tolowerTable{"Tolower_AllBytesAndEof_MatchesGetptolowerTable", [] {
    RequireConversionMatchesTable(tolower_nid_postfix, _Getptolower_nid_postfix(), "tolower");
}};

} // namespace
