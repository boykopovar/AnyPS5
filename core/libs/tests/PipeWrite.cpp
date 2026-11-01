#include "prx/libkernel/Socket/include/SocketRuntime.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <string>
#include <sys/resource.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

extern "C" [[noreturn]] void NotImplemented_nid_no_patch(const char* name) {
    throw std::runtime_error(name);
}

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) throw std::runtime_error("check failed at line " + std::to_string(location.line()));
}

struct Descriptor {
    int value = -1;
    explicit Descriptor(int value = -1) : value(value) {}
    ~Descriptor() { if (value >= 0) ::close(value); }
    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;
    void Close() { if (value >= 0) ::close(value); value = -1; }
};

struct Pipe {
    Descriptor reader;
    Descriptor writer;
    Pipe() {
        int descriptors[2];
        Require(::pipe(descriptors) == 0);
        reader.value = descriptors[0];
        writer.value = descriptors[1];
        const int flags = ::fcntl(writer.value, F_GETFL);
        Require(flags >= 0 && ::fcntl(writer.value, F_SETFL, flags | O_NONBLOCK) == 0);
    }

    void Fill() {
        std::array<char, 4096> bytes{};
        std::size_t total = 0;
        for (;;) {
            const auto written = ::write(writer.value, bytes.data(), bytes.size());
            if (written < 0 && errno == EINTR) continue;
            if (written < 0 && errno == EAGAIN) return;
            Require(written > 0);
            total += static_cast<std::size_t>(written);
            Require(total <= 1024 * 1024);
        }
    }

    void Drain(std::size_t count) {
        std::array<char, 512> bytes{};
        Require(count <= bytes.size());
        Require(::read(reader.value, bytes.data(), count) == static_cast<ssize_t>(count));
    }
};

struct State {
    std::int64_t data = -1;
    bool eof = true;
    bool writable = true;
    explicit State(int descriptor) {
        Require(GuestSockets::PipeWriteState(descriptor, &data, &eof, &writable));
    }
};

int DescriptorCount() {
    int count = 0;
    for (int descriptor = 0; descriptor < 1024; ++descriptor)
        if (::fcntl(descriptor, F_GETFD) >= 0) ++count;
    return count;
}

void Run(const std::string& mode) {
    Pipe pipe;
    if (mode == "ready") {
        for (int index = 0; index < 3; ++index) {
            const State state(pipe.writer.value);
            Require(state.writable && !state.eof && state.data >= 512);
        }
    } else if (mode == "full") {
        pipe.Fill();
        const State state(pipe.writer.value);
        Require(!state.writable && !state.eof && state.data == 0);
    } else if (mode == "threshold") {
        pipe.Fill();
        pipe.Drain(511);
        const State below(pipe.writer.value);
        Require(!below.writable && !below.eof);
        pipe.Drain(1);
        const State ready(pipe.writer.value);
        Require(ready.writable && !ready.eof && ready.data == 512);
        pipe.Drain(512);
        const State more(pipe.writer.value);
        Require(more.writable && !more.eof && more.data == 1024);
    } else if (mode == "eof") {
        for (const bool full : {false, true}) {
            Pipe closing;
            if (full) closing.Fill();
            closing.reader.Close();
            const State state(closing.writer.value);
            Require(state.eof && !state.writable && state.data == 0);
        }
    } else if (mode == "duplicate") {
        Descriptor duplicate(::dup(pipe.writer.value));
        Require(duplicate.value >= 0);
        const State original(pipe.writer.value);
        const State copy(duplicate.value);
        Require(original.data == copy.data && original.writable == copy.writable);
        pipe.writer.Close();
        const State retained(duplicate.value);
        Require(retained.writable && !retained.eof);
        pipe.reader.Close();
        const State closed(duplicate.value);
        Require(closed.eof && closed.data == 0);
    } else if (mode == "invalid") {
        std::int64_t data = -1;
        bool eof = false, writable = false;
        Descriptor regular(::open("/dev/null", O_WRONLY));
        Descriptor socket(::socket(AF_INET, SOCK_STREAM, 0));
        Require(regular.value >= 0 && socket.value >= 0);
        for (const int descriptor : {-1, pipe.reader.value, regular.value, socket.value})
            Require(!GuestSockets::PipeWriteState(descriptor, &data, &eof, &writable));
        const int closed = pipe.writer.value;
        pipe.writer.Close();
        Require(!GuestSockets::PipeWriteState(closed, &data, &eof, &writable));
    } else if (mode == "cleanup") {
        const int before = DescriptorCount();
        for (int index = 0; index < 256; ++index) {
            const State state(pipe.writer.value);
            Require(state.writable && !state.eof);
        }
        Require(DescriptorCount() == before);
        pipe.Fill();
        for (int index = 0; index < 256; ++index) {
            const State state(pipe.writer.value);
            Require(!state.writable && !state.eof);
        }
        Require(DescriptorCount() == before);
    } else if (mode == "exhausted") {
        struct Limit {
            rlimit original{};
            Limit() {
                Require(::getrlimit(RLIMIT_NOFILE, &original) == 0);
                const rlimit reduced{64, original.rlim_max};
                Require(::setrlimit(RLIMIT_NOFILE, &reduced) == 0);
            }
            ~Limit() { ::setrlimit(RLIMIT_NOFILE, &original); }
        } limit;
        std::vector<int> descriptors;
        for (;;) {
            const int descriptor = ::dup(pipe.writer.value);
            if (descriptor < 0) break;
            descriptors.push_back(descriptor);
        }
        Require(errno == EMFILE);
        bool failed = false;
        try {
            const State state(pipe.writer.value);
        } catch (const std::runtime_error& error) {
            failed = std::string(error.what()).find("host errno " + std::to_string(EMFILE)) != std::string::npos;
        }
        for (const int descriptor : descriptors) ::close(descriptor);
        Require(failed);
        const State recovered(pipe.writer.value);
        Require(recovered.writable && !recovered.eof);
    } else {
        throw std::runtime_error("unknown mode: " + mode);
    }
}

}

int main(int argc, char** argv) {
    try {
        Require(argc == 2);
        Run(argv[1]);
        std::cout << "pipe write " << argv[1] << " passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
