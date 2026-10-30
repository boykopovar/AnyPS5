#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_DESCRIPTORACCESS_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_DESCRIPTORACCESS_HPP

#include <memory>
#include <mutex>
#include <cstdint>
#include <atomic>

extern "C" {
void FileStreamSetCloseHookForTest_nid_no_patch(void (*hook)(int, int));
bool FileStreamCloseHookActive_nid_no_patch();
void FileStreamCloseEventForTest_nid_no_patch(int descriptor, int event);
void DescriptorAccessInitialize_nid_no_patch(void* access, int descriptor);
void DescriptorAccessLock_nid_no_patch(void* access);
bool DescriptorAccessTryLock_nid_no_patch(void* access);
void DescriptorAccessUnlock_nid_no_patch(void* access);
bool DescriptorAccessReadOrWait_nid_no_patch(void* access);
void DescriptorAccessWake_nid_no_patch(void* access) noexcept;
bool DescriptorVersionUnchanged_nid_no_patch(const void* version);
void DescriptorAccessSnapshot_nid_no_patch(const void* access, void* version);
void DescriptorOperationInitialize_nid_no_patch(void* operation, void* access);
void DescriptorOperationDestroy_nid_no_patch(void* operation);
bool DescriptorOperationInterrupted_nid_no_patch();
void DescriptorWakeInitialize_nid_no_patch(void* registration);
void DescriptorWakeDestroy_nid_no_patch(void* registration);
}

class GuestDescriptorAccess {
    struct State;
public:
    class Version {
    public:
        bool Unchanged() const { return DescriptorVersionUnchanged_nid_no_patch(this); }
    private:
        friend class GuestDescriptorAccess;
        friend bool DescriptorVersionUnchanged_nid_no_patch(const void* version);
        friend void DescriptorAccessSnapshot_nid_no_patch(const void* access, void* version);
        Version() = default;
        std::shared_ptr<State> state;
        std::uint64_t value = 0;
    };
    enum class Mode { Read, Change, Update, Close };
    class Operation {
    public:
        explicit Operation(GuestDescriptorAccess& access) { DescriptorOperationInitialize_nid_no_patch(this, &access); }
        ~Operation() { DescriptorOperationDestroy_nid_no_patch(this); }
        Operation(const Operation&) = delete;
        Operation& operator=(const Operation&) = delete;
        bool Interrupted() const { return interrupted.load(); }
        static bool CurrentInterrupted() { return DescriptorOperationInterrupted_nid_no_patch(); }
        class WakeRegistration {
        public:
            WakeRegistration(void (*interrupt)(void*) noexcept, void* argument)
                : interrupt(interrupt), argument(argument) { DescriptorWakeInitialize_nid_no_patch(this); }
            ~WakeRegistration() { DescriptorWakeDestroy_nid_no_patch(this); }
            WakeRegistration(const WakeRegistration&) = delete;
            WakeRegistration& operator=(const WakeRegistration&) = delete;
        private:
            friend class Operation;
            friend void DescriptorWakeInitialize_nid_no_patch(void* registration);
            friend void DescriptorWakeDestroy_nid_no_patch(void* registration);
            WakeRegistration* previous = nullptr;
            Operation* operation = nullptr;
            void (*interrupt)(void*) noexcept;
            void* argument;
        };
    private:
        friend struct State;
        friend void DescriptorOperationInitialize_nid_no_patch(void* operation, void* access);
        friend void DescriptorOperationDestroy_nid_no_patch(void* operation);
        friend bool DescriptorOperationInterrupted_nid_no_patch();
        friend void DescriptorWakeInitialize_nid_no_patch(void* registration);
        friend void DescriptorWakeDestroy_nid_no_patch(void* registration);
        static thread_local Operation* current;
        std::shared_ptr<State> state;
        Operation* previous = nullptr;
        WakeRegistration* wake = nullptr;
        std::atomic<bool> interrupted{false};
        void Interrupt() noexcept;
    };
    GuestDescriptorAccess(int descriptor, Mode mode)
        : GuestDescriptorAccess(descriptor, mode, std::defer_lock) { Lock(); }
    GuestDescriptorAccess(int descriptor, Mode mode, std::defer_lock_t)
        : mode(mode) { DescriptorAccessInitialize_nid_no_patch(this, descriptor); }
    ~GuestDescriptorAccess() { if (held) Unlock(); }
    GuestDescriptorAccess(const GuestDescriptorAccess&) = delete;
    GuestDescriptorAccess& operator=(const GuestDescriptorAccess&) = delete;
    void Lock() { DescriptorAccessLock_nid_no_patch(this); }
    bool TryLock() { return DescriptorAccessTryLock_nid_no_patch(this); }
    void Unlock() { DescriptorAccessUnlock_nid_no_patch(this); }
    bool ReadOrWait() { return DescriptorAccessReadOrWait_nid_no_patch(this); }
    void Wake() noexcept { DescriptorAccessWake_nid_no_patch(this); }
    Version Snapshot() const {
        Version result;
        DescriptorAccessSnapshot_nid_no_patch(this, &result);
        return result;
    }
    static void LockTogether(GuestDescriptorAccess& first, GuestDescriptorAccess& second) {
        struct Lockable {
            GuestDescriptorAccess& access;
            void lock() { access.Lock(); }
            bool try_lock() { return access.TryLock(); }
            void unlock() { access.Unlock(); }
        } firstLock{first}, secondLock{second};
        std::lock(firstLock, secondLock);
    }

private:
    friend void DescriptorAccessInitialize_nid_no_patch(void* access, int descriptor);
    friend void DescriptorAccessLock_nid_no_patch(void* access);
    friend bool DescriptorAccessTryLock_nid_no_patch(void* access);
    friend void DescriptorAccessUnlock_nid_no_patch(void* access);
    friend bool DescriptorAccessReadOrWait_nid_no_patch(void* access);
    friend void DescriptorAccessWake_nid_no_patch(void* access) noexcept;
    friend void DescriptorAccessSnapshot_nid_no_patch(const void* access, void* version);
    friend void DescriptorOperationInitialize_nid_no_patch(void* operation, void* access);
    static std::shared_ptr<State> Get(int descriptor);
    std::shared_ptr<State> state;
    Mode mode;
    bool held = false;
};

#endif
