#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_QUEUES_REGISTERLISTS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_QUEUES_REGISTERLISTS_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace AgcDriver::DriverDetail {

struct RegisterListRef {
    std::uint32_t cursor = 0;
    std::uint32_t first = 0;
    std::uint32_t pairs = 0;
    std::uint32_t flags = 0;
};

struct RegisterListView {
    std::span<const std::uint32_t> pairs;
    bool valid = false;
};

class RegisterLists {
public:
    static constexpr std::uint32_t Valid = 1;
    static constexpr std::uint32_t Cached = 2;

    std::vector<std::uint32_t> words;
    std::vector<RegisterListRef> refs;

    void Clear() {
        words.clear();
        refs.clear();
    }
    RegisterListView View(const RegisterListRef& ref) const { return {std::span<const std::uint32_t>(words).subspan(ref.first, std::size_t{ref.pairs} * 2), (ref.flags & Valid) != 0}; }
    const RegisterListRef* Find(std::size_t cursor) const;
    const RegisterListRef* Next(std::size_t cursor, std::size_t& hint) const;
};

struct Submission;

RegisterListView RegisterListAt(const Submission& submission, std::size_t cursor, std::size_t& hint);
std::optional<RegisterListView> FindRegisterList(const Submission& submission, std::size_t cursor);

}

#endif
