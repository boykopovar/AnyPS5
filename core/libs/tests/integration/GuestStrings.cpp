#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

extern "C" {
char* APS5_VABI basename_nid_postfix(const char*);
int* APS5_VABI __error_nid_postfix();
std::size_t APS5_VABI strnlen_nid_postfix(const char*, std::size_t);
std::size_t APS5_VABI strnlen_s_nid_postfix(const char*, std::size_t);
char* APS5_VABI strncat_nid_postfix(char*, const char*, std::size_t);
char* APS5_VABI strpbrk_nid_postfix(const char*, const char*);
std::size_t APS5_VABI strcspn_nid_postfix(const char*, const char*);
std::size_t APS5_VABI strlcat_nid_postfix(char*, const char*, std::size_t);
char* APS5_VABI stpcpy_nid_postfix(char*, const char*);
char* APS5_VABI strtok_r_nid_postfix(char*, const char*, char**);
char* APS5_VABI strtok_nid_postfix(char*, const char*);
char* APS5_VABI strcasestr_nid_postfix(const char*, const char*);
int APS5_VABI strcpy_s_nid_postfix(char*, std::size_t, const char*);
int APS5_VABI strcat_s_nid_postfix(char*, std::size_t, const char*);
int APS5_VABI strncat_s_nid_postfix(char*, std::size_t, const char*, std::size_t);
int APS5_VABI memcpy_s_nid_postfix(void*, std::size_t, const void*, std::size_t);
int APS5_VABI memmove_s_nid_postfix(void*, std::size_t, const void*, std::size_t);
int APS5_VABI memset_s_nid_postfix(void*, std::size_t, int, std::size_t);
char* APS5_VABI strnstr_nid_postfix(const char*, const char*, std::size_t);
int APS5_VABI snprintf_s_nid_postfix(char*, std::size_t, const char*, ...);
int APS5_VABI sscanf_s_nid_postfix(const char*, const char*, ...);
int APS5_VABI __inet_aton_nid_postfix(const char*, void*);
std::uint32_t APS5_VABI __inet_addr_nid_postfix(const char*);
}

