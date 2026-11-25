#include "prx/libSceSystemLogger/include/Api.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>
#include <deque>
#include <memory>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static_assert(sizeof(SystemLoggerLogData) == 0x64);
static_assert(offsetof(SystemLoggerLogData, size) == 0x40);
static_assert(offsetof(SystemLoggerLogData, reserved) == 0x44);

namespace {

constexpr int SCE_SYSTEM_LOGGER_ERROR_NOT_INITIALIZED = static_cast<int>(0x81080004);
constexpr int SCE_SYSTEM_LOGGER_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x81080005);
constexpr int SCE_SYSTEM_LOGGER_ERROR_INVALID_ARGUMENT = static_cast<int>(0x81080002);
constexpr int SCE_SYSTEM_LOGGER_ERROR_SERVICE = static_cast<int>(0x81080001);
constexpr std::size_t MAX_TAG_LENGTH = 15;
constexpr std::uint32_t MAX_ENTRY_COUNT = 0x20;
constexpr std::uint32_t MAX_ENTRY_SIZE = 0x40;
constexpr std::size_t RING_BUFFER_CAPACITY = 1024;

bool isTagCharacter(unsigned char character) {
    return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
}

class LogRingBuffer {
public:
    LogRingBuffer() : m_head(0), m_tail(0), m_count(0) {
        m_buffer.resize(RING_BUFFER_CAPACITY);
    }

    bool push(const SystemLoggerLogData& entry) {
        std::lock_guard lock(m_mutex);
        if (m_count >= RING_BUFFER_CAPACITY) {
            return false; // Buffer full
        }
        m_buffer[m_tail] = entry;
        m_tail = (m_tail + 1) % RING_BUFFER_CAPACITY;
        m_count++;
        return true;
    }

    std::vector<SystemLoggerLogData> pop_all() {
        std::lock_guard lock(m_mutex);
        std::vector<SystemLoggerLogData> entries;
        if (m_count == 0) {
            return entries;
        }
        entries.reserve(m_count);
        for (std::size_t i = 0; i < m_count; ++i) {
            entries.push_back(m_buffer[m_head]);
            m_head = (m_head + 1) % RING_BUFFER_CAPACITY;
        }
        m_count = 0;
        return entries;
    }

    size_t size() const {
        std::lock_guard lock(m_mutex);
        return m_count;
    }

private:
    std::vector<SystemLoggerLogData> m_buffer;
    std::size_t m_head;
    std::size_t m_tail;
    std::size_t m_count;
    mutable std::mutex m_mutex;
};

class LoggerTransport {
public:
    enum class Type {
        STDOUT,
        FILE,
        DISABLED
    };

    LoggerTransport(Type type, const std::string& config = "")
        : m_type(type), m_file(nullptr) {
        if (type == Type::FILE) {
            m_file = std::fopen(config.c_str(), "wb");
            if (!m_file) {
                m_type = Type::STDOUT;
            }
        }
    }

    ~LoggerTransport() {
        if (m_file) {
            std::fclose(m_file);
        }
    }

    void write(const std::string& tag, const SystemLoggerLogData& entry) {
        if (m_type == Type::DISABLED) {
            return;
        }

        std::FILE* output = (m_type == Type::FILE && m_file) ? m_file : stderr;
        std::fprintf(output, "[sceSystemLogger:%s] ", tag.c_str());
        std::fwrite(entry.data, 1, entry.size, output);
        std::fputc('\n', output);
        std::fflush(output);
    }

private:
    Type m_type;
    std::FILE* m_file;
};

class SystemLoggerService {
public:
    SystemLoggerService() : m_initialized(false), m_transport(LoggerTransport::Type::STDOUT) {}

    bool initialize() {
        std::lock_guard lock(m_mutex);
        if (m_initialized) {
            return false;
        }
        m_initialized = true;
        return true;
    }

    bool terminate() {
        std::lock_guard lock(m_mutex);
        if (!m_initialized) {
            return false;
        }
        m_initialized = false;
        return true;
    }

    int submit(const std::string& tag, const std::vector<SystemLoggerLogData>& entries) {
        std::lock_guard lock(m_mutex);
        if (!m_initialized) {
            return SCE_SYSTEM_LOGGER_ERROR_NOT_INITIALIZED;
        }

        // Add entries to ring buffer
        for (const auto& entry : entries) {
            if (!m_ring_buffer.push(entry)) {
                // Buffer full, write directly to transport
                m_transport.write(tag, entry);
            }
        }
        return 0;
    }

