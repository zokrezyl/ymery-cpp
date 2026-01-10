// Core Audio backend plugin - captures audio on macOS
#include "../../types.hpp"
#include "../../result.hpp"
#include "../../backend/audio_buffer.hpp"
#include <map>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioToolbox.h>
#include <ytrace/ytrace.hpp>

// Forward declarations for plugin create signature
namespace ymery { class Dispatcher; class PluginManager; }

namespace ymery::plugins {

/**
 * CoreAudioDevice - captures audio from a Core Audio device using AudioQueue
 */
class CoreAudioDevice {
public:
    static Result<std::shared_ptr<CoreAudioDevice>> create(
        AudioDeviceID device_id,
        const std::string& device_name,
        int num_channels,
        int sample_rate,
        size_t buffer_size = 0  // 0 = auto (1 second)
    ) {
        auto device = std::make_shared<CoreAudioDevice>();
        device->_device_id = device_id;
        device->_device_name = device_name;
        device->_num_channels = num_channels;
        device->_sample_rate = sample_rate;
        device->_buffer_size = buffer_size > 0 ? buffer_size : sample_rate;

        // Create ring buffers and mediated buffers per channel
        for (int ch = 0; ch < num_channels; ++ch) {
            auto buffer_res = AudioRingBuffer::create(sample_rate, device->_buffer_size, 1024);
            if (!buffer_res) {
                return Err<std::shared_ptr<CoreAudioDevice>>("CoreAudioDevice: failed to create ring buffer", buffer_res);
            }
            device->_ring_buffers.push_back(*buffer_res);

            auto mediated_res = MediatedAudioBuffer::create(device->_ring_buffers[ch]);
            if (!mediated_res) {
                return Err<std::shared_ptr<CoreAudioDevice>>("CoreAudioDevice: failed to create mediated buffer", mediated_res);
            }
            device->_mediated_buffers.push_back(*mediated_res);
        }

        // Set up audio format
        device->_format.mSampleRate = sample_rate;
        device->_format.mFormatID = kAudioFormatLinearPCM;
        device->_format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
        device->_format.mBitsPerChannel = 32;
        device->_format.mChannelsPerFrame = num_channels;
        device->_format.mBytesPerFrame = sizeof(float) * num_channels;
        device->_format.mFramesPerPacket = 1;
        device->_format.mBytesPerPacket = device->_format.mBytesPerFrame;

        // Create audio queue for input
        OSStatus status = AudioQueueNewInput(
            &device->_format,
            _audio_queue_callback,
            device.get(),
            nullptr,  // Use internal run loop
            kCFRunLoopCommonModes,
            0,
            &device->_queue
        );

        if (status != noErr) {
            return Err<std::shared_ptr<CoreAudioDevice>>(
                "CoreAudioDevice: failed to create audio queue, status=" + std::to_string(status));
        }

        // Allocate and enqueue buffers
        UInt32 buffer_byte_size = 1024 * device->_format.mBytesPerFrame;
        for (int i = 0; i < 3; ++i) {
            AudioQueueBufferRef buffer;
            status = AudioQueueAllocateBuffer(device->_queue, buffer_byte_size, &buffer);
            if (status != noErr) {
                AudioQueueDispose(device->_queue, true);
                return Err<std::shared_ptr<CoreAudioDevice>>(
                    "CoreAudioDevice: failed to allocate buffer, status=" + std::to_string(status));
            }
            device->_queue_buffers.push_back(buffer);

            status = AudioQueueEnqueueBuffer(device->_queue, buffer, 0, nullptr);
            if (status != noErr) {
                AudioQueueDispose(device->_queue, true);
                return Err<std::shared_ptr<CoreAudioDevice>>(
                    "CoreAudioDevice: failed to enqueue buffer, status=" + std::to_string(status));
            }
        }

        // Allocate deinterleave buffer
        device->_channel_buffer.resize(1024);

        ydebug("CoreAudioDevice: created '{}' with {} channels at {}Hz",
                     device_name, num_channels, sample_rate);

        return device;
    }

