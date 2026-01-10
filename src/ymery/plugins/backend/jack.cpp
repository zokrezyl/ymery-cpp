// JACK audio backend plugin - captures audio via JACK Audio Connection Kit
#include "../../types.hpp"
#include "../../result.hpp"
#include "../../backend/audio_buffer.hpp"
#include <map>
#include <set>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <jack/jack.h>
#include <ytrace/ytrace.hpp>

// Forward declarations for plugin create signature
namespace ymery { class Dispatcher; class PluginManager; }

namespace ymery::plugins {

/**
 * JackDevice - a JACK client that captures audio from input ports
 */
class JackDevice {
public:
    static Result<std::shared_ptr<JackDevice>> create(
        const std::string& client_name,
        int num_channels,
        const std::string& target_client = "",
        bool auto_connect = false
    ) {
        auto device = std::make_shared<JackDevice>();
        device->_client_name = client_name;
        device->_num_channels = num_channels;
        device->_target_client = target_client;
        device->_auto_connect = auto_connect;

        // Open JACK client
        jack_status_t status;
        device->_client = jack_client_open(client_name.c_str(), JackNoStartServer, &status);
        if (!device->_client) {
            return Err<std::shared_ptr<JackDevice>>(
                "JackDevice: failed to open client '" + client_name + "', status=" + std::to_string(status));
        }

        // Get sample rate and buffer size from JACK
        device->_sample_rate = jack_get_sample_rate(device->_client);
        device->_buffer_size = jack_get_buffer_size(device->_client);

        // If auto-connect and target specified, detect num_channels from target
        if (auto_connect && !target_client.empty()) {
            const char** ports = jack_get_ports(device->_client,
                target_client.c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput);
            if (ports) {
                int count = 0;
                while (ports[count]) count++;
                if (count > 0) {
                    device->_num_channels = count;
                    ydebug("JackDevice: auto-detected {} channels from {}", count, target_client);
                }
                jack_free(ports);
            }
        }

        // Create ring buffers and mediated buffers per channel
        size_t ring_size = device->_sample_rate;  // 1 second buffer
        for (int ch = 0; ch < device->_num_channels; ++ch) {
            auto buffer_res = AudioRingBuffer::create(device->_sample_rate, ring_size, device->_buffer_size);
            if (!buffer_res) {
                jack_client_close(device->_client);
                return Err<std::shared_ptr<JackDevice>>("JackDevice: failed to create ring buffer", buffer_res);
            }
            device->_ring_buffers.push_back(*buffer_res);

            auto mediated_res = MediatedAudioBuffer::create(device->_ring_buffers[ch]);
            if (!mediated_res) {
                jack_client_close(device->_client);
                return Err<std::shared_ptr<JackDevice>>("JackDevice: failed to create mediated buffer", mediated_res);
            }
            device->_mediated_buffers.push_back(*mediated_res);
        }

        // Register input ports
        for (int i = 0; i < device->_num_channels; ++i) {
            std::string port_name = "input_" + std::to_string(i);
            jack_port_t* port = jack_port_register(device->_client, port_name.c_str(),
                JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
            if (!port) {
                jack_client_close(device->_client);
                return Err<std::shared_ptr<JackDevice>>(
                    "JackDevice: failed to register port '" + port_name + "'");
            }
            device->_input_ports.push_back(port);
        }

        // Set process callback
        jack_set_process_callback(device->_client, _process_callback, device.get());

        // Set shutdown callback
        jack_on_shutdown(device->_client, _shutdown_callback, device.get());

        ydebug("JackDevice: created client '{}' with {} channels at {}Hz, buffer={}",
                     client_name, device->_num_channels, device->_sample_rate, device->_buffer_size);

        return device;
    }

    Result<void> start() {
        if (_running) return Ok();

        // Activate client
        if (jack_activate(_client) != 0) {
            return Err<void>("JackDevice: failed to activate client");
        }

        _running = true;
        ydebug("JackDevice: activated client '{}'", _client_name);

        // Auto-connect if requested
        if (_auto_connect && !_target_client.empty()) {
            const char** ports = jack_get_ports(_client,
                _target_client.c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput);
            if (ports) {
                for (int i = 0; ports[i] && i < static_cast<int>(_input_ports.size()); ++i) {
                    const char* src_port = ports[i];
                    const char* dst_port = jack_port_name(_input_ports[i]);
                    if (jack_connect(_client, src_port, dst_port) == 0) {
                        ydebug("JackDevice: connected {} -> {}", src_port, dst_port);
                    }
                }
                jack_free(ports);
            }
        }

        return Ok();
    }

    void stop() {
        if (!_running) return;
        _running = false;

        if (_client) {
            jack_deactivate(_client);
            ydebug("JackDevice: deactivated client '{}'", _client_name);
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
    size_t buffer_size() const { return _buffer_size; }
    const std::string& client_name() const { return _client_name; }

    std::vector<std::string> get_port_names() const {
        std::vector<std::string> names;
        for (int i = 0; i < _num_channels; ++i) {
            names.push_back("input_" + std::to_string(i));
        }
        return names;
    }

    ~JackDevice() {
        stop();
        if (_client) {
            jack_client_close(_client);
            _client = nullptr;
        }
    }

private:
    static int _process_callback(jack_nframes_t nframes, void* arg) {
        auto* device = static_cast<JackDevice*>(arg);
        if (!device->_running) return 0;

        for (int ch = 0; ch < device->_num_channels; ++ch) {
            jack_default_audio_sample_t* buffer =
                static_cast<jack_default_audio_sample_t*>(
                    jack_port_get_buffer(device->_input_ports[ch], nframes));

            if (buffer) {
                device->_ring_buffers[ch]->write(buffer, nframes);
            }
        }

        return 0;
    }

    static void _shutdown_callback(void* arg) {
        auto* device = static_cast<JackDevice*>(arg);
        ywarn("JackDevice: JACK server shutdown");
        device->_running = false;
    }

    std::string _client_name;
    std::string _target_client;
    int _num_channels = 2;
    int _sample_rate = 48000;
    size_t _buffer_size = 1024;
    bool _auto_connect = false;

    jack_client_t* _client = nullptr;
    std::vector<jack_port_t*> _input_ports;
    std::vector<AudioRingBufferPtr> _ring_buffers;
    std::vector<MediatedAudioBufferPtr> _mediated_buffers;

    std::atomic<bool> _running{false};
};

using JackDevicePtr = std::shared_ptr<JackDevice>;

/**
 * JackManager - manages JACK clients, implements TreeLike
 * Tree structure:
 *   /available
 *     /client - create manual JACK client
 *     /readable-clients - list clients with output ports (can capture from)
 *       /<client_name> - client info
 *         /<port_name> - port info
 *   /opened
 *     /<client_name> - opened device
 *       /<port_name> - channel with buffer
 */
class JackManager : public TreeLike {
public:
    static Result<TreeLikePtr> create() {
        auto manager = std::make_shared<JackManager>();
        if (auto res = manager->init(); !res) {
            return Err<TreeLikePtr>("JackManager::create failed", res);
        }
        return manager;
    }

    Result<void> init() override {
        // Create a query client for discovering available ports
        jack_status_t status;
        _query_client = jack_client_open("ymery_query", JackNoStartServer, &status);
        if (!_query_client) {
            ywarn("JackManager: JACK server not running (status={})", static_cast<int>(status));
        }
        return Ok();
    }

    Result<void> dispose() override {
        for (auto& [_, device] : _devices) {
            device->stop();
        }
        _devices.clear();

        if (_query_client) {
            jack_client_close(_query_client);
            _query_client = nullptr;
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
            return Ok(std::vector<std::string>{"client", "readable-clients"});
        }

        // /available/client (leaf - no children)
        if (path_str == "/available/client") {
            return Ok(std::vector<std::string>{});
        }

        // /available/readable-clients - list clients with output ports
        if (path_str == "/available/readable-clients") {
            return _get_readable_clients();
        }

        // /available/readable-clients/<client> - list ports
        if (path.as_list().size() == 3 &&
            path.as_list()[0] == "available" &&
            path.as_list()[1] == "readable-clients") {
            return _get_client_ports(path.as_list()[2]);
        }

        // /available/readable-clients/<client>/<port> (leaf)
        if (path.as_list().size() == 4 &&
            path.as_list()[0] == "available" &&
            path.as_list()[1] == "readable-clients") {
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
                {"name", Value("jack")},
                {"label", Value("JACK Audio")},
                {"type", Value("jack-manager")},
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

        // /available/client
        if (path_str == "/available/client") {
            return Ok(Dict{
                {"name", Value("client")},
                {"label", Value("Create JACK Client")},
                {"type", Value("jack-device")},
                {"category", Value("audio-device")},
                {"capabilities", Value(Dict{
                    {"openable", Value(true)},
                    {"readable", Value(true)},
                    {"writable", Value(false)}
                })},
                {"config-schema", Value(Dict{
                    {"client-name", Value(Dict{
                        {"type", Value("str")},
                        {"default", Value("ymery_client")},
                        {"label", Value("Client Name")}
                    })},
                    {"num-channels", Value(Dict{
                        {"type", Value("int")},
                        {"default", Value(int64_t(2))},
                        {"label", Value("Number of Channels")}
                    })}
                })}
            });
        }

        // /available/readable-clients
        if (path_str == "/available/readable-clients") {
            return Ok(Dict{
                {"name", Value("readable-clients")},
                {"label", Value("Readable Clients")},
                {"type", Value("folder")},
                {"category", Value("folder")}
            });
        }

        // /available/readable-clients/<client>
        if (path.as_list().size() == 3 &&
            path.as_list()[0] == "available" &&
            path.as_list()[1] == "readable-clients") {
            std::string client = path.as_list()[2];
            auto ports_res = _get_client_ports(client);
            int num_ports = ports_res ? ports_res->size() : 0;
            return Ok(Dict{
                {"name", Value(client)},
                {"label", Value(client + " (" + std::to_string(num_ports) + " ports)")},
                {"type", Value("jack-device")},
                {"category", Value("audio-device")},
                {"capabilities", Value(Dict{
                    {"openable", Value(true)},
                    {"readable", Value(true)},
                    {"writable", Value(false)}
                })}
            });
        }

        // /available/readable-clients/<client>/<port>
        if (path.as_list().size() == 4 &&
            path.as_list()[0] == "available" &&
            path.as_list()[1] == "readable-clients") {
            std::string port = path.as_list()[3];
            return Ok(Dict{
                {"name", Value(port)},
                {"label", Value(port)},
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
                    {"name", Value(device->client_name())},
                    {"label", Value(device->client_name())},
                    {"type", Value("jack-device")},
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
                // Parse port index from "input_N"
                int channel = 0;
                if (port_name.find("input_") == 0) {
                    channel = std::stoi(port_name.substr(6));
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
        return Err<void>("JackManager: set not implemented");
    }

    Result<void> add_child(const DataPath& path, const std::string& name, const Dict& data) override {
        return Err<void>("JackManager: add_child not implemented");
    }

    Result<std::string> as_tree(const DataPath& path, int depth) override {
        return Ok(path.to_string());
    }

    // Open a JACK device
    Result<MediatedAudioBufferPtr> open(const DataPath& path, const Dict& config) {
        auto parts = path.as_list();

        // /available/readable-clients/<client>
        if (parts.size() >= 3 && parts[0] == "available" && parts[1] == "readable-clients") {
            std::string target_client = parts[2];
            std::string device_key = "ymery_" + target_client;

            // Check if already opened
            if (_devices.find(device_key) == _devices.end()) {
                auto device_res = JackDevice::create(device_key, 2, target_client, true);
                if (!device_res) {
                    return Err<MediatedAudioBufferPtr>("JackManager::open: failed to create device", device_res);
                }
                _devices[device_key] = *device_res;

                auto start_res = _devices[device_key]->start();
                if (!start_res) {
                    _devices.erase(device_key);
                    return Err<MediatedAudioBufferPtr>("JackManager::open: failed to start device", start_res);
                }
            }

            // Get channel (default 0, or specified in path)
            int channel = 0;
            if (parts.size() >= 4) {
                // Port name like "out_1" -> parse index
                std::string port_name = parts[3];
                auto ports_res = _get_client_ports(target_client);
                if (ports_res) {
                    for (size_t i = 0; i < ports_res->size(); ++i) {
                        if ((*ports_res)[i] == port_name) {
                            channel = static_cast<int>(i);
                            break;
                        }
                    }
                }
            }

            auto buffer = _devices[device_key]->get_buffer(channel);
            if (!buffer) {
                return Err<MediatedAudioBufferPtr>("JackManager::open: invalid channel");
            }
            return Ok(buffer);
        }

        // /available/client - create manual client
        if (parts.size() >= 2 && parts[0] == "available" && parts[1] == "client") {
            std::string client_name = "ymery_client";
            int num_channels = 2;

            if (auto c = config.find("client-name"); c != config.end()) {
                if (auto v = get_as<std::string>(c->second)) client_name = *v;
            }
            if (auto c = config.find("num-channels"); c != config.end()) {
                if (auto v = get_as<int64_t>(c->second)) num_channels = static_cast<int>(*v);
            }

            if (_devices.find(client_name) != _devices.end()) {
                return Err<MediatedAudioBufferPtr>("JackManager::open: client already exists");
            }

            auto device_res = JackDevice::create(client_name, num_channels);
            if (!device_res) {
                return Err<MediatedAudioBufferPtr>("JackManager::open: failed to create device", device_res);
            }
            _devices[client_name] = *device_res;

            auto start_res = _devices[client_name]->start();
            if (!start_res) {
                _devices.erase(client_name);
                return Err<MediatedAudioBufferPtr>("JackManager::open: failed to start device", start_res);
            }

            return Ok(_devices[client_name]->get_buffer(0));
        }

        return Err<MediatedAudioBufferPtr>("JackManager::open: invalid path");
    }

    ~JackManager() {
        dispose();
    }

private:
    Result<std::vector<std::string>> _get_readable_clients() {
        std::vector<std::string> clients;

        if (!_query_client) {
            return Ok(clients);
        }

        const char** ports = jack_get_ports(_query_client, nullptr,
            JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput);

        if (ports) {
            std::set<std::string> client_set;
            for (int i = 0; ports[i]; ++i) {
                std::string port_name = ports[i];
                size_t colon = port_name.find(':');
                if (colon != std::string::npos) {
                    client_set.insert(port_name.substr(0, colon));
                }
            }
            jack_free(ports);

            for (const auto& c : client_set) {
                clients.push_back(c);
            }
        }

        return Ok(clients);
    }

    Result<std::vector<std::string>> _get_client_ports(const std::string& client_name) {
        std::vector<std::string> port_names;

        if (!_query_client) {
            return Ok(port_names);
        }

        const char** ports = jack_get_ports(_query_client, client_name.c_str(),
            JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput);

        if (ports) {
            for (int i = 0; ports[i]; ++i) {
                std::string full_name = ports[i];
                size_t colon = full_name.find(':');
                if (colon != std::string::npos) {
                    port_names.push_back(full_name.substr(colon + 1));
                }
            }
            jack_free(ports);
        }

        return Ok(port_names);
    }

    jack_client_t* _query_client = nullptr;
    std::map<std::string, JackDevicePtr> _devices;
    std::mutex _mutex;
};

} // namespace ymery::plugins

extern "C" const char* name() { return "jack"; }
extern "C" const char* type() { return "device-manager"; }
extern "C" void* create(
    std::shared_ptr<ymery::Dispatcher> /*dispatcher*/,
    std::shared_ptr<ymery::PluginManager> /*plugin_manager*/
) {
    return static_cast<void*>(new ymery::Result<ymery::TreeLikePtr>(ymery::plugins::JackManager::create()));
}
