#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" {
int APS5_VABI socket_nid_postfix(int, int, int);
int APS5_VABI close_nid_postfix(int);
int APS5_VABI dup_nid_postfix(int);
int APS5_VABI dup2_nid_postfix(int, int);
int APS5_VABI pipe_nid_postfix(int*);
int APS5_VABI fcntl_nid_postfix(int, int, ...);
int* APS5_VABI __error_nid_postfix();
}

namespace {

constexpr int DuplicateDescriptor = 0;
constexpr int GetDescriptorFlags = 1;
constexpr int SetDescriptorFlags = 2;
constexpr int Duplicate2Descriptor = 10;
constexpr int DuplicateDescriptorCloseOnExec = 17;
constexpr int Duplicate2DescriptorCloseOnExec = 18;
constexpr int CloseOnExec = 1;
constexpr int Stream = 1;
constexpr int SocketCloseOnExec = 0x10000000;
constexpr int GuestEbadf = 9;
constexpr int GuestEinval = 22;

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "%s (errno %d)\n", what, *__error_nid_postfix());
    std::abort();
}

}

int main() {
    const int plain = socket_nid_postfix(2, Stream, 0);
    Require(plain >= 0, "socket failed");
    Require(fcntl_nid_postfix(plain, GetDescriptorFlags) == 0, "a new socket must not be close-on-exec (F_GETFD 0)");
    Require(fcntl_nid_postfix(plain, SetDescriptorFlags, CloseOnExec) == 0, "F_SETFD on a socket must succeed");
    Require(fcntl_nid_postfix(plain, GetDescriptorFlags) == CloseOnExec, "F_GETFD must read back FD_CLOEXEC");
    const int copy = dup_nid_postfix(plain);
    Require(copy >= 0 && fcntl_nid_postfix(copy, GetDescriptorFlags) == 0, "dup must clear FD_CLOEXEC on the copy");
    Require(fcntl_nid_postfix(plain, GetDescriptorFlags) == CloseOnExec, "dup must leave the flag of the original");
    Require(fcntl_nid_postfix(plain, SetDescriptorFlags, 0) == 0 && fcntl_nid_postfix(plain, GetDescriptorFlags) == 0, "F_SETFD 0 must clear FD_CLOEXEC");
    Require(close_nid_postfix(copy) == 0, "close failed");

    const int target = socket_nid_postfix(2, Stream, 0);
    Require(target >= 0, "socket failed");
    Require(fcntl_nid_postfix(target, SetDescriptorFlags, CloseOnExec) == 0 && fcntl_nid_postfix(target, GetDescriptorFlags) == CloseOnExec, "F_SETFD on the dup2 target must succeed");
    Require(fcntl_nid_postfix(plain, SetDescriptorFlags, CloseOnExec) == 0, "F_SETFD on a socket must succeed");
    Require(dup2_nid_postfix(plain, target) == target, "dup2 onto another socket must return the target");
    Require(fcntl_nid_postfix(target, GetDescriptorFlags) == 0, "dup2 must clear FD_CLOEXEC on the target");
    Require(fcntl_nid_postfix(plain, GetDescriptorFlags) == CloseOnExec, "dup2 must leave the flag of the original");
    Require(dup2_nid_postfix(plain, plain) == plain && fcntl_nid_postfix(plain, GetDescriptorFlags) == CloseOnExec, "dup2 onto the same descriptor must leave the flag");
    Require(fcntl_nid_postfix(plain, SetDescriptorFlags, 0) == 0, "F_SETFD 0 must clear FD_CLOEXEC");
    Require(close_nid_postfix(target) == 0, "close failed");

    Require(fcntl_nid_postfix(plain, SetDescriptorFlags, CloseOnExec) == 0, "F_SETFD on a socket must succeed");
    const int minimumDup = fcntl_nid_postfix(plain, DuplicateDescriptor, plain + 4);
    Require(minimumDup >= plain + 4 && fcntl_nid_postfix(minimumDup, GetDescriptorFlags) == 0, "F_DUPFD must honor the minimum and clear FD_CLOEXEC");
    Require(fcntl_nid_postfix(plain, GetDescriptorFlags) == CloseOnExec, "F_DUPFD must leave the flag of the original");
    Require(fcntl_nid_postfix(plain, SetDescriptorFlags, 0) == 0, "F_SETFD 0 must clear FD_CLOEXEC");
    const int cloexecDup = fcntl_nid_postfix(plain, DuplicateDescriptorCloseOnExec, minimumDup + 1);
    Require(cloexecDup >= minimumDup + 1 && fcntl_nid_postfix(cloexecDup, GetDescriptorFlags) == CloseOnExec, "F_DUPFD_CLOEXEC must honor the minimum and set FD_CLOEXEC");
    Require(fcntl_nid_postfix(plain, GetDescriptorFlags) == 0, "F_DUPFD_CLOEXEC must leave the flag of the original");
    Require(fcntl_nid_postfix(plain, DuplicateDescriptor, -1) == -1 && *__error_nid_postfix() == GuestEinval, "F_DUPFD with a negative minimum must fail with EINVAL");
    Require(fcntl_nid_postfix(plain, DuplicateDescriptorCloseOnExec, -1) == -1 && *__error_nid_postfix() == GuestEinval, "F_DUPFD_CLOEXEC with a negative minimum must fail with EINVAL");
    Require(close_nid_postfix(cloexecDup) == 0 && close_nid_postfix(minimumDup) == 0, "close failed");

    const int dup2Target = socket_nid_postfix(2, Stream, 0);
    Require(dup2Target >= 0, "socket failed");
    Require(fcntl_nid_postfix(dup2Target, SetDescriptorFlags, CloseOnExec) == 0 && fcntl_nid_postfix(plain, SetDescriptorFlags, CloseOnExec) == 0, "F_SETFD on a socket must succeed");
    Require(fcntl_nid_postfix(plain, Duplicate2Descriptor, dup2Target) == dup2Target, "F_DUP2FD onto another socket must return the target");
    Require(fcntl_nid_postfix(dup2Target, GetDescriptorFlags) == 0, "F_DUP2FD must clear FD_CLOEXEC on the target");
    Require(fcntl_nid_postfix(plain, GetDescriptorFlags) == CloseOnExec, "F_DUP2FD must leave the flag of the original");
    Require(fcntl_nid_postfix(plain, Duplicate2Descriptor, plain) == plain && fcntl_nid_postfix(plain, GetDescriptorFlags) == CloseOnExec, "F_DUP2FD onto the same descriptor must preserve FD_CLOEXEC");
    Require(fcntl_nid_postfix(plain, SetDescriptorFlags, 0) == 0, "F_SETFD 0 must clear FD_CLOEXEC");
    Require(fcntl_nid_postfix(plain, Duplicate2Descriptor, plain) == plain && fcntl_nid_postfix(plain, GetDescriptorFlags) == 0, "F_DUP2FD onto the same descriptor must preserve a cleared flag");
    Require(fcntl_nid_postfix(plain, Duplicate2DescriptorCloseOnExec, dup2Target) == dup2Target, "F_DUP2FD_CLOEXEC onto another socket must return the target");
    Require(fcntl_nid_postfix(dup2Target, GetDescriptorFlags) == CloseOnExec, "F_DUP2FD_CLOEXEC must set FD_CLOEXEC on the target");
    Require(fcntl_nid_postfix(plain, GetDescriptorFlags) == 0, "F_DUP2FD_CLOEXEC must leave the flag of the original");
    Require(fcntl_nid_postfix(plain, Duplicate2DescriptorCloseOnExec, plain) == plain && fcntl_nid_postfix(plain, GetDescriptorFlags) == CloseOnExec, "F_DUP2FD_CLOEXEC onto the same descriptor must set FD_CLOEXEC");
    Require(fcntl_nid_postfix(plain, SetDescriptorFlags, 0) == 0, "F_SETFD 0 must clear FD_CLOEXEC");
    Require(fcntl_nid_postfix(plain, Duplicate2Descriptor, -1) == -1 && *__error_nid_postfix() == GuestEbadf, "F_DUP2FD with a negative target must fail with EBADF");
    Require(fcntl_nid_postfix(plain, Duplicate2DescriptorCloseOnExec, -1) == -1 && *__error_nid_postfix() == GuestEbadf, "F_DUP2FD_CLOEXEC with a negative target must fail with EBADF");
    Require(close_nid_postfix(dup2Target) == 0, "close failed");

    int pipeDescriptors[2] = {-1, -1};
    Require(pipe_nid_postfix(pipeDescriptors) == 0, "pipe failed");
    Require(fcntl_nid_postfix(pipeDescriptors[0], SetDescriptorFlags, CloseOnExec) == 0, "F_SETFD on a pipe must succeed");
    Require(fcntl_nid_postfix(plain, SetDescriptorFlags, CloseOnExec) == 0, "F_SETFD on a socket must succeed");
    Require(fcntl_nid_postfix(plain, Duplicate2Descriptor, pipeDescriptors[0]) == pipeDescriptors[0], "F_DUP2FD onto a pipe must return the target");
    Require(fcntl_nid_postfix(pipeDescriptors[0], GetDescriptorFlags) == 0, "F_DUP2FD onto a pipe must clear FD_CLOEXEC");
    Require(fcntl_nid_postfix(plain, SetDescriptorFlags, 0) == 0, "F_SETFD 0 must clear FD_CLOEXEC");
    Require(fcntl_nid_postfix(plain, Duplicate2DescriptorCloseOnExec, pipeDescriptors[1]) == pipeDescriptors[1], "F_DUP2FD_CLOEXEC onto a pipe must return the target");
    Require(fcntl_nid_postfix(pipeDescriptors[1], GetDescriptorFlags) == CloseOnExec, "F_DUP2FD_CLOEXEC onto a pipe must set FD_CLOEXEC");
    Require(close_nid_postfix(pipeDescriptors[0]) == 0 && close_nid_postfix(pipeDescriptors[1]) == 0, "close failed");

    const int marked = socket_nid_postfix(2, Stream | SocketCloseOnExec, 0);
    Require(marked >= 0 && fcntl_nid_postfix(marked, GetDescriptorFlags) == CloseOnExec, "SOCK_CLOEXEC must set FD_CLOEXEC");
    Require(close_nid_postfix(marked) == 0, "close failed");
    const int reused = socket_nid_postfix(2, Stream, 0);
    Require(reused >= 0 && fcntl_nid_postfix(reused, GetDescriptorFlags) == 0, "the flag of a closed socket must not reach the next socket");
    Require(close_nid_postfix(reused) == 0 && close_nid_postfix(plain) == 0, "close failed");
    return 0;
}
