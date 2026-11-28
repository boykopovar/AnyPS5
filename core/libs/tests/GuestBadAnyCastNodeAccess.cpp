#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>

struct TypeRecord { const void* vtable; const char* name; const TypeRecord* base; };

using GuestWhat = const char* (APS5_VABI *)(const LibcException::ExceptionObject*);
using Thrower = void (APS5_VABI *)(const void*);

extern "C" {
extern const TypeRecord _ZTISt9exception_nid_postfix;
extern const TypeRecord _ZTISt8bad_cast_nid_postfix;
extern const TypeRecord _ZTISt11logic_error_nid_postfix;
extern const TypeRecord _ZTISt13runtime_error_nid_postfix;

void APS5_VABI SceV2Catch(Thrower thrower, const void* argument);

const void* SceV2CatchType = nullptr;
const void* SceV2OtherType = nullptr;
int SceV2Selector = 0;
}

static unsigned failures = 0;
static std::string inspected;
static char source[32];
static bool overwriteSource = false;
static unsigned newHandlerCalls = 0;

extern "C" void APS5_VABI SceV2Inspect(void* object) {
    const auto* exception = static_cast<const LibcException::ExceptionObject*>(object);
    const auto* vtable = static_cast<const void* const*>(exception->vtable);
    if (overwriteSource) std::memset(source, 'x', sizeof(source) - 1);
    inspected = static_cast<const TypeRecord*>(vtable[-1])->name;
    inspected += '|';
    inspected += static_cast<const TypeRecord*>(vtable[-1])->base->name;
    inspected += '|';
    inspected += reinterpret_cast<GuestWhat>(const_cast<void*>(vtable[2]))(exception);
}

static TypeRecord Named(const char* name) {
    return {_ZTISt9exception_nid_postfix.vtable, name, nullptr};
}

struct Case { const char* label; Thrower thrower; const void* message; const void* catchType; const void* otherType; int selector; const char* expected; };

static void Run(const Case& test) {
    SceV2CatchType = test.catchType;
    SceV2OtherType = test.otherType;
    SceV2Selector = 0;
    inspected.clear();
    SceV2Catch(test.thrower, test.message);
    if (SceV2Selector == test.selector && inspected == test.expected) return;
    std::fprintf(stderr, "FAIL: %s: selector %d, got '%s', want '%s'\n", test.label, SceV2Selector, inspected.c_str(), test.expected);
    ++failures;
}

static void APS5_VABI ThrowingNewHandler() {
    ++newHandlerCalls;
    throw std::runtime_error("new_handler failed");
}

static void CheckNothrow(bool condition, const char* label) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", label);
    ++failures;
}

static void TestNothrowNewCatchAll() {
    const std::array<void*, 10> none{};
    ApplicationHeapRegister_nid_no_patch(none.data());
    newHandlerCalls = 0;
    constexpr std::size_t huge = std::numeric_limits<std::size_t>::max();
    const GuestNewHandler previousHandler = _ZSt15set_new_handlerPFvvE_nid_postfix(&ThrowingNewHandler);
    CheckNothrow(_ZnwmRKSt9nothrow_t_nid_postfix(huge, nullptr) == nullptr && newHandlerCalls == 1,
        "nothrow new catches non-bad_alloc from new_handler");
    CheckNothrow(_ZnamRKSt9nothrow_t_nid_postfix(huge, nullptr) == nullptr && newHandlerCalls == 2,
        "nothrow array new catches non-bad_alloc from new_handler");
    CheckNothrow(_ZnwmSt11align_val_tRKSt9nothrow_t_nid_postfix(huge, 64, nullptr) == nullptr && newHandlerCalls == 3,
        "nothrow aligned new catches non-bad_alloc from new_handler");
    CheckNothrow(_ZnamSt11align_val_tRKSt9nothrow_t_nid_postfix(huge, 64, nullptr) == nullptr && newHandlerCalls == 4,
        "nothrow aligned array new catches non-bad_alloc from new_handler");
    const GuestNewHandler restoredFrom = _ZSt15set_new_handlerPFvvE_nid_postfix(previousHandler);
    CheckNothrow(restoredFrom == &ThrowingNewHandler, "set_new_handler returns previous throwing handler on restore");
    CheckNothrow(_ZSt15get_new_handlerv_nid_postfix() == previousHandler, "get_new_handler reflects restored handler");
    CheckNothrow(_ZnwmRKSt9nothrow_t_nid_postfix(huge, nullptr) == nullptr && newHandlerCalls == 4,
        "nothrow new returns nullptr without calling restored handler");
    void* small = _ZnwmRKSt9nothrow_t_nid_postfix(16, nullptr);
    CheckNothrow(small != nullptr, "nothrow new allocates small valid block");
    _ZdlPvRKSt9nothrow_t_nid_postfix(small, nullptr);
}

int main() {
    const auto badAnyCast = reinterpret_cast<Thrower>(_ZSt19_Throw_bad_any_castPKc_nid_postfix);
    const auto badNodeAccess = reinterpret_cast<Thrower>(_ZSt22_Throw_bad_node_accessPKc_nid_postfix);
    const TypeRecord badAnyCastName = Named("St12bad_any_cast");
    const TypeRecord badNodeAccessName = Named("St16_Bad_node_access");
    const Case cases[] {
        {"bad_any_cast caught as bad_cast", badAnyCast, "Bad any_cast", &_ZTISt8bad_cast_nid_postfix, &_ZTISt11logic_error_nid_postfix, 2,
            "St12bad_any_cast|St8bad_cast|Bad any_cast"},
        {"bad_any_cast caught as exception", badAnyCast, "any", &_ZTISt9exception_nid_postfix, &badNodeAccessName, 2,
            "St12bad_any_cast|St8bad_cast|any"},
        {"bad_any_cast caught by its own name", badAnyCast, "named", &_ZTISt9exception_nid_postfix, &badAnyCastName, 1,
            "St12bad_any_cast|St8bad_cast|named"},
        {"bad_any_cast with an empty message", badAnyCast, "", &_ZTISt8bad_cast_nid_postfix, &_ZTISt13runtime_error_nid_postfix, 2,
            "St12bad_any_cast|St8bad_cast|"},
        {"_Bad_node_access caught as logic_error", badNodeAccess, "node handle is empty", &_ZTISt11logic_error_nid_postfix, &_ZTISt8bad_cast_nid_postfix, 2,
            "St16_Bad_node_access|St11logic_error|node handle is empty"},
        {"_Bad_node_access is not a runtime_error", badNodeAccess, "node", &_ZTISt9exception_nid_postfix, &_ZTISt13runtime_error_nid_postfix, 2,
            "St16_Bad_node_access|St11logic_error|node"},
        {"_Bad_node_access caught by its own name", badNodeAccess, "named", &_ZTISt9exception_nid_postfix, &badNodeAccessName, 1,
            "St16_Bad_node_access|St11logic_error|named"},
    };
    for (const auto& test : cases) Run(test);
    std::strcpy(source, "copied message");
    overwriteSource = true;
    Run({"_Bad_node_access keeps its own copy", badNodeAccess, source, &_ZTISt11logic_error_nid_postfix, &_ZTISt8bad_cast_nid_postfix, 2,
        "St16_Bad_node_access|St11logic_error|copied message"});
    std::strcpy(source, "copied message");
    Run({"bad_any_cast keeps its own copy", badAnyCast, source, &_ZTISt8bad_cast_nid_postfix, &_ZTISt11logic_error_nid_postfix, 2,
        "St12bad_any_cast|St8bad_cast|copied message"});
    TestNothrowNewCatchAll();
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
