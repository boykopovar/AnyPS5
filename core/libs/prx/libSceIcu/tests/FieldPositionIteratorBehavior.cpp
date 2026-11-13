#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include "fphdlimp.h"
#include <cstdint>
#include <cstdio>
#include <exception>
#include <initializer_list>

namespace {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

void Empty(UFieldPositionIterator* iterator) {
    std::int32_t begin = 101;
    std::int32_t end = 202;
    Require(ufieldpositer_next_nid_postfix(iterator, &begin, &end) == -1);
    Require(begin == 101 && end == 202);
    Require(ufieldpositer_next_nid_postfix(iterator, nullptr, nullptr) == -1);
}

void Populate(UFieldPositionIterator* iterator) {
    UErrorCode status = U_ZERO_ERROR;
    {
        icu::FieldPositionIteratorHandler handler(reinterpret_cast<icu::FieldPositionIterator*>(iterator), status);
        handler.addAttribute(7, 2, 6);
        handler.addAttribute(3, 1, 9);
        handler.addAttribute(5, 4, 8);
        handler.addAttribute(8, 0, 3);
        handler.getError(status);
    }
    Require(U_SUCCESS(status));
}

void Run() {
    for (std::int32_t incoming : {0, -128, -127}) {
        auto status = incoming;
        auto* iterator = ufieldpositer_open_nid_postfix(&status);
        Require(iterator != nullptr);
        Require(status == incoming);
        Empty(iterator);
        ufieldpositer_close_nid_postfix(iterator);
    }
    for (std::int32_t incoming : {1, 7, 15}) {
        auto status = incoming;
        Require(ufieldpositer_open_nid_postfix(&status) == nullptr);
        Require(status == incoming);
    }
    RequireThrows([] { ufieldpositer_open_nid_postfix(nullptr); });
    RequireThrows([] { ufieldpositer_next_nid_postfix(nullptr, nullptr, nullptr); });
    std::int32_t foreign = 0;
    RequireThrows([&] { ufieldpositer_next_nid_postfix(reinterpret_cast<UFieldPositionIterator*>(&foreign), nullptr, nullptr); });
    Require(foreign == 0);
    std::int32_t status = 0;
    auto* first = ufieldpositer_open_nid_postfix(&status);
    auto* second = ufieldpositer_open_nid_postfix(&status);
    Require(first && second && first != second);
    Populate(first);
    Empty(second);
    Populate(second);
    std::int32_t begin = -10;
    std::int32_t end = -20;
    Require(ufieldpositer_next_nid_postfix(first, &begin, &end) == 7);
    Require(begin == 2 && end == 6);
    Require(ufieldpositer_next_nid_postfix(first, nullptr, &end) == 3);
    Require(end == 9);
    Require(ufieldpositer_next_nid_postfix(first, &begin, nullptr) == 5);
    Require(begin == 4);
    Require(ufieldpositer_next_nid_postfix(first, nullptr, nullptr) == 8);
    Empty(first);
    for (const auto& field : {std::initializer_list<std::int32_t>{7, 2, 6}, {3, 1, 9}, {5, 4, 8}, {8, 0, 3}}) {
        auto value = field.begin();
        Require(ufieldpositer_next_nid_postfix(second, &begin, &end) == *value++);
        Require(begin == *value++);
        Require(end == *value);
    }
    Empty(second);
    ufieldpositer_close_nid_postfix(first);
    ufieldpositer_close_nid_postfix(second);
    ufieldpositer_close_nid_postfix(nullptr);
}

}

int main() {
    try {
        Run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
