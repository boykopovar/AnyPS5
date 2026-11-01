#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <poll.h>
#include <string>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <thread>
#include <ucontext.h>
#include <unistd.h>

namespace {

int failures = 0;
const char* library = nullptr;
volatile sig_atomic_t failStackInstall = 0;
volatile sig_atomic_t failStackQuery = 0;
volatile sig_atomic_t expectNodefer = 0;
volatile sig_atomic_t expectReset = 0;
volatile sig_atomic_t recoverFault = 0;
std::atomic<bool> interruptWrite{false};
std::atomic<bool> interrupted{false};
std::atomic<int> concurrentHandlers{0};
int observationFd = STDERR_FILENO;
std::uintptr_t callerStack = 0;
std::size_t callerStackBytes = 0;

void Text(const char* text) {
    syscall(SYS_write, observationFd, text, std::strlen(text));
}

void Check(bool condition, const char* what) {
    if (condition) return;
    Text(what);
    _exit(81);
}

void Expect(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
}

void Load() {
    if (dlopen(library, RTLD_NOW | RTLD_LOCAL) != nullptr) return;
    Text(dlerror());
    _exit(80);
}

[[gnu::noinline]] void WriteNearNull() {
    auto* volatile target = reinterpret_cast<volatile int*>(std::uintptr_t{0x18});
    *target = 1;
}

[[gnu::noinline]] void Trap() {
    __builtin_trap();
}

[[gnu::noinline]] void DivideByZero() {
    volatile int numerator = 123;
    volatile int denominator = 0;
    volatile int quotient = numerator / denominator;
    static_cast<void>(quotient);
}

[[gnu::noinline]] void RecoverableTrap() {
    asm volatile("ud2");
}

[[gnu::noinline]] int ExhaustStack(unsigned depth) {
    volatile unsigned char bytes[8192];
    bytes[0] = static_cast<unsigned char>(depth);
    bytes[sizeof(bytes) - 1] = static_cast<unsigned char>(depth + 1);
    const int next = depth == 100000 ? 0 : ExhaustStack(depth + 1);
    return next + bytes[0] + bytes[sizeof(bytes) - 1];
}

void SimpleHandler(int signal) {
    sigset_t mask;
    Check(sigprocmask(SIG_SETMASK, nullptr, &mask) == 0 && sigismember(&mask, SIGUSR1) == 1, "saved mask lost\n");
    Check(sigismember(&mask, signal) == (expectNodefer ? 0 : 1), "saved nodefer lost\n");
    if (expectReset) {
        struct sigaction action{};
        Check(sigaction(signal, nullptr, &action) == 0 && action.sa_handler == SIG_DFL, "saved one-shot lost\n");
    }
    if (callerStack != 0) {
        const auto address = reinterpret_cast<std::uintptr_t>(&mask);
        Check(address >= callerStack && address < callerStack + callerStackBytes, "caller stack not used\n");
    }
    Text("simple handler reached\n");
}

void InfoHandler(int signal, siginfo_t* info, void* context) {
    Check(info != nullptr && info->si_signo == signal && context != nullptr, "saved siginfo lost\n");
    SimpleHandler(signal);
    if (recoverFault) static_cast<ucontext_t*>(context)->uc_mcontext.gregs[REG_RIP] += 2;
    Text("siginfo handler reached\n");
}

void ConcurrentHandler(int signal, siginfo_t*, void*) {
    concurrentHandlers.fetch_add(1, std::memory_order_acq_rel);
    const timespec pause{0, 1000000};
    for (int waited = 0; concurrentHandlers.load(std::memory_order_acquire) < 2 && waited < 1000; ++waited) nanosleep(&pause, nullptr);
    Check(concurrentHandlers.load(std::memory_order_acquire) == 2, "second concurrent handler not reached\n");
    Text("both concurrent handlers reached\n");
    struct sigaction action{};
    action.sa_handler = SIG_DFL;
    sigemptyset(&action.sa_mask);
    sigaction(signal, &action, nullptr);
    syscall(SYS_tgkill, getpid(), syscall(SYS_gettid), signal);
}

void InstallHandler(bool info, int flags = 0) {
    struct sigaction action{};
    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, SIGUSR1);
    action.sa_flags = flags | (info ? SA_SIGINFO : 0);
    if (info) action.sa_sigaction = InfoHandler;
    else action.sa_handler = SimpleHandler;
    Check(sigaction(SIGILL, &action, nullptr) == 0, "cannot install fixture handler\n");
}

