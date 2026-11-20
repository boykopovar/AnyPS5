#include "FontFixture.hpp"
#include "Type1Fixture.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_MULTIPLE_MASTERS_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

using namespace FreeType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType variation check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename TCall> static bool ThrowsWith(TCall call, const std::string& prefix) {
    try {
        call();
    } catch (const std::exception& error) {
        return std::string(error.what()).rfind(prefix, 0) == 0;
    }
    return false;
}

struct Libraries {
    GuestLibrary guest{};
    FT_Library native = nullptr;

    Libraries() { Require(FT_Init_FreeType(&guest.host) == 0 && FT_Init_FreeType(&native) == 0); }
    ~Libraries() { Require(FT_Done_FreeType(guest.host) == 0 && FT_Done_FreeType(native) == 0); }
};

static Libraries& Shared() {
    static Libraries libraries;
    return libraries;
}

static std::vector<unsigned char> VariableFont() {
    const std::vector<unsigned char> fvar = FontFixture::Words({1, 0, 16, 2, 1, 20, 1, 8,
        0x7767, 0x6874, 0xff9c, 0, 0x0190, 0, 0x0384, 0, 1, 256, 257, 0, 600, 0});
    const std::vector<unsigned char> gvar = FontFixture::Words({1, 0, 1, 0, 0, 0, 3, 0, 0, 28, 0, 0, 0, 0});
    return FontFixture::BuildFont(3, {{"fvar", fvar}, {"gvar", gvar}});
}

struct FacePair {
    std::vector<unsigned char> bytes;
    GuestFace guest;
    FT_Face oracle = nullptr;

    explicit FacePair(std::vector<unsigned char> font) : bytes(std::move(font)) {
        Libraries& libraries = Shared();
        guest.library = &libraries.guest;
        Require(FT_New_Memory_Face(libraries.guest.host, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &guest.host) == 0);
        Require(FT_New_Memory_Face(libraries.native, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &oracle) == 0);
        SyncFace(guest);
    }

    ~FacePair() {
        Require(FT_Done_Face(guest.host) == 0);
        Require(FT_Done_Face(oracle) == 0);
    }
};

struct GuestVariationAxis {
    const char* name;
    std::int64_t minimum;
    std::int64_t def;
    std::int64_t maximum;
    std::uint64_t tag;
    std::uint32_t strid;
};

struct GuestNamedStyle {
    const std::int64_t* coords;
    std::uint32_t strid;
    std::uint32_t psid;
};

struct GuestVariation {
    std::uint32_t numAxis;
    std::uint32_t numDesigns;
    std::uint32_t numNamedStyles;
    const void* axes;
    const void* namedStyles;
};

static_assert(sizeof(GuestVariation) == 32 && offsetof(GuestVariation, axes) == 16);
static_assert(sizeof(GuestVariationAxis) == 48 && sizeof(GuestNamedStyle) == 16);

template <typename T> static T ReadRecord(const void* data, std::size_t offset) {
    T value;
    std::memcpy(&value, static_cast<const std::byte*>(data) + offset, sizeof(value));
    return value;
}

static void SameVariations(const void* actual, FT_MM_Var* expected) {
    const auto guest = ReadRecord<GuestVariation>(actual, 0);
    Require(guest.numAxis == expected->num_axis && guest.numDesigns == expected->num_designs && guest.numNamedStyles == expected->num_namedstyles);
    for (std::uint32_t index = 0; index < guest.numAxis; ++index) {
        const auto left = ReadRecord<GuestVariationAxis>(guest.axes, index * sizeof(GuestVariationAxis));
        const FT_Var_Axis& right = expected->axis[index];
        Require(left.minimum == right.minimum && left.def == right.def && left.maximum == right.maximum && left.tag == right.tag && left.strid == right.strid);
        Require((left.name == nullptr) == (right.name == nullptr) && (!left.name || std::strcmp(left.name, right.name) == 0));
        FT_UInt nativeFlags = 0;
        Require(FT_Get_Var_Axis_Flags(expected, index, &nativeFlags) == 0);
        Require(ReadRecord<std::uint16_t>(actual, sizeof(GuestVariation) + index * sizeof(std::uint16_t)) == nativeFlags);
    }
    for (std::uint32_t index = 0; index < guest.numNamedStyles; ++index) {
        const auto left = ReadRecord<GuestNamedStyle>(guest.namedStyles, index * sizeof(GuestNamedStyle));
        const FT_Var_Named_Style& right = expected->namedstyle[index];
        Require(left.strid == right.strid && left.psid == right.psid);
        for (std::uint32_t axis = 0; axis < guest.numAxis; ++axis) Require(left.coords[axis] == right.coords[axis]);
    }
}

