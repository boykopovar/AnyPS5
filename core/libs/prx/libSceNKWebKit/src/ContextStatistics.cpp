#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/LiveObjectCount.hpp"

#include <atomic>
#include <cerrno>
#include <stdexcept>

namespace {

std::atomic<std::uint32_t> liveViews{0};
std::atomic<std::uint32_t> livePages{0};
std::atomic<std::uint32_t> liveFrames{0};

}

extern "C" {

void AdjustLiveObjectCount_nid_no_patch(NKWebKit::ObjectType kind, std::int32_t delta) {
    if (kind == NKWebKit::ObjectType::View) liveViews += static_cast<std::uint32_t>(delta);
    if (kind == NKWebKit::ObjectType::Page) livePages += static_cast<std::uint32_t>(delta);
    if (kind == NKWebKit::ObjectType::Frame) liveFrames += static_cast<std::uint32_t>(delta);
}

void APS5_VABI WKContextGetGlobalStatistics(NKWebKit::ContextStatistics* statistics) {
    const NKWebKit::ContextStatistics current{liveViews.load(), livePages.load(), liveFrames.load()};
    const int savedErrno = errno;
    const bool written = GuestCopyOut_nid_no_patch(statistics, &current, sizeof(current));
    errno = savedErrno;
    if (!written) throw std::invalid_argument("WKContextGetGlobalStatistics: unwritable statistics");
}

}
