#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("icu converter bulk: " + message);
}

constexpr std::int32_t ZeroError = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t InvalidCharacter = 10;
constexpr std::int32_t BufferOverflow = 15;
constexpr char ByteSentinel = static_cast<char>(0x55);
constexpr char16_t UnitSentinel = 0x5555;

void FillSentinel(char* bytes, std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) bytes[index] = ByteSentinel;
}

void RequireByteSentinel(const char* bytes, std::size_t count, const std::string& message) {
    for (std::size_t index = 0; index < count; ++index) {
        Require(bytes[index] == ByteSentinel, message);
    }
}

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());

    std::int32_t status = ZeroError;
    UConverter* utf8 = ucnv_open_nid_postfix("UTF-8", &status);
    Require(status == ZeroError && utf8 != nullptr, "ucnv_open did not open UTF-8");

    const char16_t text[] = {u'A', u'B', 0};
    char encoded[8] = {};
    char* encodedCursor = encoded;
    const char16_t* textCursor = text;
    status = ZeroError;
    ucnv_fromUnicode_nid_postfix(utf8, &encodedCursor, encoded + sizeof(encoded), &textCursor, text + 2,
        nullptr, 1, &status);
    Require(status == ZeroError, "ucnv_fromUnicode failed for UTF-8");
    Require(encodedCursor == encoded + 2, "ucnv_fromUnicode did not advance the target by two bytes");
    Require(textCursor == text + 2, "ucnv_fromUnicode did not advance the source by two units");
    Require(encoded[0] == 'A' && encoded[1] == 'B', "ucnv_fromUnicode did not encode AB");

    char16_t decoded[8] = {};
    char16_t* decodedCursor = decoded;
    const char* encodedSource = encoded;
    status = ZeroError;
    ucnv_toUnicode_nid_postfix(utf8, &decodedCursor, decoded + 8, &encodedSource, encoded + 2, nullptr, 1,
        &status);
    Require(status == ZeroError, "ucnv_toUnicode failed for UTF-8");
    Require(decodedCursor == decoded + 2, "ucnv_toUnicode did not advance the target by two units");
    Require(encodedSource == encoded + 2, "ucnv_toUnicode did not advance the source by two bytes");
    Require(decoded[0] == u'A' && decoded[1] == u'B', "ucnv_toUnicode did not decode AB");

    std::int32_t offsets[2] = {-1, -1};
    char* rejectedTarget = encoded;
    const char16_t* rejectedSource = text;
    status = ZeroError;
    ucnv_fromUnicode_nid_postfix(utf8, &rejectedTarget, encoded + sizeof(encoded), &rejectedSource, text + 2,
        offsets, 1, &status);
    Require(status == IllegalArgument, "ucnv_fromUnicode did not report the unsupported offsets");
    Require(rejectedTarget == encoded, "ucnv_fromUnicode moved the target after rejecting offsets");
    Require(rejectedSource == text, "ucnv_fromUnicode moved the source after rejecting offsets");

    char16_t* rejectedDecoded = decoded;
    const char* rejectedBytes = encoded;
    status = ZeroError;
    ucnv_toUnicode_nid_postfix(utf8, &rejectedDecoded, decoded + 8, &rejectedBytes, encoded + 2, offsets, 1,
        &status);
    Require(status == IllegalArgument, "ucnv_toUnicode did not report the unsupported offsets");
    Require(rejectedDecoded == decoded, "ucnv_toUnicode moved the target after rejecting offsets");
    Require(rejectedBytes == encoded, "ucnv_toUnicode moved the source after rejecting offsets");

    // ICU reads the caller's status before it looks at any argument: an incoming
    // failure means no conversion, no cursor movement and no destination writes.
    {
        char destination[8];
        FillSentinel(destination, sizeof(destination));
        char* target = destination;
        const char16_t* source = text;
        status = BufferOverflow;
        ucnv_fromUnicode_nid_postfix(utf8, &target, destination + sizeof(destination), &source, text + 2,
            nullptr, 1, &status);
        Require(status == BufferOverflow, "ucnv_fromUnicode overwrote an incoming overflow status");
        Require(target == destination, "ucnv_fromUnicode moved the target under an incoming failure");
        Require(source == text, "ucnv_fromUnicode moved the source under an incoming failure");
        RequireByteSentinel(destination, sizeof(destination), "ucnv_fromUnicode wrote under an incoming failure");
    }
    {
        char destination[8];
        FillSentinel(destination, sizeof(destination));
        char* target = destination;
        const char16_t* source = text;
        status = BufferOverflow;
        ucnv_fromUnicode_nid_postfix(nullptr, &target, destination + sizeof(destination), &source, text + 2,
            nullptr, 1, &status);
        Require(status == BufferOverflow, "ucnv_fromUnicode replaced an incoming failure with an argument error");
        Require(target == destination, "ucnv_fromUnicode moved the target when the incoming status had failed");
        Require(source == text, "ucnv_fromUnicode moved the source when the incoming status had failed");
        RequireByteSentinel(destination, sizeof(destination),
            "ucnv_fromUnicode wrote when the incoming status had failed");
    }
    {
        char16_t destination[8];
        for (auto& unit : destination) unit = UnitSentinel;
        char16_t* target = destination;
        const char* source = encoded;
        status = InvalidCharacter;
        ucnv_toUnicode_nid_postfix(nullptr, &target, destination + 8, &source, encoded + 2, nullptr, 1, &status);
        Require(status == InvalidCharacter, "ucnv_toUnicode replaced an incoming failure with an argument error");
        Require(target == destination, "ucnv_toUnicode moved the target when the incoming status had failed");
        Require(source == encoded, "ucnv_toUnicode moved the source when the incoming status had failed");
        for (const auto unit : destination) {
            Require(unit == UnitSentinel, "ucnv_toUnicode wrote when the incoming status had failed");
        }
    }

    // A target that fills after two bytes reports ICU's overflow: the two bytes
    // before the limit are kept and both cursors move by what ICU consumed.
    {
        std::vector<char16_t> source(52, u'A');
        char destination[8];
        FillSentinel(destination, sizeof(destination));
        char* target = destination;
        const char16_t* cursor = source.data();
        status = ZeroError;
        ucnv_fromUnicode_nid_postfix(utf8, &target, destination + 2, &cursor, source.data() + source.size(),
            nullptr, 1, &status);
        Require(status == BufferOverflow, "ucnv_fromUnicode did not report the full target as an overflow");
        Require(target == destination + 2, "ucnv_fromUnicode did not keep the two bytes before the target limit");
        Require(cursor == source.data() + 2, "ucnv_fromUnicode did not consume the two units that were converted");
        Require(destination[0] == 'A' && destination[1] == 'A', "ucnv_fromUnicode lost the partial prefix");
        RequireByteSentinel(destination + 2, sizeof(destination) - 2, "ucnv_fromUnicode wrote past the target");
    }

    // More source than one staging chunk, with a target that can take all of it:
    // one call must convert everything, as ICU does, not report a staging overflow
    // after 4096 units.
    {
        std::vector<char16_t> source(5000, u'A');
        std::vector<char> destination(8192, ByteSentinel);
        char* target = destination.data();
        const char16_t* cursor = source.data();
        status = ZeroError;
        ucnv_fromUnicode_nid_postfix(utf8, &target, destination.data() + destination.size(), &cursor,
            source.data() + source.size(), nullptr, 1, &status);
        Require(status == ZeroError, "ucnv_fromUnicode reported a staging-limit overflow");
        Require(cursor == source.data() + source.size(), "ucnv_fromUnicode did not consume the whole source");
        Require(target == destination.data() + 5000, "ucnv_fromUnicode did not write the whole output");
        Require(destination[0] == 'A' && destination[4999] == 'A', "ucnv_fromUnicode lost the converted bytes");
        RequireByteSentinel(destination.data() + 5000, destination.size() - 5000,
            "ucnv_fromUnicode wrote past the converted output");
    }

    // The to-Unicode side with the same shape: 50 source bytes fill a two-unit
    // target, and 5000 source bytes with a roomy target convert in one call.
    {
        std::vector<char> source(50, 'A');
        char16_t destination[8];
        for (auto& unit : destination) unit = UnitSentinel;
        char16_t* target = destination;
        const char* cursor = source.data();
        status = ZeroError;
        ucnv_toUnicode_nid_postfix(utf8, &target, destination + 2, &cursor, source.data() + source.size(),
            nullptr, 1, &status);
        Require(status == BufferOverflow, "ucnv_toUnicode did not report the full target as an overflow");
        Require(target == destination + 2, "ucnv_toUnicode did not keep the two units before the target limit");
        Require(cursor == source.data() + 2, "ucnv_toUnicode did not consume the two bytes that were decoded");
        Require(destination[0] == u'A' && destination[1] == u'A', "ucnv_toUnicode lost the partial prefix");
        for (std::size_t index = 2; index < 8; ++index) {
            Require(destination[index] == UnitSentinel, "ucnv_toUnicode wrote past the target");
        }
    }
    {
        std::vector<char> source(5000, 'A');
        std::vector<char16_t> destination(5001, UnitSentinel);
        char16_t* target = destination.data();
        const char* cursor = source.data();
        status = ZeroError;
        ucnv_toUnicode_nid_postfix(utf8, &target, destination.data() + 5000, &cursor, source.data() + source.size(),
            nullptr, 1, &status);
        Require(status == ZeroError, "ucnv_toUnicode reported a staging-limit overflow");
        Require(cursor == source.data() + source.size(), "ucnv_toUnicode did not consume the whole source");
        Require(target == destination.data() + 5000, "ucnv_toUnicode did not write the whole output");
        Require(destination[0] == u'A' && destination[4999] == u'A', "ucnv_toUnicode lost the decoded units");
        Require(destination[5000] == UnitSentinel, "ucnv_toUnicode wrote past the converted output");
    }

    ucnv_close_nid_postfix(utf8);
    return 0;
}
