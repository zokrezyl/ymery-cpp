// PipeWire audio backend plugin - captures audio via PipeWire
#include "../../types.hpp"
#include "../../result.hpp"
#include "../../backend/audio_buffer.hpp"
#include <map>
#include <set>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/debug/types.h>
#include <spa/utils/result.h>

// Forward declarations for plugin create signature
namespace ymery { class Dispatcher; class PluginManager; }
#include <spdlog/spdlog.h>

namespace ymery::plugins {

/**
 * PipeWireDevice - captures audio from a PipeWire stream
 */
class PipeWireDevice {
public:
    static Result<std::shared_ptr<PipeWireDevice>> create(
        const std::string& target_name,
        int num_channels,
        int sample_rate
    ) {
        auto device = std::make_shared<PipeWireDevice>();
        device->_target_name = target_name;
        device->_num_channels = num_channels;
        device->_sample_rate = sample_rate;

        // Create ring buffers and mediated buffers per channel
        size_t ring_size = sample_rate;  // 1 second buffer
        size_t period_size = 1024;
        for (int ch = 0; ch < num_channels; ++ch) {
            auto buffer_res = AudioRingBuffer::create(sample_rate, ring_size, period_size);
            if (!buffer_res) {
                return Err<std::shared_ptr<PipeWireDevice>>("PipeWireDevice: failed to create ring buffer", buffer_res);
            }
            device->_ring_buffers.push_back(*buffer_res);

            auto mediated_res = MediatedAudioBuffer::create(device->_ring_buffers[ch]);
            if (!mediated_res) {
                return Err<std::shared_ptr<PipeWireDevice>>("PipeWireDevice: failed to create mediated buffer", mediated_res);
            }
            device->_mediated_buffers.push_back(*mediated_res);
        }

        // Allocate deinterleave buffer
        device->_channel_buffer.resize(period_size);

        spdlog::debug("PipeWireDevice: created for '{}' with {} channels at {}Hz",
                     target_name, num_channels, sample_rate);

        return device;
    }

    Result<void> start() {
        if (_running) return Ok();

        _running = true;
        _thread = std::thread(&PipeWireDevice::_run, this);

        spdlog::debug("PipeWireDevice: started '{}'", _target_name);
        return Ok();
    }

    void stop() {
        if (!_running) return;
        _running = false;

        // Signal the loop to quit
        if (_loop) {
            pw_loop_signal_event(_loop, _quit_signal);
        }

        if (_thread.joinable()) {
            _thread.join();
        }

        spdlog::debug("PipeWireDevice: stopped '{}'", _target_name);
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
    const std::string& target_name() const { return _target_name; }

    std::vector<std::string> get_port_names() const {
        std::vector<std::string> names;
        for (int i = 0; i < _num_channels; ++i) {
            names.push_back("channel_" + std::to_string(i));
        }
        return names;
    }

    ~PipeWireDevice() {
        stop();
    }

private:
    void _run() {
        // Initialize PipeWire for this thread
        pw_init(nullptr, nullptr);

        _loop = pw_loop_new(nullptr);
        if (!_loop) {
            spdlog::error("PipeWireDevice: failed to create loop");
            _running = false;
            return;
        }

        // Create a quit signal
        _quit_signal = pw_loop_add_signal(_loop, SIGINT, _on_quit_signal, this);

        struct pw_context* context = pw_context_new(_loop, nullptr, 0);
        if (!context) {
            spdlog::error("PipeWireDevice: failed to create context");
            pw_loop_destroy(_loop);
            _loop = nullptr;
            _running = false;
            return;
        }

        struct pw_core* core = pw_context_connect(context, nullptr, 0);
        if (!core) {
            spdlog::error("PipeWireDevice: failed to connect to PipeWire");
            pw_context_destroy(context);
            pw_loop_destroy(_loop);
            _loop = nullptr;
            _running = false;
            return;
        }

        // Set up audio format
        uint8_t buffer[1024];
        struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

        struct spa_audio_info_raw audio_info = {};
        audio_info.format = SPA_AUDIO_FORMAT_F32;
        audio_info.rate = _sample_rate;
        audio_info.channels = _num_channels;

        const struct spa_pod* params[1];
        params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &audio_info);

        // Stream events
        static const struct pw_stream_events stream_events = {
            .version = PW_VERSION_STREAM_EVENTS,
            .process = _on_process,
        };

        // Create capture stream
        struct pw_properties* props = pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio",
            PW_KEY_MEDIA_CATEGORY, "Capture",
            PW_KEY_MEDIA_ROLE, "Music",
            nullptr
        );

