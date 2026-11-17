#include "prx/libcairo/include/Surface.hpp"

#include <array>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <type_traits>

using ExpectedCallback = void (APS5_VABI *)(void*);
static_assert(std::is_same_v<CairoDestroyFunc, ExpectedCallback>);
static_assert(sizeof(CairoUserDataKey) == sizeof(int));

namespace {

std::array<void*, 128> events{};
std::size_t eventCount{};

void APS5_VABI Record(void* pointer) {
    assert(eventCount < events.size());
    events[eventCount++] = pointer;
}

void ResetEvents() {
    events.fill(nullptr);
    eventCount = 0;
}

void Expect(std::initializer_list<void*> expected) {
    assert(eventCount == expected.size());
    std::size_t index = 0;
    for (void* pointer : expected) assert(events[index++] == pointer);
}

CairoSurface* Image(int width = 1, int height = 1) {
    auto* surface = cairo_image_surface_create(0, width, height);
    assert(surface && cairo_surface_status(surface) == 0);
    return surface;
}

void CheckReplacement() {
    ResetEvents();
    auto* surface = Image();
    CairoUserDataKey first{7}, equal{7}, absent{};
    int a{}, b{}, c{}, d{}, ignored{};
    assert(&first != &equal);
    assert(cairo_surface_set_user_data(surface, &first, &a, Record) == 0);
    assert(cairo_surface_set_user_data(surface, &equal, &b, Record) == 0);
    Expect({});
    assert(cairo_surface_set_user_data(surface, &first, &c, Record) == 0);
    Expect({&a});
    assert(cairo_surface_set_user_data(surface, &first, &c, Record) == 0);
    Expect({&a, &c});
    assert(cairo_surface_set_user_data(surface, &equal, nullptr, Record) == 0);
    Expect({&a, &c, &b});
    assert(cairo_surface_set_user_data(surface, &absent, nullptr, Record) == 0);
    assert(cairo_surface_set_user_data(surface, &equal, &d, Record) == 0);
    assert(cairo_surface_set_user_data(surface, &absent, &ignored, nullptr) == 0);
    cairo_surface_destroy(surface);
    Expect({&a, &c, &b, &d, &c});
}

void CheckSlotOrder() {
    ResetEvents();
    auto* surface = Image();
    std::array<CairoUserDataKey, 6> keys{};
    std::array<int, 7> values{};
    for (int i = 0; i < 4; ++i)
        assert(cairo_surface_set_user_data(surface, &keys[i], &values[i], Record) == 0);
    assert(cairo_surface_set_user_data(surface, &keys[0], nullptr, nullptr) == 0);
    assert(cairo_surface_set_user_data(surface, &keys[2], nullptr, nullptr) == 0);
    Expect({&values[0], &values[2]});
    assert(cairo_surface_set_user_data(surface, &keys[4], &values[4], Record) == 0);
    assert(cairo_surface_set_user_data(surface, &keys[3], &values[5], Record) == 0);
    Expect({&values[0], &values[2], &values[3]});
    cairo_surface_destroy(surface);
    Expect({&values[0], &values[2], &values[3], &values[5], &values[4], &values[1]});
}

void CheckNullKey() {
    ResetEvents();
    auto* surface = Image();
    CairoUserDataKey first{};
    int a{}, b{}, c{}, d{};
    assert(cairo_surface_set_user_data(surface, &first, &a, Record) == 0);
    assert(cairo_surface_set_user_data(surface, nullptr, &b, Record) == 0);
    assert(cairo_surface_set_user_data(surface, &first, nullptr, nullptr) == 0);
    assert(cairo_surface_set_user_data(surface, nullptr, &c, Record) == 0);
    Expect({&a});
    assert(cairo_surface_set_user_data(surface, nullptr, &d, Record) == 0);
    Expect({&a, &c});
    assert(cairo_surface_set_user_data(surface, nullptr, nullptr, Record) == 0);
    assert(cairo_surface_set_user_data(surface, nullptr, nullptr, Record) == 0);
    Expect({&a, &c, &d});
    cairo_surface_destroy(surface);
    Expect({&a, &c, &d, &b});
}

CairoSurface* finishingSurface{};
CairoUserDataKey reentryKey{};
int reentryData{};
int registrationStatus{-1};
int removalStatus{-1};

void APS5_VABI Reenter(void* pointer) {
    Record(pointer);
    registrationStatus = cairo_surface_set_user_data(finishingSurface, &reentryKey, &reentryData, Record);
    removalStatus = cairo_surface_set_user_data(finishingSurface, &reentryKey, nullptr, Record);
}

unsigned pixelReleases{};
void* releasedPixels{};

void APS5_VABI ReleasePixels(void* pointer) {
    Record(pointer);
    ++pixelReleases;
    releasedPixels = pointer;
    std::free(pointer);
}

void CheckLifetime() {
    ResetEvents();
    CairoUserDataKey parentKey{}, childKey{};
    int childData{};
    auto* pixels = static_cast<unsigned char*>(std::malloc(64));
    assert(pixels);
    pixels[13] = 93;
    auto* parent = cairo_image_surface_create_for_data(pixels, 0, 4, 4, 16);
    assert(cairo_surface_status(parent) == 0);
    assert(cairo_image_surface_get_data(parent) == pixels);
    assert(cairo_surface_set_user_data(parent, &parentKey, pixels, ReleasePixels) == 0);
    assert(cairo_surface_reference(parent) == parent);
    auto* child = cairo_surface_create_for_rectangle(parent, 0, 0, 2, 2);
    assert(cairo_surface_status(child) == 0);
    assert(cairo_surface_set_user_data(child, &childKey, &childData, Record) == 0);
    cairo_surface_destroy(parent);
    Expect({});
    assert(cairo_image_surface_get_data(parent)[13] == 93);
    cairo_surface_destroy(parent);
    Expect({});
    assert(pixelReleases == 0);
    cairo_surface_destroy(child);
    Expect({pixels, &childData});
    assert(pixelReleases == 1 && releasedPixels == pixels);

    ResetEvents();
    int data{};
    finishingSurface = Image();
    assert(cairo_surface_set_user_data(finishingSurface, &parentKey, &data, Reenter) == 0);
    cairo_surface_destroy(finishingSurface);
    Expect({&data});
    assert(registrationStatus == 12 && removalStatus == 12);
    finishingSurface = nullptr;

    for (auto dimensions : {std::array{0, 4}, std::array{4, 0}}) {
        ResetEvents();
        auto* empty = Image(dimensions[0], dimensions[1]);
        assert(cairo_surface_set_user_data(empty, &parentKey, &data, Record) == 0);
        cairo_surface_destroy(empty);
        Expect({&data});
    }
}

void CheckErrors() {
    ResetEvents();
    CairoUserDataKey key{};
    int data{};
    auto* valid = Image();
    std::array<CairoSurface*, 4> errors{
        cairo_image_surface_create(99, 1, 1),
        cairo_image_surface_create(0, -1, 1),
        cairo_image_surface_create_for_data(nullptr, 0, 2, 2, 1),
        cairo_surface_create_similar(valid, 0, 1, 1)};
    constexpr std::array expected{16, 32, 24, 15};
    for (std::size_t i = 0; i < errors.size(); ++i) {
        assert(cairo_surface_status(errors[i]) == expected[i]);
        assert(cairo_surface_set_user_data(errors[i], &key, &data, Record) == expected[i]);
        assert(cairo_surface_set_user_data(errors[i], nullptr, nullptr, Record) == expected[i]);
        cairo_surface_destroy(errors[i]);
    }
    cairo_surface_destroy(valid);
    Expect({});
}

}

int main() {
    CheckReplacement();
    CheckSlotOrder();
    CheckNullKey();
    CheckLifetime();
    CheckErrors();
    std::puts("guest cairo surface user-data tests passed");
}
