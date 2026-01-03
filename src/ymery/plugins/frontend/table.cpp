// table widget plugin - ImGui table container
#include "../../frontend/composite.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins {

class Table : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Table>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Table::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _begin_container() override {
        std::string label = "##table";
        if (auto res = _data_bag->get("label"); res && res->has_value()) {
            if (auto l = get_as<std::string>(*res)) label = *l;
        }

        int num_columns = 1;
        if (auto res = _data_bag->get_static("columns"); res && res->has_value()) {
            if (auto c = get_as<int>(*res)) num_columns = *c;
        }

        ImGuiTableFlags flags = ImGuiTableFlags_None;
        if (auto res = _data_bag->get_static("borders"); res && res->has_value()) {
            if (auto b = get_as<bool>(*res); b && *b) {
                flags |= ImGuiTableFlags_Borders;
            }
        }
        if (auto res = _data_bag->get_static("resizable"); res && res->has_value()) {
            if (auto r = get_as<bool>(*res); r && *r) {
                flags |= ImGuiTableFlags_Resizable;
            }
        }
        if (auto res = _data_bag->get_static("row-bg"); res && res->has_value()) {
            if (auto r = get_as<bool>(*res); r && *r) {
                flags |= ImGuiTableFlags_RowBg;
            }
        }

        std::string imgui_id = label + "##" + _uid;
        _container_open = ImGui::BeginTable(imgui_id.c_str(), num_columns, flags);
        return Ok();
    }

    Result<void> _end_container() override {
        if (_container_open) {
            ImGui::EndTable();
        }
        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "table"; }
extern "C" const char* type() { return "widget"; }
extern "C" void* create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return static_cast<void*>(new ymery::Result<ymery::WidgetPtr>(ymery::plugins::Table::create(wf, d, ns, db)));
}
