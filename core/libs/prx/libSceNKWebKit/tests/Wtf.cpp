#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../src/WtfStringAbi.hpp"
#include <limits>
#include <stdexcept>
#include <string>
#include <source_location>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename Function, typename... Args>
void* TryResult(Function function, Args... args) {
    WTF::TryMallocReturnValue result{reinterpret_cast<void*>(std::uintptr_t{0xDEADBEEF})};
    WTF::TryMallocReturnValue* returned = function(&result, args...);
    Require(returned == &result);
    return result.data;
}

using WTF::ClockType;
using WTF::TimeType;
using WTF::Seconds;
using WTF::MonotonicTime;
using WTF::WallTime;
using WTF::TimeWithDynamicClockType;
using WTF::CPUTime;
using WTF::GregorianDateTime;
using WTF::LocalTimeOffset;
using WebCore::FloatPoint;
using WebCore::FloatSize;
using WebCore::FloatRect;
using WebCore::IntPoint;
using WebCore::IntSize;
using WebCore::IntRect;

}  // namespace

static void TestAllocationBoundaries() {
    constexpr std::size_t maxSize = std::numeric_limits<std::size_t>::max();
    Require(TryResult(_ZN3WTF13tryFastMallocEm, maxSize) == nullptr);
    Require(TryResult(_ZN3WTF13tryFastMallocEm, maxSize - 8) == nullptr);
    Require(TryResult(_ZN3WTF13tryFastMallocEm, maxSize - 64) == nullptr);
    Require(TryResult(_ZN3WTF19tryFastZeroedMallocEm, maxSize) == nullptr);
    Require(TryResult(_ZN3WTF13tryFastCallocEmm, 1, maxSize) == nullptr);
    Require(TryResult(_ZN3WTF13tryFastCallocEmm, maxSize, 2) == nullptr);
    Require(TryResult(_ZN3WTF13tryFastCallocEmm, maxSize / 2 + 1, 2) == nullptr);
    Require(_ZN3WTF20tryFastAlignedMallocEmm(64, maxSize) == nullptr);
    Require(_ZN3WTF20tryFastAlignedMallocEmm(std::size_t{1} << 62, 64) == nullptr);
    Require(_ZN3WTF20tryFastAlignedMallocEmm(maxSize, 64) == nullptr);
    Require(_ZN3WTF20tryFastAlignedMallocEmm(48, 64) == nullptr);

    void* p = _ZN3WTF10fastMallocEm(32);
    Require(p != nullptr);
    std::memset(p, 0x5A, 32);
    Require(TryResult(_ZN3WTF14tryFastReallocEPvm, p, maxSize) == nullptr);
    Require(_ZN3WTF14fastMallocSizeEPKv(p) == 32);
    Require(static_cast<unsigned char*>(p)[31] == 0x5A);
    _ZN3WTF8fastFreeEPv(p);

    void* z = TryResult(_ZN3WTF13tryFastCallocEmm, maxSize / 8, 0);
    if (z) {
        _ZN3WTF8fastFreeEPv(z);
    }
    void* zero = TryResult(_ZN3WTF13tryFastCallocEmm, 0, maxSize);
    if (zero) {
        _ZN3WTF8fastFreeEPv(zero);
    }

    void* m = TryResult(_ZN3WTF13tryFastMallocEm, std::size_t{48});
    Require(m != nullptr);
    Require(_ZN3WTF14fastMallocSizeEPKv(m) == 48);
    std::memset(m, 0x3C, 48);
    void* r = TryResult(_ZN3WTF14tryFastReallocEPvm, m, std::size_t{96});
    Require(r != nullptr);
    Require(_ZN3WTF14fastMallocSizeEPKv(r) == 96);
    for (int i = 0; i < 48; ++i) {
        Require(static_cast<unsigned char*>(r)[i] == 0x3C);
    }
    _ZN3WTF8fastFreeEPv(r);

    auto* zm = static_cast<unsigned char*>(TryResult(_ZN3WTF19tryFastZeroedMallocEm, std::size_t{40}));
    Require(zm != nullptr);
    for (int i = 0; i < 40; ++i) {
        Require(zm[i] == 0);
    }
    _ZN3WTF8fastFreeEPv(zm);

    auto* cm = static_cast<unsigned char*>(TryResult(_ZN3WTF13tryFastCallocEmm, std::size_t{6}, std::size_t{8}));
    Require(cm != nullptr);
    for (int i = 0; i < 48; ++i) {
        Require(cm[i] == 0);
    }
    _ZN3WTF8fastFreeEPv(cm);
}

static void TestMemoryAllocations() {
    void* p1 = _ZN3WTF10fastMallocEm(128);
    Require(p1 != nullptr);
    Require((reinterpret_cast<std::uintptr_t>(p1) & 0x0F) == 0);
    std::memset(p1, 0xAB, 128);
    Require(_ZN3WTF14fastMallocSizeEPKv(p1) >= 128);
    Require(_ZN3WTF18fastMallocGoodSizeEm(128) >= 128);

    void* p2 = _ZN3WTF11fastReallocEPvm(p1, 256);
    Require(p2 != nullptr);
    auto* b2 = static_cast<unsigned char*>(p2);
    for (int i = 0; i < 128; ++i) {
        Require(b2[i] == 0xAB);
    }
    _ZN3WTF8fastFreeEPv(p2);

    void* pZero = _ZN3WTF16fastZeroedMallocEm(64);
    Require(pZero != nullptr);
    auto* bZero = static_cast<unsigned char*>(pZero);
    for (int i = 0; i < 64; ++i) {
        Require(bZero[i] == 0);
    }
    _ZN3WTF8fastFreeEPv(pZero);

    void* pCalloc = _ZN3WTF10fastCallocEmm(10, 8);
    Require(pCalloc != nullptr);
    _ZN3WTF8fastFreeEPv(pCalloc);

    void* pAligned = _ZN3WTF17fastAlignedMallocEmm(64, 128);
    Require(pAligned != nullptr);
    Require((reinterpret_cast<std::uintptr_t>(pAligned) & 63) == 0);
    _ZN3WTF15fastAlignedFreeEPv(pAligned);

    char* dup = _ZN3WTF10fastStrDupEPKc("hello wtf");
    Require(dup != nullptr);
    Require(std::strcmp(dup, "hello wtf") == 0);
    _ZN3WTF8fastFreeEPv(dup);

    Require(_ZN3WTF19isFastMallocEnabledEv() == true);
    _ZN3WTF27releaseFastMallocFreeMemoryEv();
    _ZN3WTF40releaseFastMallocFreeMemoryForThisThreadEv();
    _ZN3WTF18fastEnableMiniModeEv();
    _ZN3WTF20fastDisableScavengerEv();
    FastMallocStatistics stats = _ZN3WTF20fastMallocStatisticsEv();
    static_cast<void>(stats);
    _ZN3WTF25fastMallocDumpMallocStatsEv();

    void* jsVal = _ZN3WTF13jsValueMallocEm(32);
    Require(jsVal != nullptr);
    _ZN3WTF11jsValueFreeEPv(jsVal);

    void* bm = _ZN7bmalloc3api15mallocOutOfLineEmNS_8HeapKindE(96, 0);
    Require(bm != nullptr);
    _ZN7bmalloc3api13freeOutOfLineEPvNS_8HeapKindE(bm, 0);
    Require(_ZN7bmalloc3api9isEnabledENS_8HeapKindE(0) == true);
    Require(_ZN7bmalloc15availableMemoryEv() > 0);
    Require(_ZN7bmalloc16isActiveHeapKindENS_8HeapKindE(0) == true);
    Require(_ZN7bmalloc16isActiveHeapKindENS_8HeapKindE(1) == false);
    Require(_ZN7bmalloc19mapToActiveHeapKindENS_8HeapKindE(2) == 0);
    _ZN7bmalloc3api8scavengeEv();
    _ZN7bmalloc3api14enableMiniModeEv();
    _ZN7bmalloc3api16disableScavengerEv();

    void* gc = _ZN8Gigacage6mallocENS_4KindEm(0, 112);
    Require(gc != nullptr);
    _ZN8Gigacage4freeENS_4KindEPv(0, gc);

    void* gcArr = _ZN8Gigacage11mallocArrayENS_4KindEmm(0, 5, 20);
    Require(gcArr != nullptr);
    void* gcRealloc = _ZN8Gigacage10tryReallocENS_4KindEPvm(0, gcArr, 200);
    Require(gcRealloc != nullptr);
    _ZN8Gigacage4freeENS_4KindEPv(0, gcRealloc);

    void* gcAligned = _ZN8Gigacage16tryAlignedMallocENS_4KindEmm(0, 128, 64);
    Require(gcAligned != nullptr);
    Require((reinterpret_cast<std::uintptr_t>(gcAligned) & 127) == 0);
    _ZN8Gigacage11alignedFreeENS_4KindEPv(0, gcAligned);

    void* cross = _ZN3WTF10fastMallocEm(50);
    _ZN7bmalloc3api13freeOutOfLineEPvNS_8HeapKindE(cross, 0);

    void* cross2 = _ZN7bmalloc3api15mallocOutOfLineEmNS_8HeapKindE(60, 0);
    _ZN8Gigacage4freeENS_4KindEPv(0, cross2);
}

