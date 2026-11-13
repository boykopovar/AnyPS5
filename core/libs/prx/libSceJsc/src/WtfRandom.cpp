#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/HostEntropy.hpp"
#include "prx/libSceJsc/include/WtfRandomExports.hpp"

#include <atomic>
#include <cstdint>
#include <limits>
#include <mutex>

namespace {

class WeakRandom {
public:
    explicit WeakRandom(unsigned int seed) {
        if (!seed) seed = 1;
        m_low = seed;
        m_high = seed;
        Advance();
    }

    unsigned int GetUint32() {
        return static_cast<unsigned int>(Advance());
    }

private:
    static std::uint64_t NextState(std::uint64_t x, std::uint64_t y) {
        x ^= x << 23;
        x ^= x >> 17;
        x ^= y ^ (y >> 26);
        return x;
    }

    std::uint64_t Advance() {
        const std::uint64_t x = m_low;
        const std::uint64_t y = m_high;
        m_low = y;
        m_high = NextState(x, y);
        return m_high + m_low;
    }

    std::uint64_t m_low = 0;
    std::uint64_t m_high = 0;
};

std::atomic<std::uint64_t> g_identifier{0};
std::atomic<std::uint64_t> g_threadSafeIdentifier{0};

}  // namespace

extern "C" {

std::uint32_t APS5_VABI _ZN3WTF29cryptographicallyRandomNumberEv() {
    std::uint32_t value = 0;
    HostEntropy_nid_no_patch(&value, sizeof(value));
    return value;
}

double APS5_VABI _ZN3WTF12randomNumberEv() {
    const std::uint32_t bits = _ZN3WTF29cryptographicallyRandomNumberEv();
    return static_cast<double>(bits) / (static_cast<double>(std::numeric_limits<std::uint32_t>::max()) + 1.0);
}

unsigned int APS5_VABI _ZN3WTF16weakRandomUint32Ev() {
    static std::mutex mutex;
    static WeakRandom random(_ZN3WTF29cryptographicallyRandomNumberEv());
    std::lock_guard lock(mutex);
    return random.GetUint32();
}

std::uint64_t APS5_VABI _ZN3WTF20ObjectIdentifierBase26generateIdentifierInternalEv() {
    return ++g_identifier;
}

std::uint64_t APS5_VABI _ZN3WTF20ObjectIdentifierBase36generateThreadSafeIdentifierInternalEv() {
    return ++g_threadSafeIdentifier;
}

}
