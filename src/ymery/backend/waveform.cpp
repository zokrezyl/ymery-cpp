// waveform - generates standard waveforms (sine, square, triangle)
#include "../types.hpp"
#include "../result.hpp"
#include "audio_buffer.hpp"
#include <map>
#include <thread>
#include <atomic>
#define _USE_MATH_DEFINES
#include <cmath>
#include <chrono>
#include <ytrace/ytrace.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ymery {

static const std::vector<std::string> WAVEFORM_TYPES = {"sine", "square", "triangle"};

class WaveformDevice {
public:
    static Result<std::shared_ptr<WaveformDevice>> create(
        const std::string& waveform_type,
        int sample_rate = 48000,
        float frequency = 440.0f,
        size_t period_size = 1024,
        size_t buffer_size = 48000
    ) {
        auto device = std::make_shared<WaveformDevice>();
        device->_waveform_type = waveform_type;
        device->_sample_rate = sample_rate;
        device->_frequency = frequency;
        device->_period_size = period_size;

        auto buffer_res = AudioRingBuffer::create(sample_rate, buffer_size, period_size);
        if (!buffer_res) {
            return Err<std::shared_ptr<WaveformDevice>>("WaveformDevice: failed to create ring buffer", buffer_res);
        }
        device->_ring_buffer = *buffer_res;

        auto mediated_res = MediatedAudioBuffer::create(device->_ring_buffer);
        if (!mediated_res) {
            return Err<std::shared_ptr<WaveformDevice>>("WaveformDevice: failed to create mediated buffer", mediated_res);
        }
        device->_mediated_buffer = *mediated_res;

        device->_sample_buffer.resize(period_size);

        return device;
    }

    void start() {
        if (_running) return;
        _running = true;
        _thread = std::thread(&WaveformDevice::_run, this);
    }

    void stop() {
        _running = false;
        if (_thread.joinable()) {
            _thread.join();
        }
    }

    bool is_running() const { return _running; }
    MediatedAudioBufferPtr get_buffer() const { return _mediated_buffer; }
    const std::string& waveform_type() const { return _waveform_type; }
    float frequency() const { return _frequency; }
    int sample_rate() const { return _sample_rate; }

    ~WaveformDevice() {
        stop();
    }

private:
    void _run() {
        while (_running) {
            _generate_waveform();
            _ring_buffer->write(_sample_buffer);

            auto sleep_us = static_cast<int64_t>(_period_size * 1000000.0 / _sample_rate);
            std::this_thread::sleep_for(std::chrono::microseconds(sleep_us));
        }
    }

    void _generate_waveform() {
        float phase_increment = 2.0f * M_PI * _frequency / _sample_rate;

        for (size_t i = 0; i < _period_size; ++i) {
            float phase = _phase + i * phase_increment;

            if (_waveform_type == "sine") {
                _sample_buffer[i] = std::sin(phase);
            } else if (_waveform_type == "square") {
                _sample_buffer[i] = std::sin(phase) >= 0 ? 1.0f : -1.0f;
            } else if (_waveform_type == "triangle") {
                float t = std::fmod(phase / (2.0f * M_PI), 1.0f);
                if (t < 0) t += 1.0f;
                _sample_buffer[i] = 2.0f * std::abs(2.0f * t - 1.0f) - 1.0f;
            }
        }

        _phase = std::fmod(_phase + _period_size * phase_increment, 2.0f * M_PI);
    }

    std::string _waveform_type;
    int _sample_rate = 48000;
    float _frequency = 440.0f;
    size_t _period_size = 1024;
    float _phase = 0.0f;

    AudioRingBufferPtr _ring_buffer;
    MediatedAudioBufferPtr _mediated_buffer;
    std::vector<float> _sample_buffer;

    std::atomic<bool> _running{false};
    std::thread _thread;
};

using WaveformDevicePtr = std::shared_ptr<WaveformDevice>;

class WaveformManager : public TreeLike {
public:
    static Result<TreeLikePtr> create() {
        auto manager = std::make_shared<WaveformManager>();
        if (auto res = manager->init(); !res) {
            return Err<TreeLikePtr>("WaveformManager::create failed", res);
        }

        // Auto-start all waveforms for demo
        for (const auto& type : WAVEFORM_TYPES) {
            auto device_res = WaveformDevice::create(type, 48000, 440.0f);
            if (device_res) {
                manager->_devices[type] = *device_res;
                manager->_devices[type]->start();
            }
        }

        return manager;
    }

    Result<std::vector<std::string>> get_children_names(const DataPath& path) override {
        std::string path_str = path.to_string();
        if (!path_str.empty() && path_str[0] != '/') {
            path_str = "/" + path_str;
        }

        if (path_str == "/" || path_str.empty()) {
            return Ok(std::vector<std::string>{"available", "opened"});
        }

        if (path_str == "/available") {
            return Ok(WAVEFORM_TYPES);
        }

        for (const auto& type : WAVEFORM_TYPES) {
            if (path_str == "/available/" + type) {
                return Ok(std::vector<std::string>{"0"});
            }
        }

        if (path_str == "/opened") {
            std::vector<std::string> children;
            for (const auto& [type, _] : _devices) {
                children.push_back(type);
            }
            return Ok(children);
        }

        for (const auto& type : WAVEFORM_TYPES) {
            if (path_str == "/opened/" + type) {
                if (_devices.count(type)) {
                    return Ok(std::vector<std::string>{"0"});
                }
            }
        }

        return Ok(std::vector<std::string>{});
    }

