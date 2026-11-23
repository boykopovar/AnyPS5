#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

using NKWebKit::WebCoreAbi::BlobDataFileReference;
using NKWebKit::WebCoreAbi::BlobDataItem;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "BlobDataItem check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

constexpr const char* kLength = "_ZNK7WebCore12BlobDataItem6lengthEv";

template <typename Function>
bool FaultsAsNotImplemented(Function function) {
    try {
        static_cast<void>(function());
    } catch (const std::invalid_argument&) {
        return false;
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == std::string(kLength) + " not implemented";
    }
    return false;
}

template <typename Function>
bool RejectsInput(Function function) {
    try {
        static_cast<void>(function());
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

BlobDataFileReference FileOfSize(std::uint64_t size) {
    BlobDataFileReference file{};
    file.refCount = 1;
    file.size = size;
    file.expectedModificationTime = 1234.5;
    return file;
}

BlobDataItem Item(std::int32_t type, BlobDataFileReference* file, std::int64_t offset, std::int64_t length) {
    BlobDataItem item{};
    item.type = type;
    item.file = file;
    item.offset = offset;
    item.length = length;
    return item;
}

long long Length(const BlobDataItem& item) {
    return _ZNK7WebCore12BlobDataItem6lengthEv(&item);
}

}

int main() {
    auto* unreadableFile = reinterpret_cast<BlobDataFileReference*>(0x10);
    BlobDataFileReference file = FileOfSize(4096);
    BlobDataFileReference empty = FileOfSize(0);
    BlobDataFileReference huge = FileOfSize(1ULL << 40);

    Require(Length(Item(0, nullptr, 7, 123)) == 123);
    Require(Length(Item(0, nullptr, 0, 0)) == 0);
    Require(Length(Item(0, nullptr, 0, -1)) == -1);
    Require(Length(Item(0, nullptr, 0, -2)) == -2);

    Require(Length(Item(1, &file, 0, -1)) == 4096);
    Require(Length(Item(1, &empty, 0, -1)) == 0);
    Require(Length(Item(1, &huge, 0, -1)) == 1099511627776LL);
    Require(Length(Item(1, &file, 10, 50)) == 50);
    Require(Length(Item(1, &file, 10, 0)) == 0);
    Require(Length(Item(1, unreadableFile, 10, 50)) == 50);
    Require(Length(Item(7, &file, 0, -1)) == -1);
    Require(file.size == 4096 && file.refCount == 1);

    Require(RejectsInput([] { return _ZNK7WebCore12BlobDataItem6lengthEv(nullptr); }));
    const BlobDataItem withoutFile = Item(1, nullptr, 0, -1);
    Require(RejectsInput([&] { return Length(withoutFile); }));

    const BlobDataItem withUnreadableFile = Item(1, unreadableFile, 0, -1);
    Require(FaultsAsNotImplemented([&] { return Length(withUnreadableFile); }));
    auto* unreadableItem = reinterpret_cast<const BlobDataItem*>(0x10);
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore12BlobDataItem6lengthEv(unreadableItem); }));
    return 0;
}
