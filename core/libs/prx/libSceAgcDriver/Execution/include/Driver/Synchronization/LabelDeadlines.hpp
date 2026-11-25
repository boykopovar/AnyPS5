#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_SYNCHRONIZATION_LABELDEADLINES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_SYNCHRONIZATION_LABELDEADLINES_HPP

#include <algorithm>
#include <chrono>
#include <cstdlib>

namespace AgcDriver::DriverDetail {

struct LabelDeadlines {
    std::chrono::microseconds waiter;
    std::chrono::microseconds idle;
};

inline LabelDeadlines ReadLabelDeadlines(const char* flushUs, const char* idleUs, bool batchSubmit) {
    const std::chrono::microseconds waiter(flushUs != nullptr ? std::atoi(flushUs) : (batchSubmit ? 100 : 250));
    if (idleUs == nullptr) return {waiter, waiter};
    return {waiter, std::max(waiter, std::chrono::microseconds(std::atoi(idleUs)))};
}

}

#endif
