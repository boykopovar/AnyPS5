#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include <cerrno>
#include <cstdlib>

extern "C" int APS5_VABI pthread_cond_init_nid_postfix(PthreadCond*, const PthreadCondattr*);
extern "C" int APS5_VABI pthread_cond_destroy_nid_postfix(PthreadCond*);
extern "C" int APS5_VABI scePthreadCondInit(PthreadCond*, const PthreadCondattr*, const char*);
extern "C" int APS5_VABI scePthreadCondDestroy(PthreadCond*);

static void Require(bool condition) { if (!condition) std::abort(); }

int main() {
    PthreadCond cond = nullptr;
    Require(pthread_cond_init_nid_postfix(nullptr, nullptr) == EINVAL);
    Require(pthread_cond_init_nid_postfix(&cond, nullptr) == 0 && cond);
    Require(pthread_cond_destroy_nid_postfix(&cond) == 0 && !cond);
    Require(pthread_cond_destroy_nid_postfix(&cond) == EINVAL);
    Require(pthread_cond_destroy_nid_postfix(nullptr) == EINVAL);

    PthreadCondattr invalid = nullptr;
    Require(pthread_cond_init_nid_postfix(&cond, &invalid) == EINVAL && !cond);
    Require(scePthreadCondInit(&cond, nullptr, nullptr) == 0 && cond);
    Require(pthread_cond_destroy_nid_postfix(&cond) == 0 && !cond);
    Require(pthread_cond_init_nid_postfix(&cond, nullptr) == 0 && cond);
    Require(scePthreadCondDestroy(&cond) == 0 && !cond);
}
