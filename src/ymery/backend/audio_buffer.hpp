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
class StaticAudioBuffer;
class FileAudioBuffer;
class StaticAudioBufferMediator;

using AudioRingBufferPtr = std::shared_ptr<AudioRingBuffer>;
using MediatedAudioBufferPtr = std::shared_ptr<MediatedAudioBuffer>;
using StaticAudioBufferPtr = std::shared_ptr<StaticAudioBuffer>;
using FileAudioBufferPtr = std::shared_ptr<FileAudioBuffer>;
using StaticAudioBufferMediatorPtr = std::shared_ptr<StaticAudioBufferMediator>;

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

/**
 * Static audio buffer base class - for loaded/immutable audio data
 * One buffer = one channel
 */
class StaticAudioBuffer {
public:
    virtual ~StaticAudioBuffer() = default;

    // Data access
    virtual const std::vector<float>& data() const = 0;
    virtual size_t size() const = 0;

    // Properties
    virtual int sample_rate() const = 0;

    // No-op locking for static buffers (immutable after load)
    virtual bool try_lock() { return true; }
    virtual void lock() {}
    virtual void unlock() {}
};

/**
 * File audio buffer - holds loaded audio file data for one channel
 */
class FileAudioBuffer : public StaticAudioBuffer {
public:
    static Result<FileAudioBufferPtr> create(
        const std::string& file_path,
        std::vector<float> data,
        int sample_rate
    );

    const std::vector<float>& data() const override { return _buffer; }
    size_t size() const override { return _buffer.size(); }
    int sample_rate() const override { return _sample_rate; }

    const std::string& file_path() const { return _file_path; }

private:
    FileAudioBuffer() = default;

    std::string _file_path;
    int _sample_rate = 0;
    std::vector<float> _buffer;
};

/**
 * Mediated static buffer - consumer view into static buffer with slicing
 */
class MediatedStaticBuffer {
public:
    static Result<std::shared_ptr<MediatedStaticBuffer>> create(
        StaticAudioBufferMediator* mediator,
        size_t start = 0,
        size_t length = 0
    );

    // Sliced data access
    std::vector<float> data() const;
    size_t size() const;

    // Range control
    void set_range(size_t start, size_t length);
    void set_start(size_t start) { _start = start; }
    void set_length(size_t length) { _length = length; }

    size_t start() const { return _start; }
    size_t length() const { return _length; }

    // Lock delegation
    bool try_lock();
    void lock();
    void unlock();

    // Properties
    int sample_rate() const;

private:
    MediatedStaticBuffer() = default;

    StaticAudioBufferMediator* _mediator = nullptr;
    size_t _start = 0;
    size_t _length = 0;
};

using MediatedStaticBufferPtr = std::shared_ptr<MediatedStaticBuffer>;

/**
 * Mediator for static buffers - manages consumer access
 */
class StaticAudioBufferMediator {
public:
    static Result<StaticAudioBufferMediatorPtr> create(StaticAudioBufferPtr backend);

    // Create mediated buffer for consumers
    Result<MediatedStaticBufferPtr> open(size_t start = 0, size_t length = 0);

    // Backend access
    StaticAudioBufferPtr backend() const { return _backend; }
    const std::vector<float>& data() const;

    // Lock delegation
    bool try_lock();
    void lock();
    void unlock();

private:
    StaticAudioBufferMediator() = default;

    StaticAudioBufferPtr _backend;
    std::vector<MediatedStaticBufferPtr> _mediated_buffers;
};

} // namespace ymery
