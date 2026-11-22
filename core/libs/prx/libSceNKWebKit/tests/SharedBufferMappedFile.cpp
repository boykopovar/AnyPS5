#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceNKWebKit/include/SharedBufferExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>

using NKWebKit::WebCoreAbi::DataSegment;
using NKWebKit::WebCoreAbi::SharedBuffer;
using NKWebKit::WtfAbi::String;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "SharedBufferMappedFile check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static std::map<void*, std::size_t> guestAllocations;

static void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* pointer = std::malloc(size);
    if (pointer) guestAllocations[pointer] = size;
    return pointer;
}

static void APS5_VABI Free_nid_no_patch(void* pointer) {
    Require(guestAllocations.erase(pointer) == 1);
    std::free(pointer);
}

static void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) {
    void* pointer = std::calloc(count, size);
    if (pointer) guestAllocations[pointer] = count * size;
    return pointer;
}

static void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    void* resized = std::realloc(pointer, size);
    if (!resized) return nullptr;
    guestAllocations.erase(pointer);
    guestAllocations[resized] = size;
    return resized;
}

static void* APS5_VABI Align(std::size_t alignment, std::size_t size) {
    void* pointer = nullptr;
    if (posix_memalign(&pointer, alignment, size)) return nullptr;
    guestAllocations[pointer] = size;
    return pointer;
}

static void* APS5_VABI Realign_nid_no_patch(void* pointer, std::size_t size, std::size_t alignment) {
    void* resized = Align(alignment, size);
    if (!resized) return nullptr;
    std::memcpy(resized, pointer, std::min(size, guestAllocations[pointer]));
    Free_nid_no_patch(pointer);
    return resized;
}

static int APS5_VABI PosixAlign_nid_no_patch(void** pointer, std::size_t alignment, std::size_t size) {
    *pointer = Align(alignment, size);
    return *pointer ? 0 : 12;
}

static SharedBuffer* Allocate() {
    return static_cast<SharedBuffer*>(_ZN3WTF10fastMallocEm(sizeof(SharedBuffer)));
}

static std::size_t Size(const SharedBuffer* buffer) {
    return _ZNK7WebCore12SharedBuffer4sizeEv(buffer);
}

static const char* Bytes(const SharedBuffer* buffer) {
    return _ZNK7WebCore12SharedBuffer4dataEv(buffer);
}

static String GuestPath(const char* path) {
    return _ZN3WTF6String8fromUTF8EPKh(reinterpret_cast<const unsigned char*>(path));
}

static WTF::MappedFileData MapFile(const char* guestPath) {
    WTF::MappedFileData data{};
    bool success = false;
    _ZN3WTF14FileSystemImpl14MappedFileDataC1ERKNS_6StringENS0_14MappedFileModeERb(&data, GuestPath(guestPath), 0, success);
    Require(success == (data.data != nullptr));
    return data;
}

static void WriteFile(const std::filesystem::path& path, const char* contents, std::size_t length) {
    std::ofstream file(path, std::ios::binary);
    file.write(contents, static_cast<std::streamsize>(length));
    file.close();
    Require(!file.bad());
    Require(std::filesystem::file_size(path) == length);
}

