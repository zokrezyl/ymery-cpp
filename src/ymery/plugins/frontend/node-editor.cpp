// node-editor widget plugin - Node graph editor
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui_node_editor.h>
#include <imgui.h>

namespace ed = ax::NodeEditor;

namespace ymery::plugins {

class NodeEditor : public Widget {
public:
    NodeEditor() = default;

    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<NodeEditor>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("NodeEditor::create failed", res);
        }
        return widget;
    }

    Result<void> init() override {
        if (auto res = Widget::init(); !res) {
            return res;
        }

        // Create node editor context
        _editor_context = ed::CreateEditor();
        return Ok();
    }

    Result<void> dispose() override {
        if (_editor_context) {
            ed::DestroyEditor(_editor_context);
            _editor_context = nullptr;
        }
        return Widget::dispose();
    }

protected:
    Result<void> _pre_render_head() override {
        std::string label = "Node Editor";
        if (auto res = _data_bag->get("label"); res) {
            if (auto l = get_as<std::string>(*res)) {
                label = *l;
            }
        }

        // Get size
        ImVec2 size(800, 600);
        if (auto res = _data_bag->get("size"); res) {
            if (auto s = get_as<List>(*res)) {
                if (s->size() >= 2) {
                    if (auto w = get_as<double>((*s)[0])) size.x = static_cast<float>(*w);
                    if (auto h = get_as<double>((*s)[1])) size.y = static_cast<float>(*h);
                }
            }
        }

        // Set context and begin editor
        ed::SetCurrentEditor(_editor_context);
        ed::Begin(label.c_str(), size);

        // Debug: create a test node directly
        ed::BeginNode(ed::NodeId(1));
        ImGui::Text("Test Node");
        ed::BeginPin(ed::PinId(1), ed::PinKind::Output);
        ImGui::Text("Out");
        ed::EndPin();
        ed::EndNode();

        _is_body_activated = true;
        return Ok();
    }

    Result<void> _post_render_head() override {
        ed::End();
        return Ok();
    }

private:
    ed::EditorContext* _editor_context = nullptr;
};

} // namespace ymery::plugins

extern "C" const char* name() { return "node-editor"; }
extern "C" const char* type() { return "widget"; }
extern "C" void* create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return static_cast<void*>(new ymery::Result<ymery::WidgetPtr>(ymery::plugins::NodeEditor::create(wf, d, ns, db)));
}
