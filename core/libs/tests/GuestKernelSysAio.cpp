#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <source_location>
#include <string>

namespace {

struct Sigevent {
    int notify;
    int signo;
    std::int64_t value;
    std::int64_t extra[8];
};

struct Aiocb {
    int fildes;
    std::int64_t offset;
    void* buf;
    std::uint64_t nbytes;
    int spare[2];
    void* spare2;
    int lioOpcode;
    int reqprio;
    std::int64_t status;
    std::int64_t error;
    void* kernelinfo;
    Sigevent sigevent;
};

struct Timespec {
    std::int64_t seconds;
    std::int64_t nanoseconds;
};

}

extern "C" {
int* APS5_VABI __error_nid_postfix();
int APS5_VABI open_nid_postfix(const char* path, int flags, int mode);
int APS5_VABI close_nid_postfix(int descriptor);
int APS5_VABI pipe_nid_postfix(int* descriptors);
int APS5_VABI aio_read_nid_postfix(Aiocb* aiocb);
int APS5_VABI aio_write_nid_postfix(Aiocb* aiocb);
int APS5_VABI aio_fsync_nid_postfix(int operation, Aiocb* aiocb);
int APS5_VABI aio_error_nid_postfix(const Aiocb* aiocb);
std::int64_t APS5_VABI aio_return_nid_postfix(Aiocb* aiocb);
int APS5_VABI aio_suspend_nid_postfix(const Aiocb* const* list, int count, const Timespec* timeout);
std::int64_t APS5_VABI aio_waitcomplete_nid_postfix(Aiocb** result, const Timespec* timeout);
int APS5_VABI aio_cancel_nid_postfix(int descriptor, Aiocb* aiocb);
void AioHoldWorkerForTest_nid_no_patch(bool hold);
}

namespace {

constexpr int Ebadf = 9;
constexpr int Efault = 14;
constexpr int Einval = 22;
constexpr int Eagain = 35;
constexpr int Einprogress = 36;
constexpr int Eopnotsupp = 45;
constexpr int Ecanceled = 85;
constexpr int ORdonly = 0;
constexpr int ORdwr = 2;
constexpr int OCreat = 0x200;
constexpr int OSync = 0x80;
constexpr int ODsync = 0x1000000;
constexpr int AioCanceled = 1;
constexpr int AioAllDone = 3;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::exit(1);
}

int Error() {
    return *__error_nid_postfix();
}

Aiocb Request(int descriptor, void* buffer, std::uint64_t bytes, std::int64_t offset = 0) {
    Aiocb request{};
    request.fildes = descriptor;
    request.buf = buffer;
    request.nbytes = bytes;
    request.offset = offset;
    return request;
}

void Wait(Aiocb& request) {
    const Aiocb* list[] = {&request};
    Require(aio_suspend_nid_postfix(list, 1, nullptr) == 0);
}

void BeforeUse() {
    Aiocb request{};
    const Aiocb* list[] = {&request};
    Require(aio_error_nid_postfix(&request) == Einval);
    Require(aio_return_nid_postfix(&request) == -1 && Error() == Einval);
    Require(aio_suspend_nid_postfix(list, 1, nullptr) == -1 && Error() == Eagain);
}

void Validation(int file, int readOnly) {
    char buffer[8] = {};
    Require(aio_read_nid_postfix(nullptr) == -1 && Error() == Efault);

    auto bad = Request(-1, buffer, sizeof(buffer));
    Require(aio_read_nid_postfix(&bad) == -1 && Error() == Ebadf);
    Require(bad.status == -1 && bad.error == Ebadf && aio_error_nid_postfix(&bad) == Ebadf);

    auto readOnlyWrite = Request(readOnly, buffer, sizeof(buffer));
    Require(aio_write_nid_postfix(&readOnlyWrite) == -1 && Error() == Ebadf);

    auto negative = Request(file, buffer, sizeof(buffer), -1);
    Require(aio_read_nid_postfix(&negative) == -1 && Error() == Einval && negative.error == Einval);

    auto huge = Request(file, buffer, std::uint64_t{std::numeric_limits<int>::max()} + 1);
    Require(aio_read_nid_postfix(&huge) == -1 && Error() == Einval && huge.error == 0);

    auto notify = Request(file, buffer, sizeof(buffer));
    notify.sigevent.notify = 2;
    Require(aio_read_nid_postfix(&notify) == -1 && Error() == Einval && notify.error == Einval);
    notify.sigevent = {1, 0, 0, {}};
    Require(aio_read_nid_postfix(&notify) == -1 && Error() == Einval);

    int pipe[2] = {-1, -1};
    Require(pipe_nid_postfix(pipe) == 0);
    auto piped = Request(pipe[0], buffer, sizeof(buffer));
    Require(aio_read_nid_postfix(&piped) == -1 && Error() == Eopnotsupp && piped.error == Eopnotsupp);
    auto pipeSync = Request(pipe[1], nullptr, 0);
    Require(aio_fsync_nid_postfix(OSync, &pipeSync) == -1 && Error() == Einval);
    Require(close_nid_postfix(pipe[0]) == 0 && close_nid_postfix(pipe[1]) == 0);

    auto sync = Request(file, nullptr, 0);
    Require(aio_fsync_nid_postfix(0, &sync) == -1 && Error() == Einval);
    Require(aio_fsync_nid_postfix(ODsync, &sync) == -1 && Error() == Einval);
}

