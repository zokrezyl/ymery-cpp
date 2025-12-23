// Audio buffer infrastructure for real-time audio data
#pragma once

#include "../result.hpp"
#include <vector>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <memory>

namespace ymery {

// Forward declarations
class AudioRingBuffer;
class MediatedAudioBuffer;

using AudioRingBufferPtr = std::shared_ptr<AudioRingBuffer>;
using MediatedAudioBufferPtr = std::shared_ptr<MediatedAudioBuffer>;

/**
 * Lock-free audio ring buffer for single producer, multiple consumers
 * Stores float samples in a circular buffer
 */
class AudioRingBuffer {
public:
    static Result<AudioRingBufferPtr> create(
        int sample_rate,
        size_t buffer_size,
        size_t period_size
    );

    // Producer interface
    void write(const float* data, size_t count);
    void write(const std::vector<float>& data);

    // Consumer interface - returns copy of data
    std::vector<float> read_all() const;
    size_t size() const;

    // Properties
    int sample_rate() const { return _sample_rate; }
    size_t buffer_size() const { return _buffer_size; }
    size_t period_size() const { return _period_size; }

    // Thread-safe access
    bool try_lock();
    void unlock();

private:
    AudioRingBuffer() = default;

    int _sample_rate = 48000;
    size_t _buffer_size = 0;
    size_t _period_size = 1024;

    std::vector<float> _buffer;
    std::atomic<size_t> _write_pos{0};
    std::atomic<size_t> _available{0};
    mutable std::mutex _mutex;
    std::atomic<bool> _locked{false};
};

/**
 * Mediated buffer - provides read access to underlying ring buffer
 * Multiple consumers can have their own mediated buffer
 */
class MediatedAudioBuffer {
public:
    static Result<MediatedAudioBufferPtr> create(AudioRingBufferPtr ring_buffer);

    // Consumer interface
    std::vector<float> data() const;
    size_t size() const;

    // Lock management
    bool try_lock();
    void unlock();

    // Properties
    int sample_rate() const;

private:
    MediatedAudioBuffer() = default;
    AudioRingBufferPtr _ring_buffer;
};

} // namespace ymery