static void TestTime() {
    MonotonicTime mNow = _ZN3WTF13MonotonicTime3nowEv();
    Require(mNow.m_value > 0.0);
    WallTime wNow = _ZN3WTF8WallTime3nowEv();
    Require(wNow.m_value > 1000000000.0);

    WallTime approxW = _ZNK3WTF13MonotonicTime19approximateWallTimeEv(&mNow);
    Require(std::abs(approxW.m_value - wNow.m_value) < 5.0);

    MonotonicTime approxM = _ZNK3WTF8WallTime24approximateMonotonicTimeEv(&wNow);
    Require(std::abs(approxM.m_value - mNow.m_value) < 5.0);

    TimeWithDynamicClockType dynWall = _ZN3WTF24TimeWithDynamicClockType3nowENS_9ClockTypeE(ClockType::Wall);
    Require(dynWall.m_type == ClockType::Wall);
    Require(dynWall.m_value > 0.0);

    TimeWithDynamicClockType dynMono = _ZN3WTF24TimeWithDynamicClockType3nowENS_9ClockTypeE(ClockType::Monotonic);
    Require(dynMono.m_type == ClockType::Monotonic);

    TimeWithDynamicClockType dynMono2 = dynMono;
    dynMono2.m_value += 1.0;
    Require(_ZNK3WTF24TimeWithDynamicClockTypegtERKS0_(&dynMono2, dynMono));
    Require(_ZNK3WTF24TimeWithDynamicClockTypegeERKS0_(&dynMono2, dynMono));
    Require(_ZNK3WTF24TimeWithDynamicClockTypeleERKS0_(&dynMono, dynMono2));

    Seconds diff = _ZNK3WTF24TimeWithDynamicClockTypemiERKS0_(&dynMono2, dynMono);
    Require(std::abs(diff.m_value - 1.0) < 1e-6);

    Require(_ZN3WTF10hasElapsedERKNS_24TimeWithDynamicClockTypeE(dynMono) == true);
    TimeWithDynamicClockType farFuture = dynMono;
    farFuture.m_value += 1000.0;
    Require(_ZN3WTF10hasElapsedERKNS_24TimeWithDynamicClockTypeE(farFuture) == false);

    Seconds sec{10.0};
    MonotonicTime mSub = _ZNK3WTF7SecondsmiENS_13MonotonicTimeE(&sec, MonotonicTime{4.0});
    Require(std::abs(mSub.m_value - 6.0) < 1e-6);
    MonotonicTime mAdd = _ZNK3WTF7SecondsplENS_13MonotonicTimeE(&sec, MonotonicTime{4.0});
    Require(std::abs(mAdd.m_value - 14.0) < 1e-6);

    WallTime wSub = _ZNK3WTF7SecondsmiENS_8WallTimeE(&sec, WallTime{3.0});
    Require(std::abs(wSub.m_value - 7.0) < 1e-6);
    WallTime wAdd = _ZNK3WTF7SecondsplENS_8WallTimeE(&sec, WallTime{3.0});
    Require(std::abs(wAdd.m_value - 13.0) < 1e-6);

    GregorianDateTime gdt{};
    _ZN3WTF17GregorianDateTime21setToCurrentLocalTimeEv(&gdt);
    Require(gdt.m_year >= 2024);
    Require(gdt.m_month >= 0 && gdt.m_month <= 11);
    Require(gdt.m_monthDay >= 1 && gdt.m_monthDay <= 31);

    LocalTimeOffset lto = _ZN3WTF24calculateLocalTimeOffsetEdNS_8TimeTypeE(0.0, TimeType::UTCTime);
    Require(lto.offset >= -86400000 && lto.offset <= 86400000);

    bool isLocal = false;
    double parsed = _ZN3WTF40parseES5DateFromNullTerminatedCharactersEPKcRb("2024-05-10T12:00:00Z", isLocal);
    Require(!std::isnan(parsed));
    Require(parsed > 1700000000000.0);
    Require(isLocal == false);

    bool isLocal2 = false;
    double parsedInvalid = _ZN3WTF40parseES5DateFromNullTerminatedCharactersEPKcRb("not-a-date", isLocal2);
    Require(std::isnan(parsedInvalid));

    Seconds threadCpu = _ZN3WTF7CPUTime16forCurrentThreadEv();
    Require(threadCpu.m_value >= 0.0);
}

static void TestSystem() {
    int cores = _ZN3WTF22numberOfProcessorCoresEv();
    Require(cores >= 1);

    std::uint64_t ram = _ZN3WTF7ramSizeEv();
    Require(ram >= 1024ULL * 1024ULL * 1024ULL);

    std::size_t page = _ZN3WTF8pageSizeEv();
    Require(page == 4096);

    _ZN3WTF6Config17permanentlyFreezeEv();

    char b1[16] = {0};
    char b2[16] = {0};
    Require(_ZN3WTF18constantTimeMemcmpEPKvS1_m(b1, b2, 16) == 0);
    b2[5] = 1;
    Require(_ZN3WTF18constantTimeMemcmpEPKvS1_m(b1, b2, 16) != 0);

    std::uint8_t randBytes[32] = {0};
    _ZN3WTF29cryptographicallyRandomValuesEPvm(randBytes, sizeof(randBytes));
    bool hasNonZero = false;
    for (std::size_t i = 0; i < sizeof(randBytes); ++i) {
        if (randBytes[i] != 0) {
            hasNonZero = true;
            break;
        }
    }
    Require(hasNonZero);
}

static void TestString() {
    const char* ascii = "hello world";
    unsigned int dataLen = 0;
    unsigned int u16Len = 0;
    unsigned int hash = _ZN3WTF7Unicode51calculateStringHashAndLengthFromUTF8MaskingTop8BitsEPKcS2_RjS3_(
        ascii, ascii + std::strlen(ascii), dataLen, u16Len);
    Require(hash != 0);
    Require(dataLen == std::strlen(ascii));
    Require(u16Len == std::strlen(ascii));
    Require((hash & 0xFF000000U) == 0);

    const char* utf8 = "caf\xc3\xa9";
    unsigned int hash2 = _ZN3WTF7Unicode51calculateStringHashAndLengthFromUTF8MaskingTop8BitsEPKcS2_RjS3_(
        utf8, utf8 + std::strlen(utf8), dataLen, u16Len);
    Require(hash2 != 0);
    Require(dataLen == 5);
    Require(u16Len == 4);

    const char16_t numStr[] = u"123.456 extra";
    std::size_t parsedLen = 0;
    double d = _ZN3WTF8Internal25parseDoubleFromLongStringEPKDsmRm(numStr, sizeof(numStr) / sizeof(char16_t) - 1, parsedLen);
    Require(std::abs(d - 123.456) < 1e-6);
    Require(parsedLen == 7);
}

static void TestWebCorePure() {
    Require(_ZN7WebCore30isStatusCodeCacheableByDefaultEi(200) == true);
    Require(_ZN7WebCore30isStatusCodeCacheableByDefaultEi(404) == true);
    Require(_ZN7WebCore30isStatusCodeCacheableByDefaultEi(403) == false);
    Require(_ZN7WebCore30isStatusCodeCacheableByDefaultEi(500) == false);

    Require(_ZN7WebCore32isStatusCodePotentiallyCacheableEi(302) == true);
    Require(_ZN7WebCore32isStatusCodePotentiallyCacheableEi(403) == true);
    Require(_ZN7WebCore32isStatusCodePotentiallyCacheableEi(500) == false);

    Require(_ZN7WebCore43initializeMaximumHTTPConnectionCountPerHostEv() == 4);
    _ZN7WebCore27setPresentingApplicationPIDEi(1234);

    FloatRect container{FloatPoint{0.0f, 0.0f}, FloatSize{200.0f, 100.0f}};
    FloatRect inside = _ZN7WebCore36largestRectWithAspectRatioInsideRectEfRKNS_9FloatRectE(1.0f, container);
    Require(std::abs(inside.m_size.m_width - 100.0f) < 1e-3f);
    Require(std::abs(inside.m_size.m_height - 100.0f) < 1e-3f);
    Require(std::abs(inside.m_location.m_x - 50.0f) < 1e-3f);

    FloatRect around = _ZN7WebCore37smallestRectWithAspectRatioAroundRectEfRKNS_9FloatRectE(1.0f, container);
    Require(std::abs(around.m_size.m_width - 200.0f) < 1e-3f);
    Require(std::abs(around.m_size.m_height - 200.0f) < 1e-3f);

    FloatRect fr{FloatPoint{10.2f, 20.8f}, FloatSize{100.4f, 50.6f}};
    IntRect ir = _ZN7WebCore14roundedIntRectERKNS_9FloatRectE(fr);
    Require(ir.m_location.m_x == 10);
    Require(ir.m_location.m_y == 21);
    Require(ir.m_size.m_width == 100);
    Require(ir.m_size.m_height == 51);
}

static void TestJscAndPal() {
    double arr[4] = {1.0, 2.0, 3.0, 4.0};
    _ZN3JSC16clearArrayMemsetEPdj(arr, 4);
    for (int i = 0; i < 4; ++i) {
        Require(std::isnan(arr[i]));
    }

    _ZN3JSC18enableSuperSamplerEv();
    _ZN3JSC19disableSuperSamplerEv();
    _ZN3JSC22printSuperSamplerStateEv();

    Require(_ZN3JSC25numberOfOSRExitFuzzChecksEv() == 0);
    Require(_ZN3JSC31numberOfStaticOSRExitFuzzChecksEv() == 0);
    Require(_ZN3JSC27numberOfExceptionFuzzChecksEv() == 0);
    Require(_ZN3JSC38numberOfExecutableAllocationFuzzChecksEv() == 0);

    _ZN3JSC27finalizeStatsAtEndOfTestingEv();
    _ZN3JSC20waitForVMDestructionEv();
    _ZN3JSC30waitForAsynchronousDisassemblyEv();
    _ZN3JSC18logTotalPhaseTimesEv();

    _ZN3PAL10systemBeepEv();

    void* stack[8] = {nullptr};
    int frames = 0;
    WTFGetBacktrace(stack, &frames);
    Require(frames >= 0);
    Require(WTFIsDebuggerAttached() == false);
}