    void start() {
        if (_running) return;

        OSStatus status = AudioQueueStart(_queue, nullptr);
        if (status != noErr) {
            ywarn("CoreAudioDevice: failed to start queue, status={}", status);
            return;
        }

        _running = true;
        ydebug("CoreAudioDevice: started '{}'", _device_name);
    }

    void stop() {
        if (!_running) return;
        _running = false;

        AudioQueueStop(_queue, true);
        ydebug("CoreAudioDevice: stopped '{}'", _device_name);
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
    const std::string& device_name() const { return _device_name; }
    AudioDeviceID device_id() const { return _device_id; }

    ~CoreAudioDevice() {
        stop();
        if (_queue) {
            AudioQueueDispose(_queue, true);
            _queue = nullptr;
        }
    }

private:
    static void _audio_queue_callback(
        void* user_data,
        AudioQueueRef queue,
        AudioQueueBufferRef buffer,
        const AudioTimeStamp* start_time,
        UInt32 num_packets,
        const AudioStreamPacketDescription* packet_desc
    ) {
        auto* device = static_cast<CoreAudioDevice*>(user_data);
        if (!device->_running) return;

        // Get interleaved float samples
        const float* samples = static_cast<const float*>(buffer->mAudioData);
        UInt32 num_frames = buffer->mAudioDataByteSize / device->_format.mBytesPerFrame;

        // Deinterleave and write to per-channel ring buffers
        for (int ch = 0; ch < device->_num_channels; ++ch) {
            device->_deinterleave_channel(samples, ch, num_frames);
            device->_ring_buffers[ch]->write(device->_channel_buffer.data(), num_frames);
        }

        // Re-enqueue the buffer
        AudioQueueEnqueueBuffer(queue, buffer, 0, nullptr);
    }

    void _deinterleave_channel(const float* interleaved, int channel, UInt32 num_frames) {
        if (num_frames > _channel_buffer.size()) {
            _channel_buffer.resize(num_frames);
        }
        for (UInt32 i = 0; i < num_frames; ++i) {
            _channel_buffer[i] = interleaved[i * _num_channels + channel];
        }
    }

    AudioDeviceID _device_id = 0;
    std::string _device_name;
    int _num_channels = 2;
    int _sample_rate = 48000;
    size_t _buffer_size = 48000;

    AudioStreamBasicDescription _format{};
    AudioQueueRef _queue = nullptr;
    std::vector<AudioQueueBufferRef> _queue_buffers;

    std::vector<AudioRingBufferPtr> _ring_buffers;
    std::vector<MediatedAudioBufferPtr> _mediated_buffers;
    std::vector<float> _channel_buffer;

    std::atomic<bool> _running{false};
};

using CoreAudioDevicePtr = std::shared_ptr<CoreAudioDevice>;

/**
 * CoreAudioManager - manages Core Audio devices, implements TreeLike
 * Tree structure:
 *   /available - list available input devices
 *   /available/<device_id> - device info
 *   /available/<device_id>/<channel> - channel info
 *   /opened - list opened devices
 *   /opened/<device_name>/<channel> - opened channel with buffer
 */
class CoreAudioManager : public TreeLike {
public:
    static Result<TreeLikePtr> create() {
        auto manager = std::make_shared<CoreAudioManager>();
        if (auto res = manager->init(); !res) {
            return Err<TreeLikePtr>("CoreAudioManager::create failed", res);
        }
        return manager;
    }

    Result<void> init() override {
        _refresh_devices();
        return Ok();
    }

