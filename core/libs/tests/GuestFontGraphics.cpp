#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceFont/libSceFont.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {

constexpr int InvalidParameter = static_cast<int>(0x80460002);
constexpr int InvalidFill = static_cast<int>(0x80460086);
constexpr int InvalidRegion = static_cast<int>(0x80460087);
constexpr int InvalidFillPlot = static_cast<int>(0x80460088);
constexpr int InvalidFillRates = static_cast<int>(0x80460089);
constexpr float NaN = std::numeric_limits<float>::quiet_NaN();

int g_lastError = 0;
int g_errorCount = 0;

void APS5_VABI RecordError(int error) {
    g_lastError = error;
    ++g_errorCount;
}

void Require(bool value) { if (!value) std::abort(); }

template <typename T>
void Fill(T& object, std::uint8_t value) {
    std::memset(&object, value, sizeof(object));
}

template <typename T>
bool AllBytes(const T& object, std::size_t from, std::size_t to, std::uint8_t value) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&object);
    for (std::size_t i = from; i < to; ++i) if (bytes[i] != value) return false;
    return true;
}

int g_expectedErrors = 0;

void RequireError(int result, int error) {
    Require(result == error);
    Require(g_lastError == error);
    g_lastError = 0;
    ++g_expectedErrors;
}

const FontGraphicsFill* Sentinel(std::intptr_t value) {
    return reinterpret_cast<const FontGraphicsFill*>(value);
}

void Regions() {
    FontGraphicsRegion region;
    Fill(region, 0xa5);
    Require(sceFontGraphicsRegionInit(&region, 1.0f, 2.0f, 3.0f, 4.0f) == 0);
    Require(region.x == 1.0f && region.y == 2.0f && region.width == 3.0f && region.height == 4.0f);
    Require(region.flags == 0xf && region.tag == 0xf60 && region.outerRadius == 0.0f && region.innerRadius == 0.0f);
    Require(sceFontGraphicsRegionInit(&region, 0.0f, 0.0f, 0.0f, 0.0f) == 0);
    Require(sceFontGraphicsRegionInit(&region, 0.0f, 0.0f, NaN, NaN) == 0);
    Fill(region, 0xa5);
    RequireError(sceFontGraphicsRegionInit(&region, 0.0f, 0.0f, -1.0f, 1.0f), InvalidParameter);
    RequireError(sceFontGraphicsRegionInit(&region, 0.0f, 0.0f, 1.0f, -1.0f), InvalidParameter);
    RequireError(sceFontGraphicsRegionInit(nullptr, 0.0f, 0.0f, 1.0f, 1.0f), InvalidParameter);
    Require(AllBytes(region, 0, sizeof(region), 0xa5));

    Require(sceFontGraphicsRegionInitCircular(&region, 10.0f, 20.0f, 4.0f, 1.0f) == 0);
    Require(region.x == 6.0f && region.y == 16.0f && region.width == 8.0f && region.height == 8.0f);
    Require(region.flags == 0xff && region.tag == 0xf60 && region.outerRadius == 4.0f && region.innerRadius == 1.0f);
    Require(sceFontGraphicsRegionInitCircular(&region, 0.0f, 0.0f, 2.0f, 2.0f) == 0);
    RequireError(sceFontGraphicsRegionInitCircular(&region, 0.0f, 0.0f, 1.0f, 2.0f), InvalidParameter);
    RequireError(sceFontGraphicsRegionInitCircular(&region, 0.0f, 0.0f, 0.0f, 0.0f), InvalidParameter);
    RequireError(sceFontGraphicsRegionInitCircular(&region, 0.0f, 0.0f, 1.0f, 0.0f), InvalidParameter);
    RequireError(sceFontGraphicsRegionInitCircular(nullptr, 0.0f, 0.0f, 2.0f, 1.0f), InvalidParameter);

    Require(sceFontGraphicsRegionInitRoundish(&region, 1.0f, 2.0f, 10.0f, 8.0f, 4.0f, 2.0f, 0x5) == 0);
    Require(region.width == 10.0f && region.height == 8.0f && region.flags == 0x5 && region.tag == 0xf60);
    Require(region.outerRadius == 4.0f && region.innerRadius == 2.0f);
    Require(sceFontGraphicsRegionInitRoundish(&region, 0.0f, 0.0f, 10.0f, 8.0f, 0.0f, 9.0f, 0xff) == 0);
    RequireError(sceFontGraphicsRegionInitRoundish(&region, 0.0f, 0.0f, 10.0f, 8.0f, 4.5f, 0.0f, 0), InvalidParameter);
    RequireError(sceFontGraphicsRegionInitRoundish(&region, 0.0f, 0.0f, 10.0f, 8.0f, 2.0f, 3.0f, 0), InvalidParameter);
    RequireError(sceFontGraphicsRegionInitRoundish(&region, 0.0f, 0.0f, 10.0f, 8.0f, 2.0f, 1.0f, 0x100), InvalidParameter);
    RequireError(sceFontGraphicsRegionInitRoundish(&region, 0.0f, 0.0f, 0.0f, 8.0f, 0.0f, 0.0f, 0), InvalidParameter);
    RequireError(sceFontGraphicsRegionInitRoundish(&region, 0.0f, 0.0f, 10.0f, 8.0f, -1.0f, 0.0f, 0), InvalidParameter);
}