        if (!_target_name.empty() && _target_name != "default") {
            pw_properties_set(props, PW_KEY_TARGET_OBJECT, _target_name.c_str());
        }

        _stream = pw_stream_new(core, "ymery-capture", props);
        if (!_stream) {
            spdlog::error("PipeWireDevice: failed to create stream");
            pw_core_disconnect(core);
            pw_context_destroy(context);
            pw_loop_destroy(_loop);
            _loop = nullptr;
            _running = false;
            return;
        }

        pw_stream_add_listener(_stream, &_stream_listener, &stream_events, this);

        int res = pw_stream_connect(_stream,
            PW_DIRECTION_INPUT,
            PW_ID_ANY,
            static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
            params, 1);

        if (res < 0) {
            spdlog::error("PipeWireDevice: failed to connect stream: {}", spa_strerror(res));
            pw_stream_destroy(_stream);
            _stream = nullptr;
            pw_core_disconnect(core);
            pw_context_destroy(context);
            pw_loop_destroy(_loop);
            _loop = nullptr;
            _running = false;
            return;
        }

        // Run the loop
        while (_running) {
            pw_loop_iterate(_loop, -1);
        }

        // Cleanup
        if (_stream) {
            pw_stream_destroy(_stream);
            _stream = nullptr;
        }
        pw_core_disconnect(core);
        pw_context_destroy(context);
        pw_loop_destroy(_loop);
        _loop = nullptr;

        pw_deinit();
    }

    static void _on_process(void* userdata) {
        auto* device = static_cast<PipeWireDevice*>(userdata);
        if (!device->_running || !device->_stream) return;

        struct pw_buffer* b = pw_stream_dequeue_buffer(device->_stream);
        if (!b) return;

        struct spa_buffer* buf = b->buffer;
        if (!buf->datas[0].data) {
            pw_stream_queue_buffer(device->_stream, b);
            return;
        }

        const float* samples = static_cast<const float*>(buf->datas[0].data);
        uint32_t n_frames = buf->datas[0].chunk->size / (sizeof(float) * device->_num_channels);

        // Deinterleave and write to per-channel ring buffers
        for (int ch = 0; ch < device->_num_channels; ++ch) {
            device->_deinterleave_channel(samples, ch, n_frames);
            device->_ring_buffers[ch]->write(device->_channel_buffer.data(), n_frames);
        }

        pw_stream_queue_buffer(device->_stream, b);
    }

    static void _on_quit_signal(void* userdata, int signal_number) {
        auto* device = static_cast<PipeWireDevice*>(userdata);
        device->_running = false;
    }

    void _deinterleave_channel(const float* interleaved, int channel, uint32_t num_frames) {
        if (num_frames > _channel_buffer.size()) {
            _channel_buffer.resize(num_frames);
        }
        for (uint32_t i = 0; i < num_frames; ++i) {
            _channel_buffer[i] = interleaved[i * _num_channels + channel];
        }
    }

    std::string _target_name;
    int _num_channels = 2;
    int _sample_rate = 48000;

    struct pw_loop* _loop = nullptr;
    struct pw_stream* _stream = nullptr;
    struct spa_hook _stream_listener{};
    struct spa_source* _quit_signal = nullptr;

    std::vector<AudioRingBufferPtr> _ring_buffers;
    std::vector<MediatedAudioBufferPtr> _mediated_buffers;
    std::vector<float> _channel_buffer;

    std::atomic<bool> _running{false};
    std::thread _thread;
};

