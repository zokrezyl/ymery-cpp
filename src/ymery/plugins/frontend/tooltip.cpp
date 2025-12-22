// tooltip widget plugin - Shows tooltip when PREVIOUS sibling item is hovered
// Usage: place after the widget you want to add tooltip to
// Example:
//   - button:
//       label: Hover me
//   - tooltip:
//       body:
//         - text: I am a tooltip!
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include "../../frontend/composite.hpp"
#include <imgui.h>

namespace ymery::plugins {

class Tooltip : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Tooltip>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Tooltip::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        // Check if previous item is hovered - standard ImGui tooltip pattern
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            _is_body_activated = true;
            _tooltip_open = true;
        } else {
            _is_body_activated = false;
            _tooltip_open = false;
        }
        return Ok();
    }

    Result<void> _post_render_head() override {
        if (_tooltip_open) {
            ImGui::EndTooltip();
        }
        return Ok();
    }

private:
    bool _tooltip_open = false;
};

} // namespace ymery::plugins

extern "C" const char* name() { return "tooltip"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::Tooltip::create(wf, d, ns, db);
}
