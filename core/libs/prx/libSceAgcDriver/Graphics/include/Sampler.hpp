#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SAMPLER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SAMPLER_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestSamplerResource.hpp"
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <span>

namespace AgcDriver::Graphics {

class Sampler {
public:
    Sampler(const Context& context, const GuestSamplerResource& descriptor);
    ~Sampler();
    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;

    VkSampler Handle() const;

private:
    void release() noexcept;

    Context context;
    VkSampler sampler = VK_NULL_HANDLE;
};

// One VkSampler per distinct S# (its 4 words plus the shader's depth-compare use): samplers are
// immutable and DecodeSamplerResource is a pure function of the words, so equal keys mean equal
// samplers and nothing ever invalidates an entry. Holders keep a shared_ptr, so an entry evicted from
// the LRU-capped cache lives on while a descriptor set in flight references it. One cache per device.
// APS5_NO_SAMPLER_CACHE=1 creates a sampler per binding as before.
class SamplerCache {
public:
    explicit SamplerCache(std::size_t capacity = 1024);
    SamplerCache(const SamplerCache&) = delete;
    SamplerCache& operator=(const SamplerCache&) = delete;
    std::shared_ptr<Sampler> Get(const Context& context, std::span<const std::uint32_t> words, bool compareEnable);
    // APS5_PROFILE_DRAW counters: lookups served by an existing sampler, and samplers created.
    std::uint64_t Hits() const { return hits; }
    std::uint64_t Misses() const { return misses; }

private:
    struct Entry {
        std::shared_ptr<Sampler> sampler;
        std::uint64_t lastUse;
    };
    std::mutex mutex;
    std::map<std::array<std::uint32_t, 5>, Entry> entries;
    std::uint64_t clock = 0;
    std::size_t capacity;
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
};

}

#endif