void FillMethods() {
    FontGraphicsFillMethod method;
    Fill(method, 0xa5);
    Require(sceFontGraphicsFillMethodInit(&method, 0x50, 12) == 0);
    Require(method.mode == 0x50 && method.type == 12 && method.flags == 0xf && method.tag == 0xf01b);
    Require(AllBytes(method, 16, 32, 0));
    for (std::int32_t mode : {0x00, 0x10, 0x20, 0x30, 0x40}) Require(sceFontGraphicsFillMethodInit(&method, mode, 1) == 0);
    for (std::int32_t type = 1; type <= 7; ++type) Require(sceFontGraphicsFillMethodInit(&method, 0, type) == 0);
    for (std::int32_t mode : {0x01, 0x08, 0x60, -0x10}) RequireError(sceFontGraphicsFillMethodInit(&method, mode, 1), InvalidParameter);
    for (std::int32_t type : {0, 8, 11, 13, -1}) RequireError(sceFontGraphicsFillMethodInit(&method, 0, type), InvalidParameter);
    RequireError(sceFontGraphicsFillMethodInit(nullptr, 0, 1), InvalidParameter);
}

void FillPlots() {
    FontGraphicsRegion region;
    Require(sceFontGraphicsRegionInitRoundish(&region, 1.0f, 2.0f, 10.0f, 8.0f, 4.0f, 2.0f, 0x5) == 0);
    FontGraphicsFillPlot plot;
    Fill(plot, 0xa5);
    Require(sceFontGraphicsFillPlotInit(&plot, &region) == 0);
    Require(plot.mapping[0] == 1.0f && plot.mapping[1] == 2.0f && plot.mapping[2] == 11.0f && plot.mapping[3] == 10.0f);
    Require(plot.region == &region && plot.tag == 0xf01c && plot.reserved26 == 0 && plot.layout == 0xf77 && plot.flags == 1);
    Require(AllBytes(plot, 31, 64, 0));

    Fill(plot, 0xa5);
    Require(sceFontGraphicsFillPlotInit(&plot, nullptr) == 0);
    Require(plot.region == nullptr && plot.tag == 0xf01c && plot.layout == 0 && plot.flags == 2);
    Require(AllBytes(plot, 0, 16, 0));
    const auto* whole = reinterpret_cast<const FontGraphicsRegion*>(std::intptr_t{-1});
    Require(sceFontGraphicsFillPlotInit(&plot, whole) == 0);
    Require(plot.region == whole && plot.layout == 0xf77 && plot.flags == 6);

    FontGraphicsRegion bad = region;
    bad.tag = 0;
    Fill(plot, 0xa5);
    RequireError(sceFontGraphicsFillPlotInit(&plot, &bad), InvalidRegion);
    Require(plot.tag == 0 && AllBytes(plot, 0, 24, 0xa5) && AllBytes(plot, 26, 64, 0xa5));
    bad = region;
    bad.outerRadius = NaN;
    RequireError(sceFontGraphicsFillPlotInit(&plot, &bad), InvalidRegion);
    bad = region;
    bad.outerRadius = 4.5f;
    RequireError(sceFontGraphicsFillPlotInit(&plot, &bad), InvalidRegion);
    bad = region;
    bad.outerRadius = 0.0f;
    bad.innerRadius = 4.0f;
    Require(sceFontGraphicsFillPlotInit(&plot, &bad) == 0);
    bad.innerRadius = 4.5f;
    RequireError(sceFontGraphicsFillPlotInit(&plot, &bad), InvalidRegion);
    RequireError(sceFontGraphicsFillPlotInit(nullptr, &region), InvalidFillPlot);

    Require(sceFontGraphicsFillPlotInit(&plot, &region) == 0);
    Require(sceFontGraphicsFillPlotSetLayout(&plot, 0x400, 0x24) == 0);
    Require(plot.layout == 0x424 && plot.flags == 9);
    Require(sceFontGraphicsFillPlotSetLayout(&plot, 0, 0) == 0);
    Require(plot.layout == 0 && plot.flags == 8);
    Require(sceFontGraphicsFillPlotSetLayout(&plot, 0x170, 0x1100) == 0);
    Require(plot.layout == 0x1170);
    RequireError(sceFontGraphicsFillPlotSetLayout(&plot, 0x171, 0), InvalidParameter);
    RequireError(sceFontGraphicsFillPlotSetLayout(&plot, 0, 0x3), InvalidParameter);
    RequireError(sceFontGraphicsFillPlotSetLayout(&plot, 0, 0x50), InvalidParameter);
    Require(sceFontGraphicsFillPlotSetMapping(&plot, 5.0f, 6.0f, 7.0f, NaN) == 0);
    Require(plot.mapping[0] == 5.0f && plot.mapping[2] == 7.0f && std::isnan(plot.mapping[3]) && plot.flags == 9);

    FontGraphicsFillPlot untagged{};
    RequireError(sceFontGraphicsFillPlotSetLayout(&untagged, 0, 0), InvalidFillPlot);
    RequireError(sceFontGraphicsFillPlotSetMapping(&untagged, 0.0f, 0.0f, 0.0f, 0.0f), InvalidFillPlot);
}

