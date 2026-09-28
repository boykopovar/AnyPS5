#include "SceTypes.hpp"
#include <atomic>
#include <cassert>
#include <thread>

extern "C" {
int APS5_VABI pthread_key_create_nid_postfix(PthreadKey*, pthread_key_destructor_func_t);
int APS5_VABI pthread_key_delete_nid_postfix(PthreadKey);
void* APS5_VABI pthread_getspecific_nid_postfix(PthreadKey);
int APS5_VABI pthread_setspecific_nid_postfix(PthreadKey, const void*);
int APS5_VABI scePthreadKeyCreate(PthreadKey*, pthread_key_destructor_func_t);
int APS5_VABI scePthreadKeyDelete(PthreadKey);
void* APS5_VABI scePthreadGetspecific(PthreadKey);
int APS5_VABI scePthreadSetspecific(PthreadKey, void*);
}

static std::atomic<int> destructorCalls{0};
static PthreadKey repeatedKey = 0;

static void APS5_VABI CountDestructor(void* value) {
    assert(value != nullptr);
    ++destructorCalls;
}

static void APS5_VABI RepeatDestructor(void* value) {
    assert(value != nullptr);
    ++destructorCalls;
    assert(pthread_setspecific_nid_postfix(repeatedKey, value) == 0);
}

int main() {
    assert(pthread_key_create_nid_postfix(nullptr, nullptr) == 22);
    PthreadKey first = 0;
    assert(pthread_key_create_nid_postfix(&first, CountDestructor) == 0);
    assert(first > 0);
    assert(pthread_getspecific_nid_postfix(first) == nullptr);
    int mainValue = 1;
    assert(pthread_setspecific_nid_postfix(first, &mainValue) == 0);
    assert(pthread_getspecific_nid_postfix(first) == &mainValue);
    std::thread worker([&] {
        assert(pthread_getspecific_nid_postfix(first) == nullptr);
        int workerValue = 2;
        assert(pthread_setspecific_nid_postfix(first, &workerValue) == 0);
        assert(pthread_getspecific_nid_postfix(first) == &workerValue);
    });
    worker.join();
    assert(destructorCalls == 1);
    assert(pthread_getspecific_nid_postfix(first) == &mainValue);
    assert(pthread_key_delete_nid_postfix(first) == 0);
    assert(pthread_key_delete_nid_postfix(first) == 22);
    assert(pthread_setspecific_nid_postfix(first, &mainValue) == 22);
    assert(pthread_getspecific_nid_postfix(first) == nullptr);

    PthreadKey reused = 0;
    assert(pthread_key_create_nid_postfix(&reused, CountDestructor) == 0);
    assert(reused == first);
    assert(pthread_getspecific_nid_postfix(reused) == nullptr);
    assert(pthread_key_delete_nid_postfix(reused) == 0);

    PthreadKey aliasKey = 0;
    assert(scePthreadKeyCreate(&aliasKey, CountDestructor) == 0);
    std::thread aliasWorker([&] {
        int value = 3;
        assert(scePthreadSetspecific(aliasKey, &value) == 0);
        assert(scePthreadGetspecific(aliasKey) == &value);
    });
    aliasWorker.join();
    assert(destructorCalls == 2);
    assert(scePthreadKeyDelete(aliasKey) == 0);

    assert(pthread_key_create_nid_postfix(&repeatedKey, RepeatDestructor) == 0);
    std::thread repeatedWorker([] {
        int value = 4;
        assert(pthread_setspecific_nid_postfix(repeatedKey, &value) == 0);
    });
    repeatedWorker.join();
    assert(destructorCalls == 6);
    assert(pthread_key_delete_nid_postfix(repeatedKey) == 0);
}