static void TestGetMMVar() {
    for (const bool type1 : {false, true}) {
        FacePair pair(type1 ? Type1Fixture::MultipleMasterFont() : VariableFont());
        void* actual = nullptr;
        FT_MM_Var* expected = nullptr;
        Require(FT_Get_MM_Var(pair.oracle, &expected) == 0);
        Require(FT_Get_MM_Var_nid_postfix(&pair.guest.rec, &actual) == 0 && actual && expected);
        Require(expected->num_axis == 1 && expected->axis[0].minimum < 0);
        FT_UInt flags = 0;
        Require(FT_Get_Var_Axis_Flags(expected, 0, &flags) == 0);
        Require(type1 ? (expected->num_namedstyles == 0 && flags == 0) :
                        (expected->num_namedstyles != 0 && (flags & FT_VAR_AXIS_FLAG_HIDDEN) != 0));
        SameVariations(actual, expected);
        Require(FT_Done_MM_Var_nid_postfix(&Shared().guest, actual) == 0);
        Require(FT_Done_MM_Var(Shared().native, expected) == 0);
    }
}

static void TestDoneMMVar() {
    FacePair pair(VariableFont());
    for (int round = 0; round < 3; ++round) {
        void* actual = nullptr;
        FT_MM_Var* expected = nullptr;
        Require(FT_Get_MM_Var_nid_postfix(&pair.guest.rec, &actual) == 0 && FT_Get_MM_Var(pair.oracle, &expected) == 0);
        Require(FT_Done_MM_Var_nid_postfix(nullptr, actual) == FT_Err_Invalid_Library_Handle);
        Require(FT_Done_MM_Var_nid_postfix(&Shared().guest, actual) == FT_Done_MM_Var(Shared().native, expected));
    }
    Require(FT_Done_MM_Var_nid_postfix(&Shared().guest, nullptr) == FT_Done_MM_Var(Shared().native, nullptr));
    Require(FT_Done_MM_Var_nid_postfix(nullptr, nullptr) == FT_Done_MM_Var(nullptr, nullptr));
    auto* const unreadableLibrary = reinterpret_cast<GuestLibrary*>(std::uintptr_t{1});
    Require(ThrowsWith([&] { FT_Done_MM_Var_nid_postfix(unreadableLibrary, nullptr); }, "FT_Done_MM_Var_nid_postfix not implemented"));
    alignas(std::max_align_t) std::array<std::byte, 64> misalignedStorage{};
    auto* const misaligned = reinterpret_cast<GuestLibrary*>(misalignedStorage.data() + 1);
    Require(ThrowsWith([&] { FT_Done_MM_Var_nid_postfix(misaligned, nullptr); }, "FT_Done_MM_Var_nid_postfix not implemented"));
}

static void TestSetDesign() {
    for (const std::int64_t value : {0LL, 100LL << 16, 400LL << 16, 500LL << 16, 900LL << 16, 2000LL << 16, -(5LL << 16)}) {
        for (const std::uint32_t count : {0u, 1u, 2u}) {
            FacePair pair(VariableFont());
            const std::int64_t guestCoordinates[2] = {value, value};
            FT_Fixed nativeCoordinates[2] = {static_cast<FT_Fixed>(value), static_cast<FT_Fixed>(value)};
            Require(FT_Set_Var_Design_Coordinates_nid_postfix(&pair.guest.rec, count, guestCoordinates) == FT_Set_Var_Design_Coordinates(pair.oracle, count, nativeCoordinates));
            Require(pair.guest.rec.face_flags == pair.oracle->face_flags);
        }
    }
    FacePair plain(FontFixture::BuildFont(3, {}));
    const std::int64_t one[1] = {0};
    FT_Fixed nativeOne[1] = {0};
    Require(FT_Set_Var_Design_Coordinates_nid_postfix(&plain.guest.rec, 1, one) == FT_Set_Var_Design_Coordinates(plain.oracle, 1, nativeOne));
    FacePair pair(VariableFont());
    Require(FT_Set_Var_Design_Coordinates_nid_postfix(&pair.guest.rec, 1, nullptr) == FT_Set_Var_Design_Coordinates(pair.oracle, 1, nullptr));
    Require(FT_Set_Var_Design_Coordinates_nid_postfix(nullptr, 0, nullptr) == FT_Err_Invalid_Face_Handle);
}

