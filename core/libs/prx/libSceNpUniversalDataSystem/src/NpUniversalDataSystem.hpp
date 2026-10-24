#ifndef CORE_LIBS_PRX_LIBSCENPUNIVERSALDATASYSTEM_NPUNIVERSALDATASYSTEM_HPP
#define CORE_LIBS_PRX_LIBSCENPUNIVERSALDATASYSTEM_NPUNIVERSALDATASYSTEM_HPP

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

#include "SceTypes.hpp"
#include "prx/libSceNpUniversalDataSystem/include/UdsEvent.hpp"

static constexpr int SCE_NP_UNIVERSAL_DATA_SYSTEM_OK = 0;
static constexpr int SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_INVALID_ARGUMENT = -2141900542;

static constexpr int NP_UNIVERSAL_DATA_SYSTEM_CONTEXT_DEFAULT = 1;
static constexpr int NP_UNIVERSAL_DATA_SYSTEM_HANDLE_DEFAULT  = 1;

static constexpr const char* NP_UNIVERSAL_DATA_SYSTEM_EMPTY_JSON = "{}";
static constexpr size_t NP_UNIVERSAL_DATA_SYSTEM_EMPTY_EVENT_SIZE = 3;

namespace Uds {

template <typename TValue>
class Owned {
public:
    TValue* Adopt(std::unique_ptr<TValue> value) {
        std::lock_guard lock(mutex);
        auto* pointer = value.get();
        values.emplace(pointer, std::move(value));
        return pointer;
    }

    void Release(const TValue* value) {
        std::unique_ptr<TValue> released;
        std::lock_guard lock(mutex);
        const auto found = values.find(value);
        if (found == values.end()) return;
        released = std::move(found->second);
        values.erase(found);
    }

    bool Contains(const TValue* value) {
        std::lock_guard lock(mutex);
        return values.contains(value);
    }

private:
    std::mutex mutex;
    std::map<const TValue*, std::unique_ptr<TValue>> values;
};

struct ContextOwner {
    std::int32_t userId = 0;
    std::uint32_t serviceLabel = 0;
};

std::optional<ContextOwner> FindContext(int context);

}

#endif
