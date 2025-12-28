// tree-node widget plugin
#include "../../frontend/composite.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>
#include <spdlog/spdlog.h>

namespace ymery::plugins {

class TreeNode : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<TreeNode>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("TreeNode::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _begin_container() override {
        std::string label = "Node";

        // 1. Try to get label from statics (explicitly set in YAML)
        if (auto label_res = _data_bag->get_static("label"); label_res && label_res->has_value()) {
            if (auto l = get_as<std::string>(*label_res)) {
                label = *l;
            }
        }
        // 2. Try to get label from metadata
        else if (auto meta_res = _data_bag->get_metadata(); meta_res) {
            auto& meta = *meta_res;
            // Try "label" first, then "name"
            if (auto it = meta.find("label"); it != meta.end()) {
                if (auto l = get_as<std::string>(it->second)) {
                    label = *l;
                }
            } else if (auto it = meta.find("name"); it != meta.end()) {
                if (auto l = get_as<std::string>(it->second)) {
                    label = *l;
                }
            }
        }
        // 3. Fallback to data path (last segment)
        if (label == "Node") {
            if (auto path_res = _data_bag->get_data_path_str(); path_res) {
                std::string path = *path_res;
                // Extract last segment
                auto pos = path.rfind('/');
                if (pos != std::string::npos && pos + 1 < path.size()) {
                    label = path.substr(pos + 1);
                } else if (!path.empty() && path != "/") {
                    label = path;
                }
            }
        }

        // Check if data tree has children at our path (for leaf vs expandable)
        bool has_data_children = false;
        if (auto children_res = _data_bag->get_children_names(); children_res) {
            has_data_children = !children_res->empty();
        }

        // Also check for body children in statics (widget structure)
        bool has_body_children = false;
        if (auto body_res = _data_bag->get_static("body"); body_res && body_res->has_value()) {
            if (auto body_list = get_as<List>(*body_res)) {
                has_body_children = !body_list->empty();
            } else if (auto body_str = get_as<std::string>(*body_res)) {
                // Body is a string reference (e.g., "kernel-recursive") - counts as having children
                has_body_children = !body_str->empty();
            } else if (auto body_dict = get_as<Dict>(*body_res)) {
                // Body is a dict (single widget spec) - counts as having children
                has_body_children = !body_dict->empty();
            }
        }

        bool has_children = has_data_children || has_body_children;
        spdlog::info("TreeNode: label={}, has_data_children={}, has_body_children={}, has_children={}",
                     label, has_data_children, has_body_children, has_children);

        std::string imgui_id = label + "##" + _uid;

        if (has_children) {
            // Has children - render as expandable tree node
            _container_open = ImGui::TreeNode(imgui_id.c_str());
        } else {
            // No children - render as leaf (no arrow, never opens)
            ImGui::TreeNodeEx(imgui_id.c_str(),
                ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
            _container_open = false;  // Leaf nodes never open
        }
        return Ok();
    }

    Result<void> _end_container() override {
        if (_container_open) {
            ImGui::TreePop();
        }
        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "tree-node"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::TreeNode::create(wf, d, ns, db);
}
