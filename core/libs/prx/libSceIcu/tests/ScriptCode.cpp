#include "prx/libSceIcu/ScriptCode.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

constexpr std::int32_t Success = 0;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t MemoryAllocation = 7;
constexpr std::int32_t Sentinel = -7;
constexpr std::int32_t Common = 0;
constexpr std::int32_t Latin = 25;
constexpr std::int32_t Han = 17;
constexpr std::int32_t Hangul = 18;
constexpr std::int32_t Hiragana = 20;
constexpr std::int32_t Katakana = 22;
constexpr std::int32_t Bopomofo = 5;
constexpr std::int32_t SimplifiedHan = 73;

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

void ExpectCodes(const char* name, std::int32_t capacity, std::int32_t expectedLength, const std::int32_t* expected,
                 std::int32_t expectedStatus, const char* what) {
    std::int32_t buffer[4] = {Sentinel, Sentinel, Sentinel, Sentinel};
    std::int32_t status = Success;
    ExpectInt(uscript_getCode_nid_postfix(name, buffer, capacity, &status), expectedLength, what);
    ExpectInt(status, expectedStatus, what);
    for (std::int32_t i = 0; i < capacity; ++i) {
        ExpectInt(buffer[i], expected[i], what);
    }
    for (std::int32_t i = capacity; i < 4; ++i) {
        ExpectInt(buffer[i], Sentinel, what);
    }
}

void CheckScriptNames() {
    std::int32_t buffer[2] = {Sentinel, Sentinel};
    std::int32_t status = Success;
    ExpectInt(uscript_getCode_nid_postfix("Latn", buffer, 1, &status), 1, "Latn");
    ExpectInt(status, Success, "Latn status");
    ExpectInt(buffer[0], Latin, "Latn code");
    ExpectInt(buffer[1], Sentinel, "Latn untouched");

    status = Success;
    ExpectInt(uscript_getCode_nid_postfix("Latin", buffer, 2, &status), 1, "Latin");
    ExpectInt(status, Success, "Latin status");
    ExpectInt(buffer[0], Latin, "Latin code");
    ExpectInt(buffer[1], Sentinel, "Latin untouched after capacity 2");

    status = Success;
    buffer[0] = Sentinel;
    ExpectInt(uscript_getCode_nid_postfix("Common", buffer, 1, &status), 1, "Common");
    ExpectInt(status, Success, "Common status");
    ExpectInt(buffer[0], Common, "Common code");

    status = Success;
    buffer[0] = Sentinel;
    ExpectInt(uscript_getCode_nid_postfix("Hani", buffer, 1, &status), 1, "Hani");
    ExpectInt(buffer[0], Han, "Hani code");

    status = Success;
    buffer[0] = Sentinel;
    ExpectInt(uscript_getCode_nid_postfix("Hans", buffer, 1, &status), 1, "Hans");
    ExpectInt(status, Success, "Hans status");
    ExpectInt(buffer[0], SimplifiedHan, "Hans code is not folded for a direct name");

    status = Success;
    buffer[0] = Sentinel;
    ExpectInt(uscript_getCode_nid_postfix("Latn", buffer, 0, &status), 1, "Latn capacity zero");
    ExpectInt(status, BufferOverflow, "Latn capacity zero status");
    ExpectInt(buffer[0], Sentinel, "Latn capacity zero untouched");

    status = Success;
    ExpectInt(uscript_getCode_nid_postfix("Latn", nullptr, 0, &status), 1, "Latn null destination");
    ExpectInt(status, BufferOverflow, "Latn null destination status");
}

