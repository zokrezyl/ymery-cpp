// audio_file - Load audio files using dr_libs (public domain)
// Supports WAV, MP3, FLAC formats
#include "../types.hpp"
#include "../result.hpp"
#include "audio_buffer.hpp"
#include <map>
#include <filesystem>
#include <algorithm>
#include <ytrace/ytrace.hpp>

// dr_libs implementations (single compilation unit)
#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>
#define DR_MP3_IMPLEMENTATION
#include <dr_mp3.h>
#define DR_FLAC_IMPLEMENTATION
#include <dr_flac.h>

namespace fs = std::filesystem;

namespace ymery {

class AudioFileDevice {
public:
    static Result<std::shared_ptr<AudioFileDevice>> create(const std::string& filepath) {
        auto device = std::make_shared<AudioFileDevice>();
        device->_filepath = filepath;

        // Determine format from extension
        std::string ext = fs::path(filepath).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        std::vector<float> samples;
        unsigned int channels = 0;
        unsigned int sample_rate = 0;
        uint64_t total_frames = 0;

        if (ext == ".wav") {
            drwav wav;
            if (!drwav_init_file(&wav, filepath.c_str(), nullptr)) {
                return Err<std::shared_ptr<AudioFileDevice>>("AudioFileDevice: failed to open WAV file: " + filepath);
            }

            channels = wav.channels;
            sample_rate = wav.sampleRate;
            total_frames = wav.totalPCMFrameCount;
            device->_format_name = "WAV";

            // Read all samples as float
            samples.resize(total_frames * channels);
            uint64_t frames_read = drwav_read_pcm_frames_f32(&wav, total_frames, samples.data());
            drwav_uninit(&wav);

            if (frames_read != total_frames) {
                return Err<std::shared_ptr<AudioFileDevice>>("AudioFileDevice: incomplete WAV read");
            }
        }
        else if (ext == ".mp3") {
            drmp3_config config;
            drmp3_uint64 frame_count;
            float* mp3_samples = drmp3_open_file_and_read_pcm_frames_f32(
                filepath.c_str(), &config, &frame_count, nullptr);

            if (!mp3_samples) {
                return Err<std::shared_ptr<AudioFileDevice>>("AudioFileDevice: failed to open MP3 file: " + filepath);
            }

            channels = config.channels;
            sample_rate = config.sampleRate;
            total_frames = frame_count;
            device->_format_name = "MP3";

            samples.assign(mp3_samples, mp3_samples + total_frames * channels);
            drmp3_free(mp3_samples, nullptr);
        }
        else if (ext == ".flac") {
            unsigned int flac_channels;
            unsigned int flac_sample_rate;
            drflac_uint64 frame_count;
            float* flac_samples = drflac_open_file_and_read_pcm_frames_f32(
                filepath.c_str(), &flac_channels, &flac_sample_rate, &frame_count, nullptr);

            if (!flac_samples) {
                return Err<std::shared_ptr<AudioFileDevice>>("AudioFileDevice: failed to open FLAC file: " + filepath);
            }

            channels = flac_channels;
            sample_rate = flac_sample_rate;
            total_frames = frame_count;
            device->_format_name = "FLAC";

            samples.assign(flac_samples, flac_samples + total_frames * channels);
            drflac_free(flac_samples, nullptr);
        }
        else {
            return Err<std::shared_ptr<AudioFileDevice>>("AudioFileDevice: unsupported format: " + ext);
        }

        device->_sample_rate = static_cast<int>(sample_rate);
        device->_num_channels = static_cast<int>(channels);
        device->_frames = static_cast<int64_t>(total_frames);

        // Deinterleave into per-channel buffers + mediators
        for (unsigned int ch = 0; ch < channels; ++ch) {
            std::vector<float> channel_data(total_frames);
            for (uint64_t i = 0; i < total_frames; ++i) {
                channel_data[i] = samples[i * channels + ch];
            }

            auto buf_res = FileAudioBuffer::create(filepath, std::move(channel_data), sample_rate);
            if (!buf_res) {
                return Err<std::shared_ptr<AudioFileDevice>>("AudioFileDevice: buffer create failed", buf_res);
            }
            device->_buffers.push_back(*buf_res);

            auto med_res = StaticAudioBufferMediator::create(*buf_res);
            if (!med_res) {
                return Err<std::shared_ptr<AudioFileDevice>>("AudioFileDevice: mediator create failed", med_res);
            }
            device->_mediators.push_back(*med_res);
        }

        yinfo("AudioFileDevice: loaded {} ({} ch, {} Hz, {} frames)",
              filepath, device->_num_channels, device->_sample_rate, device->_frames);

        return device;
    }

