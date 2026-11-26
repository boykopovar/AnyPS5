#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include "fphdlimp.h"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <initializer_list>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

class Pages {
public:
    Pages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        memory = static_cast<char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        Require(memory != nullptr);
#else
        const auto size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
        auto* mapped = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        memory = static_cast<char*>(mapped);
#endif
        Protect(1, false, false);
    }

    ~Pages() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, pageSize * 2);
#endif
    }

    void Protect(std::size_t page, bool readable, bool writable) {
#ifdef _WIN32
        DWORD previous = 0;
        const DWORD protection = readable ? (writable ? PAGE_READWRITE : PAGE_READONLY) : PAGE_NOACCESS;
        Require(VirtualProtect(memory + page * pageSize, pageSize, protection, &previous) != 0);
#else
        const int protection = readable ? (writable ? PROT_READ | PROT_WRITE : PROT_READ) : PROT_NONE;
        Require(mprotect(memory + page * pageSize, pageSize, protection) == 0);
#endif
    }

    std::int32_t* End(std::ptrdiff_t offset = 0) {
        return reinterpret_cast<std::int32_t*>(memory + pageSize + offset);
    }

private:
    char* memory = nullptr;
    std::size_t pageSize = 0;
};

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

void StatusMemory() {
    Pages pages;
    auto* status = pages.End(-4);
    for (const std::int32_t incoming : {0, -128, -124, 1, 7, 15}) {
        pages.Protect(0, true, true);
        *status = incoming;
        pages.Protect(0, true, false);
        errno = 71;
        auto* iterator = ufieldpositer_open_nid_postfix(status);
        Require(errno == 71 && *status == incoming);
        Require((iterator == nullptr) == (incoming > 0));
        if (iterator) Empty(iterator);
        ufieldpositer_close_nid_postfix(iterator);
    }
    for (auto* unreadable : {pages.End(), pages.End(-2)}) {
        errno = 71;
        RequireThrows([&] { ufieldpositer_open_nid_postfix(unreadable); });
        Require(errno == 71);
    }
}

void OutputMemory() {
    Pages pages;
    std::int32_t status = 0;
    auto* iterator = ufieldpositer_open_nid_postfix(&status);
    Require(iterator && status == 0);
    auto* boundary = pages.End(-4);
    *boundary = 123;
    pages.Protect(0, true, false);
    for (auto* output : {boundary, pages.End(), pages.End(-2)}) {
        Require(ufieldpositer_next_nid_postfix(iterator, output, output) == -1);
        for (bool first : {false, true}) {
            Populate(iterator);
            std::int32_t other = 456;
            errno = 71;
            RequireThrows([&] {
                ufieldpositer_next_nid_postfix(iterator, first ? output : &other, first ? &other : output);
            });
            Require(errno == 71 && other == 456 && *boundary == 123);
        }
        while (ufieldpositer_next_nid_postfix(iterator, nullptr, nullptr) != -1) {}
    }
    pages.Protect(0, true, true);
    Populate(iterator);
    std::int32_t untouched = 456;
    RequireThrows([&] { ufieldpositer_next_nid_postfix(iterator, &untouched, pages.End(-2)); });
    Require(untouched == 456 && *boundary == 123);
    Populate(iterator);
    std::int32_t end = 0;
    errno = 71;
    Require(ufieldpositer_next_nid_postfix(iterator, boundary, &end) == 7);
    Require(errno == 71 && *boundary == 2 && end == 6);
    Require(ufieldpositer_next_nid_postfix(iterator, &end, &end) == 3 && end == 9);
    {
        UErrorCode error = U_ZERO_ERROR;
        icu::FieldPositionIteratorHandler handler(reinterpret_cast<icu::FieldPositionIterator*>(iterator), error);
        handler.addAttribute(-1, 8, 12);
        handler.getError(error);
        Require(U_SUCCESS(error));
    }
    Require(ufieldpositer_next_nid_postfix(iterator, boundary, &end) == -1);
    Require(*boundary == 8 && end == 12);
    ufieldpositer_close_nid_postfix(iterator);
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
        StatusMemory();
        OutputMemory();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
