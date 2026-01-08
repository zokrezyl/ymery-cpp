#pragma once

#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"
#include "../../../backend/audio_buffer.hpp"
#include <imgui.h>
#include <implot.h>

namespace ymery::plugins::implot {

class Line : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Line>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Line::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        std::string label = "Line";
        if (auto res = _data_bag->get("label"); res) {
            if (auto l = get_as<std::string>(*res)) {
                label = *l;
            }
        }

        // Try to get data from "data" property (as a list of values)
        if (auto res = _data_bag->get("data"); res) {
            if (auto list = get_as<List>(*res)) {
                std::vector<double> values;
                values.reserve(list->size());
                for (const auto& item : *list) {
                    if (auto v = get_as<double>(item)) {
                        values.push_back(*v);
                    }
                }
                if (!values.empty()) {
                    ImPlot::PlotLine(label.c_str(), values.data(), static_cast<int>(values.size()));
                }
                return Ok();
            }
        }

        // Try to get buffer from data bag (audio buffer)
        if (auto res = _data_bag->get("buffer"); res) {
            if (auto buf_ptr = get_as<MediatedAudioBufferPtr>(*res)) {
                auto buffer = *buf_ptr;
                if (buffer && buffer->try_lock()) {
                    auto buffer_data = buffer->data();
                    if (!buffer_data.empty()) {
                        double xstart = -static_cast<double>(buffer_data.size());
                        ImPlot::PlotLine(label.c_str(), buffer_data.data(),
                                         static_cast<int>(buffer_data.size()),
                                         1.0, xstart);
                    }
                    buffer->unlock();
                }
            }
        }

        return Ok();
    }
};

} // namespace ymery::plugins::implot
