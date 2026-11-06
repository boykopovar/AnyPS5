#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_LOGFILE_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_LOGFILE_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

namespace log_file {

using Handle = std::intptr_t;
constexpr Handle NoHandle = -1;

constexpr int StandardOutput = 1;
constexpr int StandardError = 2;

struct Pipe {
    Handle read = NoHandle;
    Handle write = NoHandle;
};

bool OpenAppend(const std::string& path, Handle& file, std::string& error);
bool MakePipe(Pipe& pipe, std::string& error);
Handle KeepStandard(int standard);
bool Divert(int standard, Handle write);
void RestoreStandard(int standard, Handle original);
bool IsInteractive(Handle handle);
void PauseMicroseconds(long microseconds);
void WriteStandardDirect(int standard, const char* text, std::size_t length);
bool ReadVariable(std::string& value);
[[noreturn]] void FailStartup(const std::string& message);
bool WaitReadable(Handle read, int milliseconds);
long ReadAvailable(Handle read, char* buffer, std::size_t capacity);
long Pending(Handle read);
bool WriteAll(Handle destination, const char* data, std::size_t length);
void Close(Handle handle);

class Tee {
public:
    static constexpr std::size_t ChunkBytes = 65536;
    static constexpr std::size_t CarryBytes = 1048576;

    Tee(Handle file, Handle terminalOutput, Handle terminalError, Handle sourceOutput, Handle sourceError);
    ~Tee();
    Tee(const Tee&) = delete;
    Tee& operator=(const Tee&) = delete;

    void Start();
    void Stop();
    bool Flush(bool ending);
    void CrashWrite(int standard, const char* text, std::size_t length);
    void Note(const char* text, std::size_t length);
    bool FileFailed() const;

private:
    struct Stream {
        Handle source = NoHandle;
        Handle terminal = NoHandle;
        std::atomic<bool> terminalBroken{false};
        std::unique_ptr<char[]> chunk;
        std::unique_ptr<char[]> carry;
        std::size_t carried = 0;
        std::thread worker;
    };

    bool lock(int milliseconds);
    void unlock();
    void pump(Stream& stream);
    void toFile(const char* data, std::size_t length);
    void flushCarry(Stream& stream);
    void toTerminal(Stream& stream, const char* data, std::size_t length);
    void run(Stream& stream);
    Stream& streamOf(int standard);

    Handle _file;
    Stream _streams[2];
    std::atomic<int> _lock{0};
    std::atomic<bool> _stop{false};
    std::atomic<bool> _fileFailed{false};
    bool _started = false;
};

class Session {
public:
    virtual ~Session() = default;
    virtual Tee& Output() = 0;
    virtual void Release() = 0;
};

class Factory {
public:
    virtual ~Factory() = default;
    virtual std::unique_ptr<Session> Create(const std::string& path, std::string& error) = 0;
};

std::unique_ptr<Session> StartFromValue(const char* value, Factory& factory, std::string& error);
Factory& SystemFactory();

bool Active();
void Flush();
void Release();
void CrashWrite(int standard, const char* text, std::size_t length);

}

#endif
