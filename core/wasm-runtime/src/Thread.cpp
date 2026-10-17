#include "Program.hpp"
#include "Thread.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "wasm-runtime/Guest.hpp"
#include <emscripten.h>
#include <cstring>
#include <new>
#include <pthread.h>

namespace WasmRuntime {
namespace {

constexpr std::size_t DefaultStackSize = 2u << 20;
constexpr std::size_t StackAlignment = 0x10000;

struct ThreadState {
    GuestCpu* cpu = nullptr;
    std::uint64_t stack = 0;
    std::size_t stackSize = 0;
    ThreadTls tls;
};

thread_local ThreadState* t_thread = nullptr;
thread_local std::size_t t_stackSize = DefaultStackSize;

void DispatchPlaceholder(GuestCpu*) {
    GuestFault("the game module is not bound to this thread");
}

using DispatchFunction = void (*)(GuestCpu*);
DispatchFunction volatile g_dispatch = DispatchPlaceholder;

pthread_key_t g_threadKey;
pthread_once_t g_threadKeyOnce = PTHREAD_ONCE_INIT;

void ReleaseThread(void* value) {
    auto* state = static_cast<ThreadState*>(value);
    GuestArena::GuestArenaRelease_nid_postfix(reinterpret_cast<void*>(state->stack), state->stackSize);
    GuestArena::GuestArenaRelease_nid_postfix(reinterpret_cast<void*>(state->tls.block), state->tls.bytes);
    ::operator delete(state->cpu, std::align_val_t{alignof(GuestCpu)});
    delete state;
}

void CreateThreadKey() {
    pthread_key_create(&g_threadKey, ReleaseThread);
}

EM_JS(void, aps5_bind_game, (void* slot, const std::uint64_t* bases, std::uint32_t count, std::uint64_t stubBase, std::uint64_t addrMap), {
    if (!globalThis.aps5GameInstance) {
        const module = globalThis.aps5GameModule;
        if (!module) throw new Error('the game module did not reach this worker');
        const memory = wasmMemory;
        const view = new BigUint64Array(memory.buffer, Number(bases), count);
        const constant = (value) => new WebAssembly.Global({value: 'i64', mutable: false}, BigInt(value));
        const imports = {env: {memory}, nid: {}, aps5: {}};
        for (const entry of WebAssembly.Module.imports(module)) {
            if (entry.module === 'env' && entry.name.startsWith('__aps5_image_base_')) {
                imports.env[entry.name] = constant(view[Number(entry.name.slice(18))]);
            } else if (entry.module === 'env' && entry.name === '__aps5_stub_base') {
                imports.env[entry.name] = constant(stubBase);
            } else if (entry.module === 'env' && entry.name === '__aps5_addr_map') {
                imports.env[entry.name] = constant(addrMap);
            } else if (entry.module === 'nid') {
                const exported = wasmExports['nid.' + entry.name];
                imports.nid[entry.name] = exported || ((ctx) => {
                    const rsp = new BigUint64Array(memory.buffer, Number(ctx) + 32, 1)[0];
                    const caller = new BigUint64Array(memory.buffer, Number(rsp), 1)[0];
                    abort('the runtime does not provide NID ' + entry.name + ' (called from 0x' + caller.toString(16) + ')');
                });
            } else if (entry.module === 'aps5') {
                const exported = wasmExports['aps5.' + entry.name];
                if (!exported) throw new Error('the runtime does not provide aps5.' + entry.name);
                imports.aps5[entry.name] = exported;
            }
        }
        const instance = new WebAssembly.Instance(module, imports);
        if (instance.exports.__aps5_abi_version() !== 1) throw new Error('the game module uses another ABI version');
        globalThis.aps5GameInstance = instance;
    }
    wasmTable.set(slot, globalThis.aps5GameInstance.exports.__aps5_dispatch);
});

ThreadState* CreateThreadState() {
    pthread_once(&g_threadKeyOnce, CreateThreadKey);
    const auto& program = CurrentProgram();
    aps5_bind_game(reinterpret_cast<void*>(DispatchPlaceholder), program.bases.data(), static_cast<std::uint32_t>(program.bases.size()), program.stubBase, program.addrMap);
    auto* state = new ThreadState;
    state->cpu = new (std::align_val_t{alignof(GuestCpu)}) GuestCpu{};
    state->stackSize = (t_stackSize + StackAlignment - 1) & ~(StackAlignment - 1);
    state->stack = reinterpret_cast<std::uintptr_t>(GuestArena::GuestArenaAllocate_nid_postfix(state->stackSize, StackAlignment));
    state->tls = CreateThreadTls(program);
    auto& cpu = *state->cpu;
    cpu.gpr[Rsp] = state->stack + state->stackSize - 64;
    cpu.fsBase = state->tls.threadPointer;
    cpu.mxcsr = InitialMxcsr;
    cpu.x87Fcw = InitialFcw;
    pthread_setspecific(g_threadKey, state);
    return state;
}

}  // namespace

GuestCpu& CurrentCpu() {
    if (!t_thread) t_thread = CreateThreadState();
    return *t_thread->cpu;
}

void SetNextThreadStackSize(std::size_t bytes) {
    if (bytes != 0) t_stackSize = bytes;
}

void Dispatch(GuestCpu& cpu) {
    g_dispatch(&cpu);
}

std::uint64_t ThreadPointer() {
    CurrentCpu();
    return t_thread->tls.threadPointer;
}

}  // namespace WasmRuntime
