#include "prx/libSceLibcInternal/include/TimeGetFields.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal istreambuf_iterator equal: %s\n", message);
    std::exit(1);
}

template<typename TCall>
bool ThrowsInvalidArgument(TCall&& call) {
    try {
        call();
        return false;
    } catch (const std::invalid_argument&) {
        return true;
    }
}

template<typename TChar>
struct FakeStreambuf {
    GuestLocale::Streambuf buffer{};
    char* next = nullptr;
    char* end = nullptr;
    std::basic_string<TChar> storage;
    std::size_t index = 0;
    int underflows = 0;
    int uflows = 0;

    void Setup(std::basic_string<TChar> text, bool buffered);
    GuestLocale::InputIterator Begin() {
        return GuestLocale::InputIterator{{reinterpret_cast<std::uint64_t>(&buffer), 0}};
    }
};

template<typename TChar>
int ElementValue(TChar element) {
    if constexpr (sizeof(TChar) == 1) {
        return static_cast<unsigned char>(element);
    } else {
        return static_cast<std::uint16_t>(element);
    }
}

template<typename TChar>
int APS5_VABI FakeUnderflow(GuestLocale::Streambuf* self) {
    auto* stream = reinterpret_cast<FakeStreambuf<TChar>*>(self);
    ++stream->underflows;
    return stream->index < stream->storage.size() ? ElementValue(stream->storage[stream->index]) : -1;
}

template<typename TChar>
int APS5_VABI FakeUflow(GuestLocale::Streambuf* self) {
    auto* stream = reinterpret_cast<FakeStreambuf<TChar>*>(self);
    ++stream->uflows;
    return stream->index < stream->storage.size() ? ElementValue(stream->storage[stream->index++]) : -1;
}

template<typename TChar>
const std::array<std::uintptr_t, 9>& FakeVtable() {
    static const std::array<std::uintptr_t, 9> slots = [] {
        std::array<std::uintptr_t, 9> table{};
        table[7] = reinterpret_cast<std::uintptr_t>(&FakeUnderflow<TChar>);
        table[8] = reinterpret_cast<std::uintptr_t>(&FakeUflow<TChar>);
        return table;
    }();
    return slots;
}

template<typename TChar>
void FakeStreambuf<TChar>::Setup(std::basic_string<TChar> text, bool buffered) {
    storage = std::move(text);
    buffer = {};
    buffer.vtable = reinterpret_cast<const GuestLocale::StreambufVtable*>(FakeVtable<TChar>().data());
    if (buffered && !storage.empty()) {
        next = reinterpret_cast<char*>(storage.data());
        end = next + storage.size() * sizeof(TChar);
        index = storage.size();
        buffer.reserved[6] = &next;
        buffer.reservedCounts[2] = &end;
    }
}

bool Got(const GuestLocale::InputIterator& it) {
    return (it.words[1] & 0xffu) != 0;
}

using EqualFunction = bool (APS5_VABI*)(GuestLocale::InputIterator*, GuestLocale::InputIterator*);

template<typename TChar>
void CheckEqual(EqualFunction equal, const std::basic_string<TChar>& text) {
    {
        GuestLocale::InputIterator endA{};
        GuestLocale::InputIterator endB{};
        Require(equal(&endA, &endB), "two end-of-stream iterators are equal");
    }
    for (const bool buffered : {false, true}) {
        FakeStreambuf<TChar> stream;
        stream.Setup(text, buffered);
        GuestLocale::InputIterator first = stream.Begin();
        GuestLocale::InputIterator last{};
        Require(!equal(&first, &last), "an iterator with input left differs from end-of-stream");
        Require(!equal(&last, &first), "end-of-stream differs from an iterator with input left");
        Require(first.words[0] == reinterpret_cast<std::uint64_t>(&stream.buffer) && Got(first), "the peeked element is kept");
        Require(stream.underflows == (buffered ? 0 : 1) && stream.uflows == 0, "equal peeks without consuming");
        Require(!equal(&first, &last) && stream.underflows == (buffered ? 0 : 1), "a peeked iterator is not read again");
    }
    {
        FakeStreambuf<TChar> stream;
        stream.Setup({}, false);
        GuestLocale::InputIterator first = stream.Begin();
        GuestLocale::InputIterator last{};
        Require(equal(&first, &last), "an iterator whose buffer is at end-of-file equals end-of-stream");
        Require(first.words[0] == 0 && stream.underflows == 1 && stream.uflows == 0, "end-of-file clears the buffer pointer");
    }
    {
        FakeStreambuf<TChar> left;
        FakeStreambuf<TChar> right;
        left.Setup(text, false);
        right.Setup(text, true);
        GuestLocale::InputIterator a = left.Begin();
        GuestLocale::InputIterator b = right.Begin();
        Require(equal(&a, &b), "two iterators with input left are equal whatever their buffers");
    }
    {
        GuestLocale::InputIterator valid{};
        Require(ThrowsInvalidArgument([&] { equal(nullptr, &valid); }) && ThrowsInvalidArgument([&] { equal(&valid, nullptr); }), "a null iterator throws std::invalid_argument");
    }
}

} // namespace

int main() {
    CheckEqual<char>(&_ZNKSt19istreambuf_iteratorIcSt11char_traitsIcEE5equalERKS2__nid_postfix, "x");
    CheckEqual<char16_t>(&_ZNKSt19istreambuf_iteratorIwSt11char_traitsIwEE5equalERKS2__nid_postfix, u"\x263a");
    return 0;
}
