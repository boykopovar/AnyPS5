#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_MAINTHREAD_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_MAINTHREAD_HPP

#include <exception>
#include <optional>
#include <type_traits>
#include <utility>

#ifdef __APPLE__
extern "C" void Aps5RunOnMainThread_nid_no_patch(void (*function)(void*), void* context);
#endif

template <class Function>
std::invoke_result_t<Function&> OnMainThread(Function&& function) {
#ifdef __APPLE__
    using Result = std::invoke_result_t<Function&>;
    struct Call {
        std::remove_reference_t<Function>* function;
        std::exception_ptr error;
        std::conditional_t<std::is_void_v<Result>, bool, std::optional<Result>> result {};
    } call {&function, nullptr};
    Aps5RunOnMainThread_nid_no_patch([](void* context) {
        auto& call = *static_cast<Call*>(context);
        try {
            if constexpr (std::is_void_v<Result>) (*call.function)();
            else call.result.emplace((*call.function)());
        } catch (...) {
            call.error = std::current_exception();
        }
    }, &call);
    if (call.error) std::rethrow_exception(call.error);
    if constexpr (!std::is_void_v<Result>) return std::move(*call.result);
#else
    return function();
#endif
}

#endif
