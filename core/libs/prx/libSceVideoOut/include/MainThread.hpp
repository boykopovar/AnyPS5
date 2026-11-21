#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_MAINTHREAD_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_MAINTHREAD_HPP

#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

#ifdef __APPLE__
extern "C" void Aps5RunOnMainThread_nid_no_patch(void (*function)(void*), void* context);
#endif

template <class Function, class Runner>
std::invoke_result_t<Function> OnMainThreadUsing(Function&& function, Runner&& runner) {
    using Result = std::invoke_result_t<Function>;
    using Storage = std::conditional_t<
        std::is_void_v<Result>,
        bool,
        std::conditional_t<
            std::is_reference_v<Result>,
            std::remove_reference_t<Result>*,
            std::optional<std::remove_cv_t<Result>>>>;
    struct Call {
        std::remove_reference_t<Function>* function;
        std::exception_ptr error;
        Storage result {};
        bool completed = false;
    } call {std::addressof(function), nullptr};
    std::forward<Runner>(runner)([](void* context) {
        auto& call = *static_cast<Call*>(context);
        try {
            if constexpr (std::is_void_v<Result>) {
                std::forward<Function>(*call.function)();
            } else if constexpr (std::is_reference_v<Result>) {
                Result ref = std::forward<Function>(*call.function)();
                call.result = std::addressof(ref);
            } else {
                call.result.emplace(std::forward<Function>(*call.function)());
            }
            call.completed = true;
        } catch (...) {
            call.error = std::current_exception();
        }
    }, &call);
    if (call.error) std::rethrow_exception(call.error);
    if (!call.completed) throw std::runtime_error("OnMainThread runner did not invoke callback");
    if constexpr (std::is_reference_v<Result>) return static_cast<Result>(*call.result);
    else if constexpr (!std::is_void_v<Result>) return std::move(*call.result);
}

template <class Function>
std::invoke_result_t<Function> OnMainThread(Function&& function) {
#ifdef __APPLE__
    return OnMainThreadUsing(std::forward<Function>(function), Aps5RunOnMainThread_nid_no_patch);
#else
    return std::forward<Function>(function)();
#endif
}

#endif