    StaticAudioBufferMediatorPtr get_mediator(int channel) const {
        if (channel >= 0 && channel < static_cast<int>(_mediators.size())) {
            return _mediators[channel];
        }
        return nullptr;
    }

    const std::string& filepath() const { return _filepath; }
    int num_channels() const { return _num_channels; }
    int sample_rate() const { return _sample_rate; }
    int64_t frames() const { return _frames; }
    double duration() const { return _sample_rate > 0 ? static_cast<double>(_frames) / _sample_rate : 0.0; }
    const std::string& format_name() const { return _format_name; }

private:
    std::string _filepath;
    int _sample_rate = 0;
    int _num_channels = 0;
    int64_t _frames = 0;
    std::string _format_name;

    std::vector<FileAudioBufferPtr> _buffers;
    std::vector<StaticAudioBufferMediatorPtr> _mediators;
};

using AudioFileDevicePtr = std::shared_ptr<AudioFileDevice>;

class AudioFileManager : public TreeLike {
public:
    static Result<TreeLikePtr> create() {
        auto manager = std::make_shared<AudioFileManager>();
        if (auto res = manager->init(); !res) {
            return Err<TreeLikePtr>("AudioFileManager::create failed", res);
        }
        return manager;
    }

    Result<void> init() override {
        _supported_extensions = {"wav", "mp3", "flac"};
        yinfo("AudioFileManager: {} formats supported", _supported_extensions.size());
        return Ok();
    }

    Result<AudioFileDevicePtr> open_file(const std::string& filepath) {
        auto it = _devices.find(filepath);
        if (it != _devices.end()) {
            return it->second;
        }

        auto res = AudioFileDevice::create(filepath);
        if (!res) {
            return Err<AudioFileDevicePtr>("AudioFileManager::open_file failed", res);
        }

        _devices[filepath] = *res;
        _device_ids[_next_id] = filepath;
        ++_next_id;

        return *res;
    }

    Result<std::vector<std::string>> get_children_names(const DataPath& path) override {
        std::string p = path.to_string();
        if (!p.empty() && p[0] != '/') p = "/" + p;

        if (p == "/" || p.empty()) {
            return Ok(std::vector<std::string>{"available", "opened"});
        }

        const auto& parts = path.as_list();
        if (parts.empty()) return Ok(std::vector<std::string>{});

        if (parts[0] == "available" && parts.size() == 1) {
            return Ok(_supported_extensions);
        }

        if (parts[0] == "opened") {
            if (parts.size() == 1) {
                std::vector<std::string> ids;
                for (const auto& [id, _] : _device_ids) {
                    ids.push_back(std::to_string(id));
                }
                return Ok(ids);
            }
            if (parts.size() == 2) {
                int id = std::stoi(parts[1]);
                auto it = _device_ids.find(id);
                if (it != _device_ids.end()) {
                    auto d = _devices.find(it->second);
                    if (d != _devices.end()) {
                        std::vector<std::string> chs;
                        for (int c = 0; c < d->second->num_channels(); ++c) {
                            chs.push_back(std::to_string(c));
                        }
                        return Ok(chs);
                    }
                }
            }
        }

        return Ok(std::vector<std::string>{});
    }

