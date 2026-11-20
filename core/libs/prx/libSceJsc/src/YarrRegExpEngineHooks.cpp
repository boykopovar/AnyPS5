#include "prx/libSceJsc/src/YarrRegExpEngine.hpp"

#include <cstddef>
#include <cstdlib>

extern "C" {
#include "libregexp.h"
}

using Jsc::YarrEngine::State;

extern "C" {

bool lre_check_stack_overflow(void* opaque, size_t allocaSize) {
    const auto* state = static_cast<const State*>(opaque);
    char marker = 0;
    const char* const here = &marker;
    const std::size_t used = static_cast<std::size_t>(state->stackBase > here ? state->stackBase - here : here - state->stackBase);
    return used + allocaSize > Jsc::YarrEngine::StackBudget;
}

int lre_check_timeout(void* opaque) {
    auto* state = static_cast<State*>(opaque);
    return ++state->polls > Jsc::YarrEngine::TimeoutPolls;
}

void* lre_realloc(void*, void* pointer, size_t size) {
    if (size == 0) {
        std::free(pointer);
        return nullptr;
    }
    return std::realloc(pointer, size);
}

}
