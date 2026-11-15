#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

struct Case { const char* label; Thrower thrower; const char* message; const void* catchType; const void* otherType; int selector; const char* expected; };

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
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
