#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

constexpr std::int32_t Success = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t InvalidFormat = 3;
constexpr std::int32_t BufferOverflow = 15;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU constrained field position: %s\n", message);
    std::abort();
}

template<class F>
bool Rejects(F call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void Defaults() {
    std::int32_t error = Success;
    auto* position = ucfpos_open_nid_postfix(&error);
    Require(position && error == Success, "open");
    Require(ucfpos_getCategory_nid_postfix(position, &error) == 0 && ucfpos_getField_nid_postfix(position, &error) == 0, "undefined category and field");
    std::int32_t start = -1, limit = -1;
    ucfpos_getIndexes_nid_postfix(position, &start, &limit, &error);
    Require(start == 0 && limit == 0 && error == Success, "empty indexes");
    ucfpos_close_nid_postfix(position);
    error = BufferOverflow;
    position = ucfpos_open_nid_postfix(&error);
    Require(position && error == BufferOverflow, "open ignores an incoming error");
    ucfpos_close_nid_postfix(position);
    position = ucfpos_open_nid_postfix(nullptr);
    Require(position != nullptr, "open accepts a null error pointer");
    ucfpos_close_nid_postfix(position);
}

void Constraints() {
    std::int32_t error = Success;
    auto* position = ucfpos_open_nid_postfix(&error);
    ucfpos_constrainCategory_nid_postfix(position, 5, &error);
    Require(ucfpos_getCategory_nid_postfix(position, &error) == 5 && ucfpos_getField_nid_postfix(position, &error) == 0, "category constraint");
    ucfpos_constrainField_nid_postfix(position, 7, 3, &error);
    Require(ucfpos_getCategory_nid_postfix(position, &error) == 7 && ucfpos_getField_nid_postfix(position, &error) == 3, "field constraint");
    ucfpos_constrainCategory_nid_postfix(position, -9, &error);
    Require(ucfpos_getCategory_nid_postfix(position, &error) == -9 && ucfpos_getField_nid_postfix(position, &error) == 3, "a category constraint keeps the field");
    Require(error == Success, "constraints succeed");
    error = IllegalArgument;
    ucfpos_constrainField_nid_postfix(position, 1, 1, &error);
    ucfpos_constrainCategory_nid_postfix(position, 1, &error);
    Require(ucfpos_getCategory_nid_postfix(position, &error) == 0 && ucfpos_getField_nid_postfix(position, &error) == 0, "a set error returns defaults");
    std::int32_t start = 11, limit = 12;
    ucfpos_getIndexes_nid_postfix(position, &start, &limit, &error);
    Require(start == 11 && limit == 12 && error == IllegalArgument, "a set error leaves indexes untouched");
    error = Success;
    Require(ucfpos_getCategory_nid_postfix(position, &error) == -9 && ucfpos_getField_nid_postfix(position, &error) == 3, "a set error changes nothing");
    ucfpos_close_nid_postfix(position);
}

void Handles() {
    std::int32_t error = Success;
    Require(ucfpos_getCategory_nid_postfix(nullptr, &error) == 0 && error == IllegalArgument, "null handle");
    error = Success;
    ucfpos_constrainCategory_nid_postfix(nullptr, 4, &error);
    Require(error == IllegalArgument, "null handle constraint");
    alignas(8) std::array<std::uint8_t, 64> foreign{};
    auto* forged = reinterpret_cast<UConstrainedFieldPosition*>(foreign.data());
    error = Success;
    Require(ucfpos_getField_nid_postfix(forged, &error) == 0 && error == InvalidFormat, "foreign memory is rejected");
    error = Success;
    std::int32_t start = 21, limit = 22;
    ucfpos_getIndexes_nid_postfix(forged, &start, &limit, &error);
    Require(start == 21 && limit == 22 && error == InvalidFormat, "foreign memory indexes");
    ucfpos_close_nid_postfix(nullptr);
    ucfpos_close_nid_postfix(forged);
    auto* position = ucfpos_open_nid_postfix(&error);
    Require(Rejects([&] { ucfpos_getCategory_nid_postfix(position, nullptr); }), "null error pointer");
    Require(Rejects([&] { ucfpos_constrainField_nid_postfix(position, 1, 2, nullptr); }), "null error pointer in a constraint");
    error = Success;
    Require(Rejects([&] { ucfpos_getIndexes_nid_postfix(position, nullptr, &start, &error); }), "null start output");
    Require(Rejects([&] { ucfpos_getIndexes_nid_postfix(position, &start, nullptr, &error); }), "null limit output");
    ucfpos_close_nid_postfix(position);
}

}

int main() {
    Defaults();
    Constraints();
    Handles();
    return 0;
}
