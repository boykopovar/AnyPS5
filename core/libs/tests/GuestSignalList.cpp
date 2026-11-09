#include <cstdlib>
#include <cstring>

extern "C" {
extern const char* const sys_siglist_nid_postfix[32];
extern const int sys_nsig_nid_postfix;
}

static constexpr int GUEST_NSIG = 32;
static constexpr int GUEST_SIGHUP = 1;
static constexpr int GUEST_SIGINT = 2;
static constexpr int GUEST_SIGSEGV = 11;
static constexpr int GUEST_SIGUSR2 = 31;

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    Require(sys_nsig_nid_postfix == GUEST_NSIG);
    Require(std::strcmp(sys_siglist_nid_postfix[0], "Signal 0") == 0);
    Require(std::strcmp(sys_siglist_nid_postfix[GUEST_SIGHUP], "Hangup") == 0);
    Require(std::strcmp(sys_siglist_nid_postfix[GUEST_SIGINT], "Interrupt") == 0);
    Require(std::strcmp(sys_siglist_nid_postfix[GUEST_SIGSEGV], "Segmentation fault") == 0);
    Require(std::strcmp(sys_siglist_nid_postfix[GUEST_SIGUSR2], "User defined signal 2") == 0);
    for (int signal = 0; signal < sys_nsig_nid_postfix; ++signal) Require(sys_siglist_nid_postfix[signal] != nullptr && sys_siglist_nid_postfix[signal][0] != '\0');
}