    Result<Dict> get_metadata(const DataPath& path) override {
        std::string path_str = path.to_string();
        if (!path_str.empty() && path_str[0] != '/') {
            path_str = "/" + path_str;
        }

        if (path_str == "/" || path_str.empty()) {
            return Ok(Dict{
                {"name", Value("waveform")},
                {"label", Value("Waveform Generator")},
                {"type", Value("waveform-manager")},
                {"category", Value("audio-device-manager")}
            });
        }

        if (path_str == "/available") {
            return Ok(Dict{
                {"name", Value("available")},
                {"label", Value("Available")},
                {"type", Value("folder")},
                {"category", Value("folder")}
            });
        }

        if (path_str == "/opened") {
            return Ok(Dict{
                {"name", Value("opened")},
                {"label", Value("Opened")},
                {"type", Value("folder")},
                {"category", Value("folder")}
            });
        }

        for (const auto& type : WAVEFORM_TYPES) {
            if (path_str == "/available/" + type) {
                return Ok(Dict{
                    {"name", Value(type)},
                    {"label", Value(type + " Wave")},
                    {"type", Value("waveform-device")},
                    {"category", Value("audio-device")},
                    {"capabilities", Value(Dict{
                        {"openable", Value(true)},
                        {"readable", Value(true)},
                        {"writable", Value(false)}
                    })}
                });
            }
        }

        for (const auto& type : WAVEFORM_TYPES) {
            if (path_str == "/available/" + type + "/0") {
                return Ok(Dict{
                    {"name", Value("0")},
                    {"label", Value("Channel 0")},
                    {"type", Value("audio-channel")},
                    {"category", Value("audio-channel")},
                    {"capabilities", Value(Dict{
                        {"openable", Value(true)},
                        {"readable", Value(true)},
                        {"writable", Value(false)}
                    })}
                });
            }
        }

        for (const auto& type : WAVEFORM_TYPES) {
            if (path_str == "/opened/" + type) {
                auto it = _devices.find(type);
                if (it != _devices.end()) {
                    auto& device = it->second;
                    return Ok(Dict{
                        {"name", Value(type)},
                        {"label", Value(type + " (" + std::to_string(static_cast<int>(device->frequency())) + "Hz)")},
                        {"type", Value("waveform-device")},
                        {"category", Value("audio-device")},
                        {"status", Value(device->is_running() ? "running" : "stopped")}
                    });
                }
            }
        }

        for (const auto& type : WAVEFORM_TYPES) {
            if (path_str == "/opened/" + type + "/0") {
                auto it = _devices.find(type);
                if (it != _devices.end()) {
                    auto& device = it->second;
                    return Ok(Dict{
                        {"name", Value("0")},
                        {"label", Value("Channel 0")},
                        {"type", Value("audio-channel")},
                        {"category", Value("audio-channel")},
                        {"buffer", Value(device->get_buffer())}
                    });
                }
            }
        }

        return Ok(Dict{});
    }

    Result<std::vector<std::string>> get_metadata_keys(const DataPath& path) override {
        auto res = get_metadata(path);
        if (!res) return Err<std::vector<std::string>>("get_metadata_keys failed", res);

        std::vector<std::string> keys;
        for (const auto& [k, _] : *res) {
            keys.push_back(k);
        }
        return Ok(keys);
    }

    Result<Value> get(const DataPath& path) override {
        auto parent = path.dirname();
        auto key = path.filename();

        auto meta_res = get_metadata(parent);
        if (!meta_res) return Err<Value>("get failed", meta_res);

        auto& meta = *meta_res;
        auto it = meta.find(key);
        if (it != meta.end()) {
            return Ok(it->second);
        }
        return Ok(Value{});
    }

    Result<void> set(const DataPath& path, const Value& value) override {
        return Err<void>("WaveformManager: set not implemented");
    }

    Result<void> add_child(const DataPath& path, const std::string& name, const Dict& data) override {
        return Err<void>("WaveformManager: add_child not implemented");
    }

    Result<std::string> as_tree(const DataPath& path, int depth) override {
        return Ok(path.to_string());
    }

    Result<MediatedAudioBufferPtr> open(const DataPath& path, const Dict& config) {
        std::string path_str = path.to_string();
        if (!path_str.empty() && path_str[0] != '/') {
            path_str = "/" + path_str;
        }

        for (const auto& type : WAVEFORM_TYPES) {
            std::string expected = "/available/" + type + "/0";
            if (path_str == expected) {
                if (_devices.find(type) == _devices.end()) {
                    float frequency = 440.0f;
                    int sample_rate = 48000;

                    if (auto it = config.find("frequency"); it != config.end()) {
                        if (auto f = get_as<double>(it->second)) frequency = static_cast<float>(*f);
                    }
                    if (auto it = config.find("sample-rate"); it != config.end()) {
                        if (auto sr = get_as<int64_t>(it->second)) sample_rate = static_cast<int>(*sr);
                    }

                    auto device_res = WaveformDevice::create(type, sample_rate, frequency);
                    if (!device_res) {
                        return Err<MediatedAudioBufferPtr>("Failed to create waveform device", device_res);
                    }
                    _devices[type] = *device_res;
                    _devices[type]->start();
                }

                return Ok(_devices[type]->get_buffer());
            }
        }

        return Err<MediatedAudioBufferPtr>("Invalid path for open: " + path_str);
    }

    ~WaveformManager() {
        for (auto& [_, device] : _devices) {
            device->stop();
        }
    }

private:
    std::map<std::string, WaveformDevicePtr> _devices;
};

namespace embedded {
    Result<TreeLikePtr> create_waveform_manager() {
        return WaveformManager::create();
    }
}

} // namespace ymery
