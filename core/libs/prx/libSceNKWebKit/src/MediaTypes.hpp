#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

namespace WTF {

struct NotNullTag {};

class MediaTime {
public:
    enum {
        Valid = 1 << 0,
        HasBeenRounded = 1 << 1,
        PositiveInfinite = 1 << 2,
        NegativeInfinite = 1 << 3,
        Indefinite = 1 << 4,
        DoubleValue = 1 << 5,
    };

    enum class RoundingFlags {
        HalfAwayFromZero = 0,
        TowardZero,
        AwayFromZero,
        TowardPositiveInfinity,
        TowardNegativeInfinity,
    };

    enum ComparisonFlags {
        LessThan = -1,
        EqualTo = 0,
        GreaterThan = 1,
    };

    static constexpr uint32_t DefaultTimeScale = 10000000;
    static constexpr uint32_t MaximumTimeScale = 1000000000;

    union {
        int64_t m_timeValue;
        double m_timeValueAsDouble;
    };
    uint32_t m_timeScale;
    uint8_t m_timeFlags;
};

static_assert(sizeof(MediaTime) == 16);
static_assert(alignof(MediaTime) == 8);
static_assert(offsetof(MediaTime, m_timeScale) == 8);
static_assert(offsetof(MediaTime, m_timeFlags) == 12);
static_assert(std::is_trivially_destructible_v<MediaTime>);

} // namespace WTF

namespace WebCore {

class PlatformTimeRanges {
public:
    struct Range {
        WTF::MediaTime start;
        WTF::MediaTime end;
    };

    struct VectorRange {
        Range* m_buffer;
        unsigned m_capacity;
        unsigned m_size;
    };

    VectorRange m_ranges;
};

static_assert(sizeof(PlatformTimeRanges::Range) == 32);
static_assert(alignof(PlatformTimeRanges::Range) == 8);
static_assert(offsetof(PlatformTimeRanges::Range, start) == 0);
static_assert(offsetof(PlatformTimeRanges::Range, end) == 16);
static_assert(sizeof(PlatformTimeRanges::VectorRange) == 16);
static_assert(alignof(PlatformTimeRanges::VectorRange) == 8);
static_assert(sizeof(PlatformTimeRanges) == 16);
static_assert(alignof(PlatformTimeRanges) == 8);

class TimeRanges {
public:
    mutable unsigned m_refCount;
    PlatformTimeRanges m_ranges;

    void ref() const {
        ++m_refCount;
    }
    void deref() const;
};

static_assert(sizeof(TimeRanges) == 24);
static_assert(alignof(TimeRanges) == 8);
static_assert(offsetof(TimeRanges, m_refCount) == 0);
static_assert(offsetof(TimeRanges, m_ranges) == 8);

template<typename T>
class Ref {
public:
    T* m_ptr;

    explicit Ref(T& object) : m_ptr(&object) {}
    Ref(const Ref& other) : m_ptr(other.m_ptr) {
        if (m_ptr) m_ptr->ref();
    }
    Ref(Ref&& other) noexcept : m_ptr(std::exchange(other.m_ptr, nullptr)) {}
    ~Ref() {
        if (m_ptr) m_ptr->deref();
    }
    Ref& operator=(const Ref& other) {
        if (other.m_ptr) other.m_ptr->ref();
        T* old = std::exchange(m_ptr, other.m_ptr);
        if (old) old->deref();
        return *this;
    }
    Ref& operator=(Ref&& other) noexcept {
        T* old = std::exchange(m_ptr, std::exchange(other.m_ptr, nullptr));
        if (old) old->deref();
        return *this;
    }
    T* get() const { return m_ptr; }
    T* operator->() const { return m_ptr; }
    T& operator*() const { return *m_ptr; }
};

static_assert(sizeof(Ref<TimeRanges>) == 8);
static_assert(alignof(Ref<TimeRanges>) == 8);
static_assert(!std::is_trivially_destructible_v<Ref<TimeRanges>>);

enum ExceptionCode {
    IndexSizeError = 0,
};

struct Exception {
    int m_code{0};
    NKWebKit::WtfAbi::String m_message;
};

static_assert(sizeof(Exception) == 16);
static_assert(alignof(Exception) == 8);
static_assert(offsetof(Exception, m_code) == 0);
static_assert(offsetof(Exception, m_message) == 8);

template<typename T>
class ExceptionOr;

template<>
class ExceptionOr<double> {
public:
    union {
        double m_val;
        Exception m_err;
    };
    bool m_hasValue;

    ExceptionOr(double val) : m_val(val), m_hasValue(true) {}
    ExceptionOr(ExceptionCode code) : m_err{code, NKWebKit::WtfAbi::String{}}, m_hasValue(false) {}
    ExceptionOr(const ExceptionOr& other) : m_hasValue(other.m_hasValue) {
        if (m_hasValue) {
            m_val = other.m_val;
        } else {
            new (&m_err) Exception(other.m_err);
        }
    }
    ExceptionOr(ExceptionOr&& other) noexcept : m_hasValue(other.m_hasValue) {
        if (m_hasValue) {
            m_val = other.m_val;
        } else {
            new (&m_err) Exception(std::move(other.m_err));
        }
    }
    ~ExceptionOr() {
        if (!m_hasValue) {
            m_err.~Exception();
        }
    }

    bool hasException() const { return !m_hasValue; }
    double releaseReturnValue() { return m_val; }
    double returnValue() const { return m_val; }
};

static_assert(sizeof(ExceptionOr<double>) == 24);
static_assert(alignof(ExceptionOr<double>) == 8);
static_assert(offsetof(ExceptionOr<double>, m_hasValue) == 16);
static_assert(!std::is_trivially_destructible_v<ExceptionOr<double>>);

} // namespace WebCore
