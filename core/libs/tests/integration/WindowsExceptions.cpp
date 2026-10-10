#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <string>
#include <thread>
#include <typeinfo>

extern "C" {
extern const unsigned char _ZTVN10__cxxabiv117__class_type_infoE_nid_postfix[];
extern const unsigned char _ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix[];
extern const unsigned char _ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix[];
void NotImplemented_nid_no_patch(const char*);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

struct TypeRecord { const void* vtable; const char* name; };
struct SingleRecord { TypeRecord type; const TypeRecord* base; };
struct BaseRecord { const TypeRecord* type; std::ptrdiff_t flags; };
struct MultipleRecord { TypeRecord type; unsigned flags; unsigned count; BaseRecord bases[2]; };

using CatchType = bool (APS5_VABI *)(const void*, const void*, void**, unsigned);
using UpcastType = bool (APS5_VABI *)(const void*, const void*, void**);

struct TypeRecords {
    const unsigned char* classTable = _ZTVN10__cxxabiv117__class_type_infoE_nid_postfix;
    const unsigned char* singleTable = _ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix;
    const unsigned char* multipleTable = _ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix;
    TypeRecord base {classTable + 2 * sizeof(void*), "4Base"};
    TypeRecord other {classTable + 2 * sizeof(void*), "5Other"};
    SingleRecord single {{singleTable + 2 * sizeof(void*), "6Single"}, &base};
    MultipleRecord multiple {{multipleTable + 2 * sizeof(void*), "8Multiple"}, 0, 2, {{&other, 2}, {&base, (16 << 8) | 2}}};
    std::array<unsigned char, 32> storage {};

    CatchType Catch() const {
        CatchType catchType;
        std::memcpy(&catchType, classTable + 6 * sizeof(void*), sizeof(catchType));
        return catchType;
    }

    UpcastType Upcast() const {
        UpcastType upcastType;
        std::memcpy(&upcastType, multipleTable + 7 * sizeof(void*), sizeof(upcastType));
        return upcastType;
    }
};

int destroyed = 0;

struct Guard { ~Guard() { ++destroyed; } };

class GuardCounter {
public:
    GuardCounter() { destroyed = 0; }
    ~GuardCounter() { destroyed = 0; }
    GuardCounter(const GuardCounter&) = delete;
    GuardCounter& operator=(const GuardCounter&) = delete;
};

void ThrowNested() {
    Guard guard;
    throw std::runtime_error("native own unwind");
}

void Rethrow() {
    Guard guard;
    try { ThrowNested(); }
    catch (const std::runtime_error&) { throw; }
}

class TrackedError : public std::runtime_error {
public:
    explicit TrackedError(std::atomic<int>* count) : std::runtime_error("retained error"), count(count) {}
    ~TrackedError() override { ++*count; }

private:
    std::atomic<int>* count;
};

const Case rttiCategories{"TypeInfoVtables_CategoryTypeInfo_HasAbiNames", [] {
    const unsigned char* tables[] {
        _ZTVN10__cxxabiv117__class_type_infoE_nid_postfix,
        _ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix,
        _ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix};
    const char* names[] {"N10__cxxabiv117__class_type_infoE", "N10__cxxabiv120__si_class_type_infoE", "N10__cxxabiv121__vmi_class_type_infoE"};
    for (std::size_t index = 0; index < 3; ++index) {
        const std::type_info* category;
        std::memcpy(&category, tables[index] + sizeof(void*), sizeof(category));
        RequireEqual(std::string(category->name()), std::string(names[index]), "RTTI category of " + std::string(names[index]));
    }
}};

const Case singleCatch{"TypeInfoVtables_CatchBaseOfSingleInheritance_KeepsObjectAddress", [] {
    TypeRecords records;
    void* object = records.storage.data();
    const bool caught = records.Catch()(&records.base, &records.single, &object, 0);
    Require(caught, "single inheritance catch");
    Require(object == records.storage.data(), "adjusted object address");
}};

const Case multipleCatch{"TypeInfoVtables_CatchBaseOfMultipleInheritance_AdjustsToBaseOffset", [] {
    TypeRecords records;
    void* object = records.storage.data();
    const bool caught = records.Catch()(&records.base, &records.multiple, &object, 0);
    Require(caught, "multiple inheritance catch");
    Require(object == records.storage.data() + 16, "adjusted object address");
}};

const Case multipleUpcast{"TypeInfoVtables_UpcastMultipleInheritanceToBase_AdjustsToBaseOffset", [] {
    TypeRecords records;
    void* object = records.storage.data();
    const bool upcast = records.Upcast()(&records.multiple, &records.base, &object);
    Require(upcast, "multiple inheritance upcast");
    Require(object == records.storage.data() + 16, "adjusted object address");
}};

const Case unrelatedCatch{"TypeInfoVtables_CatchUnrelatedType_DoesNotMatch", [] {
    TypeRecords records;
    void* object = records.storage.data();
    Require(!records.Catch()(&records.other, &records.single, &object, 0), "unrelated catch rejected");
}};

const Case exceptionPointer{"ExceptionPtr_RethrownOnFourThreads_SharesObjectAndDestroysOnce", [] {
    std::atomic<int> count{0};
    std::atomic<int> caught{0};
    std::exception_ptr retained;
    const TrackedError* original = nullptr;
    try {
        throw TrackedError(&count);
    } catch (const TrackedError& error) {
        original = &error;
        retained = std::current_exception();
    }
    Require(static_cast<bool>(retained), "exception retained");
    RequireEqual(count.load(), 0, "destructions while retained");
    std::array<std::thread, 4> threads;
    for (auto& thread : threads) {
        thread = std::thread([retained, original, &caught] {
            try {
                std::rethrow_exception(retained);
            } catch (const TrackedError& error) {
                if (&error == original && std::strcmp(error.what(), "retained error") == 0) ++caught;
            }
        });
    }
    for (auto& thread : threads) thread.join();
    RequireEqual(caught.load(), 4, "threads that caught the original object");
    RequireEqual(count.load(), 0, "destructions after cross-thread rethrow");
    auto copy = retained;
    retained = nullptr;
    RequireEqual(count.load(), 0, "destructions while a copy holds ownership");
    copy = nullptr;
    RequireEqual(count.load(), 1, "destructions after releasing every owner");
    Require(!std::current_exception(), "no stale current exception");
}};

const Case rethrow{"Rethrow_ThroughCleanupFrames_CaughtWithMessageAndGuardsDestroyed", [] {
    const GuardCounter counter;
    bool returned = false;
    bool caught = false;
    std::string message;
    try {
        Rethrow();
        returned = true;
    } catch (const std::runtime_error& error) {
        caught = true;
        message = error.what();
    }
    Require(!returned, "Rethrow threw");
    Require(caught, "caught as std::runtime_error");
    RequireEqual(message, std::string("native own unwind"), "what()");
    RequireEqual(destroyed, 2, "guards destroyed");
}};

const Case crossDll{"NotImplemented_CrossDll_ThrowsRuntimeErrorWithMessage", [] {
    bool caught = false;
    std::string message;
    try { NotImplemented_nid_no_patch("cross DLL"); }
    catch (const std::runtime_error& error) { caught = true; message = error.what(); }
    Require(caught, "caught as std::runtime_error");
    RequireEqual(message, std::string("cross DLL not implemented"), "what()");
}};

const Case crossDllRethrow{"NotImplemented_RethrownInsideHandler_CaughtAsSameStdException", [] {
    const GuardCounter counter;
    bool caught = false;
    int handler = 0;
    bool same = false;
    int destroyedAtCatch = -1;
    try { NotImplemented_nid_no_patch("cross DLL"); }
    catch (const std::runtime_error& error) {
        caught = true;
        try {
            Guard guard;
            throw;
        } catch (const std::logic_error&) {
            handler = 1;
        } catch (const std::exception& rethrown) {
            handler = 2;
            same = &rethrown == &error;
            destroyedAtCatch = destroyed;
        }
    }
    Require(caught, "cross DLL error caught");
    RequireEqual(handler, 2, "selected handler");
    Require(same, "rethrown object is the original object");
    RequireEqual(destroyedAtCatch, 1, "guards destroyed");
}};

const Case crossDllCatchAll{"NotImplemented_IntThrownInsideHandler_CaughtByCatchAllAndRethrown", [] {
    const GuardCounter counter;
    bool caught = false;
    int handler = 0;
    int destroyedAtCatch = -1;
    int rethrownValue = 0;
    try { NotImplemented_nid_no_patch("cross DLL"); }
    catch (const std::runtime_error&) {
        caught = true;
        try {
            Guard guard;
            throw 42;
        } catch (const std::exception&) {
            handler = 1;
        } catch (...) {
            handler = 2;
            destroyedAtCatch = destroyed;
            try {
                throw;
            } catch (int value) {
                rethrownValue = value;
            }
        }
    }
    Require(caught, "cross DLL error caught");
    RequireEqual(handler, 2, "selected handler");
    RequireEqual(destroyedAtCatch, 1, "guards destroyed");
    RequireEqual(rethrownValue, 42, "rethrown value");
}};

} // namespace
