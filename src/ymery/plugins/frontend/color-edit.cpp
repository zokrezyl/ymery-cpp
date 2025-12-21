// color-edit widget plugin
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins {

class ColorEdit : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<ColorEdit>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("ColorEdit::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        std::string label;
        if (auto res = _data_bag->get("label"); res) {
            if (auto l = get_as<std::string>(*res)) {
                label = *l;
            }
        }

        if (auto res = _data_bag->get("value"); res) {
            if (auto color_list = get_as<List>(*res)) {
                if (color_list->size() >= 3) {
                    for (size_t i = 0; i < std::min(color_list->size(), size_t(4)); ++i) {
                        if (auto c = get_as<double>((*color_list)[i])) {
                            _color[i] = static_cast<float>(*c);
                        }
                    }
                }
            }
        }

        if (ImGui::ColorEdit4(label.c_str(), _color)) {
            List new_color;
            for (int i = 0; i < 4; ++i) {
                new_color.push_back(Value(static_cast<double>(_color[i])));
            }
            _data_bag->set("value", Value(new_color));
        }
        return Ok();
    }

private:
    float _color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
};

} // namespace ymery::plugins

extern "C" const char* name() { return "color-edit"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::ColorEdit::create(wf, d, ns, db);
}
