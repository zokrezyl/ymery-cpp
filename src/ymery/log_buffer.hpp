#pragma once

#include <deque>
#include <mutex>
#include <string>
#include <chrono>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <imgui.h>

namespace ymery {

// Log entry for spdlog messages
struct LogEntry {
    std::string message;
    std::string logger_name;
    spdlog::level::level_enum level;
    std::string timestamp;
    std::string source_file;
    int source_line;
};

// Thread-local ring buffer for spdlog messages
class LogBuffer {
public:
    explicit LogBuffer(size_t max_size = 1000) : _max_size(max_size) {}

    // Add a log entry
    void add(LogEntry entry) {
        std::lock_guard<std::mutex> lock(_mutex);
        _entries.push_back(std::move(entry));
        while (_entries.size() > _max_size) {
            _entries.pop_front();
        }
    }

    // Get all entries (thread-safe copy)
    [[nodiscard]] std::deque<LogEntry> entries() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _entries;
    }

    // Get entries count
    [[nodiscard]] size_t size() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _entries.size();
    }

    // Clear all entries
    void clear() {
        std::lock_guard<std::mutex> lock(_mutex);
        _entries.clear();
    }

    // Get max size
    [[nodiscard]] size_t max_size() const {
        return _max_size;
    }

    // Set max size
    void set_max_size(size_t max_size) {
        std::lock_guard<std::mutex> lock(_mutex);
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
    mutable std::mutex _mutex;
    std::deque<LogEntry> _entries;
    size_t _max_size;
};

// Global log buffer (shared across threads, mutex-protected)
inline LogBuffer& get_log_buffer() {
    static LogBuffer buffer;
    return buffer;
}

// Custom spdlog sink that writes to LogBuffer
template<typename Mutex>
class LogBufferSink : public spdlog::sinks::base_sink<Mutex> {
public:
    explicit LogBufferSink(LogBuffer& buffer) : _buffer(buffer) {}

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        // Format timestamp
        auto time_t = std::chrono::system_clock::to_time_t(msg.time);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            msg.time.time_since_epoch()) % 1000;

        std::tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &time_t);
#else
        localtime_r(&time_t, &tm_buf);
#endif

        char timestamp[32];
        std::strftime(timestamp, sizeof(timestamp), "%H:%M:%S", &tm_buf);

        // Pad milliseconds
        std::string ms_str = std::to_string(ms.count());
        while (ms_str.size() < 3) ms_str = "0" + ms_str;
        std::string ts = std::string(timestamp) + "." + ms_str;

        LogEntry entry;
        entry.message = std::string(msg.payload.data(), msg.payload.size());
        entry.logger_name = std::string(msg.logger_name.data(), msg.logger_name.size());
        entry.level = msg.level;
        entry.timestamp = ts;
        entry.source_file = msg.source.filename ? msg.source.filename : "";
        entry.source_line = msg.source.line;

        _buffer.add(std::move(entry));
    }

    void flush_() override {}

private:
    LogBuffer& _buffer;
};

using LogBufferSinkMt = LogBufferSink<std::mutex>;
using LogBufferSinkSt = LogBufferSink<spdlog::details::null_mutex>;

// Helper to add log buffer sink to default logger
inline void setup_log_buffer_sink() {
    auto sink = std::make_shared<LogBufferSinkMt>(get_log_buffer());
    spdlog::default_logger()->sinks().push_back(sink);
}

} // namespace ymery