static void TestGetDesign() {
    for (const std::int64_t value : {0LL, 100LL << 16, 500LL << 16, 900LL << 16}) {
        FacePair pair(VariableFont());
        const std::int64_t set[1] = {value};
        FT_Fixed nativeSet[1] = {static_cast<FT_Fixed>(value)};
        Require(FT_Set_Var_Design_Coordinates_nid_postfix(&pair.guest.rec, 1, set) == FT_Set_Var_Design_Coordinates(pair.oracle, 1, nativeSet));
        for (const std::uint32_t count : {0u, 1u, 2u}) {
            std::int64_t actual[2] = {7, 7};
            FT_Fixed expected[2] = {7, 7};
            Require(FT_Get_Var_Design_Coordinates_nid_postfix(&pair.guest.rec, count, actual) == FT_Get_Var_Design_Coordinates(pair.oracle, count, expected));
            Require(actual[0] == expected[0] && actual[1] == expected[1]);
        }
    }
    FacePair pair(VariableFont());
    Require(FT_Get_Var_Design_Coordinates_nid_postfix(&pair.guest.rec, 1, nullptr) == FT_Get_Var_Design_Coordinates(pair.oracle, 1, nullptr));
    Require(FT_Get_Var_Design_Coordinates_nid_postfix(nullptr, 0, nullptr) == FT_Get_Var_Design_Coordinates(nullptr, 0, nullptr));
}

static void TestSetBlend() {
    for (const std::int64_t value : {0LL, 0x4000LL, -0x4000LL, 0x10000LL, -0x10000LL, 0x20000LL}) {
        for (const std::uint32_t count : {0u, 1u, 2u}) {
            FacePair pair(VariableFont());
            const std::int64_t guestCoordinates[2] = {value, value};
            FT_Fixed nativeCoordinates[2] = {static_cast<FT_Fixed>(value), static_cast<FT_Fixed>(value)};
            Require(FT_Set_Var_Blend_Coordinates_nid_postfix(&pair.guest.rec, count, guestCoordinates) == FT_Set_Var_Blend_Coordinates(pair.oracle, count, nativeCoordinates));
            Require(pair.guest.rec.face_flags == pair.oracle->face_flags);
        }
    }
    FacePair plain(FontFixture::BuildFont(3, {}));
    const std::int64_t one[1] = {0};
    FT_Fixed nativeOne[1] = {0};
    Require(FT_Set_Var_Blend_Coordinates_nid_postfix(&plain.guest.rec, 1, one) == FT_Set_Var_Blend_Coordinates(plain.oracle, 1, nativeOne));
    FacePair pair(VariableFont());
    Require(FT_Set_Var_Blend_Coordinates_nid_postfix(&pair.guest.rec, 1, nullptr) == FT_Set_Var_Blend_Coordinates(pair.oracle, 1, nullptr));
    Require(FT_Set_Var_Blend_Coordinates_nid_postfix(nullptr, 0, nullptr) == FT_Set_Var_Blend_Coordinates(nullptr, 0, nullptr));
}