namespace {

using namespace std::string_view_literals;
using Testing::Case;
using Testing::RequireEqual;

std::string_view Text(const char* text) {
    return text == nullptr ? "<null>"sv : std::string_view(text);
}

std::string Bytes(const void* data, std::size_t size) {
    return std::string(static_cast<const char*>(data), size);
}

const Case strcpySFits{"StrcpyS_SourceFits_CopiesString", [] {
    char small[4] = "zz";
    RequireEqual(strcpy_s_nid_postfix(small, sizeof(small), "abc"), 0, "status");
    RequireEqual(Text(small), "abc"sv, "copy");
}};

const Case strcpySTooLong{"StrcpyS_SourceTooLong_FailsWithErangeAndClearsDestination", [] {
    char small[4] = "abc";
    RequireEqual(strcpy_s_nid_postfix(small, sizeof(small), "abcd"), 34, "status");
    RequireEqual(small[0], '\0', "destination cleared");
}};

const Case strcpySNull{"StrcpyS_NullDestination_FailsWithEinval", [] {
    RequireEqual(strcpy_s_nid_postfix(nullptr, 4, "a"), 22, "status");
}};

const Case strcatS{"StrcatS_SourceFits_AppendsStrings", [] {
    char joined[8] = "ab";
    RequireEqual(strcat_s_nid_postfix(joined, sizeof(joined), "cd"), 0, "strcat_s status");
    RequireEqual(Text(joined), "abcd"sv, "strcat_s result");
    RequireEqual(strncat_s_nid_postfix(joined, sizeof(joined), "efgh", 2), 0, "strncat_s status");
    RequireEqual(Text(joined), "abcdef"sv, "strncat_s result");
}};

const Case strcatSTooLong{"StrcatS_SourceTooLong_FailsWithErangeAndClearsDestination", [] {
    char joined[8] = "abcdef";
    RequireEqual(strcat_s_nid_postfix(joined, sizeof(joined), "gh"), 34, "status");
    RequireEqual(joined[0], '\0', "destination cleared");
}};

const Case memcpySFits{"MemcpyS_SourceFits_CopiesOnlyCount", [] {
    char bytes[4] = {1, 2, 3, 4};
    const char source[4] = {5, 6, 7, 8};
    RequireEqual(memcpy_s_nid_postfix(bytes, sizeof(bytes), source, 2), 0, "status");
    RequireEqual(bytes[0], char{5}, "copied byte");
    RequireEqual(bytes[2], char{3}, "untouched byte");
}};

const Case memcpySTooLong{"MemcpyS_CountExceedsDestination_FailsWithErangeAndClearsDestination", [] {
    char bytes[4] = {5, 6, 3, 4};
    const char source[4] = {5, 6, 7, 8};
    RequireEqual(memcpy_s_nid_postfix(bytes, 2, source, 4), 34, "status");
    RequireEqual(bytes[0], char{0}, "first byte cleared");
    RequireEqual(bytes[1], char{0}, "second byte cleared");
    RequireEqual(bytes[2], char{3}, "byte beyond destination untouched");
}};

const Case memmoveS{"MemmoveS_OverlappingRanges_MovesBytes", [] {
    char overlap[6] = "abcde";
    RequireEqual(memmove_s_nid_postfix(overlap + 1, 5, overlap, 3), 0, "status");
    RequireEqual(Text(overlap), "aabce"sv, "result");
}};

const Case memsetSTooLong{"MemsetS_CountExceedsDestination_FailsWithErangeAndFillsDestination", [] {
    char bytes[4] = {0, 0, 3, 4};
    RequireEqual(memset_s_nid_postfix(bytes, sizeof(bytes), 9, 8), 34, "status");
    RequireEqual(bytes[3], char{9}, "last byte filled");
}};

const Case strnstrCase{"Strnstr_Limit_BoundsTheSearch", [] {
    RequireEqual(strnstr_nid_postfix("haystack", "st", 4) == nullptr, true, "match beyond the limit");
    const char haystack[] = "haystack";
    RequireEqual(strnstr_nid_postfix(haystack, "st", 6) == haystack + 3, true, "match within the limit");
}};

const Case snprintfS{"SnprintfS_FittingOutput_FormatsArguments", [] {
    char formatted[8];
    RequireEqual(snprintf_s_nid_postfix(formatted, sizeof(formatted), "%d-%s", 42, "x"), 4, "count");
    RequireEqual(Text(formatted), "42-x"sv, "text");
}};

#ifndef _WIN32
const Case sscanfSSizedConversions{"SscanfS_SizedConversions_StoreWithinBufferSizes", [] {
    int number = 0;
    char word[4] = "zz";
    char letter = 0;
    char value[8] = {};
    RequireEqual(sscanf_s_nid_postfix(" 12 abc x", "%d %s %c", &number, word, 4u, &letter, 1u), 3, "\" 12 abc x\" count");
    RequireEqual(number, 12, "\" 12 abc x\" number");
    RequireEqual(Text(word), "abc"sv, "\" 12 abc x\" word");
    RequireEqual(letter, 'x', "\" 12 abc x\" letter");
    RequireEqual(sscanf_s_nid_postfix("key=val", "%3[a-z]=%3s", word, 4u, value, 8u), 2, "key=val count");
    RequireEqual(Text(word), "key"sv, "key=val key");
    RequireEqual(Text(value), "val"sv, "key=val value");
    RequireEqual(sscanf_s_nid_postfix("abcdef", "%3s", word, 4u), 1, "%3s into word count");
    RequireEqual(Text(word), "abc"sv, "%3s into word");
    RequireEqual(sscanf_s_nid_postfix("abcdef", "%3s", value, 8u), 1, "%3s into value count");
    RequireEqual(Text(value), "abc"sv, "%3s into value");
    RequireEqual(sscanf_s_nid_postfix("abcdef", "%3[a-z]", value, 8u), 1, "%3[a-z] count");
    RequireEqual(Text(value), "abc"sv, "%3[a-z] value");
    RequireEqual(sscanf_s_nid_postfix("2024ABCD", "%3s%4s", word, 4u, value, 8u), 2, "%3s%4s count");
    RequireEqual(Text(word), "202"sv, "%3s%4s word");
    RequireEqual(Text(value), "4ABC"sv, "%3s%4s value");
}};

const Case sscanfSTooSmall{"SscanfS_FieldLargerThanBuffer_StopsAndClearsBuffer", [] {
    int number = 0;
    char word[4] = "zz";
    char value[8] = {};
    RequireEqual(sscanf_s_nid_postfix("12 abcd", "%d %s", &number, word, 4u), 1, "\"12 abcd\" count");
    RequireEqual(word[0], '\0', "\"12 abcd\" word cleared");
    std::strcpy(word, "zz");
    RequireEqual(sscanf_s_nid_postfix("2024ABCD 7", "%4s%s", word, 4u, value, 8u), 0, "\"2024ABCD 7\" count");
    RequireEqual(word[0], '\0', "\"2024ABCD 7\" word cleared");
}};

const Case sscanfSLiteralPercent{"SscanfS_LiteralPercentAndCount_ReportPosition", [] {
    int number = 0;
    int position = 0;
    RequireEqual(sscanf_s_nid_postfix("7 %", "%d %%%n", &number, &position), 1, "count");
    RequireEqual(position, 3, "position");
}};

const Case sscanfSNoInput{"SscanfS_OnlyWhitespace_ReturnsEof", [] {
    int number = 0;
    RequireEqual(sscanf_s_nid_postfix("   ", "%d", &number), EOF, "result");
}};

const Case sscanfSMismatch{"SscanfS_MismatchedInput_ReturnsZero", [] {
    int number = 0;
    RequireEqual(sscanf_s_nid_postfix("x", "%d", &number), 0, "result");
}};
#endif

const Case basenameCase{"Basename_Paths_ReturnFinalComponent", [] {
    struct Example {
        const char* input;
        std::string_view expected;
    };
    const Example examples[] = {
        {nullptr, "."sv}, {"", "."sv}, {"////", "/"sv}, {"/one/two///", "two"sv}, {"one\\two", "one\\two"sv},
    };
    for (const auto& example : examples) {
        RequireEqual(Text(basename_nid_postfix(example.input)), example.expected, std::string("basename of ") + std::string(Text(example.input)));
    }
}};

const Case basenameKeepsInput{"Basename_TrailingSlashes_DoesNotModifyInput", [] {
    const char path[] = "/one/two///";
    RequireEqual(Text(basename_nid_postfix(path)), "two"sv, "result");
    RequireEqual(Text(path), "/one/two///"sv, "input");
}};

const Case basenameTooLong{"Basename_PathLongerThanBuffer_FailsWithEnametoolong", [] {
    const std::string longName(1024, 'x');
    RequireEqual(basename_nid_postfix(longName.c_str()) == nullptr, true, "result");
    RequireEqual(*__error_nid_postfix(), 63, "errno");
}};

const Case strnlenCase{"Strnlen_Limit_BoundsTheLength", [] {
    const char bounded[] = {'a', 'b', 'c'};
    RequireEqual(strnlen_nid_postfix(bounded, 0), std::size_t{0}, "limit 0");
    RequireEqual(strnlen_nid_postfix(bounded, sizeof(bounded)), std::size_t{3}, "unterminated array");
    RequireEqual(strnlen_nid_postfix("a", 8), std::size_t{1}, "short string");
}};

const Case strnlenS{"StrnlenS_NullAndBoundedStrings_ReturnBoundedLength", [] {
    RequireEqual(strnlen_s_nid_postfix(nullptr, 8), std::size_t{0}, "null");
    RequireEqual(strnlen_s_nid_postfix("abc", 8), std::size_t{3}, "abc");
    RequireEqual(strnlen_s_nid_postfix("abcdef", 4), std::size_t{4}, "abcdef limited to 4");
}};

const Case strlcatShortSize{"Strlcat_SizeShorterThanDestination_ReturnsSumWithoutWriting", [] {
    char truncated[] = "abXX";
    RequireEqual(strlcat_nid_postfix(truncated, "cd", 2), std::size_t{4}, "result");
    RequireEqual(Text(truncated), "abXX"sv, "destination");
}};

const Case strlcatTruncates{"Strlcat_LongSource_TruncatesAndReturnsFullLength", [] {
    char buffer[8] = "ab";
    RequireEqual(strlcat_nid_postfix(buffer, "cdefgh", sizeof(buffer)), std::size_t{8}, "result");
    RequireEqual(Text(buffer), "abcdefg"sv, "destination");
}};

const Case strlcatZeroSize{"Strlcat_ZeroSize_ReturnsSourceLength", [] {
    char buffer[8] = "abcdefg";
    RequireEqual(strlcat_nid_postfix(buffer, "xyz", 0), std::size_t{3}, "result");
}};

const Case strlcatSizeOne{"Strlcat_SizeOneOnEmptyDestination_KeepsTerminator", [] {
    char buffer[8] = "abcdefg";
    buffer[0] = '\0';
    RequireEqual(strlcat_nid_postfix(buffer, "x", 1), std::size_t{1}, "result");
    RequireEqual(buffer[0], '\0', "terminator");
}};

const Case stpcpyCase{"Stpcpy_ChainedCopies_ReturnEndOfString", [] {
    char chained[8] = "zzzzzzz";
    char* end = stpcpy_nid_postfix(chained, "ab");
    RequireEqual(end == chained + 2, true, "first end");
    RequireEqual(*end, '\0', "first terminator");
    RequireEqual(chained[3], 'z', "first copy stays bounded");
    end = stpcpy_nid_postfix(end, "cd");
    RequireEqual(end == chained + 4, true, "second end");
    RequireEqual(Text(chained), "abcd"sv, "chained result");
    RequireEqual(stpcpy_nid_postfix(end, "") == end, true, "empty copy end");
    RequireEqual(chained[5], 'z', "empty copy stays bounded");
}};

const Case strncatCase{"Strncat_Count_AppendsAtMostCount", [] {
    char buffer[8] = "";
    RequireEqual(strncat_nid_postfix(buffer, "xyz", 2) == buffer, true, "returns destination");
    RequireEqual(Text(buffer), "xy"sv, "result");
}};

const Case strpbrkCase{"Strpbrk_Accept_FindsFirstMatchingCharacter", [] {
    const char buffer[] = "xy";
    RequireEqual(strpbrk_nid_postfix(buffer, "ay") == buffer + 1, true, "match");
    RequireEqual(strpbrk_nid_postfix(buffer, "") == nullptr, true, "empty accept set");
}};

const Case strcspnCase{"Strcspn_Reject_CountsLeadingNonMatches", [] {
    RequireEqual(strcspn_nid_postfix("xy", "y"), std::size_t{1}, "result");
}};

const Case inetAtonValid{"InetAton_ValidAddresses_StoreNetworkOrderBytes", [] {
    struct Example {
        const char* text;
        std::string_view expected;
    };
    const Example examples[] = {
        {"192.0.2.42", "192.0.2.42"sv}, {"10.1.2", "10.1.0.2"sv}, {"127.1", "127.0.0.1"sv},
        {"3232235777", "192.168.1.1"sv}, {"0x7f.0.0.0x1", "127.0.0.1"sv}, {"0377.0.0.010", "255.0.0.8"sv},
        {"1.2.3.4 trailing", "1.2.3.4"sv}, {"1.2.3.4\n", "1.2.3.4"sv},
    };
    for (const auto& example : examples) {
        unsigned char address[4] = {0xA5, 0xA5, 0xA5, 0xA5};
        const std::string label = std::string("inet_aton(\"") + example.text + "\")";
        RequireEqual(__inet_aton_nid_postfix(example.text, address), 1, label + " result");
        char formatted[16];
        std::snprintf(formatted, sizeof(formatted), "%u.%u.%u.%u", address[0], address[1], address[2], address[3]);
        RequireEqual(Text(formatted), example.expected, label + " address");
    }
}};

const Case inetAtonNullAddress{"InetAton_NullAddress_ValidatesOnly", [] {
    RequireEqual(__inet_aton_nid_postfix("1.2.3.4", nullptr), 1, "result");
}};

const Case inetInvalid{"InetAtonAndInetAddr_InvalidAddresses_AreRejected", [] {
    for (const char* invalid : {"", " 1.2.3.4", "1.2.3.4.5", "256.1.1.1", "1.2.3.256", "1.2.65536", "08", "1..2", "a.b.c.d",
             "1.2.3.4x", "0x", "1.2.3.", "-1"}) {
        const std::string label = std::string("\"") + invalid + "\"";
        unsigned char address[4] = {0xA5, 0xA5, 0xA5, 0xA5};
        RequireEqual(__inet_aton_nid_postfix(invalid, address), 0, label + " inet_aton result");
        RequireEqual(address[0], static_cast<unsigned char>(0xA5), label + " first byte untouched");
        RequireEqual(address[3], static_cast<unsigned char>(0xA5), label + " last byte untouched");
        RequireEqual(__inet_addr_nid_postfix(invalid), std::uint32_t{0xffffffff}, label + " inet_addr result");
    }
}};

const Case inetAddrValid{"InetAddr_ValidAddresses_ReturnNetworkOrderValue", [] {
    struct Example {
        const char* text;
        std::array<unsigned char, 4> expected;
    };
    const Example examples[] = {
        {"192.0.2.42", {192, 0, 2, 42}}, {"127.1", {127, 0, 0, 1}}, {"0x7f.0.0.0x1", {127, 0, 0, 1}},
        {"0.0.0.0", {0, 0, 0, 0}}, {"255.255.255.255", {255, 255, 255, 255}}, {"4294967296", {0, 0, 0, 0}},
        {"18446744073709551617", {0, 0, 0, 1}},
    };
    for (const auto& example : examples) {
        const std::uint32_t value = __inet_addr_nid_postfix(example.text);
        RequireEqual(Bytes(&value, sizeof(value)), Bytes(example.expected.data(), example.expected.size()),
            std::string("inet_addr(\"") + example.text + "\") bytes");
    }
}};

const Case strtokR{"StrtokR_SeparateStates_TokenizeIndependently", [] {
    char first[] = ",a,,b,";
    char second[] = "x:y";
    char* firstState = nullptr;
    char* secondState = nullptr;
    RequireEqual(Text(strtok_r_nid_postfix(first, ",", &firstState)), "a"sv, "first token of first string");
    RequireEqual(Text(strtok_r_nid_postfix(second, ":", &secondState)), "x"sv, "first token of second string");
    RequireEqual(Text(strtok_r_nid_postfix(nullptr, ",", &firstState)), "b"sv, "second token of first string");
    RequireEqual(strtok_r_nid_postfix(nullptr, ",", &firstState) == nullptr, true, "first string exhausted");
    RequireEqual(strtok_r_nid_postfix(nullptr, ",", &firstState) == nullptr, true, "first string stays exhausted");
    RequireEqual(Text(strtok_r_nid_postfix(nullptr, "", &secondState)), "y"sv, "rest of second string");
}};

const Case strtokCase{"Strtok_GuestAndHostState_AreIndependent", [] {
    char hostTokens[] = "host:next";
    RequireEqual(Text(std::strtok(hostTokens, ":")), "host"sv, "first host token");
    char guestTokens[] = ",one,,two:three";
    RequireEqual(Text(strtok_nid_postfix(guestTokens, ",")), "one"sv, "first guest token");
    RequireEqual(Text(strtok_nid_postfix(nullptr, ":,")), "two"sv, "second guest token");
    RequireEqual(Text(strtok_nid_postfix(nullptr, "")), "three"sv, "third guest token");
    RequireEqual(strtok_nid_postfix(nullptr, ",") == nullptr, true, "guest string exhausted");
    RequireEqual(strtok_nid_postfix(nullptr, ",") == nullptr, true, "guest string stays exhausted");
    RequireEqual(Text(std::strtok(nullptr, ":")), "next"sv, "second host token");
}};

const Case strcasestrCase{"Strcasestr_Needles_MatchIgnoringCase", [] {
    const char text[] = "aABAbC";
    RequireEqual(strcasestr_nid_postfix(text, "ababc") == text + 1, true, "ababc");
    RequireEqual(strcasestr_nid_postfix(text, "") == text, true, "empty needle");
    RequireEqual(strcasestr_nid_postfix(text, "abcdef") == nullptr, true, "absent needle");
    RequireEqual(strcasestr_nid_postfix("", "a") == nullptr, true, "empty haystack");
    const char highBytes[] = {static_cast<char>(0xff), 'A', 0};
    RequireEqual(strcasestr_nid_postfix(highBytes, "a") == highBytes + 1, true, "high byte haystack");
}};

const Case memcpySOverlap{"MemcpyS_OverlappingRanges_FailsWithEinvalAndClearsDestination", [] {
    for (const auto offset : {0, 1, -1, 3, -3}) {
        const std::string label = "offset " + std::to_string(offset);
        unsigned char bytes[16];
        std::memset(bytes, 0x5a, sizeof(bytes));
        auto* destination = bytes + 4;
        const auto* source = destination + offset;
        RequireEqual(memcpy_s_nid_postfix(destination, 8, source, 4), 22, label + " status");
        RequireEqual(Bytes(bytes + 4, 8), std::string(8, '\0'), label + " destination cleared");
        RequireEqual(bytes[3], static_cast<unsigned char>(0x5a), label + " byte before destination");
        RequireEqual(bytes[12], static_cast<unsigned char>(0x5a), label + " byte after destination");
    }
}};

const Case memcpySAdjacent{"MemcpyS_AdjacentRanges_Copy", [] {
    unsigned char adjacent[] = {1, 2, 3, 4, 5, 6, 7, 8};
    RequireEqual(memcpy_s_nid_postfix(adjacent, 4, adjacent + 4, 4), 0, "source after destination status");
    RequireEqual(Bytes(adjacent, 4), Bytes(adjacent + 4, 4), "source after destination bytes");
    const unsigned char original[] = {1, 2, 3, 4, 5, 6, 7, 8};
    std::memcpy(adjacent, original, sizeof(adjacent));
    RequireEqual(memcpy_s_nid_postfix(adjacent + 4, 4, adjacent, 4), 0, "source before destination status");
    const unsigned char unchanged[] = {1, 2, 3, 4, 1, 2, 3, 4};
    RequireEqual(Bytes(adjacent, sizeof(adjacent)), Bytes(unchanged, sizeof(unchanged)), "source before destination bytes");
}};

const Case memcpySZeroCount{"MemcpyS_ZeroCountOnSameBuffer_SucceedsWithoutChanges", [] {
    const unsigned char unchanged[] = {1, 2, 3, 4, 1, 2, 3, 4};
    unsigned char bytes[] = {1, 2, 3, 4, 1, 2, 3, 4};
    RequireEqual(memcpy_s_nid_postfix(bytes, sizeof(bytes), bytes, 0), 0, "status");
    RequireEqual(Bytes(bytes, sizeof(bytes)), Bytes(unchanged, sizeof(unchanged)), "bytes");
}};

} // namespace