    Result<std::vector<std::string>> get_children_names(const DataPath& path) override {
        std::string path_str = path.to_string();

        // Root
        if (path_str == "/" || path_str.empty()) {
            return Ok(std::vector<std::string>{"available", "opened"});
        }

        // /available - enumerate devices
        if (path_str == "/available") {
            _refresh_devices();
            std::vector<std::string> children;
            for (const auto& [id, _] : _available_devices) {
                children.push_back(std::to_string(id));
            }
            return Ok(children);
        }

        // /available/<device_id> - enumerate channels
        if (path.as_list().size() == 2 && path.as_list()[0] == "available") {
            AudioDeviceID device_id = std::stoul(path.as_list()[1]);
            auto it = _available_devices.find(device_id);
            if (it != _available_devices.end()) {
                std::vector<std::string> channels;
                for (int i = 0; i < it->second.num_channels; ++i) {
                    channels.push_back(std::to_string(i));
                }
                return Ok(channels);
            }
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
                {"name", Value("coreaudio")},
                {"label", Value("Core Audio")},
                {"type", Value("coreaudio-manager")},
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

        // /available/<device_id>
        if (path.as_list().size() == 2 && path.as_list()[0] == "available") {
            AudioDeviceID device_id = std::stoul(path.as_list()[1]);
            auto it = _available_devices.find(device_id);
            if (it != _available_devices.end()) {
                return Ok(Dict{
                    {"name", Value(it->second.name)},
                    {"label", Value(it->second.name)},
                    {"type", Value("coreaudio-device")},
                    {"category", Value("audio-device")},
                    {"device-id", Value(static_cast<int64_t>(device_id))},
                    {"num-channels", Value(static_cast<int64_t>(it->second.num_channels))},
                    {"sample-rate", Value(static_cast<int64_t>(it->second.sample_rate))},
                    {"capabilities", Value(Dict{
                        {"openable", Value(true)},
                        {"readable", Value(true)},
                        {"writable", Value(false)}
                    })}
                });
            }
        }

        // /available/<device_id>/<channel>
        if (path.as_list().size() == 3 && path.as_list()[0] == "available") {
            std::string channel = path.as_list()[2];
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
                    {"type", Value("coreaudio-device")},
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
        return Err<void>("CoreAudioManager: set not implemented");
    }

    Result<void> add_child(const DataPath& path, const std::string& name, const Dict& data) override {
        return Err<void>("CoreAudioManager: add_child not implemented");
    }

    Result<std::string> as_tree(const DataPath& path, int depth) override {
        return Ok(path.to_string());
    }

    // Open a Core Audio device
    Result<MediatedAudioBufferPtr> open(const DataPath& path, const Dict& config) {
        auto parts = path.as_list();
        if (parts.size() < 3 || parts[0] != "available") {
            return Err<MediatedAudioBufferPtr>("CoreAudioManager::open: invalid path");
        }

        AudioDeviceID device_id = std::stoul(parts[1]);
        int channel = std::stoi(parts[2]);

        auto it = _available_devices.find(device_id);
        if (it == _available_devices.end()) {
            return Err<MediatedAudioBufferPtr>("CoreAudioManager::open: device not found");
        }

        std::string device_key = it->second.name;

        // Check if device already opened
        if (_devices.find(device_key) == _devices.end()) {
            int num_channels = it->second.num_channels;
            int sample_rate = it->second.sample_rate;

            // Override from config if provided
            if (auto c = config.find("num-channels"); c != config.end()) {
                if (auto v = get_as<int64_t>(c->second)) num_channels = static_cast<int>(*v);
            }
            if (auto c = config.find("sample-rate"); c != config.end()) {
                if (auto v = get_as<int64_t>(c->second)) sample_rate = static_cast<int>(*v);
            }

            auto device_res = CoreAudioDevice::create(device_id, device_key, num_channels, sample_rate);
            if (!device_res) {
                return Err<MediatedAudioBufferPtr>("CoreAudioManager::open: failed to create device", device_res);
            }
            _devices[device_key] = *device_res;
            _devices[device_key]->start();
        }

        auto buffer = _devices[device_key]->get_buffer(channel);
        if (!buffer) {
            return Err<MediatedAudioBufferPtr>("CoreAudioManager::open: invalid channel " + std::to_string(channel));
        }

        return Ok(buffer);
    }

    ~CoreAudioManager() {
        for (auto& [_, device] : _devices) {
            device->stop();
        }
    }

private:
    struct DeviceInfo {
        std::string name;
        int num_channels = 2;
        int sample_rate = 48000;
    };

    void _refresh_devices() {
        _available_devices.clear();

        // Get list of all audio devices
        AudioObjectPropertyAddress property_address = {
            kAudioHardwarePropertyDevices,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };

        UInt32 data_size = 0;
        OSStatus status = AudioObjectGetPropertyDataSize(
            kAudioObjectSystemObject,
            &property_address,
            0, nullptr,
            &data_size
        );

        if (status != noErr || data_size == 0) return;

        int num_devices = data_size / sizeof(AudioDeviceID);
        std::vector<AudioDeviceID> device_ids(num_devices);

        status = AudioObjectGetPropertyData(
            kAudioObjectSystemObject,
            &property_address,
            0, nullptr,
            &data_size,
            device_ids.data()
        );

        if (status != noErr) return;

        for (AudioDeviceID device_id : device_ids) {
            // Check if device has input channels
            int num_channels = _get_device_input_channels(device_id);
            if (num_channels == 0) continue;  // Skip output-only devices

            DeviceInfo info;
            info.name = _get_device_name(device_id);
            info.num_channels = num_channels;
            info.sample_rate = _get_device_sample_rate(device_id);

            _available_devices[device_id] = info;
        }
    }

    std::string _get_device_name(AudioDeviceID device_id) {
        AudioObjectPropertyAddress property_address = {
            kAudioDevicePropertyDeviceNameCFString,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };

        CFStringRef name_ref = nullptr;
        UInt32 data_size = sizeof(CFStringRef);

        OSStatus status = AudioObjectGetPropertyData(
            device_id,
            &property_address,
            0, nullptr,
            &data_size,
            &name_ref
        );

        if (status != noErr || !name_ref) {
            return "Unknown Device";
        }

        char name_buffer[256];
        CFStringGetCString(name_ref, name_buffer, sizeof(name_buffer), kCFStringEncodingUTF8);
        CFRelease(name_ref);

        return std::string(name_buffer);
    }

    int _get_device_input_channels(AudioDeviceID device_id) {
        AudioObjectPropertyAddress property_address = {
            kAudioDevicePropertyStreamConfiguration,
            kAudioDevicePropertyScopeInput,
            kAudioObjectPropertyElementMain
        };

        UInt32 data_size = 0;
        OSStatus status = AudioObjectGetPropertyDataSize(
            device_id,
            &property_address,
            0, nullptr,
            &data_size
        );

        if (status != noErr || data_size == 0) return 0;

        std::vector<uint8_t> buffer(data_size);
        auto* buffer_list = reinterpret_cast<AudioBufferList*>(buffer.data());

        status = AudioObjectGetPropertyData(
            device_id,
            &property_address,
            0, nullptr,
            &data_size,
            buffer_list
        );

        if (status != noErr) return 0;

        int total_channels = 0;
        for (UInt32 i = 0; i < buffer_list->mNumberBuffers; ++i) {
            total_channels += buffer_list->mBuffers[i].mNumberChannels;
        }

        return total_channels;
    }

    int _get_device_sample_rate(AudioDeviceID device_id) {
        AudioObjectPropertyAddress property_address = {
            kAudioDevicePropertyNominalSampleRate,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };

        Float64 sample_rate = 48000.0;
        UInt32 data_size = sizeof(Float64);

        AudioObjectGetPropertyData(
            device_id,
            &property_address,
            0, nullptr,
            &data_size,
            &sample_rate
        );

        return static_cast<int>(sample_rate);
    }

    std::map<AudioDeviceID, DeviceInfo> _available_devices;
    std::map<std::string, CoreAudioDevicePtr> _devices;
};

} // namespace ymery::plugins

extern "C" const char* name() { return "coreaudio"; }
extern "C" const char* type() { return "tree-like"; }
extern "C" void* create(
    std::shared_ptr<ymery::Dispatcher> /*dispatcher*/,
    std::shared_ptr<ymery::PluginManager> /*plugin_manager*/
) {
    return static_cast<void*>(new ymery::Result<ymery::TreeLikePtr>(ymery::plugins::CoreAudioManager::create()));
}
