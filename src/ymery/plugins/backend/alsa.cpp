// ALSA audio backend plugin - captures audio from ALSA devices
#include "../../types.hpp"
#include "../../result.hpp"
#include "../../backend/audio_buffer.hpp"
#include <map>
#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <alsa/asoundlib.h>
#include <spdlog/spdlog.h>

// Forward declarations for plugin create signature
namespace ymery { class Dispatcher; class PluginManager; }

namespace ymery::plugins {

/**
 * AlsaDevice - captures audio from an ALSA device in a background thread
 */
class AlsaDevice {
public:
    static Result<std::shared_ptr<AlsaDevice>> create(
        const std::string& device_name,
        int num_channels,
        int sample_rate,
        size_t period_size,
        size_t buffer_size = 0  // 0 = auto (1 second)
    ) {
        auto device = std::make_shared<AlsaDevice>();
        device->_device_name = device_name;
        device->_num_channels = num_channels;
        device->_sample_rate = sample_rate;
        device->_period_size = period_size;
        device->_buffer_size = buffer_size > 0 ? buffer_size : sample_rate;  // Default 1 second

        // Open ALSA device
        int err = snd_pcm_open(&device->_pcm, device_name.c_str(), SND_PCM_STREAM_CAPTURE, 0);
        if (err < 0) {
            return Err<std::shared_ptr<AlsaDevice>>(
                "AlsaDevice: failed to open device '" + device_name + "': " + snd_strerror(err));
        }

        // Configure hardware parameters
        snd_pcm_hw_params_t* hw_params;
        snd_pcm_hw_params_alloca(&hw_params);
        snd_pcm_hw_params_any(device->_pcm, hw_params);

        // Set access type (interleaved)
        err = snd_pcm_hw_params_set_access(device->_pcm, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
        if (err < 0) {
            snd_pcm_close(device->_pcm);
            return Err<std::shared_ptr<AlsaDevice>>(
                "AlsaDevice: failed to set access type: " + std::string(snd_strerror(err)));
        }

        // Set format (float)
        err = snd_pcm_hw_params_set_format(device->_pcm, hw_params, SND_PCM_FORMAT_FLOAT_LE);
        if (err < 0) {
            // Try S16_LE as fallback
            err = snd_pcm_hw_params_set_format(device->_pcm, hw_params, SND_PCM_FORMAT_S16_LE);
            if (err < 0) {
                snd_pcm_close(device->_pcm);
                return Err<std::shared_ptr<AlsaDevice>>(
                    "AlsaDevice: failed to set format: " + std::string(snd_strerror(err)));
            }
            device->_format = SND_PCM_FORMAT_S16_LE;
        } else {
            device->_format = SND_PCM_FORMAT_FLOAT_LE;
        }

        // Set channels
        err = snd_pcm_hw_params_set_channels(device->_pcm, hw_params, num_channels);
        if (err < 0) {
            snd_pcm_close(device->_pcm);
            return Err<std::shared_ptr<AlsaDevice>>(
                "AlsaDevice: failed to set channels: " + std::string(snd_strerror(err)));
        }

        // Set sample rate
        unsigned int rate = sample_rate;
        err = snd_pcm_hw_params_set_rate_near(device->_pcm, hw_params, &rate, nullptr);
        if (err < 0) {
            snd_pcm_close(device->_pcm);
            return Err<std::shared_ptr<AlsaDevice>>(
                "AlsaDevice: failed to set sample rate: " + std::string(snd_strerror(err)));
        }
        device->_sample_rate = rate;

        // Set period size
        snd_pcm_uframes_t frames = period_size;
        err = snd_pcm_hw_params_set_period_size_near(device->_pcm, hw_params, &frames, nullptr);
        if (err < 0) {
            snd_pcm_close(device->_pcm);
            return Err<std::shared_ptr<AlsaDevice>>(
                "AlsaDevice: failed to set period size: " + std::string(snd_strerror(err)));
        }
        device->_period_size = frames;

        // Apply hardware parameters
        err = snd_pcm_hw_params(device->_pcm, hw_params);
        if (err < 0) {
            snd_pcm_close(device->_pcm);
            return Err<std::shared_ptr<AlsaDevice>>(
                "AlsaDevice: failed to apply hw params: " + std::string(snd_strerror(err)));
        }

        // Create ring buffer and mediated buffer per channel
        for (int ch = 0; ch < num_channels; ++ch) {
            auto buffer_res = AudioRingBuffer::create(device->_sample_rate, device->_buffer_size, device->_period_size);
            if (!buffer_res) {
                snd_pcm_close(device->_pcm);
                return Err<std::shared_ptr<AlsaDevice>>("AlsaDevice: failed to create ring buffer", buffer_res);
            }
            device->_ring_buffers.push_back(*buffer_res);

            auto mediated_res = MediatedAudioBuffer::create(device->_ring_buffers[ch]);
            if (!mediated_res) {
                snd_pcm_close(device->_pcm);
                return Err<std::shared_ptr<AlsaDevice>>("AlsaDevice: failed to create mediated buffer", mediated_res);
            }
            device->_mediated_buffers.push_back(*mediated_res);
        }

        // Allocate interleaved sample buffer
        size_t sample_bytes = (device->_format == SND_PCM_FORMAT_FLOAT_LE) ? 4 : 2;
        device->_interleaved_buffer.resize(period_size * num_channels * sample_bytes);
        device->_channel_buffer.resize(period_size);

        spdlog::info("AlsaDevice: opened {} with {} channels at {}Hz, period={}",
                     device_name, num_channels, device->_sample_rate, device->_period_size);

        return device;
    }

    void start() {
        if (_running) return;

        int err = snd_pcm_prepare(_pcm);
        if (err < 0) {
            spdlog::error("AlsaDevice: prepare failed: {}", snd_strerror(err));
            return;
        }

        _running = true;
        _thread = std::thread(&AlsaDevice::_run, this);
    }

    void stop() {
        _running = false;
        if (_thread.joinable()) {
            _thread.join();
        }
    }

    bool is_running() const { return _running; }

    MediatedAudioBufferPtr get_buffer(int channel) const {
        if (channel >= 0 && channel < static_cast<int>(_mediated_buffers.size())) {
            return _mediated_buffers[channel];
        }
        return nullptr;
    }

    int num_channels() const { return _num_channels; }
    int sample_rate() const { return _sample_rate; }
    size_t period_size() const { return _period_size; }
    const std::string& device_name() const { return _device_name; }

    ~AlsaDevice() {
        stop();
        if (_pcm) {
            snd_pcm_close(_pcm);
            _pcm = nullptr;
        }
    }

private:
    void _run() {
        while (_running) {
            snd_pcm_sframes_t frames = snd_pcm_readi(_pcm, _interleaved_buffer.data(), _period_size);

            if (frames < 0) {
                // Handle xrun (buffer overrun)
                if (frames == -EPIPE) {
                    spdlog::warn("AlsaDevice: buffer overrun, recovering...");
                    snd_pcm_prepare(_pcm);
                    continue;
                }
                spdlog::error("AlsaDevice: read error: {}", snd_strerror(frames));
                continue;
            }

            if (frames == 0) continue;

            // Deinterleave and write to per-channel ring buffers
            for (int ch = 0; ch < _num_channels; ++ch) {
                _deinterleave_channel(ch, frames);
                _ring_buffers[ch]->write(_channel_buffer.data(), frames);
            }
        }
    }

    void _deinterleave_channel(int channel, snd_pcm_sframes_t frames) {
        if (_format == SND_PCM_FORMAT_FLOAT_LE) {
            const float* src = reinterpret_cast<const float*>(_interleaved_buffer.data());
            for (snd_pcm_sframes_t i = 0; i < frames; ++i) {
                _channel_buffer[i] = src[i * _num_channels + channel];
            }
        } else {
            // SND_PCM_FORMAT_S16_LE - convert to float
            const int16_t* src = reinterpret_cast<const int16_t*>(_interleaved_buffer.data());
            constexpr float scale = 1.0f / 32768.0f;
            for (snd_pcm_sframes_t i = 0; i < frames; ++i) {
                _channel_buffer[i] = src[i * _num_channels + channel] * scale;
            }
        }
    }

    std::string _device_name;
    int _num_channels = 2;
    int _sample_rate = 48000;
    size_t _period_size = 1024;
    size_t _buffer_size = 48000;
    snd_pcm_format_t _format = SND_PCM_FORMAT_FLOAT_LE;

    snd_pcm_t* _pcm = nullptr;
    std::vector<AudioRingBufferPtr> _ring_buffers;
    std::vector<MediatedAudioBufferPtr> _mediated_buffers;
    std::vector<uint8_t> _interleaved_buffer;
    std::vector<float> _channel_buffer;

    std::atomic<bool> _running{false};
    std::thread _thread;
};

using AlsaDevicePtr = std::shared_ptr<AlsaDevice>;

/**
 * AlsaManager - manages ALSA devices, implements TreeLike
 * Tree structure:
 *   /available - list available ALSA capture devices
 *   /available/<card>/<device> - device info
 *   /opened - list opened devices
 *   /opened/<device_name>/<channel> - opened channel with buffer
 */
class AlsaManager : public TreeLike {
public:
    static Result<TreeLikePtr> create() {
        auto manager = std::make_shared<AlsaManager>();
        if (auto res = manager->init(); !res) {
            return Err<TreeLikePtr>("AlsaManager::create failed", res);
        }
        return manager;
    }

