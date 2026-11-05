#include "prx/libSceVideoOut/include/BufferReuseTracker.hpp"
#include <cstdlib>
static void Require(bool value) { if (!value) std::abort(); }
int main() {
    BufferReuseTracker buffer;
    Require(buffer.IsComplete(buffer.Capture()));
    const auto first = buffer.Reserve();
    const auto second = buffer.Reserve();
    const auto fence = buffer.Capture();
    const auto future = buffer.Reserve();
    Require(!buffer.IsComplete(fence));
    buffer.Complete(second); // Out-of-order retirement must not release first.
    Require(!buffer.IsComplete(fence));
    buffer.Complete(first);
    Require(buffer.IsComplete(fence)); // Later reservations do not block this wait.
    Require(!buffer.IsComplete(buffer.Capture()));
    buffer.Complete(future); // Cancellation uses the same retirement operation.
    Require(buffer.IsComplete(buffer.Capture()));
    const auto next = buffer.Reserve();
    Require(next > future && buffer.IsComplete(fence));
    buffer.Complete(next);
    bool rejected = false;
    try { buffer.IsComplete(next + 1); }
    catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected);
}
