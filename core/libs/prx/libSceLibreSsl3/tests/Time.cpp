#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <source_location>
#include <stdexcept>
#include <string_view>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 time check failed at line %u\n", location.line());
    std::abort();
}

Asn1String Time(std::string_view text, int type = 24) {
    return {static_cast<int>(text.size()), type,
        reinterpret_cast<unsigned char*>(const_cast<char*>(text.data())), 0};
}

void Check(std::string_view from, std::string_view to, int days, int seconds) {
    auto start = Time(from);
    auto end = Time(to);
    std::array<int, 4> output{17, 18, 19, 20};
    Require(ASN1_TIME_diff_nid_postfix(&output[1], &output[2], &start, &end) == 1);
    Require(output == std::array<int, 4>{17, days, seconds, 20});
    Require(ASN1_TIME_diff_nid_postfix(&output[1], &output[2], &end, &start) == 1);
    Require(output == std::array<int, 4>{17, -days, -seconds, 20});
    Require(ASN1_TIME_diff_nid_postfix(&output[1], &output[1], &start, &end) == 1);
    Require(output[1] == days);
}

void Invalid(const Asn1String& invalid) {
    auto valid = Time("20000101000000Z");
    int days = 123;
    int seconds = 456;
    Require(ASN1_TIME_diff_nid_postfix(&days, &seconds, &invalid, &valid) == 0);
    Require(days == 123 && seconds == 456);
    Require(ASN1_TIME_diff_nid_postfix(&days, &seconds, &valid, &invalid) == 0);
    Require(days == 123 && seconds == 456);
}

void CurrentTime() {
    auto epoch = Time("19700101000000Z");
    int days;
    int seconds;
    auto before = std::time(nullptr);
    Require(ASN1_TIME_diff_nid_postfix(&days, &seconds, &epoch, nullptr) == 1);
    auto after = std::time(nullptr);
    const std::int64_t forward = static_cast<std::int64_t>(days) * 86400 + seconds;
    Require(forward >= before && forward <= after);
    before = std::time(nullptr);
    Require(ASN1_TIME_diff_nid_postfix(&days, &seconds, nullptr, &epoch) == 1);
    after = std::time(nullptr);
    const std::int64_t backward = static_cast<std::int64_t>(days) * 86400 + seconds;
    Require(-backward >= before && -backward <= after);
    before = std::time(nullptr);
    Require(ASN1_TIME_diff_nid_postfix(&days, &seconds, nullptr, nullptr) == 1);
    after = std::time(nullptr);
    const std::int64_t elapsed = static_cast<std::int64_t>(days) * 86400 + seconds;
    Require(elapsed >= 0 && elapsed <= after - before);
}

}

int main() {
    Check("19700101000000Z", "19700101000000Z", 0, 0);
    Check("19700101235959Z", "19700102000000Z", 0, 1);
    Check("19991231235959Z", "20000102000001Z", 1, 2);
    Check("19700101000000Z", "19700101235959Z", 0, 86399);
    Check("00000101000000Z", "99991231235959Z", 3652424, 86399);
    Check("00000228000000Z", "00000301000000Z", 2, 0);
    Check("500101000000Z", "19500101000000Z", 0, 0);
    Check("491231235959Z", "20500101000000Z", 0, 1);
    Check("000229000000Z", "20000301000000Z", 1, 0);
    for (int year = 0; year < 10000; ++year) {
        char from[32];
        char to[32];
        std::snprintf(from, sizeof(from), "%04d0228000000Z", year);
        std::snprintf(to, sizeof(to), "%04d0301000000Z", year);
        const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
        Check(from, to, leap ? 2 : 1, 0);
    }
    for (const auto text : {"", "200001010000Z", "20000101000000", "20000101000000z",
             "20000101000000+0000", "20000101000000.0Z", "20000101000000ZZ",
             "20000001000000Z", "20001301000000Z", "20000100000000Z", "20000132000000Z",
             "20000431000000Z", "19000229000000Z", "21000229000000Z", "20010229000000Z",
             "20000101240000Z", "20000101006000Z", "20000101000060Z", "200001010000/0Z"})
        Invalid(Time(text));
    std::array<unsigned char, 15> bytes{};
    std::memcpy(bytes.data(), "20000101000000Z", bytes.size());
    Asn1String bounded{15, -100, bytes.data(), -1};
    int days;
    int seconds;
    auto same = Time("000101000000Z", 99);
    Require(ASN1_TIME_diff_nid_postfix(&days, &seconds, &bounded, &same) == 1);
    Require(days == 0 && seconds == 0);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const auto saved = bytes[i];
        for (const auto replacement : {0, 0x80, 0xff}) {
            bytes[i] = static_cast<unsigned char>(replacement);
            Invalid(bounded);
        }
        bytes[i] = saved;
    }
    bounded.length = -1;
    Invalid(bounded);
    bounded.length = 15;
    bounded.data = nullptr;
    Invalid(bounded);
    for (bool missingDays : {false, true}) {
        bool threw = false;
        days = 123;
        seconds = 456;
        try {
            ASN1_TIME_diff_nid_postfix(missingDays ? nullptr : &days,
                missingDays ? &seconds : nullptr, &same, &same);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Require(threw && days == 123 && seconds == 456);
    }
    CurrentTime();
}