static void TestMapsAFileIntoTheBuffer(const std::filesystem::path& root) {
    constexpr char Contents[] = "shared buffer mapped from a file";
    const std::size_t length = sizeof(Contents) - 1;
    WriteFile(root / "mapped.bin", Contents, length);

    WTF::MappedFileData data = MapFile("/nkwebkit-sharedbuffer/mapped.bin");
    Require(data.size == length);
    void* mapping = data.data;
    VirtualQueryInfo mapped{};
    Require(sceKernelVirtualQuery(mapping, 0, &mapped, sizeof(mapped)) == 0);

    SharedBuffer* buffer = Allocate();
    _ZN7WebCore12SharedBufferC1EON3WTF14FileSystemImpl14MappedFileDataE(buffer, &data);
    Require(!data.data && !data.size);
    Require(Size(buffer) == length && !_ZNK7WebCore12SharedBuffer7isEmptyEv(buffer));
    Require(buffer->refCount == 1 && buffer->segments.size == 1);
    Require(buffer->segments.buffer[0].beginPosition == 0);
    Require(buffer->segments.buffer[0].segment->index == 1);
    Require(buffer->segments.buffer[0].segment->refCount == 1);
    Require(buffer->segments.buffer == buffer->segments.inlineBuffer && buffer->segments.capacity == 1);
    Require(Bytes(buffer) == static_cast<const char*>(mapping));
    Require(std::memcmp(Bytes(buffer), Contents, length) == 0);
    Require(_ZNK7WebCore12SharedBuffer11DataSegment4sizeEv(buffer->segments.buffer[0].segment) == length);
    Require(std::memcmp(_ZNK7WebCore12SharedBuffer11DataSegment4dataEv(buffer->segments.buffer[0].segment), Contents, length) == 0);

    _ZN7WebCore12SharedBufferD1Ev(buffer);
    _ZN3WTF8fastFreeEPv(buffer);
    Require(sceKernelVirtualQuery(mapping, 0, &mapped, sizeof(mapped)) != 0);
    Require(guestAllocations.empty());
}

static void TestBaseConstructor(const std::filesystem::path& root) {
    constexpr char Contents[] = "abcdef";
    WriteFile(root / "base.bin", Contents, sizeof(Contents) - 1);

    WTF::MappedFileData data = MapFile("/nkwebkit-sharedbuffer/base.bin");
    SharedBuffer* buffer = Allocate();
    _ZN7WebCore12SharedBufferC2EON3WTF14FileSystemImpl14MappedFileDataE(buffer, &data);
    Require(!data.data && !data.size);
    Require(Size(buffer) == sizeof(Contents) - 1 && std::memcmp(Bytes(buffer), Contents, sizeof(Contents) - 1) == 0);

    _ZN7WebCore12SharedBufferD2Ev(buffer);
    _ZN3WTF8fastFreeEPv(buffer);
    Require(guestAllocations.empty());
}

static void TestEmptyMapping() {
    WTF::MappedFileData empty{nullptr, 0};
    SharedBuffer* buffer = Allocate();
    _ZN7WebCore12SharedBufferC1EON3WTF14FileSystemImpl14MappedFileDataE(buffer, &empty);
    Require(Size(buffer) == 0 && _ZNK7WebCore12SharedBuffer7isEmptyEv(buffer));
    Require(buffer->segments.size == 1 && buffer->segments.buffer[0].segment->index == 1);
    Require(!Bytes(buffer) && !_ZNK7WebCore12SharedBuffer14dataAsUInt8PtrEv(buffer));

    _ZN7WebCore12SharedBufferD1Ev(buffer);
    _ZN3WTF8fastFreeEPv(buffer);
    Require(guestAllocations.empty());
}

static void TestMissingFileMapsNothing() {
    WTF::MappedFileData data = MapFile("/nkwebkit-sharedbuffer/missing.bin");
    Require(!data.data && !data.size);

    SharedBuffer* buffer = Allocate();
    _ZN7WebCore12SharedBufferC2EON3WTF14FileSystemImpl14MappedFileDataE(buffer, &data);
    Require(Size(buffer) == 0 && _ZNK7WebCore12SharedBuffer7isEmptyEv(buffer));
    _ZN7WebCore12SharedBufferD2Ev(buffer);
    _ZN3WTF8fastFreeEPv(buffer);
    Require(guestAllocations.empty());
}

