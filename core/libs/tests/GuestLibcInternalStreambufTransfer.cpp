#include "prx/libSceLibcInternal/include/StreambufTransfer.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal streambuf xsgetn/xsputn: %s\n", message);
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

template<typename TElement>
struct FakeStreambuf {
    GuestLocale::Streambuf buffer{};
    std::vector<TElement> getArea;
    char* getNext = nullptr;
    char* getEnd = nullptr;
    std::vector<TElement> more;
    std::size_t moreIndex = 0;
    int uflows = 0;
    std::vector<TElement> putArea;
    char* putNext = nullptr;
    char* putEnd = nullptr;
    std::vector<int> overflowed;
    std::size_t overflowRoom = 0;
    int overflows = 0;

    FakeStreambuf(std::vector<TElement> get, std::vector<TElement> extra, std::size_t putRoom, std::size_t overflowLimit);
};

template<typename TElement>
int APS5_VABI FakeUflow(GuestLocale::Streambuf* self) {
    auto* stream = reinterpret_cast<FakeStreambuf<TElement>*>(self);
    ++stream->uflows;
    if (stream->moreIndex >= stream->more.size()) return -1;
    return static_cast<int>(static_cast<std::make_unsigned_t<TElement>>(stream->more[stream->moreIndex++]));
}

template<typename TElement>
int APS5_VABI FakeOverflow(GuestLocale::Streambuf* self, int element) {
    auto* stream = reinterpret_cast<FakeStreambuf<TElement>*>(self);
    ++stream->overflows;
    if (stream->overflowed.size() >= stream->overflowRoom) return -1;
    stream->overflowed.push_back(element);
    return element;
}

template<typename TElement>
const std::array<std::uintptr_t, 16>& FakeVtable() {
    static const std::array<std::uintptr_t, 16> slots = [] {
        std::array<std::uintptr_t, 16> table{};
        table[4] = reinterpret_cast<std::uintptr_t>(&FakeOverflow<TElement>);
        table[8] = reinterpret_cast<std::uintptr_t>(&FakeUflow<TElement>);
        return table;
    }();
    return slots;
}

template<typename TElement>
FakeStreambuf<TElement>::FakeStreambuf(std::vector<TElement> get, std::vector<TElement> extra, std::size_t putRoom, std::size_t overflowLimit)
    : getArea(std::move(get)), more(std::move(extra)), putArea(putRoom), overflowRoom(overflowLimit) {
    buffer.vtable = reinterpret_cast<const GuestLocale::StreambufVtable*>(FakeVtable<TElement>().data());
    if (!getArea.empty()) {
        getNext = reinterpret_cast<char*>(getArea.data());
        getEnd = getNext + getArea.size() * sizeof(TElement);
    }
    if (!putArea.empty()) {
        putNext = reinterpret_cast<char*>(putArea.data());
        putEnd = putNext + putArea.size() * sizeof(TElement);
    }
    buffer.reserved[6] = &getNext;
    buffer.reservedCounts[2] = &getEnd;
    buffer.putNext = &putNext;
    buffer.putEnd = &putEnd;
}

template<typename TElement>
using GetFunction = int (APS5_VABI*)(GuestLocale::Streambuf*, TElement*, int);
template<typename TElement>
using PutFunction = int (APS5_VABI*)(GuestLocale::Streambuf*, const TElement*, int);

template<typename TElement>
std::vector<TElement> Units(std::initializer_list<unsigned> values) {
    std::vector<TElement> units;
    for (const auto value : values) units.push_back(static_cast<TElement>(value));
    return units;
}