static void TestGetBlend() {
    for (const std::int64_t value : {0LL, 0x4000LL, -0x4000LL, 0x10000LL}) {
        FacePair pair(VariableFont());
        const std::int64_t set[1] = {value};
        FT_Fixed nativeSet[1] = {static_cast<FT_Fixed>(value)};
        Require(FT_Set_Var_Blend_Coordinates_nid_postfix(&pair.guest.rec, 1, set) == FT_Set_Var_Blend_Coordinates(pair.oracle, 1, nativeSet));
        for (const std::uint32_t count : {0u, 1u, 2u}) {
            std::int64_t actual[2] = {7, 7};
            FT_Fixed expected[2] = {7, 7};
            Require(FT_Get_Var_Blend_Coordinates_nid_postfix(&pair.guest.rec, count, actual) == FT_Get_Var_Blend_Coordinates(pair.oracle, count, expected));
            Require(actual[0] == expected[0] && actual[1] == expected[1]);
        }
    }
    FacePair pair(VariableFont());
    Require(FT_Get_Var_Blend_Coordinates_nid_postfix(&pair.guest.rec, 1, nullptr) == FT_Get_Var_Blend_Coordinates(pair.oracle, 1, nullptr));
    Require(FT_Get_Var_Blend_Coordinates_nid_postfix(nullptr, 0, nullptr) == FT_Get_Var_Blend_Coordinates(nullptr, 0, nullptr));
}

static void TestSetMMBlend() {
    for (const std::int64_t value : {0LL, 0x4000LL, -0x4000LL, 0x10000LL, 0x20000LL}) {
        for (const std::uint32_t count : {0u, 1u, 2u}) {
            FacePair pair(VariableFont());
            const std::int64_t guestCoordinates[2] = {value, value};
            FT_Fixed nativeCoordinates[2] = {static_cast<FT_Fixed>(value), static_cast<FT_Fixed>(value)};
            Require(FT_Set_MM_Blend_Coordinates_nid_postfix(&pair.guest.rec, count, guestCoordinates) == FT_Set_MM_Blend_Coordinates(pair.oracle, count, nativeCoordinates));
            Require(pair.guest.rec.face_flags == pair.oracle->face_flags);
        }
    }
    FacePair plain(FontFixture::BuildFont(3, {}));
    const std::int64_t one[1] = {0};
    FT_Fixed nativeOne[1] = {0};
    Require(FT_Set_MM_Blend_Coordinates_nid_postfix(&plain.guest.rec, 1, one) == FT_Set_MM_Blend_Coordinates(plain.oracle, 1, nativeOne));
    FacePair pair(VariableFont());
    Require(FT_Set_MM_Blend_Coordinates_nid_postfix(&pair.guest.rec, 1, nullptr) == FT_Set_MM_Blend_Coordinates(pair.oracle, 1, nullptr));
    Require(FT_Set_MM_Blend_Coordinates_nid_postfix(nullptr, 0, nullptr) == FT_Set_MM_Blend_Coordinates(nullptr, 0, nullptr));
}

static void TestSetMMDesign() {
    for (const std::int64_t value : {0LL, 100LL, 400LL, 900LL, 100LL << 16, -5LL}) {
        for (const std::uint32_t count : {0u, 1u, 2u}) {
            FacePair pair(VariableFont());
            const std::int64_t guestCoordinates[2] = {value, value};
            FT_Long nativeCoordinates[2] = {static_cast<FT_Long>(value), static_cast<FT_Long>(value)};
            Require(FT_Set_MM_Design_Coordinates_nid_postfix(&pair.guest.rec, count, guestCoordinates) == FT_Set_MM_Design_Coordinates(pair.oracle, count, nativeCoordinates));
            Require(pair.guest.rec.face_flags == pair.oracle->face_flags);
        }
    }
    FacePair plain(FontFixture::BuildFont(3, {}));
    const std::int64_t one[1] = {0};
    FT_Long nativeOne[1] = {0};
    Require(FT_Set_MM_Design_Coordinates_nid_postfix(&plain.guest.rec, 1, one) == FT_Set_MM_Design_Coordinates(plain.oracle, 1, nativeOne));
    FacePair pair(VariableFont());
    Require(FT_Set_MM_Design_Coordinates_nid_postfix(&pair.guest.rec, 1, nullptr) == FT_Set_MM_Design_Coordinates(pair.oracle, 1, nullptr));
    Require(FT_Set_MM_Design_Coordinates_nid_postfix(nullptr, 0, nullptr) == FT_Set_MM_Design_Coordinates(nullptr, 0, nullptr));
}

