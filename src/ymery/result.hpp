#pragma once

#if defined(YMERY_ANDROID) || defined(YMERY_WEB)
// Use tl::expected on Android and Emscripten due to libc++ issues with std::expected
#include <tl/expected.hpp>
#else
#include <expected>
#endif
#include <string>
#include <memory>

// source_location support - Emscripten's libc++ may lack it
#if defined(YMERY_WEB) || defined(YMERY_ANDROID)
// Minimal source_location stub for older libc++
namespace std {
struct source_location {
    static constexpr source_location current() noexcept { return {}; }
    constexpr const char* file_name() const noexcept { return ""; }
    constexpr uint_least32_t line() const noexcept { return 0; }
    constexpr uint_least32_t column() const noexcept { return 0; }
    constexpr const char* function_name() const noexcept { return ""; }
};
}
#else
#include <source_location>
#endif

namespace ymery {

// Error with chaining and source location
class Error {
public:
    explicit Error(std::string msg, std::source_location loc = std::source_location::current())
        : _msg(std::move(msg)), _loc(loc) {}

    Error(std::string msg, Error prev, std::source_location loc = std::source_location::current())
        : _msg(std::move(msg)), _prev(std::make_unique<Error>(std::move(prev))), _loc(loc) {}

    Error(const Error& other)
        : _msg(other._msg), _loc(other._loc) {
        if (other._prev) _prev = std::make_unique<Error>(*other._prev);
    }

    Error& operator=(const Error& other) {
        if (this != &other) {
            _msg = other._msg;
            _loc = other._loc;
            _prev = other._prev ? std::make_unique<Error>(*other._prev) : nullptr;
        }
        return *this;
    }

    Error(Error&&) = default;
    Error& operator=(Error&&) = default;

    [[nodiscard]] std::string to_string() const {
        std::string result = _msg;
        result += " [";
        result += _loc.file_name();
        result += ":";
        result += std::to_string(_loc.line());
        result += "]";
        if (_prev) {
            result += " <- ";
            result += _prev->to_string();
        }
        return result;
    }

private:
    std::string _msg;
    std::unique_ptr<Error> _prev;
    std::source_location _loc;
};

#if defined(YMERY_ANDROID) || defined(YMERY_WEB)
// Use tl::expected on Android and Emscripten
template<typename T>
using Result = tl::expected<T, Error>;

// Helper macros/functions for creating results
template<typename T>
[[nodiscard]] inline Result<T> Ok(T value) {
    return Result<T>(std::move(value));
}

[[nodiscard]] inline Result<void> Ok() {
    return Result<void>();
}

template<typename T = void>
[[nodiscard]] inline tl::unexpected<Error> Err(std::string msg, std::source_location loc = std::source_location::current()) {
    return tl::make_unexpected(Error(std::move(msg), loc));
}

template<typename T, typename U>
[[nodiscard]] inline tl::unexpected<Error> Err(std::string msg, const Result<U>& prev, std::source_location loc = std::source_location::current()) {
    if (!prev.has_value()) {
        return tl::make_unexpected(Error(std::move(msg), prev.error(), loc));
    }
    return tl::make_unexpected(Error(std::move(msg), loc));
}
#else
// Use std::expected on other platforms
template<typename T>
using Result = std::expected<T, Error>;

// Helper macros/functions for creating results
template<typename T>
[[nodiscard]] inline Result<T> Ok(T value) {
    return Result<T>(std::move(value));
}

[[nodiscard]] inline Result<void> Ok() {
    return Result<void>();
}

template<typename T = void>
[[nodiscard]] inline std::unexpected<Error> Err(std::string msg, std::source_location loc = std::source_location::current()) {
    return std::unexpected(Error(std::move(msg), loc));
}

template<typename T, typename U>
[[nodiscard]] inline std::unexpected<Error> Err(std::string msg, const Result<U>& prev, std::source_location loc = std::source_location::current()) {
    if (!prev.has_value()) {
        return std::unexpected(Error(std::move(msg), prev.error(), loc));
    }
    return std::unexpected(Error(std::move(msg), loc));
}
#endif

// Get error message from result
template<typename T>
[[nodiscard]] inline std::string error_msg(const Result<T>& res) {
    return res.has_value() ? "" : res.error().to_string();
}

} // namespace ymery