    Result<std::vector<std::string>> get_children_names(const DataPath& path) override {
        std::string path_str = path.to_string();

        // Root
        if (path_str == "/" || path_str.empty()) {
            return Ok(std::vector<std::string>{"available", "opened"});
        }

        // /available - enumerate cards
        if (path_str == "/available") {
            return _get_available_cards();
        }

        // /available/<card> - enumerate devices
        if (path.as_list().size() == 2 && path.as_list()[0] == "available") {
            return _get_card_devices(path.as_list()[1]);
        }

        // /available/<card>/<device> - enumerate channels
        if (path.as_list().size() == 3 && path.as_list()[0] == "available") {
            return _get_device_channels(path.as_list()[1], path.as_list()[2]);
        }

        // /opened - list opened devices
        if (path_str == "/opened") {
            std::vector<std::string> children;
            for (const auto& [name, _] : _devices) {
                children.push_back(name);
            }
            return Ok(children);
        }

        // /opened/<device> - list channels
        if (path.as_list().size() == 2 && path.as_list()[0] == "opened") {
            auto it = _devices.find(path.as_list()[1]);
            if (it != _devices.end()) {
                std::vector<std::string> channels;
                for (int i = 0; i < it->second->num_channels(); ++i) {
                    channels.push_back(std::to_string(i));
                }
                return Ok(channels);
            }
        }

        return Ok(std::vector<std::string>{});
    }

