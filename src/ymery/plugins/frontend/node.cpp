// node widget plugin - Single node in node editor
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui_node_editor.h>
#include <imgui.h>
#include <functional>

namespace ed = ax::NodeEditor;

namespace ymery::plugins {

class Node : public Widget {
public:
    Node() = default;

    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Node>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Node::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        std::string label = "Node";
        if (auto res = _data_bag->get("label"); res) {
            if (auto l = get_as<std::string>(*res)) {
                label = *l;
            }
        }

        // Create unique node ID from uid hash (like Python: hash(f"node_{self.uid}"))
        std::hash<std::string> hasher;
        _node_id = ed::NodeId(hasher("node_" + _uid) & 0x7FFFFFFF);

        // Set initial position on first render
        if (!_positioned) {
            float x = 100.0f, y = 100.0f;
            if (auto res = _data_bag->get("x"); res) {
                if (auto v = get_as<double>(*res)) x = static_cast<float>(*v);
            }
            if (auto res = _data_bag->get("y"); res) {
                if (auto v = get_as<double>(*res)) y = static_cast<float>(*v);
            }
            ed::SetNodePosition(_node_id, ImVec2(x, y));
            _positioned = true;
        }

        // Begin node
        ed::BeginNode(_node_id);
        ImGui::Text("%s", label.c_str());

        _is_body_activated = true;
        return Ok();
    }

    Result<void> _post_render_head() override {
        ed::EndNode();
        return Ok();
    }

private:
    ed::NodeId _node_id;
    bool _positioned = false;
};

} // namespace ymery::plugins

extern "C" const char* name() { return "node"; }
extern "C" const char* type() { return "widget"; }
extern "C" void* create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return static_cast<void*>(new ymery::Result<ymery::WidgetPtr>(ymery::plugins::Node::create(wf, d, ns, db)));
}