void TestAdditionalWtf();

int main() {
    TestAdditionalWtf();
    TestMemoryAllocations();
    TestAllocationBoundaries();
    TestTime();
    TestSystem();
    TestString();
    TestWebCorePure();
    TestJscAndPal();
    return 0;
}

namespace Wtf2Tests {

using namespace NKWebKit::WtfAbi;

extern "C" {
}

const auto Create8 = &_ZN3WTF10StringImpl6createEPKhj;
const auto Create16 = &_ZN3WTF10StringImpl6createEPKDsj;
const auto Narrow = &_ZN3WTF10StringImpl20create8BitIfPossibleEPKDsj;
const auto Uninit8 = &_ZN3WTF10StringImpl19createUninitializedEjRPh;
const auto Uninit16 = &_ZN3WTF10StringImpl19createUninitializedEjRPDs;
const auto ImplLower = &_ZN3WTF10StringImpl23convertToASCIILowercaseEv;
const auto ImplUpper = &_ZN3WTF10StringImpl23convertToASCIIUppercaseEv;
const auto Lower = &_ZNK3WTF6String23convertToASCIILowercaseEv;
const auto Upper = &_ZNK3WTF6String23convertToASCIIUppercaseEv;
const auto ViewLower = &_ZNK3WTF10StringView23convertToASCIILowercaseEv;
const auto ViewUpper = &_ZNK3WTF10StringView23convertToASCIIUppercaseEv;
const auto ImplUTF8 = &_ZNK3WTF10StringImpl4utf8ENS_14ConversionModeE;
const auto UTF8 = &_ZNK3WTF6String4utf8ENS_14ConversionModeE;
const auto ViewUTF8 = &_ZNK3WTF10StringView4utf8ENS_14ConversionModeE;
const auto FromUTF8 = &_ZN3WTF6String8fromUTF8EPKhm;
const auto FromUTF8Z = &_ZN3WTF6String8fromUTF8EPKh;
const auto FromCString = &_ZN3WTF6String8fromUTF8ERKNS_7CStringE;
const auto LatinFallback = &_ZN3WTF6String26fromUTF8WithLatin1FallbackEPKhm;
const auto FromCodePoint = &_ZN3WTF6String13fromCodePointEi;
const auto Character = &_ZNK3WTF6String19characterStartingAtEj;
const auto ImplCharacter = &_ZN3WTF10StringImpl19characterStartingAtEj;
const auto Hash = &_ZNK3WTF10StringImpl12hashSlowCaseEv;
const auto ConcurrentHash = &_ZNK3WTF10StringImpl14concurrentHashEv;
const auto BytesSize = &_ZNK3WTF10StringImpl11sizeInBytesEv;
const auto Starts = &_ZNK3WTF10StringImpl10startsWithEDs;
const auto Ends = &_ZNK3WTF10StringImpl8endsWithEDs;
const auto Prefix = &_ZNK3WTF10StringImpl10startsWithEPKcj;
const auto Suffix = &_ZNK3WTF10StringImpl8endsWithEPKcj;
const auto ViewStarts = &_ZNK3WTF10StringView10startsWithEDs;
const auto ViewEnds = &_ZNK3WTF10StringView8endsWithEDs;
const auto ReverseFind = &_ZN3WTF10StringImpl11reverseFindEDsj;
const auto Substring = &_ZN3WTF10StringImpl9substringEjj;
const auto SharedSubstring = &_ZNK3WTF6String20substringSharingImplEjj;
const auto Safe = &_ZNK3WTF6String27isSafeToSendToAnotherThreadEv;
const auto Ascii = &_ZNK3WTF6String5asciiEv;
const auto Latin1 = &_ZNK3WTF6String6latin1Ev;
const auto Find = &_ZNK3WTF10StringView4findES0_j;
const auto Case8 = &_ZNK3WTF10StringView26getCharactersWithASCIICaseENS0_15CaseConvertTypeEPh;
const auto Case16 = &_ZNK3WTF10StringView26getCharactersWithASCIICaseENS0_15CaseConvertTypeEPDs;
const auto EqualViews = &_ZN3WTF22equalRespectingNullityENS_10StringViewES0_;
const auto Equal = &_ZN3WTF5equalEPKNS_10StringImplES2_;
const auto EqualRefs = &_ZN3WTF5equalERKNS_10StringImplES2_;
const auto Equal8 = &_ZN3WTF5equalEPKNS_10StringImplEPKhj;
const auto Equal16 = &_ZN3WTF5equalEPKNS_10StringImplEPKDsj;
const auto EqualZ = &_ZN3WTF5equalEPKNS_10StringImplEPKh;
const auto EqualNullity = &_ZN3WTF20equalIgnoringNullityEPNS_10StringImplES1_;
const auto EqualRawNullity = &_ZN3WTF20equalIgnoringNullityEPKDsmPNS_10StringImplE;
const auto EqualCase = &_ZN3WTF22equalIgnoringASCIICaseEPKNS_10StringImplES2_;
const auto EqualCaseNonNull = &_ZN3WTF29equalIgnoringASCIICaseNonNullEPKNS_10StringImplES2_;
const auto CStringNew = &_ZN3WTF7CString16newUninitializedEmRPc;
const auto CStringMutable = &_ZN3WTF7CString11mutableDataEv;
const auto CStringGrow = &_ZN3WTF7CString4growEm;
const auto CStringHash = &_ZNK3WTF7CString4hashEv;
const auto CStringEqual = &_ZN3WTFeqERKNS_7CStringES2_;
const auto CStringEqualZ = &_ZN3WTFeqERKNS_7CStringEPKc;
const auto CStringLess = &_ZN3WTFltERKNS_7CStringES2_;
const auto CStringHashEqual = &_ZN3WTF11CStringHash5equalERKNS_7CStringES3_;
const auto AllPrivileges = &_ZN3WTF13allPrivilegesEv;
const auto SetPrivileges = &_ZN3WTF20setProcessPrivilegesENS_9OptionSetINS_16ProcessPrivilegeEEE;
const auto AddPrivilege = &_ZN3WTF19addProcessPrivilegeENS_16ProcessPrivilegeE;
const auto RemovePrivilege = &_ZN3WTF22removeProcessPrivilegeENS_16ProcessPrivilegeE;
const auto HasPrivilege = &_ZN3WTF19hasProcessPrivilegeENS_16ProcessPrivilegeE;
const auto SixHash = &_ZN3WTF31sixCharacterHashStringToIntegerEPKc;
const auto IsReload = &_ZN7WebCore8isReloadENS_13FrameLoadTypeE;
const auto IsBackForward = &_ZN7WebCore21isBackForwardLoadTypeENS_13FrameLoadTypeE;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WTF additional test failed at line %u\n", location.line());
        std::abort();
    }
}

template<typename T> void Throws(T action) {
    bool threw = false;
    try { action(); } catch (const std::exception&) { threw = true; }
    Check(threw);
}

const unsigned char* U8(const char* data) { return reinterpret_cast<const unsigned char*>(data); }
std::string Bytes(const CString& value) {
    return value.buffer ? std::string(reinterpret_cast<const char*>(value.buffer + 1), value.buffer->length) : std::string();
}
void Text(const String& value, const char* expected) {
    Check(value.impl != nullptr);
    Check(Bytes(UTF8(&value, 1)) == expected);
}

