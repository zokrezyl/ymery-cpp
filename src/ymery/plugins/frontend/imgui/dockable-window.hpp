#pragma once
// dockable-window widget - a window that docks into a dock space
// The actual Begin/End is handled by docking-main-window, this just provides the body
#include "../../../frontend/composite.hpp"
#include "../../../frontend/widget_factory.hpp"
#include "../../../types.hpp"
#include <imgui.h>

namespace ymery::plugins::imgui {

class DockableWindow : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<DockableWindow>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("DockableWindow::create failed", res);
        }
        return widget;
    }

    Result<void> init() override {
        // Get window label
        if (auto res = _data_bag->get("label"); res && res->has_value()) {
            if (auto l = get_as<std::string>(*res)) {
                _label = *l;
            }
        }

        // Get dock space name
        if (auto res = _data_bag->get("dock-space-name"); res && res->has_value()) {
            if (auto s = get_as<std::string>(*res)) {
                _dock_space_name = *s;
            }
        }

        return Composite::init();
    }

    Result<void> render() override {
        // Ensure children are created
        if (auto res = _ensure_children(); !res) {
            return res;
        }

        // When rendered by docking-main-window, we're already inside Begin/End
        // Just render children
        return _render_children();
    }

    const std::string& label() const { return _label; }
    const std::string& dock_space_name() const { return _dock_space_name; }

protected:
    Result<void> _begin_container() override {
        // Not used - docking-main-window handles Begin/End
        _container_open = true;
        return Ok();
    }

    Result<void> _end_container() override {
        // Not used - docking-main-window handles Begin/End
        return Ok();
    }

private:
    std::string _label = "Dockable Window";
    std::string _dock_space_name = "MainDockSpace";
};

} // namespace ymery::plugins::imgui
