#include "prx/libSceAgc/DcbState/include/ContextState.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename TAction>
void expectFailure(TAction action) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        check(error.what()[0] != '\0', "empty exception message");
        return;
    }
    throw std::runtime_error("expected an exception");
}

void testSizes() {
    const std::array<std::uint64_t, 4> sizes{20, 108, 108, 128};
    for (std::uint32_t operation = 0; operation < sizes.size(); ++operation) {
        check(sceAgcDcbContextStateOpGetSize(operation) == sizes[operation], "context state size mismatch");
        std::array<std::uint32_t, 64> words{};
        CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(),
                             nullptr, nullptr, 0};
        check(sceAgcDcbContextStateOp_0100(&buffer, operation) == words.data(), "context state op did not start at the cursor");
        const auto written = static_cast<std::uint64_t>(buffer.cursor_up - words.data()) * sizeof(std::uint32_t);
        check(written == sizes[operation], "context state size differs from the written packets");

        std::array<std::uint32_t, 64> unversionedWords{};
        CommandBuffer unversionedBuffer{unversionedWords.data(), unversionedWords.data() + unversionedWords.size(),
                                        unversionedWords.data(), unversionedWords.data() + unversionedWords.size(),
                                        nullptr, nullptr, 0};
        check(sceAgcDcbContextStateOp(&unversionedBuffer, operation) == unversionedWords.data(), "context state op did not start at the cursor");
        const auto unversionedWritten = static_cast<std::uint64_t>(unversionedBuffer.cursor_up - unversionedWords.data()) * sizeof(std::uint32_t);
        check(unversionedWritten == sizes[operation], "context state size differs from the written packets");
        check(words == unversionedWords, "unversioned context state op mismatch with versioned op");
    }
}

void testRejections() {
    expectFailure([] { sceAgcDcbContextStateOpGetSize(4); });
    expectFailure([] { sceAgcDcbContextStateOpGetSize(0xffffffffu); });
}

}

int main() {
    try {
        testSizes();
        testRejections();
        LibcRunShutdown_nid_postfix();
        std::puts("AGC context state size tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { LibcRunShutdown_nid_postfix(); }
        catch (const std::exception& shutdown) { std::fprintf(stderr, "shutdown: %s\n", shutdown.what()); }
        return 1;
    }
}