void TestStorageAndQueries() {
    String null;
    auto empty = Create8(U8(""), 0);
    Check(null.impl == nullptr && empty.impl != nullptr && empty.impl->length == 0);
    auto fromNull = Create16(nullptr, 99);
    Check(fromNull.impl == empty.impl);
    unsigned char* data8 = reinterpret_cast<unsigned char*>(1);
    auto empty8 = Uninit8(0, data8);
    Check(!data8 && empty8.impl == empty.impl);
    char16_t* data16 = reinterpret_cast<char16_t*>(1);
    auto empty16 = Uninit16(0, data16);
    Check(!data16 && empty16.impl == empty.impl);
    auto made8 = Uninit8(3, data8);
    std::memcpy(data8, "aBa", 3);
    Check(made8.impl->refCount == 2 && made8.impl->hashAndFlags == 4);
    Check(made8.impl->data == reinterpret_cast<unsigned char*>(made8.impl) + 20);
    auto made16 = Uninit16(3, data16);
    data16[0] = 'a'; data16[1] = 'B'; data16[2] = 'a';
    Check(made16.impl->hashAndFlags == 0);
    Check(Equal(made8.impl, made16.impl) && EqualRefs(*made8.impl, *made16.impl));
    Check(Equal8(made16.impl, U8("aBa"), 3) && Equal16(made8.impl, u"aBa", 3));
    Check(EqualZ(made16.impl, U8("aBa")) && !EqualZ(made16.impl, U8("aB")));
    Check(Equal(nullptr, nullptr) && !Equal(nullptr, empty.impl));
    Check(Equal8(nullptr, nullptr, 4) && !Equal8(empty.impl, nullptr, 0));
    Check(EqualNullity(nullptr, empty.impl) && EqualRawNullity(nullptr, 0, empty.impl));
    Check(!EqualNullity(nullptr, made8.impl) && !EqualRawNullity(u"aB", 2, made8.impl));
    auto lower = Create8(U8("aba"), 3);
    Check(EqualCase(made16.impl, lower.impl) && EqualCaseNonNull(made8.impl, lower.impl));
    Check(EqualCase(nullptr, nullptr) && !EqualCase(nullptr, empty.impl));
    Throws([&] { EqualCaseNonNull(nullptr, lower.impl); });
    Check(BytesSize(made8.impl) == 27 && BytesSize(made16.impl) == 30);
    Check(ConcurrentHash(made8.impl) == ConcurrentHash(made16.impl));
    Check(made8.impl->hashAndFlags == 4);
    auto hash = Hash(made8.impl);
    Check(hash != 0 && made8.impl->hashAndFlags == ((hash << 8) | 4));
    Check(Hash(made16.impl) == hash);
    auto hello = Create8(U8("hello"), 5);
    Check(Hash(hello.impl) == 0x00D17551U);
    unsigned bytesRead = 99;
    unsigned utf16Read = 99;
    const char* valid = "hello";
    Check(_ZN3WTF7Unicode51calculateStringHashAndLengthFromUTF8MaskingTop8BitsEPKcS2_RjS3_(valid, valid + 5, bytesRead, utf16Read) == 0x00D17551U);
    Check(bytesRead == 5 && utf16Read == 5);
    for (const char* invalid : {"\xc0\xaf", "\xe0\x80\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe2\x82"}) {
        bytesRead = 99;
        Check(!_ZN3WTF7Unicode51calculateStringHashAndLengthFromUTF8MaskingTop8BitsEPKcS2_RjS3_(invalid, invalid + std::strlen(invalid), bytesRead, utf16Read));
        Check(bytesRead == 99 && utf16Read == 0);
    }
    auto released = Create8(U8("fastMalloc"), 10);
    auto* storage = std::exchange(released.impl, nullptr);
    Check(_ZN3WTF14fastMallocSizeEPKv(storage) >= 30);
    _ZN3WTF8fastFreeEPv(storage);
    Check(Starts(made8.impl, 'a') && Ends(made16.impl, 'a'));
    Check(!Starts(empty.impl, 0) && !Ends(empty.impl, 0));
    Check(Prefix(made16.impl, "aB", 2) && Suffix(made8.impl, "Ba", 2));
    Check(Prefix(empty.impl, nullptr, 0) && !Prefix(made8.impl, "aBab", 4));
    Check(!Suffix(made16.impl, "bb", 2));
    Throws([&] { Prefix(made8.impl, nullptr, 1); });
    Check(ReverseFind(made8.impl, 'a', 99) == 2 && ReverseFind(made16.impl, 'a', 1) == 0);
    Check(ReverseFind(empty.impl, 'a', 0) == std::size_t(-1));
    Check(ReverseFind(made8.impl, 0x100, 99) == std::size_t(-1));
    auto narrow = Narrow(u"caf\u00e9", 4);
    Check((narrow.impl->hashAndFlags & 4) != 0);
    auto wide = Narrow(u"x\u0100", 2);
    Check((wide.impl->hashAndFlags & 4) == 0);
    Check(Character(&null, 0) == 0 && Character(&made8, 99) == 0);
    Check(ImplCharacter(made8.impl, 1) == 'B');
    Throws([&] { ImplCharacter(made8.impl, 3); });
    Throws([&] { BytesSize(nullptr); });
    Throws([&] { Uninit16(0xFFFFFFFF, data16); });
    Check(Safe(&null) && Safe(&made8) && Safe(&empty));
    { auto copy = made8; Check(!Safe(&made8)); }
    Check(Safe(&made8));
}

void TestASCIIAndViews() {
    auto original = Create16(u"Ab\u00c9z", 4);
    auto lower = ImplLower(original.impl);
    auto upper = ImplUpper(original.impl);
    Check(Equal16(lower.impl, u"ab\u00c9z", 4));
    Check(Equal16(upper.impl, u"AB\u00c9Z", 4));
    { auto same = ImplLower(lower.impl); Check(same.impl == lower.impl && lower.impl->refCount == 4); }
    Check(lower.impl->refCount == 2);
    Check(Equal16(Lower(&original).impl, u"ab\u00c9z", 4));
    Check(Equal16(Upper(&original).impl, u"AB\u00c9Z", 4));
    String null;
    Check(!Lower(&null).impl && !Upper(&null).impl);
    StringView view{U8("aBcD"), 4, true};
    StringView wide{u"aBcD", 4, false};
    StringView empty{"", 0, true};
    StringView absent{nullptr, 0, true};
    Text(ViewLower(&view), "abcd");
    Text(ViewUpper(&wide), "ABCD");
    Check(!ViewLower(&absent).impl && ViewUpper(&empty).impl);
    Check(ViewStarts(&view, 'a') && ViewEnds(&wide, 'D'));
    Check(!ViewStarts(&absent, 0) && !ViewEnds(&empty, 0));
    Check(EqualViews(view, wide) && !EqualViews(absent, empty));
    Check(EqualViews(absent, absent) && EqualViews(empty, empty));
    StringView match{u"Bc", 2, false};
    Check(Find(&view, match, 0) == 1 && Find(&wide, {"D", 1, true}, 2) == 3);
    Check(Find(&view, match, 2) == std::size_t(-1));
    Check(Find(&view, absent, 4) == 4 && Find(&view, absent, 5) == std::size_t(-1));
    unsigned char output8[5] = {0, 0, 0, 0, 77};
    char16_t output16[5] = {0, 0, 0, 0, 77};
    Case8(&view, 0, output8);
    Case16(&view, 1, output16);
    Check(!std::memcmp(output8, "ABCD", 4) && output8[4] == 77);
    Check(!std::memcmp(output16, u"abcd", 8) && output16[4] == 77);
    Case16(&wide, 0, output16);
    Check(!std::memcmp(output16, u"ABCD", 8));
    Case8(&absent, 0, nullptr);
    Throws([&] { Case8(&wide, 1, output8); });
    Throws([&] { Case16(&wide, 1, nullptr); });
    auto special = Create16(u"\0\n\x7f\u00e9\u0100", 5);
    Check(Bytes(Ascii(&special)) == std::string("\0?\x7f??", 5));
    Check(Bytes(Latin1(&special)) == std::string("\0\n\x7f\xe9?", 5));
    Check(Ascii(&null).buffer != nullptr && Latin1(&null).buffer != nullptr);
}

void TestUTFConversions() {
    String null;
    Check(Bytes(UTF8(&null, 1)).empty());
    Check(!FromUTF8(nullptr, 9).impl && !FromUTF8Z(nullptr).impl);
    auto ascii = FromUTF8(U8("abc"), 3);
    Check((ascii.impl->hashAndFlags & 4) != 0);
    auto unicode = FromUTF8(U8("caf\xc3\xa9\xf0\x9f\x98\x80"), 9);
    Check(unicode.impl && unicode.impl->length == 6 && !(unicode.impl->hashAndFlags & 4));
    Check(Character(&unicode, 4) == 0x1F600 && Character(&unicode, 5) == 0);
    Check(Bytes(ImplUTF8(unicode.impl, 1)) == "caf\xc3\xa9\xf0\x9f\x98\x80");
    StringView view{unicode.impl->data, unicode.impl->length, false};
    Check(Bytes(ViewUTF8(&view, 1)) == Bytes(UTF8(&unicode, 1)));
    for (const char* invalid : {"\xc0\xaf", "\xe0\x80\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\x80", "\xe2\x82"}) {
        Check(!FromUTF8Z(U8(invalid)).impl);
    }
    auto fallback = LatinFallback(U8("\xff"), 1);
    Check(Equal16(fallback.impl, u"\xff", 1) && (fallback.impl->hashAndFlags & 4));
    auto zero = FromUTF8(U8("a\0b"), 3);
    Check(zero.impl->length == 3 && Character(&zero, 1) == 0);
    char16_t bad[] = {0xD800, 'X', 0xDC00, 0xDFFF};
    auto malformed = Create16(bad, 4);
    Check(ImplCharacter(malformed.impl, 0) == 0 && ImplCharacter(malformed.impl, 2) == 0);
    Check(Bytes(UTF8(&malformed, 0)) == "\xed\xa0\x80X\xed\xb0\x80\xed\xbf\xbf");
    Check(Bytes(UTF8(&malformed, 2)) == "\xef\xbf\xbdX\xef\xbf\xbd\xef\xbf\xbd");
    Throws([&] { UTF8(&malformed, 1); });
    Text(FromCodePoint(0x1F600), "\xf0\x9f\x98\x80");
    Check(!FromCodePoint(-1).impl && !FromCodePoint(0x110000).impl);
    auto surrogate = FromCodePoint(0xD800);
    Check(surrogate.impl && surrogate.impl->length == 1 && Character(&surrogate, 0) == 0);
    Throws([&] { FromUTF8(U8(""), std::size_t(0x80000000)); });
}

void TestSubstrings() {
    auto original = Create8(U8("abcdefghijklmnopqrstuvwx"), 24);
    { auto same = Substring(original.impl, 0, 99); Check(same.impl == original.impl); }
    Text(Substring(original.impl, 23, 99), "x");
    Check(Substring(original.impl, 99, 9).impl->length == 0);
    auto shared = SharedSubstring(&original, 1, 20);
    Check((shared.impl->hashAndFlags & 3) == 2);
    Check(shared.impl->data == static_cast<const unsigned char*>(original.impl->data) + 1);
    Check(original.impl->refCount == 4);
    {
        auto nested = SharedSubstring(&shared, 1, 18);
        Text(nested, "cdefghijklmnopqrst");
        Check(original.impl->refCount == 6 && shared.impl->refCount == 2);
    }
    Check(original.impl->refCount == 4);
    auto small = SharedSubstring(&original, 1, 3);
    Check((small.impl->hashAndFlags & 3) == 0);
    Text(small, "bcd");
    String null;
    Check(!SharedSubstring(&null, 5, 9).impl);
    auto wide = Create16(u"abcdefghijklmnopqrstuvwx", 24);
    auto wideShared = SharedSubstring(&wide, 2, 20);
    Check((wideShared.impl->hashAndFlags & 3) == 2 && !(wideShared.impl->hashAndFlags & 4));
    Text(wideShared, "cdefghijklmnopqrstuv");
}

