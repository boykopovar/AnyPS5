#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/LabelDeadlines.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

using AgcDriver::DriverDetail::ReadLabelDeadlines;
using std::chrono::microseconds;

void require(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error(reason);
}

microseconds deadlineBeforeSplit(const char* flushUs, bool batchSubmit) {
    return microseconds(flushUs != nullptr ? std::atoi(flushUs) : (batchSubmit ? 100 : 250));
}

std::string settings(const char* flushUs, const char* idleUs, bool batchSubmit) {
    return "APS5_LABEL_FLUSH_US=" + std::string(flushUs ? flushUs : "unset") + ", APS5_LABEL_IDLE_FLUSH_US=" + std::string(idleUs ? idleUs : "unset") +
           (batchSubmit ? ", batched" : ", unbatched");
}

void waiterKeepsTodaysDeadline() {
    for (const char* flushUs : {static_cast<const char*>(nullptr), "0", "50", "100", "250", "2000", "60000000"}) {
        for (const char* idleUs : {static_cast<const char*>(nullptr), "0", "50", "500", "2000", "10000"}) {
            for (const bool batchSubmit : {false, true}) {
                const auto deadlines = ReadLabelDeadlines(flushUs, idleUs, batchSubmit);
                require(deadlines.waiter == deadlineBeforeSplit(flushUs, batchSubmit), "the waiter deadline differs from the one before the split (" + settings(flushUs, idleUs, batchSubmit) + ")");
                require(deadlines.idle >= deadlines.waiter, "the idle deadline is shorter than the waiter deadline (" + settings(flushUs, idleUs, batchSubmit) + ")");
            }
        }
    }
}

void unsetKeepsOneDeadline() {
    for (const char* flushUs : {static_cast<const char*>(nullptr), "0", "50", "100", "250", "2000", "60000000"}) {
        for (const bool batchSubmit : {false, true}) {
            const auto deadlines = ReadLabelDeadlines(flushUs, nullptr, batchSubmit);
            require(deadlines.idle == deadlineBeforeSplit(flushUs, batchSubmit), "without APS5_LABEL_IDLE_FLUSH_US the idle paths do not keep the deadline before the split (" + settings(flushUs, nullptr, batchSubmit) + ")");
        }
    }
}

void optIn() {
    require(ReadLabelDeadlines(nullptr, "2000", true).idle == microseconds(2000), "APS5_LABEL_IDLE_FLUSH_US sets the idle deadline");
    require(ReadLabelDeadlines(nullptr, "2000", false).idle == microseconds(2000), "APS5_LABEL_IDLE_FLUSH_US does not depend on batching");
    require(ReadLabelDeadlines(nullptr, "50", true).idle == microseconds(100), "APS5_LABEL_IDLE_FLUSH_US cannot go below the waiter deadline");
    require(ReadLabelDeadlines("60000000", "2000", true).idle == microseconds(60000000), "a waiter deadline past the idle one raises it");
}

}

int main() {
    try {
        waiterKeepsTodaysDeadline();
        unsetKeepsOneDeadline();
        optIn();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "label deadlines: %s\n", error.what());
        return 1;
    }
    return 0;
}