template<typename TElement>
void CheckGet(GetFunction<TElement> get, unsigned high) {
    {
        FakeStreambuf<TElement> stream(Units<TElement>({'a', 'b', 'c', 'd'}), Units<TElement>({'e', high, 'g'}), 0, 0);
        std::vector<TElement> out(10, static_cast<TElement>('#'));
        Require(get(&stream.buffer, out.data(), 10) == 7, "xsgetn returns the get area and the uflow elements up to end-of-file");
        Require(out == Units<TElement>({'a', 'b', 'c', 'd', 'e', high, 'g', '#', '#', '#'}), "xsgetn stores the elements in order and nothing after them");
        Require(stream.getNext == stream.getEnd && stream.uflows == 4, "xsgetn empties the get area, then calls uflow until it returns end-of-file");
    }
    {
        FakeStreambuf<TElement> stream(Units<TElement>({'a', 'b', 'c', 'd'}), Units<TElement>({'e'}), 0, 0);
        std::vector<TElement> out(4, static_cast<TElement>('#'));
        Require(get(&stream.buffer, out.data(), 2) == 2 && out == Units<TElement>({'a', 'b', '#', '#'}), "xsgetn takes only the count asked for");
        Require(stream.getNext == reinterpret_cast<char*>(stream.getArea.data() + 2) && stream.uflows == 0, "xsgetn advances the get area by the count and leaves uflow alone");
        Require(get(&stream.buffer, out.data(), 0) == 0 && get(&stream.buffer, out.data(), -3) == 0 && get(&stream.buffer, nullptr, 0) == 0, "xsgetn of no elements returns 0");
        Require(out == Units<TElement>({'a', 'b', '#', '#'}) && stream.uflows == 0 && stream.getNext == reinterpret_cast<char*>(stream.getArea.data() + 2), "xsgetn of no elements changes nothing");
    }
    {
        FakeStreambuf<TElement> stream({}, Units<TElement>({'x', 'y'}), 0, 0);
        std::vector<TElement> out(3, static_cast<TElement>('#'));
        Require(get(&stream.buffer, out.data(), 3) == 2 && out == Units<TElement>({'x', 'y', '#'}) && stream.uflows == 3, "without a get area every element comes from uflow");
    }
}

template<typename TElement>
void CheckPut(PutFunction<TElement> put, unsigned high) {
    {
        FakeStreambuf<TElement> stream({}, {}, 4, 2);
        const auto text = Units<TElement>({'a', 'b', 'c', 'd', high, 'f', 'g', 'h'});
        Require(put(&stream.buffer, text.data(), 8) == 6, "xsputn returns the put area and the elements overflow accepted");
        Require(stream.putArea == Units<TElement>({'a', 'b', 'c', 'd'}) && stream.putNext == stream.putEnd, "xsputn fills the put area first");
        Require(stream.overflowed == std::vector<int>{static_cast<int>(high), 'f'} && stream.overflows == 3, "xsputn passes each further element to overflow as a non-negative value and stops at end-of-file");
    }
    {
        FakeStreambuf<TElement> stream({}, {}, 4, 4);
        const auto text = Units<TElement>({'a', 'b', 'c'});
        Require(put(&stream.buffer, text.data(), 2) == 2 && stream.putNext == reinterpret_cast<char*>(stream.putArea.data() + 2) && stream.overflows == 0, "xsputn stores only the count asked for");
        Require(put(&stream.buffer, text.data(), 0) == 0 && put(&stream.buffer, text.data(), -1) == 0 && put(&stream.buffer, nullptr, 0) == 0, "xsputn of no elements returns 0");
        Require(stream.putNext == reinterpret_cast<char*>(stream.putArea.data() + 2) && stream.overflows == 0, "xsputn of no elements changes nothing");
    }
}

template<typename TElement>
void CheckNull(GetFunction<TElement> get, PutFunction<TElement> put) {
    FakeStreambuf<TElement> stream({}, {}, 0, 0);
    TElement element{};
    Require(ThrowsInvalidArgument([&] { get(nullptr, &element, 1); }) && ThrowsInvalidArgument([&] { put(nullptr, &element, 1); }), "a null stream buffer throws std::invalid_argument");
    Require(ThrowsInvalidArgument([&] { get(&stream.buffer, nullptr, 1); }) && ThrowsInvalidArgument([&] { put(&stream.buffer, nullptr, 1); }), "a null element range throws std::invalid_argument");
}

} // namespace

int main() {
    CheckGet<char>(&_ZNSt15basic_streambufIcSt11char_traitsIcEE6xsgetnEPci_nid_postfix, 0xff);
    CheckPut<char>(&_ZNSt15basic_streambufIcSt11char_traitsIcEE6xsputnEPKci_nid_postfix, 0xff);
    CheckNull<char>(&_ZNSt15basic_streambufIcSt11char_traitsIcEE6xsgetnEPci_nid_postfix, &_ZNSt15basic_streambufIcSt11char_traitsIcEE6xsputnEPKci_nid_postfix);
    CheckGet<std::uint16_t>(&_ZNSt15basic_streambufIwSt11char_traitsIwEE6xsgetnEPwi_nid_postfix, 0xffff);
    CheckPut<std::uint16_t>(&_ZNSt15basic_streambufIwSt11char_traitsIwEE6xsputnEPKwi_nid_postfix, 0xffff);
    CheckNull<std::uint16_t>(&_ZNSt15basic_streambufIwSt11char_traitsIwEE6xsgetnEPwi_nid_postfix, &_ZNSt15basic_streambufIwSt11char_traitsIwEE6xsputnEPKwi_nid_postfix);
    return 0;
}
