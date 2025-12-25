// implot-layer widget plugin - Renders a single plot layer (line plot) from audio buffer
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include "../../backend/audio_buffer.hpp"
#include <implot.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

namespace ymery::plugins {

class ImplotLayer : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<ImplotLayer>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("ImplotLayer::create failed", res);
        }

        // Debug: log creation with data-path
        auto path_res = data_bag->get_data_path();
        spdlog::info("ImplotLayer CREATED: data-path='{}'", path_res ? (*path_res).to_string() : "N/A");

        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        // Get label
        std::string label = "Layer";
        if (auto res = _data_bag->get("label"); res) {
            if (auto l = get_as<std::string>(*res)) {
                label = *l;
            }
        }

        // Debug: show data path
        if (!_debug_printed) {
            auto path_res = _data_bag->get_data_path();
            if (path_res) {
                spdlog::info("ImplotLayer '{}': data-path = {}", label, (*path_res).to_string());
            }
            _debug_printed = true;
        }

        // Try to get buffer from cached or from data bag
        MediatedAudioBufferPtr buffer = _cached_buffer;

        if (!buffer) {
            // Try to get buffer directly from data tree via get()
            if (auto res = _data_bag->get("buffer"); res) {
                spdlog::info("ImplotLayer '{}': got buffer from get()", label);
                // The buffer is stored as a Value containing shared_ptr
                if (auto buf_ptr = get_as<MediatedAudioBufferPtr>(*res)) {
                    buffer = *buf_ptr;
                    _cached_buffer = buffer;
                    spdlog::info("ImplotLayer '{}': buffer extracted successfully", label);
                } else {
                    spdlog::warn("ImplotLayer '{}': buffer value exists but wrong type", label);
                }
            } else {
                spdlog::warn("ImplotLayer '{}': get(buffer) failed: {}", label, error_msg(res));
            }
        }

        if (!buffer) {
            // No buffer available - try to open from tree-like
            // Check if this is an openable channel
            auto cap_res = _data_bag->get("capabilities");
            if (cap_res) {
                if (auto caps = get_as<Dict>(*cap_res)) {
                    auto openable_it = caps->find("openable");
                    if (openable_it != caps->end()) {
                        if (auto open = get_as<bool>(openable_it->second); open && *open) {
                            // This is openable - we need to open it
                            // For now, just skip rendering
                            spdlog::info("ImplotLayer '{}': channel is openable but not opened yet", label);
                            return Ok();
                        }
                    }
                }
            }
            return Ok();  // No buffer, skip rendering
        }

        // Try to lock buffer
        if (!buffer->try_lock()) {
            return Ok();  // Buffer busy, skip this frame
        }

        // Get buffer data
        auto buffer_data = buffer->data();
        if (buffer_data.empty()) {
            buffer->unlock();
            return Ok();  // No data to plot
        }

        // X-axis: oldest sample at negative X, newest at 0
        double xstart = -static_cast<double>(buffer_data.size());

        // Plot line
        ImPlot::PlotLine(label.c_str(), buffer_data.data(),
                         static_cast<int>(buffer_data.size()),
                         1.0, xstart);

        buffer->unlock();
        return Ok();
    }

private:
    MediatedAudioBufferPtr _cached_buffer;
    bool _debug_printed = false;
};

} // namespace ymery::plugins

extern "C" const char* name() { return "implot-layer"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::ImplotLayer::create(wf, d, ns, db);
}
