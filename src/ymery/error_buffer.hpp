#pragma once

#include "result.hpp"
#include <deque>
#include <mutex>
#include <string>
#include <chrono>
#include <spdlog/spdlog.h>
#include <imgui.h>

namespace ymery {

// Thread-local ring buffer for Result Error accumulation
// Created per-thread where the main loop runs
class ErrorBuffer {
public:
    struct ErrorEntry {
        Error error;
        spdlog::level::level_enum level;
        std::string timestamp;
    };

    explicit ErrorBuffer(size_t max_size = 1000) : _max_size(max_size) {}

    // Add an error to the buffer and dump to spdlog
    void add(Error error, spdlog::level::level_enum level = spdlog::level::err) {
        // Always dump to spdlog
        switch (level) {
            case spdlog::level::trace:
                spdlog::trace("{}", error.to_string());
                break;
            case spdlog::level::debug:
                spdlog::debug("{}", error.to_string());
                break;
            case spdlog::level::info:
                spdlog::info("{}", error.to_string());
                break;
            case spdlog::level::warn:
                spdlog::warn("{}", error.to_string());
                break;
            case spdlog::level::err:
                spdlog::error("{}", error.to_string());
                break;
            case spdlog::level::critical:
                spdlog::critical("{}", error.to_string());
                break;
            default:
                spdlog::error("{}", error.to_string());
                break;
        }

        // Get timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &time_t);
#else
        localtime_r(&time_t, &tm_buf);
#endif

        char timestamp[32];
        std::strftime(timestamp, sizeof(timestamp), "%H:%M:%S", &tm_buf);
        std::string ts = std::string(timestamp) + "." +
            std::to_string(ms.count()).insert(0, 3 - std::to_string(ms.count()).size(), '0');

        // Add to ring buffer
        _entries.push_back({std::move(error), level, ts});

        // Trim if exceeds max size
        while (_entries.size() > _max_size) {
            _entries.pop_front();
        }
    }

    // Add error from a Result
    template<typename T>
    void add_from_result(const Result<T>& result, spdlog::level::level_enum level = spdlog::level::err) {
        if (!result.has_value()) {
            add(result.error(), level);
        }
    }

    // Get all entries (newest last)
    [[nodiscard]] const std::deque<ErrorEntry>& entries() const {
        return _entries;
    }

    // Get entries count
    [[nodiscard]] size_t size() const {
        return _entries.size();
    }

    // Clear all entries
    void clear() {
        _entries.clear();
    }

    // Get max size
    [[nodiscard]] size_t max_size() const {
        return _max_size;
    }

    // Set max size
    void set_max_size(size_t max_size) {
        _max_size = max_size;
        while (_entries.size() > _max_size) {
            _entries.pop_front();
        }
    }

    // Level to string
    static const char* level_to_string(spdlog::level::level_enum level) {
        switch (level) {
            case spdlog::level::trace: return "TRACE";
            case spdlog::level::debug: return "DEBUG";
            case spdlog::level::info: return "INFO";
            case spdlog::level::warn: return "WARN";
            case spdlog::level::err: return "ERROR";
            case spdlog::level::critical: return "CRITICAL";
            default: return "UNKNOWN";
        }
    }

    // Level to ImGui color
    static ImVec4 level_to_color(spdlog::level::level_enum level) {
        switch (level) {
            case spdlog::level::trace: return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
            case spdlog::level::debug: return ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
            case spdlog::level::info: return ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
            case spdlog::level::warn: return ImVec4(1.0f, 0.8f, 0.0f, 1.0f);
            case spdlog::level::err: return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
            case spdlog::level::critical: return ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
            default: return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        }
    }

private:
    std::deque<ErrorEntry> _entries;
    size_t _max_size;
};

// Thread-local error buffer accessor
// Each thread gets its own buffer
inline ErrorBuffer& get_thread_error_buffer() {
    thread_local ErrorBuffer buffer;
    return buffer;
}

// Convenience function to add an error to the buffer
inline void add_error(Error error, spdlog::level::level_enum level = spdlog::level::err) {
    get_thread_error_buffer().add(std::move(error), level);
}

// Convenience function to add error from a Result
template<typename T>
inline void add_error_from_result(const Result<T>& result, spdlog::level::level_enum level = spdlog::level::err) {
    get_thread_error_buffer().add_from_result(result, level);
}

} // namespace ymery