using PipeWireDevicePtr = std::shared_ptr<PipeWireDevice>;

/**
 * PipeWireManager - manages PipeWire streams, implements TreeLike
 * Tree structure:
 *   /available
 *     /default - default audio source
 *     /nodes - list available audio nodes
 *       /<node_name> - node info
 *   /opened
 *     /<stream_name> - opened device
 *       /<channel> - channel with buffer
 */
class PipeWireManager : public TreeLike {
public:
    static Result<TreeLikePtr> create() {
        auto manager = std::make_shared<PipeWireManager>();
        if (auto res = manager->init(); !res) {
            return Err<TreeLikePtr>("PipeWireManager::create failed", res);
        }
        return manager;
    }

    Result<void> init() override {
        pw_init(nullptr, nullptr);
        _initialized = true;
        spdlog::debug("PipeWireManager: initialized");
        return Ok();
    }

    Result<void> dispose() override {
        for (auto& [_, device] : _devices) {
            device->stop();
        }
        _devices.clear();

        if (_initialized) {
            pw_deinit();
            _initialized = false;
        }
        return Ok();
    }

    Result<std::vector<std::string>> get_children_names(const DataPath& path) override {
        std::string path_str = path.to_string();
        // Normalize: ensure path starts with / for consistent matching
        if (!path_str.empty() && path_str[0] != '/') {
            path_str = "/" + path_str;
        }

        // Root
        if (path_str == "/" || path_str.empty()) {
            return Ok(std::vector<std::string>{"available", "opened"});
        }

        // /available
        if (path_str == "/available") {
            return Ok(std::vector<std::string>{"default", "nodes"});
        }

        // /available/default (leaf)
        if (path_str == "/available/default") {
            return Ok(std::vector<std::string>{});
        }

        // /available/nodes - enumerate audio sources
        if (path_str == "/available/nodes") {
            return _get_available_nodes();
        }

        // /available/nodes/<node> (leaf)
        if (path.as_list().size() == 3 &&
            path.as_list()[0] == "available" &&
            path.as_list()[1] == "nodes") {
            return Ok(std::vector<std::string>{});
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
                return Ok(it->second->get_port_names());
            }
        }

