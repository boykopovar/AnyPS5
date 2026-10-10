#include "prx/libSceVideoOut/include/BufferReuseTracker.hpp"

#include <Testing/Test.hpp>

#include <stdexcept>

namespace {

using Testing::Case;
using Testing::Require;

const Case empty{"Capture_NothingReserved_IsComplete", [] {
    BufferReuseTracker buffer;
    Require(buffer.IsComplete(buffer.Capture()), "an empty tracker is complete");
}};

const Case outOfOrder{"Complete_OutOfOrder_WaitsForEveryEarlierReservation", [] {
    BufferReuseTracker buffer;
    const auto first = buffer.Reserve();
    const auto second = buffer.Reserve();
    const auto fence = buffer.Capture();
    buffer.Reserve();
    Require(!buffer.IsComplete(fence), "fence pending with two reservations");
    buffer.Complete(second);
    Require(!buffer.IsComplete(fence), "out-of-order retirement does not release the first reservation");
    buffer.Complete(first);
    Require(buffer.IsComplete(fence), "later reservations do not block the fence");
}};

const Case cancellation{"Complete_LaterReservation_CancelsLikeRetirement", [] {
    BufferReuseTracker buffer;
    const auto first = buffer.Reserve();
    const auto second = buffer.Reserve();
    const auto fence = buffer.Capture();
    const auto future = buffer.Reserve();
    buffer.Complete(second);
    buffer.Complete(first);
    Require(!buffer.IsComplete(buffer.Capture()), "the later reservation is still pending");
    buffer.Complete(future);
    Require(buffer.IsComplete(buffer.Capture()), "cancellation retires the later reservation");
    const auto next = buffer.Reserve();
    Require(next > future, "reservations keep increasing");
    Require(buffer.IsComplete(fence), "an old fence stays complete");
    buffer.Complete(next);
}};

const Case futureFence{"IsComplete_FenceBeyondReservations_Throws", [] {
    BufferReuseTracker buffer;
    const auto next = buffer.Reserve();
    buffer.Complete(next);
    Testing::RequireThrows<std::invalid_argument>([&] { buffer.IsComplete(next + 1); }, "a fence that was never captured");
}};

} // namespace
