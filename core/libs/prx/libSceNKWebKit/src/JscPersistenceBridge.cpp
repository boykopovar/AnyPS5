#include "prx/libSceJsc/include/WtfSha1Exports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>

namespace NKWebKit {

namespace {

void* FastMalloc(std::size_t size) {
    return _ZN3WTF10fastMallocEm(size);
}

void FastFree(void* ptr) {
    _ZN3WTF8fastFreeEPv(ptr);
}

constexpr Jsc::WtfAbi::PersistenceAllocator persistenceAllocator{&FastMalloc, &FastFree};

struct PersistenceAllocatorRegistration {
    PersistenceAllocatorRegistration() {
        RegisterPersistenceAllocator_nid_no_patch(&persistenceAllocator);
    }

    ~PersistenceAllocatorRegistration() {
        RegisterPersistenceAllocator_nid_no_patch(nullptr);
    }
};

const PersistenceAllocatorRegistration persistenceAllocatorRegistration{};

}

}
