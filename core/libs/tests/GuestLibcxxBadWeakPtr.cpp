#include "prx/libc/include/Api.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

}

int main() {
    const auto& rtti = _ZTINSt3__112bad_weak_ptrE_nid_postfix;
    const auto& table = _ZTVNSt3__112bad_weak_ptrE_nid_postfix;
    const auto* category = static_cast<const LibcException::TypeRecord*>(rtti.vtable[-1]);
    Require(std::strcmp(rtti.name, "NSt3__112bad_weak_ptrE") == 0, "incorrect libc++ bad_weak_ptr RTTI name");
    Require(std::strcmp(category->name, "N10__cxxabiv120__si_class_type_infoE") == 0 &&
            rtti.base && std::strcmp(rtti.base->name, "St9exception") == 0,
            "libc++ bad_weak_ptr must have std::exception as its single public base");
    Require(table.offset == 0 && table.type == &rtti, "bad_weak_ptr vtable must refer to the libc++ type");

    LibcException::ExceptionObject exception{&table.destroy, nullptr};
    Require(std::strcmp(table.what(&exception), "bad_weak_ptr") == 0, "incorrect bad_weak_ptr what()");
    _ZNSt3__112bad_weak_ptrD1Ev_nid_postfix(&exception);
    LibcException::ExceptionObject second{&table.destroy, nullptr};
    table.destroy(&second);

    void* allocation = std::malloc(sizeof(LibcException::ExceptionObject));
    Require(allocation != nullptr, "failed to allocate bad_weak_ptr object");
    auto* allocated = new (allocation) LibcException::ExceptionObject{&table.destroy, nullptr};
    table.deleteObject(allocated);
    std::puts("libc++ bad_weak_ptr destructor, RTTI and vtable checks passed");
    return 0;
}
