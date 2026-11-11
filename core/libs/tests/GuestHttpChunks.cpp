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
        ";foo=\"\\!\\[\\]\\~\\\x80\""sv, ";FoO=BaR"sv
    };
    constexpr std::array invalid{
        ";"sv, "; \t"sv, ";=x"sv, ";\"foo\"=x"sv, ";na/me=x"sv, ";foo="sv, ";foo= \t"sv,
        ";foo=\"bar"sv, ";foo=\"bar\\"sv, ";foo=bar/baz"sv, ";f\0=x"sv, ";\xff=x"sv,
        ";foo=\"x\x01y\""sv, ";foo=\"x\x7fy\""sv, ";foo=\"x\\\x01y\""sv, ";foo=\"x\\\x7fy\""sv,
        ";foo=\"x\"junk"sv, ";foo;;bar"sv, ";foo==x"sv, " "sv, ";foo "sv,
        ";foo=x "sv, ";foo=\"x\"\t"sv, ";foo=\"x\ny\""sv, ";foo=x\\y"sv
    };
    constexpr std::array prefixes{"1"sv, "Ab"sv, "0000"sv};
    constexpr std::array<std::uint64_t, 3> sizes{1, 171, 0};
    std::size_t accepted = 0;
    std::size_t refused = 0;
    for (std::size_t prefix = 0; prefix < prefixes.size(); ++prefix) {
        for (std::size_t suffix = 0; suffix < valid.size(); ++suffix) {
            try {
                if (ChunkSize(std::string(prefixes[prefix]) + std::string(valid[suffix])) == sizes[prefix]) ++accepted;
                else std::fprintf(stderr, "Wrong chunk size: prefix %zu suffix %zu\n", prefix, suffix);
            } catch (const HttpFailure&) {
                std::fprintf(stderr, "Valid chunk extension refused: prefix %zu suffix %zu\n", prefix, suffix);
            }
        }
        for (std::size_t suffix = 0; suffix < invalid.size(); ++suffix) {
            try {
                ChunkSize(std::string(prefixes[prefix]) + std::string(invalid[suffix]));
                std::fprintf(stderr, "Malformed chunk extension accepted: prefix %zu suffix %zu\n", prefix, suffix);
            } catch (const HttpFailure&) { ++refused; }
        }
    }
    constexpr std::array invalidSizes{
        ""sv, "+1"sv, "-1"sv, "0x1"sv, "g"sv, "1g"sv, "18446744073709551616"sv,
        "10000000000000000"sv, " 1"sv, "1 "sv, "\t1"sv, "1\t"sv
    };
    std::size_t badSizes = 0;
    for (const auto text : invalidSizes) {
        try { ChunkSize(text); }
        catch (const HttpFailure&) { ++badSizes; }
    }
    bool maximum = false;
    try { maximum = ChunkSize("FfFfFfFfFfFfFfFf;flag") == UINT64_MAX; }
    catch (const HttpFailure&) {}
    std::printf("Chunk extensions: valid %zu/48, malformed refused %zu/75, invalid sizes refused %zu/12, maximum %d/1\n",
        accepted, refused, badSizes, maximum);
    return accepted == 48 && refused == 75 && badSizes == 12 && maximum ? 0 : 1;
}
