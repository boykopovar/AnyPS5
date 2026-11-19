#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace DnsOverride {
using Address = std::array<std::uint8_t, 4>;

struct Entry {
    std::string hostname;
    Address address{};
};

struct Parsed {
    std::vector<Entry> entries;
    std::vector<std::string> malformed;
};

Parsed Parse(const char* text);

class Table {
public:
    explicit Table(const Parsed& parsed);
    bool Find(const char* hostname, Address& address) const;

private:
    std::vector<Entry> entries;
};

bool LookupOverride(const char* hostname, Address& address);
}
