#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace FreeType;

static bool Equal(const GuestMatrix& a, const GuestMatrix& b) {
    return a.xx == b.xx && a.xy == b.xy && a.yx == b.yx && a.yy == b.yy;
}

template <typename TMatrix>
static void Print(const TMatrix& matrix) {
    std::cout << ' ' << matrix.xx << ' ' << matrix.xy << ' ' << matrix.yx << ' ' << matrix.yy;
}

static int CheckWatched() {
    using namespace GuestArena;
    constexpr std::size_t bytes = 65536;
#ifndef _WIN32
    if (!GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) {
#ifdef __APPLE__
        std::abort();
#else
        std::cout << "Page write watch is unavailable\n";
        return 77;
#endif
    }
#else
    if (!GuestArenaWriteWatched_nid_postfix()) std::abort();
#endif
    auto* memory = static_cast<std::byte*>(GuestArenaAllocate_nid_postfix(bytes, bytes));
    if (!memory) std::abort();
#ifdef _WIN32
    const auto section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, 0, bytes, nullptr);
    if (!section) std::abort();
    GuestArenaMap_nid_postfix(memory, bytes, section, 0, PAGE_READWRITE);
    CloseHandle(section);
#else
    GuestArenaCommit_nid_postfix(memory, bytes, 0x04, bytes);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(memory, bytes);
#endif
    const auto collect = [&] {
#ifdef _WIN32
        std::array<void*, 32> pages{};
        auto count = pages.size();
        if (!GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(memory), bytes, pages.data(), &count, true)) std::abort();
        return count != 0;
#else
        bool written = false;
        if (!GuestWriteWatch::GuestWriteWatchCollect_nid_postfix(reinterpret_cast<std::uintptr_t>(memory), bytes,
            [](void* context, std::uintptr_t, std::uintptr_t) { *static_cast<bool*>(context) = true; }, &written)) std::abort();
        return written;
#endif
    };
    auto* vector = reinterpret_cast<GuestVector*>(memory + 16384 - 8);
    auto* matrix = reinterpret_cast<GuestMatrix*>(memory + 32768 - 16);
    auto* inverse = reinterpret_cast<GuestMatrix*>(memory + 49152 - 16);
    *vector = {1, -2};
    *matrix = {131072, 0, 0, 131072};
    *inverse = {131072, 0, 0, 32768};
    if (!collect() || collect()) std::abort();
#ifdef _WIN32
    for (const auto offset : {16376, 16391, 32752, 32783, 49136, 49167}) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(memory + offset, &info, sizeof(info)) != sizeof(info) || info.Protect != PAGE_READONLY) std::abort();
    }
#endif
    FT_Vector_Transform_nid_postfix(vector, matrix);
    if (vector->x != 2 || vector->y != -4 || !collect() || collect()) std::abort();
    FT_Matrix_Multiply_nid_postfix(matrix, matrix);
    if (!Equal(*matrix, {262144, 0, 0, 262144}) || !collect() || collect()) std::abort();
    if (FT_Matrix_Invert_nid_postfix(inverse) != FT_Err_Ok || !Equal(*inverse, {32768, 0, 0, 131072}) || !collect() || collect()) std::abort();
    FT_Vector_Transform_nid_postfix(vector, nullptr);
    FT_Matrix_Multiply_nid_postfix(nullptr, matrix);
    if (collect()) std::abort();
    *inverse = {1, 0, 0, 1};
    if (!collect() || FT_Matrix_Invert_nid_postfix(inverse) != FT_Err_Invalid_Argument || !Equal(*inverse, {1, 0, 0, 1}) || collect()) std::abort();
#ifndef _WIN32
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(memory, bytes);
#endif
    GuestArenaReset_nid_postfix(memory, bytes);
    GuestArenaRelease_nid_postfix(memory, bytes);
    std::cout << "Geometry CPU stores into watched guest pages passed\n";
    return 0;
}

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--watched") return CheckWatched();
    if (argc != 1) return 1;
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    GuestMatrix unchanged{65536, -32768, 131072, 65536};
    const auto original = unchanged;
    GuestVector untouched{123, -456};
    FT_Vector_Transform_nid_postfix(nullptr, nullptr);
    FT_Vector_Transform_nid_postfix(nullptr, &unchanged);
    FT_Vector_Transform_nid_postfix(&untouched, nullptr);
    FT_Matrix_Multiply_nid_postfix(nullptr, nullptr);
    FT_Matrix_Multiply_nid_postfix(&unchanged, nullptr);
    FT_Matrix_Multiply_nid_postfix(nullptr, &unchanged);
    if (!Equal(unchanged, original) || untouched.x != 123 || untouched.y != -456 ||
        FT_Matrix_Invert_nid_postfix(nullptr) != FT_Err_Invalid_Argument) std::abort();
    std::cout << "FT_GEOMETRY " << sizeof(FT_Long) << '\n';
    GuestVector input{};
    GuestMatrix a{}, b{};
    while (std::cin >> input.x >> input.y >> a.xx >> a.xy >> a.yx >> a.yy >> b.xx >> b.xy >> b.yx >> b.yy) {
        struct {
            std::uint64_t before;
            GuestMatrix value;
            std::uint64_t after;
        } product{0x123456789ABCDEF0, b, 0xFEDCBA9876543210};
        struct {
            std::uint64_t before;
            GuestVector value;
            std::uint64_t after;
        } vector{0x123456789ABCDEF0, input, 0xFEDCBA9876543210};
        const auto left = a;
        auto square = a;
        auto inverse = a;
        FT_Vector_Transform_nid_postfix(&vector.value, &a);
        FT_Matrix_Multiply_nid_postfix(&a, &product.value);
        FT_Matrix_Multiply_nid_postfix(&square, &square);
        const int error = FT_Matrix_Invert_nid_postfix(&inverse);
        if (!Equal(a, left) || product.before != 0x123456789ABCDEF0 || product.after != 0xFEDCBA9876543210 ||
            vector.before != 0x123456789ABCDEF0 || vector.after != 0xFEDCBA9876543210) std::abort();
        std::cout << vector.value.x << ' ' << vector.value.y;
        Print(product.value);
        Print(square);
        std::cout << ' ' << error;
        Print(inverse);
        if constexpr (sizeof(FT_Long) == 8) {
            FT_Vector hostVector{static_cast<FT_Pos>(input.x), static_cast<FT_Pos>(input.y)};
            const FT_Matrix hostA{static_cast<FT_Fixed>(a.xx), static_cast<FT_Fixed>(a.xy), static_cast<FT_Fixed>(a.yx), static_cast<FT_Fixed>(a.yy)};
            FT_Matrix hostB{static_cast<FT_Fixed>(b.xx), static_cast<FT_Fixed>(b.xy), static_cast<FT_Fixed>(b.yx), static_cast<FT_Fixed>(b.yy)};
            auto hostSquare = hostA;
            auto hostInverse = hostA;
            FT_Vector_Transform(&hostVector, &hostA);
            FT_Matrix_Multiply(&hostA, &hostB);
            FT_Matrix_Multiply(&hostSquare, &hostSquare);
            const int hostError = FT_Matrix_Invert(&hostInverse);
            std::cout << ' ' << hostVector.x << ' ' << hostVector.y;
            Print(hostB);
            Print(hostSquare);
            std::cout << ' ' << hostError;
            Print(hostInverse);
        }
        std::cout << '\n';
    }
    if (!std::cin.eof()) return 1;
}
