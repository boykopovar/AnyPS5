#ifndef CORE_LIBS_PRX_LIBSCEJOBMANAGER_COMPUTEPROGRESS_HPP
#define CORE_LIBS_PRX_LIBSCEJOBMANAGER_COMPUTEPROGRESS_HPP

#include <cstdint>
#include <stdexcept>

namespace JobManager {

class ComputeProgress {
public:
    std::uint32_t Publish(std::uint32_t count) {
        total += count;
        return total;
    }

    std::uint32_t Collect(std::uint64_t value) {
        const auto current = static_cast<std::uint32_t>(value);
        const std::uint32_t finished = current - seen;
        if (finished > std::uint32_t{total - seen}) throw std::runtime_error("libSceJobManager: a compute label advanced beyond its published work");
        seen = current;
        return finished;
    }

private:
    std::uint32_t total = 0;
    std::uint32_t seen = 0;
};

}

#endif
