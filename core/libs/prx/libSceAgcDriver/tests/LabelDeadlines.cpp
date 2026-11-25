#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/LabelDeadlines.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

using AgcDriver::DriverDetail::DefaultIdleLabelDeadline;
using AgcDriver::DriverDetail::LabelDeadlines;
using AgcDriver::DriverDetail::ReadLabelDeadlines;
using std::chrono::microseconds;

void require(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error(reason);
}

microseconds deadlineBeforeSplit(const char* flushUs, bool batchSubmit) {
    return microseconds(flushUs != nullptr ? std::atoi(flushUs) : (batchSubmit ? 100 : 250));
}

void waiterKeepsTodaysDeadline() {
    for (const char* flushUs : {static_cast<const char*>(nullptr), "0", "50", "100", "250", "2000", "60000000"}) {
        for (const char* idleUs : {static_cast<const char*>(nullptr), "0", "500", "2000", "10000"}) {
            for (const bool sameDeadline : {false, true}) {
                for (const bool batchSubmit : {false, true}) {
                    const auto deadlines = ReadLabelDeadlines(flushUs, idleUs, sameDeadline, batchSubmit);
                    require(deadlines.waiter == deadlineBeforeSplit(flushUs, batchSubmit),
                            "the waiter deadline differs from the one before the split (APS5_LABEL_FLUSH_US=" + std::string(flushUs ? flushUs : "unset") +
                                ", APS5_LABEL_IDLE_FLUSH_US=" + std::string(idleUs ? idleUs : "unset") + ")");
                    require(deadlines.idle >= deadlines.waiter, "the idle deadline is shorter than the waiter deadline");
                }
            }
        }
    }
}

void idleDefaults() {
    require(ReadLabelDeadlines(nullptr, nullptr, false, true).waiter == microseconds(100), "batched label submission waits 100 us for a waiter");
    require(ReadLabelDeadlines(nullptr, nullptr, false, false).waiter == microseconds(250), "unbatched label submission waits 250 us for a waiter");
    require(ReadLabelDeadlines(nullptr, nullptr, false, true).idle == DefaultIdleLabelDeadline, "the idle paths default to 2000 us");
    require(ReadLabelDeadlines(nullptr, nullptr, false, false).idle == DefaultIdleLabelDeadline, "the idle default does not depend on batching");
    require(ReadLabelDeadlines("60000000", nullptr, false, true).idle == microseconds(60000000), "a waiter deadline past the idle default raises the idle one");
}

void switches() {
    const auto same = ReadLabelDeadlines(nullptr, "5000", true, true);
    require(same.idle == same.waiter && same.waiter == microseconds(100), "APS5_NO_LABEL_IDLE_DEADLINE gives both paths the waiter deadline");
    require(ReadLabelDeadlines("300", nullptr, true, false).idle == microseconds(300), "APS5_NO_LABEL_IDLE_DEADLINE follows APS5_LABEL_FLUSH_US");
    require(ReadLabelDeadlines(nullptr, "500", false, true).idle == microseconds(500), "APS5_LABEL_IDLE_FLUSH_US sets the idle deadline");
    require(ReadLabelDeadlines(nullptr, "50", false, true).idle == microseconds(100), "APS5_LABEL_IDLE_FLUSH_US cannot go below the waiter deadline");
}

}

int main() {
    try {
        waiterKeepsTodaysDeadline();
        idleDefaults();
        switches();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "label deadlines: %s\n", error.what());
        return 1;
    }
    return 0;
}