void CheckLocaleNames() {
    constexpr std::int32_t japanese[] = {Katakana, Hiragana, Han};
    constexpr std::int32_t korean[] = {Hangul, Han};
    constexpr std::int32_t hanBopomofo[] = {Han, Bopomofo};
    constexpr std::int32_t latinOnly[] = {Latin};
    constexpr std::int32_t hanOnly[] = {Han};

    ExpectCodes("ja", 3, 3, japanese, Success, "ja");
    ExpectCodes("ko", 2, 2, korean, Success, "ko");
    ExpectCodes("zh_Hant", 2, 2, hanBopomofo, Success, "zh_Hant");
    ExpectCodes("en_Latn", 1, 1, latinOnly, Success, "en_Latn");
    ExpectCodes("zh_Hans", 1, 1, hanOnly, Success, "zh_Hans folds to Han");
    ExpectCodes("ja-JP", 3, 3, japanese, Success, "ja-JP");

    std::int32_t buffer[4] = {Sentinel, Sentinel, Sentinel, Sentinel};
    std::int32_t status = Success;
    ExpectInt(uscript_getCode_nid_postfix("ja", buffer, 2, &status), 3, "ja short capacity");
    ExpectInt(status, BufferOverflow, "ja short capacity status");
    ExpectInt(buffer[0], Sentinel, "ja short capacity untouched");
    ExpectInt(buffer[1], Sentinel, "ja short capacity untouched");

    status = Success;
    ExpectInt(uscript_getCode_nid_postfix("zh-Hant-TW", buffer, 1, &status), 2, "zh-Hant-TW short capacity");
    ExpectInt(status, BufferOverflow, "zh-Hant-TW short capacity status");
    ExpectInt(buffer[0], Sentinel, "zh-Hant-TW short capacity untouched");

    status = Success;
    ExpectInt(uscript_getCode_nid_postfix("ko", nullptr, 0, &status), 2, "ko null destination");
    ExpectInt(status, BufferOverflow, "ko null destination status");
}

void CheckArgumentErrors() {
    std::int32_t buffer[2] = {Sentinel, Sentinel};
    std::int32_t status = Success;
    ExpectInt(uscript_getCode_nid_postfix(nullptr, buffer, 1, &status), 0, "null name");
    ExpectInt(status, IllegalArgument, "null name status");
    ExpectInt(buffer[0], Sentinel, "null name untouched");

    status = Success;
    ExpectInt(uscript_getCode_nid_postfix("Latn", nullptr, 1, &status), 0, "null destination with capacity");
    ExpectInt(status, IllegalArgument, "null destination with capacity status");

    status = Success;
    ExpectInt(uscript_getCode_nid_postfix("Latn", buffer, -1, &status), 0, "negative capacity");
    ExpectInt(status, IllegalArgument, "negative capacity status");
    ExpectInt(buffer[0], Sentinel, "negative capacity untouched");

    status = IllegalArgument;
    ExpectInt(uscript_getCode_nid_postfix("Latn", buffer, 1, &status), 0, "incoming failure");
    ExpectInt(status, IllegalArgument, "incoming failure status unchanged");
    ExpectInt(buffer[0], Sentinel, "incoming failure untouched");

    status = MemoryAllocation;
    ExpectInt(uscript_getCode_nid_postfix("ja", buffer, 2, &status), 0, "incoming failure with locale");
    ExpectInt(status, MemoryAllocation, "incoming failure with locale status unchanged");
    ExpectInt(buffer[0], Sentinel, "incoming failure with locale untouched");

    ExpectThrow<std::invalid_argument>([] { uscript_getCode_nid_postfix("Latn", nullptr, 0, nullptr); }, "null error pointer");
}

void CheckUnresolvedNames() {
    std::int32_t buffer[1] = {Sentinel};
    std::int32_t status = Success;
    ExpectThrow<std::runtime_error>([&] { uscript_getCode_nid_postfix("en_US", buffer, 1, &status); }, "en_US needs likely subtags");
    ExpectThrow<std::runtime_error>([&] { uscript_getCode_nid_postfix("Garay", buffer, 1, &status); }, "Garay needs data");
    ExpectThrow<std::runtime_error>([&] { uscript_getCode_nid_postfix("Hans_CN", buffer, 1, &status); }, "Hans_CN needs likely subtags");
    ExpectInt(status, Success, "unresolved status unchanged");
    ExpectInt(buffer[0], Sentinel, "unresolved output untouched");
}

}

int main() {
    CheckScriptNames();
    CheckLocaleNames();
    CheckArgumentErrors();
    CheckUnresolvedNames();
    return 0;
}
