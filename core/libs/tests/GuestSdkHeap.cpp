#include <cstddef>

extern "C" void HeapTestSdkInitialize();
extern "C" void HeapTestSdkFinalize();
extern "C" void* HeapTestAllocatorEntry(unsigned index);

namespace {
struct SdkLifetime {
    SdkLifetime() { HeapTestSdkInitialize(); }
    ~SdkLifetime() { HeapTestSdkFinalize(); }
};
SdkLifetime sdkLifetime;
}

extern "C" void* SdkMalloc(std::size_t size) {
    return reinterpret_cast<void* (*)(std::size_t)>(HeapTestAllocatorEntry(0))(size);
}
extern "C" void SdkFree(void* pointer) {
    reinterpret_cast<void (*)(void*)>(HeapTestAllocatorEntry(1))(pointer);
}
extern "C" void* SdkCalloc(std::size_t count, std::size_t size) {
    return reinterpret_cast<void* (*)(std::size_t, std::size_t)>(HeapTestAllocatorEntry(2))(count, size);
}
extern "C" void* SdkRealloc(void* pointer, std::size_t size) {
    return reinterpret_cast<void* (*)(void*, std::size_t)>(HeapTestAllocatorEntry(3))(pointer, size);
}
extern "C" void* SdkMemalign(std::size_t alignment, std::size_t size) {
    return reinterpret_cast<void* (*)(std::size_t, std::size_t)>(HeapTestAllocatorEntry(4))(alignment, size);
}
extern "C" void* SdkReallocalign(void* pointer, std::size_t size, std::size_t alignment) {
    return reinterpret_cast<void* (*)(void*, std::size_t, std::size_t)>(HeapTestAllocatorEntry(5))(pointer, size, alignment);
}
extern "C" int SdkPosixMemalign(void** pointer, std::size_t alignment, std::size_t size) {
    return reinterpret_cast<int (*)(void**, std::size_t, std::size_t)>(HeapTestAllocatorEntry(6))(pointer, alignment, size);
}
extern "C" int SdkStats(void*) { return 0; }
extern "C" int SdkStatsFast(void*) { return 0; }
extern "C" std::size_t SdkUsableSize(void*) { return 256; }

#define SDK_EXPORT(function, nid) asm(".globl \"" nid "#guest\"\n.set \"" nid "#guest\", " #function "\n")
SDK_EXPORT(SdkFree, "tIhsqj0qsFE");
#ifndef SDK_HEAP_PARTIAL
SDK_EXPORT(SdkMalloc, "gQX+4GDQjpM");
SDK_EXPORT(SdkCalloc, "2X5agFjKxMc");
SDK_EXPORT(SdkRealloc, "Y7aJ1uydPMo");
SDK_EXPORT(SdkMemalign, "Ujf3KzMvRmI");
SDK_EXPORT(SdkReallocalign, "OGybVuPAhAY");
SDK_EXPORT(SdkPosixMemalign, "cVSk9y8URbc");
SDK_EXPORT(SdkStats, "CC-BLMBu9-I");
SDK_EXPORT(SdkStatsFast, "KuOuD58hqn4");
SDK_EXPORT(SdkUsableSize, "NDcSfcYZRC8");
#endif