    void process_pending() {
        std::lock_guard lock(m_mutex);
        if (!m_initialized) {
            return;
        }

        auto entries = m_ring_buffer.pop_all();
        if (entries.empty()) {
            return;
        }

        // For now, just write to transport
        // In a real implementation, this would send to the system logger service
        for (const auto& entry : entries) {
            m_transport.write("", entry); // Tag is empty here as we lost it
        }
    }

private:
    bool m_initialized;
    std::mutex m_mutex;
    LogRingBuffer m_ring_buffer;
    LoggerTransport m_transport;
};

std::unique_ptr<SystemLoggerService> g_logger_service;

} // namespace

extern "C" {

int APS5_VABI sceSystemLoggerInitialize() {
    std::lock_guard lock(loggerMutex);
    if (loggerInitialized) return SCE_SYSTEM_LOGGER_ERROR_ALREADY_INITIALIZED;
    
    if (!g_logger_service) {
        g_logger_service = std::make_unique<SystemLoggerService>();
    }
    
    if (!g_logger_service->initialize()) {
        return SCE_SYSTEM_LOGGER_ERROR_ALREADY_INITIALIZED;
    }
    
    loggerInitialized = true;
    return 0;
}

int APS5_VABI sceSystemLoggerSetLogData(const char* tag, uint32_t count, const SystemLoggerLogData* data) {
    std::lock_guard lock(loggerMutex);
    if (!loggerInitialized) return SCE_SYSTEM_LOGGER_ERROR_NOT_INITIALIZED;
    if (!tag) return SCE_SYSTEM_LOGGER_ERROR_INVALID_ARGUMENT;
    const int savedErrno = errno;
    std::string localTag;
    const int tagCopyStatus = CopyGuestString_nid_no_patch(tag, MAX_TAG_LENGTH + 1, localTag);
    errno = savedErrno;
    if (tagCopyStatus != 0) {
        return SCE_SYSTEM_LOGGER_ERROR_INVALID_ARGUMENT;
    }
    const std::size_t length = localTag.size();
    if (length > MAX_TAG_LENGTH) return SCE_SYSTEM_LOGGER_ERROR_INVALID_ARGUMENT;
    for (std::size_t index = 0; index < length; ++index) {
        if (!isTagCharacter(static_cast<unsigned char>(localTag[index]))) return SCE_SYSTEM_LOGGER_ERROR_INVALID_ARGUMENT;
    }
    if (count > MAX_ENTRY_COUNT) return SCE_SYSTEM_LOGGER_ERROR_INVALID_ARGUMENT;
    if (count != 0 && !data) return SCE_SYSTEM_LOGGER_ERROR_INVALID_ARGUMENT;
    std::array<SystemLoggerLogData, MAX_ENTRY_COUNT> entries;
    const bool entriesCopied = GuestCopyIn_nid_no_patch(entries.data(), data, static_cast<std::size_t>(count) * sizeof(SystemLoggerLogData));
    errno = savedErrno;
    if (!entriesCopied) {
        return SCE_SYSTEM_LOGGER_ERROR_INVALID_ARGUMENT;
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        if (entries[index].size > MAX_ENTRY_SIZE) return SCE_SYSTEM_LOGGER_ERROR_INVALID_ARGUMENT;
        for (const std::uint8_t byte : entries[index].reserved) {
            if (byte != 0) return SCE_SYSTEM_LOGGER_ERROR_INVALID_ARGUMENT;
        }
    }
    
    // Submit to service
    if (g_logger_service) {
        std::vector<SystemLoggerLogData> service_entries(entries.begin(), entries.begin() + count);
        const int result = g_logger_service->submit(localTag, service_entries);
        if (result != 0) {
            return result;
        }
    }
    
    // Keep existing behavior: write to stderr
    for (std::uint32_t index = 0; index < count; ++index) {
        std::fprintf(stderr, "[sceSystemLogger:%s] ", localTag.c_str());
        std::fwrite(entries[index].data, 1, entries[index].size, stderr);
        std::fputc('\n', stderr);
    }
    
    errno = savedErrno;
    return 0;
}

int APS5_VABI sceSystemLoggerTerminate() {
    std::lock_guard lock(loggerMutex);
    if (!loggerInitialized) return SCE_SYSTEM_LOGGER_ERROR_NOT_INITIALIZED;
    
    if (g_logger_service && !g_logger_service->terminate()) {
        return SCE_SYSTEM_LOGGER_ERROR_NOT_INITIALIZED;
    }
    
    loggerInitialized = false;
    return 0;
}

} // extern "C"
