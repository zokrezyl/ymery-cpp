// table-row widget plugin - table row for use inside table
#include "../../frontend/composite.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins {

class TableRow : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<TableRow>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("TableRow::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _begin_container() override {
        float min_height = 0.0f;
        if (auto res = _data_bag->get_static("min-height"); res && res->has_value()) {
            if (auto h = get_as<float>(*res)) min_height = *h;
            else if (auto h = get_as<double>(*res)) min_height = static_cast<float>(*h);
        }

        ImGuiTableRowFlags flags = ImGuiTableRowFlags_None;
        if (auto res = _data_bag->get_static("headers"); res && res->has_value()) {
            if (auto h = get_as<bool>(*res); h && *h) {
                flags |= ImGuiTableRowFlags_Headers;
            }
        }

        ImGui::TableNextRow(flags, min_height);
        _container_open = true;  // Rows always "open"
        return Ok();
    }

    Result<void> _end_container() override {
        // No end call needed for table rows
        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "table-row"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::TableRow::create(wf, d, ns, db);
}
