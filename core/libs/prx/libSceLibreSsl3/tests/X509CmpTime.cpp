#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <source_location>
#include <stdexcept>
#include <string_view>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509_cmp_current_time check failed at line %u\n", location.line());
    std::abort();
}

Asn1String Time(std::string_view text, int type) {
    return {static_cast<int>(text.size()), type,
        reinterpret_cast<unsigned char*>(const_cast<char*>(text.data())), 0};
}

Asn1String Current(std::time_t stamp, std::array<char, 16>& storage) {
    std::tm parts{};
#ifdef _WIN32
    gmtime_s(&parts, &stamp);
#else
    gmtime_r(&stamp, &parts);
#endif
    const int year = parts.tm_year + 1900;
    if (year >= 1950 && year < 2050) {
        std::snprintf(storage.data(), storage.size(), "%02d%02d%02d%02d%02d%02dZ",
            year % 100, parts.tm_mon + 1, parts.tm_mday, parts.tm_hour, parts.tm_min, parts.tm_sec);
        return {13, 23, reinterpret_cast<unsigned char*>(storage.data()), 0};
    }
    std::snprintf(storage.data(), storage.size(), "%04d%02d%02d%02d%02d%02dZ",
        year, parts.tm_mon + 1, parts.tm_mday, parts.tm_hour, parts.tm_min, parts.tm_sec);
    return {15, 24, reinterpret_cast<unsigned char*>(storage.data()), 0};
}

void Check(std::string_view text, int type, int expected,
           std::source_location location = std::source_location::current()) {
    Asn1String time = Time(text, type);
    Require(X509_cmp_current_time_nid_postfix(&time) == expected, location);
}

void CheckNotZero(std::string_view text, int type,
                  std::source_location location = std::source_location::current()) {
    Asn1String time = Time(text, type);
    Require(X509_cmp_current_time_nid_postfix(&time) != 0, location);
}

}

int main() {
    const std::time_t now = std::time(nullptr);

    std::array<char, 16> equalStorage{};
    Asn1String equal = Current(now, equalStorage);
    Require(X509_cmp_current_time_nid_postfix(&equal) == -1);

    std::array<char, 16> futureStorage{};
    Asn1String future = Current(now + 3600, futureStorage);
    Require(X509_cmp_current_time_nid_postfix(&future) == 1);

    std::array<char, 16> pastStorage{};
    Asn1String past = Current(now - 3600, pastStorage);
    Require(X509_cmp_current_time_nid_postfix(&past) == -1);

    Check("20200101000000Z", 24, 0);
    Check("20200101000000Z", 23, 0);
    Check("20200101000000Z", 0, 0);
    Check("99991231235959Z", 24, 1);
    Check("99991231235959Z", 0, 1);
    Check("500101000000Z", 23, -1);
    Check("500101000000Z", 0, -1);
    Check("500101000000Z", 99, -1);
    Check("500101000000Z", 24, 0);
    Check("200101000000Z", 23, -1);
    Check("200101000000Z", 0, -1);
    Check("200101000000Z", 24, 0);
    Check("200101000000Z", 4, -1);
    Check("200229120000Z", 23, -1);
    Check("491231235959Z", 23, 1);

    Check("691231235959Z", 23, 0);
    Check("700101000000Z", 23, -1);
    Check("000231000000Z", 23, -1);
    Check("000431000000Z", 23, -1);
    Check("210229000000Z", 23, -1);
    Check("260231000000Z", 23, -1);
    Check("20491231235959Z", 24, 0);
    CheckNotZero("491231235959Z", 23);
    Check("20500101000000Z", 24, 1);
    Check("20500101000000Z", 23, 0);

    for (const auto text : {"", "20000101000000", "20000101000000z", "200001010000000Z",
             "20000001000000Z", "20001301000000Z", "20000100000000Z", "20000132000000Z",
             "20000101240000Z", "20000101006000Z", "20000101000060Z", "200001010000/0Z",
             "205001010000000Z"})
        Check(text, 24, 0);

    for (const auto text : {"200001010000Z", "201301000000Z", "200001000000Z", "200101240000Z",
             "200101006000Z", "200101000060Z", "200101000000X", "2001010000Z",
             "20010100000Z", "20010100000aZ", "2001010000+0Z"})
        Check(text, 23, 0);

    const unsigned char digit = '0';
    Asn1String missing{13, 23, nullptr, 0};
    Require(X509_cmp_current_time_nid_postfix(&missing) == 0);
    Asn1String negative{-13, 23, const_cast<unsigned char*>(&digit), 0};
    Require(X509_cmp_current_time_nid_postfix(&negative) == 0);

    bool threw = false;
    try {
        X509_cmp_current_time_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    std::puts("libSceLibreSsl3 X509_cmp_current_time tests passed");
}