void TestCStrings() {
    char* data;
    auto value = CStringNew(3, data);
    Check(data[3] == 0 && value.buffer->refCount == 1);
    std::memcpy(data, "abc", 3);
    CString shared(value);
    Check(value.buffer->refCount == 2);
    char* writable = CStringMutable(&shared);
    writable[0] = 'z';
    Check(Bytes(value) == "abc" && Bytes(shared) == "zbc");
    Check(value.buffer->refCount == 1 && shared.buffer->refCount == 1);
    Check(CStringLess(value, shared) && !CStringEqual(value, shared));
    Check(CStringEqualZ(value, "abc") && !CStringEqualZ(value, nullptr));
    auto embedded = CStringNew(3, data);
    std::memcpy(data, "a\0b", 3);
    auto different = CStringNew(3, data);
    std::memcpy(data, "a\0c", 3);
    Check(!CStringEqual(embedded, different) && CStringEqualZ(embedded, "a"));
    Check(CStringHash(&embedded) == CStringHash(&different));
    Check(!CStringHashEqual(embedded, different));
    CString absent;
    auto empty = CStringNew(0, data);
    Check(data[0] == 0 && !CStringEqual(absent, empty));
    Check(CStringHash(&absent) == 0 && CStringHash(&empty) != 0);
    Check(CStringEqualZ(absent, nullptr) && CStringLess(absent, empty));
    Check(!CStringMutable(&absent));
    CStringGrow(&value, 8);
    Check(value.buffer->length == 8 && !std::memcmp(Bytes(value).data(), "abc\0", 4));
    writable = CStringMutable(&value);
    std::memcpy(writable + 3, "defgh", 5);
    writable[8] = 0;
    Check(Bytes(value) == "abcdefgh");
    Throws([&] { CStringGrow(&value, 8); });
    Throws([&] { CStringGrow(&absent, 1); });
    Throws([&] { CStringNew(std::size_t(-1), data); });
    Text(FromCString(embedded), "a");
    Check(!FromCString(absent).impl);
    auto deleted = reinterpret_cast<CStringBuffer*>(std::uintptr_t(-1));
    CString a(deleted), b(deleted);
    Check(CStringHashEqual(a, b) && !CStringHashEqual(a, value));
    a.buffer = nullptr; b.buffer = nullptr;
}

void TestPrimitiveState() {
    Check(AllPrivileges().storage == 7);
    SetPrivileges({0});
    Check(!HasPrivilege(1));
    AddPrivilege(1); AddPrivilege(4);
    Check(HasPrivilege(1) && HasPrivilege(4) && !HasPrivilege(2));
    RemovePrivilege(1);
    Check(!HasPrivilege(1) && HasPrivilege(4));
    SetPrivileges({2});
    Check(HasPrivilege(2) && !HasPrivilege(4));
    SetPrivileges({0});
    Check(SixHash("AAAAAA") == 0 && SixHash("AAAAAB") == 1);
    Check(SixHash("AAAAAa") == 26 && SixHash("AAAAA0") == 52);
    Check(SixHash("999999") == 965660735U);
    Throws([&] { SixHash(nullptr); });
    Throws([&] { SixHash("short"); });
    Throws([&] { SixHash("AAAAAAA"); });
    Throws([&] { SixHash("AAAAA!"); });
    for (std::uint8_t type = 0; type < 10; ++type) {
        Check(IsReload(type) == (type == 4 || type == 8 || type == 9));
        Check(IsBackForward(type) == (type >= 1 && type <= 3));
    }
    Throws([&] { IsReload(255); });
    Throws([&] { IsBackForward(255); });
}

void TestIntegersAndConstructors() {
    Text(_ZN3WTF6String6numberEi(std::numeric_limits<int>::min()), "-2147483648");
    Text(_ZN3WTF6String6numberEi(0), "0");
    Text(_ZN3WTF6String6numberEj(std::numeric_limits<unsigned>::max()), "4294967295");
    Text(_ZN3WTF6String6numberEj(0), "0");
    Text(_ZN3WTF6String6numberEl(std::numeric_limits<std::int64_t>::min()), "-9223372036854775808");
    Text(_ZN3WTF6String6numberEl(0), "0");
    Text(_ZN3WTF6String6numberEm(std::numeric_limits<std::uint64_t>::max()), "18446744073709551615");
    Text(_ZN3WTF6String6numberEm(0), "0");
    Text(_ZN3WTF6String6numberEx(std::numeric_limits<long long>::min()), "-9223372036854775808");
    Text(_ZN3WTF6String6numberEx(0), "0");
    Text(_ZN3WTF6String6numberEy(std::numeric_limits<unsigned long long>::max()), "18446744073709551615");
    Text(_ZN3WTF6String6numberEy(0), "0");
    { String value; _ZN3WTF6StringC1EPKDsj(&value, u"test", 4); Text(value, "test"); }
    { String value; _ZN3WTF6StringC1EPKDsj(&value, nullptr, 0); Check(!value.impl); }
    { String value; _ZN3WTF6StringC2EPKDsj(&value, u"test", 4); Text(value, "test"); }
    { String value; _ZN3WTF6StringC2EPKDsj(&value, nullptr, 0); Check(!value.impl); }
    { String value; _ZN3WTF6StringC1EPKc(&value, "test"); Text(value, "test"); }
    { String value; _ZN3WTF6StringC1EPKc(&value, nullptr); Check(!value.impl); }
    { String value; _ZN3WTF6StringC2EPKc(&value, "test"); Text(value, "test"); }
    { String value; _ZN3WTF6StringC2EPKc(&value, nullptr); Check(!value.impl); }
    { String value; _ZN3WTF6StringC1EPKcj(&value, "test", 4); Text(value, "test"); }
    { String value; _ZN3WTF6StringC1EPKcj(&value, nullptr, 0); Check(!value.impl); }
    { String value; _ZN3WTF6StringC2EPKcj(&value, "test", 4); Text(value, "test"); }
    { String value; _ZN3WTF6StringC2EPKcj(&value, nullptr, 0); Check(!value.impl); }
    { String value; _ZN3WTF6StringC1EPKhj(&value, U8("test"), 4); Text(value, "test"); }
    { String value; _ZN3WTF6StringC1EPKhj(&value, nullptr, 0); Check(!value.impl); }
    { String value; _ZN3WTF6StringC2EPKhj(&value, U8("test"), 4); Text(value, "test"); }
    { String value; _ZN3WTF6StringC2EPKhj(&value, nullptr, 0); Check(!value.impl); }
    { CString value; _ZN3WTF7CStringC1EPKc(&value, "test"); Check(Bytes(value) == "test"); }
    { CString value; _ZN3WTF7CStringC1EPKc(&value, nullptr); Check(!value.buffer); }
    { CString value; _ZN3WTF7CStringC2EPKc(&value, "test"); Check(Bytes(value) == "test"); }
    { CString value; _ZN3WTF7CStringC2EPKc(&value, nullptr); Check(!value.buffer); }
    { CString value; _ZN3WTF7CStringC1EPKcm(&value, "test", 4); Check(Bytes(value) == "test"); }
    { CString value; _ZN3WTF7CStringC1EPKcm(&value, nullptr, 0); Check(!value.buffer); }
    { CString value; _ZN3WTF7CStringC2EPKcm(&value, "test", 4); Check(Bytes(value) == "test"); }
    { CString value; _ZN3WTF7CStringC2EPKcm(&value, nullptr, 0); Check(!value.buffer); }
}

void Run() {
    TestStorageAndQueries();
    TestASCIIAndViews();
    TestUTFConversions();
    TestSubstrings();
    TestCStrings();
    TestPrimitiveState();
    TestIntegersAndConstructors();
}

}  // namespace Wtf2Tests

namespace Wtf2Tests { void TestURLsAndReallocation(); }

void TestAdditionalWtf() { Wtf2Tests::Run(); Wtf2Tests::TestURLsAndReallocation(); }

