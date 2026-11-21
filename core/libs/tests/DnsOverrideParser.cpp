#include "common/DnsOverride.hpp"
#include <cstdio>
#include <cstdlib>

namespace {
void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "dns override parser: %s\n", message);
    std::abort();
}

void RequireEntry(const DnsOverride::Parsed& parsed, std::size_t index, const char* hostname,
    DnsOverride::Address address) {
    Require(index < parsed.entries.size(), "entry missing");
    Require(parsed.entries[index].hostname == hostname, "hostname changed");
    Require(parsed.entries[index].address == address, "address changed");
}

void RequireMalformed(const char* text, std::size_t count) {
    const auto parsed = DnsOverride::Parse(text);
    Require(parsed.entries.empty(), "malformed input produced an entry");
    Require(parsed.malformed.size() == count, "malformed count differs");
}
}

int main() {
    Require(DnsOverride::Parse(nullptr).entries.empty(), "null text produced entries");
    Require(DnsOverride::Parse("").entries.empty() && DnsOverride::Parse("").malformed.empty(), "empty text is not empty");

    const auto single = DnsOverride::Parse("Example.COM=192.0.2.7");
    Require(single.entries.size() == 1 && single.malformed.empty(), "single entry not parsed");
    RequireEntry(single, 0, "example.com", {192, 0, 2, 7});

    const auto many = DnsOverride::Parse("a.test=10.0.0.1,b.test=0.0.0.0,c.test=255.255.255.255");
    Require(many.entries.size() == 3 && many.malformed.empty(), "comma list not parsed");
    RequireEntry(many, 0, "a.test", {10, 0, 0, 1});
    RequireEntry(many, 1, "b.test", {0, 0, 0, 0});
    RequireEntry(many, 2, "c.test", {255, 255, 255, 255});

    const auto mixed = DnsOverride::Parse("good.test=1.2.3.4,bad,also.bad=1.2.3,ok.test=5.6.7.8");
    Require(mixed.entries.size() == 2 && mixed.malformed.size() == 2, "valid entries lost beside malformed ones");
    RequireEntry(mixed, 0, "good.test", {1, 2, 3, 4});
    RequireEntry(mixed, 1, "ok.test", {5, 6, 7, 8});
    Require(mixed.malformed[0] == "bad" && mixed.malformed[1] == "also.bad=1.2.3", "malformed text changed");

    RequireMalformed("=1.2.3.4", 1);
    RequireMalformed("host.test=", 1);
    RequireMalformed("host.test", 1);
    RequireMalformed("host.test=1.2.3", 1);
    RequireMalformed("host.test=1.2.3.4.5", 1);
    RequireMalformed("host.test=256.0.0.1", 1);
    RequireMalformed("host.test=1.2.3.04", 1);
    RequireMalformed("host.test=1..2.3", 1);
    RequireMalformed("host.test=1.2.3.4 ", 1);
    RequireMalformed("host.test=::1", 1);
    RequireMalformed("host.test=1234.0.0.1", 1);
    RequireMalformed("bad host=1.2.3.4", 1);
    RequireMalformed(",", 2);
    const auto trailing = DnsOverride::Parse("a.test=1.2.3.4,");
    Require(trailing.entries.size() == 1 && trailing.malformed.size() == 1, "trailing comma not reported");
    RequireEntry(trailing, 0, "a.test", {1, 2, 3, 4});
    return 0;
}
