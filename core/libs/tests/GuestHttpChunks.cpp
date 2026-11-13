#include "prx/libSceHttp/src/ClientParsing.hpp"
#include <array>
#include <cstdio>
#include <string>

int main() {
    using namespace std::literals;
    constexpr std::array valid{
        ""sv, ";flag"sv, ";foo=bar"sv, " \t; \tfoo \t= \tbar \t; flag"sv,
        ";foo=\"bar baz\""sv, ";foo=\"\""sv, ";foo=\"a;b=c\""sv, ";foo=\"a\\\"b\\\\c\\\t\\ \\\xff\""sv,
        ";foo=\"x\x80\xff\""sv, ";foo=\" \tx\t \""sv, ";!#$%&'*+-.^_`|~=!#$%&'*+-.^_`|~"sv,
        ";foo=a;foo=b"sv, ";a;b=c;d=\"e;f\";g"sv, ";foo=\"!#[]~\x80\xff\""sv,
        ";foo=\"\\!\\[\\]\\~\\\x80\""sv, ";FoO=BaR"sv,
        ";"sv, "; \t"sv, ";=x"sv, ";\"foo\"=x"sv, ";na/me=x"sv, ";foo="sv, ";foo= \t"sv,
        ";foo=\"bar"sv, ";foo=\"bar\\"sv, ";foo=bar/baz"sv, ";f\0=x"sv, ";\xff=x"sv,
        ";foo=\"x\x01y\""sv, ";foo=\"x\x7fy\""sv, ";foo=\"x\\\x01y\""sv, ";foo=\"x\\\x7fy\""sv,
        ";foo=\"x\"junk"sv, ";foo;;bar"sv, ";foo==x"sv, " "sv, ";foo "sv,
        ";foo=x "sv, ";foo=\"x\"\t"sv, ";foo=\"x\ny\""sv, ";foo=x\\y"sv
    };
    constexpr std::array prefixes{"1"sv, "Ab"sv, "0000"sv, "FfFfFfFfFfFfFfFf"sv};
    constexpr std::array<std::uint64_t, 4> sizes{1, 171, 0, UINT64_MAX};
    constexpr std::array padding{""sv, " "sv, "\t"sv, " \t "sv};
    std::size_t accepted = 0;
    for (std::size_t prefix = 0; prefix < prefixes.size(); ++prefix) {
        for (const auto leading : padding) {
            for (const auto suffix : valid) {
                for (const auto trailing : padding) {
                    const auto line = std::string(leading) + std::string(prefixes[prefix]) + std::string(suffix) + std::string(trailing);
                    if (ChunkSize(line) == sizes[prefix]) ++accepted;
                }
            }
        }
    }
    constexpr std::array invalidSizes{
        ""sv, "+1"sv, "-1"sv, "0x1"sv, "g"sv, "1g"sv, "18446744073709551616"sv,
        "10000000000000000"sv, " "sv, "\t"sv, ";foo "sv, ";foo=x "sv,
        " \t;foo=x\t"sv, "1 2"sv, "1\t2"sv, "1 foo"sv, "1\tfoo"sv,
        "1\v"sv, "\v1"sv, "1\f"sv, "\f1"sv, "1\r"sv, "1\n"sv, "1\0"sv,
        "1\xff"sv, "1/"sv, "1=foo"sv
    };
    std::size_t badSizes = 0;
    for (const auto text : invalidSizes) {
        for (const auto suffix : valid) {
            if (!ChunkSize(std::string(text) + std::string(suffix))) ++badSizes;
        }
    }
    constexpr auto expectedValid = prefixes.size() * valid.size() * padding.size() * padding.size();
    constexpr auto expectedInvalid = invalidSizes.size() * valid.size();
    std::printf("Chunk lines: accepted %zu/%zu, invalid sizes refused %zu/%zu\n", accepted, expectedValid, badSizes, expectedInvalid);
    return accepted == expectedValid && badSizes == expectedInvalid ? 0 : 1;
}
