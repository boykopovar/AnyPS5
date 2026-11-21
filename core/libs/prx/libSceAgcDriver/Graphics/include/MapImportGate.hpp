#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_MAPIMPORTGATE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_MAPIMPORTGATE_HPP

#include <bitset>
#include <cstdint>

namespace AgcDriver::Graphics::MapImportGate {

constexpr std::uint32_t StripeShift = 21;
constexpr std::uint32_t StripeCount = 256;
using Stripes = std::bitset<StripeCount>;

Stripes StripesOf(std::uint64_t begin, std::uint64_t end);
bool LockOrderChecked();
void NoteLeafLock() noexcept;
void NoteLeafUnlock() noexcept;
void RequireNoLeafLockHeld(const char* where);

template<typename TMutex> class LeafMutex {
public:
    void lock() noexcept {
        mutex.lock();
        NoteLeafLock();
    }

    bool try_lock() noexcept {
        if (!mutex.try_lock()) return false;
        NoteLeafLock();
        return true;
    }

    void unlock() noexcept {
        NoteLeafUnlock();
        mutex.unlock();
    }

private:
    TMutex mutex;
};

class PublishHold {
public:
    PublishHold(std::uint64_t address, std::uint64_t bytes);
    ~PublishHold();
    PublishHold(const PublishHold&) = delete;
    PublishHold& operator=(const PublishHold&) = delete;

private:
    enum class Mode : std::uint8_t { Nested, Fast, Striped };
    Mode mode = Mode::Nested;
    Stripes added;
};

class ImportHold {
public:
    explicit ImportHold(const Stripes& stripes);
    ~ImportHold();
    ImportHold(const ImportHold&) = delete;
    ImportHold& operator=(const ImportHold&) = delete;
    bool Covers(std::uint64_t begin, std::uint64_t end) const;
    const Stripes& Held() const { return held; }

private:
    Stripes held;
};

struct Counters {
    std::uint64_t fastPublications = 0;
    std::uint64_t stripedPublications = 0;
    std::uint64_t waitedPublications = 0;
    std::uint64_t importHolds = 0;
    std::uint64_t drainWaits = 0;
    std::uint64_t retries = 0;
};

Counters CountersForTest();
std::uint32_t WaitingPublishersForTest();
std::uint32_t DrainingImportsForTest();
void CountRetry() noexcept;
void SetUnlockedHookForTest(void (*hook)());
void RunUnlockedHookForTest();

}

#endif
