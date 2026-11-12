#include "prx/libfontconfig/include/Config.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <type_traits>

static_assert(std::is_same_v<decltype(FcConfigCreate()), FcConfig*>);
static_assert(std::is_same_v<decltype(FcConfigReference(nullptr)), FcConfig*>);
static_assert(std::is_same_v<decltype(FcConfigDestroy(nullptr)), void>);

int main() {
    std::uint64_t sentinel = UINT64_C(0x8877665544332211);
    auto* nonnull = reinterpret_cast<FcConfig*>(&sentinel);
    for (int i = 0; i < 256; ++i) {
        FcConfig* created = FcConfigCreate();
        assert(created == nullptr);
        FcConfig* referenceNull = FcConfigReference(nullptr);
        FcConfig* referenceNonnull = FcConfigReference(nonnull);
        assert(referenceNull == nullptr);
        assert(referenceNonnull == nullptr);
        FcConfigDestroy(nullptr);
        FcConfigDestroy(nonnull);
    }
    assert(sentinel == UINT64_C(0x8877665544332211));
}
