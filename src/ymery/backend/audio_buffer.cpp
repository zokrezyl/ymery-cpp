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

// ============== FileAudioBuffer ==============

Result<FileAudioBufferPtr> FileAudioBuffer::create(
    const std::string& file_path,
    std::vector<float> data,
    int sample_rate
) {
    auto buffer = std::shared_ptr<FileAudioBuffer>(new FileAudioBuffer());
    buffer->_file_path = file_path;
    buffer->_buffer = std::move(data);
    buffer->_sample_rate = sample_rate;
    return buffer;
}

// ============== MediatedStaticBuffer ==============

Result<MediatedStaticBufferPtr> MediatedStaticBuffer::create(
    StaticAudioBufferMediator* mediator,
    size_t start,
    size_t length
) {
    if (!mediator) {
        return Err<MediatedStaticBufferPtr>("MediatedStaticBuffer::create: null mediator");
    }

    auto buffer = std::shared_ptr<MediatedStaticBuffer>(new MediatedStaticBuffer());
    buffer->_mediator = mediator;
    buffer->_start = start;
    buffer->_length = length > 0 ? length : mediator->backend()->size();
    return buffer;
}

std::vector<float> MediatedStaticBuffer::data() const {
    if (!_mediator) return {};

    const auto& source = _mediator->data();
    size_t available = source.size();
    size_t start = std::min(_start, available);
    size_t end = std::min(_start + _length, available);

    if (start >= end) return {};

    return std::vector<float>(source.begin() + start, source.begin() + end);
}

size_t MediatedStaticBuffer::size() const {
    if (!_mediator) return 0;
    size_t available = _mediator->backend()->size();
    size_t start = std::min(_start, available);
    size_t end = std::min(_start + _length, available);
    return end > start ? end - start : 0;
}

void MediatedStaticBuffer::set_range(size_t start, size_t length) {
    _start = start;
    _length = length;
}

bool MediatedStaticBuffer::try_lock() {
    if (!_mediator) return false;
    return _mediator->try_lock();
}

void MediatedStaticBuffer::lock() {
    if (_mediator) _mediator->lock();
}

void MediatedStaticBuffer::unlock() {
    if (_mediator) _mediator->unlock();
}

int MediatedStaticBuffer::sample_rate() const {
    if (!_mediator) return 0;
    return _mediator->backend()->sample_rate();
}

// ============== StaticAudioBufferMediator ==============

Result<StaticAudioBufferMediatorPtr> StaticAudioBufferMediator::create(StaticAudioBufferPtr backend) {
    if (!backend) {
        return Err<StaticAudioBufferMediatorPtr>("StaticAudioBufferMediator::create: null backend");
    }

    auto mediator = std::shared_ptr<StaticAudioBufferMediator>(new StaticAudioBufferMediator());
    mediator->_backend = backend;
    return mediator;
}

Result<MediatedStaticBufferPtr> StaticAudioBufferMediator::open(size_t start, size_t length) {
    auto res = MediatedStaticBuffer::create(this, start, length);
    if (!res) {
        return Err<MediatedStaticBufferPtr>("StaticAudioBufferMediator::open failed", res);
    }
    _mediated_buffers.push_back(*res);
    return res;
}

const std::vector<float>& StaticAudioBufferMediator::data() const {
    static const std::vector<float> empty;
    if (!_backend) return empty;
    return _backend->data();
}

bool StaticAudioBufferMediator::try_lock() {
    if (!_backend) return false;
    return _backend->try_lock();
}

void StaticAudioBufferMediator::lock() {
    if (_backend) _backend->lock();
}

void StaticAudioBufferMediator::unlock() {
    if (_backend) _backend->unlock();
}

} // namespace ymery
