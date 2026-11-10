#include "prx/libSceIcu/libSceIcu.hpp"
#include <bitset>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::int32_t Success = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t IllegalCharFound = 12;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t StringNotTerminated = -124;
constexpr std::int32_t UnicodeName = 0;
constexpr std::int32_t Unicode10Name = 1;
constexpr std::int32_t ExtendedName = 2;
constexpr std::int32_t AliasName = 3;

void Expect(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

void ExpectInt(std::int64_t actual, std::int64_t expected, const char* what) {
    if (actual == expected) return;
    std::fprintf(stderr, "%s returned %lld, expected %lld\n", what, static_cast<long long>(actual), static_cast<long long>(expected));
    std::abort();
}

template <typename Exception, typename Function>
void ExpectThrow(Function function, const char* what) {
    try {
        function();
    } catch (const Exception&) {
        return;
    }
    Expect(false, what);
}

std::string Name(std::int32_t character, std::int32_t choice) {
    char buffer[128];
    std::int32_t error = Success;
    const auto length = u_charName_nid_postfix(character, choice, buffer, sizeof(buffer), &error);
    Expect(error == Success, "u_charName status");
    Expect(length >= 0 && length < static_cast<std::int32_t>(sizeof(buffer)) && buffer[length] == 0, "u_charName terminated");
    return std::string(buffer, length);
}

void ExpectName(std::int32_t character, std::int32_t choice, const char* expected, const char* what) {
    if (Name(character, choice) == expected) return;
    std::fprintf(stderr, "%s returned \"%s\", expected \"%s\"\n", what, Name(character, choice).c_str(), expected);
    std::abort();
}

void ExpectCharacter(std::int32_t choice, const char* name, std::int32_t expected, const char* what) {
    std::int32_t error = Success;
    ExpectInt(u_charFromName_nid_postfix(choice, name, &error), expected, what);
    ExpectInt(error, Success, what);
}

void TestName() {
    ExpectName(u'A', UnicodeName, "LATIN CAPITAL LETTER A", "u_charName(A)");
    ExpectName(0x1F600, UnicodeName, "GRINNING FACE", "u_charName(U+1F600)");
    ExpectName(0x4E00, UnicodeName, "CJK UNIFIED IDEOGRAPH-4E00", "u_charName(U+4E00)");
    ExpectName(0xAC01, UnicodeName, "HANGUL SYLLABLE GAG", "u_charName(U+AC01)");
    ExpectName(0, UnicodeName, "", "u_charName(U+0000)");
    ExpectName(0, ExtendedName, "<control-0000>", "u_charName(U+0000, extended)");
    ExpectName(0xE000, ExtendedName, "<private use area-E000>", "u_charName(U+E000, extended)");
    ExpectName(0x01A2, AliasName, "LATIN CAPITAL LETTER GHA", "u_charName(U+01A2, alias)");
    ExpectName(u'A', Unicode10Name, "", "u_charName(A, Unicode 1.0)");
    ExpectName(0x110000, UnicodeName, "", "u_charName(U+110000)");
    ExpectName(-1, ExtendedName, "", "u_charName(-1)");

    char buffer[32];
    std::int32_t error = Success;
    std::memset(buffer, 'x', sizeof(buffer));
    ExpectInt(u_charName_nid_postfix(u'A', UnicodeName, buffer, 5, &error), 22, "u_charName overflow length");
    ExpectInt(error, BufferOverflow, "u_charName overflow");
    Expect(std::string(buffer, 5) == "LATIN" && buffer[5] == 'x', "u_charName overflow contents");
    error = Success;
    ExpectInt(u_charName_nid_postfix(u'A', UnicodeName, buffer, 22, &error), 22, "u_charName exact length");
    ExpectInt(error, StringNotTerminated, "u_charName exact");
    error = Success;
    ExpectInt(u_charName_nid_postfix(u'A', UnicodeName, nullptr, 0, &error), 22, "u_charName preflight");
    ExpectInt(error, BufferOverflow, "u_charName preflight status");

    for (const std::int32_t choice : {4, -1, 0x7fffffff}) {
        error = Success;
        ExpectInt(u_charName_nid_postfix(u'A', choice, buffer, sizeof(buffer), &error), 0, "u_charName bad choice");
        ExpectInt(error, IllegalArgument, "u_charName bad choice status");
    }
    error = Success;
    u_charName_nid_postfix(u'A', UnicodeName, nullptr, 4, &error);
    ExpectInt(error, IllegalArgument, "u_charName null buffer");
    error = Success;
    u_charName_nid_postfix(u'A', UnicodeName, buffer, -1, &error);
    ExpectInt(error, IllegalArgument, "u_charName negative capacity");
    error = BufferOverflow;
    ExpectInt(u_charName_nid_postfix(u'A', UnicodeName, buffer, sizeof(buffer), &error), 0, "u_charName failed input");
    ExpectInt(error, BufferOverflow, "u_charName failed input status");
    ExpectThrow<std::invalid_argument>([&] { u_charName_nid_postfix(u'A', UnicodeName, buffer, sizeof(buffer), nullptr); }, "u_charName null error");
}

void TestFromName() {
    ExpectCharacter(UnicodeName, "LATIN SMALL LETTER SHARP S", 0xDF, "u_charFromName(sharp s)");
    ExpectCharacter(UnicodeName, "grinning face", 0x1F600, "u_charFromName(lowercase)");
    ExpectCharacter(UnicodeName, "CJK UNIFIED IDEOGRAPH-4E00", 0x4E00, "u_charFromName(U+4E00)");
    ExpectCharacter(UnicodeName, "HANGUL SYLLABLE GAG", 0xAC01, "u_charFromName(U+AC01)");
    ExpectCharacter(ExtendedName, "<control-0007>", 7, "u_charFromName(<control-0007>)");
    ExpectCharacter(ExtendedName, "LATIN CAPITAL LETTER A", u'A', "u_charFromName(extended A)");
    ExpectCharacter(AliasName, "LATIN CAPITAL LETTER GHA", 0x01A2, "u_charFromName(alias)");

    std::int32_t error = Success;
    ExpectInt(u_charFromName_nid_postfix(UnicodeName, "<control-0007>", &error), 0xFFFF, "u_charFromName(<control-0007>, Unicode)");
    ExpectInt(error, IllegalCharFound, "u_charFromName(<control-0007>, Unicode) status");
    error = Success;
    ExpectInt(u_charFromName_nid_postfix(UnicodeName, "NO SUCH CHARACTER", &error), 0xFFFF, "u_charFromName(unknown)");
    ExpectInt(error, IllegalCharFound, "u_charFromName(unknown) status");
    for (const char* name : {static_cast<const char*>(nullptr), ""}) {
        error = Success;
        ExpectInt(u_charFromName_nid_postfix(UnicodeName, name, &error), 0xFFFF, "u_charFromName(empty)");
        ExpectInt(error, IllegalArgument, "u_charFromName(empty) status");
    }
    error = Success;
    u_charFromName_nid_postfix(-1, "LATIN CAPITAL LETTER A", &error);
    ExpectInt(error, IllegalArgument, "u_charFromName bad choice");
    ExpectThrow<std::invalid_argument>([] { u_charFromName_nid_postfix(UnicodeName, "LATIN CAPITAL LETTER A", nullptr); }, "u_charFromName null error");
}

struct Visit {
    std::vector<std::int32_t> characters;
    std::vector<std::string> names;
    std::int32_t stopAfter;
};

std::int8_t APS5_VABI Collect(void* context, std::int32_t character, std::int32_t choice, const char* name, std::int32_t length) {
    auto& visit = *static_cast<Visit*>(context);
    Expect(choice == ExtendedName, "u_enumCharNames choice");
    visit.characters.push_back(character);
    visit.names.emplace_back(name, length);
    return static_cast<std::int32_t>(visit.characters.size()) < visit.stopAfter;
}

void TestEnumeration() {
    Visit visit{{}, {}, 100};
    std::int32_t error = Success;
    u_enumCharNames_nid_postfix(0x40, 0x43, Collect, &visit, ExtendedName, &error);
    ExpectInt(error, Success, "u_enumCharNames status");
    Expect(visit.characters == std::vector<std::int32_t>{0x40, 0x41, 0x42}, "u_enumCharNames characters");
    Expect(visit.names == std::vector<std::string>{"COMMERCIAL AT", "LATIN CAPITAL LETTER A", "LATIN CAPITAL LETTER B"}, "u_enumCharNames names");

    visit = Visit{{}, {}, 2};
    u_enumCharNames_nid_postfix(0x1F600, 0x1F610, Collect, &visit, ExtendedName, &error);
    Expect(visit.characters == std::vector<std::int32_t>{0x1F600, 0x1F601}, "u_enumCharNames stops");

    visit = Visit{{}, {}, 100};
    u_enumCharNames_nid_postfix(0x4DFF, 0x4E01, Collect, &visit, ExtendedName, &error);
    Expect(visit.names == std::vector<std::string>{"HEXAGRAM FOR BEFORE COMPLETION", "CJK UNIFIED IDEOGRAPH-4E00"}, "u_enumCharNames algorithmic");

    visit = Visit{{}, {}, 100};
    u_enumCharNames_nid_postfix(0x50, 0x40, Collect, &visit, ExtendedName, &error);
    Expect(visit.characters.empty() && error == Success, "u_enumCharNames empty range");
    u_enumCharNames_nid_postfix(0x40, 0x43, nullptr, &visit, ExtendedName, &error);
    ExpectInt(error, IllegalArgument, "u_enumCharNames null visitor");
    ExpectThrow<std::invalid_argument>([&] { u_enumCharNames_nid_postfix(0x40, 0x43, Collect, &visit, ExtendedName, nullptr); }, "u_enumCharNames null error");
}

struct CharacterSet {
    void* self;
    std::bitset<128> ascii;
    std::int32_t other;
};

void APS5_VABI Add(void* set, std::int32_t character) {
    auto& characters = *static_cast<CharacterSet*>(set);
    Expect(characters.self == set, "uprv_getCharNameCharacters set");
    if (character >= 0 && character < 128) {
        characters.ascii.set(character);
    } else {
        ++characters.other;
    }
}

void TestNameSet() {
    const auto longest = Name(0xFBF9, UnicodeName);
    ExpectInt(static_cast<std::int64_t>(longest.size()), 83, "u_charName(U+FBF9) length");
    const auto maximum = uprv_getMaxCharNameLength_nid_postfix();
    Expect(maximum >= static_cast<std::int32_t>(longest.size()) && maximum < 128, "uprv_getMaxCharNameLength");

    CharacterSet characters{};
    characters.self = &characters;
    const Icu::SetAdder adder{&characters, Add, nullptr, nullptr, nullptr, nullptr};
    uprv_getCharNameCharacters_nid_postfix(&adder);
    for (const char character : std::string("AZ09 -<>control")) {
        Expect(characters.ascii.test(static_cast<unsigned char>(character)), "uprv_getCharNameCharacters contains");
    }
    for (const char character : std::string("@!~;\\")) {
        Expect(!characters.ascii.test(static_cast<unsigned char>(character)), "uprv_getCharNameCharacters excludes");
    }
    Expect(!characters.ascii.test(0) && characters.other == 0, "uprv_getCharNameCharacters ASCII only");
    ExpectThrow<std::invalid_argument>([] { uprv_getCharNameCharacters_nid_postfix(nullptr); }, "uprv_getCharNameCharacters null");
}

}

int main() {
    TestName();
    TestFromName();
    TestEnumeration();
    TestNameSet();
    std::puts("ICU character name tests passed");
}
