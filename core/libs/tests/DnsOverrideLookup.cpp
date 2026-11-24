#include "common/DnsOverride.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "dns override lookup: %s\n", message);
    std::abort();
}

void RequireHit(const DnsOverride::Table& table, const char* hostname, DnsOverride::Address expected) {
    DnsOverride::Address address{};
    Require(table.Find(hostname, address), "expected override missing");
    Require(address == expected, "override address changed");
}

void RequireMiss(const DnsOverride::Table& table, const char* hostname) {
    DnsOverride::Address address{9, 9, 9, 9};
    Require(!table.Find(hostname, address), "unexpected override");
    Require(address == (DnsOverride::Address{9, 9, 9, 9}), "miss changed the address");
}
}

int main() {
    const DnsOverride::Table table(DnsOverride::Parse("Example.COM=192.0.2.7,api.test=10.0.0.2,api.test=10.0.0.3,bad"));
    RequireHit(table, "example.com", {192, 0, 2, 7});
    RequireHit(table, "EXAMPLE.Com", {192, 0, 2, 7});
    RequireHit(table, "api.TEST", {10, 0, 0, 2});
    RequireMiss(table, "sub.example.com");
    RequireMiss(table, "example.com.");
    RequireMiss(table, "example");
    RequireMiss(table, "");
    RequireMiss(table, nullptr);
    RequireMiss(DnsOverride::Table(DnsOverride::Parse(nullptr)), "example.com");

#ifdef _WIN32
    Require(_putenv_s("ANYPS5_DNS_OVERRIDE", "env.test=198.51.100.9,bad entry") == 0, "putenv failed");
#else
    Require(setenv("ANYPS5_DNS_OVERRIDE", "env.test=198.51.100.9,bad entry", 1) == 0, "setenv failed");
#endif
    DnsOverride::Address address{};
    Require(DnsOverride::LookupOverride("ENV.test", address), "environment override missing");
    Require(address == (DnsOverride::Address{198, 51, 100, 9}), "environment override address changed");
    Require(!DnsOverride::LookupOverride("other.test", address), "environment override matched another name");
    return 0;
}