void Scenario(const std::string& name) {
    if (name == "simple" || name == "siginfo" || name == "nodefer" || name == "oneshot" || name == "recover" || name == "caller-stack") {
        expectNodefer = name == "nodefer";
        expectReset = name == "oneshot";
        recoverFault = name == "recover";
        if (name == "caller-stack") {
            callerStackBytes = 128 * 1024;
            void* stack = mmap(nullptr, callerStackBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            Check(stack != MAP_FAILED, "cannot map caller stack\n");
            callerStack = reinterpret_cast<std::uintptr_t>(stack);
            stack_t alternate{stack, 0, callerStackBytes};
            Check(sigaltstack(&alternate, nullptr) == 0, "cannot install caller stack\n");
        }
        InstallHandler(name != "simple" && name != "oneshot", (expectNodefer ? SA_NODEFER : 0) | (expectReset ? SA_RESETHAND : 0) | SA_RESTART | (callerStack ? SA_ONSTACK : 0));
        Load();
        struct sigaction installed{};
        Check(sigaction(SIGILL, nullptr, &installed) == 0 && (installed.sa_flags & SA_RESTART) != 0, "saved restart flag lost\n");
        if (callerStack != 0) {
            stack_t alternate{};
            Check(sigaltstack(nullptr, &alternate) == 0 && reinterpret_cast<std::uintptr_t>(alternate.ss_sp) == callerStack && alternate.ss_size == callerStackBytes, "caller stack replaced\n");
            Text("caller stack preserved\n");
        }
        if (recoverFault) {
            RecoverableTrap();
            RecoverableTrap();
        } else {
            raise(SIGILL);
            raise(SIGILL);
        }
        Text("handler recovery completed\n");
        return;
    }
    if (name == "stack-failure" || name == "stack-query-failure") {
        InstallHandler(false);
        failStackInstall = name == "stack-failure";
        failStackQuery = name == "stack-query-failure";
        Load();
        struct sigaction action{};
        Check(sigaction(SIGILL, nullptr, &action) == 0 && action.sa_handler == SimpleHandler, "failed setup changed saved handler\n");
        raise(SIGILL);
        return;
    }
    if (name == "ignored") {
        struct sigaction action{};
        action.sa_handler = SIG_IGN;
        sigemptyset(&action.sa_mask);
        Check(sigaction(SIGILL, &action, nullptr) == 0, "cannot ignore fixture signal\n");
        Load();
        Check(sigaction(SIGILL, nullptr, &action) == 0 && action.sa_handler == SIG_IGN, "ignored disposition replaced\n");
        raise(SIGILL);
        return;
    }
    if (name == "concurrent") {
        struct sigaction action{};
        action.sa_sigaction = ConcurrentHandler;
        action.sa_flags = SA_SIGINFO;
        sigemptyset(&action.sa_mask);
        Check(sigaction(SIGILL, &action, nullptr) == 0, "cannot install concurrent handler\n");
    }
    Load();
    if (name == "near-null" || name == "logged") WriteNearNull();
    if (name == "trap") Trap();
    if (name == "divide") DivideByZero();
    if (name == "sent") kill(getpid(), SIGSEGV);
    if (name == "overflow") {
        rlimit limit{256 * 1024, 256 * 1024};
        Check(setrlimit(RLIMIT_STACK, &limit) == 0, "cannot bound fixture stack\n");
        volatile int result = ExhaustStack(0);
        static_cast<void>(result);
    }
    if (name == "bus") {
        char path[] = "/tmp/anyps5-bus-XXXXXX";
        const int file = mkstemp(path);
        Check(file >= 0, "cannot create bus fixture\n");
        unlink(path);
        void* mapping = mmap(nullptr, 4096, PROT_READ, MAP_PRIVATE, file, 0);
        close(file);
        Check(mapping != MAP_FAILED, "cannot map bus fixture\n");
        volatile char value = *static_cast<volatile char*>(mapping);
        static_cast<void>(value);
    }
    if (name == "mappings") {
        const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        void* mapping = mmap(nullptr, page * 8400, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Check(mapping != MAP_FAILED, "cannot reserve mapping fixture\n");
        for (std::size_t index = 0; index < 8400; index += 2)
            Check(mprotect(static_cast<char*>(mapping) + index * page, page, PROT_READ) == 0, "cannot split mapping fixture\n");
        Trap();
    }
    if (name == "interrupted") {
        interruptWrite.store(true, std::memory_order_release);
        constexpr char marker[] = "interrupt-tee\n";
        syscall(SYS_write, STDOUT_FILENO, marker, sizeof(marker) - 1);
        for (;;) pause();
    }
    if (name == "concurrent") {
        std::atomic<int> ready{0};
        std::atomic<bool> go{false};
        const auto fault = [&] {
            ready.fetch_add(1, std::memory_order_release);
            while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
            syscall(SYS_tgkill, getpid(), syscall(SYS_gettid), SIGILL);
        };
        std::thread first(fault), second(fault);
        while (ready.load(std::memory_order_acquire) != 2) std::this_thread::yield();
        go.store(true, std::memory_order_release);
        first.join();
        second.join();
    }
}

struct Outcome {
    std::string output;
    std::string logged;
    int signal = 0;
    int exit = -1;
    bool timedOut = false;
};

Outcome RunChild(const std::string& name, const std::filesystem::path& log) {
    int pipeEnds[2];
    if (pipe(pipeEnds) != 0) std::exit(2);
    const pid_t child = fork();
    if (child < 0) std::exit(2);
    if (child == 0) {
        dup2(pipeEnds[1], STDERR_FILENO);
        close(pipeEnds[0]);
        close(pipeEnds[1]);
        observationFd = dup(STDERR_FILENO);
        if (log.empty()) unsetenv("ANYPS5_LOG_FILE");
        else setenv("ANYPS5_LOG_FILE", log.c_str(), 1);
        Scenario(name);
        _exit(0);
    }
    close(pipeEnds[1]);
    fcntl(pipeEnds[0], F_SETFL, fcntl(pipeEnds[0], F_GETFL) | O_NONBLOCK);
    Outcome outcome;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    int status = 0;
    for (;;) {
        char buffer[4096];
        for (ssize_t got; (got = read(pipeEnds[0], buffer, sizeof(buffer))) > 0;) outcome.output.append(buffer, static_cast<std::size_t>(got));
        if (waitpid(child, &status, WNOHANG) == child) {
            for (ssize_t got; (got = read(pipeEnds[0], buffer, sizeof(buffer))) > 0;) outcome.output.append(buffer, static_cast<std::size_t>(got));
            break;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            outcome.timedOut = true;
            kill(child, SIGKILL);
            waitpid(child, &status, 0);
            break;
        }
        pollfd pending{pipeEnds[0], POLLIN, 0};
        poll(&pending, 1, 5);
    }
    close(pipeEnds[0]);
    outcome.signal = WIFSIGNALED(status) ? WTERMSIG(status) : 0;
    outcome.exit = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    if (!log.empty()) {
        std::ifstream input(log);
        outcome.logged.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }
    return outcome;
}

bool Has(const Outcome& outcome, const char* text) {
    return outcome.output.find(text) != std::string::npos;
}

std::size_t Count(const std::string& text, const std::string& needle) {
    std::size_t found = 0, at = 0;
    while ((at = text.find(needle, at)) != std::string::npos) { ++found; at += needle.size(); }
    return found;
}

}

extern "C" int sigaltstack(const stack_t* stack, stack_t* previous) noexcept {
    if ((stack != nullptr && failStackInstall) || (stack == nullptr && failStackQuery)) {
        errno = ENOMEM;
        return -1;
    }
    return static_cast<int>(syscall(SYS_sigaltstack, stack, previous));
}

extern "C" ssize_t write(int descriptor, const void* data, std::size_t bytes) {
    constexpr char marker[] = "interrupt-tee\n";
    if (interruptWrite.load(std::memory_order_acquire) && bytes == sizeof(marker) - 1 && std::memcmp(data, marker, bytes) == 0 && !interrupted.exchange(true))
        syscall(SYS_tgkill, getpid(), syscall(SYS_gettid), SIGILL);
    return static_cast<ssize_t>(syscall(SYS_write, descriptor, data, bytes));
}

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    library = argv[1];
    const std::string group = argv[2];
    rlimit cores{0, 0};
    setrlimit(RLIMIT_CORE, &cores);
    char path[] = "/tmp/anyps5-crash-tests-XXXXXX";
    const char* directory = mkdtemp(path);
    if (directory == nullptr) return 2;
    const std::filesystem::path root(directory);
    const auto run = [&](const std::string& name, int signal, bool logged = false) {
        const auto result = RunChild(name, logged ? root / (name + ".log") : std::filesystem::path{});
        Expect(!result.timedOut, name + ": child did not terminate within 3 seconds");
        Expect(result.signal == signal && (signal != 0 || result.exit == 0), name + ": wrong terminating signal or exit: " + std::to_string(result.signal) + "/" + std::to_string(result.exit) + " " + result.output);
        return result;
    };
    if (group == "basic") {
        const std::string program = std::filesystem::path(argv[0]).filename().string() + "+0x";
        const auto fault = run("near-null", SIGSEGV);
        Expect(Has(fault, "[libc] fatal SIGSEGV (code 1) at address 0x0000000000000018 in thread "), "fault address and signal missing");
        Expect(Has(fault, "  rip 0x") && Has(fault, program.c_str()), "fault executable missing");
        Expect(Has(fault, "  rax 0x") && Has(fault, " r15 0x"), "fault registers missing");
        Expect(Has(fault, "  frame chain from rbp:") && Has(fault, "  stack words that point at code, from rsp:"), "fault stack missing");
        Expect(Has(run("trap", SIGILL), "[libc] fatal SIGILL"), "trap report missing");
        Expect(Has(run("divide", SIGFPE), "[libc] fatal SIGFPE"), "divide report missing");
        Expect(Has(run("bus", SIGBUS), "[libc] fatal SIGBUS"), "bus report missing");
        Expect(Has(run("sent", SIGSEGV), "[libc] fatal SIGSEGV (code 0)"), "sent signal report missing");
        Expect(Has(run("overflow", SIGSEGV), "[libc] fatal SIGSEGV"), "alternate-stack overflow report missing");
        Expect(Has(run("mappings", SIGILL), "mappings truncated after 4096 entries"), "mapping truncation missing");
    } else if (group == "handlers") {
        for (const char* name : {"simple", "siginfo", "nodefer", "recover", "caller-stack"}) {
            const auto result = run(name, 0);
            Expect(Has(result, "handler recovery completed"), std::string(name) + ": recovery did not finish");
            Expect(Count(result.output, "[libc] fatal SIGILL") == 2, std::string(name) + ": both recovered faults were not reported");
            if (std::strcmp(name, "caller-stack") == 0) Expect(Has(result, "caller stack preserved"), "caller stack not preserved");
        }
        const auto once = run("oneshot", SIGILL);
        Expect(Count(once.output, "simple handler reached") == 1, "one-shot handler ran more than once");
        Expect(Count(once.output, "[libc] fatal SIGILL") == 1, "one-shot reporter did not reset");
        for (const char* name : {"stack-failure", "stack-query-failure"}) {
            const auto result = run(name, 0);
            Expect(Has(result, "crash report: cannot ") && Has(result, "simple handler reached"), std::string(name) + ": failure not handled");
        }
        const auto ignored = run("ignored", 0);
        Expect(!Has(ignored, "[libc] fatal"), "ignored signal was intercepted");
    } else if (group == "logging") {
        for (const auto name : {"logged", "concurrent", "interrupted"}) {
            const auto result = run(name, std::strcmp(name, "logged") == 0 ? SIGSEGV : SIGILL, true);
            Expect(Has(result, "[libc] fatal") && Has(result, "  stack words that point at code, from rsp:"), std::string(name) + ": stderr report incomplete");
            Expect(result.logged.find("[libc] fatal") != std::string::npos && result.logged.find("  stack words that point at code, from rsp:") != std::string::npos, std::string(name) + ": file report incomplete");
            if (std::strcmp(name, "concurrent") == 0) Expect(Has(result, "both concurrent handlers reached"), "concurrent callbacks not both reached");
        }
    } else return 2;
    std::filesystem::remove_all(root);
    if (failures != 0) return 1;
    std::printf("linux crash report %s tests passed\n", group.c_str());
    return 0;
}