namespace Wtf2Tests {

extern "C" {
}

const auto URLProtocol = &_ZNK3WTF3URL8protocolEv;
const auto URLHost = &_ZNK3WTF3URL4hostEv;
const auto URLPath = &_ZNK3WTF3URL4pathEv;
const auto URLUser = &_ZNK3WTF3URL11encodedUserEv;
const auto URLPassword = &_ZNK3WTF3URL15encodedPasswordEv;
const auto URLFragment = &_ZNK3WTF3URL18fragmentIdentifierEv;
const auto URLFragmentSign = &_ZNK3WTF3URL39fragmentIdentifierWithLeadingNumberSignEv;
const auto URLQuery = &_ZNK3WTF3URL28queryWithLeadingQuestionMarkEv;
const auto URLIsProtocol = &_ZNK3WTF3URL10protocolIsENS_10StringViewE;
const auto URLHierarchical = &_ZNK3WTF3URL14isHierarchicalEv;
const auto URLBlank = &_ZNK3WTF3URL12isAboutBlankEv;
const auto URLSrcdoc = &_ZNK3WTF3URL13isAboutSrcDocEv;
const auto URLJavaScript = &_ZNK3WTF3URL20protocolIsJavaScriptEv;
const auto URLInvalidate = &_ZN3WTF3URL10invalidateEv;
const auto URLEqualIgnoringFragment = &_ZN3WTF31equalIgnoringFragmentIdentifierERKNS_3URLES2_;
const auto URLPortAllowed = &_ZN3WTF11portAllowedERKNS_3URLE;
const auto IsJavaScript = &_ZN3WTF20protocolIsJavaScriptENS_10StringViewE;
const auto IsHTTP = &_ZN3WTF22protocolIsInHTTPFamilyENS_10StringViewE;
const auto JSRealloc = &_ZN3WTF14jsValueReallocEPvm;

std::string ViewText(StringView view) { return Bytes(ViewUTF8(&view, 1)); }

void TestURLsAndReallocation() {
    struct PinnedBits {
        unsigned valid : 1;
        unsigned http : 1;
        unsigned cannotBeBase : 1;
        unsigned portLength : 3;
        unsigned schemeEnd : 26;
    };
    static_assert(sizeof(PinnedBits) == 4);
    PinnedBits fields{1, 1, 0, 5, 5};
    unsigned bits;
    std::memcpy(&bits, &fields, sizeof(bits));
    Check(bits == (1U | 2U | (5U << 3) | (5U << 6)));
    auto string = FromUTF8Z(U8("https://u%20:p%40@example.test:8443/a/b?q=1#frag"));
    URL url{string.impl, bits, 8, 12, 17, 29, 37, 38, 42};
    std::string raw = Bytes(UTF8(&string, 1));
    url.hostEnd = static_cast<unsigned>(raw.find(":8443"));
    url.pathAfterLastSlash = static_cast<unsigned>(raw.rfind('/') + 1);
    url.pathEnd = static_cast<unsigned>(raw.find('?'));
    url.queryEnd = static_cast<unsigned>(raw.find('#'));
    Check(ViewText(URLProtocol(&url)) == "https");
    Check(ViewText(URLHost(&url)) == "example.test");
    Check(ViewText(URLUser(&url)) == "u%20" && ViewText(URLPassword(&url)) == "p%40");
    Check(ViewText(URLPath(&url)) == "/a/b");
    Check(ViewText(URLQuery(&url)) == "?q=1");
    Check(ViewText(URLFragment(&url)) == "frag" && ViewText(URLFragmentSign(&url)) == "#frag");
    Check(URLIsProtocol(&url, {"https", 5, true}) && !URLIsProtocol(&url, {"http", 4, true}));
    Check(URLHierarchical(&url) && !URLBlank(&url) && !URLSrcdoc(&url));
    auto wideString = Create16(u"http://wide.test/path", 21);
    URL wide{wideString.impl, 1U | 2U | (4U << 6), 7, 7, 7, 16, 17, 21, 21};
    Check(ViewText(URLProtocol(&wide)) == "http" && !URLHost(&wide).is8Bit);
    Check(ViewText(URLHost(&wide)) == "wide.test" && ViewText(URLPath(&wide)) == "/path");
    auto about = FromUTF8Z(U8("about:blank#"));
    URL blank{about.impl, 1U | 4U | (5U << 6), 6, 6, 6, 6, 6, 11, 11};
    Check(URLBlank(&blank) && !URLSrcdoc(&blank) && !URLHierarchical(&blank));
    Check(URLFragment(&blank).data && URLFragment(&blank).length == 0);
    auto src = FromUTF8Z(U8("about:srcdoc"));
    blank.impl = src.impl; blank.pathEnd = 12; blank.queryEnd = 12;
    Check(URLSrcdoc(&blank) && !URLBlank(&blank));
    URL absent{};
    Check(!URLProtocol(&absent).data && !URLHost(&absent).data && !URLPath(&absent).data);
    Check(!URLPassword(&absent).data && !URLQuery(&absent).data && !URLFragment(&absent).data);
    Check(URLUser(&absent).data && URLUser(&absent).length == 0);
    Check(!URLHierarchical(&absent) && !URLIsProtocol(&absent, {"", 0, true}));
    auto js = FromUTF8Z(U8(" \tJaVa\nScRiPt:alert(1)"));
    URL javascript{js.impl, 0, 0, 0, 0, 0, 0, 0, 0};
    Check(URLJavaScript(&javascript));
    Check(IsJavaScript({js.impl->data, js.impl->length, true}));
    Check(!IsJavaScript({"javascriptx:", 12, true}) && !IsJavaScript({nullptr, 0, true}));
    Check(IsHTTP({"HTTP:", 5, true}) && IsHTTP({u"HtTpS:", 6, false}));
    Check(!IsHTTP({" https:", 7, true}) && !IsHTTP({"httpx:", 6, true}));
    Check(URLEqualIgnoringFragment(&url, &url));
    auto urlDiffFragStr = FromUTF8Z(U8("https://u%20:p%40@example.test:8443/a/b?q=1#other"));
    URL urlDiffFrag = url;
    urlDiffFrag.impl = urlDiffFragStr.impl;
    std::string rawDiffFrag = Bytes(UTF8(&urlDiffFragStr, 1));
    urlDiffFrag.queryEnd = static_cast<unsigned>(rawDiffFrag.find('#'));
    Check(URLEqualIgnoringFragment(&url, &urlDiffFrag));
    auto urlNoFragStr = FromUTF8Z(U8("https://u%20:p%40@example.test:8443/a/b?q=1"));
    URL urlNoFrag = url;
    urlNoFrag.impl = urlNoFragStr.impl;
    std::string rawNoFrag = Bytes(UTF8(&urlNoFragStr, 1));
    urlNoFrag.queryEnd = static_cast<unsigned>(rawNoFrag.length());
    Check(URLEqualIgnoringFragment(&url, &urlNoFrag));
    auto urlEmptyFragStr = FromUTF8Z(U8("https://u%20:p%40@example.test:8443/a/b?q=1#"));
    URL urlEmptyFrag = url;
    urlEmptyFrag.impl = urlEmptyFragStr.impl;
    std::string rawEmptyFrag = Bytes(UTF8(&urlEmptyFragStr, 1));
    urlEmptyFrag.queryEnd = static_cast<unsigned>(rawEmptyFrag.find('#'));
    Check(URLEqualIgnoringFragment(&url, &urlEmptyFrag));
    Check(URLEqualIgnoringFragment(&urlNoFrag, &urlEmptyFrag));
    auto aboutNoFrag = FromUTF8Z(U8("about:blank"));
    URL blankAbout{about.impl, 1U | 4U | (5U << 6), 6, 6, 6, 6, 6, 11, 11};
    blankAbout.queryEnd = static_cast<unsigned>(Bytes(UTF8(&about, 1)).find('#'));
    URL blankNoFrag = blankAbout;
    blankNoFrag.impl = aboutNoFrag.impl;
    blankNoFrag.queryEnd = static_cast<unsigned>(Bytes(UTF8(&aboutNoFrag, 1)).length());
    Check(URLEqualIgnoringFragment(&blankAbout, &blankNoFrag));
    auto urlDiffQueryStr = FromUTF8Z(U8("https://u%20:p%40@example.test:8443/a/b?q=2#frag"));
    URL urlDiffQuery = url;
    urlDiffQuery.impl = urlDiffQueryStr.impl;
    std::string rawDiffQuery = Bytes(UTF8(&urlDiffQueryStr, 1));
    urlDiffQuery.queryEnd = static_cast<unsigned>(rawDiffQuery.find('#'));
    Check(!URLEqualIgnoringFragment(&url, &urlDiffQuery));
    auto urlDiffPathStr = FromUTF8Z(U8("https://u%20:p%40@example.test:8443/a/c?q=1#frag"));
    URL urlDiffPath = url;
    urlDiffPath.impl = urlDiffPathStr.impl;
    std::string rawDiffPath = Bytes(UTF8(&urlDiffPathStr, 1));
    urlDiffPath.queryEnd = static_cast<unsigned>(rawDiffPath.find('#'));
    Check(!URLEqualIgnoringFragment(&url, &urlDiffPath));
    auto urlDiffHostStr = FromUTF8Z(U8("https://u%20:p%40@other.test:8443/a/b?q=1#frag"));
    URL urlDiffHost = url;
    urlDiffHost.impl = urlDiffHostStr.impl;
    std::string rawDiffHost = Bytes(UTF8(&urlDiffHostStr, 1));
    urlDiffHost.queryEnd = static_cast<unsigned>(rawDiffHost.find('#'));
    Check(!URLEqualIgnoringFragment(&url, &urlDiffHost));
    auto wideHash = [](const StringImpl* impl) {
        if (!impl || !impl->data) return 0U;
        const auto* chars = static_cast<const char16_t*>(impl->data);
        for (unsigned i = 0; i < impl->length; ++i) {
            if (chars[i] == u'#') return i;
        }
        return impl->length;
    };
    auto wide1 = Create16(u"http://wide.test/path#sec1", 26);
    auto wide2 = Create16(u"http://wide.test/path#sec2", 26);
    URL urlWide1 = wide;
    urlWide1.impl = wide1.impl;
    urlWide1.queryEnd = wideHash(urlWide1.impl);
    URL urlWide2 = wide;
    urlWide2.impl = wide2.impl;
    urlWide2.queryEnd = wideHash(urlWide2.impl);
    Check(URLEqualIgnoringFragment(&urlWide1, &urlWide2));
    auto wideNoFrag = Create16(u"http://wide.test/path", 21);
    URL urlWideNoFrag = wide;
    urlWideNoFrag.impl = wideNoFrag.impl;
    urlWideNoFrag.queryEnd = wideHash(urlWideNoFrag.impl);
    auto narrow8Str = FromUTF8Z(U8("http://wide.test/path#other"));
    URL urlNarrow8 = wide;
    urlNarrow8.impl = narrow8Str.impl;
    std::string rawNarrow8 = Bytes(UTF8(&narrow8Str, 1));
    urlNarrow8.queryEnd = static_cast<unsigned>(rawNarrow8.find('#'));
    Check(URLEqualIgnoringFragment(&urlWide1, &urlNarrow8));
    Check(URLEqualIgnoringFragment(&urlNarrow8, &urlWideNoFrag));
    auto narrow8DiffStr = FromUTF8Z(U8("http://wide.test/other#sec1"));
    URL urlNarrow8Diff = wide;
    urlNarrow8Diff.impl = narrow8DiffStr.impl;
    std::string rawNarrow8Diff = Bytes(UTF8(&narrow8DiffStr, 1));
    urlNarrow8Diff.queryEnd = static_cast<unsigned>(rawNarrow8Diff.find('#'));
    Check(!URLEqualIgnoringFragment(&urlWide1, &urlNarrow8Diff));
    Check(URLEqualIgnoringFragment(&absent, &absent));
    Check(URLEqualIgnoringFragment(&javascript, &javascript));
    Check(!URLEqualIgnoringFragment(&url, &absent));
    Check(!URLEqualIgnoringFragment(&absent, &url));
    auto jsFrag1 = FromUTF8Z(U8(" \tJaVa\nScRiPt:alert(1)#x"));
    auto jsFrag2 = FromUTF8Z(U8(" \tJaVa\nScRiPt:alert(1)#y"));
    URL urlJsFrag1{jsFrag1.impl, 0, 0, 0, 0, 0, 0, 0, 0};
    std::string rawJsFrag1 = Bytes(UTF8(&jsFrag1, 1));
    urlJsFrag1.queryEnd = static_cast<unsigned>(rawJsFrag1.find('#'));
    URL urlJsFrag2{jsFrag2.impl, 0, 0, 0, 0, 0, 0, 0, 0};
    std::string rawJsFrag2 = Bytes(UTF8(&jsFrag2, 1));
    urlJsFrag2.queryEnd = static_cast<unsigned>(rawJsFrag2.find('#'));
    Check(!URLEqualIgnoringFragment(&urlJsFrag1, &urlJsFrag2));
    Check(!URLEqualIgnoringFragment(&absent, &javascript));
    Throws([&] { URLEqualIgnoringFragment(nullptr, &url); });
    Throws([&] { URLEqualIgnoringFragment(&url, nullptr); });
    auto urlNoPortStr = FromUTF8Z(U8("http://example.test/path"));
    URL urlNoPort{urlNoPortStr.impl, 1U | 2U | (0U << 3) | (4U << 6), 7, 7, 7, 19, 20, 24, 24};
    Check(URLPortAllowed(&urlNoPort));
    auto urlPort80Str = FromUTF8Z(U8("http://example.test:80/path"));
    URL urlPort80{urlPort80Str.impl, 1U | 2U | (3U << 3) | (4U << 6), 7, 7, 7, 19, 23, 27, 27};
    Check(URLPortAllowed(&urlPort80));
    auto urlPort443Str = FromUTF8Z(U8("http://example.test:443/path"));
    URL urlPort443{urlPort443Str.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
    Check(URLPortAllowed(&urlPort443));
    auto urlPort8080Str = FromUTF8Z(U8("http://example.test:8080/path"));
    URL urlPort8080{urlPort8080Str.impl, 1U | 2U | (5U << 3) | (4U << 6), 7, 7, 7, 19, 25, 29, 29};
    Check(URLPortAllowed(&urlPort8080));
    auto urlPort0Str = FromUTF8Z(U8("http://example.test:0/path"));
    URL urlPort0{urlPort0Str.impl, 1U | 2U | (2U << 3) | (4U << 6), 7, 7, 7, 19, 22, 26, 26};
    Check(URLPortAllowed(&urlPort0));
    auto urlPort10081Str = FromUTF8Z(U8("http://example.test:10081/path"));
    URL urlPort10081{urlPort10081Str.impl, 1U | 2U | (6U << 3) | (4U << 6), 7, 7, 7, 19, 26, 30, 30};
    Check(URLPortAllowed(&urlPort10081));
    auto urlPort1Str = FromUTF8Z(U8("http://example.test:1/path"));
    URL urlPort1{urlPort1Str.impl, 1U | 2U | (2U << 3) | (4U << 6), 7, 7, 7, 19, 22, 26, 26};
    Check(!URLPortAllowed(&urlPort1));
    auto urlPort7Str = FromUTF8Z(U8("http://example.test:7/path"));
    URL urlPort7{urlPort7Str.impl, 1U | 2U | (2U << 3) | (4U << 6), 7, 7, 7, 19, 22, 26, 26};
    Check(!URLPortAllowed(&urlPort7));
    auto urlPort21Str = FromUTF8Z(U8("http://example.test:21/path"));
    URL urlPort21{urlPort21Str.impl, 1U | 2U | (3U << 3) | (4U << 6), 7, 7, 7, 19, 23, 27, 27};
    Check(!URLPortAllowed(&urlPort21));
    auto urlPort22Str = FromUTF8Z(U8("http://example.test:22/path"));
    URL urlPort22{urlPort22Str.impl, 1U | 2U | (3U << 3) | (4U << 6), 7, 7, 7, 19, 23, 27, 27};
    Check(!URLPortAllowed(&urlPort22));
    auto urlPort25Str = FromUTF8Z(U8("http://example.test:25/path"));
    URL urlPort25{urlPort25Str.impl, 1U | 2U | (3U << 3) | (4U << 6), 7, 7, 7, 19, 23, 27, 27};
    Check(!URLPortAllowed(&urlPort25));
    auto urlPort87Str = FromUTF8Z(U8("http://example.test:87/path"));
    URL urlPort87{urlPort87Str.impl, 1U | 2U | (3U << 3) | (4U << 6), 7, 7, 7, 19, 23, 27, 27};
    Check(!URLPortAllowed(&urlPort87));
    auto urlPort10080Str = FromUTF8Z(U8("http://example.test:10080/path"));
    URL urlPort10080{urlPort10080Str.impl, 1U | 2U | (6U << 3) | (4U << 6), 7, 7, 7, 19, 26, 30, 30};
    Check(!URLPortAllowed(&urlPort10080));
    auto ftpPort20Str = FromUTF8Z(U8("ftp://example.test:20/path"));
    URL ftpPort20{ftpPort20Str.impl, 1U | 0U | (3U << 3) | (3U << 6), 6, 6, 6, 18, 22, 26, 26};
    Check(!URLPortAllowed(&ftpPort20));
    auto ftpPort21Str = FromUTF8Z(U8("ftp://example.test:21/path"));
    URL ftpPort21{ftpPort21Str.impl, 1U | 0U | (3U << 3) | (3U << 6), 6, 6, 6, 18, 22, 26, 26};
    Check(URLPortAllowed(&ftpPort21));
    auto ftpPort22Str = FromUTF8Z(U8("ftp://example.test:22/path"));
    URL ftpPort22{ftpPort22Str.impl, 1U | 0U | (3U << 3) | (3U << 6), 6, 6, 6, 18, 22, 26, 26};
    Check(URLPortAllowed(&ftpPort22));
    auto ftpPort25Str = FromUTF8Z(U8("ftp://example.test:25/path"));
    URL ftpPort25{ftpPort25Str.impl, 1U | 0U | (3U << 3) | (3U << 6), 6, 6, 6, 18, 22, 26, 26};
    Check(!URLPortAllowed(&ftpPort25));
    auto ftpUpper21Str = FromUTF8Z(U8("FTP://example.test:21/path"));
    URL ftpUpper21{ftpUpper21Str.impl, 1U | 0U | (3U << 3) | (3U << 6), 6, 6, 6, 18, 22, 26, 26};
    Check(URLPortAllowed(&ftpUpper21));
    auto ftpUpper22Str = FromUTF8Z(U8("FTP://example.test:22/path"));
    URL ftpUpper22{ftpUpper22Str.impl, 1U | 0U | (3U << 3) | (3U << 6), 6, 6, 6, 18, 22, 26, 26};
    Check(URLPortAllowed(&ftpUpper22));
    auto ftpUpper25Str = FromUTF8Z(U8("FTP://example.test:25/path"));
    URL ftpUpper25{ftpUpper25Str.impl, 1U | 0U | (3U << 3) | (3U << 6), 6, 6, 6, 18, 22, 26, 26};
    Check(!URLPortAllowed(&ftpUpper25));
    auto filePort1Str = FromUTF8Z(U8("file://example.test:1/path"));
    URL filePort1{filePort1Str.impl, 1U | 0U | (2U << 3) | (4U << 6), 7, 7, 7, 19, 22, 26, 26};
    Check(URLPortAllowed(&filePort1));
    auto filePort21Str = FromUTF8Z(U8("file://example.test:21/path"));
    URL filePort21{filePort21Str.impl, 1U | 0U | (3U << 3) | (4U << 6), 7, 7, 7, 19, 23, 27, 27};
    Check(URLPortAllowed(&filePort21));
    auto filePort25Str = FromUTF8Z(U8("file://example.test:25/path"));
    URL filePort25{filePort25Str.impl, 1U | 0U | (3U << 3) | (4U << 6), 7, 7, 7, 19, 23, 27, 27};
    Check(URLPortAllowed(&filePort25));
    auto filePort10080Str = FromUTF8Z(U8("file://example.test:10080/path"));
    URL filePort10080{filePort10080Str.impl, 1U | 0U | (6U << 3) | (4U << 6), 7, 7, 7, 19, 26, 30, 30};
    Check(URLPortAllowed(&filePort10080));
    auto fileUpper25Str = FromUTF8Z(U8("FILE://example.test:25/path"));
    URL fileUpper25{fileUpper25Str.impl, 1U | 0U | (3U << 3) | (4U << 6), 7, 7, 7, 19, 23, 27, 27};
    Check(URLPortAllowed(&fileUpper25));
    auto urlMinusPortStr = FromUTF8Z(U8("http://example.test:-80/path"));
    URL urlMinusPort{urlMinusPortStr.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
    Check(URLPortAllowed(&urlMinusPort));
    auto urlOverflowPortStr = FromUTF8Z(U8("http://example.test:65536/path"));
    URL urlOverflowPort{urlOverflowPortStr.impl, 1U | 2U | (6U << 3) | (4U << 6), 7, 7, 7, 19, 26, 30, 30};
    Check(URLPortAllowed(&urlOverflowPort));
    auto urlBarePlusPortStr = FromUTF8Z(U8("http://example.test:+/path"));
    URL urlBarePlusPort{urlBarePlusPortStr.impl, 1U | 2U | (2U << 3) | (4U << 6), 7, 7, 7, 19, 22, 26, 26};
    Check(URLPortAllowed(&urlBarePlusPort));
    auto urlPlus25PortStr = FromUTF8Z(U8("http://example.test:+25/path"));
    URL urlPlus25Port{urlPlus25PortStr.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
    Check(!URLPortAllowed(&urlPlus25Port));
    auto urlSpace25PortStr = FromUTF8Z(U8("http://example.test: 25 /path"));
    URL urlSpace25Port{urlSpace25PortStr.impl, 1U | 2U | (5U << 3) | (4U << 6), 7, 7, 7, 19, 25, 29, 29};
    Check(!URLPortAllowed(&urlSpace25Port));
    auto urlTab25PortStr = FromUTF8Z(U8("http://example.test:\t25/path"));
    URL urlTab25Port{urlTab25PortStr.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
    Check(!URLPortAllowed(&urlTab25Port));
    auto urlLf25PortStr = FromUTF8Z(U8("http://example.test:\n25/path"));
    URL urlLf25Port{urlLf25PortStr.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
    Check(!URLPortAllowed(&urlLf25Port));
    auto urlCr25PortStr = FromUTF8Z(U8("http://example.test:\r25/path"));
    URL urlCr25Port{urlCr25PortStr.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
    Check(!URLPortAllowed(&urlCr25Port));
    auto urlFf25PortStr = FromUTF8Z(U8("http://example.test:\f25/path"));
    URL urlFf25Port{urlFf25PortStr.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
    Check(!URLPortAllowed(&urlFf25Port));
    auto urlTrailingTab25PortStr = FromUTF8Z(U8("http://example.test:25\t/path"));
    URL urlTrailingTab25Port{urlTrailingTab25PortStr.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
    Check(!URLPortAllowed(&urlTrailingTab25Port));
    auto urlTrailingLf25PortStr = FromUTF8Z(U8("http://example.test:25\n/path"));
    URL urlTrailingLf25Port{urlTrailingLf25PortStr.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
    Check(!URLPortAllowed(&urlTrailingLf25Port));
    auto urlTrailingCr25PortStr = FromUTF8Z(U8("http://example.test:25\r/path"));
    URL urlTrailingCr25Port{urlTrailingCr25PortStr.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
    Check(!URLPortAllowed(&urlTrailingCr25Port));
    auto urlTrailingFf25PortStr = FromUTF8Z(U8("http://example.test:25\f/path"));
    URL urlTrailingFf25Port{urlTrailingFf25PortStr.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
    Check(!URLPortAllowed(&urlTrailingFf25Port));
    auto urlJunk25PortStr = FromUTF8Z(U8("http://example.test:25x/path"));
    URL urlJunk25Port{urlJunk25PortStr.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 19, 24, 28, 28};
    Check(URLPortAllowed(&urlJunk25Port));
    auto urlJunk80PortStr = FromUTF8Z(U8("http://example.test:80junk/path"));
    URL urlJunk80Port{urlJunk80PortStr.impl, 1U | 2U | (7U << 3) | (4U << 6), 7, 7, 7, 19, 27, 31, 31};
    Check(URLPortAllowed(&urlJunk80Port));
    auto widePort8080 = Create16(u"http://wide.test:8080/path", 26);
    URL urlWidePort8080{widePort8080.impl, 1U | 2U | (5U << 3) | (4U << 6), 7, 7, 7, 16, 22, 26, 26};
    Check(URLPortAllowed(&urlWidePort8080));
    auto widePort25 = Create16(u"http://wide.test:25/path", 24);
    URL urlWidePort25{widePort25.impl, 1U | 2U | (3U << 3) | (4U << 6), 7, 7, 7, 16, 20, 24, 24};
    Check(!URLPortAllowed(&urlWidePort25));
    auto wideFtpPort21 = Create16(u"ftp://wide.test:21/path", 23);
    URL urlWideFtpPort21{wideFtpPort21.impl, 1U | 0U | (3U << 3) | (3U << 6), 6, 6, 6, 15, 19, 23, 23};
    Check(URLPortAllowed(&urlWideFtpPort21));
    auto wideFilePort25 = Create16(u"file://wide.test:25/path", 24);
    URL urlWideFilePort25{wideFilePort25.impl, 1U | 0U | (3U << 3) | (4U << 6), 7, 7, 7, 16, 20, 24, 24};
    Check(URLPortAllowed(&urlWideFilePort25));
    auto wideFtpUpper21 = Create16(u"FTP://wide.test:21/path", 23);
    URL urlWideFtpUpper21{wideFtpUpper21.impl, 1U | 0U | (3U << 3) | (3U << 6), 6, 6, 6, 15, 19, 23, 23};
    Check(URLPortAllowed(&urlWideFtpUpper21));
    auto wideFileUpper25 = Create16(u"FILE://wide.test:25/path", 24);
    URL urlWideFileUpper25{wideFileUpper25.impl, 1U | 0U | (3U << 3) | (4U << 6), 7, 7, 7, 16, 20, 24, 24};
    Check(URLPortAllowed(&urlWideFileUpper25));
    auto wideTab25 = Create16(u"http://wide.test:\t25/path", 25);
    URL urlWideTab25{wideTab25.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 16, 21, 25, 25};
    Check(!URLPortAllowed(&urlWideTab25));
    auto wideLf25 = Create16(u"http://wide.test:\n25/path", 25);
    URL urlWideLf25{wideLf25.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 16, 21, 25, 25};
    Check(!URLPortAllowed(&urlWideLf25));
    auto wideCr25 = Create16(u"http://wide.test:\r25/path", 25);
    URL urlWideCr25{wideCr25.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 16, 21, 25, 25};
    Check(!URLPortAllowed(&urlWideCr25));
    auto wideFf25 = Create16(u"http://wide.test:\f25/path", 25);
    URL urlWideFf25{wideFf25.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 16, 21, 25, 25};
    Check(!URLPortAllowed(&urlWideFf25));
    auto wideTrailingTab25 = Create16(u"http://wide.test:25\t/path", 25);
    URL urlWideTrailingTab25{wideTrailingTab25.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 16, 21, 25, 25};
    Check(!URLPortAllowed(&urlWideTrailingTab25));
    auto wideTrailingLf25 = Create16(u"http://wide.test:25\n/path", 25);
    URL urlWideTrailingLf25{wideTrailingLf25.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 16, 21, 25, 25};
    Check(!URLPortAllowed(&urlWideTrailingLf25));
    auto wideTrailingCr25 = Create16(u"http://wide.test:25\r/path", 25);
    URL urlWideTrailingCr25{wideTrailingCr25.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 16, 21, 25, 25};
    Check(!URLPortAllowed(&urlWideTrailingCr25));
    auto wideTrailingFf25 = Create16(u"http://wide.test:25\f/path", 25);
    URL urlWideTrailingFf25{wideTrailingFf25.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 16, 21, 25, 25};
    Check(!URLPortAllowed(&urlWideTrailingFf25));
    auto wideJunk25 = Create16(u"http://wide.test:25x/path", 25);
    URL urlWideJunk25{wideJunk25.impl, 1U | 2U | (4U << 3) | (4U << 6), 7, 7, 7, 16, 21, 25, 25};
    Check(URLPortAllowed(&urlWideJunk25));
    auto urlWraparoundPortStr = FromUTF8Z(U8("25://example.test/path"));
    URL urlWraparoundPort{urlWraparoundPortStr.impl, 1U | 2U | (3U << 3) | (2U << 6), 5, 5, 5, ~0U, 23, 22, 22};
    Check(URLPortAllowed(&urlWraparoundPort));
    Check(URLPortAllowed(&absent));
    Throws([&] { URLPortAllowed(nullptr); });
    URLInvalidate(&url);
    Check(url.impl == string.impl && url.bits == 0 && url.userStart == 0 && url.userEnd == 0);
    Check(url.passwordEnd == 0 && url.hostEnd == 0 && url.pathEnd == 0 && url.pathAfterLastSlash == 0 && url.queryEnd == 0);
    Throws([&] { URLInvalidate(nullptr); });
    Throws([&] { URLProtocol(nullptr); });
    auto* memory = static_cast<unsigned char*>(JSRealloc(nullptr, 5));
    std::memcpy(memory, "hello", 5);
    memory = static_cast<unsigned char*>(JSRealloc(memory, 50));
    Check(!std::memcmp(memory, "hello", 5));
    memory = static_cast<unsigned char*>(JSRealloc(memory, 2));
    Check(memory[0] == 'h' && memory[1] == 'e');
    Check(!JSRealloc(memory, 0));
}

}  // namespace Wtf2Tests
