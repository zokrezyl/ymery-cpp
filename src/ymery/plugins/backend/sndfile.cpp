// sndfile backend plugin - Load audio files using libsndfile
#include "../../types.hpp"
#include "../../result.hpp"
#include "../../backend/audio_buffer.hpp"
#include <sndfile.h>
#include <map>
#include <filesystem>
#include <ytrace/ytrace.hpp>

namespace fs = std::filesystem;

namespace ymery { class Dispatcher; class PluginManager; }

namespace ymery::plugins {

/**
 * SndFileDevice - loads audio file, creates per-channel static buffers
 */
class SndFileDevice {
public:
    static Result<std::shared_ptr<SndFileDevice>> create(const std::string& filepath) {
        auto device = std::make_shared<SndFileDevice>();
        device->_filepath = filepath;

        SF_INFO sfinfo{};
        SNDFILE* file = sf_open(filepath.c_str(), SFM_READ, &sfinfo);
        if (!file) {
            return Err<std::shared_ptr<SndFileDevice>>(
                "SndFileDevice: " + std::string(sf_strerror(nullptr)));
        }

        device->_sample_rate = sfinfo.samplerate;
        device->_num_channels = sfinfo.channels;
        device->_frames = sfinfo.frames;

        // Format names
        SF_FORMAT_INFO fmt_info;
        fmt_info.format = sfinfo.format & SF_FORMAT_TYPEMASK;
        if (sf_command(nullptr, SFC_GET_FORMAT_INFO, &fmt_info, sizeof(fmt_info)) == 0 && fmt_info.name) {
            device->_format_name = fmt_info.name;
        }
        fmt_info.format = sfinfo.format & SF_FORMAT_SUBMASK;
        if (sf_command(nullptr, SFC_GET_FORMAT_INFO, &fmt_info, sizeof(fmt_info)) == 0 && fmt_info.name) {
            device->_subtype_name = fmt_info.name;
        }

        // Read interleaved samples
        std::vector<float> interleaved(sfinfo.frames * sfinfo.channels);
        sf_count_t read = sf_readf_float(file, interleaved.data(), sfinfo.frames);
        sf_close(file);

        if (read != sfinfo.frames) {
            return Err<std::shared_ptr<SndFileDevice>>("SndFileDevice: incomplete read");
        }

        // Deinterleave into per-channel buffers + mediators
        for (int ch = 0; ch < sfinfo.channels; ++ch) {
            std::vector<float> channel_data(sfinfo.frames);
            for (sf_count_t i = 0; i < sfinfo.frames; ++i) {
                channel_data[i] = interleaved[i * sfinfo.channels + ch];
            }

            auto buf_res = FileAudioBuffer::create(filepath, std::move(channel_data), sfinfo.samplerate);
            if (!buf_res) {
                return Err<std::shared_ptr<SndFileDevice>>("SndFileDevice: buffer create failed", buf_res);
            }
            device->_buffers.push_back(*buf_res);

            auto med_res = StaticAudioBufferMediator::create(*buf_res);
            if (!med_res) {
                return Err<std::shared_ptr<SndFileDevice>>("SndFileDevice: mediator create failed", med_res);
            }
            device->_mediators.push_back(*med_res);
        }

        yinfo("SndFileDevice: loaded {} ({} ch, {} Hz, {} frames)",
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
    sf_count_t frames() const { return _frames; }
    double duration() const { return _sample_rate > 0 ? static_cast<double>(_frames) / _sample_rate : 0.0; }
    const std::string& format_name() const { return _format_name; }
    const std::string& subtype_name() const { return _subtype_name; }

private:
    std::string _filepath;
    int _sample_rate = 0;
    int _num_channels = 0;
    sf_count_t _frames = 0;
    std::string _format_name;
    std::string _subtype_name;

    std::vector<FileAudioBufferPtr> _buffers;
    std::vector<StaticAudioBufferMediatorPtr> _mediators;
};

using SndFileDevicePtr = std::shared_ptr<SndFileDevice>;

/**
 * SndFileManager - creates and manages SndFileDevice instances, implements TreeLike
 */
class SndFileManager : public TreeLike {
public:
    static Result<TreeLikePtr> create() {
        auto manager = std::make_shared<SndFileManager>();
        if (auto res = manager->init(); !res) {
            return Err<TreeLikePtr>("SndFileManager::create failed", res);
        }
        return manager;
    }

    Result<void> init() override {
        int major_count = 0;
        sf_command(nullptr, SFC_GET_FORMAT_MAJOR_COUNT, &major_count, sizeof(major_count));

        for (int i = 0; i < major_count; ++i) {
            SF_FORMAT_INFO info;
            info.format = i;
            if (sf_command(nullptr, SFC_GET_FORMAT_MAJOR, &info, sizeof(info)) == 0 && info.extension) {
                _supported_extensions.push_back(info.extension);
            }
        }
        yinfo("SndFileManager: {} formats supported", _supported_extensions.size());
        return Ok();
    }

    Result<SndFileDevicePtr> open_file(const std::string& filepath) {
        auto it = _devices.find(filepath);
        if (it != _devices.end()) {
            return it->second;
        }

        auto res = SndFileDevice::create(filepath);
        if (!res) {
            return Err<SndFileDevicePtr>("SndFileManager::open_file failed", res);
        }

        _devices[filepath] = *res;
        _device_ids[_next_id] = filepath;
        ++_next_id;

        return *res;
    }

    // TreeLike interface
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
                {"name", Value("sndfile")},
                {"label", Value("Sound File Manager")},
                {"type", Value("sndfile-manager")},
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
                        {"format", Value(dev->format_name())},
                        {"subtype", Value(dev->subtype_name())}
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
        return Err<void>("SndFileManager: set not implemented");
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
        return Err<void>("SndFileManager: add_child not supported");
    }

    Result<std::string> as_tree(const DataPath& path, int) override {
        return Ok(path.to_string());
    }

private:
    std::vector<std::string> _supported_extensions;
    std::map<std::string, SndFileDevicePtr> _devices;
    std::map<int, std::string> _device_ids;
    int _next_id = 1;
};

} // namespace ymery::plugins

namespace ymery::embedded {
    Result<TreeLikePtr> create_sndfile_manager() {
        return plugins::SndFileManager::create();
    }
}

#ifndef YMERY_EMBEDDED_PLUGINS
extern "C" const char* name() { return "sndfile"; }
extern "C" const char* type() { return "device-manager"; }
extern "C" void* create(
    std::shared_ptr<ymery::Dispatcher>,
    std::shared_ptr<ymery::PluginManager>
) {
    return static_cast<void*>(new ymery::Result<ymery::TreeLikePtr>(ymery::plugins::SndFileManager::create()));
}
#endif
