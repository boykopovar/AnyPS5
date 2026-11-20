#include "WtfObjectIdentifier.hpp"

#include <atomic>

namespace {

std::atomic<std::uint64_t> lastObjectIdentifier{0};

}

namespace NKWebKit::WtfAbi {

std::uint64_t GenerateObjectIdentifier() {
    return lastObjectIdentifier.fetch_add(1) + 1;
}

}