static void TestSharedSegmentAndClear(const std::filesystem::path& root) {
    constexpr char Contents[] = "mapped contents";
    const std::size_t length = sizeof(Contents) - 1;
    WriteFile(root / "shared.bin", Contents, length);

    WTF::MappedFileData data = MapFile("/nkwebkit-sharedbuffer/shared.bin");
    SharedBuffer* mapped = Allocate();
    _ZN7WebCore12SharedBufferC1EON3WTF14FileSystemImpl14MappedFileDataE(mapped, &data);

    {
        auto created = _ZN7WebCore12SharedBuffer6createEPKcm("head:", 5);
        SharedBuffer* appended = created.ptr;
        _ZN7WebCore12SharedBuffer6appendERKS0_(appended, mapped);
        Require(Size(appended) == length + 5 && appended->segments.size == 2);
        Require(appended->segments.buffer[1].beginPosition == 5);
        Require(appended->segments.buffer[1].segment == mapped->segments.buffer[0].segment);
        Require(mapped->segments.buffer[0].segment->refCount == 2);
        Require(Bytes(appended)[0] == 'h' && std::memcmp(Bytes(appended) + 5, Contents, length) == 0);

        _ZN7WebCore12SharedBuffer5clearEv(appended);
        Require(Size(appended) == 0 && !Bytes(appended));
        Require(mapped->segments.buffer[0].segment->refCount == 1);
        Require(Size(mapped) == length && std::memcmp(Bytes(mapped), Contents, length) == 0);

        _ZN7WebCore12SharedBuffer5clearEv(appended);
        Require(Size(appended) == 0);
    }

    _ZN7WebCore12SharedBufferD1Ev(mapped);
    _ZN3WTF8fastFreeEPv(mapped);
    Require(guestAllocations.empty());
}

static void TestCopySharesTheMapping(const std::filesystem::path& root) {
    constexpr char Contents[] = "copied mapping";
    const std::size_t length = sizeof(Contents) - 1;
    WriteFile(root / "copied.bin", Contents, length);

    WTF::MappedFileData data = MapFile("/nkwebkit-sharedbuffer/copied.bin");
    SharedBuffer* mapped = Allocate();
    _ZN7WebCore12SharedBufferC2EON3WTF14FileSystemImpl14MappedFileDataE(mapped, &data);

    {
        auto clone = _ZNK7WebCore12SharedBuffer4copyEv(mapped);
        Require(clone.ptr != nullptr && clone.ptr->segments.buffer[0].segment == mapped->segments.buffer[0].segment);
        Require(mapped->segments.buffer[0].segment->refCount == 2);
        Require(Size(clone.ptr) == length && std::memcmp(Bytes(clone.ptr), Contents, length) == 0);
        _ZN7WebCore12SharedBuffer5clearEv(clone.ptr);
        Require(Size(mapped) == length && std::memcmp(Bytes(mapped), Contents, length) == 0);
    }
    Require(mapped->segments.buffer[0].segment->refCount == 1);

    _ZN7WebCore12SharedBufferD2Ev(mapped);
    _ZN3WTF8fastFreeEPv(mapped);
    Require(guestAllocations.empty());
}

static void TestNullArguments() {
    WTF::MappedFileData data{nullptr, 0};
    SharedBuffer* buffer = Allocate();
    bool threw = false;
    try {
        _ZN7WebCore12SharedBufferC1EON3WTF14FileSystemImpl14MappedFileDataE(nullptr, &data);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    threw = false;
    try {
        _ZN7WebCore12SharedBufferC2EON3WTF14FileSystemImpl14MappedFileDataE(buffer, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    Require(guestAllocations.size() == 1);
    _ZN3WTF8fastFreeEPv(buffer);
    Require(guestAllocations.empty());
}

int main() {
    std::array<void*, 10> heapApi{
        reinterpret_cast<void*>(&Allocate_nid_no_patch),
        reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch),
        reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align),
        reinterpret_cast<void*>(&Realign_nid_no_patch),
        reinterpret_cast<void*>(&PosixAlign_nid_no_patch),
    };
    ApplicationHeapRegister_nid_no_patch(heapApi.data());

    const auto root = std::filesystem::current_path() / ("nkwebkit-sharedbuffer-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto hostPath = root.u8string();
    AddPathAlias_nid_no_patch("/nkwebkit-sharedbuffer", reinterpret_cast<const char*>(hostPath.c_str()));

    TestMapsAFileIntoTheBuffer(root);
    TestBaseConstructor(root);
    TestEmptyMapping();
    TestMissingFileMapsNothing();
    TestSharedSegmentAndClear(root);
    TestCopySharesTheMapping(root);
    TestNullArguments();

    RemovePathAlias_nid_no_patch("/nkwebkit-sharedbuffer");
    std::filesystem::remove_all(root);
    Require(guestAllocations.empty());
    return 0;
}
