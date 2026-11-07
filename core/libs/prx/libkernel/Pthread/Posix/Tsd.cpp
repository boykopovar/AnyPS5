#include "prx/libkernel/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "Common.hpp"

extern "C" {

void* APS5_VABI pthread_getspecific_nid_postfix(PthreadKey key) {
    return scePthreadGetspecific(key);
}

int APS5_VABI pthread_setspecific_nid_postfix(PthreadKey key, void* value) {
    return PosixThread::ToErrno(scePthreadSetspecific(key, value));
}

int APS5_VABI pthread_key_create_nid_postfix(PthreadKey* key, pthread_key_destructor_func_t destructor) {
    if (!key) return PosixThread::GUEST_EINVAL;
    return PosixThread::ToErrno(scePthreadKeyCreate(key, destructor));
}

int APS5_VABI pthread_key_delete_nid_postfix(PthreadKey key) {
    return PosixThread::ToErrno(scePthreadKeyDelete(key));
}

}