    Result<Dict> get_metadata(const DataPath& path) override {
        std::string p = path.to_string();
        if (!p.empty() && p[0] != '/') p = "/" + p;

        if (p == "/" || p.empty()) {
            return Ok(Dict{
                {"name", Value("audio-file")},
                {"label", Value("Audio File Manager")},
                {"type", Value("audio-file-manager")},
                {"category", Value("audio-device-manager")}
            });
        }

        const auto& parts = path.as_list();
        if (parts.empty()) return Ok(Dict{});

        if (parts[0] == "available" && parts.size() == 1) {
            return Ok(Dict{
                {"name", Value("available")},
                {"label", Value("Supported Formats")},
                {"type", Value("folder")},
                {"category", Value("folder")}
            });
        }

        if (parts[0] == "opened") {
            if (parts.size() == 1) {
                return Ok(Dict{
                    {"name", Value("opened")},
                    {"label", Value("Opened Files")},
                    {"type", Value("folder")},
                    {"category", Value("folder")}
                });
            }

            if (parts.size() >= 2) {
                int id = std::stoi(parts[1]);
                auto it = _device_ids.find(id);
                if (it == _device_ids.end()) return Ok(Dict{});

                auto d = _devices.find(it->second);
                if (d == _devices.end()) return Ok(Dict{});

                auto& dev = d->second;

                if (parts.size() == 2) {
                    return Ok(Dict{
                        {"name", Value(std::to_string(id))},
                        {"label", Value(fs::path(dev->filepath()).filename().string())},
                        {"type", Value("audio-file")},
                        {"category", Value("audio-device")},
                        {"filepath", Value(dev->filepath())},
                        {"sample_rate", Value(static_cast<int64_t>(dev->sample_rate()))},
                        {"channels", Value(static_cast<int64_t>(dev->num_channels()))},
                        {"frames", Value(static_cast<int64_t>(dev->frames()))},
                        {"duration", Value(dev->duration())},
                        {"format", Value(dev->format_name())}
                    });
                }

                if (parts.size() == 3) {
                    int ch = std::stoi(parts[2]);
                    if (ch >= 0 && ch < dev->num_channels()) {
                        std::string ch_name = dev->num_channels() == 2
                            ? (ch == 0 ? "Left" : "Right")
                            : ("Channel " + std::to_string(ch));
                        return Ok(Dict{
                            {"name", Value(std::to_string(ch))},
                            {"label", Value(ch_name)},
                            {"type", Value("audio-channel")},
                            {"category", Value("audio-channel")},
                            {"sample_rate", Value(static_cast<int64_t>(dev->sample_rate()))},
                            {"frames", Value(static_cast<int64_t>(dev->frames()))},
                            {"mediator", Value(dev->get_mediator(ch))}
                        });
                    }
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
        auto meta = get_metadata(parent);
        if (!meta) return Err<Value>("get failed", meta);
        auto it = meta->find(key);
        return Ok(it != meta->end() ? it->second : Value{});
    }

    Result<void> set(const DataPath&, const Value&) override {
        return Err<void>("AudioFileManager: set not implemented");
    }

    Result<void> add_child(const DataPath& path, const std::string&, const Dict& data) override {
        std::string p = path.to_string();
        if (p == "/opened" || p == "opened") {
            auto it = data.find("filepath");
            if (it != data.end()) {
                if (auto fp = get_as<std::string>(it->second)) {
                    auto res = open_file(*fp);
                    if (!res) return Err<void>("add_child failed", res);
                    return Ok();
                }
            }
        }
        return Err<void>("AudioFileManager: add_child not supported");
    }

    Result<std::string> as_tree(const DataPath& path, int) override {
        return Ok(path.to_string());
    }

private:
    std::vector<std::string> _supported_extensions;
    std::map<std::string, AudioFileDevicePtr> _devices;
    std::map<int, std::string> _device_ids;
    int _next_id = 1;
};

namespace embedded {
    Result<TreeLikePtr> create_audio_file_manager() {
        return AudioFileManager::create();
    }
}

} // namespace ymery
