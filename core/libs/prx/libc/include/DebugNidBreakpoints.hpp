#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_DEBUGNIDBREAKPOINTS_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_DEBUGNIDBREAKPOINTS_HPP

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>

namespace Debug {

bool ParseNid(std::string_view text, std::uint64_t& nid);
std::string FormatNid(std::uint64_t nid);

class NidBreakpointTable {
public:
    static constexpr std::size_t kCapacity = 256;

    enum class AddResult { Added, Duplicate, Full };

    AddResult Add(std::uint64_t nid);
    bool Remove(std::uint64_t nid);
    std::size_t Size() const;
    bool Empty() const;
    bool IsArmed(std::uint64_t nid) const;
    std::uint64_t DuplicateCount() const;
    std::size_t List(std::uint64_t* out, std::size_t capacity) const;

private:
    std::size_t IndexOf(std::uint64_t nid) const;

    mutable std::mutex mutex_;
    std::array<std::atomic<std::uint64_t>, kCapacity> nids_{};
    std::atomic<std::size_t> count_{0};
    std::uint64_t duplicates_ = 0;
};

NidBreakpointTable& NidBreakpoints();

using MalformedEntryReport = void (*)(std::string_view entry);

std::size_t AddNidList(std::string_view list, NidBreakpointTable& table, MalformedEntryReport report);

void LoadNidBreakpointsFromEnvironment();

}

#endif
