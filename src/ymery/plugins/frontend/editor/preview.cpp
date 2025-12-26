// editor-preview widget plugin - live preview of the layout being edited
// Uses the widget factory to render actual plugins - no hardcoded widget rendering
#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"
#include "../../../types.hpp"
#include "shared_model.hpp"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <map>

namespace ymery::plugins {

class EditorPreview : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<EditorPreview>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("EditorPreview::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        auto& model = SharedLayoutModel::instance();

        if (model.empty()) {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            ImVec2 text_size = ImGui::CalcTextSize("No widgets in layout");
            ImGui::SetCursorPos(ImVec2(
                (avail.x - text_size.x) / 2,
                (avail.y - text_size.y) / 2
            ));
            ImGui::TextDisabled("No widgets in layout");
            return Ok();
        }

        _render_node(model.root());

        return Ok();
    }

private:
    // Cache of created widgets by node id
    std::map<int, WidgetPtr> _widget_cache;

    void _render_node(LayoutNode* node) {
        if (!node) return;

        ImGui::PushID(node->id);

        // Handle same-line positioning
        if (node->position == LayoutPosition::SameLine && node->parent) {
            auto& siblings = node->parent->children;
            for (size_t i = 0; i < siblings.size(); ++i) {
                if (siblings[i].get() == node && i > 0) {
                    ImGui::SameLine();
                    break;
                }
            }
        }

        const std::string& type = node->widget_type;

        // Get or create widget from cache
        WidgetPtr widget;
        auto it = _widget_cache.find(node->id);
        if (it != _widget_cache.end()) {
            widget = it->second;
        } else {
            // Build spec as a Dict with widget type
            Dict spec;
            spec["widget"] = type;
            spec["label"] = node->label;

            // Create widget via factory (uses plugins)
            auto res = _widget_factory->create_widget(_data_bag, Value(spec), _namespace);
            if (res) {
                widget = *res;
                _widget_cache[node->id] = widget;
            }
        }

        // Render the widget
        if (widget) {
            widget->render();

            // For container widgets, render children
            for (auto& child : node->children) {
                _render_node(child.get());
            }
        } else {
            // Fallback for unknown widgets
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "[%s: %s]", type.c_str(), node->label.c_str());
            for (auto& child : node->children) {
                _render_node(child.get());
            }
        }

        ImGui::PopID();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "editor-preview"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::EditorPreview::create(wf, d, ns, db);
}
