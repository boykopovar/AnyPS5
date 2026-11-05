#pragma once
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <exception>

// Caller serializes access with the owning VideoOutConfig mutex.
class BufferReuseTracker {
    std::uint64_t _issued = 0;
    std::set<std::uint64_t> _pending;
public:
    std::uint64_t Reserve() {
        if (_issued == std::numeric_limits<std::uint64_t>::max())
            throw std::overflow_error("VideoOut: buffer reservation sequence exhausted");
        const auto ticket = _issued + 1;
        _pending.insert(ticket);
        _issued = ticket;
        return ticket;
    }
    std::uint64_t Capture() const { return _issued; }
    bool IsComplete(std::uint64_t through) const {
        if (through > _issued) throw std::invalid_argument("VideoOut: unissued buffer fence");
        return _pending.empty() || *_pending.begin() > through;
    }
    void Complete(std::uint64_t ticket) noexcept {
        if (_pending.erase(ticket) != 1) std::terminate();
    }
};
