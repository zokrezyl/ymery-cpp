// input-text widget plugin
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins {

class InputText : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<InputText>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;
        widget->_buffer.resize(256);

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("InputText::create failed", res);
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
            if (auto v = get_as<std::string>(*res)) {
                if (_buffer.size() < v->size() + 1) {
                    _buffer.resize(v->size() + 128);
                }
                std::copy(v->begin(), v->end(), _buffer.begin());
                _buffer[v->size()] = '\0';
            }
        }

        if (ImGui::InputText(label.c_str(), _buffer.data(), _buffer.size())) {
            _data_bag->set("value", Value(std::string(_buffer.c_str())));
        }
        return Ok();
    }

private:
    std::string _buffer;
};

} // namespace ymery::plugins

extern "C" const char* name() { return "input-text"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::InputText::create(wf, d, ns, db);
}
