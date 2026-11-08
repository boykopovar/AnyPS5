#include "prx/libc/include/DebugNidBreakpoints.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <mutex>

namespace {

constexpr char kAlphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";

int DecodeChar(char c) {
    for (int i = 0; i < 64; ++i) {
        if (kAlphabet[i] == c)
            return i;
    }
    return -1;
}

void ReportMalformedEntry(std::string_view entry) {
    std::fprintf(stderr, "ANYPS5_DEBUG_NIDS: ignored malformed entry '%.*s'\n",
                 static_cast<int>(entry.size()), entry.data());
}

}

namespace Debug {

bool ParseNid(std::string_view text, std::uint64_t& nid) {
    if (text.size() != 11)
        return false;

    std::array<int, 11> digits{};
    for (std::size_t i = 0; i < digits.size(); ++i) {
        digits[i] = DecodeChar(text[i]);
        if (digits[i] < 0)
            return false;
    }

    std::array<std::uint8_t, 8> bytes{};
    for (std::size_t group = 0; group < 2; ++group) {
        const std::size_t c = group * 4;
        const auto triple = static_cast<std::uint32_t>((digits[c] << 18) | (digits[c + 1] << 12) |
                                                       (digits[c + 2] << 6) | digits[c + 3]);
        bytes[group * 3] = static_cast<std::uint8_t>(triple >> 16);
        bytes[group * 3 + 1] = static_cast<std::uint8_t>(triple >> 8);
        bytes[group * 3 + 2] = static_cast<std::uint8_t>(triple);
    }

    const auto tail = static_cast<std::uint32_t>((digits[8] << 12) | (digits[9] << 6) | digits[10]);
    if ((tail & 3u) != 0)
        return false;
    bytes[6] = static_cast<std::uint8_t>(tail >> 10);
    bytes[7] = static_cast<std::uint8_t>(tail >> 2);

    nid = 0;
    for (std::uint8_t byte : bytes)
        nid = (nid << 8) | byte;
    return true;
}

std::string FormatNid(std::uint64_t nid) {
    std::array<std::uint8_t, 8> bytes{};
    for (std::size_t i = 0; i < bytes.size(); ++i)
        bytes[i] = static_cast<std::uint8_t>(nid >> (56 - 8 * i));

    std::string text;
    text.reserve(11);
    for (std::size_t group = 0; group < 2; ++group) {
        const auto triple = (static_cast<std::uint32_t>(bytes[group * 3]) << 16) |
                            (static_cast<std::uint32_t>(bytes[group * 3 + 1]) << 8) |
                            static_cast<std::uint32_t>(bytes[group * 3 + 2]);
        text.push_back(kAlphabet[(triple >> 18) & 0x3F]);
        text.push_back(kAlphabet[(triple >> 12) & 0x3F]);
        text.push_back(kAlphabet[(triple >> 6) & 0x3F]);
        text.push_back(kAlphabet[triple & 0x3F]);
    }
    const auto tail = (static_cast<std::uint32_t>(bytes[6]) << 16) | (static_cast<std::uint32_t>(bytes[7]) << 8);
    text.push_back(kAlphabet[(tail >> 18) & 0x3F]);
    text.push_back(kAlphabet[(tail >> 12) & 0x3F]);
    text.push_back(kAlphabet[(tail >> 6) & 0x3F]);
    return text;
}

NidBreakpointTable::AddResult NidBreakpointTable::Add(std::uint64_t nid) {
    std::lock_guard lock(mutex_);
    const std::size_t count = count_.load(std::memory_order_relaxed);
    const std::size_t index = IndexOf(nid);
    if (index != count) {
        ++duplicates_;
        return AddResult::Duplicate;
    }
    if (count == kCapacity)
        return AddResult::Full;
    nids_[count].store(nid, std::memory_order_relaxed);
    count_.store(count + 1, std::memory_order_release);
    return AddResult::Added;
}

std::size_t NidBreakpointTable::Size() const {
    return count_.load(std::memory_order_acquire);
}

std::size_t NidBreakpointTable::List(std::uint64_t* out, std::size_t capacity) const {
    std::lock_guard lock(mutex_);
    const std::size_t count = count_.load(std::memory_order_relaxed);
    const std::size_t written = count < capacity ? count : capacity;
    for (std::size_t i = 0; i < written; ++i)
        out[i] = nids_[i].load(std::memory_order_relaxed);
    return written;
}

bool NidBreakpointTable::Empty() const {
    return count_.load(std::memory_order_acquire) == 0;
}

bool NidBreakpointTable::IsArmed(std::uint64_t nid) const {
    if (Empty())
        return false;
    const std::size_t count = count_.load(std::memory_order_acquire);
    for (std::size_t i = 0; i < count; ++i) {
        if (nids_[i].load(std::memory_order_relaxed) == nid)
            return true;
    }
    return false;
}

std::uint64_t NidBreakpointTable::DuplicateCount() const {
    std::lock_guard lock(mutex_);
    return duplicates_;
}

bool NidBreakpointTable::Remove(std::uint64_t nid) {
    std::lock_guard lock(mutex_);
    const std::size_t count = count_.load(std::memory_order_relaxed);
    const std::size_t index = IndexOf(nid);
    if (index == count)
        return false;
    nids_[index].store(nids_[count - 1].load(std::memory_order_relaxed), std::memory_order_relaxed);
    count_.store(count - 1, std::memory_order_release);
    return true;
}

std::size_t NidBreakpointTable::IndexOf(std::uint64_t nid) const {
    const std::size_t count = count_.load(std::memory_order_relaxed);
    for (std::size_t i = 0; i < count; ++i) {
        if (nids_[i].load(std::memory_order_relaxed) == nid)
            return i;
    }
    return count;
}

NidBreakpointTable& NidBreakpoints() {
    static NidBreakpointTable table;
    return table;
}

void LoadNidBreakpointsFromEnvironment() {
    static std::once_flag once;
    std::call_once(once, [] {
        const char* value = std::getenv("ANYPS5_DEBUG_NIDS");
        if (value == nullptr || *value == '\0')
            return;
        AddNidList(value, NidBreakpoints(), ReportMalformedEntry);
    });
}

std::size_t AddNidList(std::string_view list, NidBreakpointTable& table, MalformedEntryReport report) {
    std::size_t added = 0;
    while (!list.empty()) {
        const std::size_t comma = list.find(',');
        std::string_view entry = list.substr(0, comma);
        list = comma == std::string_view::npos ? std::string_view{} : list.substr(comma + 1);

        while (!entry.empty() && (entry.front() == ' ' || entry.front() == '\t'))
            entry.remove_prefix(1);
        while (!entry.empty() && (entry.back() == ' ' || entry.back() == '\t'))
            entry.remove_suffix(1);
        if (entry.empty())
            continue;

        std::uint64_t nid = 0;
        if (!ParseNid(entry, nid)) {
            if (report != nullptr)
                report(entry);
            continue;
        }
        if (table.Add(nid) == NidBreakpointTable::AddResult::Added)
            ++added;
    }
    return added;
}

}
