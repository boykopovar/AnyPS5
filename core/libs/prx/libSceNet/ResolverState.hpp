#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace NetResolver {

constexpr int Interrupted = static_cast<int>(0x80410104u);
constexpr int Invalid = static_cast<int>(0x80410109u);
constexpr int Busy = static_cast<int>(0x804101ddu);

enum class LookupKind : std::uint32_t { Ntoa = 1, Aton = 2 };

class State {
public:
    int Abort(std::uint32_t flags) {
        std::lock_guard lock(mutex);
        if (destroyed) return Invalid;
        if ((flags & ~3u) != 0u) throw std::runtime_error("sceNetResolverAbort: unsupported flags");
        if (active) {
            active->cancelled = true;
            status = Interrupted;
        } else {
            pendingAbort |= flags;
        }
        return 0;
    }

    void Destroy() {
        std::lock_guard lock(mutex);
        destroyed = true;
        if (active) active->cancelled = true;
        active.reset();
    }

    int GetError(int& result) const {
        std::lock_guard lock(mutex);
        if (destroyed) return Invalid;
        result = status;
        return 0;
    }

    template <typename TWork, typename TDeliver>
    int Run(LookupKind kind, TWork work, TDeliver deliver) {
        std::shared_ptr<Operation> operation;
        {
            std::lock_guard lock(mutex);
            if (destroyed) return Invalid;
            if (active) return Busy;
            const auto bit = static_cast<std::uint32_t>(kind);
            if ((pendingAbort & bit) != 0u) {
                pendingAbort &= ~bit;
                status = Interrupted;
                return Interrupted;
            }
            operation = std::make_shared<Operation>();
            active = operation;
        }
        int result;
        try {
            result = work();
        } catch (...) {
            std::lock_guard lock(mutex);
            if (active == operation) active.reset();
            throw;
        }
        std::lock_guard lock(mutex);
        active.reset();
        if (destroyed) return Invalid;
        if (operation->cancelled) return Interrupted;
        if (result == 0) result = deliver();
        status = result;
        return result;
    }

private:
    struct Operation { bool cancelled = false; };
    mutable std::mutex mutex;
    std::shared_ptr<Operation> active;
    std::uint32_t pendingAbort = 0;
    int status = 0;
    bool destroyed = false;
};

}