        return Ok(std::vector<std::string>{});
    }

    Result<Dict> get_metadata(const DataPath& path) override {
        std::string path_str = path.to_string();
        // Normalize: ensure path starts with / for consistent matching
        if (!path_str.empty() && path_str[0] != '/') {
            path_str = "/" + path_str;
        }

        // Root
        if (path_str == "/" || path_str.empty()) {
            return Ok(Dict{
                {"name", Value("pipewire")},
                {"label", Value("PipeWire Audio")},
                {"type", Value("pipewire-manager")},
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

        // /available/default
        if (path_str == "/available/default") {
            return Ok(Dict{
                {"name", Value("default")},
                {"label", Value("Default Audio Source")},
                {"type", Value("pipewire-device")},
                {"category", Value("audio-device")},
                {"capabilities", Value(Dict{
                    {"openable", Value(true)},
                    {"readable", Value(true)},
                    {"writable", Value(false)}
                })},
                {"config-schema", Value(Dict{
                    {"num-channels", Value(Dict{
                        {"type", Value("int")},
                        {"default", Value(int64_t(2))},
                        {"label", Value("Number of Channels")}
                    })},
                    {"sample-rate", Value(Dict{
                        {"type", Value("int")},
                        {"default", Value(int64_t(48000))},
                        {"label", Value("Sample Rate")}
                    })}
                })}
            });
        }

        // /available/nodes
        if (path_str == "/available/nodes") {
            return Ok(Dict{
                {"name", Value("nodes")},
                {"label", Value("Audio Nodes")},
                {"type", Value("folder")},
                {"category", Value("folder")}
            });
        }

        // /available/nodes/<node>
        if (path.as_list().size() == 3 &&
            path.as_list()[0] == "available" &&
            path.as_list()[1] == "nodes") {
            std::string node = path.as_list()[2];
            return Ok(Dict{
                {"name", Value(node)},
                {"label", Value(node)},
                {"type", Value("pipewire-device")},
                {"category", Value("audio-device")},
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
                    {"name", Value(device->target_name())},
                    {"label", Value(device->target_name())},
                    {"type", Value("pipewire-device")},
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
                std::string port_name = path.as_list()[2];
                int channel = 0;
                if (port_name.find("channel_") == 0) {
                    channel = std::stoi(port_name.substr(8));
                }
                auto buffer = it->second->get_buffer(channel);
                if (buffer) {
                    return Ok(Dict{
                        {"name", Value(port_name)},
                        {"label", Value(port_name)},
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
        for (const auto& [k, _] : *res) keys.push_back(k);
        return Ok(keys);
    }

    Result<Value> get(const DataPath& path) override {
        auto parent = path.dirname();
        auto key = path.filename();
        auto meta_res = get_metadata(parent);
        if (!meta_res) return Err<Value>("get failed", meta_res);
        auto it = meta_res->find(key);
        if (it != meta_res->end()) return Ok(it->second);
        return Ok(Value{});
    }

    Result<void> set(const DataPath& path, const Value& value) override {
        return Err<void>("PipeWireManager: set not implemented");
    }

    Result<void> add_child(const DataPath& path, const std::string& name, const Dict& data) override {
        return Err<void>("PipeWireManager: add_child not implemented");
    }

    Result<std::string> as_tree(const DataPath& path, int depth) override {
        return Ok(path.to_string());
    }

    // Open a PipeWire device
    Result<MediatedAudioBufferPtr> open(const DataPath& path, const Dict& config) {
        auto parts = path.as_list();

        std::string target_name = "default";
        int num_channels = 2;
        int sample_rate = 48000;

        // Parse path
        if (parts.size() >= 2 && parts[0] == "available") {
            if (parts[1] == "default") {
                target_name = "default";
            } else if (parts[1] == "nodes" && parts.size() >= 3) {
                target_name = parts[2];
            }
        }

        // Override from config
        if (auto c = config.find("num-channels"); c != config.end()) {
            if (auto v = get_as<int64_t>(c->second)) num_channels = static_cast<int>(*v);
        }
        if (auto c = config.find("sample-rate"); c != config.end()) {
            if (auto v = get_as<int64_t>(c->second)) sample_rate = static_cast<int>(*v);
        }

        std::string device_key = "pw_" + target_name;

        // Check if already opened
        if (_devices.find(device_key) == _devices.end()) {
            auto device_res = PipeWireDevice::create(target_name, num_channels, sample_rate);
            if (!device_res) {
                return Err<MediatedAudioBufferPtr>("PipeWireManager::open: failed to create device", device_res);
            }
            _devices[device_key] = *device_res;

            auto start_res = _devices[device_key]->start();
            if (!start_res) {
                _devices.erase(device_key);
                return Err<MediatedAudioBufferPtr>("PipeWireManager::open: failed to start device", start_res);
            }
        }

        // Default to channel 0
        int channel = 0;
        auto buffer = _devices[device_key]->get_buffer(channel);
        if (!buffer) {
            return Err<MediatedAudioBufferPtr>("PipeWireManager::open: invalid channel");
        }
        return Ok(buffer);
    }

    ~PipeWireManager() {
        dispose();
    }

private:
    Result<std::vector<std::string>> _get_available_nodes() {
        // Note: Full node enumeration requires running the PipeWire loop
        // For now, return a placeholder
        // A complete implementation would use pw_registry to enumerate nodes
        return Ok(std::vector<std::string>{});
    }

    std::map<std::string, PipeWireDevicePtr> _devices;
    bool _initialized = false;
};

} // namespace ymery::plugins

extern "C" const char* name() { return "pipewire"; }
extern "C" const char* type() { return "device-manager"; }
extern "C" ymery::Result<ymery::TreeLikePtr> create(
    std::shared_ptr<ymery::Dispatcher> /*dispatcher*/,
    std::shared_ptr<ymery::PluginManager> /*plugin_manager*/
) {
    return ymery::plugins::PipeWireManager::create();
}
