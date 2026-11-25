#include "prx/libSceMetadataReaderWriter/libSceMetadataReaderWriter.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

template <typename TAction>
bool ThrowsMessage(TAction action, const char* expected) {
    try {
        action();
    } catch (const std::invalid_argument& error) {
        return std::string(error.what()).find(expected) != std::string::npos;
    }
    return false;
}

template <typename TCopy>
void CheckParserCopy(TCopy copyFn, const char* name) {
    std::array<unsigned char, 64> src{};
    for (std::size_t i = 0; i < src.size(); ++i) src[i] = static_cast<unsigned char>(i + 1);
    std::array<unsigned char, 64> dst{};
    dst.fill(0xA5);

    errno = 71;
    copyFn(dst.data(), src.data());
    Require(errno == 71);
    for (std::size_t i = 0; i < 56; ++i) Require(dst[i] == src[i]);
    for (std::size_t i = 56; i < dst.size(); ++i) Require(dst[i] == 0xA5);

    std::array<unsigned char, 64> overlap = src;
    copyFn(overlap.data() + 4, overlap.data());
    for (std::size_t i = 0; i < 56; ++i) Require(overlap[i + 4] == src[i]);

    Require(ThrowsMessage([&] { copyFn(nullptr, src.data()); }, (std::string(name) + ": null object").c_str()));
    Require(ThrowsMessage([&] { copyFn(dst.data(), nullptr); }, (std::string(name) + ": null object").c_str()));
    errno = 71;
    Require(ThrowsMessage([&] { copyFn(dst.data(), reinterpret_cast<const void*>(static_cast<std::uintptr_t>(0x10))); }, (std::string(name) + ": unmapped source").c_str()));
    Require(errno == 71);
    Require(ThrowsMessage([&] { copyFn(reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x10)), src.data()); }, (std::string(name) + ": unmapped destination").c_str()));
    Require(errno == 71);
}

struct ValueRecord {
    std::uint32_t kind;
    std::uint32_t pad;
    std::uint64_t size;
    std::uint64_t payload;
};

static_assert(sizeof(ValueRecord) == 24);

struct ThumbnailRecord {
    std::uint64_t size;
    std::uint64_t reserved;
    std::uint64_t address;
};

static_assert(sizeof(ThumbnailRecord) == 24);

} // namespace

int main() {
    CheckParserCopy(_ZN23sceMetadataReaderWriter15ParserInterfaceC1ERKS0_, "_ZN23sceMetadataReaderWriter15ParserInterfaceC1ERKS0_");
    CheckParserCopy(_ZN23sceMetadataReaderWriter15ParserInterfaceC2ERKS0_, "_ZN23sceMetadataReaderWriter15ParserInterfaceC2ERKS0_");

    for (std::uint32_t kind = 0; kind <= 3; ++kind) {
        const ValueRecord src{kind, 0x11111111u, 0x2222333344445555ull, 0x66778899aabbccddull + kind};
        ValueRecord dst{0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCCCCCCCCCull, 0xDDDDDDDDDDDDDDDDull};
        errno = 71;
        Require(_ZN23sceMetadataReaderWriter5ValueaSERS0_(&dst, &src) == &dst);
        Require(errno == 71);
        Require(dst.kind == kind);
        Require(dst.pad == 0xBBBBBBBBu);
        Require(dst.size == src.size);
        Require(dst.payload == src.payload);
    }

    {
        const ValueRecord src{4u, 0u, 0x9988776655443322ull, 0x0102030405060708ull};
        ValueRecord dst{1u, 0u, 0x1234ull, 0x1122334455667788ull};
        Require(_ZN23sceMetadataReaderWriter5ValueaSERS0_(&dst, &src) == &dst);
        Require(dst.kind == 4u);
        Require(dst.size == src.size);
        Require(dst.payload == 0x1122334455667788ull);
    }

    Require(ThrowsMessage([&] { _ZN23sceMetadataReaderWriter5ValueaSERS0_(nullptr, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(0x10))); }, "null object"));
    errno = 71;
    Require(ThrowsMessage([&] {
        ValueRecord dst{};
        _ZN23sceMetadataReaderWriter5ValueaSERS0_(&dst, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(0x10)));
    }, "unmapped source"));
    Require(errno == 71);

    {
        std::array<std::uint64_t, 6> metadata{};
        Require(_ZNK23sceMetadataReaderWriter8Metadata13getFieldCountEv(metadata.data()) == 0);
        std::array<std::int32_t, 4> impl{0, 7, 0, 0};
        metadata[5] = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(impl.data()));
        Require(_ZNK23sceMetadataReaderWriter8Metadata13getFieldCountEv(metadata.data()) == 7);
        impl[1] = 0;
        Require(_ZNK23sceMetadataReaderWriter8Metadata13getFieldCountEv(metadata.data()) == 0);
        Require(ThrowsMessage([&] { _ZNK23sceMetadataReaderWriter8Metadata13getFieldCountEv(nullptr); }, "null object"));
        errno = 71;
        Require(ThrowsMessage([&] { _ZNK23sceMetadataReaderWriter8Metadata13getFieldCountEv(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(0x10))); }, "unmapped object"));
        Require(errno == 71);
    }

    for (auto ctor : {libSceMetadataReaderWriterUnknown_Ly4V1dsyKCA_nid_no_patch, libSceMetadataReaderWriterUnknown_ckVz4iaNj4E_nid_no_patch}) {
        std::array<unsigned char, 32> buf{};
        buf.fill(0xA5);
        errno = 71;
        ctor(buf.data());
        Require(errno == 71);
        for (std::size_t i = 0; i < 24; ++i) Require(buf[i] == 0);
        for (std::size_t i = 24; i < buf.size(); ++i) Require(buf[i] == 0xA5);
        Require(ThrowsMessage([&] { ctor(nullptr); }, "null object"));
    }

    {
        ThumbnailRecord empty{16, 0xA5A5A5A5A5A5A5A5ull, 0};
        Require(libSceMetadataReaderWriterUnknown_GCOzTsybYr0_nid_no_patch(&empty) == 0);
        Require(empty.size == 0 && empty.reserved == 0 && empty.address == 0);
    }

    for (int mode = 0; mode < 3; ++mode) {
        void* mapped = nullptr;
        constexpr std::size_t mapSize = 0x4000;
        Require(sceKernelMmap(nullptr, mapSize, 0x3, 0x1002, -1, 0, &mapped) == 0 && mapped != nullptr);
        ThumbnailRecord thumb{mapSize, 0x1122334455667788ull, static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(mapped))};
        if (mode == 0) {
            Require(libSceMetadataReaderWriterUnknown_GCOzTsybYr0_nid_no_patch(&thumb) == 0);
        } else if (mode == 1) {
            libSceMetadataReaderWriterUnknown_2oFr2hbrkRs_nid_no_patch(&thumb);
        } else {
            libSceMetadataReaderWriterUnknown_YeIbFGmyUVM_nid_no_patch(&thumb);
        }
        Require(thumb.size == 0 && thumb.reserved == 0 && thumb.address == 0);
    }

    Require(ThrowsMessage([&] { libSceMetadataReaderWriterUnknown_GCOzTsybYr0_nid_no_patch(nullptr); }, "null object"));
    return 0;
}
