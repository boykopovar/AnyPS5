// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "prx/libSceLibcInternal/include/LibcInternal.hpp"
#include "prx/libc/include/General.hpp"

#include <array>
#include <atomic>
#include <mutex>
#include <vector>

static constexpr u32 MAX_THREAD_DTORS = 32;

struct ThreadDtorEntry {
    void (*dtor)(void*);
    void* arg;
};

// Per-thread destructor lists using thread-local storage
thread_local std::vector<ThreadDtorEntry> g_threadDtors;
thread_local bool g_dtorsRunning = false;

static std::mutex g_globalMutex;
static std::atomic<u32> g_threadCount{0};

extern "C" {

#pragma GCC visibility push(default)

int sceLibcInternalThreadExitDtorAdd(void (*dtor)(void*), void* arg) {
    if (!dtor) {
        return SCE_ERROR_INVALID_POINTER;
    }
    
    ThreadDtorEntry entry{dtor, arg};
    g_threadDtors.push_back(entry);
    return SCE_OK;
}

// Run all registered destructors for the current thread
static void RunThreadDtors() {
    if (g_dtorsRunning) {
        return; // Prevent reentrancy
    }
    
    g_dtorsRunning = true;
    
    // Run destructors in reverse order (LIFO)
    for (auto it = g_threadDtors.rbegin(); it != g_threadDtors.rend(); ++it) {
        if (it->dtor) {
            it->dtor(it->arg);
        }
    }
    
    g_threadDtors.clear();
    g_dtorsRunning = false;
}

// Explicit call to run destructors (for main/host threads)
void _sceLibcInternalThreadDtors() {
    RunThreadDtors();
}

// Thread wrapper that ensures destructors run on thread exit
typedef struct {
    void* (*start_routine)(void*);
    void* arg;
} ThreadStartInfo;

static void* ThreadWrapper(void* info_ptr) {
    ThreadStartInfo* info = static_cast<ThreadStartInfo*>(info_ptr);
    
    try {
        // Run the actual thread function
        void* result = info->start_routine(info->arg);
        
        // Run destructors before thread exits
        RunThreadDtors();
        
        return result;
    } catch (...) {
        // Still run destructors on exception
        RunThreadDtors();
        throw;
    }
}

int sceLibcInternalThreadSpawn(void** thread_id, void* (*start_routine)(void*), void* arg) {
    if (!thread_id || !start_routine) {
        return SCE_ERROR_INVALID_POINTER;
    }
    
    // Allocate thread start info
    ThreadStartInfo* info = new ThreadStartInfo{start_routine, arg};
    
    // Use SDL's thread creation for cross-platform compatibility
    #ifdef _WIN32
    // Windows: use CreateThread directly
    HANDLE handle = CreateThread(nullptr, 0, 
        (LPTHREAD_START_ROUTINE)ThreadWrapper, info, 0, nullptr);
    if (!handle) {
        delete info;
        return SCE_ERROR_THREAD_FAILED_TO_CREATE;
    }
    *thread_id = handle;
    #else
    // POSIX: use pthread_create
    pthread_t thread;
    int result = pthread_create(&thread, nullptr, ThreadWrapper, info);
    if (result != 0) {
        delete info;
        return SCE_ERROR_THREAD_FAILED_TO_CREATE;
    }
    *thread_id = (void*)thread;
    #endif
    
    g_threadCount.fetch_add(1);
    return SCE_OK;
}

int sceLibcInternalThreadJoin(void* thread_id) {
    if (!thread_id) {
        return SCE_ERROR_INVALID_POINTER;
    }
    
    #ifdef _WIN32
    WaitForSingleObject((HANDLE)thread_id, INFINITE);
    CloseHandle((HANDLE)thread_id);
    #else
    pthread_join((pthread_t)thread_id, nullptr);
    #endif
    
    g_threadCount.fetch_sub(1);
    return SCE_OK;
}

u32 sceLibcInternalGetThreadCount() {
    return g_threadCount.load();
}

#pragma GCC visibility pop

} // extern "C"