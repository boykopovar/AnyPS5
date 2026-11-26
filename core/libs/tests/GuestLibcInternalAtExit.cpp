#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/RuntimeInternals.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

std::vector<int> threadExitOrder;
std::vector<Pthread> threadExitThreads;

void APS5_VABI ThreadExitFirst() {
    threadExitOrder.push_back(1);
    threadExitThreads.push_back(scePthreadSelf());
}

void APS5_VABI ThreadExitSecond() {
    threadExitOrder.push_back(2);
    threadExitThreads.push_back(scePthreadSelf());
}

void APS5_VABI ThreadExitDynamic() {
    threadExitOrder.push_back(3);
    threadExitThreads.push_back(scePthreadSelf());
    Require(_Atthreadexit_nid_postfix(ThreadExitFirst) == 0);
}

std::vector<int> destructorOrder;
int lateObject = 3;
void* testModule = nullptr;

void APS5_VABI RecordDestructor(void* object) {
    destructorOrder.push_back(*static_cast<int*>(object));
}

void APS5_VABI CallbackAfterDestructors() {
    destructorOrder.push_back(100);
}

void APS5_VABI CallbackRegistersDestructor() {
    destructorOrder.push_back(200);
    Require(_sceLibcInternalThreadAtexit_nid_postfix(RecordDestructor, &lateObject, testModule) == 0);
}

void* APS5_VABI WorkerThreadFunc(void*) {
    Require(_Atthreadexit_nid_postfix(ThreadExitFirst) == 0);
    Require(_Atthreadexit_nid_postfix(ThreadExitSecond) == 0);
    return nullptr;
}

std::vector<int> isolationCalls;

void APS5_VABI IsolationMainCb() {
    isolationCalls.push_back(100);
}

void APS5_VABI IsolationWorkerCb() {
    isolationCalls.push_back(200);
}

void* APS5_VABI IsolationWorkerFunc(void*) {
    Require(_Atthreadexit_nid_postfix(IsolationWorkerCb) == 0);
    return nullptr;
}

int quickOrder = 0;

void APS5_VABI QuickSecond() {
    quickOrder = 1;
}

void APS5_VABI QuickFirst() {
    if (quickOrder != 1) std::_Exit(1);
    std::puts("Guest quick_exit handlers completed");
    std::fflush(stdout);
}

std::filesystem::path OwnExecutable() {
#ifdef _WIN32
    std::wstring executable(32768, L'\0');
    executable.resize(GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size())));
    return executable;
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> executable(size);
    Require(_NSGetExecutablePath(executable.data(), &size) == 0);
    return std::filesystem::canonical(executable.data());
#else
    return std::filesystem::read_symlink("/proc/self/exe");
#endif
}

int RunChild(const std::filesystem::path& self, const char* mode) {
#ifdef _WIN32
    std::wstring commandLine = L"\"" + self.wstring() + L"\" " + std::wstring(mode, mode + std::strlen(mode));
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    Require(CreateProcessW(self.c_str(), commandLine.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup, &process) != 0);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return static_cast<int>(code);
#else
    const pid_t child = fork();
    Require(child >= 0);
    if (child == 0) {
        execl(self.c_str(), self.c_str(), mode, static_cast<char*>(nullptr));
        std::_Exit(127);
    }
    int status = 0;
    Require(waitpid(child, &status, 0) == child);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
#endif
}

} // namespace

int main(int argc, char** argv) {
    const int firstArgument = argc > 0 && std::strncmp(argv[0], "--", 2) == 0 ? 0 : 1;
    if (argc > firstArgument && std::strcmp(argv[firstArgument], "--quick-exit") == 0) {
        Require(_Atqexit_nid_postfix(nullptr) == 0);
        Require(_Atqexit_nid_postfix(QuickFirst) == 0);
        Require(_Atqexit_nid_postfix(QuickSecond) == 0);
        quick_exit_nid_postfix(0);
        return 2;
    }

    Require(_Atqexit_nid_postfix(nullptr) == 0);
    Require(_Atthreadexit_nid_postfix(nullptr) == 0);

    const Pthread mainThread = scePthreadSelf();

    Require(_Atthreadexit_nid_postfix(ThreadExitFirst) == 0);
    Require(_Atthreadexit_nid_postfix(ThreadExitSecond) == 0);
    _sceLibcInternalThreadDtors_nid_postfix();
    Require(threadExitOrder == std::vector<int>{2, 1});
    Require(threadExitThreads == std::vector<Pthread>{mainThread, mainThread});

    _sceLibcInternalThreadDtors_nid_postfix();
    Require(threadExitOrder.size() == 2);

    threadExitOrder.clear();
    threadExitThreads.clear();
    Require(_Atthreadexit_nid_postfix(ThreadExitSecond) == 0);
    Require(_Atthreadexit_nid_postfix(ThreadExitDynamic) == 0);
    _sceLibcInternalThreadDtors_nid_postfix();
    Require(threadExitOrder == std::vector<int>{3, 1, 2});

    threadExitOrder.clear();
    threadExitThreads.clear();
    Pthread worker = nullptr;
    Require(scePthreadCreate(&worker, nullptr, WorkerThreadFunc, nullptr, "WorkerAtExit") == 0);
    Require(scePthreadJoin(worker, nullptr) == 0);
    Require(threadExitOrder == std::vector<int>{2, 1});
    Require(threadExitThreads == std::vector<Pthread>{worker, worker});

    Require(_Atthreadexit_nid_postfix(IsolationMainCb) == 0);
    Pthread isolationWorker = nullptr;
    Require(scePthreadCreate(&isolationWorker, nullptr, IsolationWorkerFunc, nullptr, "IsoWorker") == 0);
    Require(scePthreadJoin(isolationWorker, nullptr) == 0);
    Require(isolationCalls == std::vector<int>{200});
    _sceLibcInternalThreadDtors_nid_postfix();
    Require(isolationCalls == std::vector<int>{200, 100});

#ifdef _WIN32
    testModule = reinterpret_cast<void*>(GetModuleHandleW(nullptr));
#else
    testModule = dlopen(nullptr, RTLD_NOW);
#endif
    Require(testModule != nullptr);

    int firstObject = 1;
    int secondObject = 2;
    Require(_sceLibcInternalThreadAtexit_nid_postfix(RecordDestructor, &firstObject, testModule) == 0);
    Require(_sceLibcInternalThreadAtexit_nid_postfix(RecordDestructor, &secondObject, testModule) == 0);
    _sceLibcInternalThreadDtors_nid_postfix();
    Require(destructorOrder == std::vector<int>{2, 1});

    destructorOrder.clear();
    Require(_Atthreadexit_nid_postfix(CallbackAfterDestructors) == 0);
    Require(_sceLibcInternalThreadAtexit_nid_postfix(RecordDestructor, &firstObject, testModule) == 0);
    Require(_sceLibcInternalThreadAtexit_nid_postfix(RecordDestructor, &secondObject, testModule) == 0);
    _sceLibcInternalThreadDtors_nid_postfix();
    Require(destructorOrder == std::vector<int>{2, 1, 100});

    destructorOrder.clear();
    Require(_Atthreadexit_nid_postfix(CallbackRegistersDestructor) == 0);
    _sceLibcInternalThreadDtors_nid_postfix();
    Require(destructorOrder == std::vector<int>{200, 3});
    _sceLibcInternalThreadDtors_nid_postfix();
    Require(destructorOrder.size() == 2);

    const auto self = OwnExecutable();
    const int childCode = RunChild(self, "--quick-exit");
    Require(childCode == 0);

    return 0;
}
