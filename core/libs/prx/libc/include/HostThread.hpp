#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_HOSTTHREAD_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_HOSTTHREAD_HPP

#include <functional>
#include <thread>
#include <stop_token>
#include <tuple>
#include <type_traits>
#include <utility>

#if defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
#include <immintrin.h>
#define ANYPS5_HOST_HAS_MXCSR 1
#endif

namespace Libc {

inline void SetHostMxcsr() {
#ifdef ANYPS5_HOST_HAS_MXCSR
    _mm_setcsr(0x1f80u);
#endif
}

template<class Function, class... Arguments>
std::thread StartHostThread(Function&& function, Arguments&&... arguments) {
    return std::thread([task = std::decay_t<Function>(std::forward<Function>(function)), values = std::tuple<std::decay_t<Arguments>...>(std::forward<Arguments>(arguments)...)]() mutable {
        SetHostMxcsr();
        std::apply(std::move(task), std::move(values));
    });
}

template<class Function>
std::jthread StartHostJThread(Function&& function) {
    return std::jthread([task = std::decay_t<Function>(std::forward<Function>(function))](std::stop_token token) mutable {
        SetHostMxcsr();
        std::invoke(task, token);
    });
}

}

#endif
