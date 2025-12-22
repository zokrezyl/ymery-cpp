// Audio buffer implementation
#include "audio_buffer.hpp"
#include <algorithm>
#include <cstring>

namespace ymery {

// ============== AudioRingBuffer ==============

Result<AudioRingBufferPtr> AudioRingBuffer::create(
    int sample_rate,
    size_t buffer_size,
    size_t period_size
) {
    auto buffer = std::shared_ptr<AudioRingBuffer>(new AudioRingBuffer());
    buffer->_sample_rate = sample_rate;
    buffer->_buffer_size = buffer_size;
    buffer->_period_size = period_size;
    buffer->_buffer.resize(buffer_size, 0.0f);
    buffer->_write_pos = 0;
    buffer->_available = 0;
    return buffer;
}

void AudioRingBuffer::write(const float* data, size_t count) {
    if (count == 0 || _buffer_size == 0) return;

    std::lock_guard<std::mutex> lock(_mutex);

    for (size_t i = 0; i < count; ++i) {
        _buffer[_write_pos] = data[i];
        _write_pos = (_write_pos + 1) % _buffer_size;
    }

    // Update available count (capped at buffer size)
    size_t new_available = _available + count;
    if (new_available > _buffer_size) {
        new_available = _buffer_size;
    }
    _available = new_available;
}

void AudioRingBuffer::write(const std::vector<float>& data) {
    write(data.data(), data.size());
}

std::vector<float> AudioRingBuffer::read_all() const {
    std::lock_guard<std::mutex> lock(_mutex);

    size_t avail = _available;
    if (avail == 0) return {};

    std::vector<float> result(avail);

    // Calculate read position (oldest data)
    size_t read_pos = (_write_pos + _buffer_size - avail) % _buffer_size;

    for (size_t i = 0; i < avail; ++i) {
        result[i] = _buffer[(read_pos + i) % _buffer_size];
    }

    return result;
}

size_t AudioRingBuffer::size() const {
    return _available;
}

bool AudioRingBuffer::try_lock() {
    bool expected = false;
    return _locked.compare_exchange_strong(expected, true);
}

void AudioRingBuffer::unlock() {
    _locked = false;
}

// ============== MediatedAudioBuffer ==============

Result<MediatedAudioBufferPtr> MediatedAudioBuffer::create(AudioRingBufferPtr ring_buffer) {
    if (!ring_buffer) {
        return Err<MediatedAudioBufferPtr>("MediatedAudioBuffer::create: null ring buffer");
    }

    auto buffer = std::shared_ptr<MediatedAudioBuffer>(new MediatedAudioBuffer());
    buffer->_ring_buffer = ring_buffer;
    return buffer;
}

std::vector<float> MediatedAudioBuffer::data() const {
    if (!_ring_buffer) return {};
    return _ring_buffer->read_all();
}

size_t MediatedAudioBuffer::size() const {
    if (!_ring_buffer) return 0;
    return _ring_buffer->size();
}

bool MediatedAudioBuffer::try_lock() {
    if (!_ring_buffer) return false;
    return _ring_buffer->try_lock();
}

void MediatedAudioBuffer::unlock() {
    if (_ring_buffer) {
        _ring_buffer->unlock();
    }
}

int MediatedAudioBuffer::sample_rate() const {
    if (!_ring_buffer) return 0;
    return _ring_buffer->sample_rate();
}

} // namespace ymery
