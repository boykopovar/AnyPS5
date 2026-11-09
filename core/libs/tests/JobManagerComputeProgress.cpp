#include "prx/libSceJobManager/ComputeProgress.hpp"
#include <cstdint>
#include <iostream>
#include <stdexcept>

int main() {
    try {
        JobManager::ComputeProgress progress;
        if (progress.Publish(0xfffffffd) != 0xfffffffd || progress.Collect(0xfffffffc) != 0xfffffffc)
            throw std::runtime_error("partial retirement before wrap failed");
        if (progress.Publish(5) != 2 || progress.Collect(2) != 6 || progress.Collect(2) != 0)
            throw std::runtime_error("batched completion across wrap failed");
        if (progress.Publish(9) != 11 || progress.Collect(5) != 3 || progress.Collect(11) != 6)
            throw std::runtime_error("partial retirement after wrap failed");
        bool rejected = false;
        try {
            progress.Collect(12);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        if (!rejected || progress.Collect(11) != 0) throw std::runtime_error("an invalid completion changed retirement state");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
