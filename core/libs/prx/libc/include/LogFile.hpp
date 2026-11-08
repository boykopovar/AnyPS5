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
bool ReadVariable(const char* name, std::string& value);
bool ReadPathVariable(std::string& value);
[[noreturn]] void FailStartup(const std::string& message);
bool WaitReadable(Handle read, int milliseconds);
long ReadAvailable(Handle read, char* buffer, std::size_t capacity);
long Pending(Handle read);
bool WriteAll(Handle destination, const char* data, std::size_t length);
void Close(Handle handle);
std::uint64_t FileSize(Handle file);
bool RenameFile(const std::string& from, const std::string& to);
bool RemoveFile(const std::string& path);

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
    void SignalWrite(int standard, const char* text, std::size_t length);
    void Note(const char* text, std::size_t length);
    void SetRotation(const std::string& path, std::uint64_t maxBytes, unsigned keep);
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
    bool rotate();
    void failFile();
    void toTerminal(Stream& stream, const char* data, std::size_t length);
    void run(Stream& stream);
    Stream& streamOf(int standard);

    Handle _file;
    std::string _path;
    std::uint64_t _maxBytes = 0;
    std::uint64_t _fileBytes = 0;
    unsigned _keep = 0;
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

bool ParseMaxBytes(const char* text, std::uint64_t& value, std::string& error);
bool ParseKeep(const char* text, unsigned& value, std::string& error);
bool RotationNeeded(std::uint64_t size, std::uint64_t length, std::uint64_t maxBytes);
std::string RotatedName(const std::string& path, unsigned index);

bool Active();
void Flush();
void Release();
void CrashWrite(int standard, const char* text, std::size_t length);
void SignalWrite_nid_no_patch(int standard, const char* text, std::size_t length);

}

#endif
