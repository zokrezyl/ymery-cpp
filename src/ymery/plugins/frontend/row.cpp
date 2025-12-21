// row widget plugin
#include "../../frontend/composite.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins {

class Row : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Row>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Row::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _begin_container() override {
        ImGui::BeginGroup();
        _container_open = true;
        return Ok();
    }

    Result<void> _render_children() override {
        bool first = true;
        for (auto& child : _children) {
            if (child) {
                if (!first) {
                    ImGui::SameLine();
                }
                first = false;
                if (auto res = child->render(); !res) {
                    // Log but continue
                }
            }
        }
        return Ok();
    }

    Result<void> _end_container() override {
        ImGui::EndGroup();
        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "row"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::Row::create(wf, d, ns, db);
}
