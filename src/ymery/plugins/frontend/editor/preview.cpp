// editor-preview widget plugin - live preview of the layout being edited
// Simply renders the YAML structure via widget factory - no special logic
#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"
#include "../../../types.hpp"
#include "shared_model.hpp"
#include <imgui.h>
#include <spdlog/spdlog.h>

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

        // Create widget from YAML and render it
        // Recreate each frame to reflect editor changes
        auto res = _widget_factory->create_widget(_data_bag, model.root(), _namespace);
        if (res) {
            (*res)->render();
        } else {
            std::string err = error_msg(res);
            spdlog::error("EditorPreview: failed to create widget: {}", err);
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s", err.c_str());
        }

        return Ok();
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
