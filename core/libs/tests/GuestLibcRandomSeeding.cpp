#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
std::int64_t APS5_VABI random_nid_postfix();
void APS5_VABI srandom_nid_postfix(std::uint64_t);
char* APS5_VABI initstate_nid_postfix(std::uint64_t, char*, std::int64_t);
char* APS5_VABI setstate_nid_postfix(char*);
}

static int failures = 0;

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        ++failures;
    }
}

static std::vector<std::int64_t> Draw(std::size_t count) {
    std::vector<std::int64_t> values;
    values.reserve(count);
    for (std::size_t index = 0; index < count; ++index) values.push_back(random_nid_postfix());
    return values;
}

static void RequireSequence(const std::vector<std::int64_t>& actual, const std::vector<std::int64_t>& expected, const char* message) {
    Require(actual == expected, message);
}

static std::string FirstStateWord(const char* table) {
    static const char* digits = "0123456789abcdef";
    std::string text;
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(table);
    for (std::size_t index = 0; index < 8; ++index) {
        text.push_back(digits[bytes[index] >> 4]);
        text.push_back(digits[bytes[index] & 0x0f]);
    }
    return text;
}

struct StateCase {
    std::int64_t size;
    std::vector<std::int64_t> values;
    const char* firstStateWord;
};

int main() {
    RequireSequence(Draw(5), {1804289383, 846930886, 1681692777, 1714636915, 1957747793},
        "a fresh process must draw the measured first five values");

    srandom_nid_postfix(1);
    RequireSequence(Draw(6), {1804289383, 846930886, 1681692777, 1714636915, 1957747793, 424238335},
        "srandom(1) must match the measured sequence");

    srandom_nid_postfix(0);
    RequireSequence(Draw(6), {577655601, 1248161417, 192959563, 143633695, 541917479, 1903093295},
        "srandom(0) must match the measured sequence");

    srandom_nid_postfix(2);
    RequireSequence(Draw(6), {1505335290, 1738766719, 190686788, 260874575, 747983061, 906156498},
        "srandom(2) must match the measured sequence");

    srandom_nid_postfix(12345);
    RequireSequence(Draw(6), {383100999, 858300821, 357768173, 455528251, 133005921, 116285904},
        "srandom(12345) must match the measured sequence");

    srandom_nid_postfix(0xffffffffull);
    RequireSequence(Draw(6), {254925627, 1205188300, 366127624, 1401405153, 76053476, 1604170158},
        "srandom(0xffffffff) must match the measured sequence");

    srandom_nid_postfix(0x80000000ull);
    RequireSequence(Draw(6), {1336741213, 1210407648, 1447044896, 337392383, 82502902, 538660432},
        "srandom(0x80000000) must match the measured sequence");

    std::vector<char> tiny(16);
    Require(initstate_nid_postfix(1, tiny.data(), 0) == nullptr, "initstate of 0 bytes must return null");
    Require(initstate_nid_postfix(1, tiny.data(), 7) == nullptr, "initstate of 7 bytes must return null");

    const std::array<StateCase, 13> states {{
        {8, {1646035001, 1025921153, 510616708, 590357944, 771515668}, "000000001465fc2d"},
        {15, {1646035001, 1025921153, 510616708, 590357944, 771515668}, "000000001465fc2d"},
        {31, {1646035001, 1025921153, 510616708, 590357944, 771515668}, "000000001465fc2d"},
        {32, {964237963, 406111040, 156505215, 1274863108, 1882652865}, "0100000082016ee0"},
        {63, {964237963, 406111040, 156505215, 1274863108, 1882652865}, "0100000082016ee0"},
        {64, {1894937090, 1645272306, 2143216519, 1889283008, 669383071}, "02000000fea8f5e2"},
        {127, {1894937090, 1645272306, 2143216519, 1889283008, 669383071}, "02000000fea8f5e2"},
        {128, {1804289383, 846930886, 1681692777, 1714636915, 1957747793}, "03000000b1391599"},
        {129, {1804289383, 846930886, 1681692777, 1714636915, 1957747793}, "03000000b1391599"},
        {255, {1804289383, 846930886, 1681692777, 1714636915, 1957747793}, "03000000b1391599"},
        {256, {510644794, 625058908, 1816371419, 326864818, 1257431873}, "0400000032ba371f"},
        {257, {510644794, 625058908, 1816371419, 326864818, 1257431873}, "0400000032ba371f"},
        {1000, {510644794, 625058908, 1816371419, 326864818, 1257431873}, "0400000032ba371f"},
    }};

    std::vector<std::vector<char>> buffers;
    for (const auto& state : states) {
        buffers.emplace_back(static_cast<std::size_t>(state.size) + 16, static_cast<char>(0xa5));
        char* const buffer = buffers.back().data();
        char* const returned = initstate_nid_postfix(1, buffer, state.size);
        Require(returned != nullptr, "initstate must not return null from 8 bytes up");
        RequireSequence(Draw(5), state.values, "initstate must draw the measured values");
        Require(FirstStateWord(buffer) == state.firstStateWord, "initstate must save the measured type word and first state word");
    }

    std::vector<char> last(128);
    char* const replaced = initstate_nid_postfix(5, last.data(), 128);
    Require(replaced != nullptr, "initstate must not return null from 8 bytes up");
    Require(setstate_nid_postfix(replaced) != nullptr, "setstate of the state initstate returned must not return null");

    return failures == 0 ? 0 : 1;
}
