#include "tests/FontFixture.hpp"
#include "prx/libSceFont/include/FontInternal.hpp"
#include <array>
#include <atomic>
#include <barrier>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <string_view>
#include <thread>
#include <vector>

#ifdef __linux__
namespace FallbackPublication {

bool track = false;
bool fail = false;
bool faulted = false;
bool dataAliveDuringClose = false;
std::size_t failedBytes = 0;
FT_Face opened = nullptr;
unsigned finalized = 0;
std::weak_ptr<const std::vector<unsigned char>> backing;

void Finalize(void*) {
    ++finalized;
    dataAliveDuringClose = !backing.expired();
}

void Reset() {
    track = fail = faulted = dataAliveDuringClose = false;
    opened = nullptr;
    failedBytes = 0;
    finalized = 0;
    backing.reset();
}

}

extern "C" FT_Error __real_FT_New_Memory_Face(FT_Library, const FT_Byte*, FT_Long, FT_Long, FT_Face*);

extern "C" FT_Error __wrap_FT_New_Memory_Face(FT_Library library, const FT_Byte* bytes, FT_Long length, FT_Long index, FT_Face* face) {
    const FT_Error error = __real_FT_New_Memory_Face(library, bytes, length, index, face);
    if (error == 0 && face && *face && FallbackPublication::track) {
        FallbackPublication::opened = *face;
        (*face)->generic.finalizer = FallbackPublication::Finalize;
    }
    return error;
}

void* operator new(std::size_t bytes) {
    if (FallbackPublication::track && FallbackPublication::opened && FallbackPublication::fail) {
        FallbackPublication::fail = false;
        FallbackPublication::faulted = true;
        FallbackPublication::failedBytes = bytes;
        std::fprintf(stderr, "fallback publication stage=vector_growth qualified=%d bytes=%zu face=%p\n",
            bytes == sizeof(Font::FallbackFace), bytes, static_cast<void*>(FallbackPublication::opened));
        throw std::bad_alloc();
    }
    if (void* result = std::malloc(bytes ? bytes : 1)) return result;
    throw std::bad_alloc();
}

void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }

#endif

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

#ifdef __linux__
static std::shared_ptr<const std::vector<unsigned char>> PublicationFont() {
    using FontFixture::BuildFont;
    using FontFixture::Words;
    return std::make_shared<const std::vector<unsigned char>>(BuildFont(3, {
        {"cmap", Words({0, 1, 3, 1, 0, 12, 4, 40, 0, 6, 4, 1, 2, 'A', 'V', 0xFFFF, 0, 'A', 'V', 0xFFFF, 1 - 'A', 2 - 'V', 1, 0, 0, 0})},
    }));
}

static int RunFallbackPublication() {
    FallbackPublication::Reset();
    {
        Font::FontState state;
        auto bytes = PublicationFont();
        FallbackPublication::backing = bytes;
        FallbackPublication::track = true;
        Font::AddStateFallbackFace(state, std::move(bytes), 0);
        FallbackPublication::track = false;
        Require(state.fallbacks.size() == 1 && state.fallbacks[0].face == FallbackPublication::opened, "successful fallback control publishes the native face");
        Require(FT_Get_Char_Index(state.fallbacks[0].face, 'A') == 1, "successful fallback control maps a glyph");
    }
    Require(FallbackPublication::finalized == 1 && FallbackPublication::dataAliveDuringClose && FallbackPublication::backing.expired(), "fallback control closes once before releasing font bytes");

    FallbackPublication::Reset();
    {
        Font::FontState state;
        auto bytes = PublicationFont();
        FallbackPublication::backing = bytes;
        FallbackPublication::track = FallbackPublication::fail = true;
        bool caught = false;
        try {
            Font::AddStateFallbackFace(state, std::move(bytes), 0);
        } catch (const std::bad_alloc&) {
            caught = true;
        }
        FallbackPublication::track = FallbackPublication::fail = false;
        Require(caught && FallbackPublication::faulted && FallbackPublication::opened && FallbackPublication::failedBytes == sizeof(Font::FallbackFace), "fault occurs at vector growth after native fallback creation");
        Require(state.fallbacks.empty(), "failed fallback insertion publishes nothing");
        std::fprintf(stderr, "fallback publication cleanup qualified=1 finalized=%u dataAliveDuringClose=%d\n", FallbackPublication::finalized, FallbackPublication::dataAliveDuringClose);
        Require(FallbackPublication::finalized == 1 && FallbackPublication::dataAliveDuringClose, "failed fallback insertion closes its native face while font bytes remain live");
        Require(FallbackPublication::backing.expired(), "failed fallback insertion releases font bytes");
    }
    return 0;
}
#endif

int main(int argc, char** argv) {
#ifdef __linux__
    if (argc == 2 && std::string_view(argv[1]) == "fallback-publication") return RunFallbackPublication();
#endif
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