static void TestGetMultiMaster() {
    struct GuestAxis {
        const char* name;
        std::int64_t minimum;
        std::int64_t maximum;
    };
    static_assert(sizeof(GuestAxis) == 24);
    constexpr std::size_t headerSize = 2 * sizeof(std::uint32_t);
    std::array<std::byte, headerSize + 4 * sizeof(GuestAxis)> output;
    output.fill(std::byte{0xab});
    FacePair type1(Type1Fixture::MultipleMasterFont());
    FT_Multi_Master expected{};
    Require(FT_Get_Multi_Master(type1.oracle, &expected) == 0);
    Require(expected.num_axis == 1 && expected.num_designs == 2);
    Require(FT_Get_Multi_Master_nid_postfix(&type1.guest.rec, output.data()) == 0);
    Require(ReadRecord<std::uint32_t>(output.data(), 0) == expected.num_axis);
    Require(ReadRecord<std::uint32_t>(output.data(), sizeof(std::uint32_t)) == expected.num_designs);
    const auto axis = ReadRecord<GuestAxis>(output.data(), headerSize);
    Require(axis.name && expected.axis[0].name && std::strcmp(axis.name, expected.axis[0].name) == 0);
    Require(axis.minimum == expected.axis[0].minimum && axis.maximum == expected.axis[0].maximum);
    Require(axis.minimum == -20 && axis.maximum == 20);
    for (std::size_t offset = headerSize + sizeof(GuestAxis); offset < output.size(); ++offset) Require(output[offset] == std::byte{0xab});

    FacePair openType(VariableFont());
    const auto before = output;
    const auto error = FT_Get_Multi_Master(openType.oracle, &expected);
    Require(error == FT_Err_Invalid_Argument);
    Require(FT_Get_Multi_Master_nid_postfix(&openType.guest.rec, output.data()) == error && output == before);
}

static void TestProbeErrors() {
    for (auto font : {FontFixture::BuildFont(3, {}), FontFixture::BitmapFont()}) {
        FacePair pair(std::move(font));
        Require(!FT_HAS_MULTIPLE_MASTERS(pair.oracle));
        void* actual = reinterpret_cast<void*>(std::uintptr_t{1});
        FT_MM_Var* expected = reinterpret_cast<FT_MM_Var*>(std::uintptr_t{1});
        const int error = FT_Get_MM_Var(pair.oracle, &expected);
        Require(error == FT_Err_Invalid_Argument);
        Require(FT_Get_MM_Var_nid_postfix(&pair.guest.rec, &actual) == error);
        Require(actual == reinterpret_cast<void*>(std::uintptr_t{1}) && expected == reinterpret_cast<FT_MM_Var*>(std::uintptr_t{1}));
        std::array<std::uint64_t, 16> master;
        master.fill(0x1122334455667788);
        FT_Multi_Master native{};
        Require(FT_Get_Multi_Master(pair.oracle, &native) == FT_Err_Invalid_Argument);
        Require(FT_Get_Multi_Master_nid_postfix(&pair.guest.rec, master.data()) == FT_Err_Invalid_Argument);
        for (const auto value : master) Require(value == 0x1122334455667788);
        Require(FT_Get_MM_Var_nid_postfix(&pair.guest.rec, nullptr) == FT_Get_MM_Var(pair.oracle, nullptr));
        Require(FT_Get_Multi_Master_nid_postfix(&pair.guest.rec, nullptr) == FT_Get_Multi_Master(pair.oracle, nullptr));
    }
    FacePair variable(VariableFont());
    Require(FT_Get_MM_Var_nid_postfix(&variable.guest.rec, nullptr) == FT_Get_MM_Var(variable.oracle, nullptr));
    Require(FT_Get_Multi_Master_nid_postfix(&variable.guest.rec, nullptr) == FT_Get_Multi_Master(variable.oracle, nullptr));
    Require(FT_Get_MM_Var_nid_postfix(nullptr, nullptr) == FT_Get_MM_Var(nullptr, nullptr));
    Require(FT_Get_Multi_Master_nid_postfix(nullptr, nullptr) == FT_Get_Multi_Master(nullptr, nullptr));
}

int main() {
    TestGetMMVar();
    TestDoneMMVar();
    TestSetDesign();
    TestGetDesign();
    TestSetBlend();
    TestGetBlend();
    TestSetMMBlend();
    TestSetMMDesign();
    TestGetMultiMaster();
    TestProbeErrors();
}
