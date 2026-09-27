#include "SceTypes.hpp"
#include <cerrno>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#endif

extern "C" {
void APS5_VABI pthread_set_name_np_nid_postfix(Pthread, const char*);
int APS5_VABI pthread_rename_np_nid_postfix(Pthread, const char*);
Pthread APS5_VABI pthread_self_nid_postfix();
}

static void Require(bool condition) {
    if (!condition) std::abort();
}

int main() {
    // The host entry thread has no allocated guest PthreadPrivate yet.
    Require(pthread_self_nid_postfix() == nullptr);
    pthread_set_name_np_nid_postfix(nullptr, "crispy-doom");
#ifdef _WIN32
    PWSTR description = nullptr;
    Require(SUCCEEDED(GetThreadDescription(GetCurrentThread(), &description)));
    Require(std::wcscmp(description, L"crispy-doom") == 0);
    LocalFree(description);
#else
    char name[16]{};
    Require(pthread_getname_np(pthread_self(), name, sizeof(name)) == 0);
    Require(std::strcmp(name, "crispy-doom") == 0);
#endif
    Require(pthread_rename_np_nid_postfix(nullptr, "doom-audio") == 0);
    Require(pthread_rename_np_nid_postfix(nullptr, nullptr) == EINVAL);
    errno = 0;
    pthread_set_name_np_nid_postfix(nullptr, nullptr);
    Require(errno == EINVAL);
}
