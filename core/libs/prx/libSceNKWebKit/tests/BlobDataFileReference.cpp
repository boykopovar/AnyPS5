#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_set>

using NKWebKit::WebCoreAbi::BlobDataFileReference;
using NKWebKit::WebCoreAbi::OptionalWallTime;
using NKWebKit::WtfAbi::String;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "BlobDataFileReference check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

using Method = void(APS5_VABI*)(BlobDataFileReference*);

static std::unordered_set<void*> guestAllocations;

static void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* pointer = std::malloc(size);
    if (pointer) guestAllocations.insert(pointer);
    return pointer;
}

static void APS5_VABI Free_nid_no_patch(void* pointer) {
    Require(guestAllocations.erase(pointer) == 1);
    std::free(pointer);
}

static void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
static void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
static void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t size) { return Allocate_nid_no_patch(size); }
static void* APS5_VABI Realign_nid_no_patch(void* pointer, std::size_t size, std::size_t) { return Reallocate_nid_no_patch(pointer, size); }
static int APS5_VABI PosixAlign_nid_no_patch(void** pointer, std::size_t alignment, std::size_t size) {
    *pointer = Align_nid_no_patch(alignment, size);
    return *pointer ? 0 : 12;
}

template <typename Action>
static bool ThrowsInvalidArgument(Action action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static String Guest(const char* path) {
    return _ZN3WTF6String8fromUTF8EPKh(reinterpret_cast<const unsigned char*>(path));
}

static void WriteFile(const std::filesystem::path& path, std::size_t length) {
    std::ofstream file(path, std::ios::binary);
    file << std::string(length, 'b');
}

static double GuestModificationSeconds(const char* path) {
    FileStat status{};
    Require(stat_nid_postfix(path, &status) == 0);
    return static_cast<double>(status.st_mtim.tv_sec);
}

static Method Slot(const BlobDataFileReference& object, int index) {
    return reinterpret_cast<Method>(const_cast<void*>(object.vtable[index]));
}

struct Storage {
    alignas(BlobDataFileReference) unsigned char bytes[sizeof(BlobDataFileReference)];
    BlobDataFileReference* get() { return reinterpret_cast<BlobDataFileReference*>(bytes); }
};

static void ConstructsUntracked() {
    auto path = Guest("/nkwebkit-blob/untracked.bin");
    String replacement;
    Storage storage;
    std::memset(storage.bytes, 0xAB, sizeof(storage.bytes));
    auto* object = storage.get();
    _ZN7WebCore21BlobDataFileReferenceC1ERKN3WTF6StringES4_(object, &path, &replacement);
    Require(object->vtable && object->refCount == 1);
    Require(object->path.impl == path.impl && !object->replacementPath.impl);
    Require(object->size == 0 && std::isnan(object->expectedModificationTime));
    Require(_ZN7WebCore21BlobDataFileReference4pathEv(object) == &object->path);
    const OptionalWallTime time = _ZN7WebCore21BlobDataFileReference24expectedModificationTimeEv(object);
    Require(!time.hasValue);
    Require(_ZN7WebCore21BlobDataFileReference4sizeEv(object) == 0);
    _ZN7WebCore21BlobDataFileReferenceD2Ev(object);
}

static void PathPrefersTheReplacement() {
    auto path = Guest("/nkwebkit-blob/original.bin");
    auto replacement = Guest("/nkwebkit-blob/never-created.bin");
    Storage storage;
    _ZN7WebCore21BlobDataFileReferenceC2ERKN3WTF6StringES4_(storage.get(), &path, &replacement);
    Require(_ZN7WebCore21BlobDataFileReference4pathEv(storage.get()) == &storage.get()->replacementPath);
    Require(storage.get()->replacementPath.impl == replacement.impl);
    _ZN7WebCore21BlobDataFileReferenceD1Ev(storage.get());
}

static void TracksTheFile(const std::filesystem::path& root) {
    WriteFile(root / "tracked.bin", 37);
    auto path = Guest("/nkwebkit-blob/tracked.bin");
    String replacement;
    Storage storage;
    _ZN7WebCore21BlobDataFileReferenceC1ERKN3WTF6StringES4_(storage.get(), &path, &replacement);
    _ZN7WebCore21BlobDataFileReference26startTrackingModificationsEv(storage.get());
    Require(storage.get()->size == 37);
    const OptionalWallTime time = _ZN7WebCore21BlobDataFileReference24expectedModificationTimeEv(storage.get());
    Require(time.hasValue && time.value == GuestModificationSeconds("/nkwebkit-blob/tracked.bin"));
    Require(time.value == std::floor(time.value));
    _ZN7WebCore21BlobDataFileReferenceD1Ev(storage.get());
    Require(std::filesystem::exists(root / "tracked.bin"));
}

static void TakesTheSizeOfTheReplacement(const std::filesystem::path& root) {
    WriteFile(root / "source.bin", 5);
    WriteFile(root / "replacement.bin", 1234);
    auto path = Guest("/nkwebkit-blob/source.bin");
    auto replacement = Guest("/nkwebkit-blob/replacement.bin");
    Storage storage;
    _ZN7WebCore21BlobDataFileReferenceC1ERKN3WTF6StringES4_(storage.get(), &path, &replacement);
    _ZN7WebCore21BlobDataFileReference26startTrackingModificationsEv(storage.get());
    Require(storage.get()->size == 1234);
    Require(storage.get()->expectedModificationTime == GuestModificationSeconds("/nkwebkit-blob/source.bin"));
    _ZN7WebCore21BlobDataFileReferenceD1Ev(storage.get());
    Require(std::filesystem::exists(root / "source.bin"));
    Require(!std::filesystem::exists(root / "replacement.bin"));
}

static void KeepsTheTimeWhenTheReplacementIsMissing(const std::filesystem::path& root) {
    WriteFile(root / "kept.bin", 9);
    auto path = Guest("/nkwebkit-blob/kept.bin");
    auto replacement = Guest("/nkwebkit-blob/missing-replacement.bin");
    Storage storage;
    _ZN7WebCore21BlobDataFileReferenceC1ERKN3WTF6StringES4_(storage.get(), &path, &replacement);
    _ZN7WebCore21BlobDataFileReference26startTrackingModificationsEv(storage.get());
    Require(storage.get()->size == 0);
    Require(_ZN7WebCore21BlobDataFileReference24expectedModificationTimeEv(storage.get()).hasValue);
    _ZN7WebCore21BlobDataFileReferenceD1Ev(storage.get());
    Require(std::filesystem::exists(root / "kept.bin"));
}

static void IgnoresMissingAndEmptyPaths() {
    for (const char* name : {"/nkwebkit-blob/missing.bin", ""}) {
        auto path = Guest(name);
        String replacement;
        Storage storage;
        _ZN7WebCore21BlobDataFileReferenceC1ERKN3WTF6StringES4_(storage.get(), &path, &replacement);
        _ZN7WebCore21BlobDataFileReference26startTrackingModificationsEv(storage.get());
        Require(storage.get()->size == 0 && std::isnan(storage.get()->expectedModificationTime));
        _ZN7WebCore21BlobDataFileReferenceD1Ev(storage.get());
    }
}

static void DirectoriesHaveNoSize(const std::filesystem::path& root) {
    std::filesystem::create_directory(root / "folder");
    WriteFile(root / "folder" / "inside.bin", 64);
    auto path = Guest("/nkwebkit-blob/folder");
    auto replacement = Guest("/nkwebkit-blob/folder");
    Storage storage;
    _ZN7WebCore21BlobDataFileReferenceC1ERKN3WTF6StringES4_(storage.get(), &path, &replacement);
    _ZN7WebCore21BlobDataFileReference26startTrackingModificationsEv(storage.get());
    Require(storage.get()->size == 0);
    Require(_ZN7WebCore21BlobDataFileReference24expectedModificationTimeEv(storage.get()).hasValue);
    _ZN7WebCore21BlobDataFileReferenceD1Ev(storage.get());
    Require(std::filesystem::exists(root / "folder" / "inside.bin"));
}

static void CreateUsesTheVtable(const std::filesystem::path& root) {
    WriteFile(root / "created.bin", 3);
    WriteFile(root / "created-replacement.bin", 4);
    auto path = Guest("/nkwebkit-blob/created.bin");
    auto replacement = Guest("/nkwebkit-blob/created-replacement.bin");
    BlobDataFileReference* object = nullptr;
    {
        auto created = _ZN7WebCore21BlobDataFileReference6createERKN3WTF6StringES4_(&path, &replacement);
        object = created.ptr;
    }
    Require(object && object->refCount == 1 && object->size == 0);
    Require(guestAllocations.size() == 1 && guestAllocations.count(object) == 1);
    Require(_ZN7WebCore21BlobDataFileReference4pathEv(object) == &object->replacementPath);
    Slot(*object, 2)(object);
    Slot(*object, 3)(object);
    _ZN7WebCore21BlobDataFileReference20prepareForFileAccessEv(object);
    _ZN7WebCore21BlobDataFileReference16revokeFileAccessEv(object);
    Slot(*object, 1)(object);
    Require(guestAllocations.empty());
    Require(std::filesystem::exists(root / "created.bin"));
    Require(!std::filesystem::exists(root / "created-replacement.bin"));

    WriteFile(root / "complete.bin", 4);
    auto completePath = Guest("/nkwebkit-blob/complete.bin");
    Storage storage;
    _ZN7WebCore21BlobDataFileReferenceC1ERKN3WTF6StringES4_(storage.get(), &path, &completePath);
    Slot(*storage.get(), 0)(storage.get());
    Require(!std::filesystem::exists(root / "complete.bin"));
}

static void RefusesNull() {
    auto path = Guest("/nkwebkit-blob/null.bin");
    Storage storage;
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore21BlobDataFileReferenceC1ERKN3WTF6StringES4_(nullptr, &path, &path); }));
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore21BlobDataFileReferenceC1ERKN3WTF6StringES4_(storage.get(), nullptr, &path); }));
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore21BlobDataFileReferenceC1ERKN3WTF6StringES4_(storage.get(), &path, nullptr); }));
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore21BlobDataFileReference6createERKN3WTF6StringES4_(nullptr, &path); }));
    Require(ThrowsInvalidArgument([] { _ZN7WebCore21BlobDataFileReference4pathEv(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN7WebCore21BlobDataFileReference24expectedModificationTimeEv(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN7WebCore21BlobDataFileReference26startTrackingModificationsEv(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN7WebCore21BlobDataFileReferenceD1Ev(nullptr); }));
}

int main() {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    const auto root = std::filesystem::current_path() / ("nkwebkit-blob-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto hostPath = root.u8string();
    AddPathAlias_nid_no_patch("/nkwebkit-blob", reinterpret_cast<const char*>(hostPath.c_str()));
    ConstructsUntracked();
    PathPrefersTheReplacement();
    TracksTheFile(root);
    TakesTheSizeOfTheReplacement(root);
    KeepsTheTimeWhenTheReplacementIsMissing(root);
    IgnoresMissingAndEmptyPaths();
    DirectoriesHaveNoSize(root);
    CreateUsesTheVtable(root);
    RefusesNull();
    std::filesystem::remove_all(root);
    std::puts("BlobDataFileReference passed");
    return 0;
}
