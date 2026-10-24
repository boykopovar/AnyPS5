#include "tests/FontFixture.hpp"
#include "prx/libSceFont/include/FontInternal.hpp"
#include <array>
#include <atomic>
#include <barrier>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string_view>
#include <thread>
#include <vector>

static void Require(bool value, const char* step) {
    if (!value) {
        std::fprintf(stderr, "Font face lifetime: %s\n", step);
        std::abort();
    }
}

static void FinalizeFace(void* object) {
    auto* face = static_cast<FT_Face>(object);
    auto* counter = static_cast<std::atomic<unsigned>*>(face->generic.data);
    counter->fetch_add(1, std::memory_order_relaxed);
}

static void ObserveFace(Font::FontState& state, std::atomic<unsigned>& destroyed) {
    Require(state.face != nullptr, "font loaded");
    Require(state.face->generic.data == nullptr && state.face->generic.finalizer == nullptr, "unused finalizer observer");
    state.face->generic.data = &destroyed;
    state.face->generic.finalizer = FinalizeFace;
    Require(FT_Get_Char_Index(state.face, 'A') == 1, "character mapping preserved");
    Require(FT_Set_Pixel_Sizes(state.face, 32, 32) == 0, "set pixel scale");
    Require(FT_Load_Char(state.face, 'A', FT_LOAD_DEFAULT) == 0, "load glyph");
    Require(state.face->glyph->advance.x == 16 * 64, "glyph advance preserved");
}

static bool Run(unsigned workers, unsigned iterations, bool replace, bool churn,
                const std::shared_ptr<const std::vector<unsigned char>>& bytes, std::atomic<unsigned>& destroyed) {
    std::array<Font::FontHandleNative, 8> nativeFonts{};
    std::barrier phase(static_cast<std::ptrdiff_t>(workers));
    std::vector<std::thread> threads;
    for (unsigned worker = 0; worker < workers; ++worker) {
        threads.emplace_back([&, worker] {
            const auto handle = reinterpret_cast<FontHandle>(&nativeFonts[worker]);
            for (unsigned iteration = 0; iteration < iterations; ++iteration) {
                phase.arrive_and_wait();
                auto& state = replace && iteration != 0 ? *Font::TryGetState(handle) : Font::ResetState(handle);
                Font::LoadStateFace(state, bytes, 0);
                ObserveFace(state, destroyed);
                if (!churn) phase.arrive_and_wait();
                if (!replace) Font::RemoveState(handle);
            }
            if (replace) Font::RemoveState(handle);
        });
    }
    for (auto& thread : threads) thread.join();
    const unsigned expected = workers * iterations;
    std::printf("workers=%u iterations=%u replace=%d churn=%d actual_destructors=%u expected_destructors=%u\n",
                workers, iterations, replace, churn, destroyed.load(), expected);
    std::fflush(stdout);
    return destroyed.load() == expected;
}

int main(int argc, char** argv) {
    Require(argc == 1 || (argc == 2 && (std::string_view(argv[1]) == "replace" || std::string_view(argv[1]) == "churn")), "test arguments");
    const bool replace = argc == 2 && std::string_view(argv[1]) == "replace";
    const bool churn = argc == 2 && std::string_view(argv[1]) == "churn";
    using FontFixture::BuildFont;
    using FontFixture::Words;
    const auto bytes = std::make_shared<const std::vector<unsigned char>>(BuildFont(3, {
        {"cmap", Words({0, 1, 3, 1, 0, 12, 4, 40, 0, 6, 4, 1, 2, 'A', 'V', 0xFFFF, 0, 'A', 'V', 0xFFFF, 1 - 'A', 2 - 'V', 1, 0, 0, 0})},
    }));
    std::array<std::atomic<unsigned>, 2> destroyed{};
    bool passed = Run(1, 128, replace, churn, bytes, destroyed[0]);
    passed = Run(8, 512, replace, churn, bytes, destroyed[1]) && passed;
    return passed ? 0 : 1;
}