void ReadAndWrite(int file) {
    char text[] = "asynchronous";
    auto write = Request(file, text, 12, 4);
    Require(aio_write_nid_postfix(&write) == 0);
    Wait(write);
    Require(aio_error_nid_postfix(&write) == 0);
    Require(aio_return_nid_postfix(&write) == 12 && write.status == 12 && write.error == 0);
    Require(aio_return_nid_postfix(&write) == -1 && Error() == Einval);
    Require(aio_error_nid_postfix(&write) == Einval);

    char read[16] = {};
    auto request = Request(file, read, sizeof(read), 6);
    Require(aio_read_nid_postfix(&request) == 0);
    Wait(request);
    Require(aio_return_nid_postfix(&request) == 10 && std::memcmp(read, "ynchronous", 10) == 0);

    auto empty = Request(file, read, 0, 0);
    Require(aio_read_nid_postfix(&empty) == 0);
    Wait(empty);
    Require(aio_return_nid_postfix(&empty) == 0);

    const Aiocb* none[] = {&request};
    Require(aio_suspend_nid_postfix(none, 1, nullptr) == 0);
    Require(aio_suspend_nid_postfix(none, 0, nullptr) == 0);
    Require(aio_suspend_nid_postfix(none, -1, nullptr) == -1 && Error() == Einval);
    Require(aio_suspend_nid_postfix(none, 257, nullptr) == -1 && Error() == Einval);
    const Timespec badTime{0, 1000000000};
    Require(aio_suspend_nid_postfix(none, 1, &badTime) == -1 && Error() == Einval);
}

void Cancel(int file) {
    char buffer[4] = {};
    AioHoldWorkerForTest_nid_no_patch(true);
    auto first = Request(file, buffer, sizeof(buffer));
    auto second = Request(file, buffer, sizeof(buffer));
    Require(aio_read_nid_postfix(&first) == 0 && aio_read_nid_postfix(&second) == 0);
    Require(first.error == Einprogress && aio_error_nid_postfix(&first) == Einprogress);
    const Aiocb* list[] = {&first};
    const Timespec brief{0, 5000000};
    Require(aio_suspend_nid_postfix(list, 1, &brief) == -1 && Error() == Eagain);

    Require(aio_cancel_nid_postfix(file, &first) == AioCanceled);
    Require(aio_error_nid_postfix(&first) == Ecanceled && aio_error_nid_postfix(&second) == Einprogress);
    Require(aio_suspend_nid_postfix(list, 1, &brief) == 0);
    Require(aio_return_nid_postfix(&first) == -1 && Error() == Ecanceled);
    Require(first.status == -1 && first.error == Ecanceled);
    Require(aio_return_nid_postfix(&first) == -1 && Error() == Einval);
    Require(aio_cancel_nid_postfix(file, &first) == AioAllDone);

    Require(aio_cancel_nid_postfix(file, nullptr) == AioCanceled);
    Require(aio_return_nid_postfix(&second) == -1 && Error() == Ecanceled);
    Require(aio_cancel_nid_postfix(file, nullptr) == AioAllDone);
    Require(aio_cancel_nid_postfix(-1, nullptr) == -1 && Error() == Ebadf);
    AioHoldWorkerForTest_nid_no_patch(false);
}

void SyncOrder(int file) {
    char text[] = "ordered";
    AioHoldWorkerForTest_nid_no_patch(true);
    auto write = Request(file, text, 7, 0);
    auto sync = Request(file, nullptr, 0);
    Require(aio_write_nid_postfix(&write) == 0 && aio_fsync_nid_postfix(OSync, &sync) == 0);
    Require(aio_error_nid_postfix(&sync) == Einprogress);
    AioHoldWorkerForTest_nid_no_patch(false);

    Aiocb* done = nullptr;
    Require(aio_waitcomplete_nid_postfix(&done, nullptr) == 7 && done == &write);
    Require(aio_waitcomplete_nid_postfix(&done, nullptr) == 0 && done == &sync && sync.status == 0);
    const Timespec zero{0, 0};
    Require(aio_waitcomplete_nid_postfix(&done, &zero) == -1 && Error() == Eagain && done == nullptr);
    const Timespec brief{0, 5000000};
    Require(aio_waitcomplete_nid_postfix(&done, &brief) == -1 && Error() == Eagain);
    const Timespec badTime{0, -1};
    Require(aio_waitcomplete_nid_postfix(&done, &badTime) == -1 && Error() == Einval);
}

}

int main() {
    const auto path = "anyps5-kernel-sys-aio-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin";
    BeforeUse();
    const int file = open_nid_postfix(path.c_str(), ORdwr | OCreat, 0600);
    const int readOnly = open_nid_postfix(path.c_str(), ORdonly, 0);
    Require(file >= 0 && readOnly >= 0);
    Validation(file, readOnly);
    ReadAndWrite(file);
    Cancel(file);
    SyncOrder(file);
    Require(close_nid_postfix(readOnly) == 0 && close_nid_postfix(file) == 0);
    Require(std::filesystem::remove(path));
    std::puts("guest_kernel_sys_aio passed");
    return 0;
}