    Result<Dict> get_metadata(const DataPath& path) override {
        std::string path_str = path.to_string();

        // Root
        if (path_str == "/" || path_str.empty()) {
            return Ok(Dict{
                {"name", Value("alsa")},
                {"label", Value("ALSA Audio")},
                {"type", Value("alsa-manager")},
                {"category", Value("audio-device-manager")}
            });
        }

        // /available
        if (path_str == "/available") {
            return Ok(Dict{
                {"name", Value("available")},
                {"label", Value("Available")},
                {"type", Value("folder")},
                {"category", Value("folder")}
            });
        }

        // /opened
        if (path_str == "/opened") {
            return Ok(Dict{
                {"name", Value("opened")},
                {"label", Value("Opened")},
                {"type", Value("folder")},
                {"category", Value("folder")}
            });
        }

        // /available/<card>
        if (path.as_list().size() == 2 && path.as_list()[0] == "available") {
            std::string card = path.as_list()[1];
            return Ok(Dict{
                {"name", Value(card)},
                {"label", Value("Card " + card)},
                {"type", Value("folder")},
                {"category", Value("folder")}
            });
        }

        // /available/<card>/<device>
        if (path.as_list().size() == 3 && path.as_list()[0] == "available") {
            std::string card = path.as_list()[1];
            std::string device = path.as_list()[2];
            std::string device_name = "hw:" + card + "," + device;
            return Ok(Dict{
                {"name", Value(device)},
                {"label", Value("Device " + device)},
                {"type", Value("alsa-device")},
                {"category", Value("audio-device")},
                {"device", Value(device_name)},
                {"capabilities", Value(Dict{
                    {"openable", Value(true)},
                    {"readable", Value(true)},
                    {"writable", Value(false)}
                })}
            });
        }

        // /available/<card>/<device>/<channel>
        if (path.as_list().size() == 4 && path.as_list()[0] == "available") {
            std::string channel = path.as_list()[3];
            return Ok(Dict{
                {"name", Value(channel)},
                {"label", Value("Channel " + channel)},
                {"type", Value("audio-channel")},
                {"category", Value("audio-channel")},
                {"capabilities", Value(Dict{
                    {"openable", Value(true)},
                    {"readable", Value(true)},
                    {"writable", Value(false)}
                })}
            });
        }

        // /opened/<device>
        if (path.as_list().size() == 2 && path.as_list()[0] == "opened") {
            auto it = _devices.find(path.as_list()[1]);
            if (it != _devices.end()) {
                auto& device = it->second;
                return Ok(Dict{
                    {"name", Value(device->device_name())},
                    {"label", Value(device->device_name())},
                    {"type", Value("alsa-device")},
                    {"category", Value("audio-device")},
                    {"status", Value(device->is_running() ? "running" : "stopped")},
                    {"sample-rate", Value(static_cast<int64_t>(device->sample_rate()))},
                    {"num-channels", Value(static_cast<int64_t>(device->num_channels()))}
                });
            }
        }

        // /opened/<device>/<channel>
        if (path.as_list().size() == 3 && path.as_list()[0] == "opened") {
            auto it = _devices.find(path.as_list()[1]);
            if (it != _devices.end()) {
                int channel = std::stoi(path.as_list()[2]);
                auto buffer = it->second->get_buffer(channel);
                if (buffer) {
                    return Ok(Dict{
                        {"name", Value(path.as_list()[2])},
                        {"label", Value("Channel " + path.as_list()[2])},
                        {"type", Value("audio-channel")},
                        {"category", Value("audio-channel")},
                        {"buffer", Value(buffer)}
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
        return Err<void>("AlsaManager: set not implemented");
    }

    Result<void> add_child(const DataPath& path, const std::string& name, const Dict& data) override {
        return Err<void>("AlsaManager: add_child not implemented");
    }

    Result<std::string> as_tree(const DataPath& path, int depth) override {
        return Ok(path.to_string());
    }

    // Open an ALSA device - creates device and returns info
    Result<MediatedAudioBufferPtr> open(const DataPath& path, const Dict& config) {
        // Parse path: /available/<card>/<device>/<channel>
        auto parts = path.as_list();
        if (parts.size() < 4 || parts[0] != "available") {
            return Err<MediatedAudioBufferPtr>("AlsaManager::open: invalid path");
        }

        std::string device_name = "hw:" + parts[1] + "," + parts[2];
        int channel = std::stoi(parts[3]);

        // Check if device already opened
        auto it = _devices.find(device_name);
        if (it == _devices.end()) {
            // Extract config
            int num_channels = 2;
            int sample_rate = 48000;
            size_t period_size = 1024;

            if (auto c = config.find("num-channels"); c != config.end()) {
                if (auto v = get_as<int64_t>(c->second)) num_channels = static_cast<int>(*v);
            }
            if (auto c = config.find("sample-rate"); c != config.end()) {
                if (auto v = get_as<int64_t>(c->second)) sample_rate = static_cast<int>(*v);
            }
            if (auto c = config.find("period-size"); c != config.end()) {
                if (auto v = get_as<int64_t>(c->second)) period_size = static_cast<size_t>(*v);
            }

            auto device_res = AlsaDevice::create(device_name, num_channels, sample_rate, period_size);
            if (!device_res) {
                return Err<MediatedAudioBufferPtr>("AlsaManager::open: failed to create device", device_res);
            }
            _devices[device_name] = *device_res;
            _devices[device_name]->start();
        }

        auto buffer = _devices[device_name]->get_buffer(channel);
        if (!buffer) {
            return Err<MediatedAudioBufferPtr>("AlsaManager::open: invalid channel " + std::to_string(channel));
        }

        return Ok(buffer);
    }

    ~AlsaManager() {
        for (auto& [_, device] : _devices) {
            device->stop();
        }
    }

private:
    Result<std::vector<std::string>> _get_available_cards() {
        std::vector<std::string> cards;
        int card = -1;

        while (snd_card_next(&card) >= 0 && card >= 0) {
            cards.push_back(std::to_string(card));
        }

        return Ok(cards);
    }

    Result<std::vector<std::string>> _get_card_devices(const std::string& card_str) {
        std::vector<std::string> devices;
        int card = std::stoi(card_str);

        snd_ctl_t* ctl;
        std::string ctl_name = "hw:" + card_str;
        if (snd_ctl_open(&ctl, ctl_name.c_str(), 0) < 0) {
            return Ok(devices);
        }

        int device = -1;
        while (snd_ctl_pcm_next_device(ctl, &device) >= 0 && device >= 0) {
            // Check if device supports capture
            snd_pcm_info_t* pcm_info;
            snd_pcm_info_alloca(&pcm_info);
            snd_pcm_info_set_device(pcm_info, device);
            snd_pcm_info_set_subdevice(pcm_info, 0);
            snd_pcm_info_set_stream(pcm_info, SND_PCM_STREAM_CAPTURE);

            if (snd_ctl_pcm_info(ctl, pcm_info) >= 0) {
                devices.push_back(std::to_string(device));
            }
        }

        snd_ctl_close(ctl);
        return Ok(devices);
    }

    Result<std::vector<std::string>> _get_device_channels(const std::string& card_str, const std::string& device_str) {
        // Open device temporarily to query channel count
        std::string device_name = "hw:" + card_str + "," + device_str;
        snd_pcm_t* pcm;

        if (snd_pcm_open(&pcm, device_name.c_str(), SND_PCM_STREAM_CAPTURE, 0) < 0) {
            return Ok(std::vector<std::string>{});
        }

        snd_pcm_hw_params_t* hw_params;
        snd_pcm_hw_params_alloca(&hw_params);
        snd_pcm_hw_params_any(pcm, hw_params);

        unsigned int max_channels;
        snd_pcm_hw_params_get_channels_max(hw_params, &max_channels);

        snd_pcm_close(pcm);

        std::vector<std::string> channels;
        for (unsigned int i = 0; i < max_channels && i < 32; ++i) {
            channels.push_back(std::to_string(i));
        }

        return Ok(channels);
    }

    std::map<std::string, AlsaDevicePtr> _devices;
};

} // namespace ymery::plugins

extern "C" const char* name() { return "alsa"; }
extern "C" const char* type() { return "device-manager"; }
extern "C" ymery::Result<ymery::TreeLikePtr> create(
    std::shared_ptr<ymery::Dispatcher> /*dispatcher*/,
    std::shared_ptr<ymery::PluginManager> /*plugin_manager*/
) {
    return ymery::plugins::AlsaManager::create();
}
