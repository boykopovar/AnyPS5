#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("icu converter: " + message);
}

constexpr std::int32_t ZeroError = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t UCS2Type = 3;

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());

    const std::int32_t available = ucnv_countAvailable_nid_postfix();
    Require(available >= 0, "ucnv_countAvailable reported a negative count");
    if (available > 0) {
        const char* first = ucnv_getAvailableName_nid_postfix(0);
        Require(first != nullptr && first[0] != 0, "the first converter has no name");
    }

    const std::int32_t standards = ucnv_countStandards_nid_postfix();
    Require(standards >= 0, "ucnv_countStandards reported a negative count");
    if (standards > 0) {
        std::int32_t status = ZeroError;
        const char* standard = ucnv_getStandard_nid_postfix(0, &status);
        Require(status == ZeroError && standard != nullptr && standard[0] != 0, "the first standard has no name");
    }

    std::int32_t status = ZeroError;
    const std::int32_t aliases = ucnv_countAliases_nid_postfix("UTF-8", &status);
    if (aliases > 0) {
        Require(status == ZeroError, "ucnv_countAliases failed for UTF-8");
        status = ZeroError;
        const char* alias = ucnv_getAlias_nid_postfix("UTF-8", 0, &status);
        Require(status == ZeroError && alias != nullptr && alias[0] != 0, "the first UTF-8 alias is empty");
        status = ZeroError;
        const char* iana = ucnv_getStandardName_nid_postfix("UTF-8", "IANA", &status);
        Require(status == ZeroError && iana != nullptr, "ucnv_getStandardName returned no name for UTF-8");
    }

    status = ZeroError;
    Require(ucnv_countAliases_nid_postfix("no such converter", &status) == 0, "an unknown converter has aliases");

    Require(ucnv_getStandard_nid_postfix(0, nullptr) == nullptr, "ucnv_getStandard accepted a null status");
    Require(ucnv_getAlias_nid_postfix("UTF-8", 0, nullptr) == nullptr, "ucnv_getAlias accepted a null status");
    Require(ucnv_getStandardName_nid_postfix("UTF-8", "IANA", nullptr) == nullptr,
        "ucnv_getStandardName accepted a null status");

    {
        std::int32_t openStatus = ZeroError;
        UConverter* utf8 = ucnv_open_nid_postfix("UTF-8", &openStatus);
        Require(openStatus == ZeroError && utf8 != nullptr, "ucnv_open did not open UTF-8");
        Require(ucnv_getMaxCharSize_nid_postfix(utf8) >= 1, "UTF-8 reports an invalid maximum character size");
        Require(ucnv_getMinCharSize_nid_postfix(utf8) >= 1, "UTF-8 reports an invalid minimum character size");
        Require(ucnv_isAmbiguous_nid_postfix(utf8) == 0, "UTF-8 reports itself as ambiguous");
        openStatus = ZeroError;
        Require(ucnv_isFixedWidth_nid_postfix(utf8, &openStatus) == 0, "UTF-8 reports itself as fixed width");
        Require(openStatus == ZeroError, "ucnv_isFixedWidth failed for UTF-8");
        openStatus = ZeroError;
        const char* name = ucnv_getName_nid_postfix(utf8, &openStatus);
        Require(openStatus == ZeroError && name != nullptr && std::strcmp(name, "UTF-8") == 0,
            "ucnv_getName did not return UTF-8");
        openStatus = ZeroError;
        Require(ucnv_getType_nid_postfix(utf8) == UCS2Type || ucnv_getType_nid_postfix(utf8) >= 0,
            "ucnv_getType reported an invalid type");
        ucnv_setFallback_nid_postfix(utf8, 1);
        Require(ucnv_usesFallback_nid_postfix(utf8) == 1, "ucnv_setFallback did not enable fallbacks");
        ucnv_setFallback_nid_postfix(utf8, 0);
        Require(ucnv_usesFallback_nid_postfix(utf8) == 0, "ucnv_setFallback did not disable fallbacks");
        ucnv_reset_nid_postfix(utf8);
        openStatus = ZeroError;
        Require(ucnv_toUCountPending_nid_postfix(utf8, &openStatus) == 0, "a reset converter has pending bytes");

        const char16_t newSubstitution[] = {u'?', 0};
        openStatus = ZeroError;
        ucnv_setSubstString_nid_postfix(utf8, newSubstitution, 1, &openStatus);
        Require(openStatus == ZeroError, "ucnv_setSubstString failed for UTF-8");


        std::int8_t invalidLength = -1;
        char invalidBytes[32] = {};
        openStatus = ZeroError;
        ucnv_getInvalidChars_nid_postfix(utf8, invalidBytes, &invalidLength, &openStatus);
        Require(openStatus == ZeroError && invalidLength == 0, "a fresh UTF-8 converter reports invalid characters");

        std::int8_t invalidWideLength = -1;
        char16_t invalidChars[32] = {};
        openStatus = ZeroError;
        ucnv_getInvalidUChars_nid_postfix(utf8, invalidChars, &invalidWideLength, &openStatus);
        Require(openStatus == ZeroError && invalidWideLength == 0,
            "a fresh UTF-8 converter reports invalid code units");


        {
            const char16_t text[] = {u'A', u'B', 0};
            char encoded[16] = {};
            openStatus = ZeroError;
            const std::int32_t produced = ucnv_fromUChars_nid_postfix(utf8, encoded, 16, text, 2, &openStatus);
            Require(openStatus == ZeroError, "ucnv_fromUChars failed for UTF-8");
            Require(produced == 2 && encoded[0] == 'A' && encoded[1] == 'B',
                "ucnv_fromUChars did not encode the string");

            char terminated[16] = {};
            openStatus = ZeroError;
            const std::int32_t terminatedLength = ucnv_fromUChars_nid_postfix(utf8, terminated, 16, text, -1,
                &openStatus);
            Require(openStatus == ZeroError && terminatedLength == 2,
                "ucnv_fromUChars did not measure a terminated string");

            openStatus = ZeroError;
            Require(ucnv_fromUChars_nid_postfix(utf8, nullptr, 16, text, 2, &openStatus) == -1,
                "ucnv_fromUChars accepted a null destination");
            Require(openStatus == IllegalArgument, "ucnv_fromUChars did not report a null destination");
        }

        {
            void* fromAction = nullptr;
            void* fromContext = nullptr;
            ucnv_getFromUCallBack_nid_postfix(utf8, &fromAction, &fromContext);
            Require(fromAction != nullptr, "UTF-8 reports no from-Unicode callback");
            void* toAction = nullptr;
            void* toContext = nullptr;
            ucnv_getToUCallBack_nid_postfix(utf8, &toAction, &toContext);
            Require(toAction != nullptr, "UTF-8 reports no to-Unicode callback");
            ucnv_getFromUCallBack_nid_postfix(utf8, nullptr, nullptr);
        }

        {
            const char ascii[] = {'A', 0};
            const char* cursor = ascii;
            openStatus = ZeroError;
            const std::int32_t codePoint = ucnv_getNextUChar_nid_postfix(utf8, &cursor, ascii + 1, &openStatus);
            Require(openStatus == ZeroError && codePoint == 0x0041, "ucnv_getNextUChar did not decode A");
            Require(cursor == ascii + 1, "ucnv_getNextUChar did not advance the source pointer");

            const char euro[] = {static_cast<char>(0xE2), static_cast<char>(0x82), static_cast<char>(0xAC), 0};
            const char* euroCursor = euro;
            openStatus = ZeroError;
            const std::int32_t euroPoint = ucnv_getNextUChar_nid_postfix(utf8, &euroCursor, euro + 3, &openStatus);
            Require(openStatus == ZeroError && euroPoint == 0x20AC, "ucnv_getNextUChar did not decode the euro sign");
            Require(euroCursor == euro + 3, "ucnv_getNextUChar did not consume three bytes");

            openStatus = ZeroError;
            Require(ucnv_getNextUChar_nid_postfix(utf8, nullptr, nullptr, &openStatus) == -1,
                "ucnv_getNextUChar accepted a null source");
            Require(openStatus == IllegalArgument, "ucnv_getNextUChar did not report a null source");
        }

        {
            const char16_t twoChars[] = {u'A', u'B', 0};
            char canary[8] = {'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X'};
            openStatus = ZeroError;
            const std::int32_t required = ucnv_fromUChars_nid_postfix(utf8, canary, 1, twoChars, 2, &openStatus);
            Require(required == 2, "ucnv_fromUChars did not report the required length for a short buffer");
            Require(openStatus == BufferOverflow, "ucnv_fromUChars did not report an overflow");
            Require(canary[0] == 'A', "ucnv_fromUChars did not write the partial prefix on overflow");
            Require(canary[1] == 'X' && canary[2] == 'X' && canary[3] == 'X',
                "ucnv_fromUChars wrote past the destination capacity");
        }
        {
            const char16_t text[] = {u'A', u'\u00e9', u'B', 0};
            const char16_t substitution[] = {u'<', u'U', u'0', u'0', u'E', u'9', u'>', u'!', 0};
            char converted[32] = {};
            std::int32_t asciiStatus = ZeroError;
            UConverter* ascii = ucnv_open_nid_postfix("US-ASCII", &asciiStatus);
            Require(asciiStatus == ZeroError && ascii != nullptr, "US-ASCII did not open");
            asciiStatus = ZeroError;
            ucnv_setSubstString_nid_postfix(ascii, substitution, 8, &asciiStatus);
            Require(asciiStatus == ZeroError, "ucnv_setSubstString rejected an eight-unit substitution");
            asciiStatus = ZeroError;
            const std::int32_t replaced = ucnv_fromUChars_nid_postfix(ascii, converted, 32, text, 3, &asciiStatus);
            Require(asciiStatus == ZeroError && replaced == 10, "the eight-unit substitution was not used");
            Require(std::memcmp(converted, "A<U00E9>!B", 10) == 0, "the eight-unit substitution was truncated");
            ucnv_close_nid_postfix(ascii);
        }
        {
            std::vector<char16_t> source(5000, u'A');
            std::vector<char> destination(8192, 0);
            openStatus = ZeroError;
            const std::int32_t large = ucnv_fromUChars_nid_postfix(utf8, destination.data(), 8192,
                source.data(), 5000, &openStatus);
            Require(openStatus == ZeroError && large == 5000,
                "a capacity above 4096 or a source above 4095 was rejected");
            Require(destination[0] == 'A' && destination[4999] == 'A' && destination[5000] == 0,
                "the large conversion wrote the wrong bytes");
        }
        ucnv_close_nid_postfix(utf8);
        ucnv_close_nid_postfix(nullptr);

        openStatus = ZeroError;
        Require(ucnv_open_nid_postfix(nullptr, &openStatus) == nullptr, "ucnv_open accepted a null name");
        Require(openStatus == IllegalArgument, "ucnv_open did not report an illegal argument for a null name");
        openStatus = ZeroError;
        Require(ucnv_open_nid_postfix("no such converter", &openStatus) == nullptr,
            "ucnv_open accepted an unknown converter");
        Require(openStatus != ZeroError, "ucnv_open did not report an error for an unknown converter");
    }
    return 0;
}
