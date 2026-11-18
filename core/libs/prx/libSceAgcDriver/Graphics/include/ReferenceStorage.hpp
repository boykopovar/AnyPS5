#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_REFERENCESTORAGE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_REFERENCESTORAGE_HPP

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace AgcDriver::Graphics {

class ReferenceStorage {
public:
    static constexpr std::size_t Slots = 8;

    explicit ReferenceStorage(std::size_t capacityBytes) noexcept;
    ReferenceStorage(const ReferenceStorage&) = delete;
    ReferenceStorage& operator=(const ReferenceStorage&) = delete;

    using Clock = std::chrono::steady_clock;
    static constexpr std::chrono::milliseconds IdleLimit{2000};

    std::vector<std::byte> Take(std::size_t bytes, Clock::time_point now = Clock::now()) noexcept;
    void Return(std::vector<std::byte>&& storage, Clock::time_point now = Clock::now()) noexcept;
    std::size_t ReleaseIdle(Clock::time_point now = Clock::now()) noexcept;
    std::size_t IdleBytes() const noexcept;
    std::size_t IdleCount() const noexcept;

    static ReferenceStorage& Shared() noexcept;

private:
    std::size_t releaseIdle(Clock::time_point now, std::array<std::vector<std::byte>, Slots>& released) noexcept;

    mutable std::mutex mutex;
    std::array<std::vector<std::byte>, Slots> idle;
    std::array<std::uint64_t, Slots> returned{};
    std::array<Clock::time_point, Slots> returnedAt{};
    std::uint64_t returns = 0;
    std::size_t bytes = 0;
    std::size_t capacityBytes;
};

}

#endif