void FillRates() {
    FontGraphicsFillRates rates;
    Fill(rates, 0xa5);
    std::array<std::uint8_t, 8> fill{0x30};
    const auto* fillPointer = reinterpret_cast<const FontGraphicsFill*>(fill.data());
    Require(sceFontGraphicsFillRatesInit(&rates, fillPointer, 9) == 0);
    Require(rates.fill == fillPointer && rates.tag == 0xf01a && rates.effect == 9 && rates.layout == 0xf77 && rates.flags == 2);
    Require(AllBytes(rates, 0, 16, 0) && AllBytes(rates, 31, 64, 0));
    for (std::intptr_t value : {-1, 0, 1, 2}) Require(sceFontGraphicsFillRatesInit(&rates, Sentinel(value), 0) == 0);
    for (std::uint32_t effect : {0u, 1u, 2u, 3u, 4u, 5u, 8u, 9u, 16u}) {
        Require(sceFontGraphicsFillRatesInit(&rates, nullptr, effect) == 0);
        Require(rates.effect == effect);
    }

    Fill(rates, 0xa5);
    RequireError(sceFontGraphicsFillRatesInit(&rates, nullptr, 6), InvalidParameter);
    Require(AllBytes(rates, 0, sizeof(rates), 0));
    fill[0] = 0x31;
    RequireError(sceFontGraphicsFillRatesInit(&rates, fillPointer, 0), InvalidFill);
    fill[0] = 0x60;
    RequireError(sceFontGraphicsFillRatesInit(&rates, fillPointer, 0), InvalidFill);
    RequireError(sceFontGraphicsFillRatesInit(nullptr, nullptr, 0), InvalidParameter);

    fill[0] = 0x10;
    Require(sceFontGraphicsFillRatesInit(&rates, nullptr, 0) == 0);
    Require(sceFontGraphicsFillRatesSetFillEffect(&rates, fillPointer, 16) == 0);
    Require(rates.fill == fillPointer && rates.effect == 16 && rates.flags == 2 && rates.layout == 0xf77);
    RequireError(sceFontGraphicsFillRatesSetFillEffect(&rates, nullptr, 17), InvalidParameter);
    Require(rates.fill == fillPointer && rates.effect == 16);

    Require(sceFontGraphicsFillRatesSetLayout(&rates, 0xf77, 0x44) == 0);
    Require(rates.layout == 0 && rates.flags == 3);
    Require(sceFontGraphicsFillRatesSetLayout(&rates, 0, 0) == 0);
    Require(rates.layout == 0 && rates.flags == 2);
    RequireError(sceFontGraphicsFillRatesSetLayout(&rates, 0x801, 0), InvalidParameter);
    Require(sceFontGraphicsFillRatesSetMapping(&rates, 1.0f, 2.0f, 3.0f, 4.0f) == 0);
    Require(rates.mapping[0] == 1.0f && rates.mapping[3] == 4.0f && rates.flags == 1);

    FontGraphicsFillRates untagged{};
    RequireError(sceFontGraphicsFillRatesSetFillEffect(&untagged, nullptr, 0), InvalidFillRates);
    RequireError(sceFontGraphicsFillRatesSetLayout(&untagged, 0, 0), InvalidFillRates);
    RequireError(sceFontGraphicsFillRatesSetMapping(&untagged, 0.0f, 0.0f, 0.0f, 0.0f), InvalidFillRates);
}

}

int main() {
    sceFontSetGraphicsErrorCallback(RecordError);
    Regions();
    FillMethods();
    FillPlots();
    FillRates();
    Require(g_errorCount == g_expectedErrors);
    sceFontSetGraphicsErrorCallback(nullptr);
    const int before = g_errorCount;
    FontGraphicsRegion region;
    Require(sceFontGraphicsRegionInit(&region, 0.0f, 0.0f, -1.0f, 0.0f) == InvalidParameter);
    Require(g_errorCount == before);
}
