#include "../include/Pthread.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {
int APS5_VABI pthread_key_create_nid_postfix(PthreadKey*, pthread_key_destructor_func_t);
int APS5_VABI pthread_key_delete_nid_postfix(PthreadKey);
void* APS5_VABI pthread_getspecific_nid_postfix(PthreadKey);
int APS5_VABI pthread_setspecific_nid_postfix(PthreadKey, const void*);

int APS5_VABI scePthreadKeyCreate(PthreadKey* key, pthread_key_destructor_func_t destructor) {
    return pthread_key_create_nid_postfix(key, destructor);
}

int APS5_VABI scePthreadKeyDelete(PthreadKey key) {
    return pthread_key_delete_nid_postfix(key);
}

void* APS5_VABI scePthreadGetspecific(PthreadKey key) {
    return pthread_getspecific_nid_postfix(key);
}

int APS5_VABI scePthreadSetspecific(PthreadKey key, void* value) {
    return pthread_setspecific_nid_postfix(key, value);
}

}
