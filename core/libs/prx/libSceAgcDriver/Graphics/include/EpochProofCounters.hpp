#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_EPOCHPROOFCOUNTERS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_EPOCHPROOFCOUNTERS_HPP

#include "prx/libSceAgcDriver/Execution/include/ProfileCounter.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace AgcDriver::Graphics {

struct EpochProofCounters {
    ProfileCounter resourceReuses;
    ProfileCounter resourceProofs;
    ProfileCounter storageReuses;
    ProfileCounter mirrorReuses;
    ProfileCounter mirrorRefreshes;
};

enum class ProofWork : std::uint8_t { CpuWriteSync, StorageUpload, Count };

class ProofWorkTrace {
public:
    ProofWorkTrace();
    ~ProofWorkTrace();
    ProofWorkTrace(const ProofWorkTrace&) = delete;
    ProofWorkTrace& operator=(const ProofWorkTrace&) = delete;
    static bool Active();
    static void Note(ProofWork work);
    std::uint64_t Count(ProofWork work) const { return counts[static_cast<std::size_t>(work)]; }
    std::uint64_t Total() const;

private:
    std::array<std::uint64_t, static_cast<std::size_t>(ProofWork::Count)> counts{};
    ProofWorkTrace* previous = nullptr;
};

EpochProofCounters& EpochProofCounts();
bool EpochProofCountsProfiled();
void ReportEpochProofCounts();

}

#endif
